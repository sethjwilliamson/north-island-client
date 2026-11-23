#include "client/subsystems/movie/MovieDirector.h"

#include "client/subsystems/movie/SfwArt.h"

#include "core/FileSystem.h"
#include "core/Log.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace client {

    namespace {

        constexpr core::Size kVeiledGroundSize{832, 640}; // size of the story illustrations, given to a veiled movie's player instead of loading its ground

        /// @brief Tells whether a script shows a picture of the SFW list, as its ground or as a moving picture.
        /// @param script The parsed script.
        /// @return True when one is listed.
        bool showsSfwArt(const formats::MovieScript& script) {
            return (script.ground.kind() != formats::MovieGround::Kind::Map && sfwBlocked(script.ground.pictureName())) ||
                   std::any_of(script.pictures.begin(), script.pictures.end(), [](const formats::MoviePicture& picture) { return sfwBlocked(picture.name); });
        }

        constexpr std::array<const char*, 8> kStoryScripts{"15010", "14035", "25094", "25099", "25110", "25129", "25132", "25139"};

    }

    MovieDirector::MovieDirector(
        GameServices& services,
        Hooks hooks
    )
        : m_services(services),
          m_hooks(std::move(hooks)) {}

    void MovieDirector::playStory(int index, std::uint64_t nowMs) {
        if (index < 1 || index > static_cast<int>(kStoryScripts.size()) || m_screen.mode() != 0 || !m_hooks.mayStart()) { // screen mode 0: no movie
            return;
        }
        if (start(kStoryScripts[static_cast<std::size_t>(index) - 1], nowMs)) {
            m_screen.set(MovieScreenMode::kStory);
        }
    }

    bool MovieDirector::startEvent(const std::string& script, int option, std::uint64_t nowMs) {
        const bool musicWasPlaying = m_services.music.playing();
        if (!m_player) {
            saveMusic();
        }
        m_screen.eventOption(option);
        m_hooks.stopScreenLight();
        m_services.music.stop();
        m_heldFrame.reset();
        m_screen.serverMessage();
        if (m_player && m_player->playing()) {
            m_queue.push_back(script);
            return true;
        }
        if (!load(script, musicWasPlaying, nowMs)) {
            m_screen.reset();
            if (musicWasPlaying) {
                m_hooks.playSceneMusic();
            }
            return false;
        }
        return true;
    }

    bool MovieDirector::start(const std::string& script, std::uint64_t nowMs) {
        if (m_player && m_player->playing()) {
            m_queue.push_back(script);
            return true;
        }
        saveMusic();
        return load(script, m_services.music.playing(), nowMs);
    }

    void MovieDirector::saveMusic() {
        m_savedMusic = m_services.music.file();
        m_resumeMs = m_services.music.positionMs();
    }

    void MovieDirector::restoreAfterMovie() {
        m_services.zoom.reset();
        if (m_mapMovie && m_musicSaved && !m_savedMusic.empty()) {
            m_services.music.playLooped(m_savedMusic, m_resumeMs);
        }
        m_mapMovie = false;
    }

    bool MovieDirector::load(const std::string& script, bool musicWasPlaying, std::uint64_t nowMs) {
        const auto data = core::readFile(m_services.game.data("sty/" + script + ".sty"));
        const auto parsed = data ? formats::MovieScript::parse(*data) : std::nullopt;
        if (!parsed) {
            core::logWarning("the movie ", script, " cannot be read");
            return false;
        }
        if (!m_soundTable) {
            const auto table = core::readFile(m_services.game.data("sound/soundtabel.txt"));
            m_soundTable = table ? formats::SoundTable::parse(*table) : formats::SoundTable();
        }
        MovieHost& host = *this;
        const bool veiled = m_services.videoSettings().sfw && (sfwBlockedScript(script) || showsSfwArt(*parsed));
        if (veiled) {
            // the listed picture is never loaded; the player runs on a ground of the illustrations' size that nothing draws
            core::logInfo("SFW: the movie ", script, " plays over the world, without its picture");
            m_player = std::make_unique<MoviePlayer>(*parsed, *m_soundTable, host, kVeiledGroundSize, nowMs);
        } else if (parsed->ground.kind() == formats::MovieGround::Kind::Map) {
            const auto map = m_hooks.mapGround(std::atoi(parsed->ground.pictureName().c_str()));
            if (!map) {
                core::logWarning("the movie ", script, " has no map ", parsed->ground.pictureName());
                return false;
            }
            m_player = std::make_unique<MoviePlayer>(*parsed, *m_soundTable, host, map->size, nowMs, map->objects);
        } else {
            const auto ground = m_services.pictures.find(parsed->ground.pictureName());
            if (!ground) {
                core::logWarning("the movie ", script, " has no ground picture ", parsed->ground.pictureName());
                return false;
            }
            const gfx::Picture& picture = m_services.pictures.picture(*ground);
            m_player = std::make_unique<MoviePlayer>(*parsed, *m_soundTable, host, core::Size{picture.width(), picture.height()}, nowMs);
        }
        m_veiled = veiled;
        m_musicSaved = musicWasPlaying;
        m_mapMovie = !veiled && parsed->ground.kind() == formats::MovieGround::Kind::Map;
        m_jrp = parsed->ground.kind() == formats::MovieGround::Kind::Jrp;
        // role surfaces 99901 to 99904 are the team's looks
        m_teamLayout = std::any_of(parsed->roles.begin(), parsed->roles.end(), [](const formats::MovieRole& role) {
            return role.surface >= 99901 && role.surface <= 99904; // surfaces 99901..99904: team members 1 to 4
        });
        m_hooks.hudShown(false);
        return true;
    }

    void MovieDirector::finish() {
        if (!m_player) {
            return;
        }
        m_player.reset();
        m_queue.clear();
        restoreAfterMovie();
        m_hooks.hudShown(true);
        m_hooks.closeTalk();
        m_heldFrame.reset();
        m_screen.reset();
    }

    void MovieDirector::serverMessage() {
        if (m_screen.frozen()) {
            m_screen.serverMessage();
            m_heldFrame.reset();
            m_hooks.hudShown(true);
        }
    }

    void MovieDirector::update(std::uint64_t nowMs) {
        if (!m_player) {
            return;
        }
        m_player->update(nowMs);
        if (!m_player->playing()) {
            end(nowMs);
        }
    }

    void MovieDirector::end(std::uint64_t nowMs) {
        if (m_musicSaved) {
            m_hooks.playSceneMusic();
        }
        const bool held = m_screen.mode() == MovieScreenMode::kHeld;
        if (held) {
            m_heldFrame = m_player->frame(nowMs);
        }
        m_player.reset();
        restoreAfterMovie();
        m_screen.movieEnded();
        while (!m_queue.empty()) {
            const std::string next = m_queue.front();
            m_queue.pop_front();
            if (load(next, m_services.music.playing(), nowMs)) {
                m_screen.set(MovieScreenMode::kEvent);
                m_screen.serverMessage();
                m_heldFrame.reset();
                return;
            }
        }
        if (!m_screen.frozen()) {
            m_hooks.hudShown(true);
        }
    }

    std::optional<MovieFrame> MovieDirector::frame(const gfx::Canvas& canvas, std::uint64_t nowMs) {
        if (!m_player) {
            return m_heldFrame;
        }
        return m_player->frame(nowMs, canvas.betweenBeats(), canvas.beatMs());
    }

    void MovieDirector::playMusic(const std::string& soundFile) { m_services.music.playLooped(m_services.game.data(soundFile)); }

    void MovieDirector::playSceneMusic() { m_hooks.playSceneMusic(); }

    void MovieDirector::playSound(const std::string& soundFile) { m_services.playSound(soundFile); }

    int MovieDirector::playLoopedSound(const std::string& soundFile, int oldHandle) { return m_services.playLoopedSound(soundFile, oldHandle); }

    void MovieDirector::stopLoopedSound(int handle) { m_services.sounds.stopLooped(handle); }

    void MovieDirector::say(const formats::MovieTalk& talk) { m_hooks.say(talk, m_jrp, m_teamLayout); }

    int MovieDirector::random(int below) { return std::uniform_int_distribution<int>(0, below - 1)(m_random); }

    bool MovieDirector::ownSeat(int seatSurface) { return m_hooks.ownSeat(seatSurface); }

    bool MovieDirector::talkOpen() const { return m_hooks.talkOpen(); }

    int MovieDirector::roleFrames(int surface, int slot) {
        const auto look = m_hooks.roleLook(surface);
        if (!look) {
            return 0;
        }
        if (std::holds_alternative<CharacterLook>(*look)) {
            return m_services.characters.figureFrameCount(std::get<CharacterLook>(*look), slot);
        }
        return m_services.characters.npcFrameCount(std::get<formats::NpcInfo>(*look).spriteId, slot);
    }

}
