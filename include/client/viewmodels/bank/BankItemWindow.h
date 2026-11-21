#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_BANK_BANK_ITEM_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_BANK_BANK_ITEM_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/rules/BagRequests.h"
#include "client/models/items/Inventory.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "ui/EventArgs.h"
#include "ui/IconGrid.h"
#include "ui/Screen.h"
#include "ui/Window.h"

#include <array>
#include <cstddef>
#include <functional>

namespace client {

    /// @brief Bank item window: the bank and the bag side by side, items marked and moved with the arrows.
    /// @details Original: BankItemWindow.xml, TTH_BankItemForm; packets 30/2 (store) and 30/1 (withdraw).
    class BankItemWindow {
        public:
            /// @brief Builds the window and wires its grids and buttons.
            /// @param services Game services.
            /// @param screen Screen the window lives on.
            /// @param bag The player's bag.
            /// @param bank The bank's items.
            /// @param marks Bag marks shared with the bag window.
            /// @param messages Player message sink.
            /// @param bagChanged Called when the bag contents changed.
            /// @param requests Bag request sender.
            BankItemWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& bag,
                const Inventory& bank,
                BagMarks& marks,
                PlayerMessages& messages,
                std::function<void()> bagChanged,
                BagRequests& requests
            );

            /// @brief Tells whether the window is visible.
            /// @return True while open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the window with no marks on either side.
            /// @details Original: ShowCtrl; onClosed runs on HideCtrl.
            /// @param onClosed Called when the window closes.
            void show(std::function<void()> onClosed);

            /// @brief The server finished a withdrawal or deposit; clears its marks.
            /// @details Original: server 30/6 (withdrawal) and 30/7 (deposit).
            /// @param deposit True for a deposit, false for a withdrawal.
            void done(bool deposit);

            /// @brief Redraws both grids after the bag or bank changed.
            /// @details Original: RefreshGridItems.
            void refresh();

            /// @brief Handles a bag item dropped over the bank's places.
            /// @details Original: DropBagItemToBank.
            /// @param args Drop event data.
            void bagItemDropped(const ui::EventArgs& args);

            /// @brief Advances the qol sort, one move per beat.
            /// @param nowMs Current time in milliseconds.
            void updateSort(std::uint64_t nowMs);

            /// @brief The server's sort of this grid ended.
            /// @details Original: server 23/250 sub 2.
            /// @param result Server result code.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief Toggles the mark of a bag slot.
            /// @details Original: ToggleBagItemForBank.
            /// @param slot Bag slot.
            void toggleBag(int slot);

            /// @brief Toggles the mark of a bank slot.
            /// @details Original: ToggleBankItemSelect.
            /// @param slot Bank slot.
            void toggleBank(int slot);

            /// @brief Removes the mark of a bag slot.
            /// @details Original: UnmarkBagItemForBank.
            /// @param slot Bag slot.
            void unmarkBag(int slot);

            /// @brief Removes the mark of a bank slot.
            /// @details Original: UnmarkBankItem.
            /// @param slot Bank slot.
            void unmarkBank(int slot);

            /// @brief Sends the marked bag items to the bank.
            /// @details Original: StoreMarkedItems (30/2).
            void store();

            /// @brief Takes the marked bank items into the bag.
            /// @details Original: WithdrawMarkedItems (30/1).
            void withdraw();

            /// @brief Clears all bag marks.
            void clearBagMarks();

            /// @brief Closes the window.
            void close();

            /// @brief Handles a bank item dropped over the bank or the bag.
            /// @details Original: HandleBankItemDrop (MoveBankItem or WithdrawBankItemTo).
            /// @param args Drop event data.
            void bankItemDropped(const ui::EventArgs& args);

            /// @brief Moves a bank item to another bank place.
            /// @details Original: MoveBankItem, with CanSwapBankItems.
            /// @param fromSlot Source bank slot.
            /// @param toSlot Target bank slot.
            /// @param dropTarget Place the mouse was over.
            void moveBankItem(int fromSlot, int toSlot, int dropTarget);

            /// @brief Checks whether two bank places may be swapped, adjusting the slots.
            /// @details Original: CanSwapBankItems.
            /// @param fromSlot Source slot, may be adjusted.
            /// @param toSlot Target slot, may be adjusted.
            /// @param dropTarget Place the mouse was over.
            /// @return True when the move is allowed.
            bool canSwapBankItems(int& fromSlot, int& toSlot, int dropTarget) const;

            /// @brief Stores a dropped bag item into a bank place.
            /// @details Original: DropBagItemToBank.
            /// @param bagSlot Bag slot.
            /// @param bankSlot Bank slot.
            void dropBagItemToBank(int bagSlot, int bankSlot);

            /// @brief Handles a bag item dropped on the bag side: onto a place or off the form.
            /// @details Original: HandleGridItemDrop (RequestMoveBagItem or RequestDropBagItem).
            /// @param args Drop event data.
            void moveBagItem(const ui::EventArgs& args);

            /// @brief Asks the server to move a bag item.
            /// @details Original: RequestMoveBagItem.
            /// @param fromSlot Source bag slot.
            /// @param count Number of items to move.
            /// @param toSlot Target bag slot.
            /// @param searchNear Whether to look for a free place near the target.
            void requestMoveBagItem(int fromSlot, int count, int toSlot, bool searchNear);

            /// @brief Withdraws a bank item into a bag slot.
            /// @details Original: WithdrawBankItemTo.
            /// @param bankSlot Bank slot.
            /// @param bagSlot Bag slot.
            void withdrawBankItemTo(int bankSlot, int bagSlot);

            /// @brief Stores one bag stack at once, the modern way's Shift click: the server tops up the bank's stacks of it, then
            /// takes a free place (30/2 with one slot).
            /// @param bagSlot The bag place.
            void quickStore(int bagSlot);

            /// @brief Takes one bank stack to the bag at once, the modern way's Shift click (30/1 with one slot).
            /// @param bankSlot The bank place.
            void quickWithdraw(int bankSlot);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_bag;
            const Inventory& m_bank;
            BagMarks& m_marks;
            PlayerMessages& m_messages;
            std::function<void()> m_bagChanged;
            BagRequests& m_requests;
            ui::Window& m_window;
            ui::IconGrid& m_bankGrid;
            ui::IconGrid& m_bagGrid;
            std::array<bool, Inventory::kBagSlots + 1> m_bankMarked{};
            int m_dragSlot = 0;
            bool m_dropHandled = false;
            bool m_withdrawPending = false;
            bool m_storePending = false;
            int m_quickStores = 0; // Shift click stores the server has not finished: their end leaves the marks as they are
            int m_quickWithdraws = 0; // the same for Shift click withdrawals
            std::function<void()> m_onClosed;
            InventorySorter m_sorter;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
