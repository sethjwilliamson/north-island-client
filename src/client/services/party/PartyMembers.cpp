#include "client/services/party/PartyMembers.h"

#include "audio/MusicPlayer.h"
#include "audio/SoundEffects.h"
#include "client/rendering/hud/PartyPanels.h"
#include "client/rendering/hud/SimplePartyPanels.h"
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
#include <unordered_map>
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

        /// @brief Tells whether a layout is one of the popups (kPopupLayouts).
        /// @param layout The layout root.
        /// @return True for a popup.
        bool isPopupLayout(const ui::Element& layout) { return kPopupLayouts.count(layout.name()) != 0; }

        /// @brief Tells whether a layout is one of a list.
        /// @param layouts The layout roots.
        /// @param layout The layout root.
        /// @return True when listed.
        bool listed(const std::vector<const ui::Element*>& layouts, const ui::Element& layout) {
            return std::find(layouts.begin(), layouts.end(), &layout) != layouts.end();
        }

        constexpr std::uint8_t kLayoutPopup = 1; // a layout in kPopupLayouts
        constexpr std::uint8_t kLayoutWindow = 2; // a layout holding a window

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
                error = where + "an account, its password and the character's place (1 or 2) are wanted";
                return std::nullopt;
            }
            if (slot == "auto" || slot == "manual") {
                battle = std::exchange(slot, std::string());
            }
            if (!battle.empty() && battle != "auto" && battle != "manual") {
                error = where + "after the place, only auto or manual (older files; members always take manual commands)";
                return std::nullopt;
            }
            if (!slot.empty()) {
                const auto place = core::parseInt(slot);
                if (!place || (*place != 1 && *place != 2)) {
                    error = where + "the character's place is 1 or 2";
                    return std::nullopt;
                }
                account.slot = *place;
            }
            accounts.push_back(std::move(account));
        }
        return accounts;
    }

    /// @brief One party member (an own character other than the selected one): its account, its own bundle of objects, its world scene, its following and the timers of its logging in and out.
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
        // what each layout of the member's screen now is (kLayoutPopup, kLayoutWindow), worked out once: the filter asks for every layout three
        // times a frame; cleared whenever the screen changes (a new scene, a swap)
        std::unordered_map<const ui::Element*, std::uint8_t> windowLayouts;

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
        Following following;
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
          m_panels(std::make_unique<PartyPanels>(played().pictures, played().characters)),
          m_simplePanels(std::make_unique<SimplePartyPanels>(played().pictures, played().characters)) {
        for (PartyAccount& account : accounts) {
            m_members.push_back(std::make_unique<Member>(GameTables::of(played()), m_silent, std::move(account)));
            setUpMember(*m_members.back());
        }
        setUpPlayed(played());
    }

    PartyMembers::~PartyMembers() {
        GameServices& services = played();
        services.partyLogIn = nullptr;
        services.partyLogOut = nullptr;
        services.partyMirrorsNpcs = nullptr;
        services.setPartyMirrorsNpcs = nullptr;
        services.partyEventStarted = nullptr;
        services.partyEventAnswered = nullptr;
        services.partyEventEnded = nullptr;
        services.walkElsewhere = nullptr;
    }

    void PartyMembers::setUpPlayed(GameServices& services) {
        services.unseenPartyMember = false;
        services.chatHeard = nullptr;
        services.partyMessage = nullptr;
        services.joinsTeamOf = [this](std::int32_t inviterId) { return isOwnCharacter(inviterId); };
        services.takesLeadFrom = [this](std::int32_t leaderId) { return isOwnCharacter(leaderId); };
        services.partyLogIn = [this](const std::string& account, const std::string& password, int slot) {
            return add({account, password, slot});
        };
        services.partyLogOut = [this](std::int32_t playerId) { logOut(playerId); };
        services.partyMirrorsNpcs = [this] { return m_mirrorNpcs; };
        services.setPartyMirrorsNpcs = [this](bool on) { m_mirrorNpcs = on; };
        services.partyEventStarted = [this](PartyTrigger trigger, int index) {
            forMirroringMembers([trigger, index](Member& member, WorldScene& world) {
                if (!world.mirrorEvent(trigger, index)) {
                    core::logInfo(
                        "party member ", member.account.account, ": cannot follow the selected character to ", trigger == PartyTrigger::Door ? "door " : "NPC ", index
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
        services.walkElsewhere = [this, &services](core::Point mapPoint) { return walkToLeader(services, mapPoint); };
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
        services.chatHeard = [&member](int channel, const std::string& line, std::int32_t) {
            if (channel == kWhisperChannel || channel == kTeamChannel) {
                member.heard.push_back({channel, line, member.nowMs});
            }
        };
        services.partyMessage = [this](bool centre, const std::string& text, int durationMs) { m_messages.push_back({centre, text, durationMs}); };
        services.joinsTeamOf = [this](std::int32_t inviterId) { return isOwnCharacter(inviterId); };
        services.takesLeadFrom = [this](std::int32_t leaderId) { return isOwnCharacter(leaderId); };
        services.partyLogIn = nullptr;
        services.partyLogOut = nullptr;
        services.partyMirrorsNpcs = nullptr;
        services.setPartyMirrorsNpcs = nullptr;
        services.partyEventStarted = nullptr;
        services.partyEventAnswered = nullptr;
        services.partyEventEnded = nullptr;
        services.walkElsewhere = [this, &services](core::Point mapPoint) { return walkToLeader(services, mapPoint); };
    }

    WorldScene* PartyMembers::worldOf(const GameServices& services) {
        if (&services == &played()) {
            return m_played->kind == SceneKind::World && m_played->scene ? static_cast<WorldScene*>(m_played->scene.get()) : nullptr;
        }
        for (const auto& member : m_members) {
            if (&member->bundle->services == &services) {
                return member->world ? &member->world->get() : nullptr;
            }
        }
        return nullptr;
    }

    WorldScene* PartyMembers::ownWorld(std::int32_t playerId) {
        if (playerId == 0) {
            return nullptr;
        }
        if (WorldScene* world = worldOf(played()); world && world->inWorld() && world->ownId() == playerId) {
            return world;
        }
        for (const auto& member : m_members) {
            if (member->world && !member->leavingSinceMs && member->world->get().inWorld() && member->world->get().ownId() == playerId) {
                return &member->world->get();
            }
        }
        return nullptr;
    }

    bool PartyMembers::walkToLeader(const GameServices& services, core::Point mapPoint) {
        const WorldScene* self = worldOf(services);
        if (self == nullptr || !self->inWorld()) {
            return false;
        }
        const auto leaderId = self->teams().leaderOf(self->ownId());
        if (!leaderId || *leaderId == self->ownId()) {
            return false; // not in a team, or its leader: it walks itself
        }
        WorldScene* leader = ownWorld(*leaderId);
        if (leader == nullptr || leader->ownMapId() != self->ownMapId()) {
            return false; // led by someone else's character, or the leader is elsewhere
        }
        return leader->walkTo(mapPoint);
    }

    bool PartyMembers::isOwnCharacter(std::int32_t playerId) const {
        if (playerId == 0) {
            return false;
        }
        const AccountSession& session = m_played->session;
        return (session.ownPlayer && session.ownPlayer->playerId == playerId) || isMember(playerId);
    }

    template <typename Call>
    void PartyMembers::forMirroringMembers(const Call& call) {
        if (!m_mirrorNpcs || m_played->kind != SceneKind::World || !m_played->scene) {
            return;
        }
        const auto& playedWorld = static_cast<const WorldScene&>(*m_played->scene);
        const Teams& teams = playedWorld.teams();
        const auto team = teams.leaderOf(playedWorld.ownId());
        if (!team) {
            return;
        }
        const int playedMap = playedWorld.ownMapId();
        for (const auto& owned : m_members) {
            Member& member = *owned;
            if (!member.world || member.leavingSinceMs) {
                continue;
            }
            WorldScene& world = member.world->get();
            if (world.inWorld() && world.ownMapId() == playedMap && teams.leaderOf(world.ownId()) == team) {
                call(member, world);
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

    void PartyMembers::logInGone() {
        if (!m_started) {
            return;
        }
        for (int index = size() - 1; index >= 0; --index) {
            const Member& member = *m_members[static_cast<std::size_t>(index)];
            if (!member.goneReason || member.leavingSinceMs) {
                continue;
            }
            if (member.account.account == played().session.loginName) {
                remove(index);
                continue;
            }
            // a fresh member: no session, connection, timers or heard lines of the gone one are kept
            auto fresh = std::make_unique<Member>(GameTables::of(played()), m_silent, member.account);
            m_members[static_cast<std::size_t>(index)] = std::move(fresh);
            setUpMember(*m_members[static_cast<std::size_t>(index)]);
            logIn(*m_members[static_cast<std::size_t>(index)]);
        }
    }

    void PartyMembers::logIn(Member& member) {
        const AccountSession& own = played().session;
        AccountSession& session = member.bundle->session;
        session.serverAddress = own.serverAddress;
        session.serverPort = own.serverPort;
        session.serverLineName = own.serverLineName;
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
        core::logInfo("party member ", member.account.account, ": logging in to ", own.serverAddress, " (character ", member.account.slot, ")");
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
        if (played().videoSettings().partyLogin == ExtraSettings::PartyLogin::Simple && heldRows() >= kSimpleMembers) {
            return "At most " + std::to_string(kSimpleMembers + 1) + " characters can be logged in (Advanced multi-login takes more).";
        }
        m_members.push_back(std::make_unique<Member>(GameTables::of(played()), m_silent, std::move(account)));
        setUpMember(*m_members.back());
        if (m_started) {
            logIn(*m_members.back());
        }
        return {};
    }

    bool PartyMembers::allInWorld() const {
        return std::all_of(m_members.begin(), m_members.end(), [](const auto& member) {
            return member->world && member->world->get().inWorld() && !member->leavingSinceMs && !member->goneReason;
        });
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
        const auto leader = member.world->get().teams().leaderOf(member.world->get().ownId());
        if (leader && member.bundle->connection.isOpen()) { // a dead connection would hold the send until kLeaveWaitMs
            member.bundle->connection.send(protocol::leaveTeamRequest(*leader));
        }
        member.leavingSinceMs = m_nowMs;
        member.leavingReason = reason;
        core::logInfo("party member ", member.account.account, ": ", reason);
    }

    void PartyMembers::select(int index) {
        if (index < 0 || index >= size()) {
            return;
        }
        const Member& member = *m_members[static_cast<std::size_t>(index)];
        if (!member.world || member.leavingSinceMs || !member.world->get().inWorld() || m_played->kind != SceneKind::World) {
            return;
        }
        m_pendingSelect = index;
    }

    bool PartyMembers::applySelection() {
        const int index = std::exchange(m_pendingSelect, -1);
        if (index < 0 || index >= size()) {
            return false;
        }
        const Member& member = *m_members[static_cast<std::size_t>(index)];
        // checked again: the member may have logged out, or the played account left the world, since the click
        if (!member.world || member.leavingSinceMs || !member.world->get().inWorld() || m_played->kind != SceneKind::World || !m_played->scene) {
            return false;
        }
        swapPlayed(index);
        return true;
    }

    void PartyMembers::swapPlayed(int index) {
        Member& member = *m_members[static_cast<std::size_t>(index)];
        AccountBundle& old = *m_played;
        // each character keeps its own track (it may be on another map) and setDevice carries it over: the newly played one's is
        // heard from where its silent player had it, the other's goes on remembered on the silent device; the old one leaves the
        // shared music track first
        old.sounds.setDevice(m_silent);
        old.music.setDevice(m_silent);
        member.bundle->sounds.setDevice(m_device);
        member.bundle->music.setDevice(m_device);
        const PartyAccount oldAccount{old.session.loginName, old.session.password, old.session.playingSlot == 2 ? 2 : 1};
        core::logInfo("party: ", member.account.account, " is selected, ", oldAccount.account, " takes its row");
        std::swap(m_played, member.bundle);
        std::swap(m_playedFollowing, member.following);
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
        setUpPlayed(played());
        setUpMember(member);
        // the old played character's party forms answer through hooks it no longer has
        if (member.world) {
            member.world->get().closePartyForms();
            member.world->get().partyHooksChanged();
        }
        static_cast<WorldScene&>(*m_played->scene).partyHooksChanged();
        m_played->scene->screen().setLayoutFilter({});
        if (member.bundle->scene) {
            member.bundle->scene->screen().setLayoutFilter({});
        }
        m_commanding = -1;
        m_mouseOwner = -1;
        m_keyboardOwner = -1;
        m_routedKeys.clear();
        m_sentLines.clear();
    }

    void PartyMembers::remove(int index) {
        m_members.erase(m_members.begin() + index);
        // four member rows to renumber: the one asked to be selected, commanding, mouse owner, keyboard owner
        for (int& row : std::array<std::reference_wrapper<int>, 4>{m_pendingSelect, m_commanding, m_mouseOwner, m_keyboardOwner}) {
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
        member.windowLayouts.clear(); // the old screen's layouts go with it
        member.following.wasInTeam = false; // a new world scene starts without its team data
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
        screen.setHudScale(hudScale);
        screen.setLayoutFilter([&member](const ui::Element& layout) {
            auto known = member.windowLayouts.find(&layout);
            if (known == member.windowLayouts.end()) {
                // popups are always kept, so they skip the window test
                const std::uint8_t kind = isPopupLayout(layout) ? kLayoutPopup : holdsWindow(layout) ? kLayoutWindow : 0;
                known = member.windowLayouts.emplace(&layout, kind).first;
            }
            return known->second != 0;
        });
        call(screen);
        screen.setLayoutFilter({});
    }

    bool PartyMembers::playedPopupAt(core::Point point) {
        if (!m_played->scene) {
            return false;
        }
        ui::Screen& screen = m_played->scene->screen();
        const std::vector<const ui::Element*> over = screen.layoutsFrom(isPopupLayout);
        if (over.empty()) {
            return false;
        }
        // the layouts holdPlayedPopups holds back: the topmost layouts of the screen, so a hit there is the screen's own topmost one
        screen.setLayoutFilter([&over](const ui::Element& layout) { return listed(over, layout); });
        const bool caught = screen.catchesAt(point);
        screen.setLayoutFilter({});
        return caught;
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

    void PartyMembers::holdPlayedPopups() {
        const bool memberShown = std::any_of(m_members.begin(), m_members.end(), [](const auto& member) { return member->world.has_value(); });
        if (!memberShown || !m_played->scene) {
            return;
        }
        ui::Screen& screen = m_played->scene->screen();
        // from the lowest popup up, so that a window raised over the popups (bringAbovePopups) stays over them: drawn in the
        // order the screen's hit test takes them
        m_heldLayouts = screen.layoutsFrom(isPopupLayout);
        if (!m_heldLayouts.empty()) {
            screen.setLayoutFilter([this](const ui::Element& layout) { return !listed(m_heldLayouts, layout); }, false);
        }
    }

    void PartyMembers::drawWindows(gfx::Canvas& canvas, std::uint64_t nowMs, float hudScale) {
        const std::vector<const ui::Element*> held = std::exchange(m_heldLayouts, {});
        if (!held.empty() && m_played->scene) {
            m_played->scene->screen().setLayoutFilter({});
        }
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
        if (!held.empty() && m_played->scene) {
            // the selected character's popups (and the windows raised over them) stay over the other characters' windows; only
            // these layouts are drawn again (a drag picture or game pad frame of the screen is drawn by both passes, at the same place)
            ui::Screen& screen = m_played->scene->screen();
            screen.setLayoutFilter([&held](const ui::Element& layout) { return listed(held, layout); }, false);
            screen.draw(canvas, nowMs);
            screen.setLayoutFilter({});
        }
    }

    int PartyMembers::heldRows() const {
        // a gone member's row goes when another account is added; a leaving one's stays until it has left (kLeaveWaitMs at most)
        return static_cast<int>(std::count_if(m_members.begin(), m_members.end(), [](const auto& member) {
            return !member->goneReason || member->leavingSinceMs;
        }));
    }

    bool PartyMembers::loginRowShown() const {
        const ExtraSettings::PartyLogin mode = played().videoSettings().partyLogin;
        if (mode == ExtraSettings::PartyLogin::Off || m_played->kind != SceneKind::World || !m_played->scene) {
            return false;
        }
        return static_cast<const WorldScene&>(*m_played->scene).inWorld() && (mode == ExtraSettings::PartyLogin::Advanced || heldRows() < kSimpleMembers);
    }

    bool PartyMembers::simpleRows() const {
        // off too (members of an --accounts file still show): the rows as they were before Advanced
        return played().videoSettings().partyLogin != ExtraSettings::PartyLogin::Advanced && heldRows() <= kSimpleMembers;
    }

    void PartyMembers::drawPanels(gfx::Canvas& canvas, float hudScale, const WorldScene& playedWorld, const ui::Screen& shown, std::uint64_t nowMs) {
        const bool loginRow = loginRowShown();
        if (m_members.empty() && !loginRow) {
            return;
        }
        // a window of the shown screen (a shop, the globe, mail) or what one of its popups takes the mouse on (the talk box, the
        // maid's menu, an item or pet info box) under a row keeps that row out of sight and its clicks
        const auto covered = [&shown](const core::Rect& box) { return shown.windowOver(box, isPopupLayout); };
        if (simpleRows()) {
            m_simplePanels->draw(
                canvas,
                views(),
                loginRow,
                playedWorld.inBattle(),
                playedWorld.vehicleGaugeShown(),
                m_played->scene->screen().view(),
                hudScale,
                m_mouse,
                nowMs,
                covered
            );
            return;
        }
        std::vector<PartyMemberView> rows = views();
        if (playedWorld.inWorld()) {
            // the selected character's own row, after the members' (index size()): first, in its team's group when other own
            // characters are in it, so that a team of own characters looks the same whichever of them is selected
            PartyMemberView played;
            const PlayerStatus& status = playedWorld.ownStatus();
            played.played = true;
            played.account = m_played->session.loginName;
            played.state = playedWorld.inBattle() ? PartyMemberView::State::InBattle : PartyMemberView::State::InWorld;
            played.name = playedWorld.ownName();
            played.id = playedWorld.ownId();
            played.element = status.element;
            played.job = status.job;
            played.look = playedWorld.ownLook();
            played.level = status.level;
            played.hp = status.hp;
            played.maxHp = status.maxHp;
            played.sp = status.sp;
            played.maxSp = status.maxSp;
            const auto team = playedWorld.teams().leaderOf(played.id);
            played.inTeam = team.has_value();
            played.leaderId = team.value_or(0);
            rows.push_back(std::move(played));
        }
        m_panels->draw(
            canvas,
            rows,
            loginRow,
            playedWorld.inBattle(),
            playedWorld.vehicleGaugeShown(),
            m_played->scene->screen().view(),
            hudScale,
            m_mouse,
            nowMs,
            covered
        );
    }

    bool PartyMembers::handleRowButton(const SDL_Event& event, core::Point origin) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            m_panels->release();
            // only the release of the button pressed on the panel is taken: the other button's goes on to the screen
            if (m_panelButton == 0 || event.button.button != m_panelButton) {
                return false;
            }
            m_panelButton = 0;
            return true;
        }
        // the rows are drawn in the world only: elsewhere (the login screen after a lost server) their last places catch nothing
        if (m_played->kind != SceneKind::World || !m_played->scene) {
            return false;
        }
        const core::Point point{static_cast<int>(event.button.x) - origin.x, static_cast<int>(event.button.y) - origin.y};
        const bool left = event.button.button == SDL_BUTTON_LEFT;
        WorldScene& playedWorld = static_cast<WorldScene&>(*m_played->scene);
        const bool simple = simpleRows();
        if ((simple ? m_simplePanels->loginRowAt(point) : m_panels->loginRowAt(point)) && loginRowShown()) {
            m_panelButton = event.button.button;
            if (left) {
                if (!simple) {
                    played().playSound("sound/wav0151.wav"); // the Login button clicks as every button does (ImageButton's wav0151)
                }
                playedWorld.openPartyLogin();
            }
            return true;
        }
        const int pressed = simple ? m_simplePanels->memberAt(point) : m_panels->memberAt(point);
        if (pressed == size()) {
            // the selected character's own row (in its team's group): selected already, nothing to do
            m_panelButton = event.button.button;
            return true;
        }
        if (pressed < 0 || pressed >= size()) {
            // off the rows: the panel's arrows and scroll bar, or only taken on the panel (Advanced)
            if (!simple && m_panels->press(point, left)) {
                m_panelButton = event.button.button;
                return true;
            }
            return false;
        }
        m_panelButton = event.button.button;
        const Member& member = *m_members[static_cast<std::size_t>(pressed)];
        if (member.goneReason && !member.leavingSinceMs) {
            // a gone member's row has nothing to select or log out: a click clears it away
            core::logInfo("party member ", member.account.account, ": row cleared");
            remove(pressed);
            return true;
        }
        if (left && (SDL_GetModState() & SDL_KMOD_CTRL) != 0) {
            moveLastWindowTo(pressed);
        } else if (left && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0) {
            moveWindowsAndSelect(pressed);
        } else if (left && (SDL_GetModState() & SDL_KMOD_ALT) != 0) {
            leadAndSelect(pressed);
        } else if (left) {
            select(pressed);
        } else if (member.world && !member.leavingSinceMs && member.world->get().inWorld()) {
            playedWorld.askPartyLogOut(member.world->get().ownId(), member.world->get().ownName());
        }
        return true;
    }

    void PartyMembers::moveLastWindowTo(int index) {
        const Member& target = *m_members[static_cast<std::size_t>(index)];
        if (!target.world || target.leavingSinceMs || !target.world->get().inWorld()) {
            return;
        }
        WorldScene* owner = nullptr;
        std::optional<WorldScene::MovableWindow> last;
        const auto consider = [&](WorldScene* world) {
            if (world == nullptr || !world->inWorld()) {
                return;
            }
            if (const auto used = world->lastUsedWindow(); used && (!last || used->usedAtMs > last->usedAtMs)) {
                last = used;
                owner = world;
            }
        };
        consider(worldOf(played()));
        for (const auto& member : m_members) {
            if (member->world && !member->leavingSinceMs) {
                consider(&member->world->get());
            }
        }
        WorldScene& targetWorld = target.world->get();
        if (!last || owner == &targetWorld) {
            return;
        }
        // opened on the clicked character first: a window that can't open there stays where it is
        if (!targetWorld.openMovableWindow(last->key, last->position)) {
            core::logInfo("party: the ", last->key, " window cannot open for ", targetWorld.ownName(), " now");
            return;
        }
        owner->closeMovableWindow(last->key);
        core::logInfo("party: the ", last->key, " window of ", owner->ownName(), " moved to ", targetWorld.ownName());
    }

    void PartyMembers::moveWindowsAndSelect(int index) {
        const Member& target = *m_members[static_cast<std::size_t>(index)];
        WorldScene* source = worldOf(played());
        if (source != nullptr && source->inWorld() && target.world && !target.leavingSinceMs && target.world->get().inWorld()) {
            WorldScene& targetWorld = target.world->get();
            if (source != &targetWorld) {
                for (const WorldScene::MovableWindow& window : source->openWindows()) {
                    // opened on the clicked character first: a window that can't open there stays where it is
                    if (!targetWorld.openMovableWindow(window.key, window.position)) {
                        core::logInfo("party: the ", window.key, " window cannot open for ", targetWorld.ownName(), " now");
                        continue;
                    }
                    source->closeMovableWindow(window.key);
                    core::logInfo("party: the ", window.key, " window of ", source->ownName(), " moved to ", targetWorld.ownName());
                }
            }
        }
        select(index);
    }

    void PartyMembers::leadAndSelect(int index) {
        const Member& target = *m_members[static_cast<std::size_t>(index)];
        if (target.world && !target.leavingSinceMs && target.world->get().inWorld()) {
            const WorldScene& targetWorld = target.world->get();
            const std::int32_t targetId = targetWorld.ownId();
            const auto leaderId = targetWorld.teams().leaderOf(targetId);
            if (leaderId && *leaderId != targetId) {
                if (WorldScene* leader = ownWorld(*leaderId)) {
                    core::logInfo("party: ", leader->ownName(), " offers the lead to ", targetWorld.ownName());
                    leader->offerLeadTo(targetId);
                } else {
                    core::logInfo("party: ", targetWorld.ownName(), " is led by another player's character: only selected");
                }
            }
        }
        select(index);
    }

    bool PartyMembers::handleEvent(const SDL_Event& event, core::Point origin, float hudScale, bool memberShown) {
        if (!m_started) {
            return false;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            m_mouse = core::Point{static_cast<int>(event.motion.x) - origin.x, static_cast<int>(event.motion.y) - origin.y};
            if (!simpleRows() && m_panels->dragging()) {
                m_panels->drag(*m_mouse); // the panel's scroll bar thumb
                return true;
            }
        }
        if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
            (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) && handleRowButton(event, origin)) {
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL && m_played->kind == SceneKind::World && m_played->scene) {
            // the wheel over the panel scrolls its rows (when some are out of sight), before the windows under it get it
            const core::Point point{static_cast<int>(event.wheel.mouse_x) - origin.x, static_cast<int>(event.wheel.mouse_y) - origin.y};
            if (!simpleRows() && m_panels->panelAt(point)) {
                if (event.wheel.y > 0) {
                    m_panels->scrollBy(-1);
                } else if (event.wheel.y < 0) {
                    m_panels->scrollBy(1);
                }
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
            const core::Point point{static_cast<int>(x) - origin.x, static_cast<int>(y) - origin.y};
            // the played account's popups are drawn over the members' windows and take the mouse first
            const int index = playedPopupAt(point) ? -1 : memberAt(point, hudScale);
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
            if (usable(m_keyboardOwner)) {
                forwardTo(m_keyboardOwner);
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
        return false;
    }

    bool PartyMembers::inShownBattle(const WorldScene& world) const {
        if (m_played->kind != SceneKind::World || !m_played->scene) {
            return false;
        }
        return static_cast<const WorldScene&>(*m_played->scene).fightsWith(world.ownId());
    }

    std::optional<std::reference_wrapper<Scene>> PartyMembers::commandingScene(bool playedDone) {
        int found = -1;
        if (playedDone) {
            for (int index = 0; index < size(); ++index) {
                const Member& member = *m_members[static_cast<std::size_t>(index)];
                if (member.world && !member.leavingSinceMs && member.world->get().battleActor() != 0 && inShownBattle(member.world->get())) {
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

    void PartyMembers::watchTeam(Following& following, const WorldScene& world, const std::string& name) {
        if (world.teams().inTeam(world.ownId())) {
            following.wasInTeam = true;
        } else if (std::exchange(following.wasInTeam, false) && following.on) {
            following.on = false; // left, driven away or disbanded: never invited again
            core::logInfo("party member ", name, ": out of its team, no longer follows");
        }
    }

    void PartyMembers::inviteToTeam(Member& member, const WorldScene& playedWorld, std::uint64_t nowMs) {
        const WorldScene& world = member.world->get();
        const std::int32_t memberId = world.ownId();
        if (!member.following.on || world.teams().inTeam(memberId) || world.ownMapId() != playedWorld.ownMapId() || world.inBattle() ||
            playedWorld.inBattle()) {
            member.invites = 0; // a later time out of a team on the map gets its invites again
            return;
        }
        if (member.invites >= kMostInvites || (member.invites > 0 && nowMs - member.invitedMs < kInviteGapMs)) {
            return;
        }
        const std::int32_t playedId = playedWorld.ownId();
        const Teams& teams = playedWorld.teams();
        // the selected character's team is joined: the invitation comes from its leader, the selected character itself or another own one
        const std::int32_t leaderId = teams.leaderOf(playedId).value_or(playedId);
        if (teams.members(leaderId).size() >= Teams::kMostMembers) {
            return; // the team is full
        }
        net::GameConnection* inviter = &played().connection;
        if (leaderId != playedId) {
            const auto leader = std::find_if(m_members.begin(), m_members.end(), [leaderId](const auto& other) {
                return other->world && !other->leavingSinceMs && other->world->get().inWorld() && other->world->get().ownId() == leaderId;
            });
            if (leader == m_members.end()) {
                return; // led by someone else's character
            }
            inviter = &(*leader)->bundle->connection;
        }
        inviter->send(protocol::inviteRequest(memberId));
        ++member.invites;
        member.invitedMs = nowMs;
        core::logInfo("party member ", member.account.account, ": invited to the selected character's team (", member.invites, ")");
    }

    void PartyMembers::update(std::uint64_t nowMs, std::optional<std::reference_wrapper<const WorldScene>> playedWorld) {
        const bool playedInWorld = playedWorld && playedWorld->get().inWorld();
        if (playedInWorld) {
            watchTeam(m_playedFollowing, playedWorld->get(), played().session.loginName);
        }
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
            if (member.bundle->scene->cutOff()) {
                // kicked (0), locked, logged in elsewhere, or the server closed: nobody clicks the unseen disconnect form
                gone(member, member.bundle->kind == SceneKind::Login ? "could not log in" : "disconnected");
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
                if (!member.leavingSinceMs && member.world->get().inWorld()) {
                    watchTeam(member.following, member.world->get(), member.account.account);
                    if (playedInWorld && played().session.ownPlayer) {
                        inviteToTeam(member, playedWorld->get(), nowMs);
                    }
                }
            }
        }
    }

    void PartyMembers::setNextBeat(std::uint64_t nextBeatMs) {
        for (const auto& member : m_members) {
            member->bundle->services.nextBeatMs = nextBeatMs;
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
            view.commanding = static_cast<int>(views.size()) == m_commanding;
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
                const AccountSession& session = m_played->session;
                const auto team = world.teams().leaderOf(world.ownId());
                view.inTeam = team && session.ownPlayer && world.teams().leaderOf(session.ownPlayer->playerId) == team;
                view.leaderId = team.value_or(0);
                // its commands are due in a battle other than the shown one (or the selected character fights none): its row blinks until they are given
                view.attention = world.battleActor() != 0 && !inShownBattle(world);
            }
            views.push_back(std::move(view));
        }
        return views;
    }

}
