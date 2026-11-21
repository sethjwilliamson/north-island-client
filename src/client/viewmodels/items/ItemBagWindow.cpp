#include "client/viewmodels/items/ItemBagWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "gfx/TextRenderer.h"
#include "protocol/ItemRequests.h"

#include <cstdlib>
#include <utility>

namespace client {

    namespace {

        constexpr int kRows = 5; // rows of cells shown in this window
        constexpr int kColumns = Inventory::kColumns;
        constexpr int kCells = kRows * kColumns;
        constexpr int kMostStack = Inventory::kMaxStack;
        constexpr int kBagRows = Inventory::kRows;
        constexpr int kUnstorable = 39111; // item 39111: Sweet Door (type 30), cannot be put into the item bag
        constexpr int kMessageMs = 2000; // how long a player message stays up, ms
        constexpr std::uint16_t kTitleColor = 31; // 0x001F: pure blue (RGB565)

        /// @brief Loads ItemBagWindow.xml and returns its window.
        /// @param services Game services (layout, load context).
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("ItemBagWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("ItemBagWindow");
        }

    }

    ItemBagWindow::ItemBagWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& bag,
        const BagMarks& marks,
        PlayerMessages& messages,
        CountBox& countBox,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_bag(bag),
          m_marks(marks),
          m_messages(messages),
          m_countBox(countBox),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_cells(screen.get<ui::IconGrid>("ItemBagCells")),
          m_titleSpot(screen.get<ui::Spot>("ItemBagTitle")),
          m_grid(services.items, kRows),
          m_restrain(services.pictures.find("Panel_RestrainBank_1").value_or(gfx::PictureId{})),
          m_sortLocks(services, screen, "ItemBagSortLock", "ItemBagCells") {
        screen.get<ui::DrawingArea>("ItemBagPage").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintPage(canvas, at); });
        screen.on("onItemBagClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onItemBagSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            if (m_services.serverSorts) {
                m_services.connection.send(protocol::sortRequest(protocol::SortGrid::ItemBag, m_sortLocks.places()));
                m_sorter.startOnServer(m_screen.context().nowMs);
            } else {
                m_sorter.start();
            }
        });
        screen.on("onItemBagDrop", [this](const ui::EventArgs& args) { gridItemDropped(args); });
    }

    void ItemBagWindow::open() {
        refresh();
        m_window.setVisible(true);
        if (m_screen.context().bringAbovePopups) {
            m_screen.context().bringAbovePopups(m_window.id());
        }
    }

    void ItemBagWindow::close() {
        m_window.setVisible(false);
        m_hooks.closed();
    }

    void ItemBagWindow::setList(const std::vector<protocol::BagItemRecord>& records) {
        for (int cell = 1; cell <= kCells; ++cell) {
            m_grid.takeFromBag(cell, kMostStack);
        }
        for (const protocol::BagItemRecord& record : records) {
            if (record.slot >= 1 && record.slot <= kCells) {
                m_grid.addToBag(record.slot, record.item);
            }
        }
        refresh();
    }

    void ItemBagWindow::replace(int cell, const protocol::ItemInstance& item) {
        if (cell < 1 || cell > kCells) {
            return;
        }
        m_grid.takeFromBag(cell, kMostStack);
        if (!item.empty()) {
            m_grid.addToBag(cell, item);
        }
        refresh();
    }

    void ItemBagWindow::moved(int kind, int first, int second, int count) {
        if (kind == 3) { // move kind 3: bag to grid
            protocol::ItemInstance item = m_bag.bagItem(first);
            if (item.empty()) {
                return;
            }
            item.count = count;
            m_hooks.takeFromBag(first, count);
            m_grid.addToBag(second, item);
        } else if (first >= 1 && first <= kCells && !m_grid.bagItem(first).empty()) {
            protocol::ItemInstance item = m_grid.bagItem(first);
            item.count = count;
            m_grid.takeFromBag(first, count);
            if (kind == 1) { // move kind 1: grid to bag
                m_hooks.addToBag(second, item);
            } else {
                m_grid.addToBag(second, item);
            }
        }
        refresh();
    }

    void ItemBagWindow::refresh() {
        m_cells.clearCells();
        for (int cell = 1; cell <= kCells; ++cell) {
            const protocol::ItemInstance& item = m_grid.bagItem(cell);
            if (!item.empty()) {
                m_cells.setCell(cell, itemIconCell(m_services, item, true));
            }
        }
    }

    void ItemBagWindow::paintPage(gfx::Canvas& canvas, core::Point at) {
        const std::string title = "Item Bag";
        const int width = static_cast<int>(title.size()) * 8; // width of one character in pixels
        // text box height 16 pixels
        canvas.drawText(
            {at.x + (m_titleSpot.rect().width - width) / 2, at.y + m_titleSpot.at().y},
            title,
            {core::Color::fromRgb565(kTitleColor), gfx::TextEffect::Plain, std::nullopt},
            width,
            16
        );
        if (!m_spaceContract && m_restrain.isValid()) {
            canvas.drawPicture(m_restrain, {at.x + m_cells.bounds().x, at.y + m_cells.bounds().y});
        }
    }

    int ItemBagWindow::neighbour(int slot, int rows, int direction) {
        // directions 1..9 form a 3 x 3 block around the place
        const int row = (slot - 1) / kColumns + (direction - 1) / 3 - 1;
        const int column = (slot - 1) % kColumns + (direction - 1) % 3 - 1;
        if (row < 0 || row >= rows || column < 0 || column >= kColumns) {
            return 0;
        }
        return row * kColumns + column + 1;
    }

    void ItemBagWindow::requestToGrid(int bagSlot, int cell, int count, bool exact) {
        const protocol::ItemInstance& item = m_bag.bagItem(bagSlot);
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || item.empty() || cell < 1 || cell > kCells) {
            return;
        }
        if (m_grid.fitCount(cell, item.itemId, count, 0) != count) {
            if (!exact) {
                return;
            }
            bool found = false;
            for (int direction = 1; direction <= 9 && !found; ++direction) { // the 9 neighbour directions
                const int near = neighbour(cell, kRows, direction);
                if (near != 0 && near != cell && m_grid.fitCount(near, item.itemId, count, 0) == count) {
                    cell = near;
                    found = true;
                }
            }
            if (!found) {
                return;
            }
        }
        if (item.count < count || m_grid.bagItem(cell).count + count > kMostStack) {
            return;
        }
        if (m_grid.fitCount(cell, item.itemId, count, 0) == count) {
            m_services.connection.send(protocol::itemBagMoveRequest(protocol::ItemBagMove::ToGrid, bagSlot, cell, count));
        }
    }

    void ItemBagWindow::requestMove(int from, int to, int count, bool exact) {
        const protocol::ItemInstance& item = m_grid.bagItem(from);
        if (from < 1 || from > kCells || item.empty() || to < 1 || to > kCells) {
            return;
        }
        if (m_grid.fitCount(to, item.itemId, count, from) != count) {
            if (!exact) {
                return;
            }
            bool found = false;
            for (int direction = 1; direction <= 9 && !found; ++direction) { // the 9 neighbour directions
                const int near = neighbour(to, kRows, direction);
                if (near != 0 && near != to && m_grid.fitCount(near, item.itemId, count, from) == count) {
                    to = near;
                    found = true;
                }
            }
            if (!found) {
                return;
            }
        }
        if (item.count >= count) {
            m_services.connection.send(protocol::itemBagMoveRequest(protocol::ItemBagMove::Within, from, to, count));
        }
    }

    void ItemBagWindow::requestToBag(int cell, int bagSlot, int count, bool exact) {
        const protocol::ItemInstance& item = m_grid.bagItem(cell);
        if (cell < 1 || cell > kCells || item.empty() || bagSlot < 1 || bagSlot > Inventory::kBagSlots) {
            return;
        }
        if (m_bag.fitCount(bagSlot, item.itemId, count, 0) != count) {
            if (!exact) {
                return;
            }
            bool found = false;
            for (int direction = 1; direction <= 9 && !found; ++direction) { // the 9 neighbour directions
                const int near = neighbour(bagSlot, kBagRows, direction);
                if (near != 0 && near != bagSlot && m_bag.fitCount(near, item.itemId, count, 0) == count) {
                    bagSlot = near;
                    found = true;
                }
            }
            if (!found) {
                return;
            }
        }
        if (item.count < count || m_bag.bagItem(bagSlot).count + count > kMostStack) {
            return;
        }
        if (m_bag.fitCount(bagSlot, item.itemId, count, 0) == count) {
            m_services.connection.send(protocol::itemBagMoveRequest(protocol::ItemBagMove::ToBag, cell, bagSlot, count));
        }
    }

    void ItemBagWindow::gridItemDropped(const ui::EventArgs& args) {
        const int source = args.index;
        if (source < 1 || source > kCells || m_grid.bagItem(source).empty()) {
            return;
        }
        const int itemId = m_grid.bagItem(source).itemId;
        const int target = args.targetIndex;
        const int grabCells = args.grab.y / m_cells.pitch().y * kColumns + args.grab.x / m_cells.pitch().x;
        if (args.target == "ItemBag") {
            if (target < 1 || target > Inventory::kBagSlots) {
                return;
            }
            const int anchor = m_bag.occupantOf(target);
            if (anchor != 0 && m_bag.bagItem(anchor).itemId != itemId) {
                return;
            }
            if (args.ctrl) {
                const int left = m_bag.bagItem(target).count;
                const int count = m_grid.bagItem(source).count + left > kMostStack ? kMostStack - left : m_grid.bagItem(source).count;
                m_countBox.ask(count, [this, source, target](int asked) {
                    if (asked > 0) {
                        requestToBag(source, target, asked, false);
                    }
                    return true;
                });
            } else if (target >= grabCells) {
                requestToBag(source, target - grabCells, 1, true);
            }
        } else if (args.target == "ItemBagCells") {
            if (target < 1 || target > kCells) {
                return;
            }
            if (!m_spaceContract) {
                m_messages.show("It can be used after using Space Contract", kMessageMs);
                return;
            }
            if (!m_grid.bagItem(target).empty() && m_grid.bagItem(target).itemId != itemId) {
                return;
            }
            if (args.ctrl) {
                const int left = m_grid.bagItem(target).count;
                const int count = left + m_grid.bagItem(source).count > kMostStack ? kMostStack - left : m_grid.bagItem(source).count;
                m_countBox.ask(count, [this, source, target](int asked) {
                    if (asked > 0) {
                        requestMove(source, target, asked, false);
                    }
                    return true;
                });
                return;
            }
            int destination = target;
            if (source == destination) {
                const core::Point origin = m_window.bounds().position() + m_cells.bounds().position() + m_cells.cellPosition(source);
                const int mouseColumn = std::abs(origin.x - args.position.x) / m_cells.pitch().x;
                const int mouseRow = std::abs(origin.y - args.position.y) / m_cells.pitch().y;
                destination = source + mouseRow * kColumns + mouseColumn;
            }
            if (destination < grabCells) {
                return;
            }
            destination -= grabCells;
            if (source == destination || m_grid.fitCount(destination, itemId, 1, source) != 1) {
                return;
            }
            requestMove(source, destination, 1, true);
        }
    }

    void ItemBagWindow::bagItemDropped(const ui::EventArgs& args) {
        const int source = args.index;
        const int target = args.targetIndex;
        if (source < 1 || source > Inventory::kBagSlots || m_bag.bagItem(source).empty() || target < 1 || target > kCells) {
            return;
        }
        if (m_marks.isPriced(source) || m_marks.isInFurnitureStall(source) || m_marks.useMark(source) != 0 || m_marks.busyKind(source) != 0 ||
            m_marks.vehicleState(source) != 0) {
            return;
        }
        const int itemId = m_bag.bagItem(source).itemId;
        if (!m_grid.bagItem(target).empty() && m_grid.bagItem(target).itemId != itemId) {
            return;
        }
        if (!m_spaceContract) {
            m_messages.show("It can be used after using Space Contract", kMessageMs);
            return;
        }
        if (itemId == kUnstorable) {
            m_messages.show("Can't be put in warehouse ", kMessageMs);
            return;
        }
        const int grabCells = args.grab.y / m_cells.pitch().y * kColumns + args.grab.x / m_cells.pitch().x;
        if (target <= grabCells) {
            return;
        }
        const int destination = target - grabCells;
        if (args.ctrl) {
            const int left = m_grid.bagItem(destination).count;
            const int count = left + m_bag.bagItem(source).count > kMostStack ? kMostStack - left : m_bag.bagItem(source).count;
            m_countBox.ask(count, [this, source, destination](int asked) {
                if (asked > 0) {
                    requestToGrid(source, destination, asked, false);
                }
                return true;
            });
            return;
        }
        requestToGrid(source, destination, 1, true);
    }

    void ItemBagWindow::updateSort(std::uint64_t nowMs) {
        const bool sortShown = m_services.videoSettings().sortButtons;
        m_screen.get<ui::Element>("ItemBagSort").setVisible(sortShown);
        m_sortLocks.update(sortShown, isOpen());
        if (!isOpen()) {
            m_sorter.stop();
            return;
        }
        if (!m_sorter.isRunning()) {
            return;
        }
        if (const auto said = m_sorter.step(sortCellsOf(m_grid, m_services.items, m_sortLocks.places()), nowMs, [this](const SortMove& move) {
                m_services.connection.send(protocol::itemBagMoveRequest(protocol::ItemBagMove::Within, move.from, move.to, move.count));
            })) {
            m_messages.show(*said);
        }
    }

}
