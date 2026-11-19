#ifndef NORTH_ISLAND_CLIENT_SCENES_SCENE_H
#define NORTH_ISLAND_CLIENT_SCENES_SCENE_H

#include "client/scenes/SceneKind.h"
#include "client/services/input/PadAsk.h"
#include "client/services/input/ScriptLogin.h"
#include "client/services/input/ScriptProbe.h"
#include "gfx/Canvas.h"
#include "ui/Screen.h"

#include <functional>
#include <cstdint>
#include <optional>
#include <vector>

namespace client {

    /// @brief Base class of a program stage (loading, login, world): its screen plus the logic behind it.
    class Scene {
        public:
            virtual ~Scene() = default;

            /// @brief Runs one logic beat of the scene (on the 30 ms beat).
            /// @param nowMs Current time in ms.
            virtual void update(std::uint64_t nowMs) = 0;

            /// @brief Draws the scene.
            /// @param canvas Target canvas.
            /// @param nowMs Current time in ms.
            virtual void draw(gfx::Canvas& canvas, std::uint64_t nowMs) = 0;

            /// @brief Moves what moves smoothly (the people on the map and the view) before every drawn frame with free frames (--fps).
            /// @param nowMs Current time in ms.
            virtual void move(std::uint64_t nowMs) { (void)nowMs; }

            /// @brief Tells whether frames may be drawn between the beats now; a scene whose drawing steps animations per drawn frame keeps to the beat.
            /// @return True when free frames are allowed.
            virtual bool freeFrames() const { return false; }

            /// @brief Gives the time the scene's last update held the clock to (the battle's 35 ms sleeps) with free frames.
            /// @return Time in ms; the next beat comes after it and the frames until then are drawn at it (0: not held).
            virtual std::uint64_t heldUntilMs() const { return 0; }

            /// @brief Gives the screen the window's input goes to.
            /// @return The scene's screen.
            virtual ui::Screen& screen() = 0;

            /// @brief Gives the scene to switch to, once this one is done.
            /// @return The next scene, or nothing while this one goes on.
            virtual std::optional<SceneKind> nextScene() const = 0;

            /// @brief Tells whether the scene is cut off from the server: its disconnect form is up (kicked, locked, the connection failed or lost) and waits for a click.
            /// @details A party member's unseen scene has nobody to click that form, so PartyMembers drops the member then.
            /// @return True while cut off (the default: never).
            virtual bool cutOff() const { return false; }

            /// @brief Does what a game pad asks for directly.
            /// @param ask What is asked.
            /// @return True when the scene took it: the pad then does nothing else with the button (the default takes nothing).
            virtual bool padAsk(PadAsk ask) {
                (void)ask;
                return false;
            }

            /// @brief Tells whether the pad's directions step through something now (a list of choices, the battle's command ring), so that the pad does not walk with them.
            /// @return True while the directions are asked for.
            virtual bool wantsPadSteps() const { return false; }

            /// @brief Tells the scene whether a game pad is what is played with.
            /// @param on True in controller mode.
            virtual void setPadInUse(bool on) { (void)on; }

            /// @brief Gives the places on the scene itself the pad's pointer steps through while no window is open (the fighters a command can be aimed at).
            /// @return The places in screen coordinates, the one to start on first; empty when there are none.
            virtual std::vector<core::Point> padSpots() { return {}; }

            /// @brief A click the scene wants a game pad to make with the mouse pointer, on what its accept or cancel button reached.
            struct PadClick {
                core::Point point; // where, in screen coordinates
                bool right = false; // with the right button
            };

            /// @brief Hands over the click the scene asked for since the last call (after PadAsk::Interact or PadAsk::Cancel), once.
            /// @return The click, or none.
            virtual std::optional<PadClick> takePadClick() { return std::nullopt; }

            /// @brief Tells the scene whether the pad plays without the pointer, so that it can mark what PadAsk::Interact would use.
            /// @param on True in controller mode with the pointer hidden and no menu of the pad open.
            /// @param hand The hand to mark it with (it points down).
            virtual void setPadAim(bool on, const ui::PadPointer& hand) {
                (void)on;
                (void)hand;
            }

            /// @brief Gives what a test script may ask of the scene.
            /// @return The probe, or nothing (the default: it offers nothing).
            virtual std::optional<std::reference_wrapper<ScriptProbe>> scriptProbe() { return std::nullopt; }

            /// @brief Gives what a test script's login command may ask of the scene.
            /// @return The login hook, or nothing (the default: only the login scene offers it).
            virtual std::optional<std::reference_wrapper<ScriptLogin>> scriptLogin() { return std::nullopt; }
    };

}

#endif
