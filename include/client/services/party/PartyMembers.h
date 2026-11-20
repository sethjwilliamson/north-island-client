#ifndef NORTH_ISLAND_CLIENT_SERVICES_PARTY_PARTY_MEMBERS_H
#define NORTH_ISLAND_CLIENT_SERVICES_PARTY_PARTY_MEMBERS_H

#include "audio/AudioDevice.h"
#include "client/models/character/CharacterLook.h"
#include "client/services/party/AccountBundle.h"
#include "client/services/GameServices.h"
#include "gfx/Canvas.h"
#include "ui/Screen.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace client {

    /// @brief A party member's account: a line of the --accounts file.
    struct PartyAccount {
        std::string account;
        std::string password;
        int slot = 1;

        /// @brief Compares all fields of two accounts.
        /// @param other The account to compare with.
        /// @return True when account, password and slot all match.
        bool operator==(const PartyAccount& other) const { return account == other.account && password == other.password && slot == other.slot; }
    };

    /// @brief Parses the --accounts file: "account password [place]" a line; blank and # lines are skipped; any number of accounts.
    /// @details An "auto" or "manual" word after the place (older files) is accepted and ignored: members always take manual battle commands.
    /// @param lines The lines of the file.
    /// @param error Receives what is wrong with the file (nothing is returned then).
    /// @return The accounts, or nothing when a line is invalid.
    std::optional<std::vector<PartyAccount>> parsePartyAccounts(const std::vector<std::string>& lines, std::string& error);

    /// @brief What the party panels show of a member.
    struct PartyMemberView {
        /// @brief Where the member is in its login and play.
        enum class State { LoggingIn, InWorld, InBattle, Gone };
        std::string account;
        std::string name;
        std::int32_t id = 0;
        State state = State::LoggingIn;
        int level = 0;
        int hp = 0;
        int maxHp = 0;
        int sp = 0;
        int maxSp = 0;
        int element = 0;
        int job = 0;
        std::optional<CharacterLook> look;
        std::string reason;
        bool inTeam = false; // in the selected character's team
        std::int32_t leaderId = 0; // its team's leader (its own world's Teams), 0 while in no team
        bool played = false; // the selected character itself: its row shows first (in its team's group with other own characters)
        bool commanding = false; // its battle commands are given now, in the played account's place
        bool attention = false; // its battle commands are due in a battle other than the shown one, or while the selected character fights none (a blinking outline)
    };

    /// @brief A whisper or team line another own character heard, for the selected character's chat window.
    struct PartyChatLine {
        int channel = 0;
        std::string line;
        std::string member;
    };

    /// @brief A member's message for the played account's top box (centre: its middle one).
    struct PartyMessage {
        bool centre = false;
        std::string text;
        int durationMs = 0;
    };

    class PartyPanels;
    class SimplePartyPanels;
    class Scene;
    class WorldScene;

    /// @brief The qol client's several own characters at once (at most kSimpleMembers + 1 in Simple multi-login, any number in
    /// Advanced): the selected one is the played account (drawn, steered,
    /// with the sound and its HUD), the others ("members") run their login and world scenes unseen and show as the party rows.
    /// @details There is no leader on the client side: team leadership is only the server's (WorldScene::teams()). The first account of
    /// --accounts is only the one shown at start; a click on a member's row swaps it with the played account (select), in the world
    /// or in battle; a right click asks to log it out, and the login button under the rows logs another account in. A member follows
    /// from its login: while it is not in a team it is invited into the selected character's team (13/7, by that team's leader when it
    /// is an own character) when on its map, until it has been in a team and left it. Own characters accept each other's team
    /// invitations and lead. A ground click of a character in a team led by another own character walks that leader (walkToLeader); a
    /// Ctrl+click on a row moves the last used window to that member (moveLastWindowTo); a Shift+click moves all the selected
    /// character's open windows to it and selects it (moveWindowsAndSelect); an Alt+click makes it its team's leader and
    /// selects it (leadAndSelect). Also covers manual battle turns shown in the
    /// played account's place (a member due in another battle asks for attention on its row), the members' windows over the screen,
    /// the whisper and team chat relay, the NPC mirror and the services' log in/out hooks (WorldScene::openPartyLogin, askPartyLogOut).
    class PartyMembers {
        public:
            static constexpr int kSimpleMembers = 3; // most members (extra accounts) in Simple multi-login: 4 characters with the played one

            /// @brief Sets up the members' accounts (not yet logged in).
            /// @param played The played account's bundle, which the members share tables and settings of and which a member's swaps with when it is selected.
            /// @param device The open sound device (the played account's).
            /// @param setUpPlayed What the Application gives the played account's services (message box, screenshots, Video tab); applied again for a member that is selected.
            /// @param accounts The members' accounts.
            PartyMembers(
                std::unique_ptr<AccountBundle>& played,
                const audio::AudioDevice& device,
                std::function<void(GameServices&)> setUpPlayed,
                std::vector<PartyAccount> accounts
            );

            /// @brief Removes the party hooks from the played account's services.
            ~PartyMembers();
            PartyMembers(const PartyMembers&) = delete;
            PartyMembers& operator=(const PartyMembers&) = delete;

            /// @brief Tells whether there are no members.
            /// @return True when no member is in the list.
            bool empty() const { return m_members.empty(); }

            /// @brief Counts the members (rows), including those logging out or gone.
            /// @return The number of members.
            int size() const { return static_cast<int>(m_members.size()); }

            /// @brief Tells whether every member has entered the world (test scripts wait for it).
            /// @return True when all are in the world, also with no members.
            bool allInWorld() const;

            /// @brief Finds the member whose battle commands are given now, shown and steered in the played account's place: the first
            /// member in the played account's own battle whose commands are due, once the played account's are given.
            /// @details A member whose commands are due in another battle is not shown: its row asks for attention (PartyMemberView::attention).
            /// @param playedDone The played account is in a fight and its own fighters have their commands.
            /// @return The member's scene, or nothing for the played account.
            std::optional<std::reference_wrapper<Scene>> commandingScene(bool playedDone);

            /// @brief Asks to make a member the played account (the selected character); the swap waits for applySelection.
            /// @details Only a member in the world (not logging out) while the played account is in the world can be selected; either
            /// may be in a battle (a battle keeps a character in the world: WorldScene::inWorld).
            /// @param index The member's row.
            void select(int index);

            /// @brief Swaps in the member asked for by select (a row click): it becomes the played account and the account left takes its
            /// row. Call it where no Scene reference of the played account is held (the top of a frame).
            /// @return True when the played account changed (its view and HUD scale want setting again).
            bool applySelection();

            /// @brief Takes the whispers and team lines the members heard a moment ago (the played account's own copy of a shared line has come by then), each once.
            /// @param nowMs The current time in milliseconds.
            /// @return The lines for the played account's chat window.
            std::vector<PartyChatLine> takeChatLines(std::uint64_t nowMs);

            /// @brief Takes the members' messages since the last call, for the played account's boxes (PlayerMessages::showGrouped).
            /// @return The messages.
            std::vector<PartyMessage> takeMessages() { return std::exchange(m_messages, {}); }

            /// @brief Keeps the played account's popups (talk box, messages, menus, item and pet info) out of its screen's coming draw
            /// while a member is in the world, so that drawWindows draws them over the members' windows; call it just before the played
            /// scene is drawn, and drawWindows after it. Every layout from the lowest popup up is held, so a window raised over the
            /// popups (ui::UiContext::bringAbovePopups: a count box, a shop) stays over them, as the screen's hit test takes it.
            /// @details Each pass draws the screen's hover only when the hovered element lies in a layout it keeps (the element under a
            /// hovered popup is not highlighted, nor is its hint drawn).
            void holdPlayedPopups();

            /// @brief Draws the members' windows and popups over the played account's screen, each tagged with the member's name, then
            /// the played account's popups held by holdPlayedPopups over them.
            /// @param canvas The canvas, at the game screen's origin.
            /// @param nowMs The current time in milliseconds.
            /// @param hudScale The HUD scale.
            void drawWindows(gfx::Canvas& canvas, std::uint64_t nowMs, float hudScale);

            /// @brief Draws the members' rows under the own status, and the login button while another account can log in (PartyPanels).
            /// @param canvas The canvas, at the game screen's origin.
            /// @param hudScale The HUD scale.
            /// @param playedWorld The played account's world scene (in a battle the rows go higher).
            /// @param shown The shown screen (the played account's, or a member's giving its battle commands): a row under one of its
            /// windows or popups' boxes (kPopupLayouts) is left out, not drawn and not hit.
            /// @param nowMs The current time in milliseconds.
            void drawPanels(gfx::Canvas& canvas, float hudScale, const WorldScene& playedWorld, const ui::Screen& shown, std::uint64_t nowMs);

            /// @brief Handles an input event for the members: a click on a member's row (select; with Ctrl, the last used window moves to
            /// it: moveLastWindowTo; with Alt, it leads its team and is selected: leadAndSelect), on the login button (WorldScene::openPartyLogin),
            /// a right click on a member's row (WorldScene::askPartyLogOut; a gone member's row goes), the mouse over a member's window,
            /// typing into a member's text box.
            /// @details Both buttons' presses and releases on the rows are taken, so nothing under the rows (a right-click menu) sees them.
            /// The played account's popups, drawn over the members' windows, catch the mouse before them (the event is left to the played screen).
            /// @param event The SDL event.
            /// @param origin The game screen's place in the window.
            /// @param hudScale The HUD scale.
            /// @param memberShown A member's whole scene is shown (its battle commands), which takes the input itself.
            /// @return True when a member took the event.
            bool handleEvent(const SDL_Event& event, core::Point origin, float hudScale, bool memberShown);

            /// @brief Tells whether the members have been logged in.
            /// @return True once start() has run.
            bool started() const { return m_started; }

            /// @brief Logs every member in to the server the played account is on (once it is in the world); one added later logs in at once.
            void start();

            /// @brief Logs the gone members (cut off from the server, could not log in) in again once the played account enters the world anew.
            /// @details A gone member on the played account's own account now is dropped instead: logging it in would push the played account off.
            void logInGone();

            /// @brief Tells that the played account's scene was replaced (a login, or logged in again): its new world scene starts
            /// without team data, so its following's team watch starts over.
            void playedSceneChanged() { m_playedFollowing.wasInTeam = false; }

            /// @brief Adds another member (the login form of WorldScene::openPartyLogin), refused in Simple multi-login while
            /// kSimpleMembers hold a row (heldRows: members logging out count until they have left); rows of members that could not
            /// log in go.
            /// @param account The account to log in.
            /// @return Empty, or why the account cannot be added.
            std::string add(PartyAccount account);

            /// @brief Tells whether a member plays this character (not one logging out).
            /// @param playerId The character's id.
            /// @return True when a member plays it.
            bool isMember(std::int32_t playerId) const;

            /// @brief Makes that member leave its team and log out; its row goes once the leave has been sent.
            /// @param playerId The member's character id.
            void logOut(std::int32_t playerId);

            /// @brief Updates every member once a beat: its scenes, its following (team invites) and its view.
            /// @param nowMs The current time in milliseconds.
            /// @param playedWorld The played account's world scene (nothing: not in the world).
            void update(std::uint64_t nowMs, std::optional<std::reference_wrapper<const WorldScene>> playedWorld);

            /// @brief Moves the members' scenes before every frame.
            /// @param nowMs The current time in milliseconds.
            void move(std::uint64_t nowMs);

            /// @brief Gives the members the next beat's time once the played account's beat has moved on.
            /// @details update runs before the beat clock moves, so the time it copies is the beat now running: the
            /// battle tween of a member's scene (shown while it takes its commands) then never tweened between beats.
            /// @param nextBeatMs The next beat, ms.
            void setNextBeat(std::uint64_t nextBeatMs);

            /// @brief Describes every member for the party panels.
            /// @return One view per member.
            std::vector<PartyMemberView> views() const;

        private:
            static constexpr int kMostInvites = 3; // most team invites a member gets before the selected character stops asking
            static constexpr std::uint64_t kInviteGapMs = 5000; // ms between two invites
            static constexpr std::uint64_t kChatDelayMs = 1500; // ms a chat line a member heard waits before it is relayed to the played account
            static constexpr std::uint64_t kChatRepeatMs = 5000; // ms a sent line is remembered against repeats
            static constexpr int kWhisperChannel = 3; // chat channel 3: whisper
            static constexpr int kTeamChannel = 5; // chat channel 5: team

            static constexpr std::uint64_t kLeaveWaitMs = 2000; // ms a logging-out member waits for its sends to finish before it is removed

            /// @brief A character's following of the selected one, which goes with the character when it is swapped.
            struct Following {
                bool on = true; // still invited to the selected character's team when out of a team (on at login)
                bool wasInTeam = false; // has been in a team since it entered the world: leaving it turns the following off
            };

            struct Member;

            /// @brief Gives the played account's services.
            /// @return The services.
            GameServices& played() { return m_played->services; }

            /// @brief Gives the played account's services (read only).
            /// @return The services.
            const GameServices& played() const { return m_played->services; }

            /// @brief Sets the party hooks of the played account's services.
            /// @param services The played account's services.
            void setUpPlayed(GameServices& services);

            /// @brief Sets the hooks of a member's services.
            /// @param member The member.
            void setUpMember(Member& member);

            /// @brief Makes that member the played account and the played account that member, in its row (the sound, the hooks and the following change hands).
            /// @param index The member's row.
            void swapPlayed(int index);

            /// @brief Counts the members whose rows stay when another account is added: logged in, logging in or logging out (a
            /// leaving member's row stays until it has left); only a gone member's row goes.
            /// @return The number of members.
            int heldRows() const;

            /// @brief Tells whether the login button (Advanced) or the "+" row (Simple) shows: multi-login on, the played account in
            /// the world and, in Simple, fewer than kSimpleMembers members holding a row (heldRows).
            /// @return True when it shows.
            bool loginRowShown() const;

            /// @brief Takes a press or release of the mouse button on the rows: on a press, acts on the row under it (the left
            /// button selects or opens the login form, with Ctrl held moves the last used window to it, with Shift held moves all the
            /// selected character's open windows to it and selects it, with Alt held makes it lead and
            /// selects it; the right one asks to log out);
            /// the release of the button whose press was taken is taken too.
            /// @param event The SDL mouse button event.
            /// @param origin The game screen's place in the window.
            /// @return True when the event was taken.
            bool handleRowButton(const SDL_Event& event, core::Point origin);

            /// @brief Moves the movable window used last by any own character in the world (WorldScene::lastUsedWindow) to a member, at
            /// the same place on the screen: it opens there (or moves to the front of its own one), then closes for its owner. Nothing
            /// happens when the member owns it already, or when it can't open for the member (the owner keeps it). The selected character stays.
            /// @param index The member's row.
            void moveLastWindowTo(int index);

            /// @brief Moves all the selected character's open movable windows (WorldScene::openWindows) to a member, each at the same
            /// place on the screen, then selects the member. A window that can't open for the member stays with the selected character.
            /// @param index The member's row.
            void moveWindowsAndSelect(int index);

            /// @brief Makes a member its team's leader and selects it: the team's leader, when it is an own character, offers it the lead
            /// (WorldScene::offerLeadTo), which own characters take unasked. A member that leads already, is in no team or is led by
            /// another player's character is only selected.
            /// @param index The member's row.
            void leadAndSelect(int index);

            /// @brief Finds the world scene of a character's services: the played account's or a member's.
            /// @param services The character's services.
            /// @return Its world scene, or null when it is not in a world scene.
            WorldScene* worldOf(const GameServices& services);

            /// @brief Finds the world scene of an own character in the world (the played account, or a member not logging out).
            /// @param playerId The character's id.
            /// @return Its world scene, or null for another player's character.
            WorldScene* ownWorld(std::int32_t playerId);

            /// @brief A character's ground click (GameServices::walkElsewhere): when it is in a team led by another own character on its
            /// map, that leader walks there (its team follows) instead.
            /// @param services The clicking character's services.
            /// @param mapPoint The map point clicked.
            /// @return True when the leader walks there; false when the character walks itself (no team, its leader, led by someone
            /// else's character or from another map, or the leader can't walk now).
            bool walkToLeader(const GameServices& services, core::Point mapPoint);

            /// @brief Tells whether a character fights in the battle shown: the played account's.
            /// @param world The character's world scene.
            /// @return True when the played account is in a battle and the character is one of its fighters.
            bool inShownBattle(const WorldScene& world) const;

            /// @brief Tells whether a character is one of this client's own (the played account or a member in the world, not logging out).
            /// @param playerId The character's id.
            /// @return True for an own character.
            bool isOwnCharacter(std::int32_t playerId) const;

            /// @brief Starts a member's login scene on the played account's server.
            /// @param member The member.
            void logIn(Member& member);

            /// @brief Sends the member's leave of the team, after which it is removed.
            /// @param member The member.
            /// @param reason What its row says meanwhile.
            void beginLogOut(Member& member, const std::string& reason);

            /// @brief Drops a member's row (its connection closes) and moves the rows after it up.
            /// @param index The member's row.
            void remove(int index);

            /// @brief Switches a member to the scene its current scene asked for.
            /// @param member The member.
            void changeScene(Member& member);

            /// @brief Follows a character's team: in a team it is marked as having been in one; out of it after that, its following goes off.
            /// @param following The character's following.
            /// @param world The character's world scene, in the world.
            /// @param name The account, for the log.
            static void watchTeam(Following& following, const WorldScene& world, const std::string& name);

            /// @brief Sends the selected character's team invitation to a following member out of a team on its map, a few tries
            /// kInviteGapMs apart, while that team has room: from the selected character when it leads or is in none, from the leader's
            /// connection when another own character leads it; never when someone else's character leads it.
            /// @param member The member to invite.
            /// @param playedWorld The played account's world scene.
            /// @param nowMs The current time in milliseconds.
            void inviteToTeam(Member& member, const WorldScene& playedWorld, std::uint64_t nowMs);

            /// @brief Runs a call on the member's screen with only its windows and popups kept (its HUD left out).
            /// @tparam Call The callable type; it takes the member's ui::Screen.
            /// @param index The member's row.
            /// @param hudScale The HUD scale.
            /// @param call What to run.
            template <typename Call>
            void withWindows(int index, float hudScale, const Call& call);

            /// @brief Tells whether the played account's popups, or the windows raised over them, catch the mouse at a point of the
            /// game screen: the layouts holdPlayedPopups holds, drawn over the members' windows.
            /// @param point The point, in the game screen's coordinates.
            /// @return True when one does.
            bool playedPopupAt(core::Point point);

            /// @brief Finds the member whose kept layouts catch the mouse at a point of the game screen.
            /// @param point The point, in the game screen's coordinates.
            /// @param hudScale The HUD scale.
            /// @return The member's row, or -1 for none.
            int memberAt(core::Point point, float hudScale);

            /// @brief Runs a call on each member in the world on the played account's map and in its team, while the NPC mirror is on.
            /// @tparam Call The callable type; it takes the Member and its WorldScene.
            /// @param call What to run.
            template <typename Call>
            void forMirroringMembers(const Call& call);

            /// @brief Marks a member as left the party: closes its scene and connection.
            /// @param member The member.
            /// @param reason Why it left (shown on its row).
            static void gone(Member& member, const std::string& reason);

            std::unique_ptr<AccountBundle>& m_played;
            const audio::AudioDevice& m_device;
            std::function<void(GameServices&)> m_setUpPlayed;
            const audio::AudioDevice m_silent{audio::AudioDevice::Silent{}};
            std::vector<std::unique_ptr<Member>> m_members;
            bool m_started = false;
            std::uint64_t m_nowMs = 0;
            Following m_playedFollowing;
            int m_commanding = -1;
            bool m_mirrorNpcs = false;
            std::vector<const ui::Element*> m_heldLayouts; // the played screen's layouts holdPlayedPopups kept for drawWindows
            int m_pendingSelect = -1; // row asked for by select, swapped in by applySelection
            std::unique_ptr<PartyPanels> m_panels; // the Advanced panel
            std::unique_ptr<SimplePartyPanels> m_simplePanels; // the Simple rows

            /// @brief Tells whether the Simple rows show: Simple multi-login (or off) with at most kSimpleMembers members holding a
            /// row (more, logged in under Advanced, keep the Advanced panel until they are down to that).
            /// @return True for the Simple rows.
            bool simpleRows() const;
            std::vector<PartyMessage> m_messages;
            std::uint8_t m_panelButton = 0; // the SDL mouse button pressed on a row, whose release is taken too; 0 for none
            std::optional<core::Point> m_mouse;
            int m_mouseOwner = -1;
            int m_keyboardOwner = -1;
            std::vector<SDL_Keycode> m_routedKeys;

            /// @brief A chat line already handed to the played account, and when.
            struct SentLine {
                int channel;
                std::string line;
                std::uint64_t atMs;
            };

            std::vector<SentLine> m_sentLines;
    };

}

#endif
