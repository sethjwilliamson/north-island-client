#include "client/services/input/InputScript.h"

#include "client/services/input/ScriptCommand.h"
#include "core/FileSystem.h"
#include "core/Log.h"
#include "core/StringUtil.h"
#include "ui/ImageButton.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>

namespace client {

    namespace {

        /// @brief Strips a modifier prefix such as "Ctrl+" or "Alt+" from a key name.
        /// @param name Key name; the prefix is removed from it when present.
        /// @param prefix The prefix to look for.
        /// @return True when the prefix was there.
        bool withoutPrefix(std::string& name, const std::string& prefix) {
            if (name.rfind(prefix, 0) != 0) {
                return false;
            }
            name.erase(0, prefix.size());
            return true;
        }

        /// @brief Looks up the key a script names (Enter, Tab, Escape, arrows, letters, digits, F1..F8, Space ...).
        /// @param name Key name from the script.
        /// @return The key, empty when the name is unknown.
        std::optional<ui::Key> keyNamed(const std::string& name) {
            static const std::map<std::string, ui::Key> keys = {
                {"Enter", ui::Key::Enter},   {"Tab", ui::Key::Tab},   {"Escape", ui::Key::Escape}, {"Backspace", ui::Key::Backspace},
                {"Delete", ui::Key::Delete}, {"Left", ui::Key::Left}, {"Right", ui::Key::Right},   {"Up", ui::Key::Up},
                {"Down", ui::Key::Down},     {"Home", ui::Key::Home}, {"End", ui::Key::End},       {"A", ui::Key::A},
                {"D", ui::Key::D},           {"F", ui::Key::F},       {"H", ui::Key::H},           {"J", ui::Key::J},
                {"E", ui::Key::E},           {"G", ui::Key::G},       {"I", ui::Key::I},           {"M", ui::Key::M},
                {"P", ui::Key::P},           {"Q", ui::Key::Q},       {"Space", ui::Key::Space},   {"F1", ui::Key::F1},
                {"F2", ui::Key::F2},         {"F3", ui::Key::F3},     {"F4", ui::Key::F4},         {"F5", ui::Key::F5},
                {"F6", ui::Key::F6},         {"F7", ui::Key::F7},     {"F8", ui::Key::F8},         {"S", ui::Key::S},
                {"T", ui::Key::T},           {"1", ui::Key::Digit1},  {"2", ui::Key::Digit2},      {"3", ui::Key::Digit3},
                {"4", ui::Key::Digit4},      {"5", ui::Key::Digit5},  {"6", ui::Key::Digit6},      {"7", ui::Key::Digit7},
                {"8", ui::Key::Digit8},
            };
            const auto found = keys.find(name);
            if (found == keys.end()) {
                return std::nullopt;
            }
            return found->second;
        }

        constexpr std::uint64_t kClickSettleMs = 100; // wait after a click before the next step, ms
        constexpr int kMostMoves = 5; // most mouse moves allowed within one click
        constexpr std::uint64_t kWalkTimeoutMs = 15000; // timeout for a walk step, ms
        constexpr std::uint64_t kWindowTimeoutMs = 10000; // timeout waiting for a window, ms
        constexpr std::uint64_t kMessageTimeoutMs = 10000; // timeout waiting for a message, ms
        constexpr std::uint64_t kMapTimeoutMs = 30000; // timeout waiting for a map change, ms
        constexpr std::uint64_t kLoginTimeoutMs = 60000; // timeout of a login until the world is loaded, ms
        constexpr std::uint64_t kWorldTimeoutMs = 60000; // timeout waiting for the world, ms
        constexpr std::uint64_t kDialogTimeoutMs = 10000; // timeout waiting for a talk box, ms
        constexpr std::uint64_t kEventTimeoutMs = 15000; // timeout waiting for an event to end, ms

        /// @brief Formats a point as "(x,y)" for the log.
        /// @param point The point.
        /// @return The text.
        std::string pointText(core::Point point) { return "(" + std::to_string(point.x) + "," + std::to_string(point.y) + ")"; }

        /// @brief Reads the optional time-out word of a wait command.
        /// @param words The command's words.
        /// @param index Position of the time-out word.
        /// @param fallback Value used when the word is missing or not a positive number.
        /// @return The time-out in milliseconds.
        std::uint64_t timeoutOf(const std::vector<std::string>& words, std::size_t index, std::uint64_t fallback) {
            const auto ms = index < words.size() ? ScriptCommand::number(words[index]) : std::nullopt;
            return ms && *ms > 0 ? static_cast<std::uint64_t>(*ms) : fallback;
        }

    }

    std::optional<InputScript> InputScript::load(const std::filesystem::path& file) {
        auto lines = core::readLines(file);
        if (!lines) {
            return std::nullopt;
        }
        InputScript script;
        for (const std::string& line : *lines) {
            const std::string command = core::trim(line);
            if (!command.empty() && command[0] != '#') {
                script.m_lines.push_back(command);
            }
        }
        return script;
    }

    std::optional<std::string> InputScript::takeScreenshotRequest() {
        auto request = std::move(m_screenshot);
        m_screenshot.reset();
        return request;
    }

    std::optional<int> InputScript::takeCtrlWheel() {
        auto steps = m_ctrlWheel;
        m_ctrlWheel.reset();
        return steps;
    }

    void InputScript::step(
        ui::Screen& screen,
        std::uint64_t nowMs,
        const std::function<void(const core::Bytes&)>& inject,
        const Probe& probe,
        const Host& host
    ) {
        m_host = &host;
        if (m_click && nowMs >= m_click->dueMs) {
            finishClick(screen, nowMs, probe);
        }
        if (m_wait) {
            checkWait(screen, nowMs, probe);
        }
        while (m_next < m_lines.size() && nowMs >= m_waitUntilMs && !m_screenshot && !m_quit && !m_wait && !m_click) {
            const std::string& line = m_lines[m_next++];
            std::istringstream words(line);
            std::string command;
            words >> command;
            if (probe && command != "where" && command != "shot" && command.rfind("wait", 0) != 0) {
                m_actionMark = probe->get().textMark();
            }
            if (command == "wait") {
                int ms = 0;
                words >> ms;
                m_waitUntilMs = nowMs + static_cast<std::uint64_t>(ms);
            } else if (command == "click" || command == "double" || command == "ctrlclick") {
                int x = 0;
                int y = 0;
                std::string side;
                words >> x >> y >> side;
                const ui::MouseButton button = side == "right" ? ui::MouseButton::Right : ui::MouseButton::Left;
                const bool ctrl = command == "ctrlclick";
                screen.mouseMove({x, y}, false);
                screen.mouseDown({x, y}, button, 1, ctrl);
                screen.mouseUp({x, y}, button, ctrl);
                if (command == "double") {
                    screen.mouseDown({x, y}, button, 2); // click count 2: the second press of a double click
                    screen.mouseUp({x, y}, button);
                }
            } else if (command == "press" || command == "ctrlpress" || command == "drag" || command == "release" || command == "ctrlrelease") {
                int x = 0;
                int y = 0;
                words >> x >> y;
                screen.mouseMove({x, y}, command != "press" && command != "ctrlpress");
                if (command == "press" || command == "ctrlpress") {
                    screen.mouseDown({x, y}, ui::MouseButton::Left, 1, command == "ctrlpress");
                } else if (command == "release" || command == "ctrlrelease") {
                    screen.mouseUp({x, y}, ui::MouseButton::Left, command == "ctrlrelease");
                }
            } else if (command == "sdlclick") {
                int x = 0;
                int y = 0;
                std::string side;
                words >> x >> y >> side;
                SDL_Event motion{};
                motion.type = SDL_EVENT_MOUSE_MOTION;
                motion.motion.x = static_cast<float>(x);
                motion.motion.y = static_cast<float>(y);
                SDL_PushEvent(&motion);
                for (const bool down : {true, false}) {
                    SDL_Event press{};
                    press.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                    press.button.button = side == "right" ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
                    press.button.down = down;
                    press.button.clicks = 1;
                    press.button.x = static_cast<float>(x);
                    press.button.y = static_cast<float>(y);
                    SDL_PushEvent(&press);
                }
            } else if (command == "sdlkey") {
                std::string name;
                words >> name;
                const bool ctrl = withoutPrefix(name, "Ctrl+");
                SDL_Keycode code = SDLK_UNKNOWN;
                if (name == "Escape") {
                    code = SDLK_ESCAPE;
                } else if (name == "Enter") {
                    code = SDLK_RETURN;
                } else if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name[0])) != 0) {
                    code = static_cast<SDL_Keycode>(std::tolower(static_cast<unsigned char>(name[0])));
                }
                if (code == SDLK_UNKNOWN) {
                    core::logWarning("script: unknown key ", name);
                } else {
                    for (const bool down : {true, false}) {
                        SDL_Event key{};
                        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                        key.key.key = code;
                        key.key.mod = ctrl ? SDL_KMOD_LCTRL : SDL_KMOD_NONE;
                        key.key.down = down;
                        SDL_PushEvent(&key);
                    }
                }
            } else if (command == "pad") {
                std::string what;
                std::string name;
                std::string value;
                words >> what >> name >> value;
                if (what == "attach") {
                    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); // a test's window need not have the keyboard
                    SDL_VirtualJoystickDesc pad;
                    SDL_INIT_INTERFACE(&pad);
                    pad.type = SDL_JOYSTICK_TYPE_GAMEPAD;
                    pad.naxes = SDL_GAMEPAD_AXIS_COUNT;
                    pad.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
                    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&pad);
                    m_pad = id != 0 ? SDL_OpenJoystick(id) : nullptr;
                    if (m_pad == nullptr) {
                        core::logError("script: ERROR ", line, ": ", SDL_GetError());
                    } else {
                        // a trigger let go is the low end of its axis; a new virtual pad has every axis in the middle
                        SDL_SetJoystickVirtualAxis(m_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_MIN_SINT16);
                        SDL_SetJoystickVirtualAxis(m_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_MIN_SINT16);
                    }
                } else if (m_pad == nullptr) {
                    core::logError("script: ERROR ", line, ": no pad attached (pad attach)");
                } else if (what == "detach") {
                    const SDL_JoystickID id = SDL_GetJoystickID(m_pad);
                    SDL_CloseJoystick(m_pad);
                    SDL_DetachVirtualJoystick(id);
                    m_pad = nullptr;
                } else if (what == "button") {
                    const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(name.c_str());
                    if (button == SDL_GAMEPAD_BUTTON_INVALID || (value != "down" && value != "up")) {
                        core::logError("script: ERROR ", line, ": pad button NAME down|up, NAME as SDL names it (a, b, x, y, back, start, dpup ...)");
                    } else {
                        SDL_SetJoystickVirtualButton(m_pad, button, value == "down");
                    }
                } else if (what == "axis") {
                    const SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(name.c_str());
                    if (axis == SDL_GAMEPAD_AXIS_INVALID || value.empty()) {
                        core::logError("script: ERROR ", line, ": pad axis NAME VALUE, NAME as SDL names it (leftx, lefty, rightx, righty, lefttrigger, righttrigger), VALUE from -1 to 1 (a trigger's from 0 to 1)");
                    } else {
                        double amount = std::max(-1.0, std::min(1.0, std::atof(value.c_str())));
                        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                            amount = std::max(0.0, amount) * 2.0 - 1.0; // a trigger goes from 0 to 1 over its axis's whole range
                        }
                        SDL_SetJoystickVirtualAxis(m_pad, axis, static_cast<Sint16>(amount * 32767.0)); // 32767: an axis's largest value
                    }
                } else {
                    core::logError("script: ERROR ", line, ": pad attach, pad detach, pad button NAME down|up or pad axis NAME VALUE");
                }
            } else if (command == "move") {
                int x = 0;
                int y = 0;
                words >> x >> y;
                screen.mouseMove({x, y}, false);
            } else if (command == "ctrlwheel") {
                int steps = 0;
                words >> steps;
                m_ctrlWheel = steps;
                screen.mouseWheel(steps);
            } else if (command == "type") {
                const std::size_t space = line.find(' ');
                if (space != std::string::npos) {
                    typeText(screen, line.substr(space + 1));
                }
            } else if (command == "keydown" || command == "keyup") {
                std::string name;
                words >> name;
                const bool ctrl = withoutPrefix(name, "Ctrl+");
                const bool alt = withoutPrefix(name, "Alt+");
                if (const auto key = keyNamed(name)) {
                    if (command == "keydown") {
                        screen.keyDown({*key, ctrl, false, alt});
                    } else {
                        screen.keyUp({*key, ctrl, false, alt});
                    }
                } else {
                    core::logWarning("script: unknown key ", name);
                }
            } else if (command == "key") {
                std::string name;
                words >> name;
                const bool ctrl = withoutPrefix(name, "Ctrl+");
                const bool alt = withoutPrefix(name, "Alt+");
                if (const auto key = keyNamed(name)) {
                    screen.keyDown({*key, ctrl, false, alt});
                    screen.keyUp({*key, ctrl, false, alt});
                } else {
                    core::logWarning("script: unknown key ", name);
                }
            } else if (command == "shot") {
                std::string file;
                words >> file;
                if (file.find('.') == std::string::npos) {
                    file += ".bmp";
                }
                m_screenshot = file;
            } else if (command == "recv") {
                core::Bytes payload;
                int value = 0;
                while (words >> std::hex >> value) {
                    payload.push_back(static_cast<std::uint8_t>(value));
                }
                inject(payload);
            } else if (command == "quit") {
                m_quit = true;
            } else if (!sceneCommand(command, line, screen, nowMs, probe) &&
                       !stateCommand(command, ScriptCommand::words(line), line, screen, nowMs, probe)) {
                core::logWarning("script: unknown command ", line);
            }
        }
        m_host = nullptr;
    }

    bool InputScript::stateCommand(
        const std::string& command,
        const std::vector<std::string>& words,
        const std::string& line,
        ui::Screen& screen,
        std::uint64_t nowMs,
        const Probe& probe
    ) {
        ScriptWait wait;
        wait.what = line;
        if (command == "login") {
            const auto slot = words.size() > 3 ? ScriptCommand::number(words[3]) : std::optional<int>(1);
            if (words.size() < 3 || !slot || *slot < 1) {
                core::logError("script: ERROR ", line, ": login ACCOUNT PASSWORD [SLOT] [MS] needs the account and password");
                return true;
            }
            wait.kind = ScriptWait::Kind::Login;
            wait.what = "login " + words[1];
            wait.text = words[1];
            wait.password = words[2];
            wait.number = *slot;
            wait.deadlineMs = nowMs + timeoutOf(words, 4, kLoginTimeoutMs);
        } else if (command == "waitworld") {
            wait.kind = ScriptWait::Kind::World;
            wait.deadlineMs = nowMs + timeoutOf(words, 1, kWorldTimeoutMs);
        } else if (command == "waitdialog") {
            wait.kind = ScriptWait::Kind::Dialog;
            wait.deadlineMs = nowMs + timeoutOf(words, 1, kDialogTimeoutMs);
        } else if (command == "waitevent" || command == "skiptalk") {
            wait.kind = command == "waitevent" ? ScriptWait::Kind::Event : ScriptWait::Kind::SkipTalk;
            wait.deadlineMs = nowMs + timeoutOf(words, 1, kEventTimeoutMs);
        } else if (command == "choose") {
            std::string result;
            if (words.size() < 2) {
                core::logError("script: ERROR ", line, ": choose needs a choice number, Confirm, Cancel, Close or Next");
            } else if (!probe) {
                core::logError("script: ERROR ", line, ": this scene has no talk box");
            } else if (!probe->get().answerTalk(words[1], result)) {
                core::logError("script: ERROR ", line, ": ", result);
            } else {
                core::logInfo("script: ", line, " ok: ", result);
            }
            return true;
        } else if (command == "button") {
            if (words.size() < 2) {
                core::logError("script: ERROR ", line, ": the button's name is missing");
            } else if (!screen.isShown(words[1])) {
                core::logError("script: ERROR ", line, ": no shown element named ", words[1]);
            } else {
                try {
                    screen.get<ui::ImageButton>(words[1]).click(screen.context());
                    core::logInfo("script: ", line, " ok");
                } catch (const ui::LayoutError& error) {
                    core::logError("script: ERROR ", line, ": ", error.what());
                }
            }
            return true;
        } else {
            return false;
        }
        startWait(wait, nowMs, screen, probe);
        return true;
    }

    void InputScript::typeText(ui::Screen& screen, const std::string& text) {
        if (!screen.isTyping()) {
            core::logWarning("script: type: no text field has the keyboard (click it, or use focus NAME or typefield NAME TEXT)");
        }
        for (const char letter : text) {
            screen.textInput(std::string(1, letter));
        }
    }

    bool InputScript::sceneCommand(
        const std::string& command,
        const std::string& line,
        ui::Screen& screen,
        std::uint64_t nowMs,
        const Probe& probe
    ) {
        const std::vector<std::string> words = ScriptCommand::words(line);
        std::string error;
        if (const auto click = ScriptCommand::click(words, error)) {
            startClick(*click, line, screen, nowMs, probe);
            return true;
        }
        if (!error.empty()) {
            core::logError("script: ERROR ", line, ": ", error);
            return true;
        }
        if (command == "focus" || command == "typefield") {
            if (words.size() < 2) {
                core::logError("script: ERROR ", line, ": the element's name is missing");
                return true;
            }
            if (!screen.isShown(words[1])) {
                core::logError("script: ERROR ", line, ": no shown element named ", words[1]);
                return true;
            }
            screen.focus(words[1]);
            if (command == "typefield") {
                const std::size_t nameAt = line.find(words[1]);
                const std::size_t textAt = line.find_first_not_of(" \t", nameAt + words[1].size());
                if (textAt != std::string::npos) {
                    typeText(screen, line.substr(textAt));
                }
            }
            core::logInfo("script: ", command, " ", words[1], " ok");
            return true;
        }
        if (command == "where") {
            const auto object = ScriptCommand::object(words, 1, error);
            if (!object) {
                core::logError("script: ERROR ", line, ": ", error);
            } else if (!probe) {
                core::logError("script: ERROR ", line, ": this scene has no map to look at");
            } else if (const auto sighting = probe->get().sight(*object)) {
                core::logInfo(
                    "script: where ",
                    ScriptCommand::kindName(object->kind),
                    " ",
                    object->id,
                    ": ",
                    sighting->label,
                    " map ",
                    pointText(sighting->map),
                    " screen ",
                    sighting->screen ? pointText(*sighting->screen) : std::string("none (off the screen or covered)")
                );
            } else {
                core::logError("script: ERROR ", line, ": not in the scene");
            }
            return true;
        }
        if (command == "walkto") {
            const auto x = words.size() > 1 ? ScriptCommand::number(words[1]) : std::nullopt;
            const auto y = words.size() > 2 ? ScriptCommand::number(words[2]) : std::nullopt;
            std::string reason;
            if (!x || !y) {
                core::logError("script: ERROR ", line, ": walkto X Y [MS] needs the map point");
            } else if (!probe) {
                core::logError("script: ERROR ", line, ": this scene has no map to walk on");
            } else if (!probe->get().startWalk({*x, *y}, reason)) {
                core::logError("script: ERROR ", line, ": ", reason);
            } else {
                ScriptWait wait;
                wait.kind = ScriptWait::Kind::Walk;
                wait.what = line;
                wait.deadlineMs = nowMs + timeoutOf(words, 3, kWalkTimeoutMs); // the MS word is the fourth word of walkto X Y [MS]
                startWait(wait, nowMs, screen, probe);
            }
            return true;
        }
        if (command == "waitwin" || command == "waitmsg" || command == "waitmap") {
            ScriptWait wait;
            wait.what = line;
            if (words.size() < 2) {
                core::logError("script: ERROR ", line, ": what to wait for is missing");
                return true;
            }
            wait.text = words[1];
            if (command == "waitwin") {
                wait.kind = ScriptWait::Kind::Window;
                wait.deadlineMs = nowMs + timeoutOf(words, 2, kWindowTimeoutMs);
            } else if (command == "waitmsg") {
                wait.kind = ScriptWait::Kind::Message;
                wait.mark = m_actionMark;
                wait.deadlineMs = nowMs + timeoutOf(words, 2, kMessageTimeoutMs);
            } else {
                const auto id = ScriptCommand::number(words[1]);
                if (!id) {
                    core::logError("script: ERROR ", line, ": the map number is not a number");
                    return true;
                }
                wait.kind = ScriptWait::Kind::Map;
                wait.number = *id;
                wait.deadlineMs = nowMs + timeoutOf(words, 2, kMapTimeoutMs);
            }
            startWait(wait, nowMs, screen, probe);
            return true;
        }
        return false;
    }

    void InputScript::startClick(
        const ScriptClickSpec& spec,
        const std::string& line,
        ui::Screen& screen,
        std::uint64_t nowMs,
        const Probe& probe
    ) {
        if (!probe) {
            core::logError("script: ERROR ", line, ": this scene has no objects to click");
            return;
        }
        const auto sighting = probe->get().sight(spec.target);
        if (!sighting) {
            core::logError("script: ERROR ", line, ": the object is not in the scene");
            return;
        }
        if (!sighting->screen) {
            core::logError("script: ERROR ", line, ": ", sighting->label, " is at map ", pointText(sighting->map), " but off the screen or covered");
            return;
        }
        screen.mouseMove(*sighting->screen, false);
        m_click = ScriptPendingClick{spec, line, *sighting->screen, nowMs + kClickSettleMs, 0};
    }

    void InputScript::finishClick(ui::Screen& screen, std::uint64_t nowMs, const Probe& probe) {
        ScriptPendingClick click = *m_click;
        m_click.reset();
        const auto sighting = probe ? probe->get().sight(click.spec.target) : std::nullopt;
        if (!sighting || !sighting->screen) {
            core::logError("script: ERROR ", click.what, ": the object left the scene or the screen before the click");
            return;
        }
        if (*sighting->screen != click.at) {
            if (++click.moves > kMostMoves) {
                core::logError("script: ERROR ", click.what, ": the object keeps moving");
                return;
            }
            click.at = *sighting->screen;
            click.dueMs = nowMs + kClickSettleMs;
            screen.mouseMove(click.at, false);
            m_click = click;
            return;
        }
        const ui::MouseButton button = click.spec.button;
        m_actionMark = probe->get().textMark();
        screen.mouseDown(click.at, button, 1, click.spec.ctrl);
        screen.mouseUp(click.at, button, click.spec.ctrl);
        if (click.spec.clicks == 2) {
            screen.mouseDown(click.at, button, 2); // click count 2: the second press of a double click
            screen.mouseUp(click.at, button);
        }
        core::logInfo("script: ", click.what, " ok: ", sighting->label, " clicked at ", pointText(click.at), " (map ", pointText(sighting->map), ")");
    }

    void InputScript::startWait(ScriptWait wait, std::uint64_t nowMs, ui::Screen& screen, const Probe& probe) {
        wait.startMs = nowMs;
        m_wait = std::move(wait);
        checkWait(screen, nowMs, probe);
    }

    void InputScript::checkWait(ui::Screen& screen, std::uint64_t nowMs, const Probe& probe) {
        const ScriptWait& wait = *m_wait;
        std::string result;
        switch (wait.kind) {
            case ScriptWait::Kind::Walk: // walkto: done when the player stands
                if (probe && !probe->get().isWalking()) {
                    result = "stopped at " + pointText(probe->get().ownPosition());
                }
                break;
            case ScriptWait::Kind::Window: // waitwin: done when the named window is shown
                if (screen.isShown(wait.text)) {
                    result = "shown";
                }
                break;
            case ScriptWait::Kind::Message: // waitmsg: done when a chat or message line with the text was shown
                if (probe && probe->get().textSince(wait.mark, wait.text)) {
                    result = "seen";
                }
                break;
            case ScriptWait::Kind::Map: // waitmap: done when the player is on the map
                if (probe && probe->get().mapId() == wait.number) {
                    result = "reached";
                }
                break;
            case ScriptWait::Kind::Login: // login: hand the account to the login scene once it lists a server, then wait for the world
                if (!wait.started) {
                    const auto& login = m_host ? m_host->login : std::nullopt;
                    if (login && login->get().hasServerRow()) {
                        std::string reason;
                        if (!login->get().startLogin(wait.text, wait.password, wait.number, reason)) {
                            core::logError("script: ERROR ", wait.what, ": ", reason);
                            m_wait.reset();
                            return;
                        }
                        m_wait->started = true;
                    }
                } else if (m_host && m_host->worldReady && m_host->worldReady()) {
                    result = "in the world";
                }
                break;
            case ScriptWait::Kind::World: // waitworld: done when the world is loaded with the party in it
                if (m_host && m_host->worldReady && m_host->worldReady()) {
                    result = "loaded";
                }
                break;
            case ScriptWait::Kind::Dialog: // waitdialog: done when the talk box can be answered
                if (probe && probe->get().talkReady()) {
                    result = "open";
                }
                break;
            case ScriptWait::Kind::Event: // waitevent: done when the player's event no longer runs
                if (probe && !probe->get().eventRunning()) {
                    result = "ended";
                }
                break;
            case ScriptWait::Kind::SkipTalk: // skiptalk: words are clicked away; done at a question or when the event is over
                if (!probe) {
                    break;
                }
                if (probe->get().talkReady()) {
                    std::string answered;
                    if (probe->get().answerTalk("Next", answered)) {
                        ++m_wait->number;
                    } else {
                        result = "a box asks a question after " + std::to_string(wait.number) + " line(s)";
                    }
                } else if (!probe->get().eventRunning()) {
                    result = "the event ended after " + std::to_string(wait.number) + " line(s)";
                }
                break;
        }
        const std::uint64_t waited = nowMs - wait.startMs;
        if (!result.empty()) {
            core::logInfo("script: ", wait.what, " ok: ", result, " after ", waited, " ms");
            m_wait.reset();
        } else if (nowMs >= wait.deadlineMs) {
            const std::string where = probe && wait.kind == ScriptWait::Kind::Walk ? " at " + pointText(probe->get().ownPosition()) : "";
            core::logWarning("script: ", wait.what, " TIMEOUT after ", waited, " ms", where);
            m_wait.reset();
        }
    }

}
