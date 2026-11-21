#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_EXP_BOX_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_EXP_BOX_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/rules/ExpBagAutoUse.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "core/Bytes.h"
#include "core/Rect.h"
#include "ui/IconGrid.h"
#include "ui/ImageButton.h"
#include "ui/Screen.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace client {

    /// @brief EXP Bag window holding ten cells of EXP potions, cards and statues.
    /// @details Original: ExpBoxWindow.xml, CJ_Menu.pas TExpBoxForm, item 34377. Items are equipment slot 8, sub type 127.
    /// Drop on a cell = 23/132 op 1, right button takes a stack back (op 2), double click drinks one (op 3); the server
    /// sends the cells (23/218).
    class ExpBoxWindow {
        public:
            /// @brief Callbacks into the rest of the game.
            struct Hooks {
                std::function<std::optional<core::Rect>()> itemWindowBounds;
                std::function<int()> fightState;
                std::function<bool()> sportActive;
                std::function<void(const std::string&, int)> centreMessage;
                std::function<void()> marksChanged;
                std::function<bool()> onPvpServer;
                std::function<bool()> doubleExpEnded;
            };

            static constexpr int kCells = 10; // item cells in the box, 1-based
            static constexpr int kMark = BagMarks::kExpBox;
            static constexpr int kBag = 34377; // item 34377: EXP Bag (type 25)

            /// @brief Loads the window and wires its events.
            /// @param services Game services.
            /// @param screen Screen the window is loaded into.
            /// @param inventory Player inventory (read only).
            /// @param marks Bag cell marks.
            /// @param messages Player message line.
            /// @param hooks Callbacks into the rest of the game.
            ExpBoxWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& inventory,
                BagMarks& marks,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Tells whether the window is shown.
            /// @return True when visible.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the form for the EXP Bag at a bag place.
            /// @details Original: OpenExpBoxForm.
            /// @param bagSlot Bag place of the EXP Bag.
            void open(int bagSlot);

            /// @brief Hides the form and frees the bag's cell mark unless auto use is on.
            /// @details Original: HideCtrl.
            void hide();

            /// @brief Puts a bag item into the cell under the mouse.
            /// @details Original: PutBagItemInExpBox.
            /// @param bagSlot Bag place of the dropped item.
            void put(int bagSlot);

            /// @brief Handles the server's EXP box packet.
            /// @details Original: RecvExpBoxPacket (23/218): sub 1 a cell holds more, 2 it is empty, 3 one was used.
            /// @param payload Packet bytes.
            void receive(const core::Bytes& payload);

            /// @brief Updates the qol Sort button and runs the sorter each frame.
            /// @param nowMs Current time in milliseconds.
            void updateSort(std::uint64_t nowMs);

            /// @brief Passes the server's sort answer to the sorter.
            /// @details Original: S 23/250 2 (the server's sort of the box ended).
            /// @param result Result code from the server.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

            /// @brief Drinks the next filled cell when auto use is due.
            /// @details Original: CheckAutoUse, called from the game's frame.
            void checkAutoUse();

        private:
            using Cell = ExpBagAutoUse::Cell;

            /// @brief Shows the form beside the item window and marks the bag's cell.
            /// @details Original: ShowCtrl.
            void show();

            /// @brief Takes a stack out of a cell back into the bag.
            /// @details Original: TakeItemFromExpBox.
            /// @param cell Cell number (1..kCells).
            void take(int cell);

            /// @brief Asks the server to drink one item of a cell.
            /// @details Original: UseExpBoxItem.
            /// @param cell Cell number (1..kCells).
            void use(int cell);

            /// @brief Flips the auto use tick.
            /// @details Original: ToggleAutoUseCheck.
            void toggleAuto();

            /// @brief Keeps the auto use tick and hides the form.
            /// @details Original: ConfirmExpBox.
            void confirm();

            /// @brief Redraws the cell grid from the stored cells.
            void refresh();

            /// @brief Tells whether a cell number is in range.
            /// @param cell Cell number.
            /// @return True when 1..kCells.
            bool validCell(int cell) const { return cell >= 1 && cell <= kCells; }

            /// @brief Finds the cell under the mouse.
            /// @return Cell number, or 0 when none.
            int cellUnderMouse() const;

            /// @brief Handles sub 1 of the packet: a cell holds more.
            /// @details Original: RecvExpBoxAdd: u8 cell, u16 item, u8 count, u8 notify.
            /// @param payload Packet bytes.
            void receiveAdd(const core::Bytes& payload);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            PlayerMessages& m_messages;
            InventorySorter m_sorter;
            static constexpr int kConfirmX = 47, kCancelX = 125,
                                 kSortShift = 20; // x of the Confirm and Cancel buttons, px; px they move left while the sort panel shows
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_grid;
            ui::ImageButton& m_check;
            gfx::PictureId m_checkedPicture;
            gfx::PictureId m_uncheckedPicture;
            std::array<Cell, kCells + 1> m_cells{};
            int m_bagSlot = 0;
            bool m_autoUse = false;
            bool m_autoEdit = false;
            bool m_using = false;
            bool m_autoDone = false;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
