#ifndef NORTH_ISLAND_CLIENT_SUBSYSTEMS_MOVIE_SFW_ART_H
#define NORTH_ISLAND_CLIENT_SUBSYSTEMS_MOVIE_SFW_ART_H

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace client {

    /// @brief The pictures the SFW setting keeps off the screen
    inline constexpr std::array<std::string_view, 13> kSfwBlockedPictures{
        "JRole_5_5",  // (scripts 11018, 11020)
        "JRole_5_6",  // (11019)
        "JRole_5_17", // (11085)
        "JRole_5_18", // (11086)
        "JRole_5_26", // (14012)
        "JRole_5_31", // (11165, 11167)
        "JRole_5_33", // (11157)
        "JRole_5_40", // (14074)
        "JRole_5_41", // (14075)
        "JRole_5_42", // (14077)
        "JRole_5_43", // (14078)
        "JRole_5_49", // (25137)
        "JRole_5_50", // (25134)
    };

    /// @brief The movie scripts the SFW setting veils whatever pictures they use.
    inline constexpr std::array<std::string_view, 3> kSfwBlockedScripts{
        "136",   // hot spring, rocky pool: Hilltop Hot Spring 11089, Hotspring Cave 11181, Peak 12296, Korean Hotspring 12462,
                 // Hawaiian Hotsprings 12556, the French spring of Lost Jun 13264
        "159",   // hot spring, wooden bathhouse: Hotspring House 12139, Bathhouse 12263
        "14030", // hot spring, Roman pool: Public bathhouse 13207
    };

    /// @brief Tells whether a picture is on the NSFW list, by name without regard to case (the picture archives match names so).
    /// @param name Picture name as a movie script gives it.
    /// @return True when listed.
    inline bool sfwBlocked(std::string_view name) {
        return std::any_of(kSfwBlockedPictures.begin(), kSfwBlockedPictures.end(), [name](std::string_view listed) {
            return listed.size() == name.size() && std::equal(listed.begin(), listed.end(), name.begin(), [](char a, char b) {
                       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                   });
        });
    }

    /// @brief Tells whether a movie script is on the SFW list of scripts.
    /// @param script Script name, e.g. "136".
    /// @return True when listed.
    inline bool sfwBlockedScript(std::string_view script) {
        return std::find(kSfwBlockedScripts.begin(), kSfwBlockedScripts.end(), script) != kSfwBlockedScripts.end();
    }

}

#endif
