#include "stdafx.h"
#include "logistics/smuti.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "logistics/loggen.h"
#include "logistics/mrblock.h"

auto SMUTI::init(char* fileName, aPort* port, int32_t width) -> int32_t
{
    auto* file = new File;
    const int32_t result = file->open(fileName, READ, 0x32);
    Assert(result == 0, result, "Could not open SMUTI file", nullptr);
    auto* text = new uint8_t[file->getLength() + 1];
    file->read(text, static_cast<int32_t>(file->getLength()));
    // The last two bytes (the file's closing CR LF) are dropped.
    text[file->getLength() - 2] = 0;
    file->close();
    delete file;
    const int32_t height = process(text, port, width, 0);
    delete[] text;
    return height;
}

auto SMUTI::process(uint8_t* text, aPort* port, int32_t width, int32_t startY) -> int32_t
{
    if (port != nullptr)
    {
        width = port->width();
    }

    this->width = width;
    this->port = port;
    curX = 0;
    curY = startY;
    font = greenFont;
    fontSize = 0;
    fontColor = 3;
    lineLength = 0;
    lineStartX = 0;

    // Writes the pending line buffer at its start.
    auto flushLine = [this]()
    {
        if (this->port != nullptr && lineBuffer[0] != 0)
        {
            font->writeString(this->port->frame(), lineStartX, curY, reinterpret_cast<uint8_t*>(lineBuffer), -1);
        }
    };

    // Writes a character straight into the port at curX and moves past it.
    auto drawChar = [this](uint8_t c)
    {
        if (this->port != nullptr)
        {
            font->writeChar(this->port->frame(), curX, curY, static_cast<char>(c));
        }

        curX += font->width(c);
    };

    uint8_t* p = text - 1;

    for (;;)
    {
        uint8_t c = *++p;

        if (c == 0)
        {
            break;
        }

        centered = 0;

        if (c != '%')
        {
            if (c > 0x1f)
            {
                checkWrap(c);
                lineBuffer[lineLength++] = static_cast<char>(c);
                lineBuffer[lineLength] = 0;
                curX += font->width(c);
            }

            continue;
        }

        // A code: `p` is on its '%'. A %c line can hand another '%' straight back here.
        bool finished = false;

        for (bool again = true; again;)
        {
            again = false;
            c = *++p;

            if (c == 0)
            {
                finished = true;
                break;
            }

            switch (std::tolower(c))
            {
                case '%':
                {
                    checkWrap(c);
                    lineBuffer[lineLength++] = static_cast<char>(c);
                    // Port fix: terminate the buffer (the original left stale text behind the '%').
                    lineBuffer[lineLength] = 0;
                    curX += font->width(c);
                    break;
                }

                case 'c':
                {
                    // A centred line, measured and written straight into the port (at most 80 characters).
                    centered = 1;
                    char centerText[84];
                    int32_t count = 0;
                    int32_t lineWidth = 0;

                    if (curX > 0)
                    {
                        curX = 0;
                        curY += font->height();
                    }

                    c = *++p;

                    while (c != '%' && c != 0 && count != 0x50)
                    {
                        lineWidth += font->width(c);
                        centerText[count++] = static_cast<char>(c);

                        if (this->width <= lineWidth)
                        {
                            break;
                        }

                        c = *++p;
                    }

                    if (this->width < lineWidth)
                    {
                        // Too wide: write it from the left edge (including the character that overflowed) and carry that
                        // character onto the next line. The original also stores centerText[count - 1] back into *p,
                        // which is the character already there.
                        centerText[count] = 0;

                        if (this->port != nullptr)
                        {
                            font->writeString(this->port->frame(), 0, curY, reinterpret_cast<uint8_t*>(centerText), -1);
                        }

                        curX = 0;
                        curY += font->height();
                        c = *p;

                        if (c == '%')
                        {
                            again = true;
                            break;
                        }

                        // Original behaviour (OB-070): the overflowing character is drawn at x 0 and then again after
                        // itself.
                        if (this->port != nullptr)
                        {
                            font->writeChar(this->port->frame(), 0, curY, static_cast<char>(c));
                        }

                        curX = font->width(c);
                    }
                    else
                    {
                        centerText[count] = 0;
                        const int32_t x = (this->width - lineWidth) / 2;
                        curX = x;

                        if (this->port != nullptr)
                        {
                            font->writeString(this->port->frame(), x, curY, reinterpret_cast<uint8_t*>(centerText), -1);
                        }

                        curX = font->width(reinterpret_cast<uint8_t*>(centerText)) + x;
                    }

                    if (*p == '%')
                    {
                        again = true;
                        break;
                    }

                    // Port fix: the text ended inside the centred line; the original drew the terminator and read on
                    // past it.
                    if (*p == 0)
                    {
                        finished = true;
                        break;
                    }

                    // The character that stopped the line (the last one measured, or the 81st) is drawn after it.
                    drawChar(*p);
                    break;
                }

                case 'f':
                {
                    const uint8_t code = *++p;

                    if (code == 0)
                    {
                        // Port fix: the original read on past the terminator.
                        finished = true;
                        break;
                    }

                    switch (std::tolower(code))
                    {
                        case 'c':
                        {
                            const uint8_t color = *++p;

                            if (color == 0)
                            {
                                // Port fix: as above.
                                finished = true;
                                break;
                            }

                            if (this->port != nullptr && lineBuffer[0] != 0)
                            {
                                lineBuffer[lineLength] = 0;
                                font->writeString(this->port->frame(), lineStartX, curY,
                                                  reinterpret_cast<uint8_t*>(lineBuffer), -1);
                                lineStartX += curX;
                                curX = 0;
                                lineLength = 0;
                            }

                            fontColor = color - '0';
                            font = fonts[fontColor][fontSize];
                            break;
                        }

                        case 'l':
                        {
                            fontSize = 2;
                            font = fonts[fontColor][2];
                            break;
                        }
                        case 'm':
                        {
                            fontSize = 1;
                            font = fonts[fontColor][1];
                            break;
                        }
                        case 's':
                        {
                            fontSize = 0;
                            font = fonts[fontColor][0];
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }

                case 'n':
                {
                    lineBuffer[lineLength] = 0;
                    flushLine();
                    lineStartX = 0;
                    curX = 0;
                    lineLength = 0;
                    curY += font->height() + 1;
                    break;
                }

                case 't':
                {
                    // Moves right by a decimal pixel count; `p` stays on its last digit.
                    int32_t tab = 0;

                    while (p[1] >= '0' && p[1] <= '9')
                    {
                        tab = tab * 10 + (*++p - '0');
                    }

                    const int32_t oldX = curX;
                    curX = oldX + tab;
                    lineStartX += tab;

                    if (this->width <= oldX + tab)
                    {
                        checkWrap(*p);
                    }
                    break;
                }

                default:
                {
                    // An unknown code is text: the '%' and the character both go into the line.
                    checkWrap('%');
                    lineBuffer[lineLength++] = '%';
                    curX += font->width('%');
                    checkWrap(c);
                    lineBuffer[lineLength++] = static_cast<char>(c);
                    // Port fix: terminate the buffer, as for "%%".
                    lineBuffer[lineLength] = 0;
                    curX += font->width(c);
                    break;
                }
            }
        }

        if (finished)
        {
            break;
        }
    }

    if (this->port != nullptr && lineBuffer[0] != 0 && centered != 1)
    {
        font->writeString(this->port->frame(), lineStartX, curY, reinterpret_cast<uint8_t*>(lineBuffer), -1);
    }

    this->port = nullptr;
    return font->height() + curY;
}

auto SMUTI::checkWrap(uint8_t nextChar) -> void
{
    if (width > font->width(nextChar) + lineStartX + curX)
    {
        return;
    }

    // Breaks at the last space (never at index 0); with none, the last character moves one right and the split
    // falls just before it.
    int32_t split = lineLength;

    for (; split > 0; split--)
    {
        if (lineBuffer[split] == ' ')
        {
            lineBuffer[split] = 0;
            break;
        }
    }

    if (split == 0)
    {
        // Port fix: an empty buffer read and cleared the byte before lineBuffer (fontColor's top byte, always 0).
        lineBuffer[lineLength] = lineLength > 0 ? lineBuffer[lineLength - 1] : 0;
        split = lineLength - 1;

        if (lineLength > 0)
        {
            lineBuffer[lineLength - 1] = 0;
        }

        lineLength++;
    }

    if (port != nullptr)
    {
        font->writeString(port->frame(), lineStartX, curY, reinterpret_cast<uint8_t*>(lineBuffer), -1);
    }

    int32_t count = 0;

    for (int32_t i = split + 1; i < lineLength; i++)
    {
        lineBuffer[count++] = lineBuffer[i];
    }

    lineBuffer[count] = 0;
    lineStartX = 0;
    curX = font->width(reinterpret_cast<uint8_t*>(lineBuffer));
    lineLength = count;
    curY += font->height() + 1;
}
