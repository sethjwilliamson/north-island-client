#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_REMOTE_CONTROL_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_REMOTE_CONTROL_WINDOW_H

#include "client/services/GameServices.h"
#include "client/models/character/PlayerStatus.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/PetTeam.h"
#include "client/viewmodels/system/PercentSlider.h"
#include "client/models/remote/RemoteControl.h"
#include "client/models/remote/RemoteFile.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "client/viewmodels/system/RemotePlusWindow.h"
#include "ui/CheckBox.h"
#include "ui/DrawingArea.h"
#include "ui/ImageButton.h"
#include "ui/Screen.h"
#include "ui/TextBox.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client {

    /// @brief Remote Control settings window: what the client does while it plays for the player.
    /// @details Original: RemoteControlWindow.xml, TH_Menu.pas TTH_AutoPlayForm, opened by item 34058. Ticks, four percents under which HP and SP of the player and his pet are supplied, death limits, the bag places supplies are taken from and up to five items never drunk (and dropped, with its tick). Confirm starts it, Stop ends a run; the settings are kept in the player's file together with the two sets of the Plus form (RemotePlusWindow).
    class RemoteControlWindow {
        public:
            /// @brief Callbacks into the game state the window needs.
            /// @details Original: level = GetHumanAttr(Player, 40); enoughOnlineTime = Player.FEnoughOnlineTime; onVehicle = Player.FTrafficState <> 0 (a vehicle ridden); systemMessage = AddTalkMessage(TalkMsgForm, 0, text, 10, 255).
            struct Hooks {
                std::function<int()> level;
                std::function<bool()> enoughOnlineTime;
                std::function<bool()> onVehicle;
                std::function<std::int32_t()> playerId;
                std::function<void(const std::string&)> systemMessage;
                std::function<void()> openDeliver; // the Deliver button
                std::function<void()> starting; // OK, before the Remote Control starts
            };

            /// @brief Loads the window and wires its events.
            /// @param services Shared game services.
            /// @param screen Screen the window is loaded into.
            /// @param remote The Remote Control state and settings.
            /// @param plus The Remote Control Plus form (level-up setting).
            /// @param inventory The player's bag.
            /// @param pets The player's pet team.
            /// @param status The player's status.
            /// @param messages Message line for hints and refusals.
            /// @param hooks Callbacks into the game state.
            RemoteControlWindow(
                GameServices& services,
                ui::Screen& screen,
                RemoteControl& remote,
                RemotePlusWindow& plus,
                const Inventory& inventory,
                const PetTeam& pets,
                const PlayerStatus& status,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Tells whether the window is visible.
            /// @return True while the window is open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Shows the window; the saved settings are read the first time (FReloadOnShow), then it shows what the Remote Control is set to.
            /// @details Original: ShowCtrl.
            void open();

            /// @brief Hides the window.
            void close();

            /// @brief Per-frame check: the form goes when the place of the item holds nothing.
            /// @details Original: DrawControl.
            void update();

            /// @brief Sets a cell of the kept items from an item of the bag dropped on it.
            /// @details Original: SetAutoPlayItem.
            /// @param cell Cell number 1..5 of the items to keep.
            /// @param itemId Id of the dropped item.
            void setKeptItem(int cell, int itemId);

        private:
            /// @brief Reads the settings file into the Remote Control and the Plus form.
            /// @details Original: ResetAutoPlayDefaults and LoadAutoPlayFile (mode 1).
            void loadFile();

            /// @brief Puts the settings back to their defaults.
            /// @details Original: ResetAutoPlayDefaults.
            void resetDefaults();

            /// @brief Writes the settings to the player's file.
            /// @details Original: SaveAutoPlayFile.
            void saveFile();

            /// @brief Sets a tick box and its stored state.
            /// @param tick Tick number, 1-based.
            /// @param checked New state.
            void setCheck(int tick, bool checked);

            /// @brief Flips a tick.
            /// @details Original: ToggleAutoPlayCheck.
            /// @param tick Tick number, 1-based.
            void toggle(int tick);

            /// @brief Checks the conditions and starts the Remote Control run.
            /// @details Original: StartAutoPlay.
            void startRemote();

            /// @brief Paints the percent rails with their fill.
            /// @details Original: DrawControl and RenderWithChildren.
            /// @param canvas Target canvas.
            /// @param at Window origin on the canvas.
            void paintPage(gfx::Canvas& canvas, core::Point at);

            /// @brief Paints one slider's thumb and drives its drag and paging.
            /// @details Original: RenderWithChildren.
            /// @param canvas Target canvas.
            /// @param at Window origin on the canvas.
            /// @param slider Slider index 0..3.
            /// @param nowMs Current time in milliseconds.
            void paintThumb(gfx::Canvas& canvas, core::Point at, std::size_t slider, std::uint64_t nowMs);

            /// @brief Paints the hints over the rails.
            /// @param canvas Target canvas.
            /// @param at Window origin on the canvas.
            void paintHints(gfx::Canvas& canvas, core::Point at);

            /// @brief Paints the item shown in a kept-item cell.
            /// @param canvas Target canvas.
            /// @param at Origin of the cell on the canvas.
            /// @param cell Cell number 1..5.
            void paintKept(gfx::Canvas& canvas, core::Point at, int cell);

            /// @brief Empties a kept-item cell.
            /// @param cell Cell number 1..5.
            void clearKept(int cell);

            /// @brief Reads a number from a text box.
            /// @param box The text box.
            /// @return The number typed in, or 0 when empty.
            static int numberIn(const ui::TextBox& box);

            /// @brief Writes a number into a text box.
            /// @param box The text box.
            /// @param value The number to show.
            static void setNumber(ui::TextBox& box, int value);

            GameServices& m_services;
            ui::Screen& m_screen;
            RemoteControl& m_remote;
            RemotePlusWindow& m_plus;
            const Inventory& m_inventory;
            const PetTeam& m_pets;
            const PlayerStatus& m_status;
            PlayerMessages& m_messages;
            Hooks m_hooks;
            ui::Window& m_window;
            std::array<std::reference_wrapper<ui::DrawingArea>, 4> m_sliderAreas;
            std::array<std::reference_wrapper<ui::DrawingArea>, RemoteSettings::kKeepItems> m_keptAreas;
            std::array<std::reference_wrapper<ui::CheckBox>, RemoteSettings::kTicks> m_checks;
            ui::TextBox& m_from;
            ui::TextBox& m_to;
            ui::TextBox& m_deathLimit;
            ui::TextBox& m_petDeathLimit;
            ui::TextBox& m_leaveMinutes;
            ui::ImageButton& m_plusSet;
            ui::ImageButton& m_deliverSet;
            std::array<bool, RemoteSettings::kTicks + 1> m_ticks{};
            std::array<PercentSlider, 4> m_sliders;
            std::array<bool, 4> m_held{};
            std::array<int, RemoteSettings::kKeepItems + 1> m_kept{};
            std::array<int, RemoteSettings::kKeepItems + 1> m_shown{};
            bool m_reload = true;
            bool m_versionUpdated = false;
            gfx::PictureId m_rail;
            gfx::PictureId m_railFill;
            gfx::PictureId m_thumb;
            gfx::PictureId m_base;
    };

}

#endif
