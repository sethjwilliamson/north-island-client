#include "client/viewmodels/pets/ExpressWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "client/rules/ExpressItems.h"
#include "gfx/TextStyle.h"
#include "protocol/ExpressRequests.h"
#include "ui/DrawingArea.h"

#include <algorithm>
#include <utility>

namespace client {

    namespace {

        constexpr int kShortMs = 1200; // short message display time, ms
        constexpr int kMessageMs = 1500; // delivery status message display time, ms
        constexpr int kLongMs = 2000; // long error message display time, ms
        constexpr std::uint64_t kAnswerMs = 5000; // longest wait for the answer to Send, ms
        constexpr int kStandingSlot = 11; // NPC action slot of the standing pose
        constexpr int kDeliveringSlot = 3; // NPC action slot of the delivering pose
        constexpr int kScreenWidth = 800; // game screen width, px

        /// @brief Loads ExpressWindow.xml into the screen and returns its window.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The "Express" window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("ExpressWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("Express");
        }

    }

    ExpressWindow::ExpressWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& inventory,
        BagMarks& marks,
        const PetTeam& pets,
        const FriendList& friends,
        const Guild& guild,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_inventory(inventory),
          m_marks(marks),
          m_pets(pets),
          m_friends(friends),
          m_guild(guild),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_bag(screen.get<ui::IconGrid>("ExpressBag")),
          m_placeAreas{
              screen.get<ui::DrawingArea>("ExpressSlot1"),
              screen.get<ui::DrawingArea>("ExpressSlot2"),
              screen.get<ui::DrawingArea>("ExpressSlot3"),
              screen.get<ui::DrawingArea>("ExpressSlot4"),
              screen.get<ui::DrawingArea>("ExpressSlot5")
          },
          m_recipient(screen.get<ui::TextBox>("ExpressRecipient")),
          m_names(screen.get<ui::ListBox>("ExpressList")),
          m_filters{screen.get<ui::CheckBox>("ExpressFilter1"), screen.get<ui::CheckBox>("ExpressFilter2"), screen.get<ui::CheckBox>("ExpressFilter3")},
          m_paintArea(screen.get<ui::DrawingArea>("ExpressPaint")),
          m_petSpot(screen.get<ui::Spot>("ExpressPet")),
          m_nameLineSpot(screen.get<ui::Spot>("ExpressNameLine")),
          m_textColorSpot(screen.get<ui::Spot>("ExpressTextColor")),
          m_sideWidthSpot(screen.get<ui::Spot>("ExpressSideWidth")),
          m_sideListSpot(screen.get<ui::Spot>("ExpressSideList")),
          m_sideScrollSpot(screen.get<ui::Spot>("ExpressSideScroll")),
          m_sideCloseSpot(screen.get<ui::Spot>("ExpressSideClose")),
          m_sideCloseSmallSpot(screen.get<ui::Spot>("ExpressSideCloseSmall")),
          m_sideFilterSpot(screen.get<ui::Spot>("ExpressSideFilter")),
          m_iconSpot(screen.get<ui::Spot>("ExpressIcon")),
          m_countSpot(screen.get<ui::Spot>("ExpressCount")) {
        m_paintArea.setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) { paintPet(canvas, at, nowMs); });
        for (int place = 1; place <= kPlaces; ++place) {
            m_placeAreas[static_cast<std::size_t>(place - 1)].get().setPainter([this, place](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                paintPlace(canvas, at, place);
            });
        }
        screen.on("onExpressPress", [this](const ui::EventArgs& args) { m_downSlot = args.index; });
        screen.on("onExpressAdd", [this](const ui::EventArgs& args) { addItem(args.index); });
        screen.on("onExpressRemove", [this](const ui::EventArgs& args) { removeItem(args.index); });
        screen.on("onExpressSlotClear", [this](const ui::EventArgs& args) {
            for (int place = 1; place <= kPlaces; ++place) {
                if (args.source == "ExpressSlot" + std::to_string(place)) {
                    removeFromPlace(place);
                }
            }
        });
        screen.on("onExpressSend", [this](const ui::EventArgs&) { send(); });
        screen.on("onExpressStop", [this](const ui::EventArgs&) { stop(); });
        screen.on("onExpressClose", [this](const ui::EventArgs&) { closeForm(); });
        screen.on("onExpressNames", [this](const ui::EventArgs&) { toggleNames(); });
        screen.on("onExpressListClose", [this](const ui::EventArgs&) { closeNames(); });
        screen.on("onExpressFilter", [this](const ui::EventArgs&) { refillNames(); });
        screen.on("onExpressListUp", [this](const ui::EventArgs&) { m_names.scrollUp(); });
        screen.on("onExpressListDown", [this](const ui::EventArgs&) { m_names.scrollDown(); });
        screen.on("onExpressPick", [this](const ui::EventArgs& args) { pickName(args.index); });
        const core::Rect area = m_window.bounds();
        m_window.setPosition({(kScreenWidth - area.width) / 2, (600 - area.height) / 2}); // 600: screen height in pixels; centres the window
    }

    void ExpressWindow::open(const Pet& pet) {
        m_petKind = pet.info.npcId;
        m_petInfo = pet.info;
        m_petName = pet.name;
        refillNames();
        refresh();
        m_window.setVisible(true);
        if (m_screen.context().bringAbovePopups) {
            m_screen.context().bringAbovePopups(m_window.id());
        }
    }

    void ExpressWindow::refresh() {
        m_bag.clearCells();
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            if (!item.empty() && !m_marks.isHidden(slot)) {
                m_bag.setCell(slot, itemIconCell(m_services, item, true, m_marks.isBusy(slot)));
            }
        }
        for (int place = 1; place <= kPlaces; ++place) {
            const int slot = m_bagSlots[static_cast<std::size_t>(place)];
            if (slot == 0) {
                continue;
            }
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            if (item.empty()) {
                m_marks.setUseMark(slot, 0);
                m_bagSlots[static_cast<std::size_t>(place)] = 0;
            }
        }
    }

    void ExpressWindow::paintPlace(gfx::Canvas& canvas, core::Point at, int place) {
        ui::DrawingArea& area = m_placeAreas[static_cast<std::size_t>(place - 1)];
        const int slot = m_bagSlots[static_cast<std::size_t>(place)];
        if (slot == 0 || m_inventory.bagItem(slot).empty()) {
            area.setHint("");
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        const ui::IconCell cell = itemIconCell(m_services, item, true);
        area.setHint(cell.hint);
        if (cell.picture.isValid()) {
            canvas.drawPicture(cell.picture, at, core::Rect{0, 0, m_iconSpot.size().width, m_iconSpot.size().height});
        }
        canvas.drawText(
            {at.x + m_countSpot.at().x, at.y + m_countSpot.at().y},
            std::to_string(item.count),
            {m_textColorSpot.color(), gfx::TextEffect::Plain, std::nullopt},
            area.bounds().width
        );
    }

    int ExpressWindow::freePlace() const {
        for (int place = 1; place <= kPlaces; ++place) {
            if (m_bagSlots[static_cast<std::size_t>(place)] == 0) {
                return place;
            }
        }
        return 0;
    }

    void ExpressWindow::addItem(int slot) {
        if (slot < 1 || slot > Inventory::kBagSlots || m_inventory.bagItem(slot).empty()) {
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        if (m_marks.vehicleState(slot) != 0) {
            m_messages.show("Unable to perform this action while using a vehicle", kLongMs);
            return;
        }
        if (m_delivering) {
            return;
        }
        if (const auto refusal = expressRefusal(m_services.items, item)) {
            m_messages.show(*refusal, kLongMs);
            return;
        }
        if (const auto notice = m_marks.lockNotice(slot, 0)) {
            m_messages.show(notice->text, notice->ms);
            return;
        }
        if (!m_marks.isAvailable(slot)) {
            m_messages.show("Unable to operate when the item is locked", kLongMs);
            return;
        }
        if (m_downSlot != slot || m_marks.useMark(slot) != 0) {
            return;
        }
        const int place = freePlace();
        if (place == 0) {
            m_messages.show("Express failed because of too many items", kLongMs);
            return;
        }
        m_bagSlots[static_cast<std::size_t>(place)] = slot;
        m_marks.setUseMark(slot, kExpressMark);
        m_hooks.marksChanged();
        refresh();
    }

    void ExpressWindow::removeItem(int slot) {
        if (slot < 1 || slot > Inventory::kBagSlots || !m_marks.isAvailable(slot) || m_marks.useMark(slot) != kExpressMark) {
            return;
        }
        for (int& bagSlot : m_bagSlots) {
            if (bagSlot == slot) {
                bagSlot = 0;
            }
        }
        m_marks.setUseMark(slot, 0);
        m_hooks.marksChanged();
        refresh();
    }

    void ExpressWindow::removeFromPlace(int place) {
        if (m_delivering || place < 1 || place > kPlaces || m_bagSlots[static_cast<std::size_t>(place)] == 0) {
            return;
        }
        m_marks.setUseMark(m_bagSlots[static_cast<std::size_t>(place)], 0);
        m_bagSlots[static_cast<std::size_t>(place)] = 0;
        m_hooks.marksChanged();
        refresh();
    }

    void ExpressWindow::send() {
        if (m_hooks.activePetKind() == m_petKind) {
            m_messages.show("Pets in a fight cannot be expressed", kShortMs);
            return;
        }
        if (m_delivering) {
            return;
        }
        if (m_hooks.autoDelivering()) {
            m_messages.show("Auto Deliver's pet is out, send again when it is back", kLongMs);
            return;
        }
        if (m_recipient.text().empty()) {
            m_messages.show("Express failed because no one to send", kLongMs);
            return;
        }
        const auto target = m_hooks.findPlayer(m_recipient.text());
        if (!target) {
            m_messages.show("Player does not exist or off-line. Unable to express", kLongMs);
            return;
        }
        std::vector<std::pair<int, int>> items;
        for (int place = 1; place <= kPlaces; ++place) {
            const int slot = m_bagSlots[static_cast<std::size_t>(place)];
            if (slot != 0) {
                items.emplace_back(slot, m_inventory.bagItem(slot).count);
            }
        }
        m_services.connection.send(protocol::expressStartRequest(m_petKind, *target, items));
        m_sentMs = m_nowMs;
    }

    void ExpressWindow::stop() {
        m_stopped = true;
        m_services.connection.send(protocol::expressStopRequest());
    }

    void ExpressWindow::closeForm() {
        if (!m_delivering) {
            clearItems();
            m_petKind = 0;
        }
        hide();
    }

    void ExpressWindow::clearItems() {
        for (int& slot : m_bagSlots) {
            if (slot != 0) {
                m_marks.setUseMark(slot, 0);
                m_marks.setBusyKind(slot, 0);
                slot = 0;
            }
        }
        m_hooks.marksChanged();
        refresh();
    }

    void ExpressWindow::lockItems() {
        for (const int slot : m_bagSlots) {
            if (slot != 0) {
                m_marks.setUseMark(slot, kExpressMark);
                m_marks.setBusyKind(slot, kDeliveringBusy);
            }
        }
        m_hooks.marksChanged();
        refresh();
    }

    void ExpressWindow::deliveryState(std::int32_t petKind, int state) {
        m_sentMs = 0;
        const int place = m_pets.placeOf(petKind);
        if (place == 0 || !m_pets.at(place)) {
            return;
        }
        const std::string& name = m_pets.at(place)->name;
        if (state == 1) {
            m_delivering = true;
            m_messages.show(name + " Start to deliver", kMessageMs);
            m_stopped = false;
            lockItems();
        } else if (state == 2) {
            m_delivering = false;
            m_messages.show(name + " " + (m_stopped ? "Stop delivery" : "Delivery is over"), kMessageMs);
            m_stopped = false;
            clearItems();
        }
    }

    void ExpressWindow::failed(int code, int itemId) {
        m_sentMs = 0;
        std::string text;
        switch (code) {
            case 1: { // 25/22 code 1: not enough quantity of the item
                const auto record = m_services.items.find(itemId);
                text = (record ? record->name : std::string()) + "Not enough quantity, cancel expression.";
                break;
            }
            case 2: text = "The target is offline, cancel expression."; break; // 25/22 code 2: target offline
            case 3: text = "The target 's inventory is full, cancel expression."; break; // 25/22 code 3: target's inventory full
            case 4: text = "The target is in the jail, cancel expression."; break; // 25/22 code 4: target in jail
            case 5: text = "In jail, cancel express."; break; // 25/22 code 5: sender in jail
            case 6: text = "Different info, cancel express."; break; // 25/22 code 6: info differs
            case 7: text = "The target is playing a game and cancelling the express"; break; // 25/22 code 7: target is playing a game
            case 8: text = "You are on the target's blacklist, cannot send"; break; // 25/22 code 8: sender on target's blacklist
            default: return; // unknown code: no message
        }
        m_messages.show(text, kMessageMs);
    }

    void ExpressWindow::result(int kind, std::int32_t otherId, int itemId, int count, bool onScreen) {
        const std::string other = m_hooks.playerName(otherId);
        if (other.empty() || (kind != 1 && kind != 2)) {
            return;
        }
        const auto record = m_services.items.find(itemId);
        const std::string item = record ? record->name : std::string();
        const std::string text = kind == 1 ? other + " send " + item + " " + std::to_string(count) + " quantity"
                                           : "Expression Successful to " + other + " " + item + " " + std::to_string(count) + " quantity";
        if (onScreen) {
            m_messages.show(text, kShortMs);
        }
        m_hooks.chatLine(otherId, text);
    }

    void ExpressWindow::toggleNames() {
        showNames(!m_screen.get<ui::Element>("ExpressPanel").isVisible());
        refillNames();
    }

    void ExpressWindow::closeNames() { showNames(false); }

    void ExpressWindow::showNames(bool shown) {
        for (const char* name :
             {"ExpressPanel",
              "ExpressListScroll",
              "ExpressList",
              "ExpressListUp",
              "ExpressListDown",
              "ExpressListClose",
              "ExpressListCloseSmall",
              "ExpressFilter1",
              "ExpressFilter2",
              "ExpressFilter3"}) {
            m_screen.get<ui::Element>(name).setVisible(shown);
        }
        placeNames();
    }

    void ExpressWindow::placeNames() {
        const core::Rect area = m_window.bounds();
        const int left =
            kScreenWidth - m_paintArea.bounds().width - m_sideWidthSpot.size().width < area.x ? -m_sideWidthSpot.size().width : m_paintArea.bounds().width;
        const auto put = [this](const char* name, int x) {
            ui::Element& element = m_screen.get<ui::Element>(name);
            element.setPosition({x, element.bounds().y});
        };
        put("ExpressPanel", left);
        put("ExpressListScroll", left + m_sideScrollSpot.at().x);
        put("ExpressList", left + m_sideListSpot.at().x);
        put("ExpressListUp", left + m_sideScrollSpot.at().x);
        put("ExpressListDown", left + m_sideScrollSpot.at().x);
        put("ExpressListClose", left + m_sideCloseSpot.at().x);
        put("ExpressListCloseSmall", left + m_sideCloseSmallSpot.at().x);
        for (int filter = 1; filter <= 3; ++filter) { // the three recipient-list tick boxes
            put(("ExpressFilter" + std::to_string(filter)).c_str(), left + m_sideFilterSpot.at().x + (filter - 1) * m_sideFilterSpot.pitch().x);
        }
    }

    void ExpressWindow::update(std::uint64_t nowMs) {
        m_nowMs = nowMs;
        if (m_sentMs != 0 && nowMs - m_sentMs >= kAnswerMs) {
            m_sentMs = 0;
        }
        if (m_window.isVisible()) {
            placeNames();
        }
    }

    void ExpressWindow::refillNames() {
        m_names.clear();
        const auto add = [this](const std::string& name) {
            for (int row = 0; row < m_names.itemCount(); ++row) {
                if (m_names.itemText(row) == name) {
                    return;
                }
            }
            m_names.addItem(name);
        };
        if (m_filters[0].get().isChecked()) {
            for (const std::string& name : m_hooks.teamNames()) {
                add(name);
            }
        }
        if (m_filters[1].get().isChecked()) {
            for (int index = 1; index <= m_friends.count(); ++index) {
                add(m_friends.at(index).record.name);
            }
        }
        if (m_filters[2].get().isChecked()) {
            for (int index = 0; index < m_guild.count(); ++index) {
                const protocol::GuildMemberRecord& member = m_guild.at(index).record;
                if (member.id != m_hooks.ownId() && member.id != 0) {
                    add(member.name);
                }
            }
        }
    }

    void ExpressWindow::pickName(int row) {
        if (row < 0 || row >= m_names.itemCount()) {
            return;
        }
        m_recipient.setText(m_names.itemText(row));
        showNames(false);
    }

    void ExpressWindow::paintPet(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        if (m_petKind == 0) {
            return;
        }
        const int direction = m_delivering ? kDeliveringSlot : kStandingSlot;
        const int slot = m_services.characters.npcSlot(m_petInfo.spriteId, m_petInfo.directionKind, direction);
        const core::Size size = m_services.characters.npcFrameSize(m_petInfo.spriteId, slot);
        const int rise = m_petInfo.spriteSize == 0 ? 16 : 50; // pixels the pet is lifted: small sprite 16, large sprite 50
        const int frame = m_animation.frame(nowMs, m_services.characters.npcFrameCount(m_petInfo.spriteId, slot));
        m_services.characters.drawNpc(canvas, m_petInfo, {at.x + m_petSpot.at().x, at.y + m_petSpot.at().y + size.height / 2 - rise}, slot, frame);
        const gfx::TextStyle style{m_textColorSpot.color(), gfx::TextEffect::Plain, std::nullopt};
        canvas.drawText(
            {at.x + m_nameLineSpot.at().x - static_cast<int>(m_petName.size()) / 2 * 8, at.y + m_nameLineSpot.at().y},
            m_petName,
            style,
            m_paintArea.bounds().width,
            15
        ); // 8: pixels per name character (half-width centring); 15: line height
    }

}
