#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_PICNIC_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_PICNIC_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/PetTeam.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "core/Bytes.h"
#include "core/Rect.h"
#include "ui/IconGrid.h"
#include "ui/Screen.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace client {

    /// @brief The Picnic Box form: fifteen cells of HP/SP food kept in the box.
    /// @details Original: PicnicWindow.xml, CJ_Menu.pas TPicnicForm, item 34376. 23/131 op 1 puts a bag food in a cell, op 2 takes a stack back, op 3 uses one on a target; the server sends the cells in 23/217.
    class PicnicWindow {
        public:
            /// @brief Callbacks into the surrounding scene.
            struct Hooks {
                std::function<std::optional<core::Rect>()> itemWindowBounds;
                std::function<int()> fightState;
                std::function<std::string()> ownName;
                std::function<std::string()> activePetName;
                std::function<void(const std::string&)> chatLine;
                std::function<void(const std::string&, int)> centreMessage;
                std::function<void(bool)> aim;
                std::function<bool(bool sp)> playerLacks;
                std::function<void()> marksChanged;
            };

            static constexpr int kCells = 15; // item cells in the picnic box, 1-based
            static constexpr int kMark = BagMarks::kPicnic;
            static constexpr int kBox = 34376; // item 34376: Picnic Box (type 25)

            /// @brief Builds the form and wires its controls.
            /// @param services Shared game services.
            /// @param screen Screen the form lives on.
            /// @param inventory Bag contents.
            /// @param marks Bag cell marks.
            /// @param pets Pet team.
            /// @param messages Player message sink.
            /// @param hooks Scene callbacks.
            PicnicWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& inventory,
                BagMarks& marks,
                const PetTeam& pets,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Whether the form is visible.
            /// @return True when open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the form for the Picnic Box of a bag place.
            /// @param bagSlot Bag place of the box.
            /// @details Original: OpenPicnicForm.
            void open(int bagSlot);

            /// @brief Hides the form and frees the box's cell mark.
            /// @details Original: HideCtrl.
            void hide();

            /// @brief A bag item was dropped on the cells; asks the server to store it.
            /// @param bagSlot Bag place of the food.
            /// @details Original: PutBagItemInPicnic (23/131 op 1).
            void put(int bagSlot);

            /// @brief Ends the target cursor.
            void cancelAim();

            /// @brief The target cursor's click: uses the chosen food on the player or a pet.
            /// @param target 1 the player, 4 a pet of his.
            /// @param petPlace Pet team place when target is 4.
            /// @details Original: UsePicnicItemOnTarget (23/131 op 3).
            void useOn(int target, int petPlace);

            /// @brief Handles a server 23/217 packet.
            /// @param payload Packet bytes; byte 2 is the sub id: 1 a cell holds more, 2 it is empty, 3 one was used.
            /// @details Original: RecvPicnicPacket.
            void receive(const core::Bytes& payload);

            /// @brief Updates the qol Sort button (server sort only; the box has no move inside it).
            /// @param nowMs Current time in ms.
            void updateSort(std::uint64_t nowMs);

            /// @brief The server's sort of the box ended.
            /// @param result Result code.
            /// @details Server 23/250 2.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief One cell of the box: an item and its count.
            struct Cell {
                int itemId = 0;
                int count = 0;
            };

            /// @brief Shows the form beside the item window with the cells as the player has them.
            /// @details Original: ShowCtrl.
            void show();

            /// @brief Asks the server to give a cell's stack back to the bag.
            /// @param cell Cell 1..15.
            /// @details Original: TakeItemFromPicnic (23/131 op 2).
            void take(int cell);

            /// @brief Double click on a cell: turns on the target cursor.
            /// @param cell Cell 1..15.
            /// @details Original: UsePicnicItem.
            void use(int cell);

            /// @brief Redraws all cells.
            void refresh();

            /// @brief Whether a cell number is in range.
            /// @param cell Cell number.
            /// @return True for 1..kCells.
            bool validCell(int cell) const { return cell >= 1 && cell <= kCells; }

            /// @brief The cell under the mouse.
            /// @return The cell, 0 for none.
            int cellUnderMouse() const;

            /// @brief Applies a server "cell holds more" packet.
            /// @param payload Packet bytes.
            /// @details Original: RecvPicnicAdd.
            void receiveAdd(const core::Bytes& payload);

            /// @brief Applies a server "one was used" packet.
            /// @param payload Packet bytes.
            /// @details Original: RecvPicnicUsed.
            void receiveUsed(const core::Bytes& payload);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            const PetTeam& m_pets;
            PlayerMessages& m_messages;
            InventorySorter m_sorter;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_grid;
            std::array<Cell, kCells + 1> m_cells{};
            int m_bagSlot = 0;
            int m_useSlot = 0;
            bool m_aiming = false;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
