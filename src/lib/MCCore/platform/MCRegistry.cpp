#include "stdafx.h"
#include "platform/MCRegistry.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>The store's file in the user folder.</summary>
    constexpr std::string_view StoreFileName = "registry.cfg";

    /// <summary>A key and value name as one lower-case store key (<c>key|value</c>).</summary>
    std::string StoreKey(std::string_view keyName, std::string_view valueName)
    {
        std::string key;
        key.reserve(keyName.size() + valueName.size() + 1);

        for (char c : keyName)
        {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        key.push_back('|');

        for (char c : valueName)
        {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        return key;
    }

    /// <summary>Every stored value, keyed by <see cref="StoreKey"/>; each line of the file is <c>key=data</c>.</summary>
    std::map<std::string, std::string> LoadStore()
    {
        std::map<std::string, std::string> values;
        std::ifstream file(MCFileSystem::Resolve(StoreFileName));
        std::string line;

        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            const size_t equals = line.find('=');

            if (equals != std::string::npos)
            {
                values[line.substr(0, equals)] = line.substr(equals + 1);
            }
        }

        return values;
    }
}

namespace MCRegistry
{
    std::optional<std::string> Read(std::string_view keyName, std::string_view valueName)
    {
        const std::map<std::string, std::string> values = LoadStore();
        const auto found = values.find(StoreKey(keyName, valueName));

        if (found == values.end())
        {
            return std::nullopt;
        }

        return found->second;
    }

    bool Write(std::string_view keyName, std::string_view valueName, std::string_view data)
    {
        std::map<std::string, std::string> values = LoadStore();
        values[StoreKey(keyName, valueName)] = std::string(data);
        std::ofstream file(MCFileSystem::ResolveWrite(StoreFileName), std::ios::trunc);

        if (!file)
        {
            return false;
        }

        for (const auto& [key, value] : values)
        {
            file << key << '=' << value << '\n';
        }

        return static_cast<bool>(file);
    }
}
