#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_SETTINGS_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_SETTINGS_WINDOW_H

#include "client/models/system/SystemOptions.h"
#include "client/services/GameServices.h"
#include "client/services/settings/ExtraSettings.h"
#include "client/viewmodels/chat/ChannelWindow.h"
#include "client/viewmodels/system/HumInfoWindow.h"
#include "client/viewmodels/system/OnlineTipsWindow.h"
#include "client/viewmodels/system/ScrolledRows.h"
#include "ui/Screen.h"
#include "ui/Window.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace client {

    /// @brief The system/settings window (tabs Game, Sound, Chat, Video) with the log out, exit and menu buttons.
    /// @details Original: replaces TSe_SystemForm (SettingsWindow.xml); server switches are sent as 33/x, balance inquiry as 34/1.
    class SettingsWindow {
        public:
            /// @brief Callbacks the scene supplies for the window's menu buttons and for changed settings.
            struct Actions {
                std::function<void()> logout;
                std::function<void()> exitGame;
                std::function<void(const SystemSettings&)> changed;
                std::function<void()> returnToSpot;
                std::function<void()> changePassword;
                std::function<void()> blacklist;
                std::function<void()> chatSkin;
            };

            /// @brief The window's tabs (numbered from 1 as the SettingsTab buttons).
            enum class Tab { Game = 1, Sound, Chat, Video, Pad };

            /// @brief Loads the window layout and wires its events.
            /// @param services Shared game services.
            /// @param screen Screen the window layout is loaded into.
            /// @param actions Callbacks for the menu buttons and changed settings.
            /// @param beforeLogin True on the server and login screens, with no character: only the music (of those screens) and the Video tab.
            SettingsWindow(
                GameServices& services,
                ui::Screen& screen,
                Actions actions,
                bool beforeLogin = false
            );

            /// @brief Whether the window is shown.
            /// @return True when visible.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Loads the character's settings and shows the window.
            void open();

            /// @brief Hides the window and saves the character's settings.
            void close();

            /// @brief Opens the window when closed, closes it when open.
            void toggle();

            /// @brief Shows one tab.
            /// @param tab The tab to show.
            void showTab(Tab tab);

            /// @brief Reads the character's saved settings and applies them (on entering the world).
            void load();

            /// @brief The character's current settings.
            /// @return The settings.
            const SystemSettings& settings() const { return m_options.settings; }

            /// @brief Takes the server's switches and chat channels.
            /// @details Original: server packet 33/2.
            /// @param switches The four server switches.
            /// @param channels The server's chat channel bit set.
            void setServerStates(const std::array<int, 4>& switches, int channels);

            /// @brief Whether chat of a server channel may be sent and shown.
            /// @details Original: TH_channelForm.FChannelEnabled; server chat channels 2/x: 1 world, 2 local, 3 whisper, 5 team, 6 guild.
            /// @param chatChannel The server's chat channel number.
            /// @return True when the channel is on.
            bool chatChannelOn(int chatChannel) const;

            /// @brief Whether the party switch is on.
            /// @return True when party invitations are allowed.
            bool partyAllowed() const { return m_options.partyAllowed(); }

            /// @brief Whether the PK switch is on.
            /// @return True when PK is allowed.
            bool pkAllowed() const { return m_options.pkAllowed(); }

        private:
            /// @brief A choice of the Video tab's arrows: its words and the settings it makes.
            struct VideoChoice {
                std::string text;
                std::function<void(ExtraSettings&)> apply;
                std::function<bool(const ExtraSettings&)> matches;
            };

            /// @brief Handles a volume/colour arrow click (left lowers, right raises).
            /// @details Original: SystemArrowClick.
            /// @param number The arrow button number (1..14).
            void arrow(int number);

            /// @brief Handles an on/off option click (server switches, music, sound).
            /// @details Original: SystemToggleClick.
            /// @param number The option button number.
            void option(int number);

            /// @brief Handles a menu button click.
            /// @details Original: SystemMenuButtonClick.
            /// @param number The menu button number.
            void menu(int number);

            /// @brief Steps one of the Video tab's choices.
            /// @param number The Video arrow number (1..22, two per row).
            void videoArrow(int number);

            /// @brief Takes a speedup's check box (a restart applies it); from None or Auto this makes what was shown the player's own choice.
            /// @param number The check box number (from 1, the order of the Speedups group).
            void toggleSpeedup(int number);

            /// @brief Opens or closes a group of the Video tab.
            /// @param number The group number (from 1).
            void toggleVideoGroup(int number);

            /// @brief Stacks the Video tab's groups and hides the bodies of the closed ones.
            void layoutVideo();

            /// @brief Opens or closes a group of the Game tab.
            /// @param number The group number (from 1).
            void toggleGameGroup(int number);

            /// @brief Stacks the Game tab's groups and hides the bodies of the closed ones; before the login only the groups that need no server show.
            void layoutGame();

            /// @brief Applies a change to the video settings and refreshes the tab.
            /// @param change Function that edits the settings.
            void changeVideo(const std::function<void(ExtraSettings&)>& change);

            /// @brief Applies the music and sound volumes and notifies the changed callback.
            void apply();

            /// @brief Shows the loading and login screens' music and sound settings (before login).
            void loadBeforeLogin();

            /// @brief Stores the music and sound volumes as the loading and login screens' ones.
            void rememberLoadingVolumes();

            /// @brief Gives the character the login screen's music and sound volumes (one value for both), or stores the character's as the
            /// login screen's when none is stored yet.
            void syncLoadingVolumes();

            /// @brief Shows the On/Off pictures of the option buttons.
            /// @details Original: SetOptionButtonState / RefreshSoundButtons.
            void showStates();

            /// @brief Shows the values and check boxes of the extra settings (the Video tab's and the Game tab's two).
            void showVideo();

            /// @brief The frame rate choices.
            /// @param now The current video settings.
            /// @return The choices.
            std::vector<VideoChoice> frameChoices(const ExtraSettings& now) const;

            /// @brief The window size choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> windowChoices(const ExtraSettings& now) const;

            /// @brief The renderer choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> rendererChoices(const ExtraSettings& now) const;

            /// @brief The pixel scale choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> scaleChoices(const ExtraSettings& now) const;

            /// @brief The pixel-art scaler choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> pixelArtChoices(const ExtraSettings& now) const;

            /// @brief The display mode choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> displayChoices(const ExtraSettings& now) const;

            /// @brief The choices of the figures' shadows: Off, 2D (from the flat sprites) and 3D (from a rough solid shape).
            /// @param now The current settings.
            /// @return The choices.
            std::vector<VideoChoice> shadowChoices(const ExtraSettings& now) const;

            /// @brief The HUD size choices.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> hudChoices(const ExtraSettings& now) const;

            /// @brief The performance overlay's levels: off, the frame rate, with use and times, with the graph.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> overlayChoices(const ExtraSettings& now) const;

            /// @brief The choices of which speedups run: none, the recommended ones, the player's own.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> speedupModeChoices(const ExtraSettings& now) const;

            /// @brief The renderer the recommended speedups are shown for: the chosen one, else the one in use.
            /// @param video The current video settings.
            /// @return The renderer's name.
            std::string speedupRenderer(const ExtraSettings& video) const;

            /// @brief The choices of how long a capped frame's wait ends in a busy wait.
            /// @param now The current video settings (unused).
            /// @return The choices.
            std::vector<VideoChoice> spinChoices(const ExtraSettings& now) const;

            /// @brief The help line under the pixel-art choice.
            /// @param video The current video settings.
            /// @return The help text.
            static std::string pixelArtHelp(const ExtraSettings& video);

            /// @brief Paints the volume bars in their frames.
            /// @param canvas Canvas to draw on.
            /// @param at Top-left of the drawing area.
            void paintVolumes(gfx::Canvas& canvas, core::Point at);

            /// @brief Paints the colour samples of the chat channels.
            /// @param canvas Canvas to draw on.
            /// @param at Top-left of the drawing area.
            void paintColors(gfx::Canvas& canvas, core::Point at);

            GameServices& m_services;
            ui::Screen& m_screen;
            Actions m_actions;
            bool m_beforeLogin = false;
            ui::Window& m_window;
            ScrolledRows m_gameRows;
            ScrolledRows m_videoRows;
            SystemOptions m_options;
            Tab m_tab = Tab::Game;
            ExtraSettings m_video;
            HumInfoWindow m_humInfo;
            ChannelWindow m_channels;
            OnlineTipsWindow m_onlineTips;
    };

}

#endif
