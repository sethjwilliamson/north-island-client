#include "client/viewmodels/items/PicnicWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "core/ByteReader.h"
#include "protocol/BoxRequests.h"
#include "protocol/ItemRequests.h"

#include <algorithm>
#include <string>
#include <utility>

namespace client {

    namespace {

        constexpr int kScreenWidth = 800; // game screen width, px
        constexpr int kMostInCell = 50; // most items stacked in one picnic cell
        constexpr int kMessageMs = 1000; // how long a player message stays up, ms
        constexpr int kFoodTypes[] = {
            23,
            32
        }; // item types the box takes: 23 = cooked food and drinks (Apple Juice, BBQ Pork Buns), 32 = ingredients (Apple, Beef)
        constexpr int kHpKind = 25; // item attribute kind 25: restores HP
        constexpr int kSpKind = 26; // item attribute kind 26: restores SP
        constexpr int kProbeItem = 10001; // item 10001 (type 1), only used to ask whether the bag has a free slot
        constexpr int kWatching = 4; // fight state 4: the player is watching a fight

        /// @brief Loads PicnicWindow.xml into the screen.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("PicnicWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("Picnic");
        }

        /// @brief Whether an item is food of a type the box takes.
        /// @param record Item record.
        /// @return True for food.
        bool isFood(const formats::ItemRecord& record) { return record.type == kFoodTypes[0] || record.type == kFoodTypes[1]; }

        /// @brief Whether an item gives HP or SP.
        /// @param record Item record.
        /// @return True when it has an HP or SP attribute.
        bool givesHpOrSp(const formats::ItemRecord& record) {
            for (const int kind : record.attributeKinds) {
                if (kind == kHpKind || kind == kSpKind) {
                    return true;
                }
            }
            return false;
        }

    }

    PicnicWindow::PicnicWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& inventory,
        BagMarks& marks,
        const PetTeam& pets,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_inventory(inventory),
          m_marks(marks),
          m_pets(pets),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_grid(screen.get<ui::IconGrid>("PicnicCells")),
          m_sortLocks(services, screen, "PicnicSortLock", "PicnicCells") {
        screen.on("onPicnicClose", [this](const ui::EventArgs&) { hide(); });
        screen.on("onPicnicSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            m_services.connection.send(protocol::sortRequest(protocol::SortGrid::Picnic, m_sortLocks.places()));
            m_sorter.startOnServer(m_screen.context().nowMs);
        });
        screen.on("onPicnicTake", [this](const ui::EventArgs& args) { take(args.index); });
        screen.on("onPicnicUse", [this](const ui::EventArgs& args) { use(args.index); });
    }

    void PicnicWindow::open(int bagSlot) {
        if (m_window.isVisible()) {
            m_messages.show("It's in use!", kMessageMs);
            return;
        }
        if (m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_marks.useMark(m_bagSlot) == kMark) {
            m_marks.setUseMark(m_bagSlot, 0);
        }
        m_bagSlot = bagSlot;
        show();
    }

    void PicnicWindow::show() {
        refresh();
        if (const auto items = m_hooks.itemWindowBounds()) {
            const core::Rect area = m_window.bounds();
            const int left = kScreenWidth < items->x + items->width + area.width ? items->x - area.width : items->x + items->width;
            m_window.setPosition({left, items->y});
        }
        if (m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_inventory.bagItem(m_bagSlot).itemId == kBox && m_marks.useMark(m_bagSlot) == 0) {
            m_marks.setUseMark(m_bagSlot, kMark);
            m_hooks.marksChanged();
        }
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void PicnicWindow::hide() {
        m_window.setVisible(false);
        if (m_bagSlot >= 1 && m_bagSlot <= BagMarks::kSlots && m_marks.useMark(m_bagSlot) == kMark) {
            m_marks.setUseMark(m_bagSlot, 0);
            m_hooks.marksChanged();
        }
    }

    void PicnicWindow::refresh() {
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

    int PicnicWindow::cellUnderMouse() const {
        return std::max(0, m_grid.dropIndexAt(m_screen.mouse() - m_window.bounds().position() - m_grid.bounds().position()));
    }

    void PicnicWindow::put(int bagSlot) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || m_inventory.bagItem(bagSlot).empty()) {
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(bagSlot);
        const auto record = m_services.items.find(item.itemId);
        if (!record || record->unlisted != 0) {
            return;
        }
        if (!isFood(*record) || !givesHpOrSp(*record)) {
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
            m_services.connection.send(protocol::boxPutRequest(true, bagSlot, cell, m_bagSlot));
        }
    }

    void PicnicWindow::take(int cell) {
        if (!validCell(cell) || m_cells[static_cast<std::size_t>(cell)].itemId == 0 || m_cells[static_cast<std::size_t>(cell)].count == 0) {
            return;
        }
        if (m_inventory.freeSlotFor(kProbeItem) == 0) {
            m_messages.show("Not enough space", kMessageMs);
            return;
        }
        m_services.connection.send(protocol::boxTakeRequest(true, cell, m_bagSlot));
    }

    void PicnicWindow::use(int cell) {
        if (!validCell(cell) || m_cells[static_cast<std::size_t>(cell)].itemId == 0 || m_cells[static_cast<std::size_t>(cell)].count == 0) {
            return;
        }
        const auto record = m_services.items.find(m_cells[static_cast<std::size_t>(cell)].itemId);
        if (!record || record->equipSlot == 0 || !isFood(*record)) {
            return;
        }
        m_useSlot = cell;
        const int state = m_hooks.fightState();
        if (state == 0 || state == kWatching) {
            m_aiming = true;
            m_hooks.aim(true);
            return;
        }
        m_useSlot = 0;
    }

    void PicnicWindow::cancelAim() {
        m_useSlot = 0;
        m_aiming = false;
        m_hooks.aim(false);
    }

    void PicnicWindow::useOn(int target, int petPlace) {
        const auto stop = [this] { cancelAim(); };
        if (m_hooks.fightState() != 0 || !m_aiming || !validCell(m_useSlot)) {
            stop();
            return;
        }
        const Cell& held = m_cells[static_cast<std::size_t>(m_useSlot)];
        if (held.itemId == 0 || held.count == 0) {
            stop();
            m_messages.show("Use another item to restore your energy!", 1500); // message display time in ms
            return;
        }
        const auto record = m_services.items.find(held.itemId);
        if (!record || record->equipSlot == 0 || !isFood(*record)) {
            stop();
            return;
        }
        int order = 0;
        // target 1 is the player, target 4 a pet of his
        if (target != 1 && target != 4) {
            return;
        }
        const std::optional<Pet>& pet = target == 4 ? m_pets.at(petPlace) : std::nullopt;
        if (target == 4 && !pet) {
            stop();
            return;
        }
        bool needed = false;
        for (const int kind : record->attributeKinds) {
            if (kind == kHpKind) {
                needed = target == 1 ? m_hooks.playerLacks(false) : pet->hp < pet->maxHp;
            } else if (kind == kSpKind) {
                needed = target == 1 ? m_hooks.playerLacks(true) : pet->sp < pet->maxSp;
            }
            if (needed) {
                break;
            }
        }
        if (!needed) {
            m_messages.show("Don't need it now", 1500); // message display time in ms
            return;
        }
        if (target == 4) {
            order = pet->order;
        }
        m_services.connection.send(protocol::boxUseRequest(true, m_useSlot, order, m_bagSlot));
    }

    void PicnicWindow::receive(const core::Bytes& payload) {
        // 4: the header is 3 bytes (sub id at byte 2) plus at least the cell byte
        if (payload.size() < 4) {
            return;
        }
        const int cell = payload[3]; // byte 3: the cell number
        switch (payload[2]) { // byte 2: the sub id
            case 1: receiveAdd(payload); break; // 23/217 1: a cell holds more (RecvPicnicAdd)
            case 2: // 23/217 2: a cell is empty (RecvPicnicRemove)
                if (validCell(cell)) {
                    m_cells[static_cast<std::size_t>(cell)] = {};
                    refresh();
                }
                break;
            case 3: receiveUsed(payload); break; // 23/217 3: one was used (RecvPicnicUsed)
            default: break; // unknown sub id: ignored
        }
    }

    void PicnicWindow::receiveAdd(const core::Bytes& payload) {
        core::ByteReader reader(payload, 3); // 3: skip the header (sub id at byte 2)
        const int cell = reader.u8();
        const int itemId = reader.u16();
        const int count = reader.u8();
        const int notify = reader.u8();
        if (reader.failed() || !validCell(cell)) {
            return;
        }
        const auto record = m_services.items.find(itemId);
        if (!record || record->unlisted != 0 || !isFood(*record)) {
            return;
        }
        Cell& held = m_cells[static_cast<std::size_t>(cell)];
        held.itemId = itemId;
        held.count += count;
        refresh();
        if (notify == 1) {
            const auto box = m_services.items.find(kBox);
            // 1300: message display time in ms
            m_hooks.centreMessage(
                std::to_string(count) + " Quantity " + record->name + " Find players to help play\rexp " + (box ? box->name : std::string()),
                1300
            );
            m_services.playSound("sound/wav0152.wav");
        }
    }

    void PicnicWindow::receiveUsed(const core::Bytes& payload) {
        core::ByteReader reader(payload, 3); // 3: skip the header (sub id at byte 2)
        const int cell = reader.u8();
        const int target = reader.u8();
        const int hp = reader.u16();
        const int sp = reader.u16();
        if (reader.failed() || !validCell(cell) || m_services.items.find(m_cells[static_cast<std::size_t>(cell)].itemId) == std::nullopt) {
            return;
        }
        const bool fighting = m_hooks.fightState() != 0;
        if (!fighting) {
            const std::string subject = target == 0 ? m_hooks.ownName() : m_hooks.ownName() + " helps " + m_hooks.activePetName();
            if (hp > 0) {
                m_hooks.chatLine(subject + "recover HP:" + std::to_string(hp));
            }
            if (sp > 0) {
                m_hooks.chatLine(subject + "recover SP:" + std::to_string(sp));
            }
        }
        Cell& held = m_cells[static_cast<std::size_t>(cell)];
        if (held.count - 1 <= 0) {
            held = {};
        } else {
            --held.count;
        }
        refresh();
        if (!fighting) {
            m_services.playSound("sound/wav0152.wav");
        }
    }

    void PicnicWindow::updateSort(std::uint64_t nowMs) {
        const bool sortShown = m_services.videoSettings().sortButtons && m_services.serverSorts;
        m_screen.get<ui::Element>("PicnicSort").setVisible(sortShown);
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
