#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_AUTO_DELIVER_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_SYSTEM_AUTO_DELIVER_WINDOW_H

#include "client/models/items/Inventory.h"
#include "client/models/pets/PetTeam.h"
#include "client/models/remote/AutoDeliver.h"
#include "client/models/remote/AutoDeliverSettings.h"
#include "client/services/GameServices.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "ui/CheckBox.h"
#include "ui/DrawingArea.h"
#include "ui/Label.h"
#include "ui/ListBox.h"
#include "ui/Screen.h"
#include "ui/TextBox.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace client {

    /// @brief The Electron Control's Auto Deliver form: the recipient, the pet and the items (not in the original game).
    /// @details AutoDeliverWindow.xml. Edits are a draft until OK, Send now or the Remote Control's OK. It also reads the
    /// character's saved settings into AutoDeliver when another character plays.
    class AutoDeliverWindow {
        public:
            /// @brief Callbacks into the game state.
            struct Hooks {
                std::function<bool()> remoteFormOpen;
                std::function<std::string()> ownName;
                std::function<bool(const std::string&)> online;
                std::function<std::vector<std::string>()> names; // whom to offer, the own characters first
                std::function<std::optional<std::string>()> refusal; // why the pet express may not be used
                std::function<void(int left)> showBag; // opens the bag with its left edge there, to drag from
                std::function<void()> putBagBack; // as it was before showBag
            };

            /// @brief Loads the form and wires its events.
            /// @param services Shared game services.
            /// @param screen Screen the window is loaded into.
            /// @param deliver Auto Deliver, which holds the settings in force.
            /// @param inventory The player's bag.
            /// @param pets The player's pets.
            /// @param messages Message line for refusals.
            /// @param hooks Callbacks into the game state.
            AutoDeliverWindow(
                GameServices& services,
                ui::Screen& screen,
                AutoDeliver& deliver,
                const Inventory& inventory,
                const PetTeam& pets,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Opens the form with the settings in force, or closes it.
            void toggle();

            /// @brief Per-frame: follows the character playing, closes with the Remote Control's form and refreshes the lines.
            void update();

            /// @brief Takes a bag item dropped on a cell.
            /// @param cell Cell number 1..10.
            /// @param slot Bag place of the item.
            void dropItem(int cell, int slot);

            /// @brief The Remote Control starts: an open form's draft is put in force and the form closes.
            void remoteStarting();

        private:
            using Owner = std::tuple<std::uint16_t, std::uint32_t, int>; // server id, login id, character slot

            /// @brief Reads the settings when another character plays.
            void follow();

            /// @brief Hides the form and gives up the draft.
            void close();

            /// @brief The draft with the typed name and the tick.
            /// @return The settings as the form shows them.
            AutoDeliverSettings edited() const;

            /// @brief Checks the draft, puts it in force and saves it.
            /// @param settings The settings.
            /// @return False when refused (the reason is shown).
            bool apply(AutoDeliverSettings settings);

            /// @brief Saves the draft and asks for one trip.
            void sendNow();

            /// @brief Steps the pet through "Any free pet" and the pets of the team.
            /// @param by 1 or -1.
            void stepPet(int by);

            /// @brief Shows or hides the list of names.
            void toggleNames();

            /// @brief Takes the name clicked in the list.
            /// @param row The row.
            void pickName(int row);

            /// @brief Draws a cell and its item.
            /// @param canvas Canvas to draw on.
            /// @param at Position of the cell.
            /// @param cell Cell number 1..10.
            void paintItem(gfx::Canvas& canvas, core::Point at, int cell);

            GameServices& m_services;
            ui::Screen& m_screen;
            AutoDeliver& m_deliver;
            const Inventory& m_inventory;
            const PetTeam& m_pets;
            PlayerMessages& m_messages;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::CheckBox& m_on;
            ui::TextBox& m_to;
            ui::Label& m_online;
            ui::Label& m_offline;
            ui::Label& m_pet;
            ui::Label& m_status;
            ui::Element& m_nameList;
            ui::ListBox& m_names;
            std::array<std::reference_wrapper<ui::DrawingArea>, AutoDeliverSettings::kItems> m_cells;
            AutoDeliverSettings m_draft;
            std::optional<Owner> m_owner;
    };

}

#endif
