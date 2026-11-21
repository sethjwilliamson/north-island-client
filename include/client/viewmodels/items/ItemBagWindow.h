#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_ITEM_BAG_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_ITEM_BAG_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/viewmodels/menus/CountBox.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "gfx/PictureId.h"
#include "protocol/BagItemRecord.h"
#include "protocol/ItemBagRequests.h"
#include "ui/DrawingArea.h"
#include "ui/EventArgs.h"
#include "ui/IconGrid.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/Window.h"

#include <functional>
#include <vector>

namespace client {

    /// @brief Item Bag window: a second bag of 5 x 5 cells.
    /// @details Original: ItemBagWindow.xml, RE_Menu.pas TRe_MiniThingGridForm. Opens with the Space Contract (68/9). Items are
    /// dragged between it and the bag, or within it (Ctrl asks how many); every order goes to the server (68/1-68/3) and the copy
    /// here changes when the same packet comes back.
    class ItemBagWindow {
        public:
            /// @brief Callbacks into the main bag.
            struct Hooks {
                std::function<void(int slot, int count)> takeFromBag;
                std::function<void(int slot, const protocol::ItemInstance& item)> addToBag;
                std::function<void()> closed;
            };

            /// @brief Loads the window and wires its events.
            /// @param services Game services.
            /// @param screen Screen the window is loaded into.
            /// @param bag The main bag (read only).
            /// @param marks Bag cell marks (read only).
            /// @param messages Player message line.
            /// @param countBox Box that asks how many to move.
            /// @param hooks Callbacks into the main bag.
            ItemBagWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& bag,
                const BagMarks& marks,
                PlayerMessages& messages,
                CountBox& countBox,
                Hooks hooks
            );

            /// @brief Tells whether the window is shown.
            /// @return True when visible.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Shows the window.
            /// @details Original: ShowCtrl (68/9).
            void open();

            /// @brief Hides the window.
            /// @details Original: HideCtrl (then RequestNextEventResult).
            void close();

            /// @brief Sets whether the Space Contract is held; without it a panel lies over the cells.
            /// @details Original: FSpaceContract (23/136 kinds 1 and 2).
            /// @param on True when the contract is held.
            void setSpaceContract(bool on) { m_spaceContract = on; }

            /// @brief Replaces all cells with the server's list.
            /// @details Original: RecvMiniGridList (68/4).
            /// @param records Items with their cells.
            void setList(const std::vector<protocol::BagItemRecord>& records);

            /// @brief Applies a move the server confirmed.
            /// @details Original: RecvMiniGridToBag (kind 1), RecvMiniGridMove (2) and RecvBagToMiniGrid (3).
            /// @param kind 1 grid to bag, 2 within the grid, 3 bag to grid.
            /// @param first Source place.
            /// @param second Target place.
            /// @param count Number of items moved.
            void moved(int kind, int first, int second, int count);

            /// @brief Redraws the cell icons.
            /// @details Original: RefreshMiniGridImages.
            void refresh();

            /// @brief The item held in a cell.
            /// @details Original: FMiniGridItems (for RecvThingExpired 9).
            /// @param cell Cell number.
            /// @return The item instance.
            const protocol::ItemInstance& item(int cell) const { return m_grid.bagItem(cell); }

            /// @brief Replaces the item of a cell (an expired item's successor).
            /// @param cell Cell number.
            /// @param item New item, empty to clear.
            void replace(int cell, const protocol::ItemInstance& item);

            /// @brief Handles a bag item let go over the cells.
            /// @details Original: Se_EquipUI HandleGridItemDrop.
            /// @param args Drop event.
            void bagItemDropped(const ui::EventArgs& args);

            /// @brief Updates the qol Sort button and runs the sorter, one move a beat.
            /// @param nowMs Current time in milliseconds.
            void updateSort(std::uint64_t nowMs);

            /// @brief Passes the server's sort answer to the sorter.
            /// @details Original: S 23/250 2 (the server's sort of this grid ended).
            /// @param result Result code from the server.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief Handles an Item Bag item let go over the bag or its own cells.
            /// @details Original: HandleMiniGridDrop.
            /// @param args Drop event.
            void gridItemDropped(const ui::EventArgs& args);

            /// @brief Orders a bag item moved into the grid.
            /// @details Original: RequestBagToMiniGrid (68/1-68/3 family). When not exact the order is for that place only, else the neighbours are tried.
            /// @param bagSlot Bag place of the item.
            /// @param cell Target cell.
            /// @param count Number to move.
            /// @param exact True to try the neighbouring cells when the target does not fit.
            void requestToGrid(int bagSlot, int cell, int count, bool exact);

            /// @brief Orders an item moved within the grid.
            /// @details Original: RequestMiniGridMove.
            /// @param from Source cell.
            /// @param to Target cell.
            /// @param count Number to move.
            /// @param exact True to try the neighbouring cells when the target does not fit.
            void requestMove(int from, int to, int count, bool exact);

            /// @brief Orders a grid item moved into the bag.
            /// @details Original: RequestMiniGridToBag.
            /// @param cell Source cell.
            /// @param bagSlot Target bag place.
            /// @param count Number to move.
            /// @param exact True to try the neighbouring places when the target does not fit.
            void requestToBag(int cell, int bagSlot, int count, bool exact);

            /// @brief Finds the place beside a place.
            /// @details Original: GetNeighborGridSlot (direction 1..9, left to right, row by row).
            /// @param slot Centre place.
            /// @param rows Rows of the grid.
            /// @param direction 1..9.
            /// @return The place, 0 outside a grid of that many rows.
            static int neighbour(int slot, int rows, int direction);

            /// @brief Draws the title and the panel over the cells while there is no Space Contract.
            /// @details Original: DrawControl.
            /// @param canvas Target canvas.
            /// @param at Top-left of the drawing area.
            void paintPage(gfx::Canvas& canvas, core::Point at);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_bag;
            const BagMarks& m_marks;
            PlayerMessages& m_messages;
            CountBox& m_countBox;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_cells;
            const ui::Spot& m_titleSpot;
            Inventory m_grid;
            bool m_spaceContract = false;
            gfx::PictureId m_restrain;
            InventorySorter m_sorter;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
