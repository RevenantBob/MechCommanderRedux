#pragma once

class MCBigGameObject;
class MCMover;

/// <summary>The object type marines are made from (0x28d): the ones a destroyed building lets out, and a dead vehicle's crew.</summary>
extern int32_t DefaultPilotId;
/// <summary>The pilot profile file (in the profile path) of those marines.</summary>
inline constexpr std::string_view MarineProfileName = "PEM00001";
/// <summary>How many marines have been made this mission; each takes part id 0xfff minus this count.</summary>
extern int32_t NumMarines;

/// <summary>
/// A new marine (object type <see cref="DefaultPilotId"/>, awake) with the marine profile loaded; Fatal with
/// <paramref name="createFailure"/> when it can't be made, and with the profile's errors when that can't be read.
/// </summary>
std::unique_ptr<MCMover> MakeMarine(const char* createFailure);

/// <summary>
/// Lets <paramref name="marinesWanted"/> marines out of the destroyed <paramref name="building"/>, one for each enemy
/// warrior (from the second on, as the original counted) without a working vehicle: each is piloted by its warrior,
/// placed by <paramref name="place"/> (which also puts it on the object map), sent to the building's side's mech list,
/// and ordered to stand.
/// </summary>
void LetOutBuildingMarines(MCBigGameObject& building, int32_t marinesWanted,
                           const std::function<void(MCMover&)>& place);
