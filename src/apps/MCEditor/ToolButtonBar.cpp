#include "stdafx.h"
#include "ToolButtonBar.h"
#include "EditorPalette.h"

namespace
{
    /// <summary>The size of a button image in the bitmap.</summary>
    inline constexpr int BUTTON_IMAGE_WIDTH = 0x16;
    inline constexpr int BUTTON_IMAGE_HEIGHT = 0x12;
    /// <summary>The size of a button.</summary>
    inline constexpr int BUTTON_WIDTH = 0x1d;
    inline constexpr int BUTTON_HEIGHT = 0x19;
    /// <summary>The image lists grow by one image at a time.</summary>
    inline constexpr int IMAGE_LIST_GROW = 1;
}

ToolButtonBar::ToolButtonBar(UINT bitmapId, UINT firstCommand, UINT lastCommand)
    : bitmapId(bitmapId), firstCommand(firstCommand), lastCommand(lastCommand)
{
}

BEGIN_MESSAGE_MAP(ToolButtonBar, PaletteBar)
ON_WM_CREATE()
END_MESSAGE_MAP()

int ToolButtonBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CToolBar::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    const HINSTANCE instance = GetModuleHandleA(nullptr);
    const auto image =
        static_cast<HBITMAP>(LoadImageA(instance, MAKEINTRESOURCEA(bitmapId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    const auto disabledImage =
        static_cast<HBITMAP>(LoadImageA(instance, MAKEINTRESOURCEA(bitmapId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    const int count = static_cast<int>(lastCommand - firstCommand);
    images.Create(BUTTON_IMAGE_WIDTH, BUTTON_IMAGE_HEIGHT, ILC_COLOR8, count, IMAGE_LIST_GROW);
    disabledImages.Create(BUTTON_IMAGE_WIDTH, BUTTON_IMAGE_HEIGHT, ILC_COLOR8, count, IMAGE_LIST_GROW);

    // The disabled images are the image's pixels through the highlight table (the bits as one byte per pixel).
    BITMAP info;
    GetObjectA(disabledImage, sizeof(info), &info);
    const int size = info.bmWidth * info.bmHeight;
    std::vector<uint8_t> bits(static_cast<size_t>(size));
    GetBitmapBits(disabledImage, size, bits.data());

    for (uint8_t& pixel : bits)
    {
        pixel = editorPalette->highlight[pixel];
    }

    SetBitmapBits(disabledImage, size, bits.data());
    ImageList_Add(images.m_hImageList, image, image);
    ImageList_Add(disabledImages.m_hImageList, disabledImage, disabledImage);
    SendMessage(TB_SETBITMAPSIZE, 0, MAKELONG(BUTTON_IMAGE_WIDTH, BUTTON_IMAGE_HEIGHT));
    SendMessage(TB_SETBUTTONSIZE, 0, MAKELONG(BUTTON_WIDTH, BUTTON_HEIGHT));
    TBBUTTON button = {};
    button.fsState = TBSTATE_ENABLED;
    int index = 0;

    // Each button is inserted at its command's value as the index, which is past the end: it goes last.
    for (UINT command = firstCommand; command <= lastCommand; command++, index++)
    {
        button.iBitmap = index;
        button.idCommand = static_cast<int>(command);
        SendMessage(TB_INSERTBUTTON, command, reinterpret_cast<LPARAM>(&button));
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    DeleteObject(image);
    DeleteObject(disabledImage);
    return 0;
}
