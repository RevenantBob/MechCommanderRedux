#include "stdafx.h"
#include "MapFeaturesBar.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"

int32_t paletteImageWidth = 0x30;
int32_t paletteImageHeight = 0x30;

namespace
{
    /// <summary>The padding of a button around its image (0x004fdc90, 0x004fdc94).</summary>
    inline constexpr int32_t BUTTON_PADDING_X = 10;
    inline constexpr int32_t BUTTON_PADDING_Y = 8;
    /// <summary>The width of the separator that starts each row of the grid.</summary>
    inline constexpr int SEPARATOR_WIDTH = 0xc;
    /// <summary>The idCommand the separators carry.</summary>
    inline constexpr int SEPARATOR_COMMAND = 1;
    /// <summary>The fsState of a separator that ends a row.</summary>
    inline constexpr BYTE WRAP_STATE = TBSTATE_ENABLED | TBSTATE_WRAP;
    /// <summary>The buttons of the tab row, which survive a page change.</summary>
    inline constexpr int TAB_ROW_BUTTONS = 4;
    /// <summary>The button whose rectangle the tab control covers.</summary>
    inline constexpr WPARAM TAB_PLACEHOLDER = 1;
    /// <summary>The tab names are strings 111-115.</summary>
    inline constexpr UINT TAB_NAME_STRINGS = IDS_TAB_TERRAINS;
    inline constexpr int TAB_COUNT = 5;
    /// <summary>The controls' ids.</summary>
    inline constexpr UINT ID_TABS = 0x77;
    inline constexpr UINT ID_MOVER_GROUPS = 0x78;
    inline constexpr UINT ID_OBJECT_GROUPS = 0x212;
    inline constexpr UINT ID_OVERLAY_KINDS = 0x21c;
    /// <summary>The side radio buttons: side 2, side 1, neutral (and one more the range takes).</summary>
    inline constexpr UINT ID_SIDE_2_RADIO = 0x406;
    inline constexpr UINT ID_SIDE_1_RADIO = 0x407;
    inline constexpr UINT ID_NEUTRAL_RADIO = 0x408;
    inline constexpr UINT ID_LAST_SIDE_RADIO = 0x409;
    /// <summary>The mech variant radio buttons, A, W and J (and one more the range takes).</summary>
    inline constexpr UINT ID_VARIANT_FIRST = 500;
    inline constexpr UINT ID_VARIANT_LAST = 503;
    inline constexpr int VARIANT_COUNT = 3;
    /// <summary>The first building and unit buttons, and the last the clicks test for.</summary>
    inline constexpr UINT FIRST_BUILDING_BUTTON = 0x66;
    inline constexpr UINT LAST_BUILDING_BUTTON = 0x98;
    inline constexpr UINT FIRST_UNIT_BUTTON = 200;
    inline constexpr UINT LAST_UNIT_BUTTON = 0xfa;
    /// <summary>The first overlay, terrain and overlay layout buttons.</summary>
    inline constexpr int FIRST_OVERLAY_BUTTON = 5000;
    inline constexpr int FIRST_TERRAIN_BUTTON = 6000;
    inline constexpr int FIRST_LAYOUT_BUTTON = 10000;
    /// <summary>The tool tips cover these button ranges.</summary>
    inline constexpr UINT LAST_TERRAIN_TIP = 0x17d3;
    inline constexpr UINT LAST_BUILDING_TIP = 0x97;
    inline constexpr UINT LAST_OVERLAY_TIP = 0x13b9;
    inline constexpr UINT LAST_UNIT_TIP = 0xf9;
    /// <summary>A terrain button's variant is in bits 4-7 of its id less 6000; the texture in bits 0-3.</summary>
    inline constexpr UINT TERRAIN_TEXTURE_MASK = 0xf;
    inline constexpr int TERRAIN_VARIANT_SHIFT = 4;
    /// <summary>The second bridge button's kind has this bit.</summary>
    inline constexpr int SECOND_BRIDGE_BIT = 0x20;
    /// <summary>The variants of the concrete and crop textures the Terrains page shows beyond the first.</summary>
    inline constexpr int CONCRETE_VARIANTS = 5;
    inline constexpr int CROP_VARIANTS = 2;
    /// <summary>The overlay kinds the overlay pages look at.</summary>
    inline constexpr int PAGE_OVERLAY_KINDS = 11;
    /// <summary>The overlay corner layouts the Force page shows (each corner on or off).</summary>
    inline constexpr int OVERLAY_CORNER_SETS = 16;
    /// <summary>The height of a page's drop-down list.</summary>
    inline constexpr int DROP_DOWN_HEIGHT = 0x8c;
    /// <summary>The combo boxes' style: a sorted one for the overlay kinds.</summary>
    inline constexpr DWORD GROUP_LIST_STYLE = WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST;
    inline constexpr DWORD SORTED_LIST_STYLE = GROUP_LIST_STYLE | CBS_SORT;
    /// <summary>The first radio button of a group, and the rest.</summary>
    inline constexpr DWORD FIRST_RADIO_STYLE = WS_CHILD | WS_VISIBLE | WS_GROUP | BS_AUTORADIOBUTTON;
    inline constexpr DWORD RADIO_STYLE = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    /// <summary>The space between the radio buttons and around them.</summary>
    inline constexpr int RADIO_GAP = 6;
    /// <summary>The colour of the disabled images' checkerboard.</summary>
    inline constexpr uint8_t DISABLED_COLOR = 0x1b;

    /// <summary>The image size as TB_SETBITMAPSIZE takes it.</summary>
    LPARAM bitmapSize()
    {
        return MAKELONG(paletteImageWidth, paletteImageHeight);
    }

    /// <summary>The button size as TB_SETBUTTONSIZE takes it.</summary>
    LPARAM buttonSize()
    {
        return MAKELONG(BUTTON_PADDING_X + paletteImageWidth, BUTTON_PADDING_Y + paletteImageHeight);
    }

    /// <summary>The width of a tab, a button with its padding.</summary>
    int buttonWidth()
    {
        return BUTTON_PADDING_X + paletteImageWidth;
    }

    /// <summary>A separator <paramref name="width"/> wide with fsState <paramref name="state"/>.</summary>
    TBBUTTON separator(int width, BYTE state)
    {
        TBBUTTON button = {};
        button.iBitmap = width;
        button.idCommand = SEPARATOR_COMMAND;
        button.fsState = state;
        button.fsStyle = TBSTYLE_SEP;
        return button;
    }

    /// <summary>Sets the thumbnails' DIB header to the palette's image size, top-down.</summary>
    void prepareThumbnailInfo()
    {
        paletteBitmapInfo->bmiHeader.biWidth = paletteImageWidth;
        paletteBitmapInfo->bmiHeader.biHeight = -paletteImageHeight;
        paletteBitmapInfo->bmiHeader.biSizeImage = 0;
    }

    /// <summary>The overlay kinds of the first eleven that the tile library has.</summary>
    std::vector<int32_t> pageOverlayKinds()
    {
        std::vector<int32_t> kinds;

        for (int32_t kind = 0; kind < PAGE_OVERLAY_KINDS; kind++)
        {
            if (tileLibrary->overlays[kind][0] != nullptr)
            {
                kinds.push_back(kind);
            }
        }

        return kinds;
    }

    /// <summary>The name of overlay kind <paramref name="kind"/>, null when the library has none.</summary>
    const char* overlayName(int32_t kind)
    {
        const OverlaySet* set = tileLibrary->overlays[kind][0];
        return set != nullptr ? set->name : nullptr;
    }

    /// <summary>The id of building type <paramref name="type"/> of object category <paramref name="group"/>.</summary>
    uint32_t buildingId(int32_t group, int32_t type)
    {
        const ObjectType& objectType =
            objectLibrary->objectGroups[static_cast<size_t>(group)].types[static_cast<size_t>(type)];
        return makeObjectId(static_cast<uint32_t>(group), static_cast<uint32_t>(type), 0,
                            static_cast<uint32_t>(objectType.objectClass));
    }

    /// <summary>The id of the first frame of unit type <paramref name="type"/> of mover category <paramref name="group"/>.</summary>
    uint32_t unitId(int32_t group, int32_t type)
    {
        return objectLibrary->moverGroups[static_cast<size_t>(group)]
            .types[static_cast<size_t>(type)]
            .frames[0]
            .slots[0]
            .id;
    }
}

MapFeaturesBar::MapFeaturesBar(bool multiplayer) : multiplayer(multiplayer)
{
}

MapFeaturesBar::~MapFeaturesBar()
{
    for (CWnd*& control : pageControls)
    {
        if (control != nullptr)
        {
            control->DestroyWindow();
            delete control;
            control = nullptr;
        }
    }
}

BEGIN_MESSAGE_MAP(MapFeaturesBar, PaletteBar)
ON_WM_CREATE()
ON_WM_DESTROY()
ON_NOTIFY(TCN_SELCHANGE, ID_TABS, &MapFeaturesBar::OnSelChangeTabs)
ON_CBN_SELCHANGE(ID_MOVER_GROUPS, &MapFeaturesBar::OnMoverGroupChange)
ON_CBN_SELCHANGE(ID_OVERLAY_KINDS, &MapFeaturesBar::OnOverlayKindChange)
ON_CBN_SELCHANGE(ID_OBJECT_GROUPS, &MapFeaturesBar::OnObjectGroupChange)
ON_COMMAND_RANGE(ID_SIDE_2_RADIO, ID_LAST_SIDE_RADIO, &MapFeaturesBar::OnSideRadio)
ON_COMMAND_RANGE(ID_VARIANT_FIRST, ID_VARIANT_LAST, &MapFeaturesBar::OnVariantRadio)
END_MESSAGE_MAP()

CView* MapFeaturesBar::activeView()
{
    // Original behaviour: no main window or view crashes here.
    return static_cast<CFrameWnd*>(AfxGetThread()->GetMainWnd())->GetActiveView();
}

int MapFeaturesBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CToolBar::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    // The tab row: a margin, the tab control's place, a margin and a wrap.
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, 0, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = buttonWidth() * columns;
    SendMessage(TB_INSERTBUTTON, TAB_PLACEHOLDER, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = SEPARATOR_WIDTH;
    SendMessage(TB_INSERTBUTTON, 2, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = 1;
    button.fsState = WRAP_STATE;
    SendMessage(TB_INSERTBUTTON, 3, reinterpret_cast<LPARAM>(&button));
    CRect rect;
    SendMessage(TB_GETITEMRECT, TAB_PLACEHOLDER, reinterpret_cast<LPARAM>(&rect));
    rect.InflateRect(SEPARATOR_WIDTH, 0, SEPARATOR_WIDTH, 0);
    const int tabLeft = rect.left;
    const int tabWidth = rect.right - rect.left;
    tabs.Create(WS_VISIBLE | WS_TABSTOP | TCS_MULTILINE, rect, this, ID_TABS);
    CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(SendMessage(WM_GETFONT, 0, 0)));
    ::SendMessage(tabs.m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font != nullptr ? font->m_hObject : nullptr), TRUE);
    CString name;

    for (int i = 0; i < TAB_COUNT; i++)
    {
        name.LoadString(TAB_NAME_STRINGS + i);
        tabs.InsertItem(TCIF_TEXT, i, name, 0, 0);
    }

    ::SendMessage(tabs.m_hWnd, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&rect));

    if (multiplayer)
    {
        ::SendMessage(tabs.m_hWnd, TCM_DELETEITEM, UNITS_PAGE, 0);
    }

    // The frame lines: under the tabs, white down the left, grey and black down the right, black along the bottom.
    const int displayTop = rect.top;
    rect.top = rect.bottom + 2;
    rect.bottom += 4;
    rect.left -= 3;
    rect.right += 2;
    tabUnderline.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this, static_cast<UINT>(-1));
    rect.left = tabLeft;
    rect.right = tabLeft + 1;
    rect.bottom = displayTop + (paletteImageHeight + BUTTON_PADDING_Y) * 6;
    leftLine.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_WHITERECT, rect, this, static_cast<UINT>(-1));
    rect.left = rect.left + tabWidth - 2;
    rect.right = rect.left + 1;
    rightLine.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_GRAYRECT, rect, this, static_cast<UINT>(-1));
    rect.left++;
    rect.right = rect.left + 1;
    rightEdge.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_BLACKRECT, rect, this, static_cast<UINT>(-1));
    rect.left -= tabWidth;
    rect.top = rect.bottom;
    rect.bottom++;
    bottomEdge.Create(nullptr, WS_CHILD | WS_VISIBLE | SS_BLACKRECT, rect, this, static_cast<UINT>(-1));
    ::SendMessage(tabs.m_hWnd, TCM_SETCURFOCUS, 0, 0);
    LRESULT result;
    OnSelChangeTabs(nullptr, &result);
    SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    return static_cast<int>(result);
}

void MapFeaturesBar::OnDestroy()
{
    CToolBar::OnDestroy();
}

const char* MapFeaturesBar::getButtonText(UINT id)
{
    // The tile library's name doubles as the buffer of cliff's name.
    const char* text = tileLibrary->name;

    if (id >= static_cast<UINT>(FIRST_TERRAIN_BUTTON) && id <= LAST_TERRAIN_TIP)
    {
        const UINT texture = id & TERRAIN_TEXTURE_MASK;

        if (texture == TEXTURE_CLIFF)
        {
            if (tileLibrary->name[0] == 0)
            {
                LoadStringA(GetModuleHandleA(nullptr), texture + IDS_DIRT + static_cast<UINT>(tileSetStringOffset),
                            tileLibrary->name, sizeof(tileLibrary->name));
            }
        }
        else
        {
            text = static_cast<PureTileSet*>(tileLibrary->sets[texture][texture])->name;
        }

        if (id - FIRST_TERRAIN_BUTTON > TERRAIN_TEXTURE_MASK)
        {
            strcpy_s(tipText, text);
            char number[0x20];
            _itoa_s(static_cast<int>(id - 0x70) >> TERRAIN_VARIANT_SHIFT & 0xf, number, 10);
            strcat_s(tipText, " ");
            strcat_s(tipText, number);
            return tipText;
        }

        return text;
    }

    if (id >= FIRST_BUILDING_BUTTON && id <= LAST_BUILDING_TIP)
    {
        CWnd* list = GetDlgItem(ID_OBJECT_GROUPS);

        if (list == nullptr)
        {
            return nullptr;
        }

        const auto group = static_cast<int32_t>(list->SendMessage(CB_GETCURSEL, 0, 0));
        const uint32_t objectId = buildingId(group, static_cast<int32_t>(id - FIRST_BUILDING_BUTTON));
        const int32_t objectClass = objectIdClass(objectId);

        if (objectClass == OBJECT_CLASS_DROPZONE)
        {
            return "";
        }

        const std::vector<ObjectGroup>& groups =
            isMoverClass(objectClass) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
        return groups[objectIdGroup(objectId)].types[objectIdType(objectId)].name.c_str();
    }

    if (id >= static_cast<UINT>(FIRST_OVERLAY_BUTTON) && id <= LAST_OVERLAY_TIP)
    {
        const char* number = " 1";
        UINT kindButton = id;

        if (id == static_cast<UINT>(FIRST_OVERLAY_BUTTON + (OVERLAY_BRIDGE | SECOND_BRIDGE_BIT)))
        {
            number = " 2";
            kindButton = FIRST_OVERLAY_BUTTON + OVERLAY_BRIDGE;
        }

        const auto kind = static_cast<int32_t>(kindButton - FIRST_OVERLAY_BUTTON);
        // Original behaviour: a kind past the library's (an id the page doesn't make) reads outside its table.
        const char* name = kind < OVERLAY_KINDS ? overlayName(kind) : nullptr;
        strcpy_s(tipText, name != nullptr ? name : "");

        if (kind == OVERLAY_BRIDGE)
        {
            strcat_s(tipText, number);
        }

        return tipText;
    }

    if (id >= FIRST_UNIT_BUTTON && id <= LAST_UNIT_TIP)
    {
        CWnd* list = GetDlgItem(ID_MOVER_GROUPS);

        if (list == nullptr)
        {
            return nullptr;
        }

        const auto group = static_cast<int32_t>(list->SendMessage(CB_GETCURSEL, 0, 0));
        const uint32_t moverId = unitId(group, static_cast<int32_t>(id - FIRST_UNIT_BUTTON));
        return objectLibrary->moverGroups[objectIdGroup(moverId)].types[objectIdType(moverId)].name.c_str();
    }

    return nullptr;
}

int32_t MapFeaturesBar::checkedButtonData()
{
    TBBUTTON button;

    for (WPARAM index = 0; static_cast<LRESULT>(index) < SendMessage(TB_BUTTONCOUNT, 0, 0); index++)
    {
        SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));

        if ((button.fsState & TBSTATE_CHECKED) != 0)
        {
            return static_cast<int32_t>(button.dwData);
        }
    }

    return -1;
}

int32_t MapFeaturesBar::buttonData(UINT id)
{
    TBBUTTON button;
    const WPARAM index = SendMessage(TB_COMMANDTOINDEX, id, 0);
    SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));
    return static_cast<int8_t>(button.dwData);
}

void MapFeaturesBar::cyclePage(int32_t delta)
{
    auto index = static_cast<int32_t>(::SendMessage(tabs.m_hWnd, TCM_GETCURSEL, 0, 0)) + delta;
    const auto count = static_cast<int32_t>(::SendMessage(tabs.m_hWnd, TCM_GETITEMCOUNT, 0, 0));

    if (index < 0)
    {
        index = count - 1;
    }
    else if (count <= index)
    {
        index = 0;
    }

    ::SendMessage(tabs.m_hWnd, TCM_SETCURSEL, static_cast<WPARAM>(index), 0);
    LRESULT result;
    OnSelChangeTabs(nullptr, &result);
}

void MapFeaturesBar::OnSelChangeTabs([[maybe_unused]] NMHDR* pNMHDR, LRESULT* pResult)
{
    for (CWnd*& control : pageControls)
    {
        if (control != nullptr)
        {
            control->DestroyWindow();
            delete control;
            control = nullptr;
        }
    }

    pageControls.clear();

    if (images.m_hImageList != nullptr)
    {
        images.DeleteImageList();
    }

    if (disabledImages.m_hImageList != nullptr)
    {
        disabledImages.DeleteImageList();
    }

    for (auto index = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0)) - 1; index >= TAB_ROW_BUTTONS; index--)
    {
        SendMessage(TB_DELETEBUTTON, static_cast<WPARAM>(index), 0);
    }

    const auto selected = static_cast<int32_t>(::SendMessage(tabs.m_hWnd, TCM_GETCURSEL, 0, 0));

    // The page builders, by tab (0x00527db0).
    switch (selected)
    {
        case TERRAINS_PAGE:
        {
            buildTerrainsPage();
            break;
        }

        case OVERLAYS_PAGE:
        {
            buildOverlaysPage();
            break;
        }

        case FORCE_PAGE:
        {
            buildForcePage();
            break;
        }

        case BUILDINGS_PAGE:
        {
            buildBuildingsPage();
            break;
        }

        case UNITS_PAGE:
        {
            buildUnitsPage();
            break;
        }

        default:
            break;
    }

    page = selected;
    *pResult = 0;
}

BOOL MapFeaturesBar::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    if (message == WM_COMMAND)
    {
        TBBUTTON button;

        for (WPARAM index = 0; static_cast<LRESULT>(index) < SendMessage(TB_BUTTONCOUNT, 0, 0); index++)
        {
            SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));

            if ((button.fsState & TBSTATE_PRESSED) == 0 && (button.fsStyle & TBSTYLE_SEP) == 0 &&
                button.idCommand != SEPARATOR_COMMAND && static_cast<WPARAM>(button.idCommand) != wParam)
            {
                SendMessage(TB_CHECKBUTTON, static_cast<WPARAM>(button.idCommand), FALSE);
            }
        }

        if (wParam >= FIRST_BUILDING_BUTTON && wParam <= LAST_BUILDING_BUTTON)
        {
            selectObjectButton(wParam);
        }
        else if (wParam >= FIRST_UNIT_BUTTON && wParam <= LAST_UNIT_BUTTON)
        {
            selectUnitButton(wParam);
        }
    }

    return CWnd::OnChildNotify(message, wParam, lParam, pResult);
}

void MapFeaturesBar::OnSideRadio(UINT nID)
{
    if (nID == ID_SIDE_2_RADIO)
    {
        side = 2;
    }
    else if (nID == ID_SIDE_1_RADIO)
    {
        side = 1;
    }
    else if (nID == ID_NEUTRAL_RADIO)
    {
        side = 0;
    }

    const LRESULT count = SendMessage(TB_BUTTONCOUNT, 0, 0);
    TBBUTTON button;
    WPARAM index = 0;

    if (count <= 0)
    {
        return;
    }

    for (;;)
    {
        SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));

        if ((button.fsState & TBSTATE_CHECKED) != 0)
        {
            break;
        }

        index++;

        if (count <= static_cast<LRESULT>(index))
        {
            return;
        }
    }

    activeView()->SendMessage(WM_EDITOR_SELECT_OBJECT, static_cast<WPARAM>(-1), -1);
    selectedButton = static_cast<WPARAM>(-1);
    const LRESULT shown = ::SendMessage(tabs.m_hWnd, TCM_GETCURSEL, 0, 0);

    if (shown == BUILDINGS_PAGE)
    {
        selectObjectButton(static_cast<WPARAM>(button.idCommand));
    }
    else if (::SendMessage(tabs.m_hWnd, TCM_GETCURSEL, 0, 0) == UNITS_PAGE)
    {
        selectUnitButton(static_cast<WPARAM>(button.idCommand));
    }

    SendMessage(TB_CHECKBUTTON, static_cast<WPARAM>(button.idCommand), TRUE);
}

void MapFeaturesBar::OnVariantRadio(UINT nID)
{
    variant = static_cast<int32_t>(nID - ID_VARIANT_FIRST);
    const LRESULT count = SendMessage(TB_BUTTONCOUNT, 0, 0);
    TBBUTTON button;

    for (WPARAM index = 0; static_cast<LRESULT>(index) < count; index++)
    {
        SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));

        if ((button.fsState & TBSTATE_CHECKED) != 0)
        {
            SendMessage(TB_PRESSBUTTON, static_cast<WPARAM>(button.idCommand), FALSE);
            selectUnitButton(static_cast<WPARAM>(button.idCommand));
            SendMessage(TB_CHECKBUTTON, static_cast<WPARAM>(button.idCommand), TRUE);
        }
    }
}

bool MapFeaturesBar::makeDisabledImage(CBitmap* disabled, CBitmap* image)
{
    BITMAP info;
    GetObjectA(image->m_hObject, sizeof(info), &info);
    const int size = info.bmWidth * info.bmHeight;
    std::vector<uint8_t> bits(static_cast<size_t>(size));
    GetBitmapBits(static_cast<HBITMAP>(image->m_hObject), size, bits.data());

    for (int y = 0; y < info.bmHeight; y += 2)
    {
        for (int x = 0; x < info.bmWidth; x += 2)
        {
            bits[static_cast<size_t>(y * info.bmWidth + x)] = DISABLED_COLOR;
        }

        // Original behaviour: an odd height writes the last row's pattern one row past the bits.
        for (int x = 1; x < info.bmWidth && y + 1 < info.bmHeight; x += 2)
        {
            bits[static_cast<size_t>((y + 1) * info.bmWidth + x)] = DISABLED_COLOR;
        }
    }

    SetBitmapBits(static_cast<HBITMAP>(disabled->m_hObject), size, bits.data());
    return true;
}

int MapFeaturesBar::addImages(HBITMAP image, HBITMAP disabled)
{
    CBitmap* imageBitmap = CBitmap::FromHandle(image);
    CBitmap* disabledBitmap = CBitmap::FromHandle(disabled);
    const int index = ImageList_Add(images.m_hImageList, image, image);
    makeDisabledImage(disabledBitmap, imageBitmap);
    ImageList_Add(disabledImages.m_hImageList, disabled, disabled);
    return index;
}

void MapFeaturesBar::buildTerrainsPage()
{
    if (page != -1 && page != TERRAINS_PAGE)
    {
        activeView()->SendMessage(WM_EDITOR_SELECT_MOVER, static_cast<WPARAM>(-1), -1);
        selectedButton = static_cast<WPARAM>(-1);
    }

    CDC* windowDc = CDC::FromHandle(::GetDC(m_hWnd));
    auto memoryDc = std::make_unique<CDC>();
    memoryDc->Attach(CreateCompatibleDC(windowDc != nullptr ? windowDc->m_hDC : nullptr));
    std::vector<uint8_t> textures;

    for (uint8_t texture = 0; texture < TEXTURE_SLOTS; texture++)
    {
        if (tileLibrary->sets[texture][texture] != nullptr || texture == TEXTURE_CLIFF)
        {
            textures.push_back(texture);
        }
    }

    prepareThumbnailInfo();
    const auto count = static_cast<int>(textures.size());

    if (images.m_hImageList == nullptr)
    {
        images.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);
    }

    if (disabledImages.m_hImageList == nullptr)
    {
        disabledImages.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);
    }

    const auto addThumbnail = [&](uint8_t texture, int32_t textureVariant)
    {
        const HBITMAP image =
            tileLibrary->drawTextureThumbnail(memoryDc->m_hDC, texture, paletteBitmapInfo, textureVariant);
        const HBITMAP disabled =
            tileLibrary->drawTextureThumbnail(memoryDc->m_hDC, texture, paletteBitmapInfo, textureVariant);

        // Original behaviour: a texture with no tile crashes; the port skips it.
        if (image != nullptr && disabled != nullptr)
        {
            addImages(image, disabled);
        }

        DeleteObject(image);
        DeleteObject(disabled);
    };

    for (const uint8_t texture : textures)
    {
        addThumbnail(texture, 0);

        if (texture == TEXTURE_CONC)
        {
            for (int32_t textureVariant = 1; textureVariant < CONCRETE_VARIANTS; textureVariant++)
            {
                addThumbnail(texture, textureVariant);
            }
        }
        else if (texture == TEXTURE_CROP)
        {
            addThumbnail(texture, 1);
        }
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, TAB_ROW_BUTTONS, reinterpret_cast<LPARAM>(&button));

    // The grid: the plain textures (concrete and crop are the last two), concrete's five variants, crop's two, then
    // blank spaces. Each row ends with its last button wrapped and a margin separator.
    for (int32_t row = 0; row < rows - 1; row++)
    {
        for (int32_t column = 0; column < columns; column++)
        {
            const int32_t index = columns * row + column;
            button.iBitmap = index;

            if (index < count - 2)
            {
                button.idCommand = textures[static_cast<size_t>(index)] + FIRST_TERRAIN_BUTTON;
                button.dwData = 0;
                button.iString = 0;
                button.fsStyle = TBSTYLE_CHECK;
            }
            else if (index < count + 3)
            {
                button.dwData = static_cast<DWORD_PTR>(index - count + 3);
                button.iString = 0;
                button.idCommand =
                    static_cast<int>(button.dwData << TERRAIN_VARIANT_SHIFT | TEXTURE_CONC) + FIRST_TERRAIN_BUTTON;
                button.fsStyle = TBSTYLE_CHECK;
            }
            else if (index < count + 3 + CROP_VARIANTS)
            {
                button.dwData = static_cast<DWORD_PTR>(index - count - 2);
                button.iString = 0;
                button.idCommand =
                    static_cast<int>(button.dwData << TERRAIN_VARIANT_SHIFT | TEXTURE_CROP) + FIRST_TERRAIN_BUTTON;
                button.fsStyle = TBSTYLE_CHECK;
            }
            else
            {
                button.iBitmap = buttonWidth();
                button.idCommand = SEPARATOR_COMMAND;
                button.fsStyle = TBSTYLE_SEP;
                button.dwData = static_cast<DWORD_PTR>(paletteImageWidth);
            }

            if (column == columns - 1)
            {
                button.fsState = WRAP_STATE;
                SendMessage(TB_INSERTBUTTON, static_cast<WPARAM>((columns + 2) * row + 5 + column),
                            reinterpret_cast<LPARAM>(&button));
                button.fsStyle = TBSTYLE_SEP;
                button.iBitmap = SEPARATOR_WIDTH;
                column++;
            }

            button.fsState = TBSTATE_ENABLED;
            SendMessage(TB_INSERTBUTTON, static_cast<WPARAM>((columns + 2) * row + 5 + column),
                        reinterpret_cast<LPARAM>(&button));
        }
    }

    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    ::ReleaseDC(m_hWnd, windowDc != nullptr ? windowDc->m_hDC : nullptr);
    // Original behaviour: the memory DC is released as if it were the window's (which fails) before it is deleted.
    ::ReleaseDC(m_hWnd, memoryDc->m_hDC);
}

void MapFeaturesBar::buildOverlaysPage()
{
    activeView()->SendMessage(WM_EDITOR_SELECT_MOVER, static_cast<WPARAM>(-1), -1);
    selectedButton = static_cast<WPARAM>(-1);
    CDC* windowDc = CDC::FromHandle(::GetDC(m_hWnd));

    if (windowDc == nullptr)
    {
        return;
    }

    auto memoryDc = std::make_unique<CDC>();
    memoryDc->Attach(CreateCompatibleDC(windowDc->m_hDC));
    const std::vector<int32_t> kinds = pageOverlayKinds();
    images.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, static_cast<int>(kinds.size()), 1);
    disabledImages.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, static_cast<int>(kinds.size()), 1);
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    prepareThumbnailInfo();
    SendMessage(TB_INSERTBUTTON, TAB_ROW_BUTTONS, reinterpret_cast<LPARAM>(&button));
    button.fsStyle = TBSTYLE_CHECK;
    int32_t placed = 1;
    WPARAM index = TAB_ROW_BUTTONS + 1;

    // Ends the row with a wrapped separator and starts the next with a margin when it is full.
    const auto wrapRow = [&](int32_t filled)
    {
        if (filled % (columns + 2) != columns + 1)
        {
            return false;
        }

        button.idCommand = SEPARATOR_COMMAND;
        button.fsStyle = TBSTYLE_SEP;
        button.fsState = WRAP_STATE;
        button.iBitmap = SEPARATOR_WIDTH;
        SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
        button.fsState = TBSTATE_ENABLED;
        SendMessage(TB_INSERTBUTTON, index + 1, reinterpret_cast<LPARAM>(&button));
        index += 2;
        return true;
    };

    // Adds a button for overlay kind or bridge half kind with the given corners.
    const auto addButton = [&](int32_t kind, int32_t command, const int32_t* corners)
    {
        const HBITMAP image = tileLibrary->drawOverlayThumbnail(memoryDc->m_hDC, kind, paletteBitmapInfo, corners);

        if (image == nullptr)
        {
            return false;
        }

        const HBITMAP disabled = tileLibrary->drawOverlayThumbnail(memoryDc->m_hDC, kind, paletteBitmapInfo, corners);
        button.iBitmap = addImages(image, disabled);
        DeleteObject(image);
        DeleteObject(disabled);
        button.idCommand = command + FIRST_OVERLAY_BUTTON;
        button.dwData = static_cast<DWORD_PTR>(command);
        button.iString = 0;
        button.fsStyle = TBSTYLE_CHECK;
        button.fsState = TBSTATE_ENABLED;
        SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
        index++;
        return true;
    };

    for (const int32_t kind : kinds)
    {
        if (kind == OVERLAY_RAIL_BRIDGE || kind == OVERLAY_BRIDGE || kind == OVERLAY_RAILROAD)
        {
            continue;
        }

        // Original behaviour: a kind with no overlay of that layout crashes; the port leaves it out.
        if (!addButton(kind, kind, nullptr))
        {
            continue;
        }

        if (wrapRow(placed + 1))
        {
            placed += 3;
        }
        else
        {
            placed++;
        }
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    // The bridge gets a button for each of its two directions (corners 0 and 2, corners 1 and 3).
    auto filled = static_cast<int32_t>(index) - TAB_ROW_BUTTONS;

    for (const int32_t kind : kinds)
    {
        if (kind != OVERLAY_BRIDGE)
        {
            continue;
        }

        if (wrapRow(filled))
        {
            filled += 2;
        }

        const int32_t firstHalf[4] = {kind, OVERLAY_NONE, kind, OVERLAY_NONE};

        if (addButton(kind, kind, firstHalf))
        {
            filled++;
        }

        const int32_t secondHalf[4] = {OVERLAY_NONE, kind, OVERLAY_NONE, kind};

        if (addButton(kind, kind | SECOND_BRIDGE_BIT, secondHalf))
        {
            filled++;
        }
    }

    // Blank wrapped rows down to the grid's height.
    button = separator(SEPARATOR_WIDTH, WRAP_STATE);
    const int32_t perRow = columns + 2;
    const auto end = static_cast<int32_t>(index);

    for (int32_t row = end / perRow - 1 + (end % perRow != 0 ? 1 : 0); row < rows; row++)
    {
        SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
        index++;
    }

    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    ::ReleaseDC(m_hWnd, windowDc->m_hDC);
    // Original behaviour: the memory DC is released as if it were the window's (which fails) before it is deleted.
    ::ReleaseDC(m_hWnd, memoryDc->m_hDC);
}

void MapFeaturesBar::buildForcePage()
{
    activeView()->SendMessage(WM_EDITOR_SELECT_MOVER, static_cast<WPARAM>(-1), -1);
    selectedButton = static_cast<WPARAM>(-1);
    pageControls.assign(1, nullptr);
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, 4, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = buttonWidth() * columns;
    SendMessage(TB_INSERTBUTTON, 5, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = SEPARATOR_WIDTH;
    SendMessage(TB_INSERTBUTTON, 6, reinterpret_cast<LPARAM>(&button));
    button.iBitmap = 1;
    button.fsState = WRAP_STATE;
    SendMessage(TB_INSERTBUTTON, 7, reinterpret_cast<LPARAM>(&button));
    auto* list = new CComboBox;
    pageControls[0] = list;
    CRect rect;
    SendMessage(TB_GETITEMRECT, 5, reinterpret_cast<LPARAM>(&rect));
    // Original behaviour: the list's bottom is set to 140, not 140 below its top.
    rect.bottom = DROP_DOWN_HEIGHT;
    list->Create(SORTED_LIST_STYLE, rect, this, ID_OVERLAY_KINDS);
    const std::vector<int32_t> kinds = pageOverlayKinds();
    CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(SendMessage(WM_GETFONT, 0, 0)));
    list->SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(font != nullptr ? font->m_hObject : nullptr), TRUE);

    for (const int32_t kind : kinds)
    {
        if (kind != OVERLAY_RAIL_BRIDGE && kind != OVERLAY_RAILROAD)
        {
            const LRESULT item = list->SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(overlayName(kind)));
            list->SendMessage(CB_SETITEMDATA, static_cast<WPARAM>(item), kind);
        }
    }

    list->SendMessage(CB_SETCURSEL, 0, 0);
    OnOverlayKindChange();
}

void MapFeaturesBar::buildBuildingsPage()
{
    pageControls.assign(4, nullptr);
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);

    // Two rows of separators, inserted ahead of the tab row: the list goes on the second's.
    for (WPARAM rowStart = 0; rowStart <= 4; rowStart += 4)
    {
        button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
        SendMessage(TB_INSERTBUTTON, rowStart, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = buttonWidth() * columns;
        SendMessage(TB_INSERTBUTTON, rowStart + 1, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = SEPARATOR_WIDTH;
        SendMessage(TB_INSERTBUTTON, rowStart + 2, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = 1;
        button.fsState = WRAP_STATE;
        SendMessage(TB_INSERTBUTTON, rowStart + 3, reinterpret_cast<LPARAM>(&button));
    }

    CRect rect;
    GetItemRect(5, &rect);
    rect.bottom = rect.top + DROP_DOWN_HEIGHT;
    auto* list = new CComboBox;
    pageControls[0] = list;
    list->Create(GROUP_LIST_STYLE, rect, this, ID_OBJECT_GROUPS);
    CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(SendMessage(WM_GETFONT, 0, 0)));
    list->SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(font != nullptr ? font->m_hObject : nullptr), TRUE);

    for (const ObjectGroup& objectGroup : objectLibrary->objectGroups)
    {
        list->SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(objectGroup.name.c_str()));
    }

    list->SendMessage(CB_SETCURSEL, 0, 0);
    CRect sides;
    list->GetWindowRect(&sides);
    ScreenToClient(&sides);
    const int height = sides.bottom - sides.top;
    sides.top = sides.bottom + 3;
    sides.bottom = sides.top + height;
    std::list<CString> labels;
    CString first;
    CString second;
    CString neutral;
    first.LoadString(IDS_SIDE_TEAM_1);
    second.LoadString(IDS_SIDE_TEAM_2);
    neutral.LoadString(IDS_SIDE_NEUTRAL);

    if (!multiplayer)
    {
        first.LoadString(IDS_SIDE_ALLIED);
        second.LoadString(IDS_SIDE_ENEMY);
    }

    labels.push_back(first);
    labels.push_back(second);
    labels.push_back(neutral);
    createSideButtons(sides, &pageControls[1], labels);
    pageControls[3]->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
    // Original behaviour: the side starts at 1 while the neutral button is the one checked.
    side = 1;
    OnObjectGroupChange();
}

void MapFeaturesBar::buildUnitsPage()
{
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);

    // Two rows of separators after the tab row: the list goes on the first, the side buttons on the second.
    for (WPARAM rowStart = 4; rowStart <= 8; rowStart += 4)
    {
        button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
        SendMessage(TB_INSERTBUTTON, rowStart, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = buttonWidth() * columns;
        SendMessage(TB_INSERTBUTTON, rowStart + 1, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = SEPARATOR_WIDTH;
        SendMessage(TB_INSERTBUTTON, rowStart + 2, reinterpret_cast<LPARAM>(&button));
        button.iBitmap = 1;
        button.fsState = WRAP_STATE;
        SendMessage(TB_INSERTBUTTON, rowStart + 3, reinterpret_cast<LPARAM>(&button));
    }

    pageControls.assign(7, nullptr);
    auto* list = new CComboBox;
    pageControls[0] = list;
    CRect rect;
    GetItemRect(5, &rect);
    rect.top += 6;
    rect.bottom = rect.top + DROP_DOWN_HEIGHT;

    if (!list->Create(GROUP_LIST_STYLE, rect, this, ID_MOVER_GROUPS))
    {
        return;
    }

    CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(SendMessage(WM_GETFONT, 0, 0)));
    const auto fontHandle = reinterpret_cast<WPARAM>(font != nullptr ? font->m_hObject : nullptr);
    list->SendMessage(WM_SETFONT, fontHandle, TRUE);

    for (const ObjectGroup& moverGroup : objectLibrary->moverGroups)
    {
        list->SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(moverGroup.name.c_str()));
    }

    list->SendMessage(CB_SETCURSEL, 0, 0);
    CRect sides;
    SendMessage(TB_GETITEMRECT, 9, reinterpret_cast<LPARAM>(&sides));
    const int third = (sides.bottom - rect.top) / 3;
    sides.top = third + rect.top - 6;
    sides.bottom = sides.top + third - 6;
    std::list<CString> labels;
    CString allied;
    CString enemy;
    allied.LoadString(IDS_SIDE_ALLIED);
    enemy.LoadString(IDS_SIDE_ENEMY);
    labels.push_back(allied);
    labels.push_back(enemy);
    createSideButtons(sides, &pageControls[1], labels);

    // The variant group box under the side buttons, and its three buttons.
    sides.top = sides.bottom - 3;
    sides.bottom = sides.top + third + 6;
    auto* box = new CButton;
    pageControls[3] = box;
    CString label;
    label.LoadString(IDS_VARIANT);
    box->Create(label, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, sides, this, static_cast<UINT>(-1));
    box->SendMessage(WM_SETFONT, fontHandle, TRUE);
    CRect radio = sides;
    radio.bottom -= 6;
    radio.left = rect.left + 6;
    radio.top += 0x12;
    const int width = (sides.right - radio.left - 12) / 3;
    radio.right = radio.left + width;
    DWORD style = FIRST_RADIO_STYLE;

    for (int i = 0; i < VARIANT_COUNT; i++)
    {
        label.LoadString(IDS_A + i);
        auto* variantButton = new CButton;
        pageControls[static_cast<size_t>(4 + i)] = variantButton;

        if (!variantButton->Create(label, style, radio, this, ID_VARIANT_FIRST + i))
        {
            return;
        }

        radio.left = radio.right + 6;
        style = RADIO_STYLE;
        radio.right = width + radio.left - 6;
        variantButton->ModifyStyleEx(0, WS_EX_TRANSPARENT, 0);
        variantButton->SendMessage(WM_SETFONT, fontHandle, TRUE);

        if (variant == i)
        {
            variantButton->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
        }
    }

    pageControls[2]->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    side = 1;
    OnMoverGroupChange();
}

void MapFeaturesBar::insertSeparatorRow(WPARAM* index)
{
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, *index, reinterpret_cast<LPARAM>(&button));
    const WPARAM second = (*index)++;
    button.iBitmap = buttonWidth() * columns;
    SendMessage(TB_INSERTBUTTON, second + 1, reinterpret_cast<LPARAM>(&button));
    const WPARAM third = (*index)++;
    button.iBitmap = SEPARATOR_WIDTH;
    SendMessage(TB_INSERTBUTTON, third + 1, reinterpret_cast<LPARAM>(&button));
    const WPARAM fourth = (*index)++;
    button.iBitmap = 1;
    button.fsState = WRAP_STATE;
    SendMessage(TB_INSERTBUTTON, fourth + 1, reinterpret_cast<LPARAM>(&button));
}

void MapFeaturesBar::createSideButtons(const RECT& rect, CWnd** buttons, const std::list<CString>& labels)
{
    const auto count = static_cast<uint32_t>(labels.size());

    if (count == 0)
    {
        return;
    }

    const uint32_t width = static_cast<uint32_t>(rect.right - static_cast<LONG>(count) * RADIO_GAP - rect.left) / count;
    CRect button(rect.left + RADIO_GAP, rect.top, rect.left + RADIO_GAP + static_cast<LONG>(width), rect.bottom);
    DWORD style = FIRST_RADIO_STYLE;
    CFont* font = CFont::FromHandle(reinterpret_cast<HFONT>(SendMessage(WM_GETFONT, 0, 0)));
    UINT id = ID_SIDE_2_RADIO;

    for (const CString& label : labels)
    {
        auto* radio = new CButton;
        *buttons++ = radio;
        radio->Create(label, style, button, this, id++);
        button.left = button.right + RADIO_GAP;
        button.right = button.left + static_cast<LONG>(width);
        style = RADIO_STYLE;
        radio->ModifyStyleEx(0, WS_EX_TRANSPARENT, 0);
        radio->SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(font != nullptr ? font->m_hObject : nullptr), TRUE);
    }
}

void MapFeaturesBar::layoutButtons(int32_t firstId, int32_t endId, WPARAM index, int32_t skipRows)
{
    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
    const auto base = static_cast<int32_t>(index) + 1;
    const int32_t count = endId - firstId;
    int32_t command = firstId;

    for (int32_t row = 0; row < rows - skipRows; row++)
    {
        for (int32_t column = 0; column < columns; column++)
        {
            const int32_t cell = columns * row + column;

            if (cell < count)
            {
                button.iBitmap = cell;
                button.idCommand = command++;
                button.iString = 0;
                button.fsStyle = TBSTYLE_CHECK;
                button.fsState = TBSTATE_ENABLED;
                button.dwData = static_cast<DWORD_PTR>(cell);
            }
            else
            {
                button.iBitmap = buttonWidth();
                button.idCommand = SEPARATOR_COMMAND;
                button.fsStyle = TBSTYLE_SEP;
                button.dwData = static_cast<DWORD_PTR>(paletteImageWidth);
            }

            SendMessage(TB_INSERTBUTTON, static_cast<WPARAM>((columns + 2) * row + column + base),
                        reinterpret_cast<LPARAM>(&button));

            if (column == columns - 1)
            {
                // The row's end: a wrapped separator as wide as the last button's image index (as the original
                // leaves it), then the next row's margin.
                button.fsState = WRAP_STATE;
                button.fsStyle = TBSTYLE_SEP;
                SendMessage(TB_INSERTBUTTON, static_cast<WPARAM>((columns + 2) * row + column + 1 + base),
                            reinterpret_cast<LPARAM>(&button));
                column += 2;
                button.fsState = TBSTATE_ENABLED;
                button.iBitmap = SEPARATOR_WIDTH;
                SendMessage(TB_INSERTBUTTON, static_cast<WPARAM>((columns + 2) * row + column + base),
                            reinterpret_cast<LPARAM>(&button));
            }
        }
    }
}

void MapFeaturesBar::OnMoverGroupChange()
{
    CWnd* list = GetDlgItem(ID_MOVER_GROUPS);

    if (list == nullptr)
    {
        return;
    }

    const auto selected = static_cast<int32_t>(list->SendMessage(CB_GETCURSEL, 0, 0));

    if (selected == CB_ERR)
    {
        return;
    }

    freeGroupShapes();
    group = selected;
    const ObjectGroup& moverGroup = objectLibrary->moverGroups[static_cast<size_t>(selected)];
    const uint32_t firstId = unitId(selected, 0);

    // The variants are for mechs only.
    for (int i = 0; i < VARIANT_COUNT; i++)
    {
        if (CWnd* variantButton = GetDlgItem(ID_VARIANT_FIRST + i); variantButton != nullptr)
        {
            variantButton->EnableWindow(objectIdClass(firstId) != OBJECT_CLASS_VEHICLE);
        }
    }

    const auto count = static_cast<int32_t>(moverGroup.types.size());

    for (auto index = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0)) - 1; index > 0xb; index--)
    {
        SendMessage(TB_DELETEBUTTON, static_cast<WPARAM>(index), 0);
    }

    CDC* windowDc = CDC::FromHandle(::GetDC(m_hWnd));
    auto memoryDc = std::make_unique<CDC>();
    memoryDc->Attach(CreateCompatibleDC(windowDc != nullptr ? windowDc->m_hDC : nullptr));

    if (images.m_hImageList != nullptr)
    {
        images.DeleteImageList();
    }

    if (disabledImages.m_hImageList != nullptr)
    {
        disabledImages.DeleteImageList();
    }

    images.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);
    disabledImages.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);
    prepareThumbnailInfo();

    for (int32_t type = 0; type < count; type++)
    {
        const uint32_t id = unitId(selected, type);
        const HBITMAP image = objectLibrary->drawMoverThumbnail(memoryDc->m_hDC, id, paletteBitmapInfo);

        if (image != nullptr)
        {
            const HBITMAP disabled = objectLibrary->drawMoverThumbnail(memoryDc->m_hDC, id, paletteBitmapInfo);
            addImages(image, disabled);
            DeleteObject(image);
            DeleteObject(disabled);
        }
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    layoutButtons(FIRST_UNIT_BUTTON, count + FIRST_UNIT_BUTTON, 0xc, 3);
    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    // Original behaviour: the memory DC is released as if it were the window's (which fails) before it is deleted.
    ::ReleaseDC(m_hWnd, memoryDc->m_hDC);
    ::ReleaseDC(m_hWnd, windowDc != nullptr ? windowDc->m_hDC : nullptr);
    memoryDc.reset();
    selectUnitButton(static_cast<WPARAM>(-1));
}

void MapFeaturesBar::OnOverlayKindChange()
{
    CWnd* list = GetDlgItem(ID_OVERLAY_KINDS);

    if (list == nullptr)
    {
        return;
    }

    const LRESULT selected = list->SendMessage(CB_GETCURSEL, 0, 0);

    if (selected == CB_ERR)
    {
        return;
    }

    const auto kind = static_cast<int32_t>(list->SendMessage(CB_GETITEMDATA, static_cast<WPARAM>(selected), 0));

    for (auto index = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0)) - 1; index > 7; index--)
    {
        SendMessage(TB_DELETEBUTTON, static_cast<WPARAM>(index), 0);
    }

    CDC* windowDc = CDC::FromHandle(::GetDC(m_hWnd));
    auto memoryDc = std::make_unique<CDC>();
    memoryDc->Attach(CreateCompatibleDC(windowDc != nullptr ? windowDc->m_hDC : nullptr));
    prepareThumbnailInfo();
    // Each corner set's image index, 0xff where the kind has no overlay of that layout.
    uint8_t imageOf[OVERLAY_CORNER_SETS];
    std::fill(std::begin(imageOf), std::end(imageOf), static_cast<uint8_t>(0xff));

    if (images.m_hImageList != nullptr)
    {
        images.DeleteImageList();
    }

    if (disabledImages.m_hImageList != nullptr)
    {
        disabledImages.DeleteImageList();
    }

    images.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, OVERLAY_CORNER_SETS, 2);
    disabledImages.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, OVERLAY_CORNER_SETS, 2);
    uint8_t added = 0;

    for (int32_t corners = 0; corners < OVERLAY_CORNER_SETS; corners++)
    {
        int32_t kinds[4];

        for (int32_t corner = 0; corner < 4; corner++)
        {
            kinds[corner] = (corners >> corner & 1) != 0 ? kind : OVERLAY_NONE;
        }

        const HBITMAP image = tileLibrary->drawOverlayThumbnail(memoryDc->m_hDC, kind, paletteBitmapInfo, kinds);

        if (image != nullptr)
        {
            const HBITMAP disabled = tileLibrary->drawOverlayThumbnail(memoryDc->m_hDC, kind, paletteBitmapInfo, kinds);
            imageOf[corners] = added;
            addImages(image, disabled);
            DeleteObject(image);
            DeleteObject(disabled);
            added++;
        }
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    TBBUTTON button = separator(SEPARATOR_WIDTH, TBSTATE_ENABLED);
    SendMessage(TB_INSERTBUTTON, 8, reinterpret_cast<LPARAM>(&button));
    int32_t placed = 1;
    WPARAM index = 9;

    for (int32_t corners = 0; corners < OVERLAY_CORNER_SETS; corners++)
    {
        if (imageOf[corners] == 0xff)
        {
            continue;
        }

        button.iBitmap = imageOf[corners];
        button.idCommand = corners + FIRST_LAYOUT_BUTTON;
        button.iString = 0;
        button.fsStyle = TBSTYLE_CHECK;
        button.fsState = TBSTATE_ENABLED;
        button.dwData = static_cast<DWORD_PTR>(kind);
        SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
        SendMessage(TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&button));
        index++;
        placed++;

        if (placed % (columns + 2) == columns + 1)
        {
            button.idCommand = SEPARATOR_COMMAND;
            button.fsStyle = TBSTYLE_SEP;
            button.fsState = WRAP_STATE;
            button.iBitmap = SEPARATOR_WIDTH;
            SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
            button.fsState = TBSTATE_ENABLED;
            SendMessage(TB_INSERTBUTTON, index + 1, reinterpret_cast<LPARAM>(&button));
            index += 2;
            placed += 2;
        }
    }

    // Blank wrapped rows down to the grid's height.
    button.idCommand = SEPARATOR_COMMAND;
    button.fsStyle = TBSTYLE_SEP;
    button.fsState = WRAP_STATE;
    button.iBitmap = SEPARATOR_WIDTH;

    for (int32_t row = (static_cast<int32_t>(index) - 8) / (columns + 2) + 1; row < rows; row++)
    {
        SendMessage(TB_INSERTBUTTON, index, reinterpret_cast<LPARAM>(&button));
        index++;
    }

    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    // Original behaviour: the memory DC is released as if it were the window's (which fails) before it is deleted.
    ::ReleaseDC(m_hWnd, memoryDc->m_hDC);
    ::ReleaseDC(m_hWnd, windowDc != nullptr ? windowDc->m_hDC : nullptr);
}

void MapFeaturesBar::OnObjectGroupChange()
{
    // Original behaviour: the list is used without testing for it.
    CWnd* list = GetDlgItem(ID_OBJECT_GROUPS);
    const auto selected = static_cast<int32_t>(list->SendMessage(CB_GETCURSEL, 0, 0));

    if (selected == CB_ERR)
    {
        return;
    }

    freeGroupShapes();
    group = selected;
    const ObjectGroup& objectGroup = objectLibrary->objectGroups[static_cast<size_t>(selected)];
    const auto count = static_cast<int32_t>(objectGroup.types.size());

    for (auto index = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0)) - 1; index > 7; index--)
    {
        SendMessage(TB_DELETEBUTTON, static_cast<WPARAM>(index), 0);
    }

    CDC* windowDc = CDC::FromHandle(::GetDC(m_hWnd));
    auto memoryDc = std::make_unique<CDC>();
    memoryDc->Attach(CreateCompatibleDC(windowDc != nullptr ? windowDc->m_hDC : nullptr));
    prepareThumbnailInfo();

    if (images.m_hImageList != nullptr)
    {
        images.DeleteImageList();
    }

    if (disabledImages.m_hImageList != nullptr)
    {
        disabledImages.DeleteImageList();
    }

    images.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);
    disabledImages.Create(paletteImageWidth, paletteImageHeight, ILC_COLOR8, count, 1);

    for (int32_t type = 0; type < count; type++)
    {
        const HBITMAP image = objectLibrary->drawTypeThumbnail(memoryDc->m_hDC, selected, type, paletteBitmapInfo);
        const HBITMAP disabled = objectLibrary->drawTypeThumbnail(memoryDc->m_hDC, selected, type, paletteBitmapInfo);

        // Original behaviour: a type with no shape crashes; the port skips it.
        if (image != nullptr && disabled != nullptr)
        {
            addImages(image, disabled);
        }

        DeleteObject(image);
        DeleteObject(disabled);
    }

    SendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images.m_hImageList));
    SendMessage(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(disabledImages.m_hImageList));
    layoutButtons(FIRST_BUILDING_BUTTON, count + FIRST_BUILDING_BUTTON, 8, 2);
    SendMessage(TB_SETBITMAPSIZE, 0, bitmapSize());
    SendMessage(TB_SETBUTTONSIZE, 0, buttonSize());
    // Original behaviour: the memory DC is released as if it were the window's (which fails) before it is deleted.
    ::ReleaseDC(m_hWnd, memoryDc->m_hDC);
    ::ReleaseDC(m_hWnd, windowDc != nullptr ? windowDc->m_hDC : nullptr);
    memoryDc.reset();

    // Turrets are placed on side 1 in a single-player map.
    if (count > 0 && (objectGroup.types[0].objectClass & 0x7f) == OBJECT_CLASS_TURRET && !multiplayer)
    {
        side = 1;
        OnSideRadio(ID_SIDE_1_RADIO);
        GetDlgItem(ID_SIDE_2_RADIO)->SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
        GetDlgItem(ID_SIDE_1_RADIO)->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
        GetDlgItem(ID_NEUTRAL_RADIO)->SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
    }

    selectObjectButton(static_cast<WPARAM>(-1));
}

void MapFeaturesBar::selectObjectButton(WPARAM id)
{
    // Original behaviour: the list is used without testing for it.
    const auto selected = static_cast<int32_t>(GetDlgItem(ID_OBJECT_GROUPS)->SendMessage(CB_GETCURSEL, 0, 0));
    CommandToIndex(static_cast<UINT>(id));

    if (id == selectedButton || id == static_cast<WPARAM>(-1))
    {
        activeView()->SendMessage(WM_EDITOR_SELECT_OBJECT, static_cast<WPARAM>(-1), -1);
        selectedButton = static_cast<WPARAM>(-1);
        return;
    }

    const uint32_t objectId = buildingId(selected, static_cast<int32_t>(id - FIRST_BUILDING_BUTTON));

    if (!ObjectLibrary::isUnlinkedClass(objectIdClass(objectId)))
    {
        pageControls[1]->SendMessage(BM_SETCHECK, side == 2, 0);
        pageControls[2]->SendMessage(BM_SETCHECK, side == 1, 0);
        pageControls[3]->SendMessage(BM_SETCHECK, side == 0, 0);
        pageControls[1]->EnableWindow(TRUE);
        pageControls[2]->EnableWindow(TRUE);
        pageControls[3]->EnableWindow(TRUE);
    }
    else
    {
        pageControls[1]->SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
        pageControls[2]->SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
        pageControls[3]->SendMessage(BM_SETCHECK, BST_CHECKED, 0);
        pageControls[1]->EnableWindow(FALSE);
        pageControls[2]->EnableWindow(FALSE);
        pageControls[3]->EnableWindow(FALSE);
        side = 0;
    }

    activeView()->SendMessage(WM_EDITOR_SELECT_OBJECT, objectId, side);
    selectedButton = id;
    const auto count = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0));
    WPARAM command = FIRST_BUILDING_BUTTON;

    for (int i = 0; i < count; i++, command++)
    {
        if (command != id)
        {
            SendMessage(TB_SETSTATE, command, TBSTATE_ENABLED);
        }
    }
}

void MapFeaturesBar::selectUnitButton(WPARAM id)
{
    const auto selected = static_cast<int32_t>(GetDlgItem(ID_MOVER_GROUPS)->SendMessage(CB_GETCURSEL, 0, 0));

    if (SendMessage(TB_ISBUTTONPRESSED, id, 0) != 0 || id == static_cast<WPARAM>(-1))
    {
        activeView()->SendMessage(WM_EDITOR_SELECT_MOVER, static_cast<WPARAM>(-1), -1);
        selectedButton = static_cast<WPARAM>(-1);
        return;
    }

    const uint32_t moverId = unitId(selected, static_cast<int32_t>(id - FIRST_UNIT_BUTTON));
    activeView()->SendMessage(WM_EDITOR_SELECT_MOVER, moverId, static_cast<LPARAM>(variant) << 16 | side);
    selectedButton = id;
    const auto count = static_cast<int>(SendMessage(TB_BUTTONCOUNT, 0, 0));
    WPARAM command = FIRST_UNIT_BUTTON;

    for (int i = 0; i < count; i++, command++)
    {
        if (command != id)
        {
            SendMessage(TB_CHECKBUTTON, command, FALSE);
        }
    }
}

void MapFeaturesBar::freeGroupShapes()
{
    if (group == -1)
    {
        return;
    }

    // Original behaviour (EB-077): the loops step through each type's frames but free the first frame's shapes every
    // time, so only those are freed.
    if (page == UNITS_PAGE)
    {
        const ObjectGroup& moverGroup = objectLibrary->moverGroups[static_cast<size_t>(group)];

        for (int32_t type = 0; type < static_cast<int32_t>(moverGroup.types.size()); type++)
        {
            const uint32_t first = unitId(group, type);
            uint32_t id = first;
            objectLibrary->freeShapes(id);
            objectLibrary->stepFrame(1, &id);

            while (id != first)
            {
                objectLibrary->freeShapes(first);
                objectLibrary->stepFrame(1, &id);
            }
        }
    }
    else if (page == BUILDINGS_PAGE)
    {
        const ObjectGroup& objectGroup = objectLibrary->objectGroups[static_cast<size_t>(group)];

        for (int32_t type = 0; type < static_cast<int32_t>(objectGroup.types.size()); type++)
        {
            const uint32_t first = buildingId(group, type);
            uint32_t id = first;
            objectLibrary->freeShapes(first);
            objectLibrary->stepFrame(1, &id);

            while (id != first)
            {
                objectLibrary->freeShapes(first);
                objectLibrary->stepFrame(1, &id);
            }
        }
    }
}

void MapFeaturesBar::onButtonClick(UINT nFlags, CPoint point)
{
    const LRESULT index = SendMessage(TB_HITTEST, 0, reinterpret_cast<LPARAM>(&point));

    if (index < 0)
    {
        return;
    }

    TBBUTTON button;
    SendMessage(TB_GETBUTTON, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&button));

    if (button.idCommand >= 0)
    {
        CToolBar::OnLButtonDown(nFlags, point);
    }
}
