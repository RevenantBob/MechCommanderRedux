#pragma once

#include <list>

#include "PaletteBar.h"

// The Map Features palette (0x00432540-0x00437190, virtual table 0x004fe2a8): a floating tool bar with a tab for each
// kind of map feature (terrains, overlays, overlay corners, buildings, units), each a grid of thumbnail buttons and the
// controls that go with it. Its source file name wasn't kept; the names are the port's.

/// <summary>The view message that picks the object to place: wParam the object id (-1 none), lParam its side.</summary>
inline constexpr UINT WM_EDITOR_SELECT_OBJECT = WM_USER + 257;
/// <summary>
/// The view message that picks the mech or vehicle to place: wParam its id (-1 none), lParam its variant in the high
/// word and side in the low.
/// </summary>
inline constexpr UINT WM_EDITOR_SELECT_MOVER = WM_USER + 263;

/// <summary>The size of the palette's thumbnails (0x00777c98, 0x00777c9c; set by the static initializer 0x00432540).</summary>
extern int32_t paletteImageWidth;
extern int32_t paletteImageHeight;

/// <summary>The Map Features palette.</summary>
/// <remarks>0x464 bytes.</remarks>
class MapFeaturesBar : public PaletteBar
{
public:
    /// <summary>The tabs, in order.</summary>
    enum Page : int32_t
    {
        TERRAINS_PAGE = 0,
        OVERLAYS_PAGE = 1,
        FORCE_PAGE = 2,
        BUILDINGS_PAGE = 3,
        UNITS_PAGE = 4,
    };

    /// <summary>A palette for a single-player map, or a multiplayer one (no Units tab, team names for the sides).</summary>
    /// <remarks>0x00432560.</remarks>
    explicit MapFeaturesBar(bool multiplayer);
    /// <summary>Deletes the page's controls.</summary>
    /// <remarks>0x004326c0.</remarks>
    ~MapFeaturesBar() override;

    /// <summary>
    /// The tool tip of button <paramref name="id"/>: a terrain's name (with its variant), an overlay's (with 1 or 2 for
    /// the bridges), a building's or a unit's type name.
    /// </summary>
    /// <remarks>0x00432c40.</remarks>
    const char* getButtonText(UINT id) override;

    /// <summary>The dwData of the first checked button; -1 when none is.</summary>
    /// <remarks>0x004327f0.</remarks>
    int32_t checkedButtonData();
    /// <summary>The low byte of the dwData of button <paramref name="id"/>, sign-extended.</summary>
    /// <remarks>0x00432860.</remarks>
    int32_t buttonData(UINT id);
    /// <summary>Moves <paramref name="delta"/> tabs on, wrapping around, and shows that page.</summary>
    /// <remarks>0x004366d0.</remarks>
    void cyclePage(int32_t delta);

    /// <summary>The map is a multiplayer one (+0x20c).</summary>
    bool multiplayer;

protected:
    /// <summary>
    /// The buttons' WM_COMMAND: unchecks the other buttons and, for a building or unit, picks it
    /// (<see cref="selectObjectButton"/>, <see cref="selectUnitButton"/>).
    /// </summary>
    /// <remarks>Virtual table slot 42 (0x00436300).</remarks>
    BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;

    /// <summary>
    /// Handles ON_WM_CREATE(): the tab row (a tab control over a separator), the frame lines around the pages, and
    /// the first page.
    /// </summary>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    /// <summary>Handles ON_WM_DESTROY().</summary>
    afx_msg void OnDestroy();
    /// <summary>
    /// Handles ON_NOTIFY(TCN_SELCHANGE, 119): deletes the page's controls, images and buttons and builds the new tab's
    /// page.
    /// </summary>
    afx_msg void OnSelChangeTabs(NMHDR* pNMHDR, LRESULT* pResult);
    /// <summary>Handles ON_CBN_SELCHANGE(120): the units of the chosen category.</summary>
    afx_msg void OnMoverGroupChange();
    /// <summary>Handles ON_CBN_SELCHANGE(540): the sixteen corner layouts of the chosen overlay kind.</summary>
    afx_msg void OnOverlayKindChange();
    /// <summary>Handles ON_CBN_SELCHANGE(530): the buildings of the chosen category.</summary>
    afx_msg void OnObjectGroupChange();
    /// <summary>Handles ON_COMMAND_RANGE(0x406, 0x408): the side (2, 1 or 0) for the checked building or unit.</summary>
    afx_msg void OnSideRadio(UINT nID);
    /// <summary>Handles ON_COMMAND_RANGE(500, 503): the mech variant for the checked unit.</summary>
    afx_msg void OnVariantRadio(UINT nID);

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Builds the Terrains page: a button per texture, the concrete and crop ones with their variants.</summary>
    /// <remarks>0x00434dd0.</remarks>
    void buildTerrainsPage();
    /// <summary>Builds the Overlays page: a button per overlay kind, two for the bridge.</summary>
    /// <remarks>0x00434590.</remarks>
    void buildOverlaysPage();
    /// <summary>Builds the Force page: the overlay kinds' list and the layouts of the chosen one.</summary>
    /// <remarks>0x00433cc0.</remarks>
    void buildForcePage();
    /// <summary>Builds the Buildings page: the categories' list, the side buttons and the chosen category's types.</summary>
    /// <remarks>0x004337c0.</remarks>
    void buildBuildingsPage();
    /// <summary>
    /// Builds the Units page: the categories' list, the side buttons, the mech variants and the chosen category's types.
    /// </summary>
    /// <remarks>0x004330b0.</remarks>
    void buildUnitsPage();
    /// <summary>Inserts a row of separators (12, a tab's width, 12, a wrap) at <paramref name="index"/>, moving it on.</summary>
    /// <remarks>0x00433f50; no caller.</remarks>
    void insertSeparatorRow(WPARAM* index);
    /// <summary>
    /// Creates a radio button per label in <paramref name="labels"/> across <paramref name="rect"/>, ids 0x406 on, into
    /// <paramref name="buttons"/>.
    /// </summary>
    /// <remarks>0x00434020.</remarks>
    void createSideButtons(const RECT& rect, CWnd** buttons, const std::list<CString>& labels);
    /// <summary>
    /// Lays the buttons of commands <paramref name="firstId"/> to <paramref name="endId"/> - 1 out in the grid from
    /// <paramref name="index"/>, in rows of <see cref="columns"/>, filling the rows but the last
    /// <paramref name="skipRows"/> with blank spaces.
    /// </summary>
    /// <remarks>0x004354e0.</remarks>
    void layoutButtons(int32_t firstId, int32_t endId, WPARAM index, int32_t skipRows);
    /// <summary>
    /// Picks building button <paramref name="id"/> for the view (its side buttons enabled when its class takes part in
    /// links, else neutral), or unpicks it when it is picked or -1.
    /// </summary>
    /// <remarks>0x00436080.</remarks>
    void selectObjectButton(WPARAM id);
    /// <summary>Picks unit button <paramref name="id"/> for the view, or unpicks the unit when it is pressed or -1.</summary>
    /// <remarks>0x00436510.</remarks>
    void selectUnitButton(WPARAM id);
    /// <summary>
    /// Frees the shapes of the current Buildings or Units category's types (the first frame's only: EB-077).
    /// </summary>
    /// <remarks>0x00436820.</remarks>
    void freeGroupShapes();
    /// <summary>A left click on a button with an image goes to CToolBar.</summary>
    /// <remarks>0x00436ae0; no caller.</remarks>
    void onButtonClick(UINT nFlags, CPoint point);

    /// <summary>The view of the main frame.</summary>
    static CView* activeView();
    /// <summary>
    /// Makes <paramref name="disabled"/> <paramref name="image"/>'s pixels with every other pixel in a checkerboard
    /// colour 0x1b.
    /// </summary>
    /// <remarks>0x00436750.</remarks>
    static bool makeDisabledImage(CBitmap* disabled, CBitmap* image);
    /// <summary>Adds <paramref name="image"/> to the images and its disabled look to the disabled images.</summary>
    /// <returns>The image's index.</returns>
    int addImages(HBITMAP image, HBITMAP disabled);

    /// <summary>The tabs (+0x1c4).</summary>
    CTabCtrl tabs;
    /// <summary>The page's controls (+0x200, the count at +0x204); null where one wasn't made.</summary>
    std::vector<CWnd*> pageControls;
    /// <summary>The side for the buildings and units: 2, 1 or 0 (+0x208).</summary>
    int32_t side = 0;
    /// <summary>Buttons per row (+0x210).</summary>
    int32_t columns = 4;
    /// <summary>Rows of the grid (+0x214).</summary>
    int32_t rows = 6;
    /// <summary>The mech variant (+0x218).</summary>
    int32_t variant = 2;
    /// <summary>The page's button images (+0x21c).</summary>
    CImageList images;
    /// <summary>The page's disabled button images (+0x224).</summary>
    CImageList disabledImages;
    /// <summary>The frame lines around the pages (+0x22c, +0x268, +0x2a4, +0x2e0, +0x31c).</summary>
    CStatic leftLine;
    CStatic rightEdge;
    CStatic bottomEdge;
    CStatic tabUnderline;
    CStatic rightLine;
    /// <summary>The picked building or unit button (+0x358); -1 for none.</summary>
    WPARAM selectedButton = static_cast<WPARAM>(-1);
    /// <summary>The tool tips built from names (+0x35c).</summary>
    char tipText[0x100] = {};
    /// <summary>The page shown (+0x45c); -1 before the first.</summary>
    int32_t page = -1;
    /// <summary>The Buildings or Units category shown (+0x460); not constructed in the original (the port's -1).</summary>
    int32_t group = -1;
};
