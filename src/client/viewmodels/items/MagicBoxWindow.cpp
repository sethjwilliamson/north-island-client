#include "client/viewmodels/items/MagicBoxWindow.h"

#include "client/rendering/painters/ItemCells.h"
#include "gfx/TextStyle.h"
#include "protocol/MagicBoxRequests.h"
#include "ui/DrawingArea.h"

#include <string>
#include <utility>

namespace client {

    namespace {

        /// @brief Loads MagicBoxWindow.xml into the screen.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("MagicBoxWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("MagicBox");
        }

    }

    MagicBoxWindow::MagicBoxWindow(
        GameServices& services,
        ui::Screen& screen,
        const Inventory& inventory,
        BagMarks& marks,
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
          m_image(screen.get<ui::DrawingArea>("MagicBoxImage")),
          m_countSpot(screen.get<ui::Spot>("MagicBoxCount")) {
        screen.get<ui::DrawingArea>("MagicBoxImage").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paint(canvas, at); });
        screen.on("onMagicBoxOk", [this](const ui::EventArgs&) { confirm(); });
        screen.on("onMagicBoxClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onMagicBoxTake", [this](const ui::EventArgs&) { clearImage(); });
    }

    void MagicBoxWindow::release(int slot) {
        if (slot >= 1 && slot <= BagMarks::kSlots && m_marks.useMark(slot) == kMark) {
            m_marks.setUseMark(slot, 0);
        }
    }

    void MagicBoxWindow::clearImage() {
        if (m_requestSent) {
            return;
        }
        m_shown = protocol::ItemInstance{};
        release(m_bagSlot);
        m_bagSlot = 0;
        m_hooks.marksChanged();
    }

    void MagicBoxWindow::reset() {
        release(m_boxSlot);
        m_boxSlot = 0;
        clearImage();
        m_requestSent = false;
        m_hooks.marksChanged();
    }

    void MagicBoxWindow::close() {
        if (!m_requestSent) {
            reset();
        }
        m_window.setVisible(false);
    }

    void MagicBoxWindow::open(int boxSlot) {
        if (m_requestSent || boxSlot < 1 || boxSlot > BagMarks::kSlots) {
            return;
        }
        if (m_inventory.bagItem(boxSlot).itemId != kBoxItem) {
            close();
            return;
        }
        reset();
        m_boxSlot = boxSlot;
        m_marks.setUseMark(boxSlot, kMark);
        const core::Rect items = m_hooks.itemWindowBounds();
        const core::Rect area = m_window.bounds();
        const int left = items.x - area.width < 0 ? items.x + items.width : items.x - area.width;
        m_window.setPosition({left, items.y});
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
        m_hooks.marksChanged();
    }

    void MagicBoxWindow::put(int bagSlot) {
        if (m_requestSent) {
            return;
        }
        clearImage();
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots || m_inventory.bagItem(bagSlot).empty()) {
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(bagSlot);
        if (!m_services.items.find(item.itemId)) {
            return;
        }
        if (const auto notice = m_marks.lockNotice(bagSlot, BagMarks::kCompound)) {
            m_messages.show(notice->text, notice->ms);
        } else if (!m_marks.isAvailable(bagSlot)) {
            m_messages.show("Unable to operate when the item is locked", 3000); // message display time in ms
        } else if (item.lookItemId == 0) {
            m_messages.show("The equipment which is not pose equipment can't be used.", 3000); // message display time in ms
        } else {
            m_bagSlot = bagSlot;
            m_shown = item;
            m_marks.setUseMark(bagSlot, kMark);
            m_hooks.marksChanged();
        }
    }

    void MagicBoxWindow::confirm() {
        if (m_requestSent || m_boxSlot == 0 || m_bagSlot == 0) {
            return;
        }
        const auto box = m_services.items.find(kBoxItem);
        m_hooks.ask("As pose equipment, it can't be taken back\rUse " + (box ? box->name : std::string()) + "?", [this] { send(); });
    }

    void MagicBoxWindow::send() {
        if (m_requestSent) {
            return;
        }
        m_hooks.send(protocol::magicBoxRequest(m_boxSlot, m_bagSlot));
        m_sentSlot = m_bagSlot;
        m_requestSent = true;
        close();
    }

    void MagicBoxWindow::result(int code) {
        // result codes: 3 the look is back, 4 it failed
        if (code == 3 || code == 4) {
            if (code == 3 && m_sentSlot != 0 && m_hooks.reverted) {
                m_hooks.reverted(m_sentSlot); // the server has cleared the pose: the piece shows and trades as itself again
            }
            m_sentSlot = 0;
            m_messages.show(code == 3 ? "Pose Reverted " : "Pose Reverted Failed", 3000); // message display time in ms
            reset();
        }
    }

    void MagicBoxWindow::update() {
        if (!m_window.isVisible()) {
            return;
        }
        if (m_inventory.bagItem(m_boxSlot).itemId != kBoxItem) {
            close();
            return;
        }
        if (m_bagSlot != 0 && m_inventory.bagItem(m_bagSlot).empty()) {
            clearImage();
        }
        if (m_shown.empty() || m_screen.hoveredId() != m_image.id()) {
            return;
        }
        const core::Point at = m_window.bounds().position() + m_image.bounds().position();
        m_info.show(m_shown, {at.x, at.y, m_image.bounds().width, m_image.bounds().height});
    }

    void MagicBoxWindow::paint(gfx::Canvas& canvas, core::Point at) {
        if (m_shown.empty()) {
            return;
        }
        const ui::IconCell cell = itemIconCell(m_services, m_shown, false);
        if (cell.picture.isValid()) {
            canvas.drawPicture(cell.picture, at, core::Rect{0, 0, m_image.bounds().width, m_image.bounds().height});
        }
        if (m_shown.count > 0) {
            canvas.drawText(
                {at.x + m_countSpot.at().x, at.y + m_countSpot.at().y},
                std::to_string(m_shown.count),
                {m_countSpot.color(), gfx::TextEffect::Plain, std::nullopt},
                m_image.bounds().width
            );
        }
    }

}
