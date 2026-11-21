#include "client/models/items/SortLockFile.h"

#include "core/Bytes.h"
#include "core/FileSystem.h"

#include <nlohmann/json.hpp>

namespace client {

    namespace {

        using Json = nlohmann::json;

        /// @brief Reads the whole file as a JSON object.
        /// @param path The file.
        /// @return The object; an empty one for a missing or malformed file.
        Json readAll(const std::filesystem::path& path) {
            if (const auto data = core::readFile(path)) {
                Json root = Json::parse(core::toString(*data), nullptr, false);
                if (root.is_object()) {
                    return root;
                }
            }
            return Json::object();
        }

        /// @brief The key of a character slot's object.
        /// @param slot The slot.
        /// @return "slot" and its number.
        std::string slotKey(int slot) { return "slot" + std::to_string(slot); }

    }

    std::filesystem::path SortLockFile::pathFor(const std::filesystem::path& userRoot, std::uint16_t serverId, std::uint32_t loginId) {
        return userRoot / ("sort-locks-" + std::to_string(serverId) + "-" + std::to_string(loginId) + ".json");
    }

    std::set<int> SortLockFile::read(const std::filesystem::path& path, int slot, const std::string& grid) {
        const Json root = readAll(path);
        std::set<int> places;
        const auto character = root.find(slotKey(slot));
        if (character == root.end() || !character->is_object()) {
            return places;
        }
        const auto list = character->find(grid);
        if (list == character->end() || !list->is_array()) {
            return places;
        }
        for (const Json& place : *list) {
            if (place.is_number_integer() && place.get<int>() >= 1) {
                places.insert(place.get<int>());
            }
        }
        return places;
    }

    bool SortLockFile::write(const std::filesystem::path& path, int slot, const std::string& grid, const std::set<int>& places) {
        Json root = readAll(path);
        Json& character = root[slotKey(slot)];
        if (!character.is_object()) {
            character = Json::object();
        }
        if (places.empty()) {
            character.erase(grid);
        } else {
            character[grid] = Json(std::vector<int>(places.begin(), places.end()));
        }
        if (character.empty()) {
            root.erase(slotKey(slot));
        }
        return core::writeFile(path, core::toBytes(root.dump(2)));
    }

}
