#ifndef NORTH_ISLAND_CLIENT_SERVICES_INPUT_INPUT_SCRIPT_H
#define NORTH_ISLAND_CLIENT_SERVICES_INPUT_INPUT_SCRIPT_H

#include "client/services/input/ScriptLogin.h"
#include "client/services/input/ScriptPendingClick.h"
#include "client/services/input/ScriptProbe.h"
#include "client/services/input/ScriptWait.h"
#include "core/Bytes.h"
#include "ui/Screen.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace client {

    /// @brief Test driver (--script FILE): plays mouse and keyboard input into the current screen and takes screenshots, without the system's mouse or keyboard.
    /// @details One command per line, '#' starts a comment. Input commands:
    ///   wait MS (pause); click X Y [right]; double X Y; ctrlclick X Y [right] (click with Ctrl held);
    ///   sdlclick X Y [right] (click put on SDL's event queue, read as the system's mouse is, so the qol party panels and members' windows see it);
    ///   sdlkey NAME (key on SDL's queue: a letter, Escape or Enter; Ctrl+NAME with Ctrl); move X Y;
    ///   press X Y, drag X Y, release X Y (left button down, moved held, up; ctrlpress, ctrlrelease with Ctrl held);
    ///   ctrlwheel N (N wheel notches with Ctrl held, up positive; the world zooms); keydown NAME, keyup NAME;
    ///   type TEXT (text input to the element that has the keyboard; with none, typed text goes to the chat line);
    ///   focus NAME (give the keyboard to the layout element of that Name, e.g. StallPrice); typefield NAME TEXT (focus NAME, then type TEXT);
    ///   key NAME (Enter, Tab, Escape, Backspace, Delete, Left, Right, Up, Down, Home, End, I, S; Ctrl+NAME holds Ctrl);
    ///   shot FILE[.bmp] (save the next frame, relative to the user folder, default extension .bmp; the script goes on when it is saved);
    ///   pad attach (plug in a virtual game pad), pad detach, pad button NAME down|up, pad axis NAME VALUE (NAME as SDL names buttons and axes: a, b, x, y, back, start, leftshoulder, dpup, leftx, righty ...; VALUE from -1 to 1, a trigger's from 0 to 1);
    ///   recv HEX (a server message payload in hex, e.g. "35 0a 01", that the game reads next); quit (end the program).
    /// @details Scene commands (world scene; they find things instead of using screen coordinates, as the camera scrolls with the player) each log a line starting "script: " to north-island-client.log in the user folder: "ok" or the result, "TIMEOUT" when a wait ran out (script goes on), "ERROR" when it could not do it (script goes on with the next line):
    ///   clicknpc ID (click the NPC with that Npc.dat number at a point of its sprite where the game's hit test takes it);
    ///   clickobj KIND ID (any object; KIND: npc, npcindex (place number on the map), player (id of another player), ground (index of a thing on the ground), furn (house piece number), furnitem (first piece of that item number in the room), self);
    ///   dclicknpc, dclickobj (double click); rclicknpc, rclickobj (right button); ctrlclicknpc, ctrlclickobj (Ctrl held); the mouse moves onto the point, the script waits two frames (house piece hover is worked out when the world is drawn), then clicks; furniture must be within 170 pixels (walkto first);
    ///   where KIND ID (log what the object is, its map point and the screen point a click would use, "screen none" when off screen or covered);
    ///   walkto X Y [MS] (walk to the map point the way a click does, then wait until the walk is over, at most MS, default 15000; logs where the player stopped);
    ///   waitwin NAME [MS] (wait until the window of that layout Name is shown, default 10000);
    ///   waitmsg TEXT [MS] (wait until a chat or message line containing TEXT was shown since the last command that does something; default 10000);
    ///   waitmap ID [MS] (wait until the player is on map ID, default 30000).
    /// @details Commands that wait for the game's state instead of a fixed time (log like the scene commands):
    ///   login ACCOUNT PASSWORD [SLOT] [MS] (wait for the server list to show a server, connect to the first one, log in, enter the world with
    ///   the character of SLOT, default 1, then wait as waitworld does; at most MS in all, default 60000);
    ///   waitworld [MS] (wait until the world is loaded: the played account's map, and every --accounts party member in the world; default 60000);
    ///   waitdialog [MS] (wait until the event talk box, with words, a question or choices, is open and can be answered; default 10000);
    ///   choose ANSWER (answer the open talk box: N picks choice N, 1 the first; Confirm, Cancel or Close press that button; Next closes words);
    ///   waitevent [MS] (wait until the player's NPC or door event has ended; default 15000);
    ///   skiptalk [MS] (click away the talk boxes of words as they come, until the event ends or a box asks a question; default 15000);
    ///   button NAME (click the shown layout button of that Name, e.g. GroupPartyMirror).
    /// @details Failures: with --script, an error that would show a message box or a modal error form (a disconnection, a refused login)
    /// is logged as "script: ERROR ..." and ends the program with exit code 3; --script-timeout SECONDS ends it with exit code 4 when the script
    /// runs longer.
    class InputScript {
        public:
            /// @brief What else the program offers the commands that wait for the game's state.
            struct Host {
                std::optional<std::reference_wrapper<ScriptLogin>> login; // the login scene's hook, while it is shown
                std::function<bool()> worldReady; // the world is loaded, with the party members in it
            };

            /// @brief Reads a script file.
            /// @param file Path of the script.
            /// @return The script, empty when the file cannot be read.
            static std::optional<InputScript> load(const std::filesystem::path& file);

            using Probe = std::optional<std::reference_wrapper<ScriptProbe>>;

            /// @brief Runs the commands that are due; stops after a shot (taken once the frame is drawn).
            /// @param screen Screen that receives the input.
            /// @param nowMs Current time in milliseconds.
            /// @param inject Callback that feeds a server message payload to the game (recv command).
            /// @param probe What the scene offers for the commands that look into it (none: nothing to look into).
            /// @param host What the program offers the login and waitworld commands.
            void step(
                ui::Screen& screen,
                std::uint64_t nowMs,
                const std::function<void(const core::Bytes&)>& inject,
                const Probe& probe = std::nullopt,
                const Host& host = {}
            );

            /// @brief Takes the pending screenshot request, if any.
            /// @return The requested file name, empty when none.
            std::optional<std::string> takeScreenshotRequest();

            /// @brief Takes the notches of a ctrlwheel command since the last call.
            /// @return The notches, empty when none.
            std::optional<int> takeCtrlWheel();

            /// @brief Tells whether the script asked to end the program.
            /// @return True after the quit command.
            bool wantsQuit() const { return m_quit; }

        private:
            /// @brief Runs one line of the commands that look into the scene or wait.
            /// @param command The first word of the line.
            /// @param line The whole line.
            /// @param screen Screen that receives the input.
            /// @param nowMs Current time in milliseconds.
            /// @param probe What the scene offers.
            /// @return True when the command was a scene command.
            bool sceneCommand(
                const std::string& command,
                const std::string& line,
                ui::Screen& screen,
                std::uint64_t nowMs,
                const Probe& probe
            );

            /// @brief Starts a click on an object: moves the mouse onto it and schedules the click two frames later.
            /// @param spec Which click (button, double, Ctrl) and object.
            /// @param line The script line, for the log.
            /// @param screen Screen that receives the input.
            /// @param nowMs Current time in milliseconds.
            /// @param probe What the scene offers.
            void startClick(
                const ScriptClickSpec& spec,
                const std::string& line,
                ui::Screen& screen,
                std::uint64_t nowMs,
                const Probe& probe
            );

            /// @brief Performs the pending click once its frames have passed.
            /// @param screen Screen that receives the input.
            /// @param nowMs Current time in milliseconds.
            /// @param probe What the scene offers.
            void finishClick(ui::Screen& screen, std::uint64_t nowMs, const Probe& probe);

            /// @brief Begins waiting for a condition (window, message, map or walk end).
            /// @param wait What to wait for.
            /// @param nowMs Current time in milliseconds.
            /// @param screen Screen being watched.
            /// @param probe What the scene offers.
            void startWait(ScriptWait wait, std::uint64_t nowMs, ui::Screen& screen, const Probe& probe);

            /// @brief Checks whether the current wait is satisfied or timed out.
            /// @param screen Screen being watched.
            /// @param nowMs Current time in milliseconds.
            /// @param probe What the scene offers.
            void checkWait(ui::Screen& screen, std::uint64_t nowMs, const Probe& probe);

            /// @brief Types text to the element that has the keyboard, or to the chat line when none has it.
            /// @param screen Screen that receives the text.
            /// @param text The text to type.
            void typeText(ui::Screen& screen, const std::string& text);

            /// @brief Runs one line of the commands that wait for the game's state (login, waitworld, waitdialog, choose, waitevent, skiptalk,
            /// button).
            /// @param command The first word of the line.
            /// @param words The words of the line.
            /// @param line The whole line.
            /// @param screen Screen that receives the input.
            /// @param nowMs Current time in milliseconds.
            /// @param probe What the scene offers.
            /// @return True when the command was one of them.
            bool stateCommand(
                const std::string& command,
                const std::vector<std::string>& words,
                const std::string& line,
                ui::Screen& screen,
                std::uint64_t nowMs,
                const Probe& probe
            );

            std::vector<std::string> m_lines;
            std::size_t m_next = 0;
            std::uint64_t m_waitUntilMs = 0;
            std::optional<std::string> m_screenshot;
            std::optional<int> m_ctrlWheel;
            std::optional<ScriptWait> m_wait;
            std::uint64_t m_actionMark = 0;
            std::optional<ScriptPendingClick> m_click;
            bool m_quit = false;
            SDL_Joystick* m_pad = nullptr; // the virtual game pad of the pad commands
            const Host* m_host = nullptr; // the host given to the step that runs now
    };

}

#endif
