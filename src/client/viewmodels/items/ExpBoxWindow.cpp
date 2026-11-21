#include "client/viewmodels/items/ExpBoxWindow.h"

#include <vector>

#include "client/rendering/painters/ItemCells.h"
#include "core/ByteReader.h"
#include "protocol/BoxRequests.h"
#include "protocol/ItemRequests.h"

#include <algorithm>
#include <string>
#include <utility>

namespace client {

    namespace {

        constexpr int kScreenWidth = 800; // game window width, px
        constexpr int kMostInCell = 50; // most items stacked in one cell
        constexpr int kMessageMs = 1000; // message duration, ms
        constexpr int kExpSlot = 8; // equip slot value 8 on EXP potions / capsules
        constexpr int kExpSubType = 127; // item sub type 127: EXP potions / capsules
        constexpr int kProbeItem = 10001; // item 10001: a plain one-cell item ([Big5] "question mark 2 (unopened)", type 1), used to ask for a free bag slot

        /// @brief Loads ExpBoxWindow.xml and returns its window.
        /// @param services Game services (layout, load context).
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("ExpBoxWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("ExpBox");
        }

        /// @brief Tells whether an item may be kept in the EXP Bag.
        /// @param record Item record.
        /// @return True for equipment slot 8, sub type 127.
        bool isExpItem(const formats::ItemRecord& record) { return record.equipSlot == kExpSlot && record.subType == kExpSubType; }

    }

    ExpBoxWindow::ExpBoxWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& inventory,
        BagMarks& marks,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_inventory(inventory),
          m_marks(marks),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_grid(screen.get<ui::IconGrid>("ExpBoxCells")),
          m_check(screen.get<ui::ImageButton>("ExpBoxAuto")),
          m_sortLocks(services, screen, "ExpBoxSortLock", "ExpBoxCells") {
        m_checkedPicture = services.pictures.find("btn_Check_1").value_or(gfx::PictureId{});
        m_uncheckedPicture = services.pictures.find("btn_UnCheck_1").value_or(gfx::PictureId{});
        screen.on("onExpBoxClose", [this](const ui::EventArgs&) { hide(); });
        screen.on("onExpBoxSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            m_services.connection.send(protocol::sortRequest(protocol::SortGrid::ExpBag, m_sortLocks.places()));
            m_sorter.startOnServer(m_screen.context().nowMs);
        });
        screen.on("onExpBoxConfirm", [this](const ui::EventArgs&) { confirm(); });
        screen.on("onExpBoxAuto", [this](const ui::EventArgs&) { toggleAuto(); });
        screen.on("onExpBoxTake", [this](const ui::EventArgs& args) { take(args.index); });
        screen.on("onExpBoxUse", [this](const ui::EventArgs& args) { use(args.index); });
    }

    void ExpBoxWindow::open(int bagSlot) {
        if (m_autoUse || m_window.isVisible()) {
            m_messages.show("It's in use!", kMessageMs);
            return;
        }
        if (m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_marks.useMark(m_bagSlot) == kMark) {
            m_marks.setUseMark(m_bagSlot, 0);
        }
        m_bagSlot = bagSlot;
        show();
    }

    void ExpBoxWindow::show() {
        refresh();
        if (const auto items = m_hooks.itemWindowBounds()) {
            const core::Rect area = m_window.bounds();
            const int left = kScreenWidth < items->x + items->width + area.width ? items->x - area.width : items->x + items->width;
            m_window.setPosition({left, items->y});
        }
        if (m_hooks.onPvpServer()) {
            m_autoUse = false;
        }
        m_autoEdit = m_autoUse;
        m_check.setPicture(m_autoEdit ? m_checkedPicture : m_uncheckedPicture);
        m_using = false;
        if (m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_inventory.bagItem(m_bagSlot).itemId == kBag && m_marks.useMark(m_bagSlot) == 0) {
            m_marks.setUseMark(m_bagSlot, kMark);
            m_hooks.marksChanged();
        }
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void ExpBoxWindow::hide() {
        m_window.setVisible(false);
        if (!m_autoUse && m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_marks.useMark(m_bagSlot) == kMark) {
            m_marks.setUseMark(m_bagSlot, 0);
            m_hooks.marksChanged();
        }
        m_using = false;
        m_autoEdit = m_autoUse;
    }

    void ExpBoxWindow::checkAutoUse() {
        if (!ExpBagAutoUse::due(
                {m_hooks.fightState(), m_hooks.sportActive(), m_hooks.onPvpServer(), m_autoDone, m_autoUse, m_hooks.doubleExpEnded(), m_using}
            )) {
            return;
        }
        const int cell = ExpBagAutoUse::firstFilled(std::vector<Cell>(m_cells.begin(), m_cells.end()));
        if (cell != 0) {
            use(cell);
            m_using = true;
            return;
        }
        m_autoDone = true;
    }

    void ExpBoxWindow::refresh() {
        m_grid.clearCells();
        for (int cell = 1; cell <= kCells; ++cell) {
            const Cell& held = m_cells[static_cast<std::size_t>(cell)];
            const auto record = held.itemId != 0 && held.count != 0 ? m_services.items.find(held.itemId) : std::nullopt;
            if (!record || record->unlisted != 0) {
                continue;
            }
            protocol::ItemInstance item;
            item.itemId = held.itemId;
            item.count = held.count;
            m_grid.setCell(cell, itemIconCell(m_services, item, true));
        }
    }

    int ExpBoxWindow::cellUnderMouse() const {
        return std::max(0, m_grid.dropIndexAt(m_screen.mouse() - m_window.bounds().position() - m_grid.bounds().position()));
    }

    void ExpBoxWindow::put(int bagSlot) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || m_inventory.bagItem(bagSlot).empty()) {
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(bagSlot);
        const auto record = m_services.items.find(item.itemId);
        if (!record || record->unlisted != 0) {
            return;
        }
        if (!isExpItem(*record)) {
            m_messages.show("The item can't be stored", kMessageMs);
            return;
        }
        if (const auto notice = m_marks.lockNotice(bagSlot, 0)) {
            m_messages.show(notice->text, notice->ms);
            return;
        }
        if (!m_marks.isAvailable(bagSlot)) {
            m_messages.show("Unable to operate when the item is locked", kMessageMs);
            return;
        }
        const int cell = cellUnderMouse();
        if (!validCell(cell)) {
            return;
        }
        const Cell& held = m_cells[static_cast<std::size_t>(cell)];
        if (held.itemId == 0 || (held.itemId == item.itemId && held.count < kMostInCell)) {
            m_services.connection.send(protocol::boxPutRequest(false, bagSlot, cell, m_bagSlot));
        }
    }

    void ExpBoxWindow::take(int cell) {
        if (!validCell(cell) || m_cells[static_cast<std::size_t>(cell)].itemId == 0 || m_cells[static_cast<std::size_t>(cell)].count == 0) {
            return;
        }
        if (m_inventory.freeSlotFor(kProbeItem) == 0) {
            m_messages.show("Not enough space", kMessageMs);
            return;
        }
        m_services.connection.send(protocol::boxTakeRequest(false, cell, m_bagSlot));
    }

    void ExpBoxWindow::use(int cell) {
        if (m_hooks.fightState() != 0) {
            m_messages.show("You can't use it during the battle", kMessageMs);
            return;
        }
        if (m_hooks.sportActive()) {
            m_messages.show("Can't use during game", kMessageMs);
            return;
        }
        if (m_hooks.onPvpServer()) {
            m_messages.show("Cannot use this item on a PvP Server", 3000); // message duration in ms
            return;
        }
        if (validCell(cell) && m_cells[static_cast<std::size_t>(cell)].itemId != 0 && m_cells[static_cast<std::size_t>(cell)].count != 0 &&
            m_services.items.find(m_cells[static_cast<std::size_t>(cell)].itemId)) {
            m_services.connection.send(protocol::boxUseRequest(false, cell, 0, m_bagSlot));
        }
    }

    void ExpBoxWindow::toggleAuto() {
        if (m_hooks.onPvpServer()) {
            m_messages.show("Cancel", 1000); // message duration in ms
            return;
        }
        m_autoEdit = !m_autoEdit;
        m_check.setPicture(m_autoEdit ? m_checkedPicture : m_uncheckedPicture);
    }

    void ExpBoxWindow::confirm() {
        m_using = false;
        if (m_autoUse != m_autoEdit) {
            m_messages.show(m_autoEdit ? "Turn on Auto Use of EXP potions/Capsules" : "Turn off Auto Use for EXP potions/Capsules ", kMessageMs);
        }
        m_autoUse = m_autoEdit;
        hide();
    }

    void ExpBoxWindow::receive(const core::Bytes& payload) {
        if (payload.size() < 4) { // minimum packet length (header, sub id, cell)
            return;
        }
        const int cell = payload[3]; // byte offset of the cell number (packet 23/218: main 23, sub-action 218)
        switch (payload[2]) { // byte offset of the sub id
            case 1: receiveAdd(payload); break; // 23/218 sub 1: a cell holds more
            case 2: // 23/218 sub 2: the cell is empty
                if (validCell(cell)) {
                    m_cells[static_cast<std::size_t>(cell)] = {};
                    refresh();
                }
                break;
            case 3: // 23/218 sub 3: one item of the cell was used
                m_using = false;
                if (validCell(cell)) {
                    Cell& held = m_cells[static_cast<std::size_t>(cell)];
                    if (held.count - 1 <= 0) {
                        held = {};
                    } else {
                        --held.count;
                    }
                    refresh();
                    m_services.playSound("sound/wav0152.wav");
                }
                break;
            default: break; // unknown sub id: ignored
        }
    }

    void ExpBoxWindow::receiveAdd(const core::Bytes& payload) {
        core::ByteReader reader(payload, 3); // start offset after header and sub id
        const int cell = reader.u8();
        const int itemId = reader.u16();
        const int count = reader.u8();
        const int notify = reader.u8();
        if (reader.failed() || !validCell(cell)) {
            return;
        }
        const auto record = m_services.items.find(itemId);
        if (!record || record->unlisted != 0 || !isExpItem(*record)) {
            return;
        }
        Cell& held = m_cells[static_cast<std::size_t>(cell)];
        held.itemId = itemId;
        held.count += count;
        m_autoDone = false;
        refresh();
        if (notify == 1) { // notify flag: show the find message
            const auto bag = m_services.items.find(kBag);
            m_hooks.centreMessage(
                std::to_string(count) + " Quantity " + record->name + " Find players to help play\rexp " + (bag ? bag->name : std::string()),
                1300
            ); // 1300: message duration in ms
            m_services.playSound("sound/wav0152.wav");
        }
    }

    void ExpBoxWindow::updateSort(std::uint64_t nowMs) {
        const bool shown = m_services.videoSettings().sortButtons && m_services.serverSorts;
        m_screen.get<ui::Element>("ExpBoxSort").setVisible(shown);
        m_sortLocks.update(shown, isOpen());
        for (const auto& [name, x] : {std::pair<const char*, int>{"ExpBoxConfirm", kConfirmX}, {"ExpBoxCancel", kCancelX}}) {
            ui::Element& button = m_screen.get<ui::Element>(name);
            button.setBounds({x - (shown ? kSortShift : 0), button.bounds().y, button.bounds().width, button.bounds().height});
        }
        if (!isOpen()) {
            m_sorter.stop();
            return;
        }
        if (const auto said = m_sorter.step({}, nowMs, nullptr)) {
            m_messages.show(*said);
        }
    }

}
