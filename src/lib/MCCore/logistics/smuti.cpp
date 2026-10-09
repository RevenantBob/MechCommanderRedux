#include "stdafx.h"
#include "logistics/smuti.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiPort.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "logistics/loggen.h"
#include "logistics/mrblock.h"

auto MCSmuti::Init(char* fileName, MCGuiPort* port, int32_t width) -> int32_t
{
    auto* file = new MCFile;
    const int32_t result = file->Open(fileName);
    Assert(result == 0, result, "Could not open SMUTI file");
    auto* text = new uint8_t[file->GetLength() + 1];
    file->Read(text, static_cast<int32_t>(file->GetLength()));
    // The last two bytes (the file's closing CR LF) are dropped.
    text[file->GetLength() - 2] = 0;
    file->Close();
    delete file;
    const int32_t height = Process(text, port, width, 0);
    delete[] text;
    return height;
}

auto MCSmuti::Process(uint8_t* text, MCGuiPort* port, int32_t width, int32_t startY) -> int32_t
{
    if (port != nullptr)
    {
        width = port->Width();
    }

    this->Width = width;
    this->Port = port;
    CurX = 0;
    CurY = startY;
    Font = GreenFont;
    FontSize = 0;
    FontColor = 3;
    LineLength = 0;
    LineStartX = 0;

    // Writes the pending line buffer at its start.
    auto flushLine = [this]()
    {
        if (this->Port != nullptr && LineBuffer[0] != 0)
        {
            Font->WriteString(this->Port->Frame(), LineStartX, CurY, LineBuffer, -1);
        }
    };

    // Writes a character straight into the port at curX and moves past it.
    auto drawChar = [this](uint8_t c)
    {
        if (this->Port != nullptr)
        {
            Font->WriteChar(this->Port->Frame(), CurX, CurY, static_cast<char>(c));
        }

        CurX += Font->Width(c);
    };

    uint8_t* p = text - 1;

    for (;;)
    {
        uint8_t c = *++p;

        if (c == 0)
        {
            break;
        }

        Centered = 0;

        if (c != '%')
        {
            if (c > 0x1f)
            {
                CheckWrap(c);
                LineBuffer[LineLength++] = static_cast<char>(c);
                LineBuffer[LineLength] = 0;
                CurX += Font->Width(c);
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
                    CheckWrap(c);
                    LineBuffer[LineLength++] = static_cast<char>(c);
                    // Port fix: terminate the buffer (the original left stale text behind the '%').
                    LineBuffer[LineLength] = 0;
                    CurX += Font->Width(c);
                    break;
                }

                case 'c':
                {
                    // A centred line, measured and written straight into the port (at most 80 characters).
                    Centered = 1;
                    char centerText[84];
                    int32_t count = 0;
                    int32_t lineWidth = 0;

                    if (CurX > 0)
                    {
                        CurX = 0;
                        CurY += Font->Height();
                    }

                    c = *++p;

                    while (c != '%' && c != 0 && count != 0x50)
                    {
                        lineWidth += Font->Width(c);
                        centerText[count++] = static_cast<char>(c);

                        if (this->Width <= lineWidth)
                        {
                            break;
                        }

                        c = *++p;
                    }

                    if (this->Width < lineWidth)
                    {
                        // Too wide: write it from the left edge (including the character that overflowed) and carry that
                        // character onto the next line. The original also stores centerText[count - 1] back into *p,
                        // which is the character already there.
                        centerText[count] = 0;

                        if (this->Port != nullptr)
                        {
                            Font->WriteString(this->Port->Frame(), 0, CurY, centerText, -1);
                        }

                        CurX = 0;
                        CurY += Font->Height();
                        c = *p;

                        if (c == '%')
                        {
                            again = true;
                            break;
                        }

                        // Original behaviour (OB-070): the overflowing character is drawn at x 0 and then again after
                        // itself.
                        if (this->Port != nullptr)
                        {
                            Font->WriteChar(this->Port->Frame(), 0, CurY, static_cast<char>(c));
                        }

                        CurX = Font->Width(c);
                    }
                    else
                    {
                        centerText[count] = 0;
                        const int32_t x = (this->Width - lineWidth) / 2;
                        CurX = x;

                        if (this->Port != nullptr)
                        {
                            Font->WriteString(this->Port->Frame(), x, CurY, centerText, -1);
                        }

                        CurX = Font->Width(centerText) + x;
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

                            if (this->Port != nullptr && LineBuffer[0] != 0)
                            {
                                LineBuffer[LineLength] = 0;
                                Font->WriteString(this->Port->Frame(), LineStartX, CurY, LineBuffer, -1);
                                LineStartX += CurX;
                                CurX = 0;
                                LineLength = 0;
                            }

                            FontColor = color - '0';
                            Font = Fonts[FontColor][FontSize];
                            break;
                        }

                        case 'l':
                        {
                            FontSize = 2;
                            Font = Fonts[FontColor][2];
                            break;
                        }
                        case 'm':
                        {
                            FontSize = 1;
                            Font = Fonts[FontColor][1];
                            break;
                        }
                        case 's':
                        {
                            FontSize = 0;
                            Font = Fonts[FontColor][0];
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }

                case 'n':
                {
                    LineBuffer[LineLength] = 0;
                    flushLine();
                    LineStartX = 0;
                    CurX = 0;
                    LineLength = 0;
                    CurY += Font->Height() + 1;
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

                    const int32_t oldX = CurX;
                    CurX = oldX + tab;
                    LineStartX += tab;

                    if (this->Width <= oldX + tab)
                    {
                        CheckWrap(*p);
                    }
                    break;
                }

                default:
                {
                    // Any other code is text: the '%' and the character both go into the line.
                    CheckWrap('%');
                    LineBuffer[LineLength++] = '%';
                    CurX += Font->Width('%');
                    CheckWrap(c);
                    LineBuffer[LineLength++] = static_cast<char>(c);
                    // Port fix: terminate the buffer, as for "%%".
                    LineBuffer[LineLength] = 0;
                    CurX += Font->Width(c);
                    break;
                }
            }
        }

        if (finished)
        {
            break;
        }
    }

    if (this->Port != nullptr && LineBuffer[0] != 0 && Centered != 1)
    {
        Font->WriteString(this->Port->Frame(), LineStartX, CurY, LineBuffer, -1);
    }

    this->Port = nullptr;
    return Font->Height() + CurY;
}

auto MCSmuti::CheckWrap(uint8_t nextChar) -> void
{
    if (Width > Font->Width(nextChar) + LineStartX + CurX)
    {
        return;
    }

    // Breaks at the last space (never at index 0); with none, the last character moves one right and the split
    // falls just before it.
    int32_t split = LineLength;

    for (; split > 0; split--)
    {
        if (LineBuffer[split] == ' ')
        {
            LineBuffer[split] = 0;
            break;
        }
    }

    if (split == 0)
    {
        // Port fix: an empty buffer read and cleared the byte before lineBuffer (fontColor's top byte, always 0).
        LineBuffer[LineLength] = LineLength > 0 ? LineBuffer[LineLength - 1] : 0;
        split = LineLength - 1;

        if (LineLength > 0)
        {
            LineBuffer[LineLength - 1] = 0;
        }

        LineLength++;
    }

    if (Port != nullptr)
    {
        Font->WriteString(Port->Frame(), LineStartX, CurY, LineBuffer, -1);
    }

    int32_t count = 0;

    for (int32_t i = split + 1; i < LineLength; i++)
    {
        LineBuffer[count++] = LineBuffer[i];
    }

    LineBuffer[count] = 0;
    LineStartX = 0;
    CurX = Font->Width(LineBuffer);
    LineLength = count;
    CurY += Font->Height() + 1;
}
