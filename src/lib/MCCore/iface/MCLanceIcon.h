#pragma once

#include "gui/MCGuiOwned.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCGuiPort.h"

class MCMoverGroup;

/// <summary>A lance's badge on the mech bar: its number and one link per active mover, in the lance colour.</summary>
/// <remarks>Original source: <c>iface\iface.cpp</c> (<c>LanceIcon</c>).</remarks>
class MCLanceIcon : public MCGuiObject
{
public:
    ~MCLanceIcon() override { Destroy(); }

    /// <summary>Makes the badge for lance <paramref name="lanceNumber"/> and loads its five images.</summary>
    int32_t Init(int16_t lanceNumber);
    /// <summary>Frees the five images.</summary>
    void Destroy() override;
    /// <summary>Nothing: everything is drawn in <see cref="Display"/>.</summary>
    void Draw() override {}
    /// <summary>The number, then one link per active mover, then a bar in the lance colour.</summary>
    void Display() override;
    /// <summary>A click selects the lance (shift toggles it) or gives it the current mode's order.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Shows a floating tag over each active mover of the lance.</summary>
    void Enter() override;
    /// <summary>Hides the floating tags.</summary>
    void Leave() override;

    /// <summary>Shown only while the lance has an active mover.</summary>
    void ShowTest();
    /// <summary>The mech bar icons in this lance that are active.</summary>
    int32_t GetNumActiveMovers() const;

    /// <summary>The lance's group (<c>HomeCommander</c>'s group of the lance number).</summary>
    MCMoverGroup* Group = nullptr;
    /// <summary>The group's id, which is the lance number compared with <see cref="MCFriendlyMechIcon::Lance"/>.</summary>
    int32_t LanceId = -1;
    /// <summary>The active movers counted when the mech bar last shuffled.</summary>
    int32_t NumActiveMovers = 0;
    /// <summary>The lance number (<c>guiubf%i.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> NumberImage;
    /// <summary>The first link (<c>guiub01.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> FirstLinkImage;
    /// <summary>The short link (<c>guiub02.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> ShortLinkImage;
    /// <summary>The long link, once per further mover (<c>guiub03.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> LongLinkImage;
    /// <summary>The end link (<c>guiub04.tga</c>).</summary>
    MCGuiOwned<MCGuiPort> LastLinkImage;
    /// <summary>
    /// Set when <see cref="MCTacticalInterface::SetUnit"/> linked the lance, cleared when its point died
    /// (<see cref="MCTacticalInterface::RemoveMech"/>). <see cref="MCTacticalInterface::SelectLance"/> selects a linked
    /// lance as a whole and an unlinked one mover by mover.
    /// </summary>
    bool Linked = false;
};
