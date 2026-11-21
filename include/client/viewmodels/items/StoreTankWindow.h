#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_STORE_TANK_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_STORE_TANK_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/viewmodels/items/ItemInfoPopup.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "core/Bytes.h"
#include "protocol/ItemInstance.h"
#include "ui/IconGrid.h"
#include "ui/Screen.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>

namespace client {

    /// @brief The Store Tank form: four rows of five places, each row holding one vehicle fuel up to 250.
    /// @details Original: StoreTankWindow.xml, HL_Menu.pas TStoreTankForm, item 34399. 23/134 1 puts fuel in, 23/134 2 takes it out, 23/134 3 pours the ridden vehicle's fuel; the server tells a row's content in 23/219.
    class StoreTankWindow {
        public:
            /// @brief Callbacks into the surrounding scene.
            struct Hooks {
                std::function<void(const core::Bytes&)> send;
                std::function<bool()> fighting;
                std::function<int()> vehicleItem;
            };

            /// @brief Builds the form and wires its controls.
            /// @param services Shared game services.
            /// @param screen Screen the form lives on.
            /// @param inventory Bag contents.
            /// @param marks Bag cell marks.
            /// @param messages Player message sink.
            /// @param info Item info popup.
            /// @param hooks Scene callbacks.
            StoreTankWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& inventory,
                const BagMarks& marks,
                PlayerMessages& messages,
                ItemInfoPopup& info,
                Hooks hooks
            );

            /// @brief Whether the form is visible.
            /// @return True when open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the form of the Tank in a bag place.
            /// @param bagSlot Bag place of the Tank.
            /// @details Original: FBagSlot and ShowCtrl.
            void open(int bagSlot);

            /// @brief Hides the form and forgets the Tank's place.
            /// @details Original: HideCtrl, ResetStoreTank.
            void close();

            /// @brief Uses the Tank: pours the ridden vehicle's fuel from it.
            /// @param bagSlot Bag place of the Tank.
            /// @details Original: UseStoreTank (23/134 3).
            void use(int bagSlot);

            /// @brief A bag item was let go on the form or on a row; asks the server to store it.
            /// @param bagSlot Bag place of the fuel.
            /// @param row Row 1..4, or 0 for the form (picks a row).
            /// @details Original: PutStoreTankItem (23/134 1).
            void put(int bagSlot, int row);

            /// @brief Handles a server 23/219 packet: what a row holds now.
            /// @param kind 1 an item went in, 2 one came out.
            /// @param row Row 1..4.
            /// @param itemId Fuel item id.
            /// @param count Fuel count in the row.
            /// @details Original: RecvStoreTankPacket.
            void received(int kind, int row, int itemId, int count);

            /// @brief Closes the form when the Tank is gone from its place; shows the info of the item under the mouse.
            /// @details Original: DrawControl.
            void update();

            /// @brief Updates the qol Sort button (server sort only; the box has no move inside it).
            /// @param nowMs Current time in ms.
            void updateSort(std::uint64_t nowMs);

            /// @brief The server's sort of the box ended.
            /// @param result Result code.
            /// @details Server 23/250 2.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief Takes one place's fuel out into the bag.
            /// @param place Place 1..20, row by row.
            /// @details Original: TakeStoreTankItem (23/134 2).
            void take(int place);

            /// @brief Redraws all places.
            /// @details Original: RefreshStoreTankSlots.
            void refresh();

            /// @brief What a place of a row shows (50 to a place).
            /// @param row Row 1..4.
            /// @param column Column 1..5.
            /// @return The fuel count in that place.
            int placeCount(int row, int column) const;

            static constexpr int kRows = 4;
            static constexpr int kColumns = 5;

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_inventory;
            const BagMarks& m_marks;
            PlayerMessages& m_messages;
            InventorySorter m_sorter;
            ItemInfoPopup& m_info;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_cells;
            std::array<protocol::ItemInstance, kRows + 1> m_tank{};
            int m_bagSlot = 0;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
