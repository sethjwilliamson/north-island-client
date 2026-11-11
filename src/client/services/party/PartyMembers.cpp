#include "client/services/party/PartyMembers.h"

#include "audio/MusicPlayer.h"
#include "audio/SoundEffects.h"
#include "client/rendering/hud/PartyPanels.h"
#include "client/rendering/world/ViewZoom.h"
#include "client/scenes/LoginScene.h"
#include "client/scenes/world/WorldScene.h"
#include "client/services/AccountSession.h"
#include "client/services/ServerRelogin.h"
#include "client/services/input/InputTranslator.h"
#include "core/Log.h"
#include "core/StringUtil.h"
#include "net/GameConnection.h"
#include "protocol/TeamRequests.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace client {

    namespace {

        /// @brief Tells whether an element or any of its descendants is a ui::Window.
        /// @param element The element to look at.
        /// @return True when a window is found.
        bool holdsWindow(const ui::Element& element) {
            if (dynamic_cast<const ui::Window*>(&element) != nullptr) {
                return true;
            }
            return std::any_of(element.children().begin(), element.children().end(), [](const auto& child) { return holdsWindow(*child); });
        }

        const std::set<std::string>
            kPopupLayouts{"TalkLayer", "PlayerMessages", "CentreMessages", "ItemInfoLayer", "PetInfoLayer", "RightClickMenuLayer", "MaidLayer"};
        const std::set<std::string> kHudLayouts{"MainStatus", "MainButtons", "FunctionButtons", "HotKeyBar", "TimeEffectLayer"};

        /// @brief Tells whether an SDL event type is a mouse motion, button or wheel event.
        /// @param type The SDL event type.
        /// @return True for a mouse event.
        bool isMouseEvent(Uint32 type) {
            return type == SDL_EVENT_MOUSE_MOTION || type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP || type == SDL_EVENT_MOUSE_WHEEL;
        }

    }

    std::optional<std::vector<PartyAccount>> parsePartyAccounts(const std::vector<std::string>& lines, std::string& error) {
        std::vector<PartyAccount> accounts;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const std::string line = core::trim(lines[index]);
            if (line.empty() || line.front() == '#') {
                continue;
            }
            std::istringstream words(line);
            PartyAccount account;
            std::string slot;
            std::string battle;
            std::string more;
            words >> account.account >> account.password >> slot >> battle >> more;
            const std::string where = "line " + std::to_string(index + 1) + ": ";
            if (account.password.empty() || !more.empty()) {
                error = where + "an account, its password, the character's place (1 or 2) and auto or manual are wanted";
                return std::nullopt;
            }
            if (slot == "auto" || slot == "manual") {
                battle = std::exchange(slot, std::string());
            }
            if (!battle.empty() && battle != "auto" && battle != "manual") {
                error = where + "the battle commands are auto or manual";
                return std::nullopt;
            }
            account.autoBattle = battle != "manual";
            if (!slot.empty()) {
                const auto place = core::parseInt(slot);
                if (!place || (*place != 1 && *place != 2)) {
                    error = where + "the character's place is 1 or 2";
                    return std::nullopt;
                }
                account.slot = *place;
            }
            if (static_cast<int>(accounts.size()) == PartyMembers::kMostMembers) {
                error = "at most " + std::to_string(PartyMembers::kMostMembers) + " accounts go with the one played";
                return std::nullopt;
            }
            accounts.push_back(std::move(account));
        }
        return accounts;
    }

    /// @brief One party member: its account, its own bundle of objects, its world scene and the timers of its logging in, out and away.
    struct PartyMembers::Member {
        /// @brief Creates the member's bundle on the silent sound device.
        /// @param tables The shared game tables.
        /// @param silent The silent sound device.
        /// @param partyAccount The member's account.
        Member(const GameTables& tables, const audio::AudioDevice& silent, PartyAccount partyAccount)
            : account(std::move(partyAccount)),
              bundle(std::make_unique<AccountBundle>(tables, silent)) {}

        PartyAccount account;
        std::unique_ptr<AccountBundle> bundle;
        std::optional<std::reference_wrapper<WorldScene>> world;
        std::optional<std::string> goneReason;
        core::Rect view;
        int invites = 0;
        std::uint64_t invitedMs = 0;
        bool autoBattle = account.autoBattle;
        std::map<const ui::Element*, bool> windowLayouts;

        /// @brief A chat line the member heard, and when.
        struct Heard {
            int channel;
            std::string line;
            std::uint64_t atMs;
        };

        std::vector<Heard> heard;
        std::uint64_t nowMs = 0;
        std::optional<std::uint64_t> leavingSinceMs;
        std::string leavingReason;
        std::optional<std::uint64_t> awaySinceMs;
    };

    PartyMembers::PartyMembers(
        std::unique_ptr<AccountBundle>& playedBundle,
        const audio::AudioDevice& device,
        std::function<void(GameServices&)> playedHooks,
        std::vector<PartyAccount> accounts
    )
        : m_played(playedBundle),
          m_device(device),
          m_setUpPlayed(std::move(playedHooks)),
          m_panels(std::make_unique<PartyPanels>(played().pictures, played().characters)) {
        for (PartyAccount& account : accounts) {
            m_members.push_back(std::make_unique<Member>(GameTables::of(played()), m_silent, std::move(account)));
            setUpMember(*m_members.back());
        }
        setUpPlayed(played());
    }

    PartyMembers::~PartyMembers() {
        GameServices& services = played();
        services.partyChat = nullptr;
        services.partyLogIn = nullptr;
        services.isPartyMember = nullptr;
        services.partyLogOut = nullptr;
        services.partyMirrorsNpcs = nullptr;
        services.setPartyMirrorsNpcs = nullptr;
        services.partyEventStarted = nullptr;
        services.partyEventAnswered = nullptr;
        services.partyEventEnded = nullptr;
    }

    void PartyMembers::setUpPlayed(GameServices& services) {
        services.unseenPartyMember = false;
        services.autoBattle = [] { return false; };
        services.chatHeard = nullptr;
        services.partyMessage = nullptr;
        services.joinsTeamOf = [](std::int32_t) { return false; };
        services.takesLeadFrom = [](std::int32_t) { return false; };
        services.partyChat = [this](int typedChannel, const std::string& text) {
            if (m_selected < 0 || m_selected >= size() || !m_members[static_cast<std::size_t>(m_selected)]->world) {
                return false;
            }
            m_members[static_cast<std::size_t>(m_selected)]->world->get().sayAsPartyMember(typedChannel, text);
            return true;
        };
        services.partyLogIn = [this](const std::string& account, const std::string& password, int slot, bool autoBattle) {
            return add({account, password, slot, autoBattle});
        };
        services.isPartyMember = [this](std::int32_t playerId) { return isMember(playerId); };
        services.partyLogOut = [this](std::int32_t playerId) { logOut(playerId); };
        services.partyMirrorsNpcs = [this] { return m_mirrorNpcs; };
        services.setPartyMirrorsNpcs = [this](bool on) { m_mirrorNpcs = on; };
        services.partyEventStarted = [this](PartyTrigger trigger, int index) {
            forMirroringMembers([trigger, index](Member& member, WorldScene& world) {
                if (!world.mirrorEvent(trigger, index)) {
                    core::logInfo(
                        "party member ", member.account.account, ": cannot follow the leader to ", trigger == PartyTrigger::Door ? "door " : "NPC ", index
                    );
                }
            });
        };
        services.partyEventAnswered = [this](int dataId, int code) {
            forMirroringMembers([dataId, code](Member&, WorldScene& world) { world.mirrorEventAnswer(dataId, code); });
        };
        services.partyEventEnded = [this] {
            forMirroringMembers([](Member&, WorldScene& world) { world.mirrorLeaderEventEnded(); });
        };
        if (m_setUpPlayed) {
            m_setUpPlayed(services);
        }
    }

    void PartyMembers::setUpMember(Member& member) {
        GameServices& services = member.bundle->services;
        const GameServices& main = played();
        services.unseenPartyMember = true;
        services.freeFrames = main.freeFrames;
        services.view = main.view;
        services.videoSettings = main.videoSettings;
        services.changeVideo = [](const ExtraSettings&) {};
        services.renderDrivers = main.renderDrivers;
        services.rendererInUse = main.rendererInUse;
        services.desktopSize = main.desktopSize;
        services.messageBox = [&member](const std::string& caption, const std::string& text) {
            core::logInfo("party member ", member.account.account, ": message box: ", caption, " / ", text);
        };
        services.saveScreenshot = [](const std::filesystem::path&, const PhotoCheck&) {};
        services.autoBattle = [&member] { return member.autoBattle; };
        services.chatHeard = [&member](int channel, const std::string& line, std::int32_t) { member.heard.push_back({channel, line, member.nowMs}); };
        services.partyMessage = [this](bool centre, const std::string& text, int durationMs) { m_messages.push_back({centre, text, durationMs}); };
        services.joinsTeamOf = [this](std::int32_t inviterId) { return m_teamLeaderId != 0 && inviterId == m_teamLeaderId; };
        services.takesLeadFrom = [this](std::int32_t leaderId) {
            const AccountSession& session = m_played->session;
            return session.ownPlayer && session.ownPlayer->playerId == leaderId;
        };
        services.partyChat = nullptr;
        services.partyLogIn = nullptr;
        services.isPartyMember = nullptr;
        services.partyLogOut = nullptr;
        services.partyMirrorsNpcs = nullptr;
        services.setPartyMirrorsNpcs = nullptr;
        services.partyEventStarted = nullptr;
        services.partyEventAnswered = nullptr;
        services.partyEventEnded = nullptr;
    }

    template <typename Call>
    void PartyMembers::forMirroringMembers(const Call& call) {
        if (!m_mirrorNpcs || m_played->kind != SceneKind::World || !m_played->scene) {
            return;
        }
        const int leaderMap = static_cast<const WorldScene&>(*m_played->scene).ownMapId();
        for (const auto& owned : m_members) {
            Member& member = *owned;
            if (member.world && !member.leavingSinceMs && member.world->get().inWorld() && member.world->get().ownMapId() == leaderMap) {
                call(member, member.world->get());
            }
        }
    }

    std::vector<PartyChatLine> PartyMembers::takeChatLines(std::uint64_t nowMs) {
        m_sentLines.erase(
            std::remove_if(m_sentLines.begin(), m_sentLines.end(), [nowMs](const SentLine& sent) { return nowMs - sent.atMs > kChatRepeatMs; }),
            m_sentLines.end()
        );
        std::vector<PartyChatLine> lines;
        for (const auto& owned : m_members) {
            Member& member = *owned;
            auto& heard = member.heard;
            const auto due = std::find_if(heard.begin(), heard.end(), [nowMs](const Member::Heard& line) { return nowMs - line.atMs < kChatDelayMs; });
            for (auto line = heard.begin(); line != due; ++line) {
                const bool sent = std::any_of(m_sentLines.begin(), m_sentLines.end(), [&line](const SentLine& other) {
                    return other.channel == line->channel && other.line == line->line;
                });
                if (sent) {
                    continue;
                }
                m_sentLines.push_back({line->channel, line->line, nowMs});
                lines.push_back({line->channel, line->line, member.world ? member.world->get().ownName() : member.account.account});
            }
            heard.erase(heard.begin(), due);
        }
        return lines;
    }

    void PartyMembers::start() {
        m_started = true;
        for (const auto& member : m_members) {
            logIn(*member);
        }
    }

    void PartyMembers::logIn(Member& member) {
        const AccountSession& leader = played().session;
        AccountSession& session = member.bundle->session;
        session.serverAddress = leader.serverAddress;
        session.serverPort = leader.serverPort;
        session.serverLineName = leader.serverLineName;
        session.loginName = member.account.account;
        session.password = member.account.password;
        session.playingSlot = 0;
        ServerRelogin relogin;
        relogin.account = member.account.account;
        relogin.password = member.account.password;
        relogin.slot = member.account.slot;
        relogin.delayMs = 0;
        session.relogin = relogin;
        member.bundle->kind = SceneKind::Login;
        member.bundle->scene = std::make_unique<LoginScene>(member.bundle->services);
        core::logInfo("party member ", member.account.account, ": logging in to ", leader.serverAddress, " (character ", member.account.slot, ")");
    }

    std::string PartyMembers::add(PartyAccount account) {
        if (account.account.empty() || account.password.empty()) {
            return "Type the account and its password.";
        }
        for (int index = size() - 1; index >= 0; --index) {
            const Member& member = *m_members[static_cast<std::size_t>(index)];
            if (member.goneReason && !member.leavingSinceMs) {
                remove(index);
            }
        }
        const bool taken = account.account == played().session.loginName || std::any_of(m_members.begin(), m_members.end(), [&account](const auto& member) {
                               return member->account.account == account.account;
                           });
        if (taken) {
            return "That account is logged in already.";
        }
        if (size() + m_otherTeammates >= kMostMembers) {
            return "The team is full.";
        }
        m_members.push_back(std::make_unique<Member>(GameTables::of(played()), m_silent, std::move(account)));
        setUpMember(*m_members.back());
        if (m_started) {
            logIn(*m_members.back());
        }
        return {};
    }

    int PartyMembers::otherTeammates(const WorldScene& leaderWorld) const {
        const Teams& teams = leaderWorld.teams();
        const std::int32_t own = leaderWorld.ownId();
        const auto teamLeader = teams.leaderOf(own);
        if (!teamLeader) {
            return 0;
        }
        const std::vector<std::int32_t>& places = teams.members(*teamLeader);
        return static_cast<int>(std::count_if(places.begin(), places.end(), [this](std::int32_t id) {
            return std::none_of(m_members.begin(), m_members.end(), [id](const auto& member) { return member->world && member->world->get().ownId() == id; });
        }));
    }

    bool PartyMembers::isMember(std::int32_t playerId) const {
        return std::any_of(m_members.begin(), m_members.end(), [playerId](const auto& member) {
            return member->world && !member->leavingSinceMs && member->world->get().ownId() == playerId;
        });
    }

    void PartyMembers::logOut(std::int32_t playerId) {
        for (const auto& owned : m_members) {
            Member& member = *owned;
            if (!member.world || member.leavingSinceMs || member.world->get().ownId() != playerId) {
                continue;
            }
            beginLogOut(member, "logging out");
            return;
        }
    }

    void PartyMembers::beginLogOut(Member& member, const std::string& reason) {
        if (const auto leader = member.world->get().teams().leaderOf(member.world->get().ownId())) {
            member.bundle->connection.send(protocol::leaveTeamRequest(*leader));
        }
        member.leavingSinceMs = m_nowMs;
        member.leavingReason = reason;
        core::logInfo("party member ", member.account.account, ": ", reason);
    }

    void PartyMembers::takeLead(int index) {
        Member& member = *m_members[static_cast<std::size_t>(index)];
        AccountBundle& old = *m_played;
        const std::filesystem::path track = old.music.file();
        const std::uint64_t at = old.music.positionMs();
        old.sounds.setDevice(m_silent);
        old.music.setDevice(m_silent);
        member.bundle->sounds.setDevice(m_device);
        member.bundle->music.setDevice(m_device);
        if (!track.empty()) {
            member.bundle->music.playLooped(track, at);
        }
        const PartyAccount oldAccount{old.session.loginName, old.session.password, old.session.playingSlot == 2 ? 2 : 1, member.autoBattle};
        core::logInfo("party member ", member.account.account, ": takes the lead, ", oldAccount.account, " is a member");
        std::swap(m_played, member.bundle);
        member.account = oldAccount;
        member.world.reset();
        if (auto* world = dynamic_cast<WorldScene*>(member.bundle->scene.get())) {
            member.world = std::ref(*world);
        }
        member.goneReason.reset();
        member.view = {};
        member.invites = 0;
        member.windowLayouts.clear();
        member.heard.clear();
        member.awaySinceMs.reset();
        setUpPlayed(played());
        setUpMember(member);
        m_played->scene->screen().setLayoutFilter({});
        m_selected = -1;
        m_commanding = -1;
        m_mouseOwner = -1;
        m_keyboardOwner = -1;
        m_routedKeys.clear();
        m_sentLines.clear();
    }

    void PartyMembers::remove(int index) {
        m_members.erase(m_members.begin() + index);
        // four member rows to renumber: selected, commanding, mouse owner, keyboard owner
        for (int& row : std::array<std::reference_wrapper<int>, 4>{m_selected, m_commanding, m_mouseOwner, m_keyboardOwner}) {
            if (row == index) {
                row = -1;
            } else if (row > index) {
                --row;
            }
        }
    }

    void PartyMembers::gone(Member& member, const std::string& reason) {
        core::logInfo("party member ", member.account.account, ": ", reason);
        member.goneReason = reason;
        member.world.reset();
        member.bundle->scene.reset();
        member.bundle->connection.close();
    }

    void PartyMembers::changeScene(Member& member) {
        const SceneKind next = *member.bundle->scene->nextScene();
        member.world.reset();
        if (next == SceneKind::World) {
            member.bundle->kind = next;
            member.bundle->zoom.reset();
            auto world = std::make_unique<WorldScene>(member.bundle->services);
            member.world = std::ref(*world);
            member.bundle->scene = std::move(world);
            member.view = {};
        } else if (next == SceneKind::Login && member.bundle->session.relogin) {
            member.bundle->kind = next;
            member.bundle->scene = std::make_unique<LoginScene>(member.bundle->services);
            member.view = {};
        } else {
            gone(member, next == SceneKind::Quit ? "left the game" : "logged out");
        }
    }

    template <typename Call>
    void PartyMembers::withWindows(int index, float hudScale, const Call& call) {
        Member& member = *m_members[static_cast<std::size_t>(index)];
        ui::Screen& screen = member.bundle->scene->screen();
        const bool selected = m_selected == index;
        screen.setHudScale(hudScale);
        screen.setLayoutFilter([&member, selected](const ui::Element& layout) {
            if (kPopupLayouts.count(layout.name()) != 0 || (selected && kHudLayouts.count(layout.name()) != 0)) {
                return true;
            }
            const auto known = member.windowLayouts.find(&layout);
            if (known != member.windowLayouts.end()) {
                return known->second;
            }
            return member.windowLayouts[&layout] = holdsWindow(layout);
        });
        call(screen);
        screen.setLayoutFilter({});
    }

    int PartyMembers::memberAt(core::Point point, float hudScale) {
        for (int index = size() - 1; index >= 0; --index) {
            Member& member = *m_members[static_cast<std::size_t>(index)];
            if (!member.world) {
                continue;
            }
            bool caught = false;
            withWindows(index, hudScale, [&](ui::Screen& screen) { caught = screen.catchesAt(point); });
            if (caught) {
                return index;
            }
        }
        return -1;
    }

    void PartyMembers::coverLeaderHud(ui::Screen& leader) const {
        const bool selected = m_selected >= 0 && m_selected < size() && m_members[static_cast<std::size_t>(m_selected)]->world.has_value();
        if (!selected) {
            leader.setLayoutFilter({});
            return;
        }
        leader.setLayoutFilter([](const ui::Element& layout) { return kHudLayouts.count(layout.name()) == 0; });
    }

    void PartyMembers::drawWindows(gfx::Canvas& canvas, std::uint64_t nowMs, float hudScale) {
        for (int index = 0; index < size(); ++index) {
            const Member& member = *m_members[static_cast<std::size_t>(index)];
            if (!member.world) {
                continue;
            }
            const std::string& name = member.world->get().ownName();
            withWindows(index, hudScale, [&](ui::Screen& screen) {
                screen.draw(canvas, nowMs);
                for (const core::Rect& window : screen.shownWindows()) {
                    PartyPanels::drawWindowTag(canvas, window, name);
                }
            });
        }
    }

    void PartyMembers::drawPanels(gfx::Canvas& canvas, float hudScale, const WorldScene& leaderWorld) {
        m_panels->draw(canvas, views(), leaderWorld.battlePanelIds(), m_played->scene->screen().view(), hudScale, m_mouse);
    }

    bool PartyMembers::handleEvent(const SDL_Event& event, core::Point origin, float hudScale, bool memberShown) {
        if (!m_started) {
            return false;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            m_mouse = core::Point{static_cast<int>(event.motion.x) - origin.x, static_cast<int>(event.motion.y) - origin.y};
        }
        if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) && event.button.button == SDL_BUTTON_LEFT) {
            const core::Point point{static_cast<int>(event.button.x) - origin.x, static_cast<int>(event.button.y) - origin.y};
            const int pressed = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? m_panels->memberAt(point) : -1;
            const int row = pressed < size() ? pressed : -1;
            if (row >= 0) {
                m_panelPressed = true;
                m_selected = m_selected == row ? -1 : row;
                core::logInfo(
                    "party: ",
                    m_selected < 0 ? std::string("the leader") : m_members[static_cast<std::size_t>(row)]->account.account,
                    " has the menus"
                );
                return true;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && std::exchange(m_panelPressed, false)) {
                return true;
            }
        }
        if (memberShown) {
            return false;
        }
        const auto forwardTo = [&](int index) { withWindows(index, hudScale, [&](ui::Screen& screen) { forwardEvent(event, screen, origin); }); };
        const auto usable = [this](int index) { return index >= 0 && index < size() && m_members[static_cast<std::size_t>(index)]->world.has_value(); };
        if (isMouseEvent(event.type)) {
            if (usable(m_mouseOwner) && event.type != SDL_EVENT_MOUSE_WHEEL) {
                const int owner = m_mouseOwner;
                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(event.button.button)) == 0) {
                    m_mouseOwner = -1;
                }
                forwardTo(owner);
                return true;
            }
            m_mouseOwner = -1;
            float x = 0;
            float y = 0;
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                x = event.motion.x;
                y = event.motion.y;
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                x = event.wheel.mouse_x;
                y = event.wheel.mouse_y;
            } else {
                x = event.button.x;
                y = event.button.y;
            }
            const int index = memberAt({static_cast<int>(x) - origin.x, static_cast<int>(y) - origin.y}, hudScale);
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                m_keyboardOwner = index;
                m_mouseOwner = index;
            }
            if (index >= 0) {
                forwardTo(index);
                return true;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                for (int other = 0; other < size(); ++other) {
                    if (usable(other)) {
                        forwardTo(other);
                    }
                }
            }
            return false;
        }
        if (event.type == SDL_EVENT_KEY_UP) {
            const auto routed = std::find(m_routedKeys.begin(), m_routedKeys.end(), event.key.key);
            if (routed == m_routedKeys.end()) {
                return false;
            }
            m_routedKeys.erase(routed);
            const int owner = usable(m_keyboardOwner) ? m_keyboardOwner : m_selected;
            if (usable(owner)) {
                forwardTo(owner);
            }
            return true;
        }
        const bool typing = usable(m_keyboardOwner) && m_members[static_cast<std::size_t>(m_keyboardOwner)]->bundle->scene->screen().isTyping();
        if (event.type == SDL_EVENT_TEXT_INPUT) {
            if (typing) {
                forwardTo(m_keyboardOwner);
                return true;
            }
            return false;
        }
        if (event.type != SDL_EVENT_KEY_DOWN) {
            return false;
        }
        if (typing && event.key.key != SDLK_ESCAPE) {
            m_routedKeys.push_back(event.key.key);
            forwardTo(m_keyboardOwner);
            return true;
        }
        if (!usable(m_selected)) {
            return false;
        }
        if (event.key.key == SDLK_ESCAPE) {
            if (!event.key.repeat && !m_members[static_cast<std::size_t>(m_selected)]->bundle->scene->screen().closeTopOnEscape()) {
                m_selected = -1;
                core::logInfo("party: the leader has the menus");
            }
            return true;
        }
        if ((event.key.mod & SDL_KMOD_CTRL) != 0) {
            m_routedKeys.push_back(event.key.key);
            forwardTo(m_selected);
            return true;
        }
        return false;
    }

    std::optional<std::reference_wrapper<Scene>> PartyMembers::commandingScene(bool leaderDone) {
        int found = -1;
        if (leaderDone) {
            for (int index = 0; index < size(); ++index) {
                const Member& member = *m_members[static_cast<std::size_t>(index)];
                if (member.world && !member.autoBattle && member.world->get().battleActor() != 0) {
                    found = index;
                    break;
                }
            }
        }
        if (found != m_commanding) {
            m_commanding = found;
            if (found >= 0) {
                core::logInfo("party member ", m_members[static_cast<std::size_t>(found)]->account.account, ": gives its battle commands");
            }
        }
        if (found < 0) {
            return std::nullopt;
        }
        return std::ref(*m_members[static_cast<std::size_t>(found)]->bundle->scene);
    }

    void PartyMembers::inviteToTeam(Member& member, const WorldScene& leaderWorld, std::uint64_t nowMs) {
        const std::int32_t memberId = member.world->get().ownId();
        if (member.world->get().teams().inTeam(memberId) || member.invites >= kMostInvites || (member.invites > 0 && nowMs - member.invitedMs < kInviteGapMs)) {
            return;
        }
        if (m_teamLeaderId != leaderWorld.ownId() || static_cast<int>(leaderWorld.teams().members(m_teamLeaderId).size()) >= kMostMembers) {
            return;
        }
        played().connection.send(protocol::inviteRequest(memberId));
        ++member.invites;
        member.invitedMs = nowMs;
        core::logInfo("party member ", member.account.account, ": invited to the team (", member.invites, ")");
    }

    void PartyMembers::update(std::uint64_t nowMs, std::optional<std::reference_wrapper<const WorldScene>> leaderWorld) {
        const int leaderMapId = leaderWorld && leaderWorld->get().inWorld() ? leaderWorld->get().ownMapId() : 0;
        const bool leaderInWorld = leaderMapId != 0;
        m_teamLeaderId = leaderInWorld ? leaderWorld->get().teams().leaderOf(leaderWorld->get().ownId()).value_or(leaderWorld->get().ownId()) : 0;
        if (leaderInWorld && m_teamLeaderId != leaderWorld->get().ownId()) {
            for (int index = 0; index < size(); ++index) {
                const Member& member = *m_members[static_cast<std::size_t>(index)];
                if (member.world && !member.leavingSinceMs && member.world->get().inWorld() && member.world->get().ownId() == m_teamLeaderId) {
                    takeLead(index);
                    return;
                }
            }
        }
        m_otherTeammates = leaderInWorld ? otherTeammates(leaderWorld->get()) : 0;
        m_nowMs = nowMs;
        for (int index = size() - 1; index >= 0; --index) {
            const Member& member = *m_members[static_cast<std::size_t>(index)];
            if (member.leavingSinceMs && (!member.bundle->connection.sendsPending() || nowMs - *member.leavingSinceMs >= kLeaveWaitMs)) {
                core::logInfo("party member ", member.account.account, ": logged out");
                remove(index);
            }
        }
        for (const auto& owned : m_members) {
            Member& member = *owned;
            if (!member.bundle->scene) {
                continue;
            }
            member.bundle->services.freeFrames = played().freeFrames;
            member.bundle->services.nextBeatMs = played().nextBeatMs;
            if (!(member.view == played().view)) {
                member.view = played().view;
                member.bundle->services.view = played().view;
                member.bundle->zoom.setWindow(played().view);
                member.bundle->scene->screen().setView(played().view);
            }
            member.nowMs = nowMs;
            member.bundle->zoom.setNow(nowMs);
            member.bundle->music.update();
            member.bundle->scene->update(nowMs);
            if (member.bundle->scene->nextScene()) {
                changeScene(member);
                continue;
            }
            if (member.bundle->kind == SceneKind::Login && !member.bundle->session.relogin) {
                gone(member, "could not log in");
                continue;
            }
            if (member.bundle->kind == SceneKind::Login && member.bundle->session.playingSlot == member.account.slot) {
                const auto& characters = member.bundle->session.characters;
                const bool there = std::any_of(characters.begin(), characters.end(), [&member](const protocol::CharacterSummary& character) {
                    return character.slot == member.account.slot;
                });
                if (!there) {
                    gone(member, "place " + std::to_string(member.account.slot) + " is empty");
                    continue;
                }
            }
            if (member.world) {
                member.world->get().followView(nowMs);
                if (leaderInWorld && !member.leavingSinceMs && member.world->get().inWorld() && played().session.ownPlayer) {
                    inviteToTeam(member, leaderWorld->get(), nowMs);
                    if (member.world->get().ownMapId() == leaderMapId) {
                        member.awaySinceMs.reset();
                    } else if (!member.awaySinceMs) {
                        member.awaySinceMs = nowMs;
                    } else if (nowMs - *member.awaySinceMs >= kAwayMs) {
                        beginLogOut(member, "on another map");
                    }
                }
            }
        }
    }

    void PartyMembers::move(std::uint64_t nowMs) {
        for (const auto& member : m_members) {
            if (member->bundle->scene) {
                member->bundle->scene->move(nowMs);
            }
        }
    }

    std::vector<PartyMemberView> PartyMembers::views() const {
        std::vector<PartyMemberView> views;
        for (const auto& member : m_members) {
            PartyMemberView view;
            view.account = member->account.account;
            view.autoBattle = member->autoBattle;
            view.commanding = static_cast<int>(views.size()) == m_commanding;
            view.selected = static_cast<int>(views.size()) == m_selected;
            if (member->leavingSinceMs) {
                view.state = PartyMemberView::State::Gone;
                view.reason = member->leavingReason;
            } else if (member->goneReason) {
                view.state = PartyMemberView::State::Gone;
                view.reason = *member->goneReason;
            } else if (member->world && member->world->get().inWorld()) {
                const WorldScene& world = member->world->get();
                const PlayerStatus& status = world.ownStatus();
                view.state = world.inBattle() ? PartyMemberView::State::InBattle : PartyMemberView::State::InWorld;
                view.name = world.ownName();
                view.id = world.ownId();
                view.element = status.element;
                view.job = status.job;
                view.look = world.ownLook();
                view.level = status.level;
                view.hp = status.hp;
                view.maxHp = status.maxHp;
                view.sp = status.sp;
                view.maxSp = status.maxSp;
                const auto leader = world.teams().leaderOf(world.ownId());
                view.inTeam = leader && *leader == m_teamLeaderId;
            }
            views.push_back(std::move(view));
        }
        return views;
    }

}
