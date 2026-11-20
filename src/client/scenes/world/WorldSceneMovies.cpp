#include "client/scenes/world/WorldScene.h"
#include "client/rules/GmIds.h"

namespace client {

    namespace {

        constexpr int kPlayerSpeaker = 65535; // movie speaker id 65535: the own player

    }

    MovieDirector::Hooks WorldScene::movieHooks() {
        MovieDirector::Hooks hooks;
        hooks.mayStart = [this] { return m_fightState == 0 && m_activeSport == 0 && !m_starSky.active(); };
        hooks.hudShown = [this](bool shown) { setHudShown(shown); };
        hooks.playSceneMusic = [this] { playMapMusic(); };
        hooks.stopScreenLight = [this] { m_screenLight.stop(); };
        hooks.ownSeat = [this](int seatSurface) { return teamSeatHolder(seatSurface) == m_player.id; };
        hooks.say = [this](const formats::MovieTalk& talk, bool wide, bool teamLayout) {
            const std::optional<TalkDialog::Speaker> speaker = movieSpeaker(talk.speaker);
            if (speaker) {
                TalkDialog::Timing timing;
                timing.movie = wide;
                timing.movieScreen = true;
                timing.movieBottom = teamLayout;
                timing.modal = true;
                m_talk.say(m_services.talks.text(talk.textId), speaker, [] {}, std::move(timing));
            }
        };
        hooks.roleLook = [this](int surface) { return movieRoleLook(surface); };
        hooks.mapGround = [this](int mapId) { return movieMapGround(mapId); };
        hooks.paintMapGround = [this](gfx::Canvas& canvas, core::Point origin) {
            if (m_movieMap) {
                Camera camera;
                camera.setOrigin(origin);
                m_painter.paintMovieGround(canvas, *m_movieMap, camera);
            }
        };
        hooks.paintMapObject = [this](gfx::Canvas& canvas, std::size_t object, core::Point origin, std::uint64_t nowMs) {
            if (m_movieMap && object < m_movieMap->objects().size()) {
                Camera camera;
                camera.setOrigin(origin);
                m_painter.paintMovieObject(canvas, *m_movieMap, m_movieMap->objects()[object], camera, nowMs);
            }
        };
        // the lighting of the movie's map, as the map itself has it: the roles stand in for the players
        const auto lighted = [this](gfx::Canvas& canvas, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs, bool over) {
            if (!m_movieMap) {
                return;
            }
            m_lighting.setOptions(m_services.videoSettings().lighting);
            Camera camera;
            camera.setOrigin(origin);
            const auto scene = m_scenes ? m_scenes->find(m_movieMap->id()) : std::nullopt;
            const int type = scene ? scene->type : 0;
            std::vector<MapCharacter> roles(people.size());
            std::vector<std::reference_wrapper<MapCharacter>> characters;
            for (std::size_t i = 0; i < people.size(); ++i) {
                roles[i].position = people[i];
                characters.emplace_back(roles[i]);
            }
            if (!over) {
                if (!m_movieLit) { // the figures the map noted last are not the movie's
                    m_lighting.forgetFigures();
                    m_movieLit = true;
                }
                m_lighting.paintGround(canvas, m_services.pictures, *m_movieMap, camera, type, characters, {}, {}, nowMs);
                hideFigureShadows(m_lighting.castsShadows());
                if (m_lighting.notesFigures()) {
                    beginFigureEcho();
                    noteFigureCovers(canvas.device());
                }
                return;
            }
            m_lighting.apply(canvas, m_services.pictures, *m_movieMap, camera, type, characters, {}, nowMs);
            hideFigureShadows(false);
            endFigureEcho(canvas.device());
        };
        hooks.lightMapGround = [lighted](gfx::Canvas& canvas, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs) {
            lighted(canvas, origin, people, nowMs, false);
        };
        hooks.lightMapOver = [lighted](gfx::Canvas& canvas, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs) {
            lighted(canvas, origin, people, nowMs, true);
        };
        hooks.paintSceneEffects = [this](gfx::Canvas& canvas, core::Point origin, int forceKind, bool over, std::uint64_t nowMs) {
            SceneEffects::Conditions conditions = effectConditions();
            conditions.forceKind = forceKind;
            if (over) {
                m_effects.drawOver(canvas, origin, nowMs, conditions);
            } else {
                m_effects.drawUnder(canvas, origin, nowMs, conditions);
            }
        };
        hooks.talkOpen = [this] { return m_talk.isOpen(); };
        hooks.closeTalk = [this] { m_talk.close(); };
        return hooks;
    }

    std::optional<MovieMapGround> WorldScene::movieMapGround(int mapId) {
        if (m_map && m_map->id() == mapId) {
            m_movieMap = *m_map;
        } else {
            m_movieMap = WorldMap::load(mapId, mapFileOf(mapId), *m_mapArchive, *m_objectArchive, m_services.pictures, 0);
        }
        if (!m_movieMap) {
            return std::nullopt;
        }
        MovieMapGround ground;
        ground.size = {m_movieMap->width(), m_movieMap->height()};
        for (const GroundObject& object : m_movieMap->objects()) {
            ground.objects.push_back({object.info.kind, object.info.depth});
        }
        return ground;
    }

    std::int32_t WorldScene::teamSeatHolder(int surface) const {
        constexpr int kFirstTeamLook = 99901; // movie surface ids 99901..99904: team seats (seat = id - 99901, 0 is the leader)
        constexpr int kFirstOtherLook = 99909; // movie surface ids 99909..99912: team seats again (seat = id - 99909)
        const int seat = surface - (surface >= kFirstOtherLook ? kFirstOtherLook : kFirstTeamLook);
        const std::int32_t leader = m_teams.leaderOf(m_player.id).value_or(m_player.id);
        if (seat <= 0) {
            return leader;
        }
        const std::vector<std::int32_t>& members = m_teams.members(leader);
        return static_cast<std::size_t>(seat) > members.size() ? 0 : members[static_cast<std::size_t>(seat) - 1];
    }

    std::optional<MovieRoleLook> WorldScene::movieRoleLook(int surface) {
        const auto who = movieSpeaker(surface);
        return who ? std::optional<MovieRoleLook>(who->figure) : std::nullopt;
    }

    std::optional<TalkDialog::Speaker> WorldScene::movieSpeaker(int surface) {
        // movie speaker/surface ids 99901..99913 are the player's team and pets, not NPC ids
        constexpr int kFirstTeamLook = 99901; // 99901..99904: look of team seats (leader first)
        constexpr int kLastTeamLook = 99904;
        constexpr int kFirstOtherLook = 99909; // 99909..99912: look of team seats, second id range
        constexpr int kLastOtherLook = 99912;
        constexpr int kFirstPet = 99905; // 99905..99908: pet places 1..4
        constexpr int kLastPet = 99908;
        constexpr int kFightingPet = 99913; // 99913: the pet that is in the fight (active pet)
        if (surface == kPlayerSpeaker) {
            return ownSpeaker(true);
        }
        const bool teamLook = surface >= kFirstTeamLook && surface <= kLastTeamLook;
        if (teamLook || (surface >= kFirstOtherLook && surface <= kLastOtherLook)) {
            const std::int32_t who = teamSeatHolder(surface);
            const auto character = who != 0 ? characterOf(who) : std::nullopt;
            return character ? std::optional<TalkDialog::Speaker>(TalkDialog::Speaker{character->get().look, character->get().name, true}) : std::nullopt;
        }
        int place = 0;
        if (surface >= kFirstPet && surface <= kLastPet) {
            place = surface - kFirstPet + 1;
        } else if (surface == kFightingPet) {
            place = m_pets.activePlace();
        }
        if (place != 0) {
            const auto& pet = m_pets.at(place);
            return pet ? std::optional<TalkDialog::Speaker>(TalkDialog::Speaker{pet->info, pet->info.name, true}) : std::nullopt;
        }
        const auto npc = m_services.npcs.find(surface);
        return npc ? std::optional<TalkDialog::Speaker>(TalkDialog::Speaker{*npc, npc->name, true}) : std::nullopt;
    }

    bool WorldScene::startEventMovie(const EventResult& result) {
        if (isGmId(m_player.id)) {
            return false;
        }
        if (m_services.unseenPartyMember) { // nobody watches it or clicks its lines, so the event goes on at once
            return true;
        }
        m_movie.director().startEvent(std::to_string(result.value), result.option, m_nowMs);
        return true;
    }

    void WorldScene::onStoryMovie(int index) { m_movie.director().playStory(index, m_nowMs); }

    void WorldScene::onMovieFinish() { m_movie.director().finish(); }

}
