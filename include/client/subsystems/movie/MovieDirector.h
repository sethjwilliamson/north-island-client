#ifndef NORTH_ISLAND_CLIENT_SUBSYSTEMS_MOVIE_MOVIE_DIRECTOR_H
#define NORTH_ISLAND_CLIENT_SUBSYSTEMS_MOVIE_MOVIE_DIRECTOR_H

#include "client/services/GameServices.h"
#include "client/subsystems/movie/MovieFrame.h"
#include "client/subsystems/movie/MovieMapGround.h"
#include "client/subsystems/movie/MovieRoleLook.h"
#include "client/subsystems/movie/MoviePlayer.h"
#include "client/subsystems/movie/MovieScreenMode.h"
#include "formats/SoundTable.h"
#include "gfx/Canvas.h"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <optional>
#include <string>

namespace client {

    /// @brief Runs the movie around the game: screen mode, the movie playing, the queue of waiting ones and the music.
    /// @details Original: FScreenMode (movie branch of the frame loop is on in 1, 3 or 4), MovieQueue, FSavedMusicFile; StartMovie plays or queues a
    /// script, PlayStoryMovie is the 23/151 way in, FinishMovie (20/48) ends the one playing.
    class MovieDirector : private MovieHost {
        public:
            /// @brief Callbacks the director needs from the game around it.
            struct Hooks {
                std::function<bool()> mayStart;
                std::function<void(bool)> hudShown;
                std::function<void()> playSceneMusic;
                std::function<void()> stopScreenLight;
                std::function<void(const formats::MovieTalk&, bool wide, bool teamLayout)> say;
                std::function<bool(int seatSurface)> ownSeat;
                std::function<bool()> talkOpen;
                std::function<std::optional<MovieMapGround>(int mapId)> mapGround;
                std::function<void(gfx::Canvas&, core::Point origin)> paintMapGround;
                std::function<void(gfx::Canvas&, std::size_t object, core::Point origin, std::uint64_t nowMs)> paintMapObject;
                std::function<void(gfx::Canvas&, core::Point origin, int forceKind, bool over, std::uint64_t nowMs)> paintSceneEffects;
                // the experimental lighting of a map ground: under what stands on it (people: where the roles stand on the
                // map), and over it
                std::function<void(gfx::Canvas&, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs)> lightMapGround;
                std::function<void(gfx::Canvas&, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs)> lightMapOver;
                std::function<std::optional<MovieRoleLook>(int surface)> roleLook;
                std::function<void()> closeTalk;
            };

            /// @brief Creates the director.
            /// @param services Game services (music, zoom, pictures, data).
            /// @param hooks Callbacks into the game.
            MovieDirector(
                GameServices& services,
                Hooks hooks
            );

            /// @brief Tells whether a movie is on screen (FScreenMode 1, 3 or 4).
            /// @return True while the movie branch is on.
            bool active() const { return m_screen.playing(); }

            /// @brief Gives the screen mode.
            /// @return FScreenMode.
            int screenMode() const { return m_screen.mode(); }

            /// @brief Tells whether a held movie's last picture stays on screen (FScreenMode 2 and FMovieFrozen).
            /// @return True when frozen.
            bool frozen() const { return m_screen.frozen(); }

            /// @brief Tells whether the movie on screen is veiled: the SFW setting was on when it began and it shows a picture, or is a script, of the SFW lists
            /// (SfwArt.h). A veiled movie runs as any other (steps, talks, music, its end and held state) but nothing of it is drawn: the world
            /// shows instead. Not in the original.
            /// @return True while such a movie (or its held last frame) is on screen.
            bool veiled() const { return m_veiled; }

            /// @brief Handles a server 20/x message: the held picture goes and the HUD and world come back.
            void serverMessage();

            /// @brief Plays one of the eight story scripts.
            /// @details Original: PlayStoryMovie; sub 1 to 8 of 23/151 name the script.
            /// @param index Sub id 1 to 8.
            /// @param nowMs Current clock in ms.
            void playStory(int index, std::uint64_t nowMs);

            /// @brief Plays a script of the sty folder, or queues it while a movie plays.
            /// @details Original: StartMovie.
            /// @param script Script name, e.g. "15010".
            /// @param nowMs Current clock in ms.
            /// @return False when the script is missing or uses what the player does not do yet.
            bool start(const std::string& script, std::uint64_t nowMs);

            /// @brief Plays an event-result movie (kind 5): sets the screen mode of its option, stops the scene's light and music, then starts the script.
            /// @details Original: result kind 5 of BK_EventManage.pas; options 1 and 3 give mode 1, 2 and 4 give mode 4; StopScreenLight, StopMusic, StartMovie.
            /// @param script Script name.
            /// @param option Event option 1 to 4.
            /// @param nowMs Current clock in ms.
            /// @return False when the script cannot be played (the music is put back then).
            bool startEvent(const std::string& script, int option, std::uint64_t nowMs);

            /// @brief Ends the movie playing and clears the queue.
            /// @details Original: FinishMovie (20/48).
            void finish();

            /// @brief Advances the movie and ends it when it is over.
            /// @details Original: UpdateMovie.
            /// @param nowMs Current clock in ms.
            void update(std::uint64_t nowMs);

            /// @brief Builds what to draw this frame; with free frames drawn between the last two beats.
            /// @details Original: DrawMovie.
            /// @param canvas Canvas that gives the beat timing.
            /// @param nowMs Current clock in ms.
            /// @return The frame, the held picture when no movie plays, or nothing.
            std::optional<MovieFrame> frame(const gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Tells who a role of the movie is.
            /// @param surface Role surface id.
            /// @return The look, or none when nobody.
            std::optional<MovieRoleLook> roleLook(int surface) const { return m_hooks.roleLook(surface); }

            /// @brief Paints the scene effects of the efficacy mode, under the roles or over them.
            /// @param canvas Target canvas.
            /// @param origin View origin.
            /// @param forceKind Effect kind of the efficacy mode.
            /// @param over True to paint over the roles.
            /// @param nowMs Current clock in ms.
            void paintSceneEffects(
                gfx::Canvas& canvas,
                core::Point origin,
                int forceKind,
                bool over,
                std::uint64_t nowMs
            ) const {
                m_hooks.paintSceneEffects(canvas, origin, forceKind, over, nowMs);
            }

            /// @brief Puts the lighting's shadows on the map ground just painted, and has what is drawn next noted for it.
            /// @param canvas Target canvas.
            /// @param origin View origin.
            /// @param people Where the roles stand on the map.
            /// @param nowMs Current clock in ms.
            void lightMapGround(gfx::Canvas& canvas, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs) const {
                if (m_hooks.lightMapGround) {
                    m_hooks.lightMapGround(canvas, origin, people, nowMs);
                }
            }

            /// @brief Puts the lighting over the map ground and what stands on it.
            /// @param canvas Target canvas.
            /// @param origin View origin.
            /// @param people Where the roles stand on the map.
            /// @param nowMs Current clock in ms.
            void lightMapOver(gfx::Canvas& canvas, core::Point origin, const std::vector<core::Point>& people, std::uint64_t nowMs) const {
                if (m_hooks.lightMapOver) {
                    m_hooks.lightMapOver(canvas, origin, people, nowMs);
                }
            }

            /// @brief Paints the map ground for the movie layer.
            /// @param canvas Target canvas.
            /// @param origin View origin.
            void paintMapGround(gfx::Canvas& canvas, core::Point origin) const { m_hooks.paintMapGround(canvas, origin); }

            /// @brief Paints one ground object of the map for the movie layer.
            /// @param canvas Target canvas.
            /// @param object Index in the map's object list.
            /// @param origin View origin.
            /// @param nowMs Current clock in ms.
            void paintMapObject(gfx::Canvas& canvas, std::size_t object, core::Point origin, std::uint64_t nowMs) const {
                m_hooks.paintMapObject(canvas, object, origin, nowMs);
            }

        private:
            /// @brief Plays a looped music file from the game data.
            /// @param soundFile Sound file name.
            void playMusic(const std::string& soundFile) override;

            /// @brief Starts the scene's music again.
            void playSceneMusic() override;

            /// @brief Plays a sound effect.
            /// @param soundFile Sound file name.
            void playSound(const std::string& soundFile) override;

            /// @brief Replaces a looped sound.
            /// @param soundFile Sound file name.
            /// @param oldHandle Handle of the loop to stop.
            /// @return Handle of the new loop.
            int playLoopedSound(const std::string& soundFile, int oldHandle) override;

            /// @brief Stops a looped sound.
            /// @param handle Handle of the loop.
            void stopLoopedSound(int handle) override;

            /// @brief Opens a talk box through the hook.
            /// @param talk The speaker and text.
            void say(const formats::MovieTalk& talk) override;

            /// @brief Tells whether the talk box is open.
            /// @return True while open.
            bool talkOpen() const override;

            /// @brief Gives the frame count of a role's sprite in an action slot.
            /// @param surface Role surface id.
            /// @param slot Action slot.
            /// @return Frame count, 0 when unknown.
            int roleFrames(int surface, int slot) override;

            /// @brief Draws a random number.
            /// @param below Exclusive upper bound.
            /// @return Value from 0 to below - 1.
            int random(int below) override;

            /// @brief Tells whether a team seat is the player himself.
            /// @param seatSurface Seat surface id.
            /// @return True for the player.
            bool ownSeat(int seatSurface) override;

            /// @brief Zooms the view in.
            void zoomIn() override { m_services.zoom.zoomIn(); }

            /// @brief Zooms the view out.
            void zoomOut() override { m_services.zoom.zoomOut(); }

            /// @brief Zooms the view to the most.
            void zoomMost() override { m_services.zoom.zoomToMost(); }

            /// @brief Keeps the music file and its position to be put back after the movie.
            /// @details Original: StartMovie's saving of FSavedMusicFile and FMusicResumePos.
            void saveMusic();

            /// @brief Resets the zoom and puts a map ground's music back where it was.
            /// @details Original: FinishMovie and the frame loop after a movie.
            void restoreAfterMovie();

            /// @brief Loads a script and starts its player.
            /// @details Original: LoadMovieScript and the rest of StartMovie.
            /// @param script Script name.
            /// @param musicWasPlaying Whether music was playing when the movie began (FSavedMusicFile).
            /// @param nowMs Current clock in ms.
            /// @return False when the script or its ground cannot be loaded.
            bool load(const std::string& script, bool musicWasPlaying, std::uint64_t nowMs);

            /// @brief Ends the finished movie: music, HUD, the held picture and the next of the queue.
            /// @details Original: UpdateMovie's end and the frame loop's [0, 2] branch after it.
            /// @param nowMs Current clock in ms.
            void end(std::uint64_t nowMs);

            GameServices& m_services;
            Hooks m_hooks;
            std::optional<formats::SoundTable> m_soundTable;
            std::unique_ptr<MoviePlayer> m_player;
            std::deque<std::string> m_queue;
            MovieScreenMode m_screen;
            std::optional<MovieFrame> m_heldFrame;
            bool m_jrp = false;
            bool m_teamLayout = false;
            std::mt19937 m_random{std::random_device{}()};
            bool m_musicSaved = false;
            std::filesystem::path m_savedMusic;
            std::uint64_t m_resumeMs = 0;
            bool m_mapMovie = false;
            bool m_veiled = false;
    };

}

#endif
