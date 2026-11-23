#include "client/models/remote/AutoDeliverSettings.h"

#include "core/Bytes.h"
#include "core/FileSystem.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>

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

        /// @brief Turns a name's bytes (the game's code page, not UTF-8) into text JSON holds, a code point a byte.
        /// @param name The name.
        /// @return The UTF-8 text; ASCII stays as it is.
        std::string jsonText(const std::string& name) {
            std::string text;
            for (const char c : name) {
                const auto byte = static_cast<unsigned char>(c);
                if (byte < 0x80) { // ASCII
                    text += c;
                } else { // U+0080..U+00FF in UTF-8's two bytes
                    text += static_cast<char>(0xC0 | (byte >> 6));
                    text += static_cast<char>(0x80 | (byte & 0x3F));
                }
            }
            return text;
        }

        /// @brief Turns text of the file back into a name's bytes.
        /// @param text The UTF-8 text.
        /// @return The name.
        std::string nameBytes(const std::string& text) {
            std::string name;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const auto first = static_cast<unsigned char>(text[i]);
                if ((first == 0xC2 || first == 0xC3) && i + 1 < text.size()) { // the lead bytes of U+0080..U+00FF
                    name += static_cast<char>(((first & 0x03) << 6) | (static_cast<unsigned char>(text[++i]) & 0x3F));
                } else {
                    name += static_cast<char>(first);
                }
            }
            return name;
        }

    }

    bool AutoDeliverSettings::lists(int itemId) const { return itemId != 0 && std::find(items.begin(), items.end(), itemId) != items.end(); }

    bool AutoDeliverSettings::hasItems() const {
        return std::any_of(items.begin(), items.end(), [](int itemId) { return itemId != 0; });
    }

    std::filesystem::path AutoDeliverSettings::pathFor(const std::filesystem::path& userRoot, std::uint16_t serverId, std::uint32_t loginId) {
        return userRoot / ("auto-deliver-" + std::to_string(serverId) + "-" + std::to_string(loginId) + ".json");
    }

    AutoDeliverSettings AutoDeliverSettings::read(const std::filesystem::path& path, int slot) {
        AutoDeliverSettings settings;
        const Json root = readAll(path);
        const auto character = root.find(slotKey(slot));
        if (character == root.end() || !character->is_object()) {
            return settings;
        }
        if (const auto on = character->find("on"); on != character->end() && on->is_boolean()) {
            settings.on = on->get<bool>();
        }
        if (const auto to = character->find("to"); to != character->end() && to->is_string()) {
            settings.recipient = nameBytes(to->get<std::string>());
        }
        if (const auto pet = character->find("pet"); pet != character->end() && pet->is_number_integer()) {
            settings.petKind = pet->get<int>();
        }
        if (const auto list = character->find("items"); list != character->end() && list->is_array()) {
            for (std::size_t cell = 0; cell < list->size() && cell < settings.items.size(); ++cell) {
                if ((*list)[cell].is_number_integer()) {
                    settings.items[cell] = (*list)[cell].get<int>();
                }
            }
        }
        return settings;
    }

    bool AutoDeliverSettings::write(const std::filesystem::path& path, int slot) const {
        Json root = readAll(path);
        Json character = Json::object();
        character["on"] = on;
        character["to"] = jsonText(recipient);
        character["pet"] = petKind;
        character["items"] = items;
        root[slotKey(slot)] = std::move(character);
        return core::writeFile(path, core::toBytes(root.dump(2)));
    }

}
