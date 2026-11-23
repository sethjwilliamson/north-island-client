#include "client/viewmodels/system/AutoDeliverWindow.h"

#include "client/rules/ExpressItems.h"
#include "core/Log.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>

namespace client {

    namespace {

        constexpr int kMessageMs = 2000; // message time, ms
        constexpr int kIconSize = 30; // width and height drawn of an item's icon, px

        /// @brief Loads AutoDeliverWindow.xml into the screen and returns its window.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The AutoDeliver window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("AutoDeliverWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("AutoDeliver");
        }

        /// @brief Collects the cells of the items.
        /// @param screen Screen holding the controls.
        /// @return AutoDeliverItem1..10.
        std::array<std::reference_wrapper<ui::DrawingArea>, AutoDeliverSettings::kItems> itemCells(ui::Screen& screen) {
            const auto cell = [&screen](int number) {
                return std::ref(screen.get<ui::DrawingArea>("AutoDeliverItem" + std::to_string(number)));
            };
            return {cell(1), cell(2), cell(3), cell(4), cell(5), cell(6), cell(7), cell(8), cell(9), cell(10)};
        }

    }

    AutoDeliverWindow::AutoDeliverWindow(
        GameServices& services,
        ui::Screen& screen,
        AutoDeliver& deliver,
        const Inventory& inventory,
        const PetTeam& pets,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_deliver(deliver),
          m_inventory(inventory),
          m_pets(pets),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_on(screen.get<ui::CheckBox>("AutoDeliverOn")),
          m_to(screen.get<ui::TextBox>("AutoDeliverTo")),
          m_online(screen.get<ui::Label>("AutoDeliverOnline")),
          m_offline(screen.get<ui::Label>("AutoDeliverOffline")),
          m_pet(screen.get<ui::Label>("AutoDeliverPet")),
          m_status(screen.get<ui::Label>("AutoDeliverStatus")),
          m_nameList(screen.get<ui::Element>("AutoDeliverNameList")),
          m_names(screen.get<ui::ListBox>("AutoDeliverNameRows")),
          m_cells(itemCells(screen)) {
        for (int cell = 1; cell <= AutoDeliverSettings::kItems; ++cell) {
            m_cells[static_cast<std::size_t>(cell - 1)].get().setPainter([this, cell](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                paintItem(canvas, at, cell);
            });
        }
        screen.on("onAutoDeliverCancel", [this](const ui::EventArgs&) { close(); });
        screen.on("onAutoDeliverOk", [this](const ui::EventArgs&) {
            if (apply(edited())) {
                close();
            }
        });
        screen.on("onAutoDeliverSendNow", [this](const ui::EventArgs&) { sendNow(); });
        screen.on("onAutoDeliverPet", [this](const ui::EventArgs& args) { stepPet(args.source == "AutoDeliverPetBack" ? -1 : 1); });
        screen.on("onAutoDeliverNames", [this](const ui::EventArgs&) { toggleNames(); });
        screen.on("onAutoDeliverPick", [this](const ui::EventArgs& args) { pickName(args.index); });
        screen.on("onAutoDeliverItemClear", [this](const ui::EventArgs& args) {
            for (int cell = 1; cell <= AutoDeliverSettings::kItems; ++cell) {
                if (args.source == "AutoDeliverItem" + std::to_string(cell)) {
                    m_draft.items[static_cast<std::size_t>(cell - 1)] = 0;
                }
            }
        });
    }

    void AutoDeliverWindow::follow() {
        const AccountSession& session = m_services.session;
        const std::optional<Owner> owner =
            session.loginId == 0 ? std::nullopt : std::optional<Owner>(Owner{session.serverId, session.loginId, session.playingSlot});
        if (owner == m_owner) {
            return;
        }
        m_owner = owner;
        close();
        m_deliver.setSettings(
            owner ? AutoDeliverSettings::read(
                        AutoDeliverSettings::pathFor(m_services.game.userRoot(), session.serverId, session.loginId), session.playingSlot
                    )
                  : AutoDeliverSettings{}
        );
    }

    void AutoDeliverWindow::toggle() {
        if (m_window.isVisible()) {
            close();
            return;
        }
        follow();
        m_draft = m_deliver.settings();
        // never set up: ticked, so that a name, some items and OK are all it takes
        m_on.setChecked(m_draft.on || (m_draft.recipient.empty() && !m_draft.hasItems()));
        m_to.setText(m_draft.recipient);
        m_nameList.setVisible(false);
        m_window.setVisible(true);
        m_hooks.showBag(m_window.bounds().x + m_window.bounds().width); // beside the form: a drag raises the bag, which would cover cells
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void AutoDeliverWindow::close() {
        if (!m_window.isVisible()) {
            return;
        }
        m_nameList.setVisible(false);
        m_window.setVisible(false);
        m_hooks.putBagBack(); // it covers the Remote Control's OK
    }

    AutoDeliverSettings AutoDeliverWindow::edited() const {
        AutoDeliverSettings settings = m_draft;
        const std::string& typed = m_to.text();
        const std::size_t first = typed.find_first_not_of(' ');
        settings.recipient = first == std::string::npos ? std::string() : typed.substr(first, typed.find_last_not_of(' ') - first + 1);
        // an untouched form is Off, not an error
        settings.on = m_on.isChecked() && (!settings.recipient.empty() || settings.hasItems());
        return settings;
    }

    bool AutoDeliverWindow::apply(AutoDeliverSettings settings) {
        std::string refusal;
        if (!settings.recipient.empty() && settings.recipient == m_hooks.ownName()) {
            refusal = "You cannot deliver to yourself";
        } else if (settings.on) {
            if (settings.recipient.empty()) {
                refusal = "Choose who to deliver to";
            } else if (!settings.hasItems()) {
                refusal = "Drag the items to send from the bag";
            } else if (const auto why = m_hooks.refusal()) {
                refusal = *why;
            }
        }
        if (!refusal.empty()) {
            m_messages.show(refusal, kMessageMs);
            return false;
        }
        m_deliver.setSettings(settings);
        if (m_owner) {
            const auto& [server, login, slot] = *m_owner;
            if (!settings.write(AutoDeliverSettings::pathFor(m_services.game.userRoot(), server, login), slot)) {
                core::logWarning("cannot write the Auto Deliver settings");
            }
        }
        m_draft = settings;
        return true;
    }

    void AutoDeliverWindow::sendNow() {
        if (const auto why = m_hooks.refusal()) {
            m_messages.show(*why, kMessageMs);
            return;
        }
        AutoDeliverSettings settings = edited();
        if (settings.recipient.empty() || !settings.hasItems()) {
            m_messages.show(settings.recipient.empty() ? "Choose who to deliver to" : "Drag the items to send from the bag", kMessageMs);
            return;
        }
        if (!apply(settings)) {
            return;
        }
        if (const auto why = m_deliver.sendNow()) {
            m_messages.show(*why, kMessageMs);
        }
    }

    void AutoDeliverWindow::remoteStarting() {
        if (m_window.isVisible()) {
            apply(edited());
            close();
        }
    }

    void AutoDeliverWindow::stepPet(int by) {
        std::vector<int> kinds{0}; // 0: any free pet
        for (int place = 1; place <= PetTeam::kPlaces; ++place) {
            if (const auto& pet = m_pets.at(place); pet && pet->info.type == PetTeam::kExpressType) {
                kinds.push_back(pet->info.npcId);
            }
        }
        const auto found = std::find(kinds.begin(), kinds.end(), m_draft.petKind);
        const int at = found == kinds.end() ? 0 : static_cast<int>(found - kinds.begin());
        const int count = static_cast<int>(kinds.size());
        m_draft.petKind = kinds[static_cast<std::size_t>((at + by + count) % count)];
    }

    void AutoDeliverWindow::toggleNames() {
        if (m_nameList.isVisible()) {
            m_nameList.setVisible(false);
            return;
        }
        m_names.clear();
        for (const std::string& name : m_hooks.names()) {
            m_names.addItem(name, m_hooks.online(name) ? std::nullopt : std::optional<core::Color>(core::Color{150, 150, 150, 255})); // offline: grey
        }
        if (m_names.itemCount() == 0) {
            m_messages.show("Nobody to list: type a name", kMessageMs);
            return;
        }
        m_names.setTopIndex(0);
        m_nameList.setVisible(true);
    }

    void AutoDeliverWindow::pickName(int row) {
        if (row >= 0 && row < m_names.itemCount()) {
            m_to.setText(m_names.itemText(row));
            m_nameList.setVisible(false);
        }
    }

    void AutoDeliverWindow::dropItem(int cell, int slot) {
        if (!m_window.isVisible() || cell < 1 || cell > AutoDeliverSettings::kItems) {
            return;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        if (item.empty()) {
            return;
        }
        if (const auto refusal = expressRefusal(m_services.items, item)) {
            m_messages.show(*refusal, kMessageMs);
            return;
        }
        std::replace(m_draft.items.begin(), m_draft.items.end(), static_cast<int>(item.itemId), 0); // each kind once
        m_draft.items[static_cast<std::size_t>(cell - 1)] = item.itemId;
    }

    void AutoDeliverWindow::update() {
        follow();
        if (!m_window.isVisible()) {
            return;
        }
        if (!m_hooks.remoteFormOpen()) {
            close();
            return;
        }
        const AutoDeliverSettings settings = edited();
        const bool named = !settings.recipient.empty();
        const bool online = named && m_hooks.online(settings.recipient);
        m_online.setText(online ? "online" : "");
        m_offline.setVisible(named && !online);
        const int place = settings.petKind != 0 ? m_pets.placeOf(settings.petKind) : 0;
        const auto& pet = place != 0 ? m_pets.at(place) : std::nullopt;
        m_pet.setText(settings.petKind == 0 ? "Any free pet" : pet ? pet->name : "Pet not with you");
        m_status.setText(
            m_deliver.isBusy() ? m_deliver.status()
                               : "Next trip: " + std::to_string(m_deliver.readyCount(settings)) + "/" + std::to_string(AutoDeliver::kTripStacks) +
                                     " stacks ready"
        );
        for (std::size_t cell = 0; cell < m_cells.size(); ++cell) {
            const int itemId = settings.items[cell];
            const auto record = itemId != 0 ? m_services.items.find(itemId) : std::nullopt;
            m_cells[cell].get().setHint(record ? record->name + " (right click to remove)" : "Drop an item to send; padlocked places stay");
        }
    }

    void AutoDeliverWindow::paintItem(gfx::Canvas& canvas, core::Point at, int cell) {
        const core::Rect box{at.x, at.y, 34, 34}; // the cell's size, px
        canvas.fillRectAlpha(box, {30, 60, 120, 45}); // faint blue fill (RGBA)
        canvas.drawRectOutline(box, {30, 60, 120, 255}); // dark blue outline (RGBA)
        const int itemId = m_draft.items[static_cast<std::size_t>(cell - 1)];
        const auto record = itemId != 0 ? m_services.items.find(itemId) : std::nullopt;
        if (!record) {
            return;
        }
        if (const auto icon = m_services.pictures.find(std::to_string(record->iconId))) {
            const gfx::Picture& picture = m_services.pictures.picture(*icon);
            // 2: the gap between the cell's edge and the icon, px
            canvas.drawPicture(*icon, {at.x + 2, at.y + 2}, core::Rect{0, 0, std::min(picture.width(), kIconSize), std::min(picture.height(), kIconSize)});
        }
    }

}
