#include "stdafx.h"
#include "object/MCTreeType.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "object/MCMoverMath.h"
#include "object/MCTree.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxAppearanceType.h"

int32_t LoadShadowShape(MCFitIniFile& typeFile, std::string_view entry, MCRegisteredBlock& shadow)
{
    const MCFitResult<std::string> shadowName = typeFile.Read<std::string>(entry);

    if (!shadowName.has_value())
    {
        return 0;
    }

    MCFile shadowFile;

    if (const int32_t result = shadowFile.Open(GamePath(SpritePath, *shadowName, ".shp")); result != 0)
    {
        return result;
    }

    shadow = MCRegisteredBlock(shadowFile.FileSize(), MCDataKind::Shapes);
    shadowFile.Read(shadow.Bytes());
    shadowFile.Close();
    return 0;
}

auto MCTreeType::CreateInstance() -> std::unique_ptr<MCBaseObject>
{
    auto newTree = std::make_unique<MCTree>();

    if (newTree->Init(this) != 0)
    {
        return nullptr;
    }

    newTree->IdNumber = NextIdNumber++;
    return newTree;
}

auto MCTreeType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile treeFile;

    if (const int32_t result = treeFile.Open(objFile, fileSize); result != 0)
    {
        return result;
    }

    if (const int32_t result = treeFile.SeekBlock("TreeData"); result != 0)
    {
        return result;
    }

    MCFitReader read(treeFile);
    read.Value("DmgLevel", DmgLevel);

    if (read.Failed())
    {
        return std::to_underlying(read.Error());
    }

    ExplosionRadius = treeFile.Read<float>("ExplosionRadius").value_or(0.0f);
    ExplosionDamage = treeFile.Read<float>("ExplosionDamage").value_or(0.0f);

    if (const int32_t result = LoadShadowShape(treeFile, "NormalShadow", NormalShadow); result != 0)
    {
        return result;
    }

    if (const int32_t result = LoadShadowShape(treeFile, "DestroyedShadow", DestroyedShadow); result != 0)
    {
        return result;
    }

    return MCObjectType::Init(&treeFile);
}

auto MCTreeType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // A mover (not artillery or fire) knocks a standing tree over, away from itself.
    if (MCObjectClass::Mover <= collider->ObjectClass || collider->ObjectClass == MCObjectClass::Artillery ||
        collider->ObjectClass == MCObjectClass::Fire)
    {
        return 1;
    }

    auto* tree = static_cast<MCTree*>(collidee);

    if (tree->Fallen || tree->Falling)
    {
        return 1;
    }

    tree->Falling = true;
    const MCVector3D colliderPos = collider->GetPosition();
    const auto facing = static_cast<float>(tree->RelFacingTo(colliderPos, -1));
    MCFrameOfRef frame = tree->GetFrame();
    const auto s = static_cast<float>(std::sin(facing * MCMoverMath::DegreesToRadians));
    const auto c = static_cast<float>(std::cos(facing * MCMoverMath::DegreesToRadians));
    MCMoverMath::RotateAboutK(frame, s, c);
    tree->SetFrame(frame);

    // Fall (state 1), or a burnt tree crumble (state 4); a fall with frames to show makes a sound.
    MCVfxAppearance* treeAppearance = tree->Appearance.get();
    tree->CollisionsOn = 0;
    uint32_t numFrames = 0;

    if (!tree->Burnt)
    {
        treeAppearance->SetTypeId(MCActorState::BlowingUp1, 0xff);

        if (1 < treeAppearance->AppearType->NumStates)
        {
            numFrames = treeAppearance->AppearType->States[1].NumFrames;
        }
    }
    else
    {
        treeAppearance->SetTypeId(MCActorState::Destroyed, NoSubState);

        if (4 < treeAppearance->AppearType->NumStates)
        {
            numFrames = treeAppearance->AppearType->States[4].NumFrames;
        }
    }

    if (UseSound != 0 && SoundSystem() != nullptr && 1 < static_cast<int32_t>(numFrames))
    {
        SoundSystem()->PlayDigitalSample(0xe, 1, tree, 0, 0);
    }

    return 1;
}
