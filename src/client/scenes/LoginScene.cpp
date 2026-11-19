#include "client/scenes/LoginScene.h"

#include "client/actions/MessageDispatcher.h"
#include "client/models/remote/RemoteFile.h"
#include "client/models/system/SystemOptions.h"
#include "core/Log.h"
#include "protocol/AccountRequests.h"
#include "protocol/CreateCharacterRequest.h"
#include "protocol/DeleteCharacterRequest.h"
#include "protocol/LoginRequest.h"
#include "protocol/MessageId.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace client {

    namespace {

        constexpr std::uint64_t kLoginTimeoutMs = 30000; // give up waiting for the login answer after 30 seconds

        /// @brief Sets the music and sound volumes the loading and login screens keep, full loudness when none is saved.
        /// @param services Game services (video settings, music, sounds).
        void applyLoadingVolumes(GameServices& services) {
            const ExtraSettings video = services.videoSettings();
            const int music = video.loadingMusicVolume >= 0 ? SystemOptions::loudnessPercent(video.loadingMusicVolume) : 100; // percent
            const int sound = video.loadingSoundVolume >= 0 ? SystemOptions::loudnessPercent(video.loadingSoundVolume) : 100; // percent
            services.music.setVolume(video.loadingMusicOn ? music : 0);
            services.sounds.setVolume(video.loadingSoundOn ? sound : 0);
        }

    }

    LoginScene::LoginScene(
        GameServices& services
    )
        : m_services(services),
          m_screen(services.pictures),
          m_background(m_screen.load(services.game.layout("LoginScene.xml"), services.loadContext())),
          m_serverForm(services, m_screen, {[this](const formats::ServerEntry& server) { connect(server); }, [this] { m_next = SceneKind::Quit; }}),
          m_loginForm(
              services,
              m_screen,
              m_messages,
              {[this](const std::string& account, const std::string& password) { login(account, password); },
               [this] { backToServerList(); },
               [this] { showSignUp(); }}
          ),
          m_signUpForm(
              services,
              m_screen,
              m_messages,
              {[this](const std::string& account, const std::string& password) {
                   m_signingUp = true;
                   login(account, password);
               },
               [this] { showLoginForm(); }}
          ),
          m_characterForm(
              services,
              m_screen,
              {[this](int slot) { enterWorld(slot); },
               [this](int slot) { createCharacter(slot); },
               [this](int slot) { deleteCharacter(slot); },
               [this] { backToLoginForm(); }}
          ),
          m_createForm(
              services,
              m_screen,
              m_messages,
              m_passwordForm,
              {[this] { cancelCreating(); },
               [this](const std::string& name) { m_services.connection.send(protocol::checkNameRequest(name)); },
               [this](const protocol::CreateCharacterRequest& request) { sendNewCharacter(request); }}
          ),
          m_passwordForm(services, m_screen, m_messages),
          m_messages(services, m_screen),
          m_disconnect(services, m_screen, {[this] { backToServerList(); }, [this] { m_next = SceneKind::Quit; }}),
          m_relogin(services, m_screen),
          m_settingsButton(m_screen.load(services.game.layout("LoginSettingsButton.xml"), services.loadContext())),
          m_settings(services, m_screen, {}, true),
          m_padKeyboard(services, m_screen) {
        m_screen.context().playSound = [this](const std::string& sound) { m_services.playSound(sound); };
        // the original starts the loading music again whenever it is outside the game (CheckStartMusic)
        applyLoadingVolumes(services);
        services.music.playLooped(services.game.data("Sound/BGM0013.wav"));
        m_screen.on("onLoginSettings", [this](const ui::EventArgs&) { m_settings.toggle(); });
        m_screen.on("onSettingsWindowEscape", [this](const ui::EventArgs&) { m_settings.close(); });
        m_screen.setKeyPreview([this](const ui::KeyEvent& key) {
            if (key.key != ui::Key::Escape) {
                return false;
            }
            if (m_settings.isOpen()) {
                m_settings.close();
            } else if (m_disconnect.isOpen()) {
                m_disconnect.hide();
            } else if (m_signUpForm.isVisible()) {
                m_signUpForm.hide();
                showLoginForm();
            } else if (m_passwordForm.isVisible()) {
                m_passwordForm.hide();
            } else {
                return false;
            }
            m_services.playSound("sound/wav0154.wav"); // sound file wav0154, played when a form is dismissed here
            return true;
        });
        if (m_services.session.relogin) {
            startRelogin();
        } else {
            backToServerList();
        }
    }

    void LoginScene::startRelogin() {
        m_serverForm.hide();
        m_loginForm.hide();
        m_characterForm.hide();
        m_createForm.hide();
        m_reloginMs = 0;
        m_relogin.show(m_services.session.relogin->toPvp);
        m_settings.close();
        m_settingsButton.setVisible(false);
        m_stage = Stage::Relogin;
    }

    void LoginScene::continueRelogin() {
        AccountSession& session = m_services.session;
        const bool sameServer = session.relogin->address.empty();
        const std::string address = sameServer ? session.serverAddress : session.relogin->address;
        const int port = sameServer ? session.serverPort : 0;
        m_services.connection.connect(address, port > 0 ? static_cast<std::uint16_t>(port) : net::GameConnection::kGamePort);
        session.serverAddress = address;
        session.serverPort = port;
        if (session.relogin->toPvp && !session.relogin->lineName.empty()) {
            session.serverLineName = session.relogin->lineName;
        }
        m_services.connection.send({protocol::kActionHello});
        m_stage = Stage::Connecting;
    }

    void LoginScene::abandonRelogin() {
        endRelogin();
        backToServerList();
    }

    void LoginScene::endRelogin() {
        m_services.session.relogin.reset();
        m_relogin.hide();
        m_settingsButton.setVisible(true);
    }

    bool LoginScene::startLogin(const std::string& account, const std::string& password, int slot, std::string& reason) {
        if (m_stage != Stage::ChoosingServer) {
            reason = "the login screen is busy (not at the server list)";
            return false;
        }
        m_scripted = ScriptedLogin{account, password, slot};
        if (!m_serverForm.connectFirst()) {
            m_scripted.reset();
            reason = "the server list has no server";
            return false;
        }
        return true;
    }

    void LoginScene::reportError(const std::string& text) {
        if (m_services.errorShown) {
            m_services.errorShown(text);
        }
    }

    void LoginScene::connect(const formats::ServerEntry& server) {
        if (m_stage != Stage::ChoosingServer) {
            return;
        }
        m_services.connection.connect(server.address, server.port > 0 ? static_cast<std::uint16_t>(server.port) : net::GameConnection::kGamePort);
        m_services.session.serverLineName = server.name;
        m_services.session.serverAddress = server.address;
        m_services.session.serverPort = server.port;
        m_services.connection.send({protocol::kActionHello});
        m_serverForm.hide();
        m_stage = Stage::Connecting;
    }

    void LoginScene::login(const std::string& account, const std::string& password) {
        if (m_stage != Stage::EnteringAccount) {
            return;
        }
        m_services.session.loginName = account;
        m_services.session.password = password;
        protocol::LoginRequest request;
        request.account = account;
        request.password = password;
        // random login key byte, 0..254
        request.key = static_cast<std::uint8_t>(std::uniform_int_distribution<int>(0, 254)(m_random));
        std::error_code error;
        const auto itemDatSize = std::filesystem::file_size(m_services.game.data("Data/Item.dat"), error);
        if (!error) {
            request.itemDatSize = itemDatSize;
        }
        m_services.connection.send(request.encode());
        m_loginSentMs = m_nowMs;
        m_stage = Stage::LoggingIn;
    }

    void LoginScene::backToServerList() {
        m_services.connection.close();
        m_services.session = AccountSession();
        m_scripted.reset();
        m_relogin.hide();
        m_loginForm.hide();
        m_signUpForm.hide();
        m_signingUp = false;
        m_characterForm.hide();
        m_createForm.hide();
        m_serverForm.show();
        m_stage = Stage::ChoosingServer;
    }

    void LoginScene::showLoginForm() {
        m_serverForm.hide();
        m_characterForm.hide();
        m_signUpForm.hide();
        m_loginForm.show();
        m_stage = Stage::EnteringAccount;
    }

    void LoginScene::showSignUp() {
        if (m_stage != Stage::EnteringAccount) {
            return;
        }
        m_loginForm.hide();
        m_signUpForm.show();
    }

    void LoginScene::enterWorld(int slot) {
        if (m_stage != Stage::ChoosingCharacter) {
            return;
        }
        m_services.connection.send(protocol::selectCharacterRequest(slot));
        m_services.session.playingSlot = slot;
        m_characterForm.hide();
        m_stage = Stage::EnteringWorld;
    }

    void LoginScene::createCharacter(int slot) {
        if (m_stage != Stage::ChoosingCharacter) {
            return;
        }
        m_services.connection.send(protocol::selectCharacterRequest(slot));
        m_services.session.playingSlot = slot;
    }

    void LoginScene::deleteCharacter(int slot) {
        if (m_stage != Stage::ChoosingCharacter) {
            return;
        }
        m_characterForm.hide();
        m_passwordForm.show(
            PasswordForm::Mode::Confirm,
            [this, slot](const PasswordForm::Result& result) {
                m_services.connection.send(protocol::DeleteCharacterRequest{slot, result.password, result.cipher}.encode());
                m_characterForm.show();
            },
            [this] { m_characterForm.show(); }
        );
    }

    void LoginScene::onCharacterDeleted(int result, int slot) {
        constexpr int kMs = 2000; // how long the result message shows, ms
        switch (result) {
            case 1: // server delete result 1: character deleted
                m_messages.show("Succeeded in deleting the character", kMs);
                // slots 1 and 2 have a Remote Control file; the second slot's player id is the login id + 4500000
                if (slot == 1 || slot == 2) {
                    std::error_code error;
                    const std::int32_t playerId = static_cast<std::int32_t>(m_services.session.loginId) +
                                                  (slot == 2 ? 4500000 : 0); // 4500000: player id offset of the second character slot
                    std::filesystem::remove(RemoteFile::pathFor(m_services.game.userRoot(), playerId), error);
                }
                for (auto& c : m_services.session.characters) {
                    if (c.slot == slot) {
                        c.level = 0;
                    }
                }
                if (m_characterForm.isVisible()) {
                    m_characterForm.show();
                }
                break;
            case 2: // server delete result 2: wrong password
                m_messages.show("wrong password when deleting the character", kMs);
                break;
            case 3: // server delete result 3: wrong cipher
                m_messages.show("wrong cipher when deleting the character", kMs);
                break;
            case 5: // server delete result 5: failed with an error code (the slot is shown)
                m_messages.show("Fail to delete the character, Error codes: " + std::to_string(slot), kMs);
                break;
            default: // any other result: plain failure
                m_messages.show("Fail to delete the character", kMs);
                break;
        }
    }

    void LoginScene::backToLoginForm() {
        m_services.connection.send(protocol::leaveCharacterSelectRequest());
        m_services.session.loginId = 0;
        m_services.session.characters.clear();
        showLoginForm();
    }

    void LoginScene::onKicked(int reason) {
        endRelogin();
        m_signingUp = false;
        m_loginForm.hide();
        m_signUpForm.hide();
        m_characterForm.hide();
        m_createForm.hide();
        m_disconnect.show(disconnectNotice(reason, static_cast<std::int32_t>(m_services.session.loginId)));
    }

    void LoginScene::onServerSelected(const protocol::ServerSelected& message) {
        core::logInfo("server ", message.serverId, " (", message.serverName, ") accepted the connection");
        m_services.session.serverId = message.serverId;
        m_services.session.serverName = message.serverName;
        m_services.session.serverGrouped = message.grouped;
        if (const auto& relogin = m_services.session.relogin) {
            m_stage = Stage::EnteringAccount;
            login(relogin->account, relogin->password);
            return;
        }
        if (m_scripted) {
            m_serverForm.hide();
            m_stage = Stage::EnteringAccount;
            login(m_scripted->account, m_scripted->password);
            return;
        }
        showLoginForm();
    }

    void LoginScene::onLoginRejected(LoginRejection reason) {
        if (m_services.session.relogin) {
            abandonRelogin();
            return;
        }
        if (std::exchange(m_signingUp, false)) {
            m_messages.show(
                reason == LoginRejection::WrongPassword ? "That account name is taken, or the server makes no new accounts."
                                                        : "The server did not answer, please try later"
            );
            m_stage = Stage::EnteringAccount;
            m_signUpForm.show(m_services.session.loginName);
            return;
        }
        m_loginForm.clearFields();
        reportError(reason == LoginRejection::WrongPassword ? "login refused: wrong account or password" : "login refused: the server did not answer");
        switch (reason) {
            case LoginRejection::Timeout: // login timed out
                m_messages.show("Time out, please try later");
                break;
            case LoginRejection::WrongPassword: // wrong account or password
                m_messages.show("wrong account or password");
                break;
            case LoginRejection::ConnectionFailed: // connection to the server failed
                m_messages.show("Connection failed, please tray later");
                break;
        }
        showLoginForm();
    }

    void LoginScene::onLocked(bool wholeAccount, std::uint32_t seconds) {
        // 86400: seconds per day
        m_loginForm.hide();
        m_disconnect.show(
            DisconnectNotice{
                std::string(wholeAccount ? "The account" : "The character") + " has been locked, remaining " + std::to_string(seconds / 86400 + 1) + " day(s)",
                DisconnectNotice::Buttons::PreviousAndQuit
            }
        );
    }

    void LoginScene::onCharacterList(const protocol::CharacterList& list) {
        core::logInfo("logged in: ", list.characters.size(), " character(s)");
        m_services.session.characters = list.characters;
        if (const auto& relogin = m_services.session.relogin) {
            m_stage = Stage::ChoosingCharacter;
            enterWorld(relogin->slot);
            return;
        }
        if (m_scripted) {
            const int slot = m_scripted->slot;
            if (std::none_of(list.characters.begin(), list.characters.end(), [slot](const auto& character) { return character.slot == slot; })) {
                reportError("login: the account has no character in slot " + std::to_string(slot));
            }
            m_stage = Stage::ChoosingCharacter;
            enterWorld(slot);
            return;
        }
        if (m_stage == Stage::LoggingIn && std::exchange(m_signingUp, false)) {
            m_loginForm.rememberAccount(m_services.session.loginName);
            m_messages.show(
                list.characters.empty() ? "The account " + m_services.session.loginName + " is ready." : "Logged in to " + m_services.session.loginName + "."
            );
        } else if (m_stage == Stage::LoggingIn) {
            m_loginForm.rememberAccount();
        }
        m_loginForm.hide();
        m_characterForm.show();
        m_stage = Stage::ChoosingCharacter;
    }

    void LoginScene::onLoginId(std::uint32_t loginId) { m_services.session.loginId = loginId; }

    void LoginScene::onCreateCharacter(bool askCipher) {
        if (m_stage != Stage::ChoosingCharacter) {
            return;
        }
        m_characterForm.hide();
        m_createForm.show(askCipher);
        m_stage = Stage::CreatingCharacter;
    }

    void LoginScene::onNameChecked(NameCheckResult result) {
        if (m_stage == Stage::CreatingCharacter) {
            m_createForm.nameChecked(result);
        }
    }

    void LoginScene::cancelCreating() {
        m_services.connection.send(protocol::cancelCreateCharacterRequest());
        m_characterForm.show();
        m_stage = Stage::ChoosingCharacter;
    }

    void LoginScene::sendNewCharacter(const protocol::CreateCharacterRequest& request) {
        m_services.connection.send(request.encode());
        m_stage = Stage::EnteringWorld;
    }

    void LoginScene::onOwnPlayerEntered(const protocol::OwnPlayerRecord& record) {
        if (m_stage != Stage::EnteringWorld) {
            return;
        }
        core::logInfo("entering the world: map ", record.mapId, " at ", record.x, ',', record.y);
        m_services.session.ownPlayer = record;
        m_next = SceneKind::World;
    }

    void LoginScene::onUnhandled(protocol::MessageId id) {
        core::logInfo("server message ", static_cast<int>(id.action), '/', static_cast<int>(id.sub), " is not handled yet");
    }

    void LoginScene::handleConnectionEvents() {
        while (const auto event = m_services.connection.nextEvent()) {
            switch (*event) {
                case net::GameConnection::Event::Connected: // the socket connected
                    core::logInfo("connected");
                    break;
                case net::GameConnection::Event::ConnectFailed: // the connect attempt failed
                    m_services.connection.close();
                    endRelogin();
                    m_disconnect.show("Connection Failed");
                    break;
                case net::GameConnection::Event::Reset: // the connection was reset
                    if (m_stage != Stage::ChoosingServer && m_disconnect.isOpen()) {
                        m_disconnect.show("Connection Failed");
                    }
                    break;
                case net::GameConnection::Event::Disconnected: // the connection was lost
                    if (m_stage == Stage::EnteringAccount || m_stage == Stage::ChoosingCharacter || m_stage == Stage::CreatingCharacter) {
                        if (!m_disconnect.isOpen()) {
                            m_loginForm.hide();
                            m_signUpForm.hide();
                            m_characterForm.hide();
                            m_createForm.hide();
                            m_disconnect.show(DisconnectNotice{"Intermit to connect to the Server", DisconnectNotice::Buttons::PreviousAndQuit});
                        }
                    } else if (m_stage != Stage::ChoosingServer && m_stage != Stage::Relogin) {
                        m_messages.show("Intermit to connect to the Server");
                        backToServerList();
                    }
                    break;
            }
        }
    }

    void LoginScene::update(std::uint64_t nowMs) {
        m_nowMs = nowMs;
        m_padKeyboard.update(nowMs);
        m_services.connection.update(nowMs);
        handleConnectionEvents();
        MessageContext context{*this, m_services.connection, m_services.session.loginId};
        while (!m_next) {
            const auto payload = m_services.connection.nextMessage();
            if (!payload) {
                break;
            }
            dispatchMessage(*payload, context);
        }
        m_serverForm.update();
        m_createForm.update(nowMs);
        m_relogin.update();
        if (m_stage == Stage::Relogin) {
            if (m_reloginMs == 0) {
                m_reloginMs = nowMs;
            } else if (nowMs - m_reloginMs >= m_services.session.relogin->delayMs) {
                continueRelogin();
            }
        }
        if (m_stage == Stage::LoggingIn && nowMs - m_loginSentMs > kLoginTimeoutMs) {
            if (m_services.session.relogin) {
                abandonRelogin();
            } else {
                m_messages.show("Failure to connect the server");
                reportError("login: the server did not answer within 30 s");
                backToServerList();
            }
        }
        m_screen.update(nowMs);
    }

    void LoginScene::draw(gfx::Canvas& canvas, std::uint64_t nowMs) { m_screen.draw(canvas, nowMs); }

}
