#include "client/models/house/HomeFurniture.h"

#include "client/rules/FurnitureKinds.h"
#include "client/models/house/HouseGrid.h"

#include <algorithm>

namespace client {

    namespace {

        constexpr int kFloors = 2; // rooms with their own cell grid (two floors)
        constexpr int kColumns = HouseGrid::kGridWidth + 1; // grid columns (1-based, index 0 unused)
        constexpr int kRows = HouseGrid::kGridDepth + 1; // grid rows (1-based, index 0 unused)
        constexpr int kLayers = HomeFurniture::kLevels + 1; // grid levels (1-based, index 0 unused)
        constexpr int kMostCell = 1000; // largest cell number accepted on any axis
        constexpr int kOutside = 255; // cell value reported for a cell outside the grid
        constexpr int kOnWall = 4; // furnPlaceFlags bit 4: the piece hangs on a wall
        constexpr int kSmallCarpet = 39052; // item 39052: Small Carpet
        constexpr int kLargeCarpet = 39083; // item 39083: Tiger-fur Carpet

    }

    HomeFurniture::HomeFurniture(
        const formats::ItemTable& items
    )
        : m_items(items),
          m_grids(static_cast<std::size_t>(kFloors * kColumns * kRows * kLayers), 0) {
        m_rooms[0] = HouseRoomRect::ofSize(3); // room size 3 for the first floor
        m_rooms[1] = HouseRoomRect::ofSize(1); // room size 1 for the second floor
    }

    void HomeFurniture::setRoomArea(const HouseRoomRect& area, int index) {
        if (index >= 0 && index < kRooms && area.top > 0 && area.bottom <= HouseGrid::kCells && area.left > 0 && area.right <= HouseGrid::kCells) {
            m_rooms[static_cast<std::size_t>(index)] = area;
        }
    }

    const HouseRoomRect& HomeFurniture::room(int index) const { return m_rooms[static_cast<std::size_t>(std::clamp(index, 0, kRooms - 1))]; }

    void HomeFurniture::setItemId(int index, int itemId) {
        if (index >= 1 && index <= kMost && m_pieces[static_cast<std::size_t>(index)].itemId != 0) {
            m_pieces[static_cast<std::size_t>(index)].itemId = itemId;
        }
    }

    void HomeFurniture::setUsedCount(int index, int value) {
        if (index >= 1 && index <= kMost && m_pieces[static_cast<std::size_t>(index)].itemId != 0) {
            m_pieces[static_cast<std::size_t>(index)].usedCount = value;
        }
    }

    const FurnitureRecord& HomeFurniture::piece(int index) const { return m_pieces[static_cast<std::size_t>(index >= 1 && index <= kMost ? index : 0)]; }

    int HomeFurniture::freeSlot() const {
        if (m_count < kMost) {
            for (int slot = 1; slot <= kMost; ++slot) {
                if (m_pieces[static_cast<std::size_t>(slot)].itemId == 0) {
                    return slot;
                }
            }
        }
        return 0;
    }

    std::optional<HomeFurniture::Span> HomeFurniture::spanOf(int itemId, int rotation) const {
        const auto item = m_items.find(itemId);
        if (!item) {
            return std::nullopt;
        }
        Span span;
        span.x = rotation == 1 ? item->furnSizeX : item->furnSizeY;
        span.y = rotation == 1 ? item->furnSizeY : item->furnSizeX;
        span.z = item->furnSizeZ;
        return span;
    }

    bool HomeFurniture::isCarpet(int itemId) const { return (itemId == kSmallCarpet || itemId == kLargeCarpet) && m_items.find(itemId).has_value(); }

    int HomeFurniture::gridIndex(int cellX, int cellY, int cellZ, int room) {
        const int x = cellX - HouseGrid::kGridColumn + 1;
        const int y = cellY - HouseGrid::kGridRow + 1;
        if (room < 0 || room >= kFloors || x < 1 || x > HouseGrid::kGridWidth || y < 1 || y > HouseGrid::kGridDepth || cellZ < 1 || cellZ > kLevels) {
            return -1;
        }
        return ((room * kColumns + x) * kRows + y) * kLayers + cellZ;
    }

    int HomeFurniture::houseCell(int cellX, int cellY, int cellZ, int room) const {
        const int index = gridIndex(cellX, cellY, cellZ, room);
        return index < 0 ? kOutside : m_grids[static_cast<std::size_t>(index)];
    }

    bool HomeFurniture::setHouseCell(
        int cellX,
        int cellY,
        int cellZ,
        int value,
        int room
    ) {
        const int index = gridIndex(cellX, cellY, cellZ, room);
        if (index < 0) {
            return false;
        }
        m_grids[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(value);
        return true;
    }

    bool HomeFurniture::markCells(const FurnitureRecord& record, int value, int room) {
        if (record.cellX < 0 || record.cellY < 0 || record.cellZ < 0 || record.cellX > kMostCell || record.cellY > kMostCell || record.cellZ > kMostCell) {
            return false;
        }
        const auto span = spanOf(record.itemId, record.rotation);
        if (!span) {
            return false;
        }
        for (int x = record.cellX; x <= record.cellX + span->x - 1; ++x) {
            for (int y = record.cellY; y <= record.cellY + span->y - 1; ++y) {
                for (int z = record.cellZ; z <= record.cellZ + span->z - 1; ++z) {
                    if (!setHouseCell(x, y, z, value, room)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool HomeFurniture::placementBlocked(
        int cellX,
        int cellY,
        int cellZ,
        const FurnitureRecord& record,
        int allowedCell
    ) const {
        const auto span = spanOf(record.itemId, record.rotation);
        if (!span) {
            return true;
        }
        const HouseRoomRect& area = room(record.room);
        for (int i = 1; i <= span->x; ++i) {
            for (int j = 1; j <= span->y; ++j) {
                for (int k = 1; k <= span->z; ++k) {
                    const int x = cellX + i - 1;
                    const int y = cellY + j - 1;
                    const int z = cellZ + k - 1;
                    if (!area.contains(x, y) || z < 1 || z > kLevels) {
                        return true;
                    }
                    const int held = houseCell(x, y, z, record.room);
                    if (held != 0 && held != allowedCell) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    int HomeFurniture::validatePlace(const FurnitureRecord& record, int slot, bool strict) const {
        constexpr int kInvalid = 255; // validation result 255: invalid record / slot / wall support
        if (record.cellX < 0 || record.cellY < 0 || record.cellZ < 0 || record.cellX > kMostCell || record.cellY > kMostCell || record.cellZ > kMostCell) {
            return kInvalid;
        }
        const auto item = m_items.find(record.itemId);
        if (!item) {
            return kInvalid;
        }
        if (slot == 0) {
            slot = freeSlot();
        }
        if (slot < 1 || slot > kMost || m_pieces[static_cast<std::size_t>(slot)].itemId != 0) {
            return kInvalid;
        }
        const bool placed = record.cellX != 0 || record.cellY != 0 || record.cellZ != 0;
        const HouseRoomRect& area = room(record.room);
        if (placed && !area.contains(record.cellX, record.cellY)) {
            return 1; // validation result 1: first cell outside the room
        }
        if (record.rotation == 1 && item->furnPicture1 == 0) {
            return 3; // validation result 3: the item has no second facing
        }
        const auto span = spanOf(record.itemId, record.rotation);
        // a tool on a Work Platform (parent set) has no furniture size, so its "last cell" is x - 1, y - 1: outside the room for a platform
        // against the left or back wall. It stands on the platform, so only its first cell is checked.
        if (placed && record.parent == 0 && !area.contains(span->x + record.cellX - 1, span->y + record.cellY - 1)) {
            return 1; // validation result 1: last cell outside the room
        }
        if (!isCarpet(record.itemId) && record.retained == 0 && placementBlocked(record.cellX, record.cellY, record.cellZ, record, 0)) {
            return 2; // validation result 2: cells blocked
        }
        if (strict && (item->furnPlaceFlags & kOnWall) != 0 && !leansOnPillars(record, *span)) {
            return kInvalid;
        }
        return 0;
    }

    bool HomeFurniture::leansOnPillars(const FurnitureRecord& record, const Span& span) const {
        const HouseRoomRect& area = room(record.room);
        const bool behind = record.rotation == 0;
        if (!(behind ? area.top < record.cellY : record.rotation == 1 && area.left < record.cellX)) {
            return true;
        }
        const int fromX = behind ? record.cellX : record.cellX - 1;
        const int fromY = behind ? record.cellY - 1 : record.cellY;
        for (int x = fromX; x <= fromX + span.x - 1; ++x) {
            for (int y = fromY; y <= fromY + span.y - 1; ++y) {
                for (int z = record.cellZ; z <= span.z + record.cellZ - 1; ++z) {
                    if (houseCell(x, y, z, record.room) != kPillarCell) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool HomeFurniture::place(int slot, const FurnitureRecord& record, bool strict) {
        if (slot == 0) {
            slot = freeSlot();
        }
        if (slot == 0 || validatePlace(record, slot, strict) != 0) {
            return false;
        }
        if (!isCarpet(record.itemId) && record.retained == 0 && !markCells(record, slot, record.room)) {
            return false;
        }
        ++m_count;
        m_pieces[static_cast<std::size_t>(slot)] = record;
        return true;
    }

    bool HomeFurniture::hasFurnitureOnTop(int slot) const {
        if (slot < 1 || slot > kMost || m_pieces[static_cast<std::size_t>(slot)].itemId == 0) {
            return true;
        }
        const FurnitureRecord& record = m_pieces[static_cast<std::size_t>(slot)];
        if (record.parent != 0 || record.retained != 0 || isCarpet(record.itemId)) {
            return false;
        }
        const auto span = spanOf(record.itemId, record.rotation);
        if (!span) {
            return true;
        }
        if (record.cellZ + span->z - 1 < kLevels) {
            const int z = record.cellZ + span->z;
            for (int x = record.cellX; x <= record.cellX + span->x - 1; ++x) {
                for (int y = record.cellY; y <= record.cellY + span->y - 1; ++y) {
                    const int above = houseCell(x, y, z, record.room);
                    if (above > 0 && above <= kMost && above != slot) {
                        const auto item = m_items.find(m_pieces[static_cast<std::size_t>(above)].itemId);
                        if (item && (item->furnPlaceFlags & kOnWall) == 0) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool HomeFurniture::remove(int slot) {
        if (slot < 1 || slot > kMost || hasFurnitureOnTop(slot)) {
            return false;
        }
        const FurnitureRecord record = m_pieces[static_cast<std::size_t>(slot)];
        if (!isCarpet(record.itemId) && record.retained == 0 && !markCells(record, 0, record.room)) {
            return false;
        }
        m_pieces[static_cast<std::size_t>(slot)] = {};
        --m_count;
        return true;
    }

    int HomeFurniture::link(int child, int parent) {
        if (child < 1 || child > kMost || parent < 1 || parent > kMost || piece(child).itemId == 0 || piece(parent).itemId == 0 || piece(parent).parent != 0) {
            return 0;
        }
        FurnitureRecord& up = m_pieces[static_cast<std::size_t>(parent)];
        for (int place = 1; place <= 10; ++place) { // a piece has 10 link places for pieces standing on it
            if (up.links[static_cast<std::size_t>(place)] == 0) {
                m_pieces[static_cast<std::size_t>(child)].parent = parent;
                up.links[static_cast<std::size_t>(place)] = child;
                return place;
            }
        }
        return 0;
    }

    bool HomeFurniture::moveAllowed(int slot, const FurnitureRecord& place) const {
        if (slot < 1 || slot > kMost || m_pieces[static_cast<std::size_t>(slot)].itemId == 0 || hasFurnitureOnTop(slot) || place.cellZ < 0 ||
            place.cellZ > kLevels) {
            return false;
        }
        const HouseRoomRect& area = room(place.room);
        const auto item = m_items.find(place.itemId);
        if (!area.contains(place.cellX, place.cellY) || !item || (place.rotation == 1 && item->furnPicture1 == 0)) {
            return false;
        }
        const auto span = spanOf(place.itemId, place.rotation);
        if (!area.contains(span->x + place.cellX - 1, span->y + place.cellY - 1)) {
            return false;
        }
        if (!isCarpet(place.itemId) && placementBlocked(place.cellX, place.cellY, place.cellZ, place, slot)) {
            return false;
        }
        if ((item->furnPlaceFlags & FurnitureKinds::kLaysOnPiece) != 0) {
            for (int x = place.cellX; place.cellZ > 1 && x <= span->x + place.cellX - 1; ++x) {
                for (int y = place.cellY; y <= span->y + place.cellY - 1; ++y) {
                    const int below = houseCell(x, y, place.cellZ - 1, place.room);
                    if (below < 1 || below > kMost) {
                        return false;
                    }
                    const auto holder = m_items.find(m_pieces[static_cast<std::size_t>(below)].itemId);
                    if (!holder || (holder->furnPlaceFlags & FurnitureKinds::kHoldsPieces) == 0) {
                        return false;
                    }
                }
            }
        } else if ((item->furnPlaceFlags & kOnWall) != 0) {
            if (FurnitureKinds::wallLevel(m_items, place.itemId) != place.cellZ || !leansOnPillars(place, *span)) {
                return false;
            }
        }
        return true;
    }

    void HomeFurniture::clear() {
        m_count = 0;
        m_pieces.fill({});
        std::fill(m_grids.begin(), m_grids.end(), static_cast<std::uint8_t>(0));
    }

}
