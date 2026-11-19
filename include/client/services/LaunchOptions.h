#ifndef NORTH_ISLAND_CLIENT_SERVICES_LAUNCH_OPTIONS_H
#define NORTH_ISLAND_CLIENT_SERVICES_LAUNCH_OPTIONS_H

#include "formats/StartupInfo.h"
#include "gfx/RenderMode.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace client {

    /// @brief The parsed command line (see the option list above).
    struct LaunchOptions {
        std::filesystem::path dataDirectory;
        std::filesystem::path userDirectory;
        bool userGiven = false;
        std::filesystem::path layoutDirectory;
        gfx::RenderMode renderMode = gfx::RenderMode::Accelerated;
        std::filesystem::path scriptFile;
        bool background = false; // test runs: the window opens minimized without taking focus, and no sound plays
        int scriptTimeoutSeconds = 0; // --script-timeout: the program ends with an error when the script runs longer (0: no limit)
        std::string rendererName;
        bool showFrameGraph = false;
        std::filesystem::path frameLogFile;
        bool freeFrames = false;
        bool vsync = false;
        int fpsCap = 0;
        int windowWidth = 800; // window width, px (the original screen width)
        int windowHeight = 600; // window height, px (the original screen height)
        bool rememberWindow = false;
        int pixelScale = 0;
        int hudScale = 0;
        std::string pixelArt;
        std::string display;
        std::filesystem::path accountsFile;
        std::optional<std::string> pixelArtSkip;
        bool framesGiven = false;
        bool windowGiven = false;
        std::filesystem::path modsDirectory;
        bool noMods = false;
        std::filesystem::path modsReport;
        std::optional<formats::StartupInfo> startup;

        /// @brief Parses the command line; the program is a drop-in for the original aLogin.exe (launcher numbers replace Sinfo.txt).
        /// @param arguments The command line arguments without the program name.
        /// @param programDirectory The folder the program is in.
        /// @param error Receives a message when the arguments are wrong.
        /// @param installLocations Folders searched for the game when the program is not in it.
        /// @return The options, or nothing (and a message in error) when the arguments are wrong.
        static std::optional<LaunchOptions> parse(
            const std::vector<std::string>& arguments,
            const std::filesystem::path& programDirectory,
            std::string& error,
            const std::vector<std::filesystem::path>& installLocations = defaultInstallLocations()
        );

        /// @brief Where the game's installer puts it (on Windows %ProgramFiles(x86)%\Wonderland Online; elsewhere none).
        /// @return The candidate install folders.
        static std::vector<std::filesystem::path> defaultInstallLocations();

        /// @brief The player's own user folder for a game folder that cannot be written to (on Windows %LOCALAPPDATA%\Wonderland Online\user; elsewhere none).
        /// @return The folder, or empty.
        static std::filesystem::path ownUserDirectory();
    };

}

#endif
