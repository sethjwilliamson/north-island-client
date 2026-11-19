#ifndef NORTH_ISLAND_CLIENT_SERVICES_INPUT_SCRIPT_WAIT_H
#define NORTH_ISLAND_CLIENT_SERVICES_INPUT_SCRIPT_WAIT_H

#include <cstdint>
#include <string>

namespace client {

    /// @brief A script command that holds the script until something is so (walkto, waitwin, waitmsg, waitmap, login, waitworld, waitdialog,
    /// waitevent, skiptalk) or its time is up.
    struct ScriptWait {
        /// @brief What is waited for: the end of a walk, a window, a message line, a map, a login into the world, the world (with the party),
        /// an answerable talk box, the end of the player's event, or (skiptalk) a question or the end of the event while words are clicked away.
        enum class Kind { Walk, Window, Message, Map, Login, World, Dialog, Event, SkipTalk };

        Kind kind = Kind::Walk;
        std::string what;
        std::string text;
        std::string password; // login: the account's password
        int number = 0;
        bool started = false; // login: the login was handed to the login scene
        std::uint64_t mark = 0;
        std::uint64_t startMs = 0;
        std::uint64_t deadlineMs = 0;
    };

}

#endif
