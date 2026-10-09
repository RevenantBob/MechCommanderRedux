#pragma once

class MCFitIniFile;
class MCInventoryList;

/// <summary>The object description file under <c>ObjectPath</c>; its <c>Desc&lt;n&gt;</c> blocks hold the texts.</summary>
inline constexpr std::string_view ObjectDesc = "desc.fit";

/// <summary>
/// Description <paramref name="descIndex"/> of the object description file: "%fc4" (a colour code) and the text.
/// Empty when the file has no such block.
/// </summary>
std::string LoadDescriptionText(int32_t descIndex);

/// <summary>
/// Opens the profile <paramref name="name"/>: <paramref name="dir"/> name.fit, then the profile folder's name.fit, then
/// the save-temp folder's name.fit and, when <paramref name="bareName"/>, its bare name. Not finding it is fatal
/// (<paramref name="error"/>).
/// </summary>
void OpenProfile(MCFitIniFile& file, std::string_view dir, std::string_view name, bool bareName,
                 std::string_view error);

/// <summary>
/// Reads <c>st <paramref name="name"/></c> of a profile. A missing value (empty), or one of
/// <paramref name="maxLength"/> characters or more (the original's buffer: cut to that), is reported
/// (<paramref name="error"/>).
/// </summary>
std::string ReadProfileString(MCFitIniFile& file, std::string_view name, size_t maxLength, std::string_view error);

/// <summary>
/// Reads the Item:n blocks of a profile's inventory (<paramref name="numOther"/> others, <paramref name="numWeapons"/>
/// weapons, <paramref name="numAmmo"/> ammo) into <paramref name="inventory"/>.
/// </summary>
/// <returns>The sum of the items' resource points.</returns>
int32_t ReadInventory(MCFitIniFile& file, MCInventoryList& inventory, uint8_t numOther, uint8_t numWeapons,
                      uint8_t numAmmo);
