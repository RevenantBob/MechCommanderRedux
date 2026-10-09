#pragma once

#include "logistics/loggen.h"
#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiFont;

/// <summary>
/// A button of a <see cref="MCReusableDialog"/>: pressing it closes the dialog with <see cref="Result"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4ec bytes. Its parent is always the dialog.</remarks>
class MCLogDialogButton : public MCLogButton
{
public:
    ~MCLogDialogButton() override = default;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>
    /// Shows the gray, pressed or up picture, keyed (or a plain fill when it has none). Port: drawn each frame from
    /// the state.
    /// </summary>
    void Draw() override;

    /// <summary>
    /// On a click: shows the pressed state, runs the callback and deactivates the parent dialog. Port: a release ends
    /// the press shown.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// Set while the button is drawn pressed (lButton's own <c>pressed</c> at +0x4bc is unused here). Port: from the
    /// click to the release, or until the dialog opens again (the original set it for the one paint).
    /// </summary>
    int32_t PressedDown = 0;
    /// <summary>The value the dialog's callback gets when this button closes it.</summary>
    int32_t Result = 0;
};

/// <summary>
/// The logistics message box: a picture, one or two buttons and optionally a spinner, drawn over a darkened copy
/// of the screen; closing it calls <see cref="Callback"/> with the button pressed.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4d8 bytes.</remarks>
class MCLogDialogBox : public MCLogObject
{
public:
    ~MCLogDialogBox() override { Destroy(); }

    /// <summary>
    /// Places the box (backed by the <c>lspcb00</c> frame) and hides it.
    /// </summary>
    void Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height);

    void Destroy() override;

    /// <summary>
    /// The first time after <see cref="Activate"/>, shows a frame with the cursor hidden (the original also made a
    /// darkened snapshot of the screen behind the box, which its fill then covered); then no arrow or button shows
    /// pressed any more. (The original painted the box here.)
    /// </summary>
    void DrawBackground();

    /// <summary>
    /// Port: draws the box: its fill, the frame, the spinner arrows, the buttons and the picture, then the pressed
    /// arrow or button shown over them.
    /// </summary>
    void Draw() override;

    /// <summary>Port: the box draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Port: the part of the box shown pressed (the original copied its pressed art over the picture).</summary>
    enum class PressedPart : int32_t
    {
        None,
        Ok,
        Cancel,
        Up,
        Down,
    };

    /// <summary>Whether it shows a second (cancel) button.</summary>
    void SetTwoButton(int twoButtons);

    /// <summary>Whether it shows the quantity spinner.</summary>
    void SetSpinner(int spinner);

    /// <summary>Grabs the input, draws and shows the box.</summary>
    void Activate();

    /// <summary>Releases the input, hides the box and calls the callback with <paramref name="result"/>.</summary>
    void Deactivate(int result);

    void SetCallback(void (*newCallback)(int));

    /// <summary>The port the box is drawn into.</summary>
    void SetPort(MCLogPort* port);

    /// <summary>Nonzero when there is a cancel button.</summary>
    int32_t TwoButton = 0;
    /// <summary>Nonzero when the quantity spinner is shown.</summary>
    int32_t Spinner = 0;
    /// <summary>Called by <see cref="Deactivate"/> with the result.</summary>
    void (*Callback)(int) = nullptr;
    /// <summary>The picture shown in the box (a copy of the port given to <c>PurchaseDlg::init</c>).</summary>
    MCLogPort* PicturePort = nullptr;
    /// <summary>The darkened snapshot of the screen behind the box.</summary>
    MCLogPort* FadedBackground = nullptr;
    /// <summary>Set by <see cref="Activate"/>: take a new snapshot on the next <see cref="DrawBackground"/>.</summary>
    int32_t NeedBackground = 0;

    /// <summary>Port: the arrow or button held down (until the release, or the box shows afresh).</summary>
    PressedPart Pressed = PressedPart::None;

protected:
    /// <summary>Port: draws the fill, the frame, the spinner arrows, the buttons and the picture.</summary>
    void DrawBox();
    /// <summary>Port: draws the pressed arrow or button, if any.</summary>
    void DrawPressed();
};

/// <summary>
/// The buy/sell dialog: shows the unit cost, a quantity spinner and the resource points left, and calls
/// <see cref="PurchaseCallback"/> with the button and the quantity.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4f4 bytes.</remarks>
class MCPurchaseDlg : public MCLogDialogBox
{
public:
    ~MCPurchaseDlg() override { Destroy(); }

    /// <summary>
    /// Sets up a purchase of <paramref name="purchaseType"/> (0/1 buy/sell mech, 2/3 pilot, 4/5 component, 6/7
    /// vehicle) at <paramref name="unitCost"/> each, at most <paramref name="maxQuantity"/> (negative = 199; 1 =
    /// no spinner), with two text lines and a copy of <paramref name="picture"/>.
    /// </summary>
    void Init(int32_t purchaseType, int32_t unitCost, int32_t maxQuantity, const char* title, const char* subtitle,
              MCLogPort* picture);

    void Destroy() override;

    /// <summary>The buttons, the spinner (with auto-repeat on timer 6) and Enter/Escape.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// The base box's. (The original painted the box with the quantity and resource points as they were then, and
    /// copied it into the screen's port as well; nothing showed that copy, since the screen's panes cover it.)
    /// </summary>
    void DrawBackground();

    /// <summary>Port: draws the base box plus the texts, the costs, the quantity and the kind's picture.</summary>
    void Draw() override;

    void Activate();

    /// <summary>Stops the spinner timer, hides the box and calls the callback with the result and the quantity.</summary>
    void Deactivate(int result);

    /// <summary>The callback that also gets the quantity.</summary>
    void SetCallback(void (*newCallback)(int, int32_t));

    /// <summary>The spinner's maximum.</summary>
    int32_t MaxQuantity = 0;
    /// <summary>What is bought or sold: 0/1 mech, 2/3 pilot, 4/5 component, 6/7 vehicle (odd = selling).</summary>
    int32_t PurchaseType = 0;
    /// <summary>The quantity chosen (starts at 1).</summary>
    int32_t Quantity = 0;
    /// <summary>The first text line (owned copy).</summary>
    char* Title = nullptr;
    /// <summary>The second text line (owned copy; red for a pilot sale).</summary>
    char* Subtitle = nullptr;
    /// <summary>Called by <see cref="Deactivate"/> with the result and <see cref="Quantity"/>.</summary>
    void (*PurchaseCallback)(int, int32_t) = nullptr;
    /// <summary>The cost of one (resource points).</summary>
    int32_t UnitCost = 0;
};

/// <summary>
/// The general logistics message dialog: word-wrapped text in a frame that grows with it, OK and optionally Cancel
/// buttons, and an optional timeout.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4ec bytes. The logistics screen keeps one and reuses it.</remarks>
class MCReusableDialog : public MCLogObject
{
public:
    ~MCReusableDialog() override { Destroy(); }

    /// <summary>Loads the frame pieces (<c>dbox_top/middle/bottom</c>), centres the dialog and makes the two buttons.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    void Destroy() override;

    /// <summary>Draws the frame and the text wrapped to the width.</summary>
    void Draw() override;

    /// <summary>Port: the dialog draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Enter presses OK (or Cancel on a one-button dialog), Escape cancels, the timeout closes it.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Grabs the input, centres and shows the dialog, and starts the timeout timer if any.</summary>
    void Activate();

    /// <summary>
    /// Hides the dialog and calls the callback with <paramref name="result"/>; unless <see cref="KeepCallbacks"/>
    /// is set, then clears the callbacks and the timeout.
    /// </summary>
    void Deactivate(int32_t result);

    /// <summary>Sets the text and resizes the dialog to fit it.</summary>
    void SetText(char* newText);

    /// <summary>Shows or hides the Cancel button and places the buttons.</summary>
    void SetTwoButton(int twoButtons);

    /// <summary>Nonzero when the Cancel button shows.</summary>
    int32_t TwoButton = 0;
    /// <summary>How many middle frame pieces the text needs.</summary>
    int32_t NumMiddlePieces = 0;
    /// <summary>The text (a logistics block).</summary>
    char* Text = nullptr;
    /// <summary>Called by <see cref="Deactivate"/> with the result.</summary>
    void (*Callback)(int32_t) = nullptr;
    /// <summary>Milliseconds before the dialog closes by itself; 0 = never.</summary>
    int32_t Timeout = 0;
    /// <summary>The result passed when the timeout closes the dialog.</summary>
    int32_t TimeoutResult = 0;
    /// <summary>When set, the next <see cref="Deactivate"/> keeps the callbacks (and clears this flag).</summary>
    int32_t KeepCallbacks = 0;
    MCLogDialogButton* OkButton = nullptr;
    MCLogDialogButton* CancelButton = nullptr;
    MCLogPort* TopPiece = nullptr;
    MCLogPort* MiddlePiece = nullptr;
    MCLogPort* BottomPiece = nullptr;
};

/// <summary>
/// The refit confirmation dialog: a <see cref="MCReusableDialog"/> whose text is a comma-separated list, drawn one
/// item per line between two fixed strings.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4f4 bytes.</remarks>
class MCRefitDialog : public MCReusableDialog
{
public:
    ~MCRefitDialog() override = default;

    /// <summary>Sets the list and resizes the dialog for its lines.</summary>
    void SetText(char* newText);

    /// <summary>Draws the frame, the header (string 0x54), the items and the footer (string 0x62).</summary>
    void Draw() override;

    /// <summary>Writes <paramref name="string"/> word-wrapped from line <paramref name="yPos"/>.</summary>
    /// <returns>The y of the last line written.</returns>
    int32_t WrapText(char* string, int32_t yPos);

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>The number of items in the list.</summary>
    int32_t NumItems = 0;
    /// <summary>
    /// Set once the dialog has been drawn (the original drew it only once per <see cref="SetText"/>). Port: not read;
    /// the dialog is drawn each frame.
    /// </summary>
    int32_t Drawn = 0;
};

/// <summary>The darkening table for the screen behind a <see cref="MCLogDialogBox"/>.</summary>
extern char* LogisticDlgfade;

/// <summary>The medium blue font (dialog text).</summary>
extern MCGuiFont* MedBlueFont;
