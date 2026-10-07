#pragma once

#include "logistics/loggen.h"
#include "logistics/lport.h"

class aEvent;
class aFont;

/// <summary>
/// A button of a <see cref="ReusableDialog"/>: pressing it closes the dialog with <see cref="result"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4ec bytes. Its parent is always the dialog.</remarks>
class lDialogButton : public lButton
{
public:
    /// <remarks>MCX.EXE @ 0x006e0700 (vector deleting destructor)</remarks>
    ~lDialogButton() override = default;

    /// <remarks>MCX.EXE @ 0x006e0e20</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>
    /// Shows the gray, pressed or up picture, keyed (or a plain fill when it has none). Port: drawn each frame from
    /// the state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e0e50</remarks>
    void draw() override;

    /// <summary>
    /// On a click: shows the pressed state, runs the callback and deactivates the parent dialog. Port: a release ends
    /// the press shown.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e0f30</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// Set while the button is drawn pressed (lButton's own <c>pressed</c> at +0x4bc is unused here). Port: from the
    /// click to the release, or until the dialog opens again (the original set it for the one paint).
    /// </summary>
    int32_t pressedDown = 0; // +0x4e4
    /// <summary>The value the dialog's callback gets when this button closes it.</summary>
    int32_t result = 0; // +0x4e8
};

/// <summary>
/// The logistics message box: a picture, one or two buttons and optionally a spinner, drawn over a darkened copy
/// of the screen; closing it calls <see cref="callback"/> with the button pressed.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4d8 bytes.</remarks>
class LogDialogBox : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006f04b0 (vector deleting destructor)</remarks>
    ~LogDialogBox() override { destroy(); }

    /// <summary>
    /// Places the box (backed by the <c>lspcb00</c> frame) and hides it.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006df240 (unnamed in the symbols: the name is inferred; it is only called on a new box).</remarks>
    void init(int32_t xPos, int32_t yPos, int32_t width, int32_t height);

    /// <remarks>MCX.EXE @ 0x006df330</remarks>
    void destroy() override;

    /// <summary>
    /// The first time after <see cref="activate"/>, shows a frame with the cursor hidden (the original also made a
    /// darkened snapshot of the screen behind the box, which its fill then covered); then no arrow or button shows
    /// pressed any more. (The original painted the box here.)
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006df3d0</remarks>
    void drawBackground();

    /// <summary>
    /// Port: draws the box: its fill, the frame, the spinner arrows, the buttons and the picture, then the pressed
    /// arrow or button shown over them.
    /// </summary>
    void draw() override;

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
    /// <remarks>MCX.EXE @ 0x006df730</remarks>
    void setTwoButton(int twoButtons);

    /// <summary>Whether it shows the quantity spinner.</summary>
    /// <remarks>MCX.EXE @ 0x006df740</remarks>
    void setSpinner(int spinner);

    /// <summary>Grabs the input, draws and shows the box.</summary>
    /// <remarks>MCX.EXE @ 0x006df750</remarks>
    void activate();

    /// <summary>Releases the input, hides the box and calls the callback with <paramref name="result"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006df7c0</remarks>
    void deactivate(int result);

    /// <remarks>MCX.EXE @ 0x006e0400</remarks>
    void setCallback(void (*newCallback)(int));

    /// <summary>The port the box is drawn into.</summary>
    /// <remarks>MCX.EXE @ 0x006e0410</remarks>
    void setPort(lPort* port);

    /// <summary>Nonzero when there is a cancel button.</summary>
    int32_t twoButton = 0; // +0x4bc
    /// <summary>Nonzero when the quantity spinner is shown.</summary>
    int32_t spinner = 0; // +0x4c0
    /// <summary>Called by <see cref="deactivate"/> with the result.</summary>
    void (*callback)(int) = nullptr; // +0x4c8
    /// <summary>The picture shown in the box (a copy of the port given to <c>PurchaseDlg::init</c>).</summary>
    lPort* picturePort = nullptr; // +0x4cc
    /// <summary>The darkened snapshot of the screen behind the box.</summary>
    lPort* fadedBackground = nullptr; // +0x4d0
    /// <summary>Set by <see cref="activate"/>: take a new snapshot on the next <see cref="drawBackground"/>.</summary>
    int32_t needBackground = 0; // +0x4d4

    /// <summary>Port: the arrow or button held down (until the release, or the box shows afresh).</summary>
    PressedPart pressedPart = PressedPart::None;

protected:
    /// <summary>Port: draws the fill, the frame, the spinner arrows, the buttons and the picture.</summary>
    void drawBox();
    /// <summary>Port: draws the pressed arrow or button, if any.</summary>
    void drawPressed();
};

/// <summary>
/// The buy/sell dialog: shows the unit cost, a quantity spinner and the resource points left, and calls
/// <see cref="purchaseCallback"/> with the button and the quantity.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4f4 bytes.</remarks>
class PurchaseDlg : public LogDialogBox
{
public:
    /// <remarks>MCX.EXE @ 0x006f0460 (vector deleting destructor)</remarks>
    ~PurchaseDlg() override { destroy(); }

    /// <summary>
    /// Sets up a purchase of <paramref name="purchaseType"/> (0/1 buy/sell mech, 2/3 pilot, 4/5 component, 6/7
    /// vehicle) at <paramref name="unitCost"/> each, at most <paramref name="maxQuantity"/> (negative = 199; 1 =
    /// no spinner), with two text lines and a copy of <paramref name="picture"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006df800</remarks>
    void init(int32_t purchaseType, int32_t unitCost, int32_t maxQuantity, char* title, char* subtitle, lPort* picture);

    /// <remarks>MCX.EXE @ 0x006df9a0</remarks>
    void destroy() override;

    /// <summary>The buttons, the spinner (with auto-repeat on timer 6) and Enter/Escape.</summary>
    /// <remarks>MCX.EXE @ 0x006df9f0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// The base box's. (The original painted the box with the quantity and resource points as they were then, and
    /// copied it into the screen's port as well; nothing showed that copy, since the screen's panes cover it.)
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006dfef0</remarks>
    void drawBackground();

    /// <summary>Port: draws the base box plus the texts, the costs, the quantity and the kind's picture.</summary>
    void draw() override;

    /// <remarks>MCX.EXE @ 0x006e0330</remarks>
    void activate();

    /// <summary>Stops the spinner timer, hides the box and calls the callback with the result and the quantity.</summary>
    /// <remarks>MCX.EXE @ 0x006e03a0</remarks>
    void deactivate(int result);

    /// <summary>The callback that also gets the quantity.</summary>
    /// <remarks>MCX.EXE @ 0x006e03f0 (unnamed in the symbols: the name is inferred from its callers, which pass it a <c>CompSellCallback</c>/<c>CompPurchaseCallback</c>).</remarks>
    void setCallback(void (*newCallback)(int, int32_t));

    /// <summary>The spinner's maximum.</summary>
    int32_t maxQuantity = 0; // +0x4d8
    /// <summary>What is bought or sold: 0/1 mech, 2/3 pilot, 4/5 component, 6/7 vehicle (odd = selling).</summary>
    int32_t purchaseType = 0; // +0x4dc
    /// <summary>The quantity chosen (starts at 1).</summary>
    int32_t quantity = 0; // +0x4e0
    /// <summary>The first text line (owned copy).</summary>
    char* title = nullptr; // +0x4e4
    /// <summary>The second text line (owned copy; red for a pilot sale).</summary>
    char* subtitle = nullptr; // +0x4e8
    /// <summary>Called by <see cref="deactivate"/> with the result and <see cref="quantity"/>.</summary>
    void (*purchaseCallback)(int, int32_t) = nullptr; // +0x4ec
    /// <summary>The cost of one (resource points).</summary>
    int32_t unitCost = 0; // +0x4f0
};

/// <summary>
/// The general logistics message dialog: word-wrapped text in a frame that grows with it, OK and optionally Cancel
/// buttons, and an optional timeout.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4ec bytes. The logistics screen keeps one and reuses it.</remarks>
class ReusableDialog : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006f0420 (vector deleting destructor)</remarks>
    ~ReusableDialog() override { destroy(); }

    /// <summary>Loads the frame pieces (<c>dbox_top/middle/bottom</c>), centres the dialog and makes the two buttons.</summary>
    /// <remarks>MCX.EXE @ 0x006e0420</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <remarks>MCX.EXE @ 0x006e0790</remarks>
    void destroy() override;

    /// <summary>Draws the frame and the text wrapped to the width.</summary>
    /// <remarks>MCX.EXE @ 0x006e0880</remarks>
    void draw() override;

    /// <summary>Port: the dialog draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Enter presses OK (or Cancel on a one-button dialog), Escape cancels, the timeout closes it.</summary>
    /// <remarks>MCX.EXE @ 0x006e0a70</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Grabs the input, centres and shows the dialog, and starts the timeout timer if any.</summary>
    /// <remarks>MCX.EXE @ 0x006e0b20</remarks>
    void activate();

    /// <summary>
    /// Hides the dialog and calls the callback with <paramref name="result"/>; unless <see cref="keepCallbacks"/>
    /// is set, then clears the callbacks and the timeout.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e0ba0</remarks>
    void deactivate(int32_t result);

    /// <summary>Sets the text and resizes the dialog to fit it.</summary>
    /// <remarks>MCX.EXE @ 0x006e0c50</remarks>
    void setText(char* newText);

    /// <summary>Shows or hides the Cancel button and places the buttons.</summary>
    /// <remarks>MCX.EXE @ 0x006e0d90</remarks>
    void setTwoButton(int twoButtons);

    /// <summary>Nonzero when the Cancel button shows.</summary>
    int32_t twoButton = 0; // +0x4bc
    /// <summary>How many middle frame pieces the text needs.</summary>
    int32_t numMiddlePieces = 0; // +0x4c0
    /// <summary>The text (a logistics block).</summary>
    char* text = nullptr; // +0x4c4
    /// <summary>Called by <see cref="deactivate"/> with the result.</summary>
    void (*callback)(int32_t) = nullptr; // +0x4c8
    /// <summary>Milliseconds before the dialog closes by itself; 0 = never.</summary>
    int32_t timeout = 0; // +0x4cc
    /// <summary>The result passed when the timeout closes the dialog.</summary>
    int32_t timeoutResult = 0; // +0x4d0
    /// <summary>When set, the next <see cref="deactivate"/> keeps the callbacks (and clears this flag).</summary>
    int32_t keepCallbacks = 0;             // +0x4d4
    lDialogButton* okButton = nullptr;     // +0x4d8
    lDialogButton* cancelButton = nullptr; // +0x4dc
    lPort* topPiece = nullptr;             // +0x4e0
    lPort* middlePiece = nullptr;          // +0x4e4
    lPort* bottomPiece = nullptr;          // +0x4e8
};

/// <summary>
/// The refit confirmation dialog: a <see cref="ReusableDialog"/> whose text is a comma-separated list, drawn one
/// item per line between two fixed strings.
/// </summary>
/// <remarks>Original source: <c>logistics\logdlg.cpp</c>, 0x4f4 bytes.</remarks>
class RefitDialog : public ReusableDialog
{
public:
    /// <remarks>MCX.EXE @ 0x006f0560 (vector deleting destructor)</remarks>
    ~RefitDialog() override = default;

    /// <summary>Sets the list and resizes the dialog for its lines.</summary>
    /// <remarks>MCX.EXE @ 0x006e0fd0</remarks>
    void setText(char* newText);

    /// <summary>Draws the frame, the header (string 0x54), the items and the footer (string 0x62).</summary>
    /// <remarks>MCX.EXE @ 0x006e1100</remarks>
    void draw() override;

    /// <summary>Writes <paramref name="string"/> word-wrapped from line <paramref name="yPos"/>.</summary>
    /// <returns>The y of the last line written.</returns>
    /// <remarks>MCX.EXE @ 0x006e12f0</remarks>
    int32_t wrapText(char* string, int32_t yPos);

    /// <remarks>MCX.EXE @ 0x006e13d0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>The number of items in the list.</summary>
    int32_t numItems = 0; // +0x4ec
    /// <summary>
    /// Set once the dialog has been drawn (the original drew it only once per <see cref="setText"/>). Port: not read;
    /// the dialog is drawn each frame.
    /// </summary>
    int32_t drawn = 0; // +0x4f0
};

/// <summary>The darkening table for the screen behind a <see cref="LogDialogBox"/>.</summary>
extern char* g_logistic_dlgfade;

/// <summary>The medium blue font (dialog text).</summary>
extern aFont* medBlueFont;
