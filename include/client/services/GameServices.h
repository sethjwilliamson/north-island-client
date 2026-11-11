#ifndef NORTH_ISLAND_CLIENT_SERVICES_GAME_SERVICES_H
#define NORTH_ISLAND_CLIENT_SERVICES_GAME_SERVICES_H

#include "audio/MusicPlayer.h"
#include "audio/SoundEffects.h"
#include "client/services/AccountSession.h"
#include "client/services/GameDirectory.h"
#include "client/services/photo/PhotoCheck.h"
#include "client/services/settings/ExtraSettings.h"
#include "client/rendering/world/ViewZoom.h"
#include "client/rendering/characters/CharacterRenderer.h"
#include "client/data/EmoteCodes.h"
#include "core/Size.h"
#include "formats/CompoundTable.h"
#include "formats/FormulaData.h"
#include "formats/ItemTable.h"
#include "formats/MarkTable.h"
#include "formats/MotionTable.h"
#include "formats/NpcTable.h"
#include "formats/SkillTable.h"
#include "formats/StartupInfo.h"
#include "formats/TalkTable.h"
#include "gfx/PictureLibrary.h"
#include "net/GameConnection.h"
#include "ui/LoadContext.h"

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace client {

    /// @brief What started the party leader's event, for the members that mirror it (qol): the 20/x trigger it sent.
    enum class PartyTrigger {
        NpcClick, // 20/1: the leader clicked an NPC
        NpcNear, // 20/2: the leader came near an NPC whose events start so
        Door, // 20/8: the leader walked into a door with events
    };

    /// @brief What the screens of the game share; the Application owns all of it.
    struct GameServices {
        const GameDirectory& game;
        const formats::StartupInfo& startup;
        gfx::PictureLibrary& pictures;
        audio::SoundEffects& sounds;
        audio::MusicPlayer& music;
        net::GameConnection& connection;
        const formats::FormulaData& formula;
        const formats::ItemTable& items;
        formats::CompoundTable& compounds;
        const formats::NpcTable& npcs;
        const formats::SkillTable& skills;
        const formats::MotionTable& motions;
        const formats::TalkTable& talks;
        const formats::MarkTable& marks;
        CharacterRenderer& characters;
        AccountSession& session;
        ViewZoom& zoom;
        std::function<void(const std::string& caption, const std::string& text)> messageBox;
        std::function<void(const std::filesystem::path& file, const PhotoCheck& check)> saveScreenshot;
        bool freeFrames = false;
        std::uint64_t nextBeatMs = 0;
        core::Rect view{0, 0, 800, 600}; // visible game area: x, y, width, height in px (800 x 600 screen)
        std::function<ExtraSettings()> videoSettings = [] { return ExtraSettings{}; };
        std::function<void(const ExtraSettings&)> changeVideo = [](const ExtraSettings&) {};
        std::vector<std::string> renderDrivers;
        std::string rendererInUse;
        std::function<core::Size()> desktopSize = [] { return core::Size{}; };
        std::function<bool(std::int32_t inviterId)> joinsTeamOf = [](std::int32_t) { return false; };
        std::function<bool(std::int32_t leaderId)> takesLeadFrom = [](std::int32_t) { return false; };
        std::function<bool()> autoBattle = [] { return false; };
        std::function<void(int channel, const std::string& line, std::int32_t senderId)> chatHeard;
        std::function<bool(int typedChannel, const std::string& text)> partyChat;
        std::function<std::string(const std::string& account, const std::string& password, int slot, bool autoBattle)> partyLogIn;
        std::function<bool(std::int32_t playerId)> isPartyMember;
        std::function<void(std::int32_t playerId)> partyLogOut;
        std::function<void(bool centre, const std::string& text, int durationMs)> partyMessage;
        std::function<bool()> partyMirrorsNpcs; // the members follow the leader's NPC talk and doors (qol)
        std::function<void(bool on)> setPartyMirrorsNpcs;
        std::function<void(PartyTrigger trigger, int index)> partyEventStarted; // the leader started an NPC's or a door's event
        std::function<void(int dataId, int code)> partyEventAnswered; // the leader answered an event dialog
        std::function<void()> partyEventEnded; // the leader's event ended
        bool unseenPartyMember = false; // a party member's account, played without its own screen (qol)
        bool serverSorts = false;
        std::array<bool, 3> videoGroupsOpen = {true, false, false}; // which groups of the settings Video tab are open, until the game closes

        /// @brief Builds the UI load context (pictures, game file resolver, emote lookup).
        /// @return The load context.
        ui::LoadContext loadContext() const {
            ui::LoadContext context(pictures, [this](const std::string& relative) { return game.data(relative); });
            context.emoteAt = [](const std::string& text, std::size_t position) { return emoteAt(text, position); };
            return context;
        }

        /// @brief Plays a sound effect of the game install.
        /// @param relative Game-style relative file name.
        void playSound(const std::string& relative) const { sounds.play(game.data(relative)); }

        /// @brief Plays a looping sound effect of the game install.
        /// @param relative Game-style relative file name.
        /// @param oldHandle Handle of the loop being replaced.
        /// @return The new loop handle.
        int playLoopedSound(const std::string& relative, int oldHandle) const { return sounds.playLooped(game.data(relative), oldHandle); }
    };

}

#endif
