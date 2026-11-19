#include "client/services/Application.h"

#include "audio/AudioDevice.h"
#include "audio/MusicPlayer.h"
#include "audio/SoundEffects.h"
#include "client/services/input/CursorSet.h"
#include "client/services/input/Gamepad.h"
#include "client/services/input/HudReveal.h"
#include "client/services/frame/BeatClock.h"
#include "client/services/frame/FrameBeat.h"
#include "client/services/frame/FrameCost.h"
#include "client/services/frame/FrameLog.h"
#include "client/services/frame/FrameStats.h"
#include "client/services/frame/MachineLoad.h"
#include "client/services/GameDirectory.h"
#include "client/services/GameServices.h"
#include "client/services/input/InputScript.h"
#include "client/services/input/InputTranslator.h"
#include "client/services/input/PadControls.h"
#include "client/services/input/TouchPointer.h"
#include "client/services/input/PadPointers.h"
#include "client/services/photo/ScreenPhoto.h"
#include "client/services/mods/ModPacks.h"
#include "client/models/system/SystemOptions.h"
#include "client/services/settings/ExtraSettings.h"
#include "client/services/frame/VerticalBlank.h"
#include "client/rendering/hud/FrameGraph.h"
#include "client/services/party/AccountBundle.h"
#include "client/services/party/PartyMembers.h"
#include "client/rendering/world/ViewZoom.h"
#include "client/scenes/LoadingScene.h"
#include "client/scenes/LoginScene.h"
#include "client/scenes/world/WorldScene.h"
#include "core/Bytes.h"
#include "core/Clock.h"
#include "core/FileSystem.h"
#include "core/Log.h"
#include "core/Speedups.h"
#include "formats/BitmapFont.h"
#include "formats/LargeBitmapFont.h"
#include "formats/PngCodec.h"
#include "formats/FormulaData.h"
#include "formats/CompoundTable.h"
#include "formats/ItemTable.h"
#include "formats/StartupInfo.h"
#include "gfx/Canvas.h"
#include "gfx/GpuTimer.h"
#include "gfx/PictureLibrary.h"
#include "gfx/RenderDevice.h"
#include "gfx/LargeTextRenderer.h"
#include "gfx/TextRenderer.h"
#include "net/GameConnection.h"
#include "net/NetworkSystem.h"
#include "ui/TextBox.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace client {

    namespace {

        /// @brief Converts the qol display setting to the window display mode.
        /// @param display The setting's display mode.
        /// @return The matching window display mode.
        gfx::WindowDisplay windowDisplayOf(ExtraSettings::Display display) {
            switch (display) {
                case ExtraSettings::Display::Borderless: return gfx::WindowDisplay::Borderless; // borderless window
                case ExtraSettings::Display::Fullscreen: return gfx::WindowDisplay::Fullscreen; // fullscreen
                case ExtraSettings::Display::Windowed: break; // normal window (falls through to Windowed below)
            }
            return gfx::WindowDisplay::Windowed;
        }

#ifdef SDL_PLATFORM_ANDROID

        /// @brief Hands each log line to the system log (logcat) as well as to the stream buffer it stands before.
        class SystemLogBuffer : public std::streambuf {
            public:
                /// @brief Stands before a stream buffer.
                /// @param next The buffer that still gets everything; may be null.
                explicit SystemLogBuffer(
                    std::streambuf* next
                )
                    : m_next(next) {}

            protected:
                int_type overflow(int_type character) override {
                    if (traits_type::eq_int_type(character, traits_type::eof())) {
                        return traits_type::not_eof(character);
                    }
                    const char one = traits_type::to_char_type(character);
                    const std::lock_guard lock(m_lock);
                    if (one == '\n') {
                        SDL_Log("%s", m_line.c_str());
                        m_line.clear();
                    } else {
                        m_line += one;
                    }
                    return m_next != nullptr ? m_next->sputc(one) : character;
                }

                int sync() override { return m_next != nullptr ? m_next->pubsync() : 0; }

            private:
                std::streambuf* m_next;
                std::string m_line;
                std::mutex m_lock;
        };
#endif

        /// @brief Whether text is typed on a keyboard the system shows on the screen (a phone): it is then only asked for
        /// while a text field has the keyboard, instead of for the whole run.
        /// @return True on Android.
        bool typesOnScreenKeyboard() {
#ifdef SDL_PLATFORM_ANDROID
            return true;
#else
            return false;
#endif
        }

        /// @brief The program's name where the system shows one (a window's title, a message's heading).
#ifdef SDL_PLATFORM_ANDROID
        constexpr const char* kProgramTitle = "North Island";
#else
        constexpr const char* kProgramTitle = "Wonderland Online";
#endif

        /// @brief Sends std::clog to a file for as long as it lives (on Android to the system log too).
        class LogFile {
            public:
                /// @brief Opens (truncates) the log file and redirects std::clog to it.
                /// @param path The log file.
                explicit LogFile(
                    const std::filesystem::path& path
                )
                    : m_file(path, std::ios::trunc) {
#ifdef SDL_PLATFORM_ANDROID
                    m_previous = std::clog.rdbuf(&m_system);
#else
                    if (m_file) {
                        m_previous = std::clog.rdbuf(m_file.rdbuf());
                    }
#endif
                }

                ~LogFile() {
                    if (m_previous != nullptr) {
                        std::clog.rdbuf(m_previous);
                    }
                }

                LogFile(const LogFile&) = delete;
                LogFile& operator=(const LogFile&) = delete;

            private:
                std::ofstream m_file;
#ifdef SDL_PLATFORM_ANDROID
                SystemLogBuffer m_system{m_file ? m_file.rdbuf() : nullptr};
#endif
                std::streambuf* m_previous = nullptr;
        };

        /// @brief SDL's video and event subsystems for as long as it lives.
        class SdlVideo {
            public:
                /// @brief Starts SDL's video and event subsystems.
                SdlVideo()
                    : m_ready(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {}

                ~SdlVideo() { SDL_Quit(); }

                SdlVideo(const SdlVideo&) = delete;
                SdlVideo& operator=(const SdlVideo&) = delete;

                /// @brief Whether SDL started.
                /// @return True when SDL_Init succeeded.
                bool isReady() const { return m_ready; }

            private:
                bool m_ready;
        };

        /// @brief Creates the scene of a kind.
        /// @param kind Which scene to create.
        /// @param services The shared game services.
        /// @return The new scene, or null for Quit.
        std::unique_ptr<Scene> makeScene(SceneKind kind, GameServices& services) {
            switch (kind) {
                case SceneKind::Loading: // loading screen
                    return std::make_unique<LoadingScene>(services);
                case SceneKind::Login: // login screens
                    return std::make_unique<LoginScene>(services);
                case SceneKind::World: // the game world
                    return std::make_unique<WorldScene>(services);
                case SceneKind::Quit: // leave the program: no scene
                    break;
            }
            return nullptr;
        }

        /// @brief Sleeps to a moment of core::nowPreciseMs: whole milliseconds asleep, the last one (Speedups::limiterSpinMs) waited out.
        /// @param targetMs The target time in core::nowPreciseMs.
        void waitUntil(double targetMs) {
            const double now = core::nowPreciseMs();
            const double spinMs = core::speedups().limiterSpinMs;
            if (targetMs - now > spinMs) {
                SDL_Delay(static_cast<Uint32>(targetMs - now - spinMs));
            }
            while (core::nowPreciseMs() < targetMs) {}
        }

        /// @brief Sleeps to the next beat of a BeatClock (core::nowMs).
        /// @param beatMs The beat time in core::nowMs.
        void waitForBeat(std::uint64_t beatMs) {
            if (const std::uint64_t now = core::nowMs(); beatMs > now) {
                SDL_Delay(static_cast<Uint32>(beatMs - now));
            }
        }

        /// @brief Whether a file can be made in a folder (the folder is made first when it is not there).
        /// @param folder The folder to probe.
        /// @return True when a probe file could be written.
        bool canWriteIn(const std::filesystem::path& folder) {
            std::error_code ignored;
            std::filesystem::create_directories(folder, ignored);
            const std::filesystem::path probe = folder / ".north-island-write-test";
            const bool written = static_cast<bool>(std::ofstream(probe, std::ios::binary));
            std::filesystem::remove(probe, ignored);
            return written;
        }

        constexpr int kScriptErrorExit = 3; // exit code when an error box opened while a --script ran
        constexpr int kScriptTimeoutExit = 4; // exit code when a --script ran past --script-timeout

        bool scriptRuns = false; // a --script runs: errors are logged and end the program instead of waiting for a click on a box

        /// @brief Logs an error and shows it in an operating system message box (only logs it while a test script runs).
        /// @param message The error text.
        void showError(const std::string& message) {
            if (scriptRuns) {
                core::logError("script: ERROR ", message);
                return;
            }
            core::logError(message);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kProgramTitle, message.c_str(), nullptr);
        }

    }

    int Application::run() {
        scriptRuns = !m_options.scriptFile.empty();
        std::error_code ignored;
        if (!std::filesystem::is_directory(m_options.dataDirectory / "data", ignored)) {
#ifdef SDL_PLATFORM_ANDROID
            // the folder is made now, so it is there (and the app's own) to copy the game into
            std::filesystem::create_directories(m_options.dataDirectory, ignored);
            const std::string missing =
                "The game's files were not found.\n\nCopy the contents of the Wonderland Online folder (the data, pic and font folders and the rest) "
                "from a computer into\n" +
                m_options.dataDirectory.u8string() + "\nand start the game again.";
            SDL_Log("%s", missing.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kProgramTitle, missing.c_str(), nullptr);
            return 1;
#else
            std::string installed;
            for (const auto& place : LaunchOptions::defaultInstallLocations()) {
                installed += ", install the game to " + place.u8string();
            }
            showError(
                "The game's files were not found in " + m_options.dataDirectory.u8string() +
                ".\n\nPut north-island-client.exe in the Wonderland Online folder (the one with the data, pic and font folders)" + installed +
                " or start it with --data <that folder>."
            );
            return 1;
#endif
        }
        if (!m_options.userGiven && !canWriteIn(m_options.userDirectory)) {
            const std::filesystem::path own = LaunchOptions::ownUserDirectory();
            if (!own.empty()) {
                if (!std::filesystem::exists(own, ignored) && std::filesystem::is_directory(m_options.userDirectory, ignored)) {
                    std::filesystem::create_directories(own, ignored);
                    std::filesystem::copy(
                        m_options.userDirectory,
                        own,
                        std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing,
                        ignored
                    );
                }
                m_options.userDirectory = own;
            }
        }
        std::filesystem::create_directories(m_options.userDirectory, ignored);
        LogFile log(m_options.userDirectory / "north-island-client.log");
        core::logInfo("game data: ", m_options.dataDirectory.string());
        GameDirectory game(m_options.dataDirectory, m_options.userDirectory, m_options.layoutDirectory);

        SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
        SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
        if (m_options.background) {
            SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
            SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, "0");
        }
        const SdlVideo video;
        if (!video.isReady()) {
            showError(std::string("cannot start SDL: ") + SDL_GetError());
            return 1;
        }
        const net::NetworkSystem network;
        // built in place: VS 2017 (the XP toolset) does not elide the copy out of a conditional expression, and the device cannot be copied
        std::optional<audio::AudioDevice> audioHolder;
        if (m_options.background) {
            audioHolder.emplace(audio::AudioDevice::Silent{});
        } else {
            audioHolder.emplace();
        }
        const audio::AudioDevice& audioDevice = *audioHolder;

        const std::filesystem::path extraFile = ExtraSettings::fileIn(m_options.userDirectory);
        const auto loadedExtra = ExtraSettings::loadIn(m_options.userDirectory);
        ExtraSettings stored = loadedExtra.value_or(ExtraSettings{});
        ExtraSettings live = stored;
        live.override(m_options);
        if (!loadedExtra) {
            stored = live;
            stored.save(extraFile);
        } else if (!std::filesystem::exists(extraFile, ignored)) {
            stored.save(extraFile); // migrated from the old file name: keep what it held, without this run's overrides
        } else if (const auto written = core::readFile(extraFile); written && core::toString(*written).find("\"mode\"") == std::string::npos) {
            stored.save(extraFile); // a file from before the speedups or their mode: they are listed in it now, so they can be found
        }
        core::speedups().limiterSpinMs = live.speedups.limiterSpinMs;
        core::logInfo(
            "frames: ",
            frameModeName(live.frames),
            live.frames == ExtraSettings::Frames::Capped ? " " + std::to_string(live.fpsCap) : "",
            ", window ",
            live.windowWidth,
            "x",
            live.windowHeight,
            live.maximized ? " maximized" : "",
            ", scale ",
            live.pixelScale,
            "x, pixel art ",
            live.pixelArt,
            live.pixelArtPictures ? "" : " -pictures",
            live.pixelArtSprites ? "" : " -sprites",
            live.pixelArtText ? "" : " -text"
        );

        gfx::RenderDevice::Settings settings;
        settings.title = kProgramTitle;
        settings.mode = m_options.renderMode;
        settings.driver = live.renderer;
#ifdef SDL_PLATFORM_ANDROID
        if (settings.driver.empty()) {
            settings.driver = "vulkan"; // quicker than SDL's own choice there (OpenGL ES); a phone without it falls back to that
        }
#endif
        settings.vsync = live.vsync();
        settings.width = live.windowWidth;
        settings.height = live.windowHeight;
        settings.maximized = live.maximized;
        settings.minimized = m_options.background; // a window shown unfocused still lands on top in GNOME, so it starts in the dock
        settings.display = windowDisplayOf(live.display);
        settings.resizable = true;
        settings.pixelScale = live.pixelScale;
#ifdef SDL_PLATFORM_ANDROID
        // a phone: the window is the whole screen, and the game's screen fills its height
        settings.display = gfx::WindowDisplay::Borderless;
        settings.fitScreen = true;
#endif
        auto device = gfx::RenderDevice::create(settings);
        if (!device) {
            showError(std::string("cannot open the game window: ") + SDL_GetError());
            return 1;
        }
        if (!live.renderer.empty() && m_options.renderMode == gfx::RenderMode::Accelerated && device->backendName() != live.renderer) {
            // the saved renderer cannot start here (settings copied from another system): back to Default, in the file too
            core::logWarning("the saved renderer ", live.renderer, " is not available here: the setting is now Default");
            if (stored.renderer == live.renderer) {
                stored.renderer.clear();
                stored.save(extraFile);
            }
            live.renderer.clear();
        }
        // the renderer is known now; the text, the pictures and the workers are made after this and read the speedups then
        core::speedups() = live.speedupsFor(device->backendName());
        if (core::speedups().quickSdlChecks) {
            SDL_SetHint(SDL_HINT_INVALID_PARAM_CHECKS, "1");
        }
        core::logInfo("speedups: ", speedupModeName(live.speedupMode), " for ", device->backendName());
        device->setPixelArtFilter(formats::pixelArtFilterNamed(live.pixelArt).value_or(formats::PixelArtFilter::None));
        device->setPixelArtPart(gfx::PixelArtPart::Pictures, live.pixelArtPictures);
        device->setPixelArtPart(gfx::PixelArtPart::Sprites, live.pixelArtSprites);
        device->setPixelArtPart(gfx::PixelArtPart::Text, live.pixelArtText);

        const auto startupLines = core::readLines(game.data("Sinfo.txt"));
        const formats::StartupInfo startup = m_options.startup.value_or(formats::StartupInfo::parse(startupLines.value_or(std::vector<std::string>{})));
        const std::string smallFontFile = startup.simplified ? "tatpc1.jpc" : "tatpc1.twn";
        const std::string largeFontFile = startup.simplified ? "tatpc2.jpc" : "tatpc2.twn";
        const auto font = formats::BitmapFont::load(game.data("font/" + smallFontFile));
        if (!font) {
            showError("cannot read the font in " + game.data("font").string() + " (is --data the game folder?)");
            return 1;
        }
        auto text = gfx::TextRenderer::create(*device, *font);
        if (!text) {
            showError("cannot create the font texture");
            return 1;
        }

        auto largeFont = formats::LargeBitmapFont::load(game.data("font/" + largeFontFile));
        if (!largeFont) {
            showError("cannot read the large font in " + game.data("font").string());
            return 1;
        }
        gfx::LargeTextRenderer largeText(std::move(*largeFont));

        gfx::PictureLibrary pictures(*device);
        pictures.addDirectory(game.data("pic"));
        gfx::Canvas canvas(*device, pictures, *text, largeText);
        // exact 16-bit alpha fills read the screen back, so not on a renderer where that is slow
        const auto exactFillsFor = [&device](const ExtraSettings& video) { return !video.freeFrames() && video.pixelScale == 1 && device->readsBackQuickly(); };
        canvas.setExactAlphaFills(exactFillsFor(live));
        CursorSet cursors(game);
        const auto formulaFile = core::readFile(game.data("Data/Formula.dat"));
        const auto formula = formulaFile ? formats::FormulaData::parse(*formulaFile) : std::nullopt;
        if (!formula) {
            showError("cannot read " + game.data("Data/Formula.dat").string());
            return 1;
        }
        const auto itemFile = core::readFile(game.data("Data/Item.dat"));
        auto items = itemFile ? formats::ItemTable::parse(*itemFile) : std::nullopt;
        if (!items) {
            showError("cannot read " + game.data("Data/Item.dat").string());
            return 1;
        }
        const auto compoundFile = core::readFile(game.data("Data/Compound2.dat"));
        auto compounds = compoundFile ? formats::CompoundTable::parse(*compoundFile) : std::nullopt;
        if (!compounds) {
            showError("cannot read " + game.data("Data/Compound2.dat").string());
            return 1;
        }
        const auto npcFile = core::readFile(game.data("Data/Npc.dat"));
        auto npcs = npcFile ? formats::NpcTable::parse(*npcFile) : std::nullopt;
        if (!npcs) {
            showError("cannot read " + game.data("Data/Npc.dat").string());
            return 1;
        }
        const auto skillFile = core::readFile(game.data("Data/Skill.dat"));
        auto skills = skillFile ? formats::SkillTable::parse(*skillFile) : std::nullopt;
        if (!skills) {
            showError("cannot read " + game.data("Data/Skill.dat").string());
            return 1;
        }
        const auto motionFile = core::readFile(game.data("Data/SkillData.MBTM"));
        const auto motions = motionFile ? formats::MotionTable::parse(*motionFile) : std::nullopt;
        if (!motions) {
            showError("cannot read " + game.data("Data/SkillData.MBTM").string());
            return 1;
        }
        const auto talkFile = core::readFile(game.data("Data/Talk.Dat"));
        auto talks = talkFile ? formats::TalkTable::parse(*talkFile) : std::nullopt;
        if (!talks) {
            showError("cannot read " + game.data("Data/Talk.Dat").string());
            return 1;
        }
        const auto markFile = core::readFile(game.data("Data/Mark.Dat"));
        const auto marks = markFile ? formats::MarkTable::parse(*markFile) : std::nullopt;
        if (!marks) {
            showError("cannot read " + game.data("Data/Mark.Dat").string());
            return 1;
        }
        ModPacks mods = m_options.noMods ? ModPacks{} : ModPacks::load(m_options.modsDirectory);
        if (!mods.empty()) {
            mods.applyText(*items, *skills, *npcs, *talks, startup.simplified);
            mods.addPictures(pictures);
            game.setSoundPacks([&mods](const std::string& relative) { return mods.sound(relative); });
            const auto hdAtlas = [&mods](const std::string& fontFile) -> std::optional<formats::Image> {
                const auto file = mods.hdFont(fontFile);
                const auto data = file ? core::readFile(*file) : std::nullopt;
                return data ? formats::decodePng(*data) : std::nullopt;
            };
            if (const auto atlas = hdAtlas(smallFontFile); atlas && !text->useHdAtlas(*device, *atlas)) {
                core::logWarning("the HD font ", mods.hdFont(smallFontFile)->string(), " is not twice the size of the font's atlas: the font is used");
            }
            if (const auto atlas = hdAtlas(largeFontFile); atlas && !largeText.useHdAtlas(*atlas)) {
                core::logWarning("the HD font ", mods.hdFont(largeFontFile)->string(), " is not twice the size of the font's atlas: the font is used");
            }
        }
        for (const std::string& line : mods.report()) {
            core::logInfo("mods: ", line);
        }
        if (!m_options.modsReport.empty() && !core::writeLines(m_options.modsReport, mods.report())) {
            core::logWarning("cannot write the mods report ", m_options.modsReport.string());
        }
        RoleSpriteLibrary roleSprites(game);
        mods.addSprites(roleSprites);
        CharacterRenderer characters(roleSprites);
        std::optional<std::pair<std::filesystem::path, PhotoCheck>> photoRequest;
        std::optional<InputScript> script;
        if (!m_options.scriptFile.empty()) {
            script = InputScript::load(m_options.scriptFile);
            if (!script) {
                showError("cannot read the script " + m_options.scriptFile.string());
                return 1;
            }
        }
        auto played = std::make_unique<AccountBundle>(
            GameTables{game, startup, pictures, *formula, *items, *compounds, *npcs, *skills, *motions, *talks, *marks, characters},
            audioDevice
        );
        std::optional<ExtraSettings> videoChange;
        std::optional<std::string> scriptFailure;
        const auto setUpPlayed = [&](GameServices& services) {
            services.messageBox = [&script](const std::string& caption, const std::string& text) {
                core::logInfo("message box: ", caption, " / ", text);
                if (!script) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption.c_str(), text.c_str(), nullptr);
                }
            };
            services.errorShown = [&script, &scriptFailure](const std::string& text) {
                if (script && !scriptFailure) {
                    core::logError("script: ERROR ", text);
                    scriptFailure = text;
                }
            };
            services.saveScreenshot = [&photoRequest](const std::filesystem::path& file, const PhotoCheck& check) { photoRequest.emplace(file, check); };
            services.freeFrames = live.freeFrames();
            services.zoom.setGliding(live.freeFrames());
            services.renderDrivers = gfx::RenderDevice::driverNames(); // the loading screen drops the ones that cannot start here
            services.rendererInUse = device->backendName();
            services.desktopSize = [&device] { return device->usableDesktop(); };
            services.videoSettings = [&live] { return live; };
            services.changeVideo = [&videoChange](const ExtraSettings& changed) { videoChange = changed; };
        };
        setUpPlayed(played->services);
        if (live.loadingMusicVolume >= 0) {
            played->music.setVolume(live.loadingMusicOn ? SystemOptions::loudnessPercent(live.loadingMusicVolume) : 0);
            core::logInfo("loading music: volume ", live.loadingMusicVolume, live.loadingMusicOn ? "" : " (off)");
        }
        if (live.loadingSoundVolume >= 0) {
            played->sounds.setVolume(live.loadingSoundOn ? SystemOptions::loudnessPercent(live.loadingSoundVolume) : 0);
        }
        std::vector<PartyAccount> partyAccounts;
        if (!m_options.accountsFile.empty()) {
            std::string error;
            const auto lines = core::readLines(m_options.accountsFile);
            const auto accounts = lines ? parsePartyAccounts(*lines, error) : std::nullopt;
            if (!accounts) {
                showError("cannot read the accounts file " + m_options.accountsFile.string() + (error.empty() ? "" : ": " + error));
                return 1;
            }
            partyAccounts = *accounts;
        }
        PartyMembers party(played, audioDevice, setUpPlayed, std::move(partyAccounts));
        const auto applyView = [&](Scene& shown) {
            constexpr int kGameWidth = 800; // game screen width, px
            constexpr int kGameHeight = 600; // game screen height, px
            const core::Point origin{(device->width() - kGameWidth) / 2, (device->height() - kGameHeight) / 2};
            canvas.setOrigin(origin);
            played->services.view = canvas.window();
            played->zoom.setWindow(played->services.view);
            shown.screen().setView(played->services.view);
            shown.screen().setHudScale(static_cast<float>(live.hudScale) / 100.0f); // percent to factor
        };
        BeatClock beats;
        std::uint64_t clockFloorMs = 0;
        std::uint64_t lastBeatMs = 0;

        auto gpuTimer = gfx::GpuTimer::create(device->renderer(), device->backendName());
        core::logInfo("gpu timer: ", gpuTimer ? "available" : "none for this renderer");
        bool remakeGpuTimer = false;
        Gamepad gamepad(m_options.userDirectory, live.controller.enabled);
        PadControls padControls;
        const PadPointers padPointers(game, *device);
        padControls.setPointers(padPointers.above(), padPointers.below());
        HudReveal hudReveal;
        const auto dropGpuTimer = [&] {
            if (gpuTimer && device->backendName() == "direct3d") {
                gpuTimer.reset();
                remakeGpuTimer = true;
            }
        };
        FrameStats frameStats;
        std::optional<FrameLog> frameLog;
        if (!m_options.frameLogFile.empty()) {
            frameLog.emplace(m_options.frameLogFile);
            if (!frameLog->isOpen()) {
                core::logWarning("cannot write the frame log ", m_options.frameLogFile.string());
                frameLog.reset();
            }
        }
        int overlayLevel = live.perfOverlay; // F11 steps it for this run
        FrameGraph overlay;
        MachineLoad machineLoad;

        const bool screenKeyboard = typesOnScreenKeyboard();
        SDL_FPoint lastPress{0.0f, 0.0f}; // where the screen was last pressed, in the window: the place the screen keyboard must not cover
        if (!screenKeyboard) {
            SDL_StartTextInput(device->window());
        }
        TouchPointer touch; // fingers as the mouse, where the screen is what the game is played on
        touch.setEnabled(screenKeyboard);
        // whether a text field that can be typed in has the keyboard
        const auto typingIn = [](ui::Screen& screen) {
            const ui::TextBox* box = screen.textBoxOf(screen.focusedId());
            return box != nullptr && !box->isReadOnly();
        };
        const auto shownScene = [&]() -> Scene& {
            if (!party.started() || played->kind != SceneKind::World) {
                return *played->scene;
            }
            const auto& world = static_cast<const WorldScene&>(*played->scene);
            const auto member = party.commandingScene(world.inBattle() && world.battleActor() == 0);
            if (!member) {
                return *played->scene;
            }
            member->get().screen().setHudScale(static_cast<float>(live.hudScale) / 100.0f); // percent to factor
            return member->get();
        };
        const auto wheelZoom = [&](int steps) {
            if (played->kind == SceneKind::World) {
                for (int step = 0; step < std::abs(steps); ++step) {
                    if (steps > 0) {
                        played->zoom.zoomOut();
                    } else {
                        played->zoom.zoomIn();
                    }
                }
            }
        };
        int exitCode = 0;
        try {
            played->scene = makeScene(played->kind, played->services);
            applyView(*played->scene);
            bool running = true;
            const std::uint64_t scriptStartMs = core::nowMs();
            const auto worldReady = [&] {
                return played->kind == SceneKind::World && static_cast<const WorldScene&>(*played->scene).inWorld() && party.started() &&
                       party.allInWorld();
            };
            while (running && played->scene) {
                const std::uint64_t frameStart = std::max(core::nowMs(), clockFloorMs);
                played->zoom.setNow(frameStart);
                const double frameStartPrecise = core::nowPreciseMs();
                if (videoChange) {
                    const ExtraSettings changed = *videoChange;
                    videoChange.reset();
                    if (changed.freeFrames() != live.freeFrames()) {
                        beats = BeatClock{};
                        clockFloorMs = 0;
                        lastBeatMs = 0;
                        played->services.freeFrames = changed.freeFrames();
                        played->zoom.setGliding(changed.freeFrames());
                    }
                    if (changed.vsync() != live.vsync() || changed.windowWidth != live.windowWidth || changed.windowHeight != live.windowHeight ||
                        changed.maximized != live.maximized) {
                        dropGpuTimer();
                    }
                    if (changed.vsync() != live.vsync() && !device->setVsync(changed.vsync())) {
                        core::logWarning("the renderer cannot change vsync: ", SDL_GetError());
                    }
                    if (changed.windowWidth != live.windowWidth || changed.windowHeight != live.windowHeight || changed.maximized != live.maximized ||
                        changed.display != live.display) {
                        dropGpuTimer();
                        device->setWindowSize(changed.windowWidth, changed.windowHeight, changed.maximized);
                        if ((changed.display != live.display || changed.display == ExtraSettings::Display::Fullscreen) &&
                            !device->setDisplay(windowDisplayOf(changed.display), changed.windowWidth, changed.windowHeight)) {
                            core::logWarning("cannot change the display mode: ", SDL_GetError());
                        }
                    }
                    if (changed.perfOverlay != live.perfOverlay) {
                        overlayLevel = changed.perfOverlay;
                    }
                    if (changed.pixelScale != live.pixelScale) {
                        device->setPixelScale(changed.pixelScale);
                    }
                    canvas.setExactAlphaFills(exactFillsFor(changed));
                    core::speedups().limiterSpinMs = changed.speedups.limiterSpinMs; // the other speedups are read at the start only
                    live = changed;
                    applyView(*played->scene);
                    stored = changed;
                    stored.save(extraFile);
                    core::logInfo(
                        "video settings: frames ",
                        frameModeName(live.frames),
                        ", window ",
                        live.windowWidth,
                        "x",
                        live.windowHeight,
                        live.maximized ? " maximized" : "",
                        live.rememberWindow ? " (remembered)" : "",
                        ", ",
                        displayModeName(live.display),
                        ", scale ",
                        live.pixelScale,
                        "x, renderer ",
                        live.renderer.empty() ? "default" : live.renderer
                    );
                }
                const bool measuring = overlayLevel > 0 || frameLog;
                // the frame rate alone needs no graphics card timing (on Direct3D 9 the timing itself costs a few percent)
                const bool timingGpu = overlayLevel >= 2 || frameLog;
                machineLoad.setWanted(overlayLevel >= 2);
                if (overlayLevel >= 2) {
                    machineLoad.update(frameStartPrecise);
                }
                FrameSample sample;
                SDL_Event event;
                // a party row clicked last frame swaps the played account here, before any reference to its scene is taken for this frame
                if (party.applySelection()) {
                    applyView(*played->scene);
                }
                Scene& steered = shownScene();
                padControls.update(frameStartPrecise, gamepad, device->window(), steered, canvas.origin(), live.controller.hideSeconds);
                hudReveal.update(
                    frameStartPrecise,
                    played->scene->screen(),
                    live.controller.hideHud && padControls.wantsHudHidden(),
                    live.controller.hideStatus,
                    live.controller.hideChat,
                    padControls.wantsShortcutBar()
                );
                touch.update(device->window(), frameStartPrecise);
                while (SDL_PollEvent(&event)) {
                    if (touch.seeEvent(event, device->window(), frameStartPrecise)) {
                        continue;
                    }
                    padControls.seeEvent(event, steered.screen());
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                        lastPress = {event.button.x, event.button.y};
                    }
                    if (event.type == SDL_EVENT_DID_ENTER_BACKGROUND || event.type == SDL_EVENT_DID_ENTER_FOREGROUND) {
                        // a phone put the game away or brought it back; SDL holds this loop (and the sound) in between
                        core::logInfo(event.type == SDL_EVENT_DID_ENTER_BACKGROUND ? "the app went to the background" : "the app is back in the foreground");
                        device->presentEverything();
                        continue;
                    }
                    SDL_ConvertEventToRenderCoordinates(device->renderer(), &event);
                    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11 && !event.key.repeat) {
                        overlayLevel = (overlayLevel + 1) % FrameGraph::kLevels; // off, 1, 2, 3, off
                        continue;
                    }
                    if (gamepad.handleEvent(event)) {
                        continue;
                    }
                    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
                        device->presentEverything(); // uncovered, moved, resized, shown: the screen may not hold the last frame any more
                    }
                    if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                        dropGpuTimer();
                        device->resize(event.window.data1, event.window.data2);
                        if (!device->isMaximized() && !device->isFullscreen()) {
                            live.windowWidth = event.window.data1;
                            live.windowHeight = event.window.data2;
                        }
                        if (!device->isFullscreen()) {
                            live.maximized = device->isMaximized();
                        }
                        applyView(*played->scene);
                        continue;
                    }
                    if (party.handleEvent(
                            event,
                            canvas.origin(),
                            static_cast<float>(live.hudScale) / 100.0f,
                            &steered != played->scene.get()
                        )) { // hudScale 100 = 1.0 (percent to factor)
                        continue;
                    }
                    running = forwardEvent(event, steered.screen(), canvas.origin()) && running;
                    if (screenKeyboard && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
                        typingIn(steered.screen()) && !SDL_TextInputActive(device->window())) {
                        // a text field was pressed: the screen keyboard comes up, and the system keeps the pressed place in sight
                        constexpr int kFieldWidth = 200; // px, about a text field around the pressed place
                        constexpr int kFieldHeight = 32; // px
                        const SDL_Rect field{static_cast<int>(lastPress.x) - kFieldWidth / 2, static_cast<int>(lastPress.y) - kFieldHeight / 2, kFieldWidth, kFieldHeight};
                        SDL_SetTextInputArea(device->window(), &field, 0);
                        SDL_StartTextInput(device->window());
                    }
                    if (event.type == SDL_EVENT_MOUSE_WHEEL && (SDL_GetModState() & SDL_KMOD_CTRL) != 0 && event.wheel.integer_y != 0) {
                        wheelZoom(event.wheel.integer_y);
                    }
                }
                if (const int steps = touch.takeZoomSteps(); steps != 0) {
                    wheelZoom(steps);
                }
                if (screenKeyboard && SDL_TextInputActive(device->window()) && !typingIn(steered.screen())) {
                    SDL_StopTextInput(device->window()); // no text field has the keyboard any more: the screen keyboard goes away
                }
                const bool beat = !live.freeFrames() || beats.due(frameStart);
                if (beat && script) {
                    script->step(
                        steered.screen(),
                        frameStart,
                        [&played](const core::Bytes& payload) { played->connection.inject(payload); },
                        steered.scriptProbe(),
                        InputScript::Host{played->scene->scriptLogin(), worldReady}
                    );
                    running = running && !script->wantsQuit();
                    if (const auto steps = script->takeCtrlWheel()) {
                        wheelZoom(*steps);
                    }
                }
                if (script && scriptFailure) {
                    exitCode = kScriptErrorExit;
                    running = false;
                }
                if (script && m_options.scriptTimeoutSeconds > 0 &&
                    core::nowMs() - scriptStartMs > static_cast<std::uint64_t>(m_options.scriptTimeoutSeconds) * 1000) { // s to ms
                    core::logError("script: ERROR the script ran longer than --script-timeout ", m_options.scriptTimeoutSeconds, " s");
                    exitCode = kScriptTimeoutExit;
                    running = false;
                }
                const double updateStart = core::nowPreciseMs();
                sample.eventsMs = updateStart - frameStartPrecise;
                if (beat) {
                    played->music.update();
                    if (!party.started() && played->kind == SceneKind::World) {
                        party.start();
                    }
                    std::optional<std::reference_wrapper<const WorldScene>> playedWorld;
                    if (played->kind == SceneKind::World) {
                        playedWorld = std::cref(static_cast<const WorldScene&>(*played->scene));
                    }
                    party.update(frameStart, playedWorld);
                    if (played->kind == SceneKind::World) {
                        auto& world = static_cast<WorldScene&>(*played->scene);
                        for (const PartyChatLine& line : party.takeChatLines(frameStart)) {
                            world.addPartyLine(line.channel, line.line, line.member);
                        }
                        for (const PartyMessage& message : party.takeMessages()) {
                            world.addPartyMessage(message.centre, message.text, message.durationMs);
                        }
                    } else {
                        party.takeMessages();
                    }
                    played->scene->update(frameStart);
                    pictures.collect(frameStart);
                    if (const auto next = played->scene->nextScene()) {
                        played->kind = *next;
                        played->zoom.reset();
                        played->scene = makeScene(*next, played->services);
                        if (!played->scene) {
                            break;
                        }
                        party.playedSceneChanged();
                        applyView(*played->scene);
                        if (*next == SceneKind::World) {
                            party.logInGone(); // members cut off with the old login follow the new one
                        }
                        continue;
                    }
                    if (live.freeFrames()) {
                        clockFloorMs = std::max(clockFloorMs, played->scene->heldUntilMs());
                        beats.deferPast(clockFloorMs);
                    }
                }
                played->services.nextBeatMs = beats.nextMs();
                party.setNextBeat(played->services.nextBeatMs);
                played->scene->move(frameStart);
                party.move(frameStart);
                if (!beat && !played->scene->freeFrames()) {
                    sample.updateMs = core::nowPreciseMs() - updateStart;
                    waitForBeat(beats.nextMs());
                    if (measuring) {
                        sample.totalMs = core::nowPreciseMs() - frameStartPrecise;
                        sample.waitMs = sample.totalMs - sample.eventsMs - sample.updateMs;
                        frameStats.add(sample);
                        if (frameLog) {
                            frameLog->add(sample);
                        }
                    }
                    continue;
                }
                const std::uint64_t drawStart = core::nowMs();
                const double drawStartPrecise = core::nowPreciseMs();
                sample.updateMs = drawStartPrecise - updateStart;
                if (timingGpu && gpuTimer) {
                    gpuTimer->beginFrame();
                }
                device->clear();
                canvas.setAnimationFrame(beat);
                if (beat) {
                    lastBeatMs = frameStart;
                }
                if (live.freeFrames()) {
                    const std::uint64_t next = beats.nextMs();
                    const double amount = next > lastBeatMs && frameStart > lastBeatMs
                                              ? std::min(1.0, static_cast<double>(frameStart - lastBeatMs) / static_cast<double>(next - lastBeatMs))
                                              : 0.0;
                    canvas.setBetweenBeats(amount, lastBeatMs);
                } else {
                    canvas.setBetweenBeats(std::nullopt, frameStart);
                }
                Scene& shown = shownScene();
                const bool playedShown = &shown == played->scene.get();
                if (playedShown) {
                    party.holdPlayedPopups();
                }
                shown.draw(canvas, frameStart);
                if (playedShown) {
                    party.drawWindows(canvas, frameStart, static_cast<float>(live.hudScale) / 100.0f); // percent to factor
                }
                if (party.started() && played->kind == SceneKind::World) {
                    party.drawPanels(
                        canvas,
                        static_cast<float>(live.hudScale) / 100.0f,
                        static_cast<const WorldScene&>(*played->scene),
                        shown.screen(),
                        frameStart
                    ); // hudScale 100 = 1.0 (percent to factor)
                }
                if (script) {
                    if (const auto shot = script->takeScreenshotRequest()) {
                        device->saveScreenshot(game.user(*shot).string());
                    }
                }
                if (photoRequest) {
                    const core::Point origin = canvas.origin();
                    ScreenPhoto::save(
                        photoRequest->first,
                        device->readRegion({origin.x, origin.y, 800, 600}), // 800x600: game screen size, px
                        photoRequest->second,
                        std::time(nullptr)
                    ); // 800 x 600: the game's screen size
                    photoRequest.reset();
                }
                if (overlayLevel > 0) {
                    const core::Point origin = canvas.origin();
                    canvas.setOrigin({});
                    // under the function buttons where they are now: at the top while they are slid away or hidden (a fight)
                    int overlayTop = 0;
                    if (const auto bar = steered.screen().screenRectOf("FunctionBackground"); bar && !steered.screen().layoutAway("FunctionButtons")) {
                        overlayTop = std::max(0, bar->y + bar->height + steered.screen().layoutSlide("FunctionButtons").y + origin.y);
                    }
                    overlay.draw(canvas, frameStats, device->backendName(), gpuTimer != nullptr, device->width(), overlayTop, overlayLevel, machineLoad.reading(), frameStart);
                    canvas.setOrigin(origin);
                }
                if (timingGpu && gpuTimer) {
                    gpuTimer->endFrame();
                }
                const double presentStart = core::nowPreciseMs();
                sample.drawMs = presentStart - drawStartPrecise;
                device->present();
                if (remakeGpuTimer) {
                    gpuTimer = gfx::GpuTimer::create(device->renderer(), device->backendName());
                    remakeGpuTimer = false;
                }
                cursors.setScale(device->cursorScale(), device->pixelArtFilter(), frameStart);
                cursors.show(shown.screen().cursor(), frameStart);
                cursors.update(frameStart);
                const double waitStart = core::nowPreciseMs();
                sample.presentMs = waitStart - presentStart;

                if (live.freeFrames()) {
                    if (!played->scene->freeFrames()) {
                        waitForBeat(beats.nextMs());
                    } else if (live.frameCap() > 0) {
                        waitUntil(frameStartPrecise + 1000.0 / live.frameCap()); // 1000 ms per second
                    }
                } else {
                    if (const std::uint64_t pad = FrameCost::padMs(core::nowMs() - drawStart); pad > 0) {
                        SDL_Delay(static_cast<Uint32>(pad));
                    }
                    const std::uint64_t flipped = core::nowMs();
                    const std::uint64_t blank = VerticalBlank::waitMs(flipped, device->refreshRate());
                    if (const std::uint64_t wait = blank + FrameBeat::waitMs(flipped + blank - frameStart); wait > 0) {
                        SDL_Delay(static_cast<Uint32>(wait));
                    }
                }
                if (measuring) {
                    const double frameEnd = core::nowPreciseMs();
                    sample.waitMs = frameEnd - waitStart;
                    sample.totalMs = frameEnd - frameStartPrecise;
                    if (timingGpu && gpuTimer) {
                        sample.gpuMs = gpuTimer->latestMs();
                    }
                    frameStats.add(sample);
                    if (frameLog) {
                        frameLog->add(sample);
                    }
                }
            }
        } catch (const std::exception& error) {
            showError(error.what());
            return 1;
        }
        played->connection.close();
        if (stored.rememberWindow) {
            stored.windowWidth = live.windowWidth;
            stored.windowHeight = live.windowHeight;
            stored.maximized = live.maximized;
            stored.save(extraFile);
        }
        if (exitCode != 0) {
            core::logInfo("closed by the script's failure, exit code ", exitCode);
            return exitCode;
        }
        core::logInfo("closed normally");
        return 0;
    }

}
