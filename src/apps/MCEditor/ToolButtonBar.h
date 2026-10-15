#pragma once

#include "PaletteBar.h"

// The main frame's two button bars (0x0042d3a0-0x0042d7e0, virtual table 0x004fd588): the main tool bar (the tools,
// bitmap 220) and the standard one (gridlines, undo, redo, set height; bitmap 218). Its source file name wasn't kept;
// the names are the port's.

/// <summary>A bar with one button per command of a range, each showing its image from one bitmap.</summary>
/// <remarks>0x1e8 bytes.</remarks>
class ToolButtonBar : public PaletteBar
{
public:
    /// <summary>
    /// A bar for the commands <paramref name="firstCommand"/> to <paramref name="lastCommand"/>, whose images are
    /// bitmap <paramref name="bitmapId"/>'s, one per command.
    /// </summary>
    ToolButtonBar(UINT bitmapId, UINT firstCommand, UINT lastCommand);

protected:
    /// <summary>
    /// Handles ON_WM_CREATE(): loads the bitmap into the image list and, in the editor palette's highlight colours,
    /// into the disabled image list, and adds a button for each command.
    /// </summary>
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>The bitmap of the button images (+0x1c4).</summary>
    UINT bitmapId;
    /// <summary>The first command (+0x1c8).</summary>
    UINT firstCommand;
    /// <summary>The last command (+0x1cc).</summary>
    UINT lastCommand;
    /// <summary>Constructed and never used (+0x1d0).</summary>
    CBitmap bitmap;
    /// <summary>The buttons' images (+0x1d8).</summary>
    CImageList images;
    /// <summary>The disabled buttons' images (+0x1e0).</summary>
    CImageList disabledImages;
};
