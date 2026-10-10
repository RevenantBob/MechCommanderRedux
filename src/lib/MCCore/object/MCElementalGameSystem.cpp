#include "stdafx.h"
#include "object/MCElementalGameSystem.h"
#include "lib/MCFatal.h"
#include "object/MCGameSystemReader.h"

float ElmDamageOnImpact = 0.0f;
float ElementalTargetNoJumpDistance = 75.0f;
int UseOldProject = 0;

auto LoadElementalGameSystem(MCFitIniFile& sysFile) -> int32_t
{
    MCGameSystemReader read(sysFile);
    read.Block("Elemental:Collision");
    read.Value("DamageOnImpact", ElmDamageOnImpact);
    read.Block("Elemental:Combat");

    if (read.Error() != 0)
    {
        return read.Error();
    }

    const int32_t result = MCGameSystemReader::ReadValue(sysFile, "NoJumpRange", ElementalTargetNoJumpDistance);
    Assert(result == 0, 0, " Unable to find Elemental NoJumpRange in gamesys.fit ");
    return 0;
}
