#ifndef NORTH_ISLAND_CLIENT_MODELS_ITEMS_INVENTORY_H
#define NORTH_ISLAND_CLIENT_MODELS_ITEMS_INVENTORY_H

#include "core/Size.h"
#include "formats/ItemTable.h"
#include "protocol/ItemInstance.h"

#include <array>

namespace client {

    /// @brief The own player's items: the bag grid and the worn equipment.
    /// @details Original: TPlayer.FBagItems, FEquips and the bag grid; 50 places in 5 columns and 10 rows, and 6 equipment slots (1 head, 2 body, 3 weapon, 4 wrist, 5 feet, 6 special).
    /// An item larger than one cell lies in the place of its top left cell and covers the cells to its right and below. The server says where everything goes; the client only finds a free place for what is taken off.
    class Inventory {
        public:
            static constexpr int kBagSlots = 50; // bag places 1..50
            static constexpr int kColumns = 5; // columns of the bag grid
            static constexpr int kRows = 10; // rows of the bag grid
            static constexpr int kEquipmentSlots = 6; // 1 head, 2 body, 3 weapon, 4 wrist, 5 feet, 6 special
            static constexpr int kMaxStack = 50; // most items of one kind in one stack

            /// @brief Creates an empty inventory.
            /// @param items Item table giving sizes and equipment slots.
            /// @param rows Rows of the grid: the bank and the bag have ten, the Item Bag five.
            explicit Inventory(
                const formats::ItemTable& items,
                int rows = kRows
            )
                : m_items(items),
                  m_rows(rows) {}

            /// @brief How many places the grid has (the bag's 50).
            /// @return The place count.
            int slots() const { return kColumns * m_rows; }

            /// @brief Gives every bag place and worn item with an id from first to last the id to.
            /// @details Original: RevertItemVariantsTo20747.
            /// @param first Lowest id replaced.
            /// @param last Highest id replaced.
            /// @param to New id.
            void replaceItemIds(int first, int last, int to);

            /// @brief The item in a place of the bag; empty ones have id 0.
            /// @param slot Place 1..50.
            /// @return The item.
            const protocol::ItemInstance& bagItem(int slot) const { return m_bag.at(static_cast<std::size_t>(slot)); }

            /// @brief The item worn in a slot; empty ones have id 0.
            /// @param slot Slot 1..6.
            /// @return The item.
            const protocol::ItemInstance& equipped(int slot) const { return m_equipment.at(static_cast<std::size_t>(slot)); }

            /// @brief Finds the first place after a place holding some of an item.
            /// @details Original: FindNextBagItemSlot.
            /// @param itemId Item to look for.
            /// @param afterSlot Place to start after.
            /// @return The place, or 0 for none.
            int nextSlotWith(int itemId, int afterSlot = 0) const;

            /// @brief Adds items to a place: an empty place takes the item, a place with the same item stacks up to 50.
            /// @details Original: AddThingItem.
            /// @param slot Place.
            /// @param item Item (with its count) to add.
            /// @return How many were added.
            int addToBag(int slot, const protocol::ItemInstance& item);

            /// @brief Takes up to count items from a place; an emptied place is cleared.
            /// @param slot Place.
            /// @param count Number to take.
            void takeFromBag(int slot, int count);

            /// @brief Takes count of an item, from the lowest places first.
            /// @details Original: RemoveBagItemById.
            /// @param itemId Item to take.
            /// @param count Number to take.
            void removeFromBag(int itemId, int count);

            /// @brief Moves count items from one place to another, as the server's 23/10 answer says: into an empty place or onto the same
            /// item, or a whole stack swapped with a different item in the target.
            /// @details Original: MoveBagItemStack.
            /// @param fromSlot Source place.
            /// @param count Number to move.
            /// @param toSlot Target place.
            void moveInBag(int fromSlot, int count, int toSlot);

            /// @brief Adds to how much of a potion in a place is used up (23/208).
            /// @param slot Place.
            /// @param amount Amount to add.
            void addUsedAmount(int slot, int amount) { m_bag.at(static_cast<std::size_t>(slot)).usedAmount += amount; }

            /// @brief Sets how much of a potion in a place is used up (23/208).
            /// @param slot Place.
            /// @param amount The used amount.
            void setUsedAmount(int slot, int amount) { m_bag.at(static_cast<std::size_t>(slot)).usedAmount = amount; }

            /// @brief Sets the stone mounted in a piece; the Tear Pliers take it out (23/143).
            /// @details Original: FInsertItemId.
            /// @param slot Place.
            /// @param itemId Mounted item id.
            void setInsertItem(int slot, int itemId) { m_bag.at(static_cast<std::size_t>(slot)).insertItemId = itemId; }

            /// @brief Sets a vehicle's fuel.
            /// @details Original: FExtraAmount.
            /// @param slot Place.
            /// @param amount Fuel amount.
            void setExtraAmount(int slot, int amount) { m_bag.at(static_cast<std::size_t>(slot)).extraAmount = amount; }

            /// @brief Sets the fusion level of a bag item (the mall's forge).
            /// @param slot Place.
            /// @param level Fusion level.
            void setFusionLevel(int slot, int level) { m_bag.at(static_cast<std::size_t>(slot)).fusionLevel = level; }

            /// @brief Sets the look of another piece a bag item wears (the pose of the Sewing Tools).
            /// @param slot Place.
            /// @param itemId Item id of the look.
            void setLookItem(int slot, int itemId) { m_bag.at(static_cast<std::size_t>(slot)).lookItemId = itemId; }

            /// @brief Puts an item on, in the slot Item.dat gives it.
            /// @param item Item to wear.
            void equip(const protocol::ItemInstance& item);

            /// @brief Puts a bag item on; what was worn there goes to freeSlot.
            /// @details Original: EquipFromBag.
            /// @param bagSlot Place of the item to wear.
            /// @param freeSlot Place the old item goes to.
            void equipFromBag(int bagSlot, int freeSlot);

            /// @brief Moves what is worn in a slot to a place of the bag.
            /// @details Original: UnequipToBag.
            /// @param equipSlot Equipment slot 1..6.
            /// @param bagSlot Bag place.
            void unequipToBag(int equipSlot, int bagSlot);

            /// @brief Empties an equipment slot.
            /// @param slot Equipment slot 1..6.
            void clearEquipment(int slot);

            /// @brief Sets how worn an item of the bag is (not for a pile).
            /// @details Original: RecvThingWearUpdate.
            /// @param slot Bag place.
            /// @param wear Wear value.
            void setBagWear(int slot, int wear);

            /// @brief Sets how worn a worn item is.
            /// @details Original: RecvEquipWear.
            /// @param slot Equipment slot 1..6.
            /// @param wear Wear value.
            void setEquipmentWear(int slot, int wear);

            /// @brief The ids of what is worn, slot 1..6 at index 0..5.
            /// @return The ids.
            std::array<std::uint16_t, kEquipmentSlots> equipmentIds() const;

            /// @brief The cells an item takes (columns, rows), at least 1 x 1.
            /// @param itemId Item id.
            /// @return The size in cells.
            core::Size cellsOf(int itemId) const;

            /// @brief The place whose item covers a cell.
            /// @param cell Cell 1..50.
            /// @return The place, or 0 for none.
            int occupantOf(int cell) const;

            /// @brief The first place where the item fits on empty cells.
            /// @details Original: FindFreeGridSlot.
            /// @param itemId Item id.
            /// @return The place, or 0 for none.
            int freeSlotFor(int itemId) const;

            /// @brief How many of count items of a kind fit into a place: its cells must lie in the bag and be empty (or taken by ignoreSlot), a stack takes more of its own item up to 50.
            /// @details Original: CheckGridItemFit.
            /// @param slot Target place.
            /// @param itemId Item id.
            /// @param count Number wanted.
            /// @param ignoreSlot Place the item comes from, whose cells count as empty.
            /// @return The number that fits.
            int fitCount(int slot, int itemId, int count, int ignoreSlot) const;

            /// @brief Whether the cells an item would cover from a place lie in the grid and are empty or taken by one of the two places.
            /// @details Original: CanSwapBankItems' CheckArea, CanPlaceAt, CanPlaceInBag.
            /// @param slot Target place.
            /// @param itemId Item id.
            /// @param ownerA First place whose cells count as empty.
            /// @param ownerB Second place whose cells count as empty.
            /// @return True when it fits.
            bool fitsFree(int slot, int itemId, int ownerA, int ownerB) const;

            /// @brief The first place that takes all count items.
            /// @details Original: FindGridSlotFor.
            /// @param itemId Item id.
            /// @param count Number of items.
            /// @return The place, or 0 for none.
            int placeFor(int itemId, int count) const;

            /// @brief Whether the item piles up in one place.
            /// @details Original: IsStackableItem, for the kinds the data shows.
            /// @param itemId Item id.
            /// @return True when stackable.
            bool isStackable(int itemId) const;

        private:
            const formats::ItemTable& m_items;
            int m_rows = kRows;
            std::array<protocol::ItemInstance, kBagSlots + 1> m_bag{};
            std::array<protocol::ItemInstance, kEquipmentSlots + 1> m_equipment{};
    };

}

#endif
