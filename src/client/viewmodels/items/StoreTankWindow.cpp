#include "client/viewmodels/items/StoreTankWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "client/data/VehicleKinds.h"
#include "protocol/ItemRequests.h"
#include "protocol/StoreTankRequests.h"

#include <algorithm>

namespace client {

    namespace {

        constexpr int kStoreTankItem = 34399; // item 34399: Fuel Bank (type 25)
        constexpr int kFuelSubType = 121; // item sub type 121: vehicle fuel
        constexpr int kVehicleType = 39; // item type 39: vehicles (Airship, Boat, Cabriolet, Hot-air Balloon, ...)
        constexpr int kMostPerPlace = 50; // most fuel items shown in one cell, the rest go to the next cell of the row
        constexpr int kMostPerRow = 250; // most fuel items one tank row holds (5 cells of 50)
        constexpr int kCellSide = 32; // cell size, px
        constexpr int kMessageMs = 1000; // how long a player message stays up, ms
        constexpr int kAllowedMark = 1; // bag use-mark 1 (compound window mark), passed to lockNotice as the asking window's mark
        const char* const kLocked = "Unable to operate when the item is locked";

        /// @brief Loads StoreTankWindow.xml into the screen.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("StoreTankWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("StoreTank");
        }

    }

    StoreTankWindow::StoreTankWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& inventory,
        const BagMarks& marks,
        PlayerMessages& messages,
        ItemInfoPopup& info,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_inventory(inventory),
          m_marks(marks),
          m_messages(messages),
          m_info(info),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_cells(screen.get<ui::IconGrid>("StoreTankCells")),
          m_sortLocks(services, screen, "StoreTankSortLock", "StoreTankCells", kColumns) {
        screen.on("onStoreTankClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onStoreTankSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            m_services.connection.send(protocol::sortRequest(protocol::SortGrid::StoreTank, m_sortLocks.places()));
            m_sorter.startOnServer(m_screen.context().nowMs);
        });
        screen.on("onStoreTankTake", [this](const ui::EventArgs& args) { take(args.index); });
    }

    void StoreTankWindow::open(int bagSlot) {
        m_bagSlot = bagSlot;
        if (m_bagSlot < 1 || m_bagSlot > Inventory::kBagSlots || m_inventory.bagItem(m_bagSlot).itemId != kStoreTankItem) {
            return;
        }
        refresh();
        if (!m_window.isVisible()) {
            m_window.setVisible(true);
        }
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void StoreTankWindow::close() {
        m_window.setVisible(false);
        m_bagSlot = 0;
        m_cells.clearCells();
    }

    int StoreTankWindow::placeCount(int row, int column) const {
        const protocol::ItemInstance& fuel = m_tank[static_cast<std::size_t>(row)];
        return fuel.itemId == 0 ? 0 : std::clamp(fuel.count - (column - 1) * kMostPerPlace, 0, kMostPerPlace);
    }

    void StoreTankWindow::refresh() {
        m_cells.clearCells();
        for (int row = 1; row <= kRows; ++row) {
            for (int column = 1; column <= kColumns; ++column) {
                const int count = placeCount(row, column);
                if (count == 0) {
                    continue;
                }
                protocol::ItemInstance shown;
                shown.itemId = m_tank[static_cast<std::size_t>(row)].itemId;
                shown.count = count;
                ui::IconCell cell = itemIconCell(m_services, shown, true);
                cell.hint.clear();
                m_cells.setCell((row - 1) * kColumns + column, cell);
            }
        }
    }

    void StoreTankWindow::use(int bagSlot) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || m_inventory.bagItem(bagSlot).itemId != kStoreTankItem) {
            return;
        }
        if (m_hooks.fighting()) {
            m_messages.show("Unable to use in combat", kMessageMs);
            return;
        }
        const int vehicleItem = m_hooks.vehicleItem();
        if (vehicleItem == 0) {
            m_messages.show("Unable to use without using a vehicle", kMessageMs);
            return;
        }
        if (const auto notice = m_marks.lockNotice(bagSlot, kAllowedMark)) {
            m_messages.show(notice->text, notice->ms);
            return;
        }
        if (!m_marks.isAvailable(bagSlot)) {
            m_messages.show(kLocked, kMessageMs);
            return;
        }
        const auto vehicle = m_services.items.find(vehicleItem);
        if (!vehicle || vehicle->type != kVehicleType || VehicleKinds::kindOfItem(vehicleItem) == 0) {
            return;
        }
        int found = 0;
        // a vehicle names its fuel in its first or second attribute
        for (int attribute = 0; attribute < 2 && found == 0; ++attribute) {
            const int fuel = vehicle->attributeValues[static_cast<std::size_t>(attribute)];
            const auto record = m_services.items.find(fuel);
            if (!record || record->subType != kFuelSubType) {
                continue;
            }
            for (int row = 1; row <= kRows; ++row) {
                if (m_tank[static_cast<std::size_t>(row)].itemId != 0 && m_tank[static_cast<std::size_t>(row)].itemId == fuel) {
                    found = row;
                    break;
                }
            }
        }
        if (found == 0) {
            m_messages.show("No corresponding usable fuel", kMessageMs);
            return;
        }
        m_hooks.send(protocol::useStoreTankRequest(bagSlot, found));
    }

    void StoreTankWindow::put(int bagSlot, int row) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || m_inventory.bagItem(bagSlot).empty()) {
            return;
        }
        const int itemId = m_inventory.bagItem(bagSlot).itemId;
        const auto record = m_services.items.find(itemId);
        if (!record || record->subType != kFuelSubType) {
            m_messages.show("The item can't be stored", kMessageMs);
            return;
        }
        if (const auto notice = m_marks.lockNotice(bagSlot, kAllowedMark)) {
            m_messages.show(notice->text, notice->ms);
            return;
        }
        if (!m_marks.isAvailable(bagSlot)) {
            m_messages.show(kLocked, kMessageMs);
            return;
        }
        if (row == 0) {
            for (int candidate = 1; candidate <= kRows && row == 0; ++candidate) {
                const protocol::ItemInstance& fuel = m_tank[static_cast<std::size_t>(candidate)];
                if (fuel.itemId == itemId && fuel.count < kMostPerRow) {
                    row = candidate;
                }
            }
            for (int candidate = 1; candidate <= kRows && row == 0; ++candidate) {
                if (m_tank[static_cast<std::size_t>(candidate)].itemId == 0) {
                    row = candidate;
                }
            }
        }
        if (row < 1 || row > kRows) {
            return;
        }
        const protocol::ItemInstance& there = m_tank[static_cast<std::size_t>(row)];
        if (there.itemId != 0 && there.itemId != itemId) {
            m_messages.show("Items from different types can't' be put in", kMessageMs);
            return;
        }
        m_hooks.send(protocol::putInStoreTankRequest(m_bagSlot, bagSlot, row));
    }

    void StoreTankWindow::take(int place) {
        const int row = (place - 1) / kColumns + 1;
        const int column = (place - 1) % kColumns + 1;
        if (m_bagSlot < 1 || m_bagSlot > Inventory::kBagSlots || place < 1 || row > kRows) {
            return;
        }
        const int count = placeCount(row, column);
        if (count == 0) {
            return;
        }
        if (m_inventory.freeSlotFor(m_tank[static_cast<std::size_t>(row)].itemId) == 0) {
            m_messages.show("There is not enough space in item column to recycle", kMessageMs);
            return;
        }
        m_hooks.send(protocol::takeFromStoreTankRequest(m_bagSlot, row, count));
    }

    void StoreTankWindow::received(int kind, int row, int itemId, int count) {
        // kind: 1 an item went in, 2 one came out
        if ((kind != 1 && kind != 2) || row < 1 || row > kRows) {
            return;
        }
        protocol::ItemInstance& fuel = m_tank[static_cast<std::size_t>(row)];
        const int before = fuel.count;
        fuel.itemId = itemId;
        fuel.count = count;
        if (m_window.isVisible()) {
            refresh();
        }
        // kind 2: fuel came out
        if (kind == 2 || (m_window.isVisible() && count > before)) {
            m_services.playSound("sound/wav0152.wav");
        }
    }

    void StoreTankWindow::update() {
        if (!m_window.isVisible()) {
            return;
        }
        if (m_bagSlot < 1 || m_bagSlot > Inventory::kBagSlots || m_inventory.bagItem(m_bagSlot).itemId != kStoreTankItem) {
            close();
            return;
        }
        if (m_screen.hoveredId() != m_cells.id()) {
            return;
        }
        const core::Point origin = m_window.bounds().position() + m_cells.bounds().position();
        const int place = m_cells.cellAt(m_screen.mouse() - origin);
        if (place == 0) {
            return;
        }
        const int row = (place - 1) / kColumns + 1;
        protocol::ItemInstance shown;
        shown.itemId = m_tank[static_cast<std::size_t>(row)].itemId;
        shown.count = placeCount(row, (place - 1) % kColumns + 1);
        const core::Point at = origin + m_cells.cellPosition(place);
        m_info.show(shown, {at.x, at.y, kCellSide, kCellSide});
    }

    void StoreTankWindow::updateSort(std::uint64_t nowMs) {
        const bool sortShown = m_services.videoSettings().sortButtons && m_services.serverSorts;
        m_screen.get<ui::Element>("StoreTankSort").setVisible(sortShown);
        m_sortLocks.update(sortShown, isOpen());
        if (!isOpen()) {
            m_sorter.stop();
            return;
        }
        if (const auto said = m_sorter.step({}, nowMs, nullptr)) {
            m_messages.show(*said);
        }
    }

}
