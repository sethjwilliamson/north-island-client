#ifndef NORTH_ISLAND_CLIENT_SERVICES_INPUT_SCRIPT_LOGIN_H
#define NORTH_ISLAND_CLIENT_SERVICES_INPUT_SCRIPT_LOGIN_H

#include <string>

namespace client {

    /// @brief What a test script (InputScript's login command) may ask of the login scene. Test-only.
    class ScriptLogin {
        public:
            virtual ~ScriptLogin() = default;

            /// @brief Tells whether the server list shows a server to pick.
            /// @return True when there is a server row.
            virtual bool hasServerRow() const = 0;

            /// @brief Logs in without the forms: connects to the first server of the list, sends the account and enters the world with the
            /// character of the slot once the character list comes.
            /// @param account Account name.
            /// @param password Account password.
            /// @param slot Character slot (1 or 2).
            /// @param reason Receives why it cannot start now.
            /// @return False, with the reason, when the login cannot start.
            virtual bool startLogin(const std::string& account, const std::string& password, int slot, std::string& reason) = 0;
    };

}

#endif
