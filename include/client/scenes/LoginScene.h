#ifndef NORTH_ISLAND_CLIENT_SCENES_LOGIN_SCENE_H
#define NORTH_ISLAND_CLIENT_SCENES_LOGIN_SCENE_H

#include "client/services/GameServices.h"
#include "client/viewmodels/login/CharacterSelectForm.h"
#include "client/viewmodels/login/CreateCharacterForm.h"
#include "client/viewmodels/login/DisconnectForm.h"
#include "client/viewmodels/login/LoginForm.h"
#include "client/viewmodels/login/PasswordForm.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "client/viewmodels/login/ReloginScreen.h"
#include "client/viewmodels/login/SignUpForm.h"
#include "client/viewmodels/system/PadKeyboard.h"
#include "client/viewmodels/system/SettingsWindow.h"
#include "client/scenes/Scene.h"
#include "client/services/ServerEvents.h"
#include "client/viewmodels/login/ServerSelectForm.h"
#include "ui/Screen.h"

#include <optional>
#include <random>

namespace client {

    /// @brief The login scene: server pick, connect, login, character pick and creation, until the own player enters the world.
    /// @details Original: LoginScene.xml (background and logo). Flow: pick a server (ServerSelectForm), connect and wait for the server to accept (server 1/9), log in from the LoginForm (client 63/4), pick a character (CharacterSelectForm, server 63/1 / client 63/2) or create one in an empty slot (CreateCharacterForm, server 1/3, client 9/1) and wait for the own player record (server 3).
    class LoginScene : public Scene, private ServerEvents, private ScriptLogin {
        public:
            /// @brief Builds the login screen and its forms; starts a relogin or shows the server list.
            /// @param services The shared game services.
            explicit LoginScene(
                GameServices& services
            );

            /// @brief Overrides Scene::update: pumps the connection and server messages, runs timeouts and the relogin delay.
            /// @param nowMs Current time in ms.
            void update(std::uint64_t nowMs) override;

            /// @brief Overrides Scene::draw: draws the login screen.
            /// @param canvas Target canvas.
            /// @param nowMs Current time in ms.
            void draw(gfx::Canvas& canvas, std::uint64_t nowMs) override;

            /// @brief Overrides Scene::screen: gives the login screen.
            /// @return The screen.
            ui::Screen& screen() override { return m_screen; }

            /// @brief Gives the pad's buttons to the keyboard on the screen while it is up (Scene::padAsk).
            /// @param ask What is asked.
            /// @return True when the keyboard took it.
            bool padAsk(PadAsk ask) override {
                if (m_padKeyboard.isOpen()) {
                    return m_padKeyboard.padAsk(ask);
                }
                if (ask == PadAsk::Menu) {
                    m_settings.toggle(); // the start button: the settings
                    return true;
                }
                return false;
            }

            /// @brief Whether the pad's directions step through the keyboard on the screen (Scene::wantsPadSteps).
            /// @return True while it is up.
            bool wantsPadSteps() const override { return m_padKeyboard.isOpen(); }

            /// @brief Takes whether a game pad is played with (Scene::setPadInUse): the keyboard on the screen comes up for it.
            /// @param on True in controller mode.
            void setPadInUse(bool on) override { m_padKeyboard.setPad(on); }

            /// @brief Overrides Scene::nextScene: the world (or quit) once the login is done.
            /// @return The next scene, or nothing while logging in.
            std::optional<SceneKind> nextScene() const override { return m_next; }

            /// @brief Overrides Scene::cutOff: the disconnect form is up (kicked, locked, the connection failed or lost).
            /// @return True while the form is shown.
            bool cutOff() const override { return m_disconnect.isOpen(); }

            /// @brief Overrides Scene::scriptLogin: the login scene takes a test script's login command.
            /// @return This scene's ScriptLogin.
            std::optional<std::reference_wrapper<ScriptLogin>> scriptLogin() override { return std::ref(static_cast<ScriptLogin&>(*this)); }

        private:
            /// @brief The step of the login flow the scene is in.
            enum class Stage { ChoosingServer, Relogin, Connecting, EnteringAccount, LoggingIn, ChoosingCharacter, CreatingCharacter, EnteringWorld };

            /// @brief An account a test script logs in with (login command), kept until the world is entered.
            struct ScriptedLogin {
                std::string account;
                std::string password;
                int slot = 1;
            };

            /// @brief Overrides ScriptLogin::hasServerRow: whether the server list shows a server.
            /// @return True when it does.
            bool hasServerRow() const override { return m_serverForm.hasServerRow(); }

            /// @brief Overrides ScriptLogin::startLogin: connects to the first server; the account and slot go on by themselves.
            /// @param account Account name.
            /// @param password Account password.
            /// @param slot Character slot.
            /// @param reason Receives why it cannot start now.
            /// @return False, with the reason, when the login cannot start.
            bool startLogin(const std::string& account, const std::string& password, int slot, std::string& reason) override;

            /// @brief Tells the services' errorShown hook (a test script ends there) about an error the scene shows.
            /// @param text The error text.
            void reportError(const std::string& text);

            /// @brief Connects to the chosen server and sends the hello action.
            /// @param server The server list entry.
            void connect(const formats::ServerEntry& server);

            /// @brief Starts a relogin: the forms stay hidden and the loading screen shows until the delay is over.
            /// @details Original: Se_SystemForm.FReloginState 1.
            void startRelogin();

            /// @brief Continues a relogin once the delay is over: the connection, the login with the kept account and the character picked.
            /// @details Original: IsServerReloginDue then ConnectAndLogin (a PvP address) or AutoReconnectLogin.
            void continueRelogin();

            /// @brief Gives up a failed relogin and leaves for the server list.
            void abandonRelogin();

            /// @brief Ends the relogin state and hides its loading screen.
            void endRelogin();

            /// @brief Sends the login request for an account.
            /// @param account Account name.
            /// @param password Account password.
            void login(const std::string& account, const std::string& password);

            /// @brief Closes the connection, forgets the session and shows the server list.
            void backToServerList();

            /// @brief Shows the login form and hides the other forms.
            void showLoginForm();

            /// @brief Shows the qol client's sign-up page instead of the login form.
            void showSignUp();

            /// @brief Sends the character selection for a slot and waits for the world.
            /// @param slot Character slot.
            void enterWorld(int slot);

            /// @brief Asks the server to create a character in an empty slot.
            /// @param slot Character slot.
            void createCharacter(int slot);

            /// @brief Asks for the password and then sends the delete request for a slot.
            /// @param slot Character slot.
            void deleteCharacter(int slot);

            /// @brief Cancels the character creation and returns to the character list.
            void cancelCreating();

            /// @brief Sends the new character and waits for the server's answer.
            /// @param request The character creation request.
            void sendNewCharacter(const protocol::CreateCharacterRequest& request);

            /// @brief Leaves the character list and returns to the login form.
            void backToLoginForm();

            /// @brief Handles the connection events (connected, failed, reset, disconnected).
            void handleConnectionEvents();

            /// @brief Overrides ServerEvents::onKicked: shows the disconnection form of the reason.
            /// @param reason Kick reason code.
            void onKicked(int reason) override;

            /// @brief Overrides ServerEvents::onServerSelected: remembers the server and goes on to the login.
            /// @param message The server-accepted message (server 1/9).
            void onServerSelected(const protocol::ServerSelected& message) override;

            /// @brief Overrides ServerEvents::onLoginRejected: shows why the login was refused.
            /// @param reason The rejection reason.
            void onLoginRejected(LoginRejection reason) override;

            /// @brief Overrides ServerEvents::onLocked: shows that the account or character is locked.
            /// @param wholeAccount True when the whole account is locked, false for the character.
            /// @param seconds Remaining lock time in seconds.
            void onLocked(bool wholeAccount, std::uint32_t seconds) override;

            /// @brief Overrides ServerEvents::onCreateCharacter: opens the character creation form.
            /// @param askCipher True when the form asks for a cipher.
            void onCreateCharacter(bool askCipher) override;

            /// @brief Overrides ServerEvents::onNameChecked: passes the name check result to the creation form.
            /// @param result The name check result.
            void onNameChecked(NameCheckResult result) override;

            /// @brief Overrides ServerEvents::onCharacterList: stores the characters and shows the character list.
            /// @param list The character list (server 63/1).
            void onCharacterList(const protocol::CharacterList& list) override;

            /// @brief Overrides ServerEvents::onLoginId: stores the login id.
            /// @param loginId The login id.
            void onLoginId(std::uint32_t loginId) override;

            /// @brief Overrides ServerEvents::onOwnPlayerEntered: stores the own player record and switches to the world.
            /// @param record The own player record (server 3).
            void onOwnPlayerEntered(const protocol::OwnPlayerRecord& record) override;

            /// @brief Overrides ServerEvents::onCharacterDeleted: shows the result of a character deletion.
            /// @param result The result code.
            /// @param slot The character slot.
            void onCharacterDeleted(int result, int slot) override;

            /// @brief Overrides ServerEvents::onUnhandled: logs a server message nobody handles.
            /// @param id The message id.
            void onUnhandled(protocol::MessageId id) override;

            GameServices& m_services;
            ui::Screen m_screen;
            ui::Element& m_background;
            ServerSelectForm m_serverForm;
            LoginForm m_loginForm;
            SignUpForm m_signUpForm;
            CharacterSelectForm m_characterForm;
            CreateCharacterForm m_createForm;
            PasswordForm m_passwordForm;
            PlayerMessages m_messages;
            DisconnectForm m_disconnect;
            ReloginScreen m_relogin;
            ui::Element& m_settingsButton;
            SettingsWindow m_settings;
            PadKeyboard m_padKeyboard;

            Stage m_stage = Stage::ChoosingServer;
            std::uint64_t m_nowMs = 0;
            std::uint64_t m_loginSentMs = 0;
            bool m_signingUp = false;
            std::uint64_t m_reloginMs = 0;
            std::optional<SceneKind> m_next;
            std::optional<ScriptedLogin> m_scripted;
            std::mt19937 m_random{std::random_device{}()};
    };

}

#endif
