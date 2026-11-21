#ifndef NORTH_ISLAND_CLIENT_MODELS_ITEMS_SORT_LOCK_FILE_H
#define NORTH_ISLAND_CLIENT_MODELS_ITEMS_SORT_LOCK_FILE_H

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

namespace client {

    /// @brief The places each grid's sort leaves alone (qol sort locks), kept per account in the user folder.
    /// @details The file is JSON: {"slot1": {"ItemBag": [3, 7], ...}, "slot2": {...}}, one object per character slot, one list
    /// of 1-based places per grid. A missing or malformed file reads as no locks.
    struct SortLockFile {
        /// @brief Builds the path of an account's sort lock file.
        /// @param userRoot The user folder.
        /// @param serverId The server's id (accounts on two servers may share a login id).
        /// @param loginId The account's login id.
        /// @return The path.
        static std::filesystem::path pathFor(const std::filesystem::path& userRoot, std::uint16_t serverId, std::uint32_t loginId);

        /// @brief Reads one grid's locked places.
        /// @param path The file.
        /// @param slot The character slot.
        /// @param grid The grid's name.
        /// @return The places; empty when none are saved.
        static std::set<int> read(const std::filesystem::path& path, int slot, const std::string& grid);

        /// @brief Writes one grid's locked places, keeping the rest of the file.
        /// @param path The file.
        /// @param slot The character slot.
        /// @param grid The grid's name.
        /// @param places The places.
        /// @return True when the file was written.
        static bool write(const std::filesystem::path& path, int slot, const std::string& grid, const std::set<int>& places);
    };

}

#endif
