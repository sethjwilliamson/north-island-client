#include "client/services/settings/ExtraSettings.h"

#include "client/services/LaunchOptions.h"
#include "client/services/settings/WindowMemory.h"
#include "core/Bytes.h"
#include "core/FileSystem.h"
#include "formats/PixelArtScale.h"
#include "gfx/RenderDevice.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <system_error>
#include <thread>
#include <utility>

namespace client {

    namespace {

        using Json = nlohmann::json;
        const char* const kOldFile = "qol-settings.json"; // the file's name before it was extra-settings.json, read once to migrate

        /// @brief Reads a boolean key into a value, leaving it alone when the key is missing or of another kind.
        /// @param object The JSON object.
        /// @param key The key.
        /// @param value The value to set.
        void readBool(const Json& object, const char* key, bool& value) {
            if (const auto found = object.find(key); found != object.end() && found->is_boolean()) {
                value = found->get<bool>();
            }
        }

        /// @brief Reads an integer key into a value clamped to a range, leaving it alone when the key is missing or of another kind.
        /// @param object The JSON object.
        /// @param key The key.
        /// @param value The value to set.
        /// @param least The smallest accepted value.
        /// @param most The largest accepted value.
        void readInt(
            const Json& object,
            const char* key,
            int& value,
            int least,
            int most
        ) {
            if (const auto found = object.find(key); found != object.end() && found->is_number_integer()) {
                value = static_cast<int>(std::clamp<long long>(found->get<long long>(), least, most));
            }
        }

        /// @brief Reads a string key into a value, leaving it alone when the key is missing or of another kind.
        /// @param object The JSON object.
        /// @param key The key.
        /// @param value The value to set.
        void readString(const Json& object, const char* key, std::string& value) {
            if (const auto found = object.find(key); found != object.end() && found->is_string()) {
                value = found->get<std::string>();
            }
        }

        /// @brief Reads a number key into a value clamped to a range, leaving it alone when the key is missing or of another kind.
        /// @param object The JSON object.
        /// @param key The key.
        /// @param value The value to set.
        /// @param least The smallest accepted value.
        /// @param most The largest accepted value.
        void readNumber(
            const Json& object,
            const char* key,
            double& value,
            double least,
            double most
        ) {
            if (const auto found = object.find(key); found != object.end() && found->is_number()) {
                value = std::clamp(found->get<double>(), least, most);
            }
        }

        constexpr double kMostSpinMs = 5.0; // the longest busy wait before a capped frame, ms

        /// @brief The lighting's switches by their names in the settings file.
        constexpr std::array<std::pair<const char*, bool ExtraSettings::Lighting::*>, 6> kLighting{{
            {"enabled", &ExtraSettings::Lighting::enabled},
            {"shadows", &ExtraSettings::Lighting::shadows},
            {"clouds", &ExtraSettings::Lighting::clouds},
            {"air", &ExtraSettings::Lighting::air},
            {"occlusion", &ExtraSettings::Lighting::occlusion},
            {"shadows3d", &ExtraSettings::Lighting::shadows3d}
        }};

        /// @brief The on / off speedups by their names in the settings file.
        constexpr std::array<std::pair<const char*, bool core::Speedups::*>, 9> kSpeedups{{
            {"softwareEncodedTextures", &core::Speedups::softwareEncodedTextures},
            {"softwareChangedAreas", &core::Speedups::softwareChangedAreas},
            {"softwareScroll", &core::Speedups::softwareScroll},
            {"textPictures", &core::Speedups::textPictures},
            {"quickSdlChecks", &core::Speedups::quickSdlChecks},
            {"picturesOnFirstUse", &core::Speedups::picturesOnFirstUse},
            {"mapInParts", &core::Speedups::mapInParts},
            {"quietWorkers", &core::Speedups::quietWorkers},
            {"rememberRendererChecks", &core::Speedups::rememberRendererChecks}
        }};

        /// @brief Finds the frame mode with a name.
        /// @param name The name as written in the settings file.
        /// @return The mode, or nothing for an unknown name.
        std::optional<ExtraSettings::Frames> frameModeFrom(const std::string& name) {
            for (const auto frames :
                 {ExtraSettings::Frames::Original, ExtraSettings::Frames::Vsync, ExtraSettings::Frames::Unlimited, ExtraSettings::Frames::Capped}) {
                if (name == frameModeName(frames)) {
                    return frames;
                }
            }
            return std::nullopt;
        }

        /// @brief Finds the display mode with a name.
        /// @param name The name as written in the settings file.
        /// @return The mode, or nothing for an unknown name.
        std::optional<ExtraSettings::Display> displayModeFrom(const std::string& name) {
            for (const auto display : {ExtraSettings::Display::Windowed, ExtraSettings::Display::Borderless, ExtraSettings::Display::Fullscreen}) {
                if (name == displayModeName(display)) {
                    return display;
                }
            }
            return std::nullopt;
        }

    }

    const char* displayModeName(ExtraSettings::Display display) {
        switch (display) {
            case ExtraSettings::Display::Borderless: return "borderless"; // borderless window over the whole monitor
            case ExtraSettings::Display::Fullscreen: return "fullscreen"; // monitor switched to the window's size
            case ExtraSettings::Display::Windowed: break; // ordinary window: falls to "windowed"
        }
        return "windowed";
    }

    const char* partyLoginName(ExtraSettings::PartyLogin mode) {
        switch (mode) {
            case ExtraSettings::PartyLogin::Simple: return "simple"; // at most 4 characters, the plain rows
            case ExtraSettings::PartyLogin::Advanced: return "advanced"; // any number, the sliding panel
            case ExtraSettings::PartyLogin::Off: break; // one character: falls to "off"
        }
        return "off";
    }

    const char* speedupModeName(ExtraSettings::SpeedupMode mode) {
        switch (mode) {
            case ExtraSettings::SpeedupMode::Auto: return "auto"; // the recommended ones for the renderer
            case ExtraSettings::SpeedupMode::Custom: return "custom"; // the player's own choice
            case ExtraSettings::SpeedupMode::None: break; // all off: falls to "none"
        }
        return "none";
    }

    core::Speedups recommendedSpeedups(const std::string& renderer) {
        core::Speedups recommended;
        recommended.textPictures = true;
        recommended.quickSdlChecks = true;
        recommended.picturesOnFirstUse = true;
        recommended.mapInParts = true;
        recommended.rememberRendererChecks = true;
        if (renderer == "software") {
            recommended.softwareEncodedTextures = true;
            recommended.softwareChangedAreas = true;
            recommended.softwareScroll = gfx::RenderDevice::desktopIsComposited();
        }
        recommended.quietWorkers = std::thread::hardware_concurrency() == 1; // with more cores a worker has its own
        return recommended;
    }

    bool controllerOnByDefault() {
#ifdef _WIN32
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
#pragma warning(push)
#pragma warning(disable : 4996) // GetVersionExW is deprecated; for telling Windows XP from later ones it is right
        const bool known = GetVersionExW(&version) != 0;
#pragma warning(pop)
        return !known || version.dwMajorVersion >= 6; // 6: Windows Vista
#else
        return true;
#endif
    }

    core::Speedups ExtraSettings::speedupsFor(const std::string& renderer) const {
        core::Speedups chosen = speedupMode == SpeedupMode::Custom ? speedups : speedupMode == SpeedupMode::Auto ? recommendedSpeedups(renderer) : core::Speedups{};
        chosen.limiterSpinMs = speedups.limiterSpinMs;
        if (!gfx::RenderDevice::desktopIsComposited()) {
            chosen.softwareScroll = false; // the picture is seen half moved there, whatever was chosen
        }
        return chosen;
    }

    const char* frameModeName(ExtraSettings::Frames frames) {
        switch (frames) {
            case ExtraSettings::Frames::Vsync: return "vsync"; // frames wait for the display's vertical sync
            case ExtraSettings::Frames::Unlimited: return "unlimited"; // as many frames as possible
            case ExtraSettings::Frames::Capped: return "cap"; // at most fpsCap frames a second
            case ExtraSettings::Frames::Original: break; // the original's 30 ms beat: falls to "original"
        }
        return "original";
    }

    bool ExtraSettings::operator==(const ExtraSettings& other) const {
        return frames == other.frames && fpsCap == other.fpsCap && perfOverlay == other.perfOverlay && windowWidth == other.windowWidth &&
               windowHeight == other.windowHeight && maximized == other.maximized && rememberWindow == other.rememberWindow && renderer == other.renderer &&
               pixelScale == other.pixelScale && pixelArt == other.pixelArt && pixelArtPictures == other.pixelArtPictures &&
               pixelArtSprites == other.pixelArtSprites && pixelArtText == other.pixelArtText && hudScale == other.hudScale && display == other.display &&
               loadingMusicVolume == other.loadingMusicVolume && loadingMusicOn == other.loadingMusicOn && loadingSoundVolume == other.loadingSoundVolume &&
               loadingSoundOn == other.loadingSoundOn && sortButtons == other.sortButtons && partyLogin == other.partyLogin && modernBag == other.modernBag && sfw == other.sfw && speedupMode == other.speedupMode && speedups == other.speedups &&
               controller == other.controller && lighting == other.lighting;
    }

    std::filesystem::path ExtraSettings::fileIn(const std::filesystem::path& userRoot) { return userRoot / "extra-settings.json"; }

    std::optional<ExtraSettings> ExtraSettings::loadIn(const std::filesystem::path& userRoot) {
        if (const auto data = core::readFile(fileIn(userRoot))) {
            return parse(core::toString(*data));
        }
        if (const auto data = core::readFile(userRoot / kOldFile)) {
            return parse(core::toString(*data));
        }
        if (const auto window = WindowMemory::load(WindowMemory::fileIn(userRoot))) {
            ExtraSettings settings;
            settings.windowWidth = window->width;
            settings.windowHeight = window->height;
            settings.maximized = window->maximized;
            settings.rememberWindow = true;
            return settings;
        }
        return std::nullopt;
    }

    std::optional<ExtraSettings> ExtraSettings::parse(const std::string& json) {
        const Json root = Json::parse(json, nullptr, false);
        if (!root.is_object()) {
            return std::nullopt;
        }
        ExtraSettings settings;
        if (const auto video = root.find("video"); video != root.end() && video->is_object()) {
            std::string frames;
            readString(*video, "frames", frames);
            settings.frames = frameModeFrom(frames).value_or(settings.frames);
            readInt(*video, "fpsCap", settings.fpsCap, 30, 1000); // frame cap range in frames per second
            bool oldGraph = false; // before the levels the overlay was the whole graph or nothing
            readBool(*video, "frameGraph", oldGraph);
            settings.perfOverlay = oldGraph ? 3 : 0;
            readInt(*video, "perfOverlay", settings.perfOverlay, 0, 3); // overlay level 0 to 3
            readString(*video, "renderer", settings.renderer);
            readInt(*video, "pixelScale", settings.pixelScale, 1, 3); // window pixels per game pixel, 1 to 3
            readInt(*video, "hudScale", settings.hudScale, 100, 200); // HUD size in percent, 100 to 200
            std::string pixelArt;
            readString(*video, "pixelArt", pixelArt);
            if (formats::pixelArtFilterNamed(pixelArt)) {
                settings.pixelArt = pixelArt;
            }
            readBool(*video, "pixelArtPictures", settings.pixelArtPictures);
            readBool(*video, "pixelArtSprites", settings.pixelArtSprites);
            readBool(*video, "pixelArtText", settings.pixelArtText);
        }
        if (const auto window = root.find("window"); window != root.end() && window->is_object()) {
            readInt(*window, "width", settings.windowWidth, 800, 16384); // window width range in pixels
            readInt(*window, "height", settings.windowHeight, 600, 16384); // window height range in pixels
            readBool(*window, "maximized", settings.maximized);
            readBool(*window, "remember", settings.rememberWindow);
            std::string display;
            readString(*window, "display", display);
            settings.display = displayModeFrom(display).value_or(settings.display);
        }
        if (const auto sound = root.find("sound"); sound != root.end() && sound->is_object()) {
            readInt(*sound, "loadingMusicVolume", settings.loadingMusicVolume, -1, 10); // volume 0 to 10, or -1 for full loudness
            readBool(*sound, "loadingMusicOn", settings.loadingMusicOn);
            readInt(*sound, "loadingSoundVolume", settings.loadingSoundVolume, -1, 10); // volume 0 to 10, or -1 for full loudness
            readBool(*sound, "loadingSoundOn", settings.loadingSoundOn);
        }
        if (const auto game = root.find("game"); game != root.end() && game->is_object()) {
            readBool(*game, "sortButtons", settings.sortButtons);
            if (const auto party = game->find("partyLogin"); party != game->end()) {
                if (party->is_boolean()) {
                    // the older on/off check box: on was the plain rows, now Simple
                    settings.partyLogin = party->get<bool>() ? PartyLogin::Simple : PartyLogin::Off;
                } else if (party->is_string()) {
                    for (const auto known : {PartyLogin::Off, PartyLogin::Simple, PartyLogin::Advanced}) {
                        if (party->get<std::string>() == partyLoginName(known)) {
                            settings.partyLogin = known;
                        }
                    }
                }
            }
            readBool(*game, "modernBag", settings.modernBag);
            readBool(*game, "sfw", settings.sfw);
        }
        if (const auto controller = root.find("controller"); controller != root.end() && controller->is_object()) {
            readBool(*controller, "enabled", settings.controller.enabled);
            readBool(*controller, "hideHud", settings.controller.hideHud);
            readBool(*controller, "hideStatus", settings.controller.hideStatus);
            readBool(*controller, "hideChat", settings.controller.hideChat);
            readInt(*controller, "hideSeconds", settings.controller.hideSeconds, 0, 600); // up to ten minutes
        }
        if (const auto lighting = root.find("lighting"); lighting != root.end() && lighting->is_object()) {
            for (const auto& [name, member] : kLighting) {
                readBool(*lighting, name, settings.lighting.*member);
            }
        }
        if (const auto speedups = root.find("speedups"); speedups != root.end() && speedups->is_object()) {
            for (const auto& [name, member] : kSpeedups) {
                readBool(*speedups, name, settings.speedups.*member);
            }
            readNumber(*speedups, "limiterSpinMs", settings.speedups.limiterSpinMs, 0.0, kMostSpinMs);
            std::string mode;
            readString(*speedups, "mode", mode);
            // a file from before the modes: its own choice when it has one
            const bool any = std::any_of(kSpeedups.begin(), kSpeedups.end(), [&settings](const auto& entry) { return settings.speedups.*(entry.second); });
            settings.speedupMode = any ? SpeedupMode::Custom : SpeedupMode::None;
            for (const auto known : {SpeedupMode::None, SpeedupMode::Auto, SpeedupMode::Custom}) {
                if (mode == speedupModeName(known)) {
                    settings.speedupMode = known;
                }
            }
        }
        return settings;
    }

    std::string ExtraSettings::toJson() const {
        Json root;
        root["video"] = {
            {"frames", frameModeName(frames)},
            {"fpsCap", fpsCap},
            {"perfOverlay", perfOverlay},
            {"renderer", renderer},
            {"pixelScale", pixelScale},
            {"hudScale", hudScale},
            {"pixelArt", pixelArt},
            {"pixelArtPictures", pixelArtPictures},
            {"pixelArtSprites", pixelArtSprites},
            {"pixelArtText", pixelArtText}
        };
        root["window"] =
            {{"width", windowWidth}, {"height", windowHeight}, {"maximized", maximized}, {"remember", rememberWindow}, {"display", displayModeName(display)}};
        root["sound"] = {
            {"loadingMusicVolume", loadingMusicVolume},
            {"loadingMusicOn", loadingMusicOn},
            {"loadingSoundVolume", loadingSoundVolume},
            {"loadingSoundOn", loadingSoundOn}
        };
        root["game"] = {{"sortButtons", sortButtons}, {"partyLogin", partyLoginName(partyLogin)}, {"modernBag", modernBag}, {"sfw", sfw}};
        root["controller"] = {
            {"enabled", controller.enabled},
            {"hideHud", controller.hideHud},
            {"hideStatus", controller.hideStatus},
            {"hideChat", controller.hideChat},
            {"hideSeconds", controller.hideSeconds}
        };
        for (const auto& [name, member] : kLighting) {
            root["lighting"][name] = lighting.*member;
        }
        Json& listed = root["speedups"];
        listed["mode"] = speedupModeName(speedupMode);
        for (const auto& [name, member] : kSpeedups) {
            listed[name] = speedups.*member;
        }
        listed["limiterSpinMs"] = speedups.limiterSpinMs;
        return root.dump(2) + "\n";
    }

    bool ExtraSettings::save(const std::filesystem::path& file) const {
        std::error_code ignored;
        std::filesystem::create_directories(file.parent_path(), ignored);
        return core::writeFile(file, core::toBytes(toJson()));
    }

    void ExtraSettings::override(const LaunchOptions& options) {
        if (options.framesGiven) {
            frames = !options.freeFrames ? Frames::Original : options.vsync ? Frames::Vsync : options.fpsCap > 0 ? Frames::Capped : Frames::Unlimited;
            if (options.fpsCap > 0) {
                fpsCap = options.fpsCap;
            }
        }
        if (options.showFrameGraph) {
            perfOverlay = 3; // --perf: everything
        }
        if (!options.rendererName.empty()) {
            renderer = options.rendererName;
        }
        if (options.pixelScale > 0) {
            pixelScale = options.pixelScale;
        }
        if (options.hudScale > 0) {
            hudScale = options.hudScale;
        }
        if (!options.pixelArt.empty()) {
            pixelArt = options.pixelArt;
        }
        if (options.pixelArtSkip) {
            const auto skips = [&options](const char* part) { return options.pixelArtSkip->find(part) != std::string::npos; };
            pixelArtPictures = !skips("pictures");
            pixelArtSprites = !skips("sprites");
            pixelArtText = !skips("text");
        }
        if (const auto given = displayModeFrom(options.display)) {
            display = *given;
        }
        if (options.rememberWindow) {
            rememberWindow = true;
        } else if (options.windowGiven) {
            rememberWindow = false;
            windowWidth = options.windowWidth;
            windowHeight = options.windowHeight;
            maximized = false;
        }
    }

}
