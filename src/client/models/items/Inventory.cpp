#include "client/models/items/Inventory.h"

#include "client/rules/ItemInfo.h"

#include <algorithm>

namespace client {

    namespace {

        /// @brief Tells whether a place number lies in 1..last.
        /// @param slot Place number.
        /// @param last Highest valid place.
        /// @return True when valid.
        bool validSlot(int slot, int last) { return slot >= 1 && slot <= last; }

    }

    int Inventory::nextSlotWith(int itemId, int afterSlot) const {
        for (int slot = afterSlot + 1; slot <= slots(); ++slot) {
            const protocol::ItemInstance& item = bagItem(slot);
            if (item.itemId == itemId && item.count > 0) {
                return slot;
            }
        }
        return 0;
    }

    int Inventory::addToBag(int slot, const protocol::ItemInstance& item) {
        if (!validSlot(slot, slots()) || item.empty() || item.count < 1) {
            return 0;
        }
        protocol::ItemInstance& place = m_bag[static_cast<std::size_t>(slot)];
        if (place.empty()) {
            place = item;
            place.count = 0;
        } else if (place.itemId != item.itemId) {
            return 0;
        }
        const int added = std::min(item.count, kMaxStack - place.count);
        place.count += added;
        return added;
    }

    void Inventory::replaceItemIds(int first, int last, int to) {
        for (auto& item : m_bag) {
            if (item.itemId >= first && item.itemId <= last) {
                item.itemId = to;
            }
        }
        for (auto& item : m_equipment) {
            if (item.itemId >= first && item.itemId <= last) {
                item.itemId = to;
            }
        }
    }

    void Inventory::takeFromBag(int slot, int count) {
        if (!validSlot(slot, slots())) {
            return;
        }
        protocol::ItemInstance& place = m_bag[static_cast<std::size_t>(slot)];
        place.count -= std::min(count, place.count);
        if (place.count <= 0) {
            place = {};
        }
    }

    void Inventory::removeFromBag(int itemId, int count) {
        for (int slot = 1; slot <= slots() && count > 0; ++slot) {
            protocol::ItemInstance& place = m_bag[static_cast<std::size_t>(slot)];
            if (place.itemId == itemId && place.count > 0) {
                const int taken = std::min(count, place.count);
                takeFromBag(slot, taken);
                count -= taken;
            }
        }
    }

    void Inventory::moveInBag(int fromSlot, int count, int toSlot) {
        if (!validSlot(fromSlot, slots()) || !validSlot(toSlot, slots())) {
            return;
        }
        protocol::ItemInstance& from = m_bag[static_cast<std::size_t>(fromSlot)];
        protocol::ItemInstance& to = m_bag[static_cast<std::size_t>(toSlot)];
        if (from.empty() || from.count < count || fromSlot == toSlot) {
            return;
        }
        if (!to.empty() && to.itemId != from.itemId) {
            std::swap(from, to); // the server swaps a whole stack moved onto a different item
            return;
        }
        protocol::ItemInstance moved = from;
        moved.count = count;
        takeFromBag(fromSlot, addToBag(toSlot, moved)); // only what the target took leaves the source
    }

    void Inventory::equip(const protocol::ItemInstance& item) {
        const auto record = m_items.find(item.itemId);
        if (!record || !validSlot(record->equipSlot, kEquipmentSlots)) {
            return;
        }
        protocol::ItemInstance& worn = m_equipment[static_cast<std::size_t>(record->equipSlot)];
        worn = item;
        worn.count = 1;
    }

    void Inventory::equipFromBag(int bagSlot, int freeSlot) {
        if (!validSlot(bagSlot, slots())) {
            return;
        }
        const protocol::ItemInstance item = m_bag[static_cast<std::size_t>(bagSlot)];
        const auto record = m_items.find(item.itemId);
        if (!record || !validSlot(record->equipSlot, kEquipmentSlots)) {
            return;
        }
        takeFromBag(bagSlot, 1);
        unequipToBag(record->equipSlot, freeSlot);
        equip(item);
    }

    void Inventory::setBagWear(int slot, int wear) {
        constexpr int kDestroyed = 250; // highest wear value: the item is worn out
        if (validSlot(slot, slots())) {
            protocol::ItemInstance& item = m_bag[static_cast<std::size_t>(slot)];
            if (item.itemId != 0 && !isStackable(item.itemId)) {
                item.wear = std::min(wear, kDestroyed);
            }
        }
    }

    void Inventory::setEquipmentWear(int slot, int wear) {
        if (validSlot(slot, kEquipmentSlots)) {
            m_equipment[static_cast<std::size_t>(slot)].wear = wear;
        }
    }

    void Inventory::unequipToBag(int equipSlot, int bagSlot) {
        if (!validSlot(equipSlot, kEquipmentSlots)) {
            return;
        }
        protocol::ItemInstance& worn = m_equipment[static_cast<std::size_t>(equipSlot)];
        if (worn.empty()) {
            return;
        }
        addToBag(bagSlot, worn);
        worn = {};
    }

    void Inventory::clearEquipment(int slot) {
        if (validSlot(slot, kEquipmentSlots)) {
            m_equipment[static_cast<std::size_t>(slot)] = {};
        }
    }

    std::array<std::uint16_t, Inventory::kEquipmentSlots> Inventory::equipmentIds() const {
        std::array<std::uint16_t, kEquipmentSlots> ids{};
        for (int slot = 1; slot <= kEquipmentSlots; ++slot) {
            ids[static_cast<std::size_t>(slot - 1)] = static_cast<std::uint16_t>(m_equipment[static_cast<std::size_t>(slot)].itemId);
        }
        return ids;
    }

    core::Size Inventory::cellsOf(int itemId) const {
        const auto record = m_items.find(itemId);
        if (!record) {
            return {1, 1};
        }
        return {record->cellWidth, record->cellHeight};
    }

    int Inventory::occupantOf(int cell) const {
        const int row = (cell - 1) / kColumns;
        const int column = (cell - 1) % kColumns;
        for (int slot = 1; slot <= slots(); ++slot) {
            const protocol::ItemInstance& place = m_bag[static_cast<std::size_t>(slot)];
            if (place.empty()) {
                continue;
            }
            const core::Size size = cellsOf(place.itemId);
            const int top = (slot - 1) / kColumns;
            const int left = (slot - 1) % kColumns;
            if (row >= top && row < top + size.height && column >= left && column < left + size.width) {
                return slot;
            }
        }
        return 0;
    }

    int Inventory::freeSlotFor(int itemId) const {
        if (!m_items.find(itemId)) {
            return 0;
        }
        const core::Size size = cellsOf(itemId);
        for (int slot = 1; slot <= slots(); ++slot) {
            const int top = (slot - 1) / kColumns;
            const int left = (slot - 1) % kColumns;
            if (left + size.width > kColumns || top + size.height > m_rows) {
                continue;
            }
            bool free = true;
            for (int row = top; row < top + size.height && free; ++row) {
                for (int column = left; column < left + size.width && free; ++column) {
                    free = occupantOf(row * kColumns + column + 1) == 0;
                }
            }
            if (free) {
                return slot;
            }
        }
        return 0;
    }

    int Inventory::fitCount(int slot, int itemId, int count, int ignoreSlot) const {
        if (!validSlot(slot, slots()) || !m_items.find(itemId)) {
            return 0;
        }
        const core::Size size = cellsOf(itemId);
        const int top = (slot - 1) / kColumns;
        const int left = (slot - 1) % kColumns;
        if (left + size.width > kColumns || top + size.height > m_rows) {
            return 0;
        }
        const bool stackable = isStackable(itemId);
        for (int row = top; row < top + size.height; ++row) {
            for (int column = left; column < left + size.width; ++column) {
                const int occupant = occupantOf(row * kColumns + column + 1);
                if (occupant == 0 || occupant == ignoreSlot) {
                    continue;
                }
                const protocol::ItemInstance& there = m_bag[static_cast<std::size_t>(occupant)];
                if (!stackable || there.itemId != itemId || there.count >= kMaxStack) {
                    return 0;
                }
            }
        }
        if (!stackable) {
            return 1;
        }
        const int there = m_bag[static_cast<std::size_t>(slot)].count;
        return std::min(count, kMaxStack - there);
    }

    bool Inventory::fitsFree(int slot, int itemId, int ownerA, int ownerB) const {
        if (!validSlot(slot, slots())) {
            return false;
        }
        const core::Size size = cellsOf(itemId);
        const int top = (slot - 1) / kColumns;
        const int left = (slot - 1) % kColumns;
        if (left + size.width > kColumns || top + size.height > m_rows) {
            return false;
        }
        for (int row = top; row < top + size.height; ++row) {
            for (int column = left; column < left + size.width; ++column) {
                const int occupant = occupantOf(row * kColumns + column + 1);
                if (occupant != 0 && occupant != ownerA && occupant != ownerB) {
                    return false;
                }
            }
        }
        return true;
    }

    int Inventory::placeFor(int itemId, int count) const {
        for (int slot = 1; slot <= slots(); ++slot) {
            if (fitCount(slot, itemId, count, 0) == count) {
                return slot;
            }
        }
        return 0;
    }

    bool Inventory::isStackable(int itemId) const {
        constexpr int kOwnPlaceSlot = 10; // equipSlot value 10: item with its own placing (not a worn slot 1..6); never stacks
        constexpr int kTimedSubType = 130; // item sub type 130: time-limited items; never stack
        const auto record = m_items.find(itemId);
        if (!record) {
            return false;
        }
        constexpr int kVehicleType = 39; // item type 39: vehicles (Boat, Airship, Hot-air Balloon, ...)
        constexpr int kBoundSlot = 9; // equipSlot value 9: bound item slot; never stacks
        const bool equipment = record->equipSlot >= 1 && record->equipSlot <= kEquipmentSlots;
        const bool large = record->cellWidth > 1 || record->cellHeight > 1;
        if (equipment || large || record->equipSlot == kOwnPlaceSlot || record->equipSlot == kBoundSlot || usePeriod(*record) != 0 ||
            record->subType == kTimedSubType || record->type == kVehicleType || bottleKind(*record) != 0) {
            return false;
        }
        switch (itemId) {
            case 39074: // Magic Sandglass
            case 39097: // Magic Sandglass
            case 34106: // super handbag (Big5 name)
            case 34152: // Tear Plier
            case 34155: // Small Tear Plier
            case 34022: // Tent Palette
            case 34017: // Dog Bone
            case 34160: // Shape Clay
            case 34163: // Teleport Scroll
            case 34164: // Escape Button
            case 34168: // Time Mantle
            case 34180: // Stopwatch (all cases above and here: item ids IsStackableItem excludes from stacking (sandglasses, handbag, pliers, ...)
                return false;
            default: // every other item stacks
                return true;
        }
    }

}
