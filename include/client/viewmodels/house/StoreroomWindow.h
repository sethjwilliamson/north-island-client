#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_HOUSE_STOREROOM_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_HOUSE_STOREROOM_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/viewmodels/menus/CountBox.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "protocol/StoreroomLists.h"
#include "ui/DrawingArea.h"
#include "ui/EventArgs.h"
#include "ui/IconGrid.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/Window.h"

#include <functional>

namespace client {

    /// @brief The Storeroom form: the 50 cells of the piece 39030 (or of the Super Handbag, which opens it anywhere), with items dragged between it and the bag or within it.
    /// @details Original: StoreroomWindow.xml, Se_EquipUI.pas TRe_GarageForm ("garage" in the code); a drag with Ctrl asks how many; every order goes to the server (62/7 to 62/9) and the copy here changes when the server sends the cell back (62/10 to 62/13).
    class StoreroomWindow {
        public:
            /// @brief Callbacks the window needs from its owner.
            struct Hooks {
                std::function<bool(int piece)> pieceElsewhere;
                std::function<bool(int cell, int itemId, int count, int piece)> dropOnWorld;
            };

            /// @brief Loads the form and wires its events.
            /// @param services Game services (layouts, items, connection).
            /// @param screen Screen the form is loaded into.
            /// @param bag The player's bag.
            /// @param marks Bag cell marks.
            /// @param messages Sink for player notices.
            /// @param countBox Box asking how many to move.
            /// @param hooks Owner callbacks.
            StoreroomWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& bag,
                const BagMarks& marks,
                PlayerMessages& messages,
                CountBox& countBox,
                Hooks hooks
            );

            /// @brief Tells whether the form is visible.
            /// @return True when open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the form for a Storeroom piece.
            /// @param piece Index of the furniture piece.
            /// @details Original: HandleUtensilDrop on the piece 39030 (FFurnIndex and ShowCtrl).
            void open(int piece);

            /// @brief Opens the form for the piece the list named.
            /// @details Original: 62/50 (the Super Handbag's answer), ShowCtrl.
            void open();

            /// @brief Hides the form.
            void close();

            /// @brief Replaces the whole item list and the piece it belongs to.
            /// @param list The list from the server.
            /// @details Original: RecvGarageItemList (62/7).
            void setList(const protocol::StoreroomList& list);

            /// @brief Adds an item into the named cell, or the first place it fits.
            /// @param add The add message.
            /// @details Original: AddGarageItem (62/11) and RecvGarageItemAdd (62/10: no cell named).
            void add(const protocol::StoreroomAdd& add);

            /// @brief Removes items by cell or by the first cell holding the item.
            /// @param remove The remove message.
            /// @details Original: RemoveGarageItem (62/13) and RemoveGarageItemById (62/12).
            void remove(const protocol::StoreroomRemove& remove);

            /// @brief Reads the item in a cell.
            /// @param cell Cell 1..50.
            /// @return The item instance.
            const protocol::ItemInstance& item(int cell) const { return m_grid.bagItem(cell); }

            /// @brief Handles a bag item let go over the cells: sends it in, by its top left cell as held (Ctrl asks how many).
            /// @param args Drop event.
            /// @details Original: Se_EquipUI HandleGridItemDrop.
            void bagItemDropped(const ui::EventArgs& args);

            /// @brief Sends a bag stack in at once, the modern way's Shift click: it tops up stacks of the same thing, then takes a free
            /// cell; what does not fit stays in the bag.
            /// @param bagSlot The bag place.
            /// @return True when the Storeroom is open and took the click (even when nothing fits).
            bool quickPut(int bagSlot);

            /// @brief Redraws the cells from the item copy.
            /// @details Original: RefreshGarageItems.
            void refresh();

            /// @brief Shows the qol Sort button while sortButtons is on and runs the sort one move at a time.
            /// @param nowMs Current time in ms.
            void updateSort(std::uint64_t nowMs);

            /// @brief Tells the sorter that the server's sort of this grid ended.
            /// @param result Result code of the server answer.
            /// @details Original: server action 23/250 sub 2.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief Handles an item of the Storeroom let go on a bag place, a Storeroom cell, or off the form.
            /// @param args Drop event.
            /// @details Original: HandleGridItemDrop for an item of the Storeroom.
            void gridItemDropped(const ui::EventArgs& args);

            /// @brief Sends a bag item to a Storeroom cell.
            /// @param bagSlot Source bag slot.
            /// @param cell Target Storeroom cell.
            /// @param count How many.
            /// @param exact True to try the neighbouring cells when the place does not fit.
            /// @details Original: RequestBagToGarage.
            void requestPut(int bagSlot, int cell, int count, bool exact);

            /// @brief Moves an item within the Storeroom.
            /// @param from Source cell.
            /// @param to Target cell.
            /// @param count How many.
            /// @param exact True to try the neighbouring cells when the place does not fit.
            /// @details Original: RequestMoveInGarage.
            void requestMove(int from, int to, int count, bool exact);

            /// @brief Takes an item from a Storeroom cell to a bag slot.
            /// @param cell Source Storeroom cell.
            /// @param bagSlot Target bag slot.
            /// @param count How many.
            /// @param exact True to try the neighbouring places when the place does not fit.
            /// @details Original: RequestGarageToBag.
            void requestTake(int cell, int bagSlot, int count, bool exact);

            /// @brief Finds the place beside a place.
            /// @param slot Place 1-based.
            /// @param rows Number of rows of the grid.
            /// @param direction 1..9, left to right, row by row (5 is the place itself).
            /// @return The neighbouring place, or 0 outside the grid.
            /// @details Original: GetNeighborGridSlot.
            static int neighbour(int slot, int rows, int direction);

            /// @brief Tells whether things move the modern way (the qol setting modernBag).
            /// @return True when on.
            bool modern() const;

            /// @brief Gives how many things a drop moves the modern way: one put down, the part taken up, or else the whole stack.
            /// @param args Drop event.
            /// @param stackCount Count of the stack the drop comes from.
            /// @return The count.
            static int carriedCount(const ui::EventArgs& args, int stackCount);

            /// @brief Takes up half of a Storeroom stack the modern way, to be put down by a click.
            /// @param cell Storeroom cell 1..50.
            void rightClickCell(int cell);

            /// @brief Takes a Storeroom stack to the bag at once, the modern way's Shift click: it tops up stacks of the same thing,
            /// then takes a free place; what does not fit stays.
            /// @param cell Storeroom cell 1..50.
            void quickTake(int cell);

            /// @brief Tells whether a bag place's thing may go into the Storeroom, saying why not when it may not.
            /// @param bagSlot The bag place.
            /// @return True when it may.
            bool storable(int bagSlot);

            /// @brief Closes the form when the piece is in another room than the player.
            /// @return True when the form went and the drop is lost.
            /// @details Original: HandleGridItemDrop.
            bool pieceGone();

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_bag;
            const BagMarks& m_marks;
            PlayerMessages& m_messages;
            CountBox& m_countBox;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_cells;
            const ui::Spot& m_iconSpot;
            Inventory m_grid;
            int m_piece = 0;
            InventorySorter m_sorter;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
