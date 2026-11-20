#ifndef NORTH_ISLAND_CLIENT_SERVICES_SETTINGS_QOL_SETTINGS_H
#define NORTH_ISLAND_CLIENT_SERVICES_SETTINGS_QOL_SETTINGS_H

#include "core/Speedups.h"

#include <filesystem>
#include <optional>
#include <string>

namespace client {

    struct LaunchOptions;

    /// @brief Tells whether game pads are used unless the settings say otherwise: not on Windows before Vista, where they are untried.
    /// @return True where game pads are on by default.
    bool controllerOnByDefault();

    /// @brief The qol client's own settings (user\extra-settings.json), the ones the original never had: frame pacing, frame graph, window and renderer.
    /// @details Defaults are the original's (a 30 ms beat, an 800 x 600 window, SDL's renderer). The command line overrides them for one run; the first run writes the file from what it ran with.
    struct ExtraSettings {
        /// @brief How frames are paced.
        enum class Frames { Original, Vsync, Unlimited, Capped };

        /// @brief How the window is shown.
        enum class Display { Windowed, Borderless, Fullscreen };

        /// @brief Which speedups run: none, the recommended ones for the renderer, or the player's own choice.
        enum class SpeedupMode { None, Auto, Custom };

        /// @brief Play with a game pad ("controller" in the file).
        struct Controller {
            bool enabled = controllerOnByDefault(); // game pads are looked for and used
            bool hideHud = true; // the HUD slides away while the pad is in use
            bool hideStatus = true; // the status panel goes with it
            bool hideChat = true; // the chat box goes with it
            int hideSeconds = 10; // how long after the last use of the menus the HUD hides; 0: only when Start sends it away

            /// @brief Compares all settings.
            /// @param other The settings to compare with.
            /// @return True when every setting matches.
            bool operator==(const Controller& other) const {
                return enabled == other.enabled && hideHud == other.hideHud && hideStatus == other.hideStatus && hideChat == other.hideChat &&
                       hideSeconds == other.hideSeconds;
            }
        };

        /// @brief The experimental lighting of the map and the battles ("lighting" in the file); its looks are in lighting.json.
        struct Lighting {
            bool enabled = false; // anything is lit at all
            bool shadows = true; // figures and objects cast shadows
            bool clouds = true; // the drifting shade of clouds and leaves
            bool air = true; // specks in the air, shafts of light, glints on water
            bool occlusion = true; // the ground is darker right where a figure or an object stands on it
            bool shadows3d = false; // figures' shadows are cast from a rough 3D shape worked out of their sprites (else from the flat sprites)

            /// @brief Compares all settings.
            /// @param other The settings to compare with.
            /// @return True when every setting matches.
            bool operator==(const Lighting& other) const {
                return enabled == other.enabled && shadows == other.shadows && clouds == other.clouds && air == other.air && occlusion == other.occlusion && shadows3d == other.shadows3d;
            }
        };

        Frames frames = Frames::Original;
        int fpsCap = 144; // default frame cap in frames per second
        int perfOverlay = 0; // the performance overlay: 0 off, 1 the frame rate, 2 with processor, graphics card and RAM use and the times, 3 with the graph
        int windowWidth = 800; // the original's window width in pixels
        int windowHeight = 600; // the original's window height in pixels
        bool maximized = false;
        bool rememberWindow = false;
        Display display = Display::Windowed;
        std::string renderer;
        int pixelScale = 1;
        int hudScale = 100; // HUD size in percent (100 = original)
        std::string pixelArt = "off";
        bool pixelArtPictures = true;
        bool pixelArtSprites = true;
        bool pixelArtText = true;
        int loadingMusicVolume = -1; // -1: full loudness as the original
        bool loadingMusicOn = true;
        int loadingSoundVolume = -1; // -1: full loudness as the original
        bool loadingSoundOn = true;
        bool sortButtons = false;
        bool partyLogin = false;
        bool modernBag = false; // things in the bag and Storeroom move as in Minecraft: a click carries a stack, a right click takes half or puts one down, a held button spreads, Shift quick-moves
#ifdef __ANDROID__
        SpeedupMode speedupMode = SpeedupMode::Auto; // a phone starts with the recommended speedups
#else
        SpeedupMode speedupMode = SpeedupMode::None; // which speedups run ("mode" under "speedups" in the file)
#endif
        Controller controller;
        Lighting lighting;
        core::Speedups speedups; // the player's own choice (used in SpeedupMode::Custom) and, in every mode, the busy wait

        /// @brief Tells whether frames are paced by anything but the original's beat.
        /// @return True unless the mode is Frames::Original.
        bool freeFrames() const { return frames != Frames::Original; }

        /// @brief Tells whether frames wait for the display's vertical sync.
        /// @return True in Frames::Vsync.
        bool vsync() const { return frames == Frames::Vsync; }

        /// @brief Gives the frames a second the loop waits for.
        /// @return The cap in Frames::Capped, else 0 (none).
        int frameCap() const { return frames == Frames::Capped ? fpsCap : 0; }

        /// @brief Gives the speedups that run with a renderer: none, the recommended ones or the player's own, by the mode.
        /// @details The busy wait (limiterSpinMs) is the player's in every mode. Moving the picture on scroll is off in
        /// every mode where the desktop is not composited (gfx::RenderDevice::desktopIsComposited).
        /// @param renderer The renderer's name ("direct3d11", "software", ...).
        /// @return The speedups.
        core::Speedups speedupsFor(const std::string& renderer) const;

        /// @brief Gives the path of the settings file.
        /// @param userRoot The user folder.
        /// @return The path of extra-settings.json in it.
        static std::filesystem::path fileIn(const std::filesystem::path& userRoot);

        /// @brief Reads the settings file; without one the old qol-settings.json, then the older qol-window.ini (--window last), is taken over.
        /// @param userRoot The user folder.
        /// @return The settings, or nothing when neither file is there.
        static std::optional<ExtraSettings> loadIn(const std::filesystem::path& userRoot);

        /// @brief Parses the settings JSON; keys missing or of the wrong kind keep their defaults.
        /// @param json The text.
        /// @return The settings, or nothing when the text is not a JSON object.
        static std::optional<ExtraSettings> parse(const std::string& json);

        /// @brief Writes the settings as JSON text.
        /// @return The text.
        std::string toJson() const;

        /// @brief Saves the settings to a file, creating its folder.
        /// @param file The file to write.
        /// @return True when the file was written.
        bool save(const std::filesystem::path& file) const;

        /// @brief Replaces the settings with what was given on the command line.
        /// @param options The launch options.
        void override(const LaunchOptions& options);

        /// @brief Compares all settings.
        /// @param other The settings to compare with.
        /// @return True when every setting matches.
        bool operator==(const ExtraSettings& other) const;

        /// @brief Compares all settings for difference.
        /// @param other The settings to compare with.
        /// @return True when any setting differs.
        bool operator!=(const ExtraSettings& other) const { return !(*this == other); }
    };

    /// @brief Gives the speedups recommended for a renderer on this machine (the busy wait is left at its default).
    /// @details Every renderer: text pictures, quick SDL checks, pictures on first use, map backgrounds in parts and
    /// remembered renderer checks. The software renderer also gets its own ones (moving the picture on scroll only where the
    /// desktop is composited). Quiet workers only with one processor core.
    /// @param renderer The renderer's name.
    /// @return The speedups.
    core::Speedups recommendedSpeedups(const std::string& renderer);

    /// @brief Gives the name of a speedup mode as written in the settings file.
    /// @param mode The mode.
    /// @return "none", "auto" or "custom".
    const char* speedupModeName(ExtraSettings::SpeedupMode mode);

    /// @brief Gives the name of a frame mode as written in the settings file.
    /// @param frames The mode.
    /// @return "original", "vsync", "unlimited" or "cap".
    const char* frameModeName(ExtraSettings::Frames frames);

    /// @brief Gives the name of a display mode as written in the settings file.
    /// @param display The mode.
    /// @return "windowed", "borderless" or "fullscreen".
    const char* displayModeName(ExtraSettings::Display display);

}

#endif
