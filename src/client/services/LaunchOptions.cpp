#include "client/services/LaunchOptions.h"

#include "core/StringUtil.h"
#include "formats/PixelArtScale.h"

#include <algorithm>
#include <cstdlib>
#include <system_error>

namespace client {

    namespace {

#ifdef _WIN32

        /// @brief Reads a folder from an environment variable, wide, so a folder name of any language works.
        /// @param name The variable name.
        /// @return The folder, or empty when the variable is not set.
        std::filesystem::path environmentFolder(const wchar_t* name) {
            wchar_t* value = nullptr;
            std::size_t length = 0;
            std::filesystem::path folder;
            if (_wdupenv_s(&value, &length, name) == 0 && value != nullptr && *value != L'\0') {
                folder = value;
            }
            std::free(value);
            return folder;
        }
#endif

    }

    std::vector<std::filesystem::path> LaunchOptions::defaultInstallLocations() {
#ifdef _WIN32
        std::filesystem::path base = environmentFolder(L"ProgramFiles(x86)");
        if (base.empty()) {
            base = "C:/Program Files (x86)";
        }
        return {base / "Wonderland Online"};
#else
        return {};
#endif
    }

    std::filesystem::path LaunchOptions::ownUserDirectory() {
#ifdef _WIN32
        const std::filesystem::path appData = environmentFolder(L"LOCALAPPDATA");
        return appData.empty() ? std::filesystem::path() : appData / "Wonderland Online" / "user";
#else
        return {};
#endif
    }

    std::optional<LaunchOptions> LaunchOptions::parse(
        const std::vector<std::string>& arguments,
        const std::filesystem::path& programDirectory,
        std::string& error,
        const std::vector<std::filesystem::path>& installLocations
    ) {
        LaunchOptions options;
        if (arguments.size() == 3 && // the launcher's three numbers: SCREEN LANGUAGE MEMORY
            std::none_of(arguments.begin(), arguments.end(), [](const std::string& argument) { return argument.rfind("--", 0) == 0; })) {
            options.startup = formats::StartupInfo::fromArguments(arguments[0], arguments[1], arguments[2]);
        }
        for (std::size_t i = options.startup ? arguments.size() : 0; i < arguments.size(); ++i) {
            const std::string& argument = arguments[i];
            const bool hasValue = i + 1 < arguments.size();
            if (argument == "--data" && hasValue) {
                options.dataDirectory = arguments[++i];
            } else if (argument == "--user" && hasValue) {
                options.userDirectory = arguments[++i];
                options.userGiven = true;
            } else if (argument == "--views" && hasValue) {
                options.layoutDirectory = arguments[++i];
            } else if (argument == "--script" && hasValue) {
                options.scriptFile = arguments[++i];
            } else if (argument == "--script-timeout" && hasValue) {
                const auto seconds = core::parseInt(arguments[++i]);
                if (!seconds || *seconds <= 0) {
                    error = "--script-timeout takes a number of seconds";
                    return std::nullopt;
                }
                options.scriptTimeoutSeconds = static_cast<int>(*seconds);
            } else if (argument == "--renderer" && hasValue) {
                options.rendererName = arguments[++i];
            } else if (argument == "--perf") {
                options.showFrameGraph = true;
            } else if (argument == "--perf-log" && hasValue) {
                options.frameLogFile = arguments[++i];
            } else if (argument == "--fps" && hasValue) {
                const std::string& rate = arguments[++i];
                options.freeFrames = true;
                options.framesGiven = true;
                if (rate == "vsync") {
                    options.vsync = true;
                } else if (rate != "unlimited") {
                    const auto cap = core::parseInt(rate);
                    if (!cap || *cap < 1) {
                        error = "--fps takes unlimited, vsync or a number of frames a second";
                        return std::nullopt;
                    }
                    options.fpsCap = static_cast<int>(*cap);
                }
            } else if (argument == "--window" && hasValue) {
                const std::string& size = arguments[++i];
                options.windowGiven = true;
                if (size == "last") {
                    options.rememberWindow = true;
                    continue;
                }
                const auto cross = size.find('x');
                const auto width = cross != std::string::npos ? core::parseInt(size.substr(0, cross)) : std::nullopt;
                const auto height = cross != std::string::npos ? core::parseInt(size.substr(cross + 1)) : std::nullopt;
                if (!width || !height || *width < 800 || *height < 600) { // minimum window size 800x600 (the game's screen)
                    error = "--window takes a size of at least 800x600 (WIDTHxHEIGHT) or last";
                    return std::nullopt;
                }
                options.windowWidth = static_cast<int>(*width);
                options.windowHeight = static_cast<int>(*height);
            } else if (argument == "--scale" && hasValue) {
                const auto scale = core::parseInt(arguments[++i]);
                if (!scale || *scale < 1 || *scale > 3) { // pixel scale range 1 to 3
                    error = "--scale takes 1, 2 or 3";
                    return std::nullopt;
                }
                options.pixelScale = static_cast<int>(*scale);
            } else if (argument == "--pixel-art" && hasValue) {
                options.pixelArt = arguments[++i];
                if (!formats::pixelArtFilterNamed(options.pixelArt)) {
                    error = "--pixel-art takes off, scale or xbr";
                    return std::nullopt;
                }
            } else if (argument == "--hud-scale" && hasValue) {
                const auto scale = core::parseInt(arguments[++i]);
                if (!scale || *scale < 100 || *scale > 200) { // HUD scale percent range 100 to 200
                    error = "--hud-scale takes a percent from 100 to 200";
                    return std::nullopt;
                }
                options.hudScale = static_cast<int>(*scale);
            } else if (argument == "--accounts" && hasValue) {
                options.accountsFile = arguments[++i];
            } else if (argument == "--display" && hasValue) {
                options.display = arguments[++i];
                if (options.display != "windowed" && options.display != "borderless" && options.display != "fullscreen") {
                    error = "--display takes windowed, borderless or fullscreen";
                    return std::nullopt;
                }
            } else if (argument == "--pixel-art-skip" && hasValue) {
                const std::string list = arguments[++i];
                std::size_t start = 0;
                while (start <= list.size()) {
                    const std::size_t comma = std::min(list.find(',', start), list.size());
                    const std::string part = list.substr(start, comma - start);
                    if (part != "pictures" && part != "sprites" && part != "text" && part != "none") {
                        error = "--pixel-art-skip takes a comma list of pictures, sprites, text, or none";
                        return std::nullopt;
                    }
                    start = comma + 1;
                }
                options.pixelArtSkip = list;
            } else if (argument == "--mods" && hasValue) {
                options.modsDirectory = arguments[++i];
            } else if (argument == "--no-mods") {
                options.noMods = true;
            } else if (argument == "--mods-report" && hasValue) {
                options.modsReport = arguments[++i];
            } else if (argument == "--background") {
                options.background = true;
            } else if (argument == "--software") {
                options.renderMode = gfx::RenderMode::Software;
            } else {
                error = "unknown argument " + argument;
                return std::nullopt;
            }
        }
        std::error_code ignored;
        if (options.dataDirectory.empty()) {
            options.dataDirectory = programDirectory;
            if (!std::filesystem::is_directory(programDirectory / "data", ignored)) {
                const auto installed = std::find_if(installLocations.begin(), installLocations.end(), [&](const std::filesystem::path& place) {
                    return std::filesystem::is_directory(place / "data", ignored);
                });
                if (installed != installLocations.end()) {
                    options.dataDirectory = *installed;
                }
            }
        }
        if (options.modsDirectory.empty()) {
            options.modsDirectory = programDirectory / "mods";
        }
        if (options.userDirectory.empty()) {
            options.userDirectory = options.dataDirectory / "user";
        }
        return options;
    }

}
