#include "client/viewmodels/system/SettingsWindow.h"

#include "core/Log.h"
#include "gfx/RenderDevice.h"
#include "gfx/TextRenderer.h"
#include "gfx/TextStyle.h"
#include "protocol/SystemRequests.h"
#include "ui/CheckBox.h"
#include "ui/DrawingArea.h"
#include "ui/ImageButton.h"
#include "ui/Label.h"
#include "ui/ScrollBar.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace client {

    namespace {

        // number of the SettingsOptionN toggle button for each server switch below: 1 PK, 2 fighting, 6 trade, 10 party
        constexpr std::array<int, 4> kSwitchOptions = {1, 2, 6, 10};
        constexpr std::array<SystemOptions::Switch, 4> kSwitches =
            {SystemOptions::Switch::Pk, SystemOptions::Switch::Fighting, SystemOptions::Switch::Trade, SystemOptions::Switch::Party};
        constexpr std::array<int, 2> kVolumeRows = {50, 130}; // y of the music and sound volume rows, px
        constexpr int kFirstColorRow = 106; // y of the first chat colour row, px
        constexpr int kColorRowStep = 26; // vertical distance between chat colour rows, px
        constexpr int kVideoGroupsTop = 4; // y of the first group of the Video tab, px
        constexpr std::array<int, 8> kCaps = {30, 40, 60, 80, 120, 144, 165, 240}; // frame rate cap choices, frames per second

        /// @brief The speedups of the Speedups group's check boxes, SettingsSpeedup1 first.
        constexpr std::array<bool core::Speedups::*, 9> kSpeedupBoxes = {
            &core::Speedups::softwareEncodedTextures,
            &core::Speedups::softwareChangedAreas,
            &core::Speedups::softwareScroll,
            &core::Speedups::textPictures,
            &core::Speedups::quickSdlChecks,
            &core::Speedups::picturesOnFirstUse,
            &core::Speedups::mapInParts,
            &core::Speedups::quietWorkers,
            &core::Speedups::rememberRendererChecks
        };
        /// @brief The lighting's switches of the Lighting group's check boxes, SettingsLight1 first; none for the second row, the shadows,
        /// which is a choice (shadowChoices).
        constexpr std::array<bool ExtraSettings::Lighting::*, 5> kLightBoxes = {
            &ExtraSettings::Lighting::enabled,
            nullptr,
            &ExtraSettings::Lighting::clouds,
            &ExtraSettings::Lighting::air,
            &ExtraSettings::Lighting::occlusion
        };
        constexpr std::array<double, 4> kSpinChoicesMs = {1.0, 0.5, 0.2, 0.0}; // busy wait choices, ms (1 as the client always had)
        // window size choices, width x height in px (sizes larger than the desktop are left out)
        constexpr std::array<core::Size, 26> kWindowSizes = {{{800, 600},   {1024, 768},  {1152, 864},  {1280, 720},  {1280, 800},  {1280, 960},  {1280, 1024},
                                                              {1366, 768},  {1400, 1050}, {1440, 900},  {1600, 900},  {1600, 1200}, {1680, 1050}, {1800, 1440},
                                                              {1920, 1080}, {1920, 1200}, {1920, 1440}, {2048, 1536}, {2560, 1440}, {2560, 1600}, {2560, 2048},
                                                              {2700, 2160}, {2880, 2160}, {3200, 1800}, {3456, 2160}, {3840, 2160}}};

        /// @brief Loads SettingsWindow.xml into the screen and returns its window.
        /// @param services Shared game services.
        /// @param screen Screen to load the layout into.
        /// @return The loaded SettingsWindow.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("SettingsWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("SettingsWindow");
        }

        /// @brief Reads the number after a name prefix (the button number in "SettingsArrow3").
        /// @param name Element name.
        /// @param prefix Expected prefix.
        /// @return The number, or 0 when the name does not start with the prefix.
        int numberAfter(const std::string& name, const std::string& prefix) { return name.rfind(prefix, 0) == 0 ? std::stoi(name.substr(prefix.size())) : 0; }

        /// @brief Formats a window size as "W x H".
        /// @param width Width in pixels.
        /// @param height Height in pixels.
        /// @return The text.
        std::string sizeText(int width, int height) { return std::to_string(width) + " x " + std::to_string(height); }

        /// @brief Words a busy wait.
        /// @param ms The wait in ms.
        /// @return "None" for 0, else the ms with one decimal when it has one ("0.5 ms", "1 ms").
        std::string spinText(double ms) {
            const int tenths = static_cast<int>(std::lround(ms * 10.0)); // the settings file's value to a tenth of a ms
            if (tenths <= 0) {
                return "None";
            }
            return std::to_string(tenths / 10) + (tenths % 10 != 0 ? "." + std::to_string(tenths % 10) : std::string()) + " ms";
        }

        /// @brief The help line under the frame rate choice.
        /// @param video The current video settings.
        /// @return The help text.
        const char* framesHelp(const ExtraSettings& video) {
            switch (video.frames) {
                case ExtraSettings::Frames::Vsync: return "Smooth frames, one each monitor refresh."; // one frame per monitor refresh
                case ExtraSettings::Frames::Unlimited: return "Smooth frames, as many as the PC can draw."; // no frame limit
                case ExtraSettings::Frames::Capped: return "Smooth frames, at most this many a second."; // capped at fpsCap frames a second
                case ExtraSettings::Frames::Original: break; // original 30 ms frame time (falls through to the last return)
            }
            return "One frame every 30 ms, like the original."; // original frame time in ms
        }

    }

    SettingsWindow::SettingsWindow(
        GameServices& services,
        ui::Screen& screen,
        Actions actions,
        bool beforeLogin
    )
        : m_services(services),
          m_screen(screen),
          m_actions(std::move(actions)),
          m_beforeLogin(beforeLogin),
          m_window(loadWindow(services, screen)),
          m_gameRows(screen, "SettingsGameRows", "SettingsGameWheel", "SettingsGameScroll"),
          m_videoRows(screen, "SettingsVideoRows", "SettingsVideoWheel", "SettingsVideoScroll"),
          m_humInfo(services, screen, m_options.settings.humInfo),
          m_channels(services, screen),
          m_onlineTips(services, screen) {
        screen.on("onSettingsWindowClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onSettingsTab", [this](const ui::EventArgs& args) { showTab(static_cast<Tab>(numberAfter(args.source, "SettingsTab"))); });
        screen.on("onSettingsOption", [this](const ui::EventArgs& args) { option(numberAfter(args.source, "SettingsOption")); });
        screen.on("onSettingsArrow", [this](const ui::EventArgs& args) { arrow(numberAfter(args.source, "SettingsArrow")); });
        screen.on("onSettingsMenu", [this](const ui::EventArgs& args) { menu(numberAfter(args.source, "SettingsMenu")); });
        screen.on("onSettingsCamera", [this](const ui::EventArgs&) {
            m_options.flipCamera();
            showStates();
        });
        screen.on("onSettingsHumInfoSetup", [this](const ui::EventArgs&) { m_humInfo.toggle(); });
        screen.on("onSettingsChannelSetup", [this](const ui::EventArgs&) { m_channels.toggle(); });
        screen.on("onSettingsBlacklist", [this](const ui::EventArgs&) {
            if (m_actions.blacklist) {
                m_actions.blacklist();
            }
        });
        screen.on("onSettingsTips", [this](const ui::EventArgs&) { m_onlineTips.toggle(); });
        screen.on("onSettingsChatBox", [this](const ui::EventArgs&) {
            if (m_actions.chatSkin) {
                m_actions.chatSkin();
            }
        });
        screen.on("onSettingsVideoArrow", [this](const ui::EventArgs& args) { videoArrow(numberAfter(args.source, "SettingsVideoArrow")); });
        screen.on("onSettingsSpeedup", [this](const ui::EventArgs& args) { toggleSpeedup(numberAfter(args.source, "SettingsSpeedup")); });
        screen.on("onSettingsLight", [this](const ui::EventArgs& args) {
            const int number = numberAfter(args.source, "SettingsLight");
            if (number < 1 || number > static_cast<int>(kLightBoxes.size())) {
                return;
            }
            const auto member = kLightBoxes[static_cast<std::size_t>(number - 1)];
            if (member == nullptr) {
                return;
            }
            m_video = m_services.videoSettings();
            const bool checked = m_screen.get<ui::CheckBox>(args.source).isChecked();
            changeVideo([member, checked](ExtraSettings& video) { video.lighting.*member = checked; });
        });
        screen.on("onSettingsVideoScroll",[this](const ui::EventArgs& args) { m_videoRows.scrollTo(args.index); });
        screen.on("onSettingsVideoWheel", [this](const ui::EventArgs& args) { m_videoRows.wheel(args.index); });
        screen.on("onSettingsGameScroll", [this](const ui::EventArgs& args) { m_gameRows.scrollTo(args.index); });
        screen.on("onSettingsGameWheel", [this](const ui::EventArgs& args) { m_gameRows.wheel(args.index); });
        screen.on("onSettingsVideoOpen", [this](const ui::EventArgs& args) { toggleVideoGroup(numberAfter(args.source, "SettingsVideoOpen")); });
        screen.on("onSettingsVideoHead", [this](const ui::EventArgs& args) {
            m_services.playSound("sound/wav0151.wav"); // the sound of the heading's arrow button
            toggleVideoGroup(numberAfter(args.source, "SettingsVideoHead"));
        });
        screen.on("onSettingsGameOpen", [this](const ui::EventArgs& args) { toggleGameGroup(numberAfter(args.source, "SettingsGameOpen")); });
        screen.on("onSettingsGameHead", [this](const ui::EventArgs& args) { toggleGameGroup(numberAfter(args.source, "SettingsGameHead")); });
        screen.on("onSettingsModernBag", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            const bool checked = m_screen.get<ui::CheckBox>("SettingsModernBag").isChecked();
            changeVideo([checked](ExtraSettings& video) { video.modernBag = checked; });
        });
        screen.on("onSettingsSfw", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            const bool checked = m_screen.get<ui::CheckBox>("SettingsSfw").isChecked();
            changeVideo([checked](ExtraSettings& video) { video.sfw = checked; });
        });
        layoutGame();
        layoutVideo();
        screen.on("onSettingsRemember", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            const bool checked = m_screen.get<ui::CheckBox>("SettingsRemember").isChecked();
            changeVideo([checked](ExtraSettings& video) { video.rememberWindow = checked; });
        });
        screen.on("onSettingsSort", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            const bool checked = m_screen.get<ui::CheckBox>("SettingsSort").isChecked();
            changeVideo([checked](ExtraSettings& video) { video.sortButtons = checked; });
        });
        screen.on("onSettingsPartyLoginArrow", [this](const ui::EventArgs& args) {
            // off, Simple, Advanced, round and round; arrow 1 is the left one
            using Mode = ExtraSettings::PartyLogin;
            static constexpr std::array<Mode, 3> kModes = {Mode::Off, Mode::Simple, Mode::Advanced};
            m_video = m_services.videoSettings();
            const int step = numberAfter(args.source, "SettingsPartyLoginArrow") == 1 ? -1 : 1;
            const int count = static_cast<int>(kModes.size());
            const int now = static_cast<int>(std::find(kModes.begin(), kModes.end(), m_video.partyLogin) - kModes.begin());
            const Mode mode = kModes[static_cast<std::size_t>(((now + step) % count + count) % count)];
            changeVideo([mode](ExtraSettings& video) { video.partyLogin = mode; });
        });
        screen.on("onSettingsPad", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            ExtraSettings::Controller controller = m_video.controller;
            controller.enabled = m_screen.get<ui::CheckBox>("SettingsPadEnabled").isChecked();
            controller.hideHud = m_screen.get<ui::CheckBox>("SettingsPadHideHud").isChecked();
            controller.hideStatus = m_screen.get<ui::CheckBox>("SettingsPadHideStatus").isChecked();
            controller.hideChat = m_screen.get<ui::CheckBox>("SettingsPadHideChat").isChecked();
            changeVideo([controller](ExtraSettings& video) { video.controller = controller; });
        });
        screen.on("onSettingsPadArrow", [this](const ui::EventArgs& args) {
            // the times to choose from, in seconds; 0: the bars hide only when Start sends them away
            static constexpr std::array<int, 6> kSeconds = {0, 5, 10, 15, 30, 60};
            m_video = m_services.videoSettings();
            const int step = numberAfter(args.source, "SettingsPadArrow") == 1 ? -1 : 1; // arrow 1 is the left one
            const int count = static_cast<int>(kSeconds.size());
            int now = 0;
            for (int i = 0; i < count; ++i) {
                if (kSeconds[static_cast<std::size_t>(i)] <= m_video.controller.hideSeconds) {
                    now = i; // the choice at or below a time written into the file by hand
                }
            }
            const int seconds = kSeconds[static_cast<std::size_t>(((now + step) % count + count) % count)];
            changeVideo([seconds](ExtraSettings& video) { video.controller.hideSeconds = seconds; });
        });
        screen.on("onSettingsPixelArtPart", [this](const ui::EventArgs&) {
            m_video = m_services.videoSettings();
            const bool pictures = m_screen.get<ui::CheckBox>("SettingsPixelArtOnPictures").isChecked();
            const bool sprites = m_screen.get<ui::CheckBox>("SettingsPixelArtOnSprites").isChecked();
            const bool text = m_screen.get<ui::CheckBox>("SettingsPixelArtOnText").isChecked();
            changeVideo([pictures, sprites, text](ExtraSettings& video) {
                video.pixelArtPictures = pictures;
                video.pixelArtSprites = sprites;
                video.pixelArtText = text;
            });
        });
        screen.get<ui::DrawingArea>("SettingsVolumes").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintVolumes(canvas, at); });
        screen.get<ui::DrawingArea>("SettingsColors").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintColors(canvas, at); });
        if (m_beforeLogin) {
            screen.get<ui::Label>("SettingsTitle").setText("Settings");
            for (const char* name : {"SettingsTab3", "SettingsMenu4", "SettingsMenu5", "SettingsMenu7", "SettingsMenu8", "SettingsMenu9"}) {
                screen.get<ui::Element>(name).setVisible(false);
            }
            // tab button position (x, y) when the Chat tab is hidden; the Game tab stays for its groups that need no server
            screen.get<ui::Element>("SettingsTab4").setPosition({170, 44});
            m_tab = Tab::Sound;
            layoutGame();
        }
        showTab(m_tab);
    }

    void SettingsWindow::loadBeforeLogin() {
        const ExtraSettings video = m_services.videoSettings();
        m_options.settings.musicVolume = video.loadingMusicVolume >= 0 ? video.loadingMusicVolume : 10; // default music volume (0..10) when none is saved
        m_options.settings.musicOn = video.loadingMusicOn;
        m_options.settings.soundVolume = video.loadingSoundVolume >= 0 ? video.loadingSoundVolume : 10; // default sound volume (0..10) when none is saved
        m_options.settings.soundOn = video.loadingSoundOn;
        showStates();
    }

    void SettingsWindow::load() {
        if (m_beforeLogin) {
            loadBeforeLogin();
            return;
        }
        const auto file = SystemSettings::fileFor(m_services.game.userRoot(), m_services.session.loginId);
        m_options.settings = SystemSettings::load(file, m_services.session.loginId);
        syncLoadingVolumes();
        showStates();
        apply();
    }

    void SettingsWindow::syncLoadingVolumes() {
        const ExtraSettings video = m_services.videoSettings();
        if (video.loadingMusicVolume < 0 || video.loadingSoundVolume < 0) {
            rememberLoadingVolumes();
            return;
        }
        m_options.settings.musicVolume = video.loadingMusicVolume;
        m_options.settings.musicOn = video.loadingMusicOn;
        m_options.settings.soundVolume = video.loadingSoundVolume;
        m_options.settings.soundOn = video.loadingSoundOn;
    }

    void SettingsWindow::open() {
        load();
        showTab(m_tab);
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void SettingsWindow::close() {
        m_window.setVisible(false);
        if (m_beforeLogin) {
            return;
        }
        m_options.settings.save(SystemSettings::fileFor(m_services.game.userRoot(), m_services.session.loginId), m_services.session.loginId);
    }

    void SettingsWindow::toggle() {
        if (isOpen()) {
            close();
        } else {
            open();
        }
    }

    void SettingsWindow::showTab(Tab tab) {
        if (tab < Tab::Game || tab > Tab::Video || (m_beforeLogin && tab == Tab::Chat)) {
            return;
        }
        m_tab = tab;
        static constexpr std::array<const char*, 4> kPanels = {"SettingsGame", "SettingsSound", "SettingsChat", "SettingsVideo"};
        for (int i = 1; i <= 4; ++i) { // 4: the tabs
            const bool shown = i == static_cast<int>(tab);
            m_screen.get<ui::ImageButton>("SettingsTab" + std::to_string(i)).setSelected(shown);
            m_screen.get<ui::Element>(kPanels[static_cast<std::size_t>(i - 1)]).setVisible(shown);
        }
        if (tab == Tab::Video || tab == Tab::Game) {
            m_video = m_services.videoSettings();
            showVideo();
        }
    }

    void SettingsWindow::arrow(int number) {
        // 14 arrows: two (left, right) for each of 7 sliders (music, sound, channel colours)
        if (number < 1 || number > 14) {
            return;
        }
        const int slider = (number + 1) / 2; // arrows 1-2 music volume, 3-4 sound volume, 5-14 the chat colours
        const int step = number % 2 != 0 ? -1 : 1;
        if (m_beforeLogin && slider > 2) {
            return;
        }
        // slider 1 is the music, 2 the sound, 3.. the chat channel colours (index slider - 3)
        const bool changed = slider == 1 ? m_options.stepMusic(step) : slider == 2 ? m_options.stepSound(step) : m_options.stepColor(slider - 3, step);
        if (changed) {
            apply();
            if (slider <= 2) {
                rememberLoadingVolumes();
            }
        }
    }

    void SettingsWindow::rememberLoadingVolumes() {
        m_video = m_services.videoSettings();
        changeVideo([this](ExtraSettings& video) {
            video.loadingMusicVolume = m_options.settings.musicVolume;
            video.loadingMusicOn = m_options.settings.musicOn;
            video.loadingSoundVolume = m_options.settings.soundVolume;
            video.loadingSoundOn = m_options.settings.soundOn;
        });
    }

    void SettingsWindow::option(int number) {
        // option 7 is the music on/off button (option 8 the sound's)
        if (m_beforeLogin && number != 7 && number != 8) {
            return;
        }
        for (std::size_t i = 0; i < kSwitchOptions.size(); ++i) {
            if (number == kSwitchOptions[i]) {
                const auto [sub, state] = m_options.flip(kSwitches[i]);
                m_services.connection.send(protocol::switchRequest(sub, state));
            }
        }
        // option 7: music on/off, option 8: sound on/off
        if (number == 7) {
            m_options.settings.musicOn = !m_options.settings.musicOn;
        } else if (number == 8) {
            m_options.settings.soundOn = !m_options.settings.soundOn;
        }
        showStates();
        apply();
        // option 7: music on/off, option 8: sound on/off
        if (number == 7 || number == 8) {
            rememberLoadingVolumes();
        }
    }

    void SettingsWindow::setServerStates(const std::array<int, 4>& switches, int channels) {
        m_options.setServerStates(switches);
        showStates();
        m_channels.load(channels);
    }

    bool SettingsWindow::chatChannelOn(int chatChannel) const {
        switch (chatChannel) {
            case 1: return m_channels.isOn(5); // server channel 1 (world): channel setup entry 5
            case 2: return m_channels.isOn(1); // server channel 2 (local): channel setup entry 1
            case 3: return m_channels.isOn(2); // server channel 3 (whisper): channel setup entry 2
            case 5: return m_channels.isOn(3); // server channel 5 (team): channel setup entry 3
            case 6: return m_channels.isOn(4); // server channel 6 (guild): channel setup entry 4
            default: return true; // other channels cannot be switched off
        }
    }

    void SettingsWindow::menu(int number) {
        switch (number) {
            case 4: // Return: back to a revival spot
                if (m_actions.returnToSpot) {
                    m_actions.returnToSpot();
                }
                break;
            case 5: // Change Password
                if (m_actions.changePassword) {
                    m_actions.changePassword();
                }
                break;
            case 7: // balance inquiry (sends 34/1)
                m_services.connection.send(protocol::balanceRequest());
                break;
            case 8: // Log Out
                if (m_actions.logout) {
                    m_actions.logout();
                }
                break;
            case 9: // Exit
                if (m_actions.exitGame) {
                    m_actions.exitGame();
                }
                break;
            default: // other menu buttons (web pages, card, recharge) do nothing
                break;
        }
    }

    void SettingsWindow::toggleVideoGroup(int number) {
        const std::size_t group = static_cast<std::size_t>(number - 1);
        m_services.videoGroupsOpen[group] = !m_services.videoGroupsOpen[group];
        layoutVideo();
    }

    void SettingsWindow::toggleSpeedup(int number) {
        if (number < 1 || number > static_cast<int>(kSpeedupBoxes.size())) {
            return;
        }
        m_video = m_services.videoSettings();
        const auto member = kSpeedupBoxes[static_cast<std::size_t>(number - 1)];
        if (member == &core::Speedups::softwareScroll && !gfx::RenderDevice::desktopIsComposited()) {
            showVideo(); // cannot be on here: the box goes back to empty and nothing else changes
            return;
        }
        const bool checked = m_screen.get<ui::CheckBox>("SettingsSpeedup" + std::to_string(number)).isChecked();
        const std::string renderer = speedupRenderer(m_video);
        changeVideo([member, checked, renderer](ExtraSettings& video) {
            // a box taken while none or the recommended ones run: what was shown becomes the player's own choice
            video.speedups = video.speedupsFor(renderer);
            video.speedupMode = ExtraSettings::SpeedupMode::Custom;
            video.speedups.*member = checked;
            // moving the picture on scroll works on top of copying only the changed areas
            if (video.speedups.softwareScroll && member == &core::Speedups::softwareScroll) {
                video.speedups.softwareChangedAreas = true;
            } else if (!video.speedups.softwareChangedAreas) {
                video.speedups.softwareScroll = false;
            }
        });
    }

    void SettingsWindow::toggleGameGroup(int number) {
        const std::size_t group = static_cast<std::size_t>(number - 1);
        if (group < m_services.gameGroupsOpen.size()) {
            m_services.gameGroupsOpen[group] = !m_services.gameGroupsOpen[group];
            layoutGame();
        }
    }

    void SettingsWindow::layoutGame() {
        int y = kVideoGroupsTop;
        for (std::size_t group = 0; group < m_services.gameGroupsOpen.size(); ++group) {
            const std::string number = std::to_string(group + 1);
            ui::Element& heading = m_screen.get<ui::Element>("SettingsGameGroup" + number);
            ui::Element& body = m_screen.get<ui::Element>("SettingsGameBody" + number);
            // groups 1 (what others may ask of the player) and 4 (lists kept by the server) need the game: not before the login
            const bool offered = !m_beforeLogin || group == 1 || group == 2;
            const bool open = offered && (m_services.gameGroupsOpen[group] || (m_beforeLogin && group == 2));
            heading.setVisible(offered);
            body.setVisible(open);
            if (!offered) {
                continue;
            }
            heading.setPosition({0, y});
            y += heading.bounds().height;
            m_screen.get<ui::CheckBox>("SettingsGameOpen" + number).setChecked(open);
            body.setPosition({0, y});
            if (open) {
                y += body.bounds().height;
            }
        }
        m_gameRows.setContentHeight(y);
    }

    void SettingsWindow::layoutVideo() {
        int y = kVideoGroupsTop;
        for (std::size_t group = 0; group < m_services.videoGroupsOpen.size(); ++group) {
            const std::string number = std::to_string(group + 1);
            const bool open = m_services.videoGroupsOpen[group];
            ui::Element& heading = m_screen.get<ui::Element>("SettingsVideoGroup" + number);
            ui::Element& body = m_screen.get<ui::Element>("SettingsVideoBody" + number);
            heading.setPosition({0, y});
            y += heading.bounds().height;
            m_screen.get<ui::CheckBox>("SettingsVideoOpen" + number).setChecked(open);
            body.setVisible(open);
            body.setPosition({0, y});
            if (open) {
                y += body.bounds().height;
            }
        }
        m_videoRows.setContentHeight(y);
    }

    void SettingsWindow::videoArrow(int number) {
        // 22 video arrows: two (left, right) for each of 11 rows
        if (number < 1 || number > 22) {
            return;
        }
        const int step = number % 2 != 0 ? -1 : 1;
        m_video = m_services.videoSettings();
        // arrow number ranges per row: 1-2 frames, 3-4 window, 5-6 renderer, 7-8 scale, 9-10 pixel art, 11-12 display, 13-14 HUD, 15-16 busy wait, 17-18 which speedups, 19-20 overlay,
        // 21-22 shadows
        const std::vector<VideoChoice> choices = number <= 2    ? frameChoices(m_video)
                                                 : number <= 4  ? windowChoices(m_video)
                                                 : number <= 6  ? rendererChoices(m_video)
                                                 : number <= 8  ? scaleChoices(m_video)
                                                 : number <= 10 ? pixelArtChoices(m_video)
                                                 : number <= 12 ? displayChoices(m_video)
                                                 : number <= 14 ? hudChoices(m_video)
                                                 : number <= 16 ? spinChoices(m_video)
                                                 : number <= 18 ? speedupModeChoices(m_video)
                                                 : number <= 20 ? overlayChoices(m_video)
                                                                : shadowChoices(m_video);
        if (choices.empty()) {
            return;
        }
        const auto found = std::find_if(choices.begin(), choices.end(), [this](const VideoChoice& choice) { return choice.matches(m_video); });
        const int count = static_cast<int>(choices.size());
        const int now = found != choices.end() ? static_cast<int>(found - choices.begin()) : (step > 0 ? -1 : count);
        const int next = ((now + step) % count + count) % count;
        changeVideo(choices[static_cast<std::size_t>(next)].apply);
    }

    void SettingsWindow::changeVideo(const std::function<void(ExtraSettings&)>& change) {
        change(m_video);
        m_services.changeVideo(m_video);
        showVideo();
    }

    void SettingsWindow::apply() {
        const SystemSettings& settings = m_options.settings;
        m_services.music.setVolume(settings.musicOn ? SystemOptions::loudnessPercent(settings.musicVolume) : 0);
        if (settings.musicOn) {
            m_services.music.resume();
        } else {
            m_services.music.pause();
        }
        m_services.sounds.setVolume(settings.soundOn ? SystemOptions::loudnessPercent(settings.soundVolume) : 0);
        if (m_beforeLogin) {
            return;
        }
        if (m_actions.changed) {
            m_actions.changed(settings);
        }
    }

    void SettingsWindow::showStates() {
        const auto on = m_services.pictures.find("Btn_On_1").value_or(gfx::PictureId{});
        const auto off = m_services.pictures.find("Btn_Off_1").value_or(gfx::PictureId{});
        for (std::size_t i = 0; i < kSwitchOptions.size(); ++i) {
            m_screen.get<ui::ImageButton>("SettingsOption" + std::to_string(kSwitchOptions[i])).setPicture(m_options.isOn(kSwitches[i]) ? on : off);
        }
        m_screen.get<ui::ImageButton>("SettingsOption7").setPicture(m_options.settings.musicOn ? on : off);
        m_screen.get<ui::ImageButton>("SettingsOption8").setPicture(m_options.settings.soundOn ? on : off);
        m_screen.get<ui::ImageButton>("SettingsCamera").setPicture(m_options.cameraOn() ? on : off);
    }

    void SettingsWindow::showVideo() {
        const auto textOf = [this](const std::vector<VideoChoice>& choices, const std::string& otherwise) {
            const auto found = std::find_if(choices.begin(), choices.end(), [this](const VideoChoice& choice) { return choice.matches(m_video); });
            return found != choices.end() ? found->text : otherwise;
        };
        m_screen.get<ui::Label>("SettingsFramesValue").setText(textOf(frameChoices(m_video), frameModeName(m_video.frames)));
        m_screen.get<ui::Label>("SettingsFramesHelp").setText(framesHelp(m_video));
        m_screen.get<ui::Label>("SettingsWindowValue").setText(textOf(windowChoices(m_video), sizeText(m_video.windowWidth, m_video.windowHeight)));
        m_screen.get<ui::Label>("SettingsDisplayValue").setText(textOf(displayChoices(m_video), displayModeName(m_video.display)));
        m_screen.get<ui::Label>("SettingsHudValue").setText(textOf(hudChoices(m_video), std::to_string(m_video.hudScale) + "%"));
        m_screen.get<ui::Label>("SettingsRendererValue").setText(textOf(rendererChoices(m_video), m_video.renderer));
        m_screen.get<ui::Label>("SettingsScaleValue").setText(textOf(scaleChoices(m_video), std::to_string(m_video.pixelScale) + "x"));
        m_screen.get<ui::Label>("SettingsPixelArtValue").setText(textOf(pixelArtChoices(m_video), m_video.pixelArt));
        m_screen.get<ui::Label>("SettingsPixelArtHelp").setText(pixelArtHelp(m_video));
        m_screen.get<ui::Label>("SettingsRendererHelp").setText("Using " + m_services.rendererInUse + "; changes need a restart");
        m_screen.get<ui::CheckBox>("SettingsRemember").setChecked(m_video.rememberWindow);
        m_screen.get<ui::Label>("SettingsOverlayValue").setText(textOf(overlayChoices(m_video), "Level " + std::to_string(m_video.perfOverlay)));
        m_screen.get<ui::Label>("SettingsOverlayHelp").setText(
            m_video.perfOverlay == 1   ? "Frames a second."
            : m_video.perfOverlay == 2 ? "FPS, CPU / GPU / RAM use, CPU / GPU time."
            : m_video.perfOverlay == 3 ? "Level 2 and the frame time graph."
                                       : "No numbers on the screen."
        );
        m_screen.get<ui::CheckBox>("SettingsPadEnabled").setChecked(m_video.controller.enabled);
        m_screen.get<ui::CheckBox>("SettingsPadHideHud").setChecked(m_video.controller.hideHud);
        m_screen.get<ui::CheckBox>("SettingsPadHideStatus").setChecked(m_video.controller.hideStatus);
        m_screen.get<ui::CheckBox>("SettingsPadHideChat").setChecked(m_video.controller.hideChat);
        m_screen.get<ui::Label>("SettingsPadSecondsValue")
            .setText(m_video.controller.hideSeconds == 0 ? "Start only" : std::to_string(m_video.controller.hideSeconds) + " seconds");
        m_screen.get<ui::CheckBox>("SettingsSort").setChecked(m_video.sortButtons);
        m_screen.get<ui::Label>("SettingsPartyLoginValue")
            .setText(m_video.partyLogin == ExtraSettings::PartyLogin::Simple     ? "Simple"
                     : m_video.partyLogin == ExtraSettings::PartyLogin::Advanced ? "Advanced"
                                                                                 : "Off");
        m_screen.get<ui::CheckBox>("SettingsModernBag").setChecked(m_video.modernBag);
        m_screen.get<ui::CheckBox>("SettingsSfw").setChecked(m_video.sfw);
        m_screen.get<ui::CheckBox>("SettingsPixelArtOnPictures").setChecked(m_video.pixelArtPictures);
        m_screen.get<ui::CheckBox>("SettingsPixelArtOnSprites").setChecked(m_video.pixelArtSprites);
        m_screen.get<ui::CheckBox>("SettingsPixelArtOnText").setChecked(m_video.pixelArtText);
        for (std::size_t i = 0; i < kLightBoxes.size(); ++i) {
            if (kLightBoxes[i] != nullptr) {
                m_screen.get<ui::CheckBox>("SettingsLight" + std::to_string(i + 1)).setChecked(m_video.lighting.*kLightBoxes[i]);
            }
        }
        m_screen.get<ui::Label>("SettingsShadowValue").setText(textOf(shadowChoices(m_video), "Off"));
        // the software renderer lights nothing
        m_screen.get<ui::Label>("SettingsLightHelp")
            .setText(m_services.rendererInUse == "software" ? "Not with the software renderer." : "Shown at once. Looks: lighting.json.");
        const std::string renderer = speedupRenderer(m_video);
        const core::Speedups shown = m_video.speedupsFor(renderer);
        for (std::size_t i = 0; i < kSpeedupBoxes.size(); ++i) {
            m_screen.get<ui::CheckBox>("SettingsSpeedup" + std::to_string(i + 1)).setChecked(shown.*kSpeedupBoxes[i]);
        }
        // the third box (move the picture on scroll) cannot be on where the desktop is not composited
        m_screen.get<ui::Label>("SettingsSpeedupText3")
            .setText(gfx::RenderDevice::desktopIsComposited() ? "Software: move picture on scroll" : "Move picture on scroll: not on this PC");
        m_screen.get<ui::Label>("SettingsSpeedupModeValue").setText(textOf(speedupModeChoices(m_video), speedupModeName(m_video.speedupMode)));
        m_screen.get<ui::Label>("SettingsSpeedupHelp1").setText(
            m_video.speedupMode == ExtraSettings::SpeedupMode::Auto     ? "The recommended ones for " + renderer + "."
            : m_video.speedupMode == ExtraSettings::SpeedupMode::Custom ? "Your own choice of faster ways."
                                                                         : "All off, as the game always was."
        );
        m_screen.get<ui::Label>("SettingsSpinValue").setText(textOf(spinChoices(m_video), spinText(m_video.speedups.limiterSpinMs)));
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::overlayChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        for (int level = 0; level <= 3; ++level) { // off and levels 1 to 3
            choices.push_back(
                {level == 0 ? std::string("Off") : "Level " + std::to_string(level),
                 [level](ExtraSettings& video) { video.perfOverlay = level; },
                 [level](const ExtraSettings& video) { return video.perfOverlay == level; }}
            );
        }
        return choices;
    }

    std::string SettingsWindow::speedupRenderer(const ExtraSettings& video) const { return video.renderer.empty() ? m_services.rendererInUse : video.renderer; }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::speedupModeChoices(const ExtraSettings&) const {
        using Mode = ExtraSettings::SpeedupMode;
        std::vector<VideoChoice> choices;
        const std::array<std::pair<Mode, const char*>, 3> modes = {{{Mode::None, "None"}, {Mode::Auto, "Auto"}, {Mode::Custom, "Custom"}}};
        for (const auto& entry : modes) {
            const Mode mode = entry.first;
            choices.push_back(
                {entry.second, [mode](ExtraSettings& video) { video.speedupMode = mode; }, [mode](const ExtraSettings& video) { return video.speedupMode == mode; }}
            );
        }
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::spinChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        for (const double ms : kSpinChoicesMs) {
            choices.push_back(
                {ms == kSpinChoicesMs.front() ? spinText(ms) + " (default)" : spinText(ms),
                 [ms](ExtraSettings& video) { video.speedups.limiterSpinMs = ms; },
                 [ms](const ExtraSettings& video) { return std::abs(video.speedups.limiterSpinMs - ms) < 0.01; }} // 0.01 ms: the same choice
            );
        }
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::frameChoices(const ExtraSettings& now) const {
        using Frames = ExtraSettings::Frames;
        std::vector<VideoChoice> choices;
        const auto mode = [](Frames frames) { return [frames](ExtraSettings& video) { video.frames = frames; }; };
        const auto isMode = [](Frames frames) { return [frames](const ExtraSettings& video) { return video.frames == frames; }; };
        choices.push_back({"Original (30 ms)", mode(Frames::Original), isMode(Frames::Original)});
        std::vector<int> caps(kCaps.begin(), kCaps.end());
        if (std::find(caps.begin(), caps.end(), now.fpsCap) == caps.end()) {
            caps.insert(std::upper_bound(caps.begin(), caps.end(), now.fpsCap), now.fpsCap);
        }
        for (const int cap : caps) {
            choices.push_back(
                {std::to_string(cap) + " a second",
                 [cap](ExtraSettings& video) {
                     video.frames = Frames::Capped;
                     video.fpsCap = cap;
                 },
                 [cap](const ExtraSettings& video) { return video.frames == Frames::Capped && video.fpsCap == cap; }}
            );
        }
        choices.push_back({"Monitor (vsync)", mode(Frames::Vsync), isMode(Frames::Vsync)});
        choices.push_back({"Unlimited", mode(Frames::Unlimited), isMode(Frames::Unlimited)});
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::windowChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        const core::Size desktop = m_services.desktopSize();
        for (const core::Size size : kWindowSizes) {
            // 40: pixels reserved for the taskbar and frame; 800: the smallest size, always offered
            if (desktop.width > 0 && (size.width > desktop.width || size.height + 40 > desktop.height) && size.width > 800) {
                continue;
            }
            choices.push_back(
                {sizeText(size.width, size.height),
                 [size](ExtraSettings& video) {
                     video.windowWidth = size.width;
                     video.windowHeight = size.height;
                     video.maximized = false;
                 },
                 [size](const ExtraSettings& video) { return !video.maximized && video.windowWidth == size.width && video.windowHeight == size.height; }}
            );
        }
        choices.push_back({"Maximized", [](ExtraSettings& video) { video.maximized = true; }, [](const ExtraSettings& video) { return video.maximized; }});
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::displayChoices(const ExtraSettings&) const {
        using Display = ExtraSettings::Display;
        std::vector<VideoChoice> choices;
        for (const auto& [text, display] :
             {std::pair<const char*, Display>{"Windowed", Display::Windowed}, {"Borderless", Display::Borderless}, {"Fullscreen", Display::Fullscreen}}) {
            const Display value = display;
            choices.push_back(
                {text, [value](ExtraSettings& video) { video.display = value; }, [value](const ExtraSettings& video) { return video.display == value; }}
            );
        }
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::shadowChoices(const ExtraSettings&) const {
        // Off: no shadows of figures; 2D: cast from the flat sprites; 3D: from a rough solid shape worked out of them. Kept in the
        // settings file as the two switches "shadows" and "shadows3d", as before
        return {
            {"Off", [](ExtraSettings& video) { video.lighting.shadows = false; }, [](const ExtraSettings& video) { return !video.lighting.shadows; }},
            {"2D",
             [](ExtraSettings& video) {
                 video.lighting.shadows = true;
                 video.lighting.shadows3d = false;
             },
             [](const ExtraSettings& video) { return video.lighting.shadows && !video.lighting.shadows3d; }},
            {"3D",
             [](ExtraSettings& video) {
                 video.lighting.shadows = true;
                 video.lighting.shadows3d = true;
             },
             [](const ExtraSettings& video) { return video.lighting.shadows && video.lighting.shadows3d; }},
        };
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::hudChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        // HUD scale in percent: 100 (as laid out) to 200 in steps of 25
        for (int percent = 100; percent <= 200; percent += 25) {
            choices.push_back(
                {percent == 100 ? "100% (original)" : std::to_string(percent) + "%",
                 [percent](ExtraSettings& video) { video.hudScale = percent; },
                 [percent](const ExtraSettings& video) { return video.hudScale == percent; }}
            );
        }
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::scaleChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        for (const int scale : {1, 2, 3}) { // offered pixel scales
            choices.push_back(
                {scale == 1 ? "1x (original)" : std::to_string(scale) + "x",
                 [scale](ExtraSettings& video) { video.pixelScale = scale; },
                 [scale](const ExtraSettings& video) { return video.pixelScale == scale; }}
            );
        }
        return choices;
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::pixelArtChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        for (const auto& [text, name] : {std::pair<const char*, const char*>{"Off", "off"}, {"Scale2x / 3x", "scale"}, {"xBR", "xbr"}}) {
            const std::string value = name;
            choices.push_back(
                {text, [value](ExtraSettings& video) { video.pixelArt = value; }, [value](const ExtraSettings& video) { return video.pixelArt == value; }}
            );
        }
        return choices;
    }

    std::string SettingsWindow::pixelArtHelp(const ExtraSettings& video) {
        if (video.pixelScale == 1) {
            return "Used at a pixel scale of 2x or 3x";
        }
        if (video.pixelArt == "scale") {
            return "Rounded steps, same colours; on restart";
        }
        if (video.pixelArt == "xbr") {
            return "Smooth, blended edges; on restart";
        }
        return "The game's pixels as blocks; on restart";
    }

    std::vector<SettingsWindow::VideoChoice> SettingsWindow::rendererChoices(const ExtraSettings&) const {
        std::vector<VideoChoice> choices;
        choices.push_back({"Default", [](ExtraSettings& video) { video.renderer.clear(); }, [](const ExtraSettings& video) { return video.renderer.empty(); }});
        for (const std::string& driver : m_services.renderDrivers) {
            choices.push_back(
                {driver, [driver](ExtraSettings& video) { video.renderer = driver; }, [driver](const ExtraSettings& video) { return video.renderer == driver; }}
            );
        }
        return choices;
    }

    void SettingsWindow::paintVolumes(gfx::Canvas& canvas, core::Point at) {
        const auto frame = m_services.pictures.find("Panel29");
        const auto bar = m_services.pictures.find("Panel30");
        if (!frame || !bar) {
            return;
        }
        const gfx::Picture& framePicture = m_services.pictures.picture(*frame);
        const gfx::Picture& barPicture = m_services.pictures.picture(*bar);
        const std::array<int, 2> volumes = {m_options.settings.musicVolume, m_options.settings.soundVolume};
        // 233 and 342: x of the bar area between the left and right arrows; centres the frame in it
        const int frameX = 233 + (342 - 233 - framePicture.width()) / 2;
        for (std::size_t i = 0; i < volumes.size(); ++i) {
            const int y = kVolumeRows[i] + 2; // y offset of the frame below the arrows' top
            canvas.drawPicture(*frame, {at.x + frameX, at.y + y});
            const int width = static_cast<int>(barPicture.width() * (volumes[i] * 0.1)); // volume 0..10 as a tenth of the bar's width
            if (width > 0) {
                canvas.drawPicture(
                    *bar,
                    {at.x + frameX + 3, at.y + y + 3},
                    core::Rect{0, 0, width, barPicture.height()}
                ); // 3: bar inset inside the frame (pixels)
            }
        }
    }

    void SettingsWindow::paintColors(gfx::Canvas& canvas, core::Point at) {
        const int width = gfx::TextRenderer::textWidth("Color", gfx::TextEffect::Shadow);
        for (std::size_t i = 0; i < m_options.settings.channelColors.size(); ++i) {
            const core::Color color = core::Color::fromRgb565(chatColor(m_options.settings.channelColors[i]));
            const int y = kFirstColorRow + static_cast<int>(i) * kColorRowStep + 2; // y offset of the sample below the arrows' top
            // 233 and 342: x of the area between the arrows; 100 x 20: the text box size
            canvas.drawText({at.x + 233 + (342 - 233 - width) / 2, at.y + y}, "Color", {color, gfx::TextEffect::Shadow, std::nullopt}, 100, 20);
        }
    }

}
