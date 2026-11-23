#ifndef NORTH_ISLAND_CLIENT_MODELS_REMOTE_AUTO_DELIVER_SETTINGS_H
#define NORTH_ISLAND_CLIENT_MODELS_REMOTE_AUTO_DELIVER_SETTINGS_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace client {

    /// @brief What the Electron Control's Auto Deliver is set to (not in the original game).
    /// @details Kept apart from the Remote Control's .asd file, whose format is the game's: JSON per server and account,
    /// {"slot1": {"on": true, "to": "Name", "pet": 0, "items": [1, 2]}, ...}, one object per character slot.
    struct AutoDeliverSettings {
        static constexpr int kItems = 10; // kinds of item that can be chosen

        bool on = false;
        std::string recipient; // a name, looked up before each trip: a login id would follow the account to another character
        int petKind = 0; // Npc.dat kind of the pet that goes; 0 for any free one
        std::array<int, kItems> items{}; // item ids, 0 for an empty cell

        /// @brief Tells whether an item is chosen.
        /// @param itemId The item.
        /// @return True when it is in the list.
        bool lists(int itemId) const;

        /// @brief Tells whether any item is chosen.
        /// @return True when the list is not empty.
        bool hasItems() const;

        /// @brief Compares every setting.
        /// @param other The settings to compare with.
        /// @return True when all are the same.
        bool operator==(const AutoDeliverSettings& other) const {
            return on == other.on && recipient == other.recipient && petKind == other.petKind && items == other.items;
        }

        /// @brief Builds the path of an account's file.
        /// @param userRoot The user folder.
        /// @param serverId The server's id (accounts and names repeat from server to server).
        /// @param loginId The account's login id.
        /// @return The path.
        static std::filesystem::path pathFor(const std::filesystem::path& userRoot, std::uint16_t serverId, std::uint32_t loginId);

        /// @brief Reads a character's settings.
        /// @param path The file.
        /// @param slot The character slot.
        /// @return The settings; off and empty for a missing or malformed file.
        static AutoDeliverSettings read(const std::filesystem::path& path, int slot);

        /// @brief Writes a character's settings, keeping the other slot's.
        /// @param path The file.
        /// @param slot The character slot.
        /// @return True when the file was written.
        bool write(const std::filesystem::path& path, int slot) const;
    };

}

#endif
