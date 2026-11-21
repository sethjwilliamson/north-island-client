#include "client/viewmodels/bank/BankItemWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "protocol/BankRequests.h"
#include "protocol/ItemRequests.h"

#include <cstdlib>
#include <utility>
#include <vector>

namespace client {

    namespace {

        constexpr int kNoBankFlag = 32; // item record flag bit value 32: the item cannot be stored in the bank
        constexpr int kUnbankable = 39111; // item 39111: Sweet Door, never accepted by the bank

        /// @brief Loads BankItemWindow.xml and returns its root window.
        /// @param services Game services (layout and load context).
        /// @param screen Screen to load into.
        /// @return The "BankItem" window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("BankItemWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("BankItem");
        }

    }

    BankItemWindow::BankItemWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& bag,
        const Inventory& bank,
        BagMarks& marks,
        PlayerMessages& messages,
        std::function<void()> bagChanged,
        BagRequests& requests
    )
        : m_services(services),
          m_screen(screen),
          m_bag(bag),
          m_bank(bank),
          m_marks(marks),
          m_messages(messages),
          m_bagChanged(std::move(bagChanged)),
          m_requests(requests),
          m_window(loadWindow(services, screen)),
          m_bankGrid(screen.get<ui::IconGrid>("BankItemBank")),
          m_bagGrid(screen.get<ui::IconGrid>("BankItemBag")),
          m_sortLocks(services, screen, "BankItemSortLock", "BankItemBank") {
        screen.on("onBankItemPress", [this](const ui::EventArgs& args) { m_dragSlot = args.index; });
        screen.on("onBankItemBagClick", [this](const ui::EventArgs& args) { toggleBag(args.index); });
        screen.on("onBankItemBankClick", [this](const ui::EventArgs& args) { toggleBank(args.index); });
        screen.on("onBankItemBagUnmark", [this](const ui::EventArgs& args) { unmarkBag(args.index); });
        screen.on("onBankItemBankUnmark", [this](const ui::EventArgs& args) { unmarkBank(args.index); });
        screen.on("onBankItemBankDrop", [this](const ui::EventArgs& args) { bankItemDropped(args); });
        screen.on("onBankItemBagDrop", [this](const ui::EventArgs& args) { bagItemDropped(args); });
        screen.on("onBankItemBagQuickMove", [this](const ui::EventArgs& args) { quickStore(args.index); });
        screen.on("onBankItemBankQuickMove", [this](const ui::EventArgs& args) { quickWithdraw(args.index); });
        screen.on("onBankItemStore", [this](const ui::EventArgs&) { store(); });
        screen.on("onBankItemWithdraw", [this](const ui::EventArgs&) { withdraw(); });
        screen.on("onBankItemClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onBankItemSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            if (m_services.serverSorts) {
                m_services.connection.send(protocol::sortRequest(protocol::SortGrid::Bank, m_sortLocks.places()));
                m_sorter.startOnServer(m_screen.context().nowMs);
            } else {
                m_sorter.start();
            }
        });
    }

    void BankItemWindow::show(std::function<void()> onClosed) {
        m_onClosed = std::move(onClosed);
        m_quickStores = 0;
        m_quickWithdraws = 0;
        m_bankMarked.fill(false);
        clearBagMarks();
        refresh();
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void BankItemWindow::close() {
        m_window.setVisible(false);
        clearBagMarks();
        const std::function<void()> handler = std::move(m_onClosed);
        m_onClosed = nullptr;
        if (handler) {
            handler();
        }
    }

    void BankItemWindow::clearBagMarks() {
        m_marks.clearUseMarks(BagMarks::kBank);
        m_bagChanged();
    }

    void BankItemWindow::refresh() {
        m_bagGrid.clearCells();
        m_bankGrid.clearCells();
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            const protocol::ItemInstance& item = m_bag.bagItem(slot);
            if (!item.empty() && !m_marks.isHidden(slot)) {
                m_bagGrid.setCell(slot, itemIconCell(m_services, item, true, m_marks.isBusy(slot)));
            }
            const protocol::ItemInstance& stored = m_bank.bagItem(slot);
            if (!stored.empty()) {
                m_bankGrid.setCell(slot, itemIconCell(m_services, stored, true, m_bankMarked[static_cast<std::size_t>(slot)]));
            }
        }
    }

    void BankItemWindow::toggleBag(int slot) {
        if (m_dragSlot != slot || m_dropHandled) {
            m_dragSlot = 0;
            return;
        }
        if (slot < 1 || slot > Inventory::kBagSlots || m_bag.bagItem(slot).empty()) {
            return;
        }
        const int itemId = m_bag.bagItem(slot).itemId;
        const auto record = m_services.items.find(itemId);
        if ((record && (record->flags & kNoBankFlag) != 0) || itemId == kUnbankable) {
            m_messages.show("This item cannot be stored", 2000); // message display time in ms
            return;
        }
        if (m_marks.vehicleState(slot) != 0) {
            m_messages.show("Put item away before preserving.", 2000); // message display time in ms
            return;
        }
        if (const auto notice = m_marks.lockNotice(slot, BagMarks::kBank)) {
            m_messages.show(notice->text, notice->ms);
            return;
        }
        if (m_marks.isPriced(slot)) {
            m_messages.show("Unable to operate when the item is locked", 2000); // message display time in ms
            return;
        }
        const int mark = m_marks.useMark(slot);
        if (mark == 0) {
            m_marks.setUseMark(slot, BagMarks::kBank);
        } else if (mark == BagMarks::kBank) {
            m_marks.setUseMark(slot, 0);
        }
        m_bagChanged();
    }

    void BankItemWindow::toggleBank(int slot) {
        if (m_dragSlot != slot || m_dropHandled) {
            m_dragSlot = 0;
            return;
        }
        if (slot < 1 || slot > Inventory::kBagSlots || m_bank.bagItem(slot).empty()) {
            return;
        }
        bool& marked = m_bankMarked[static_cast<std::size_t>(slot)];
        marked = !marked;
        refresh();
    }

    void BankItemWindow::unmarkBag(int slot) {
        if (slot >= 1 && slot <= Inventory::kBagSlots && m_marks.useMark(slot) == BagMarks::kBank) {
            m_marks.setUseMark(slot, 0);
            m_bagChanged();
        }
    }

    void BankItemWindow::unmarkBank(int slot) {
        if (slot >= 1 && slot <= Inventory::kBagSlots) {
            m_bankMarked[static_cast<std::size_t>(slot)] = false;
            refresh();
        }
    }

    void BankItemWindow::store() {
        std::vector<int> slots;
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            if (m_marks.useMark(slot) == BagMarks::kBank) {
                slots.push_back(slot);
            }
        }
        if (slots.empty()) {
            m_messages.show("Please choose items you want to withdraw in the right side.", 2000); // message display time in ms
            return;
        }
        m_storePending = true;
        m_services.connection.send(protocol::bankStoreRequest(slots));
    }

    void BankItemWindow::withdraw() {
        if (m_withdrawPending || m_storePending) {
            return;
        }
        std::vector<int> slots;
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            if (m_bankMarked[static_cast<std::size_t>(slot)]) {
                slots.push_back(slot);
            }
        }
        if (slots.empty()) {
            m_messages.show("Please choose items you want to withdraw in the left side.", 2000); // message display time in ms
            return;
        }
        m_withdrawPending = true;
        m_services.connection.send(protocol::bankWithdrawRequest(slots));
    }

    void BankItemWindow::bankItemDropped(const ui::EventArgs& args) {
        const core::Point pitch = m_bankGrid.pitch();
        m_dropHandled = false;
        const int source = args.index;
        if (source < 1 || source > Inventory::kBagSlots || m_bank.bagItem(source).empty() || m_bankMarked[static_cast<std::size_t>(source)]) {
            return;
        }
        const int target = args.targetIndex;
        if (target < 1 || target > Inventory::kBagSlots) {
            return;
        }
        const int grabColumn = args.grab.x / pitch.x;
        const int grabRow = args.grab.y / pitch.y;
        if (args.target == "BankItemBank") {
            if (m_bankMarked[static_cast<std::size_t>(target)]) {
                return;
            }
            const bool grabbed = m_bank.bagItem(target).empty() || target == source;
            const int grabCells = grabbed ? grabRow * Inventory::kColumns + grabColumn : 0;
            int destination = target;
            if (source == destination) {
                const core::Point origin = m_window.bounds().position() + m_bankGrid.bounds().position() + m_bankGrid.cellPosition(source);
                const int mouseColumn = std::abs(origin.x - args.position.x) / pitch.x;
                const int mouseRow = std::abs(origin.y - args.position.y) / pitch.y;
                destination = source + mouseRow * Inventory::kColumns + mouseColumn;
            }
            if (destination < grabCells) {
                return;
            }
            destination -= grabCells;
            if (source == destination) {
                return;
            }
            moveBankItem(source, destination, target);
        } else if (args.target == "BankItemBag" || args.target == "ItemBag") {
            if (m_marks.useMark(target) != 0 || !m_marks.isAvailable(target)) {
                return;
            }
            const int grabCells = grabRow * Inventory::kColumns + grabColumn;
            if (target > grabCells) {
                withdrawBankItemTo(source, target - grabCells);
            }
        }
    }

    void BankItemWindow::bagItemDropped(const ui::EventArgs& args) {
        const core::Point pitch = m_bagGrid.pitch();
        m_dropHandled = false;
        if (args.target == "BankItemBag" || args.target == "ItemBag" || args.target.empty() || args.target == "WorldView") {
            if (args.source == "BankItemBag") {
                moveBagItem(args);
            }
            return;
        }
        const int source = args.index;
        const int target = args.targetIndex;
        if (args.target != "BankItemBank" || source < 1 || source > Inventory::kBagSlots || m_bag.bagItem(source).empty() || target < 1 ||
            target > Inventory::kBagSlots) {
            return;
        }
        if (m_marks.isPriced(source) || m_marks.useMark(source) != 0 || m_marks.busyKind(source) != 0 || m_bankMarked[static_cast<std::size_t>(target)]) {
            return;
        }
        const int grabCells = args.grab.y / pitch.y * Inventory::kColumns + args.grab.x / pitch.x;
        if (target > grabCells) {
            dropBagItemToBank(source, target - grabCells);
        }
    }

    void BankItemWindow::moveBagItem(const ui::EventArgs& args) {
        const core::Point pitch = m_bagGrid.pitch();
        const int source = args.index;
        if (source < 1 || source > Inventory::kBagSlots || m_bag.bagItem(source).empty()) {
            return;
        }
        if (args.target.empty() || args.target == "WorldView") {
            m_requests.dropOnGround(source, args.ctrl);
            return;
        }
        const int target = args.targetIndex;
        if (target < 1 || target > Inventory::kBagSlots) {
            return;
        }
        if (m_marks.isPriced(source) || m_marks.isPriced(target) || m_marks.useMark(source) != 0 || m_marks.useMark(target) != 0 ||
            m_marks.busyKind(source) != 0 || m_marks.busyKind(target) != 0) {
            return;
        }
        int grabColumn = args.grab.x / pitch.x;
        int grabRow = args.grab.y / pitch.y;
        const protocol::ItemInstance& there = m_bag.bagItem(target);
        if (!there.empty() && there.itemId != m_bag.bagItem(source).itemId) {
            grabColumn = 0;
            grabRow = 0;
        }
        int destination = target;
        if (source == destination) {
            const core::Point origin = m_window.bounds().position() + m_bagGrid.bounds().position() + m_bagGrid.cellPosition(1);
            const int mouseColumn = (args.position.x - origin.x) / pitch.x + 1;
            const int mouseRow = (args.position.y - origin.y) / pitch.y + 1;
            if (mouseColumn - grabColumn < 1 || mouseColumn - grabColumn > Inventory::kColumns || mouseRow - grabRow < 1 ||
                mouseRow - grabRow > Inventory::kBagSlots / Inventory::kColumns) {
                return;
            }
            destination = (mouseRow - 1) * Inventory::kColumns + mouseColumn;
        }
        const int grabCells = grabRow * Inventory::kColumns + grabColumn;
        if (destination < grabCells) {
            return;
        }
        destination -= grabCells;
        if (source != destination) {
            requestMoveBagItem(source, 1, destination, true);
        }
    }

    void BankItemWindow::requestMoveBagItem(int fromSlot, int count, int toSlot, bool searchNear) {
        if (!m_marks.isAvailable(fromSlot)) {
            m_messages.show("Unable to operate when the item is locked", 2000); // message display time in ms
            return;
        }
        const int itemId = m_bag.bagItem(fromSlot).itemId;
        if (m_bag.fitCount(toSlot, itemId, count, fromSlot) != count) {
            if (!searchNear) {
                return;
            }
            const int row = (toSlot - 1) / Inventory::kColumns;
            const int column = (toSlot - 1) % Inventory::kColumns;
            bool found = false;
            // 9 neighbour places in a 3x3 block around the target
            for (int direction = 1; direction <= 9 && !found; ++direction) {
                const int neighbourRow = row + (direction - 1) / 3 - 1;
                const int neighbourColumn = column + (direction - 1) % 3 - 1;
                if (neighbourRow < 0 || neighbourRow >= Inventory::kBagSlots / Inventory::kColumns || neighbourColumn < 0 ||
                    neighbourColumn >= Inventory::kColumns) {
                    continue;
                }
                const int alternative = neighbourRow * Inventory::kColumns + neighbourColumn + 1;
                if (alternative != toSlot && m_bag.fitCount(alternative, itemId, count, fromSlot) == count) {
                    toSlot = alternative;
                    found = true;
                }
            }
            if (!found) {
                return;
            }
        }
        m_dropHandled = true;
        m_services.connection.send(protocol::moveItemRequest(fromSlot, count, toSlot));
    }

    void BankItemWindow::moveBankItem(int fromSlot, int toSlot, int dropTarget) {
        if (fromSlot < 1 || fromSlot > Inventory::kBagSlots || toSlot < 1 || toSlot > Inventory::kBagSlots) {
            return;
        }
        const protocol::ItemInstance& item = m_bank.bagItem(fromSlot);
        if (item.empty()) {
            return;
        }
        const protocol::ItemInstance& there = m_bank.bagItem(toSlot);
        if (there.itemId == item.itemId && m_bank.isStackable(item.itemId)) {
            if (Inventory::kMaxStack - there.count == 0) {
                return;
            }
        } else if (!canSwapBankItems(fromSlot, toSlot, dropTarget)) {
            return;
        }
        m_dropHandled = true;
        m_services.connection.send(protocol::bankMoveRequest(fromSlot, toSlot));
    }

    bool BankItemWindow::canSwapBankItems(int& fromSlot, int& toSlot, int dropTarget) const {
        const int fromAnchor = m_bank.occupantOf(fromSlot);
        if (m_bank.bagItem(fromSlot).empty()) {
            return false;
        }
        int toAnchor = m_bank.occupantOf(toSlot);
        if (toAnchor != 0 && toAnchor != fromAnchor && m_bank.occupantOf(dropTarget) == 0) {
            return false;
        }
        if (fromAnchor != 0 && fromAnchor != fromSlot) {
            fromSlot = fromAnchor;
        }
        const int sourceAnchor = m_bank.occupantOf(fromSlot);
        if (toAnchor != 0 && toAnchor != sourceAnchor && toAnchor != toSlot) {
            toSlot = toAnchor;
        }
        toAnchor = m_bank.occupantOf(toSlot);
        const int sourceItem = sourceAnchor != 0 ? m_bank.bagItem(sourceAnchor).itemId : 0;
        const int targetItem = toAnchor != 0 ? m_bank.bagItem(toAnchor).itemId : 0;
        const bool swapping = toAnchor != 0 && sourceAnchor != toAnchor;
        if (swapping) {
            const core::Size sourceSize = m_bank.cellsOf(sourceItem);
            const core::Size targetSize = m_bank.cellsOf(targetItem);
            if (sourceSize.width != 1 || sourceSize.height != 1 || targetSize.width != 1 || targetSize.height != 1) {
                return false;
            }
        }
        if (sourceItem == 0 || !m_bank.fitsFree(toSlot, sourceItem, toAnchor, sourceAnchor)) {
            return false;
        }
        return !swapping || m_bank.fitsFree(fromSlot, targetItem, sourceAnchor, toAnchor);
    }

    void BankItemWindow::dropBagItemToBank(int bagSlot, int bankSlot) {
        const protocol::ItemInstance& item = m_bag.bagItem(bagSlot);
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || bankSlot < 1 || bankSlot > Inventory::kBagSlots || item.empty()) {
            return;
        }
        const int anchor = m_bank.occupantOf(bankSlot);
        const protocol::ItemInstance& there = anchor != 0 ? m_bank.bagItem(anchor) : protocol::ItemInstance{};
        if (!there.empty() && there.itemId != item.itemId) {
            return;
        }
        if (there.itemId == item.itemId && m_bank.isStackable(item.itemId)) {
            if (Inventory::kMaxStack - there.count == 0) {
                return;
            }
        } else if (!there.empty() || !m_bank.fitsFree(bankSlot, item.itemId, 0, 0)) {
            return;
        }
        m_services.connection.send(protocol::bankDropRequest(bankSlot, bagSlot));
    }

    void BankItemWindow::withdrawBankItemTo(int bankSlot, int bagSlot) {
        if (bankSlot < 1 || bankSlot > Inventory::kBagSlots || bagSlot < 1 || bagSlot > Inventory::kBagSlots) {
            return;
        }
        const protocol::ItemInstance& item = m_bank.bagItem(bankSlot);
        if (item.empty()) {
            return;
        }
        const int anchor = m_bag.occupantOf(bagSlot);
        const protocol::ItemInstance& there = anchor != 0 ? m_bag.bagItem(anchor) : protocol::ItemInstance{};
        if (!there.empty() && there.itemId != item.itemId) {
            return;
        }
        if (there.itemId == item.itemId && m_bag.isStackable(item.itemId)) {
            if (Inventory::kMaxStack - there.count == 0) {
                return;
            }
        } else if (!there.empty() || !m_bag.fitsFree(bagSlot, item.itemId, 0, 0)) {
            return;
        }
        m_services.connection.send(protocol::bankTakeRequest(bagSlot, bankSlot));
    }

    void BankItemWindow::quickStore(int bagSlot) {
        if (!m_services.videoSettings().modernBag || m_storePending || m_withdrawPending || bagSlot < 1 || bagSlot > Inventory::kBagSlots ||
            m_bag.bagItem(bagSlot).empty()) {
            return;
        }
        if (m_marks.isPriced(bagSlot) || m_marks.useMark(bagSlot) != 0 || m_marks.busyKind(bagSlot) != 0 || m_marks.vehicleState(bagSlot) != 0) {
            return;
        }
        ++m_quickStores;
        m_services.connection.send(protocol::bankStoreRequest({bagSlot}));
    }

    void BankItemWindow::quickWithdraw(int bankSlot) {
        if (!m_services.videoSettings().modernBag || m_storePending || m_withdrawPending || bankSlot < 1 || bankSlot > Inventory::kBagSlots ||
            m_bank.bagItem(bankSlot).empty() || m_bankMarked[static_cast<std::size_t>(bankSlot)]) {
            return;
        }
        ++m_quickWithdraws;
        m_services.connection.send(protocol::bankWithdrawRequest({bankSlot}));
    }

    void BankItemWindow::done(bool deposit) {
        int& quick = deposit ? m_quickStores : m_quickWithdraws;
        if (quick > 0 && !(deposit ? m_storePending : m_withdrawPending)) {
            // the end of a Shift click move: the items marked for the arrow buttons stay marked
            --quick;
            if (!deposit) {
                refresh();
            }
            return;
        }
        if (deposit) {
            m_storePending = false;
            clearBagMarks();
        } else {
            m_withdrawPending = false;
            m_bankMarked.fill(false);
            refresh();
        }
    }

    void BankItemWindow::updateSort(std::uint64_t nowMs) {
        const bool sortShown = m_services.videoSettings().sortButtons;
        m_screen.get<ui::Element>("BankItemSort").setVisible(sortShown);
        m_sortLocks.update(sortShown, isOpen());
        if (!isOpen()) {
            m_sorter.stop();
            return;
        }
        if (!m_sorter.isRunning()) {
            return;
        }
        if (const auto said = m_sorter.step(sortCellsOf(m_bank, m_services.items, m_sortLocks.places()), nowMs, [this](const SortMove& move) {
                m_services.connection.send(protocol::bankMoveRequest(move.from, move.to));
            })) {
            m_messages.show(*said);
        }
    }

}
