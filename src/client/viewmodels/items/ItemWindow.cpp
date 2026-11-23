#include "client/viewmodels/items/ItemWindow.h"
#include "client/rules/ElementNames.h"

#include "client/rendering/painters/ItemCells.h"
#include "client/rules/PetWeapon.h"
#include "client/rules/CombatStats.h"
#include "client/rules/Equipping.h"
#include "client/rules/Experience.h"
#include "client/rules/ItemUsing.h"
#include "client/rules/PetEquipping.h"
#include "client/rules/PotentialRules.h"
#include "client/rules/StatBuffItems.h"
#include "client/rules/Vitality.h"
#include "gfx/TextStyle.h"
#include "protocol/ItemRequests.h"
#include "protocol/StatPointsRequest.h"
#include "protocol/StatusEffectRequests.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

namespace client {

    namespace {

        // status background pictures, index = job 1..6 (index 0 unused): Assassin, Samurai, Knight, Sage, Clergyman, Prophet
        const std::array<const char*, 7> kJobBackgrounds = {"", "Bg_Asn", "Bg_Samurai", "Bg_Knight", "Bg_Sage", "Bg_Clergyman", "Bg_Prophet"};
        // job icon pictures, same job order
        const std::array<const char*, 7> kJobIcons = {"", "Icon_Asn", "Icon_Samurai", "Icon_Knight", "Icon_Sage", "Icon_Clergyman", "Icon_Prophet"};
        // the three bars (HP, SP, exp): frame pictures and fill pictures
        const std::array<const char*, 3> kBarFrames = {"Panel34", "Panel35", "Panel36"};
        const std::array<const char*, 3> kBarFills = {"Panel31", "Panel32", "Panel33"};
        constexpr int kFigureSlot = 17; // character action slot the status figure is drawn in
        constexpr int kStatLimitBase = 602; // base cap of one stat; the requirement for the current potential is added
        constexpr std::array<int, 5> kStatAttributes = {28, 29, 27, 33, 30}; // attribute ids of STR, CON, INT, WIS, AGI in request 8/1
        constexpr std::array<const char*, 5> kPointButtons = {"ItemPointStr", "ItemPointCon", "ItemPointInt", "ItemPointWis", "ItemPointAgi"};
        constexpr int kPointBlinkMs = 200; // blink step of the add-point buttons, ms per frame
        constexpr int kPlayerNameChars = 21; // characters the figure tooltip is sized for when the player (not a pet) is shown
        constexpr int kFigureTipRise = 23; // figure tooltip height and its rise above the mouse, px
        constexpr int kFigureTipLow = 50; // extra y shift of the figure tooltip when it is moved down, px
        constexpr int kScreenWidth = 800; // game screen width, px

        /// @brief A label rectangle with its hint text.
        /// @details Original: LayoutStatLabelRects, StatLabelHints.
        struct LabelHint {
            core::Rect area;
            const char* text;
        };

        // hint rects (x, y, w, h in window pixels) with hint text: six slots, five stats, five battle values
        const std::array<LabelHint, 16> kLabelHints = {{
            {{84, 113, 50, 13}, "Head"},
            {{153, 219, 50, 13}, "Body"},
            {{15, 156, 50, 13}, "Weapon"},
            {{150, 159, 50, 13}, "Wrist"},
            {{81, 251, 50, 13}, "Feet"},
            {{10, 219, 50, 13}, "Special items"},
            {{102, 337, 27, 15}, "Strength"},
            {{102, 353, 27, 15}, "Constitution"},
            {{102, 369, 27, 15}, "Intelligence"},
            {{102, 385, 27, 15}, "Wisdom"},
            {{102, 401, 27, 15}, "Agility"},
            {{16, 337, 27, 15}, "Attack"},
            {{16, 353, 27, 15}, "Defense"},
            {{16, 369, 27, 15}, "Mana Attack"},
            {{16, 385, 27, 15}, "Mana Defense"},
            {{16, 401, 27, 15}, "Swift"},
        }};

        /// @brief Whether a point lies inside a rectangle.
        /// @param area The rectangle.
        /// @param point The point.
        /// @return True when inside.
        bool inside(const core::Rect& area, core::Point point) {
            return point.x >= area.x && point.y >= area.y && point.x < area.x + area.width && point.y < area.y + area.height;
        }

        /// @brief Name of the grid element of a worn slot.
        /// @param slot Worn slot 1..6.
        /// @return The element name.
        std::string slotGridName(int slot) {
            static const std::array<const char*, 7> names = {"", "ItemHead", "ItemBody", "ItemWeapon", "ItemWrist", "ItemFeet", "ItemSpecial"};
            return names[static_cast<std::size_t>(slot)];
        }

        /// @brief Loads ItemWindow.xml into the screen.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The loaded window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("ItemWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("ItemWindow");
        }

    }

    ItemWindow::ItemWindow(
        GameServices& services,
        ui::Screen& screen,
        const PlayerStatus& status,
        const Inventory& inventory,
        const PetTeam& pets,
        const BagMarks& marks,
        const MapCharacter& player,
        PlayerMessages& messages,
        ItemInfoPopup& info
    )
        : m_services(services),
          m_status(status),
          m_inventory(inventory),
          m_pets(pets),
          m_marks(marks),
          m_player(player),
          m_messages(messages),
          m_info(info),
          m_screen(screen),
          m_window(loadWindow(services, screen)),
          m_element(screen.get<ui::ImageView>("ItemElement")),
          m_level(screen.get<ui::DigitText>("ItemLevel")),
          m_bag(screen.get<ui::IconGrid>("ItemBag")),
          m_equipment{
              screen.get<ui::IconGrid>(slotGridName(1)),
              screen.get<ui::IconGrid>(slotGridName(2)),
              screen.get<ui::IconGrid>(slotGridName(3)),
              screen.get<ui::IconGrid>(slotGridName(4)),
              screen.get<ui::IconGrid>(slotGridName(5)),
              screen.get<ui::IconGrid>(slotGridName(6))
          },
          m_pointButtons{
              screen.get<ui::ImageButton>(kPointButtons[0]),
              screen.get<ui::ImageButton>(kPointButtons[1]),
              screen.get<ui::ImageButton>(kPointButtons[2]),
              screen.get<ui::ImageButton>(kPointButtons[3]),
              screen.get<ui::ImageButton>(kPointButtons[4])
          },
          m_close(screen.get<ui::ImageButton>("ItemClose")),
          m_pointsOk(screen.get<ui::ImageButton>("ItemPointsOk")),
          m_pointsCancel(screen.get<ui::ImageButton>("ItemPointsCancel")),
          m_closeSmall(screen.get<ui::ImageButton>("ItemCloseSmall")),
          m_statusOnly(screen.get<ui::ImageButton>("ItemStatusOnly")),
          m_itemsOnly(screen.get<ui::ImageButton>("ItemItemsOnly")),
          m_potential(screen.get<ui::ImageButton>("ItemPotential")),
          m_express(screen.get<ui::ImageButton>("ItemExpress")),
          m_previousCharacter(screen.get<ui::ImageButton>("ItemPreviousCharacter")),
          m_nextCharacter(screen.get<ui::ImageButton>("ItemNextCharacter")),
          m_levelWord(screen.get<ui::ImageView>("ItemLevelWord")),
          m_textSpot(screen.get<ui::Spot>("ItemStatusText")),
          m_buffedSpot(screen.get<ui::Spot>("ItemStatusBuffed")),
          m_figureSpot(screen.get<ui::Spot>("ItemFigureFeet")),
          m_tipFillSpot(screen.get<ui::Spot>("ItemTipFill")),
          m_tipBorderSpot(screen.get<ui::Spot>("ItemTipBorder")),
          m_expTipBorderSpot(screen.get<ui::Spot>("ItemExpTipBorder")),
          m_sortLocks(services, screen, "ItemSortLock", "ItemBag") {
        screen.get<ui::DrawingArea>("ItemStatus").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
            paintStatus(canvas, at, nowMs);
        });
        screen.get<ui::DrawingArea>("ItemName").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintName(canvas, at); });
        screen.on("onItemWindowClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onItemSort", [this](const ui::EventArgs&) {
            m_sortLocks.setPicking(false);
            if (m_services.serverSorts) {
                m_services.connection.send(protocol::sortRequest(protocol::SortGrid::Bag, m_sortLocks.places()));
                m_sorter.startOnServer(m_screen.context().nowMs);
            } else {
                m_sorter.start();
            }
        });
        screen.on("onUseItem", [this](const ui::EventArgs& args) { useBagItem(args.index); });
        screen.on("onItemRightClick", [this](const ui::EventArgs& args) { rightClickBagItem(args.index); });
        screen.on("onUnequip", [this](const ui::EventArgs& args) { unequip(args.index); });
        screen.on("onItemDrop", [this](const ui::EventArgs& args) { dropped(args); });
        screen.on("onItemQuickMove", [this](const ui::EventArgs& args) {
            if (m_services.videoSettings().modernBag && m_quickMove && m_marks.vehicleState(args.index) == 0) {
                m_quickMove(args.index);
            }
        });
        screen.on("onAddStatPoint", [this](const ui::EventArgs& args) {
            for (std::size_t i = 0; i < kPointButtons.size(); ++i) {
                if (args.source == kPointButtons[i]) {
                    addStatPoint(static_cast<int>(i));
                }
            }
        });
        screen.on("onSendStatPoints", [this](const ui::EventArgs&) { sendStatPoints(); });
        screen.on("onCancelStatPoints", [this](const ui::EventArgs&) { cancelStatPoints(); });
        screen.on("onItemStatusOnly", [this](const ui::EventArgs&) { setColumnMode(m_columnMode == 0 ? 1 : 0); });
        screen.on("onItemItemsOnly", [this](const ui::EventArgs&) { setColumnMode(m_columnMode == 0 ? 2 : 0); });
        screen.get<ui::DrawingArea>("ItemTips").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) { paintTips(canvas, at, nowMs); });
        screen.on("onItemPotential", [this](const ui::EventArgs&) {
            if (m_togglePotential) {
                m_togglePotential();
            }
        });
        screen.on("onItemExpress", [this](const ui::EventArgs&) {
            if (const auto pet = shownPet(); pet && m_openExpress) {
                m_openExpress(pet->get());
            }
        });
        screen.on("onItemPreviousCharacter", [this](const ui::EventArgs&) { stepCharacter(false); });
        screen.on("onItemNextCharacter", [this](const ui::EventArgs&) { stepCharacter(true); });
    }

    std::optional<std::reference_wrapper<const Pet>> ItemWindow::shownPet() const {
        const int place = m_pets.placeByOrder(m_shownOrder);
        if (m_shownOrder == 0 || place == 0 || !m_pets.at(place)) {
            return std::nullopt;
        }
        return std::cref(*m_pets.at(place));
    }

    int ItemWindow::shownPlace() const { return shownPet() ? m_pets.placeByOrder(m_shownOrder) : 0; }

    const protocol::ItemInstance& ItemWindow::shownEquipped(int slot) const {
        if (const auto pet = shownPet()) {
            return pet->get().worn.at(static_cast<std::size_t>(slot));
        }
        return m_inventory.equipped(slot);
    }

    void ItemWindow::showCharacter(int order) {
        m_shownOrder = order;
        if (!shownPet()) {
            m_shownOrder = 0;
        }
        cancelStatPoints();
        refresh();
    }

    void ItemWindow::stepCharacter(bool forward) {
        const int pets = m_pets.count();
        int order = m_shownOrder;
        if (forward) {
            order = order + 1 > pets ? 0 : order + 1;
        } else {
            order = order == 0 ? pets : order - 1;
        }
        showCharacter(order);
    }

    ItemWindow::Shown ItemWindow::shownStats() const {
        Shown shown;
        if (const auto found = shownPet()) {
            const Pet& pet = found->get();
            shown.name = pet.name;
            shown.level = pet.level;
            shown.reborn = pet.reborn;
            shown.hp = pet.hp;
            shown.maxHp = pet.maxHp;
            shown.sp = pet.sp;
            shown.maxSp = pet.maxSp;
            shown.exp = pet.exp;
            shown.stats = {pet.strength, pet.constitution, pet.intelligence, pet.wisdom, pet.agility};
            for (std::size_t i = 0; i < shown.combat.size(); ++i) {
                shown.combat[i] = pet.serverCombatKnown ? pet.serverCombat[i] : combatValue(
                    m_services.formula,
                    static_cast<int>(i),
                    totalLevel(pet.level, pet.reborn),
                    pet.recalcStats[i],
                    pet.recalcBonus[i],
                    pet.info.element
                );
            }
            shown.potential = pet.potential;
            shown.element = pet.info.element;
            return shown;
        }
        shown.name = m_player.name;
        shown.level = m_status.level;
        shown.reborn = m_status.reborn;
        shown.hp = m_status.hp;
        shown.maxHp = m_status.maxHp;
        shown.sp = m_status.sp;
        shown.maxSp = m_status.maxSp;
        shown.exp = m_status.exp;
        shown.stats = m_status.stats;
        shown.combat = m_status.combat;
        shown.potential = m_status.potential;
        shown.element = m_status.element;
        return shown;
    }

    void ItemWindow::open() {
        if (m_shown) {
            m_shown();
        }
        cancelStatPoints();
        m_shownOrder = 0;
        refresh();
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    std::optional<ItemWindow::HoveredItem> ItemWindow::hoveredItem() const {
        if (!isOpen()) {
            return std::nullopt;
        }
        const int hovered = m_screen.hoveredId();
        const core::Point window = m_window.bounds().position();
        auto inGrid = [&](const ui::IconGrid& grid, auto itemAt) -> std::optional<HoveredItem> {
            if (grid.id() != hovered) {
                return std::nullopt;
            }
            const core::Point origin = window + grid.bounds().position();
            const int cell = grid.cellAt(m_screen.mouse() - origin);
            const protocol::ItemInstance item = cell != 0 ? itemAt(cell) : protocol::ItemInstance{};
            if (item.empty()) {
                return std::nullopt;
            }
            const core::Point at = origin + grid.cellPosition(cell);
            const core::Size cells = m_inventory.cellsOf(item.itemId);
            return HoveredItem{
                item,
                core::Rect{at.x, at.y, cells.width * 34 - 2, cells.height * 34 - 2},
                grid.id() == m_bag.id()
            }; // 34: pixel size of one bag cell, minus 2 for the border
        };
        if (auto found = inGrid(m_bag, [this](int cell) { return m_inventory.bagItem(cell); })) {
            return found;
        }
        for (const ui::IconGrid& grid : m_equipment) {
            if (auto found = inGrid(grid, [this](int cell) { return shownEquipped(cell); })) {
                return found;
            }
        }
        return std::nullopt;
    }

    void ItemWindow::update() {
        // the bag and the Storeroom, whose things go to and fro between them, both carry things the modern way
        constexpr int kMostStack = 50; // most things in one stack, in the bag and the Storeroom
        m_screen.setModernDrag(
            m_services.videoSettings().modernBag ? std::vector<std::string>{"ItemBag", "StoreroomCells"} : std::vector<std::string>{}, kMostStack
        );
        // the bank's grids keep their clicks (they mark things) and take only the Shift click
        m_screen.setQuickMoveGrids(m_services.videoSettings().modernBag ? std::vector<std::string>{"BankItemBank", "BankItemBag"} : std::vector<std::string>{});
        if (const auto hovered = m_screen.isDragging() ? std::nullopt : hoveredItem()) { // nothing is told of what lies under a carried thing
            std::optional<protocol::ItemInstance> worn;
            const auto record = m_services.items.find(hovered->item.itemId);
            if (hovered->inBag && !shownPet() && record && isEquipment(*record) && canWear(*record, m_player.look.bodyType, m_status.element) &&
                !m_inventory.equipped(record->equipSlot).empty()) {
                worn = m_inventory.equipped(record->equipSlot);
            }
            m_info.show(hovered->item, hovered->box, worn);
        } else {
            m_info.hide();
        }
    }

    void ItemWindow::setColumnMode(int mode) {
        static const std::array<const char*, 3> kBackgrounds = {"Form_RoleStatus_1", "Form_RoleStatus_2", "Form_RoleStatus_3"};
        if (mode == m_columnMode) {
            return;
        }
        if (mode == 2) {
            cancelStatPoints();
        }
        const core::Rect before = m_window.bounds();
        m_window.setPicture(kBackgrounds[static_cast<std::size_t>(mode)], m_services.pictures);
        core::Rect area = m_window.bounds();
        if (mode == 2) {
            area.x = before.x + before.width - area.width;
        } else if (mode == 0 && m_columnMode == 2) {
            area.x = before.x - (area.width - before.width);
        }
        area.x = std::clamp(area.x, 0, kScreenWidth - area.width);
        m_window.setBounds(area);
        m_columnMode = mode;

        const bool status = mode != 2;
        const int width = area.width;
        m_close.setPosition({(width - m_close.bounds().width) / 2, m_close.bounds().y});
        // x of the small close button per mode: 336 both columns, 157 status only, 159 bag only
        m_closeSmall.setPosition({mode == 0 ? 336 : mode == 1 ? 157 : 159, m_closeSmall.bounds().y});
        m_statusOnly.setVisible(mode != 1);
        // x of the status-only button: 60 in bag-only mode, else 149
        m_statusOnly.setPosition({mode == 2 ? 60 : 149, m_statusOnly.bounds().y});
        m_statusOnly.setHint(mode == 2 ? "Display completed form" : "Only display status column");
        m_itemsOnly.setVisible(mode != 2);
        // x of the items-only button: 125 in status-only mode, else 222
        m_itemsOnly.setPosition({mode == 1 ? 125 : 222, m_itemsOnly.bounds().y});
        m_itemsOnly.setHint(mode == 1 ? "Display completed form" : "Only display item column");
        for (ui::IconGrid& grid : m_equipment) {
            grid.setVisible(status);
        }
        m_previousCharacter.setVisible(status);
        m_nextCharacter.setVisible(status);
        m_element.setVisible(status);
        m_levelWord.setVisible(status);
        m_level.setVisible(status);
        m_bag.setVisible(mode != 1);
        // x of the bag grid: 19 in bag-only mode, else 193
        m_bag.setPosition({mode == 2 ? 19 : 193, m_bag.bounds().y});
        // half gap in pixels between the OK and Cancel point buttons (10 in status-only mode, else 20)
        const int gap = mode == 1 ? 10 : 20;
        m_pointsOk.setPosition({width / 2 - gap - m_pointsOk.bounds().width, m_pointsOk.bounds().y});
        m_pointsCancel.setPosition({width / 2 + gap, m_pointsCancel.bounds().y});
        applyShown();
        syncPointButtons();
    }

    void ItemWindow::close() {
        if (m_shown) {
            m_shown();
        }
        if (m_spending) {
            cancelStatPoints();
        }
        m_window.setVisible(false);
    }

    int ItemWindow::pointsLeft() const {
        int pending = 0;
        for (const int points : m_pending) {
            pending += points;
        }
        return m_status.statPoints - pending;
    }

    void ItemWindow::addStatPoint(int stat) {
        if (pointsLeft() <= 0) {
            return;
        }
        if (!m_spending) {
            m_pending = {};
            m_spending = true;
            m_close.setVisible(false);
            m_pointsOk.setVisible(true);
            m_pointsCancel.setVisible(true);
        }
        const std::size_t index = static_cast<std::size_t>(stat);
        const int limit = kPetStatRequirements[static_cast<std::size_t>(std::clamp(m_status.potential, 0, kMostPotential))] + kStatLimitBase;
        if (m_status.stats[index] + m_pending[index] + 1 <= limit) {
            ++m_pending[index];
        }
        syncPointButtons();
    }

    void ItemWindow::sendStatPoints() {
        std::vector<std::pair<int, std::int32_t>> points;
        for (std::size_t i = 0; i < m_pending.size(); ++i) {
            if (m_pending[i] != 0) {
                points.emplace_back(kStatAttributes[i], m_pending[i]);
            }
        }
        m_services.connection.send(protocol::statPointsRequest(points));
        m_pending = {};
        m_spending = false;
        m_pointsOk.setVisible(false);
        m_pointsCancel.setVisible(false);
        m_close.setVisible(true);
        syncPointButtons();
    }

    void ItemWindow::cancelStatPoints() {
        m_pending = {};
        m_spending = false;
        m_pointsOk.setVisible(false);
        m_pointsCancel.setVisible(false);
        m_close.setVisible(true);
        syncPointButtons();
    }

    void ItemWindow::syncPointButtons() {
        const bool show = pointsLeft() > 0 && m_columnMode != 2 && !shownPet();
        for (ui::ImageButton& button : m_pointButtons) {
            button.setVisible(show);
        }
    }

    void ItemWindow::paintTips(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        const core::Point mouse = m_screen.mouse();
        const core::Point local = mouse - at;
        const gfx::TextStyle white{{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt};
        if (m_columnMode == 2) {
            return;
        }
        if (m_screen.hoveredId() == m_window.id()) {
            for (const LabelHint& label : kLabelHints) {
                if (!inside(label.area, local)) {
                    continue;
                }
                const int length = static_cast<int>(std::string(label.text).size());
                // tooltip: 8 px per character plus 6 px padding, 24 px high
                core::Rect box{at.x + label.area.x + label.area.width - (length * 8 + 6), at.y + label.area.y - 24, length * 8 + 6, 24};
                box.x = std::max(box.x, 2);
                canvas.fillRectAlpha(box, m_tipFillSpot.color());
                canvas.drawRectOutline(box, m_tipBorderSpot.color());
                // text offset 3 px inside the box; 320 max text width, 16 line height
                canvas.drawText({box.x + 3, box.y + 3}, label.text, white, 320, 16);
                break;
            }
        }
        const Shown shown = shownStats();
        // the experience bar rect (x, y, w, h) in window pixels
        if (inside({44, 305, 64, 8}, local)) {
            const std::int64_t reached = totalLevelExp(m_services.formula, shown.level, shown.reborn);
            const std::int64_t needed = levelUpExp(m_services.formula, shown.level + 1, shown.reborn);
            const std::string text = "Exp: " + std::to_string(shown.exp - reached) + "/" + std::to_string(needed);
            const int length = static_cast<int>(text.size());
            // tip box: 8 px per character (centered with length * 4), 7 px padding, 17 px high, 23 px above the mouse
            const core::Rect box{mouse.x - 1 - length * 4, mouse.y - 23, length * 8 + 7, 17};
            canvas.fillRectAlpha(box, m_tipFillSpot.color());
            canvas.drawRectOutline(box, m_expTipBorderSpot.color());
            // text 3 px inside the box; 400 max text width, 16 line height
            canvas.drawText({box.x + 3, box.y}, text, white, 400, 16);
        }
        const auto pet = shownPet();
        const int petSlot = pet ? m_services.characters.npcSlot(pet->get().info.spriteId, pet->get().info.directionKind, kFigureSlot) : 0;
        const int frame = pet ? m_animation.frame(nowMs, m_services.characters.npcFrameCount(pet->get().info.spriteId, petSlot))
                              : m_animation.frame(nowMs, m_services.characters.figureFrameCount(m_player.look, kFigureSlot));
        const bool overFigure = pet ? m_services.characters.npcContains(pet->get().info, at + m_figureSpot.at(), petSlot, frame, mouse)
                                    : m_services.characters.figureContains(m_player.look, at + m_figureSpot.at(), kFigureSlot, frame, mouse);
        if (m_columnMode != 2 && overFigure) {
            const std::string label = pet ? pet->get().name : "Character information";
            const int nameChars = pet ? static_cast<int>(label.size()) : kPlayerNameChars;
            // 8 px per character plus 7 px padding
            core::Rect box{mouse.x, mouse.y - kFigureTipRise, nameChars * 8 + 7, kFigureTipRise};
            if (box.right() > kScreenWidth) {
                box.x += kScreenWidth - box.right() - 2; // 2: margin from the screen edge
            }
            if (box.x < 0) {
                box.x = 2; // 2: margin from the screen edge
            }
            if (box.y < 0) {
                box.y += kFigureTipLow;
            }
            canvas.fillRectAlpha(box, m_tipFillSpot.color());
            canvas.drawRectOutline(box, m_expTipBorderSpot.color());
            // text offset (2, 4) inside the box; 320 max text width, 16 line height
            canvas.drawText({mouse.x + 2, mouse.y + 4 - kFigureTipRise}, label, white, 320, 16);
        }
    }

    void ItemWindow::toggle() {
        if (isOpen()) {
            close();
        } else {
            open();
        }
    }

    void ItemWindow::refresh() {
        m_bag.clearCells();
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            if (!item.empty() && !m_marks.isHidden(slot)) {
                m_bag.setCell(slot, itemIconCell(m_services, item, true, m_marks.isBusy(slot)));
            }
        }
        if (m_shownOrder != 0 && !shownPet()) {
            m_shownOrder = 0;
        }
        for (int slot = 1; slot <= Inventory::kEquipmentSlots; ++slot) {
            ui::IconGrid& grid = m_equipment[static_cast<std::size_t>(slot - 1)];
            grid.clearCells();
            const protocol::ItemInstance& item = shownEquipped(slot);
            if (!item.empty()) {
                grid.setCell(slot, itemIconCell(m_services, item, false));
            }
        }
        applyShown();
        syncPointButtons();
    }

    void ItemWindow::applyShown() {
        const Shown shown = shownStats();
        m_element.setPicture(m_services.pictures.find("Icon_Element_" + std::to_string(shown.element) + "_1").value_or(gfx::PictureId{}));
        m_element.setHint(elementName(shown.element));
        m_level.setText(std::to_string(shown.level));
        const auto pet = shownPet();
        m_potential.setVisible(m_columnMode != 2 && (!pet || pet->get().info.mercenary == 0));
        m_express.setVisible(m_columnMode != 2 && pet && pet->get().info.type == PetTeam::kExpressType);
    }

    void ItemWindow::paintBar(gfx::Canvas& canvas, core::Point at, int bar, double ratio) {
        constexpr int kBarTop = 277; // y of the HP bar frame in the window, px
        constexpr int kBarStep = 14; // vertical distance between the HP, SP and exp bars, px
        const std::size_t index = static_cast<std::size_t>(bar);
        // 42: x of the bar frame in the window
        const core::Point frame{at.x + 42, at.y + kBarTop + bar * kBarStep};
        if (const auto picture = m_services.pictures.find(kBarFrames[index])) {
            canvas.drawPicture(*picture, frame);
        }
        if (const auto fill = m_services.pictures.find(kBarFills[index])) {
            const gfx::Picture& picture = m_services.pictures.picture(*fill);
            const int width = static_cast<int>(std::ceil(picture.width() * std::clamp(ratio, 0.0, 1.0)));
            if (width > 0) {
                // the fill lies 2 px inside the frame
                canvas.drawPicture(*fill, {frame.x + 2, frame.y + 2}, core::Rect{0, 0, width, picture.height()});
            }
        }
    }

    void ItemWindow::paintName(gfx::Canvas& canvas, core::Point at) {
        if (m_columnMode != 2) {
            // name position (49, 61) in the window; 138 max text width, 20 line height
            canvas.drawText({at.x + 49, at.y + 61}, shownStats().name, {m_textSpot.color(), gfx::TextEffect::Plain, std::nullopt}, 138, 20);
        }
    }

    void ItemWindow::paintStatus(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        if (m_columnMode == 2) {
            return;
        }
        const gfx::TextStyle text{m_textSpot.color(), gfx::TextEffect::Plain, std::nullopt};
        const Shown shown = shownStats();
        const auto pet = shownPet();
        const std::size_t job = pet ? 0 : static_cast<std::size_t>(m_status.job);
        if (job >= 1 && job < kJobBackgrounds.size()) {
            if (const auto background = m_services.pictures.find(kJobBackgrounds[job])) {
                canvas.drawPicture(*background, {at.x + 13, at.y + 96}); // job background position (13, 96)
            }
        }
        if (pet) {
            const formats::NpcInfo& npc = pet->get().info;
            const int slot = m_services.characters.npcSlot(npc.spriteId, npc.directionKind, kFigureSlot);
            const int frame = m_animation.frame(nowMs, m_services.characters.npcFrameCount(npc.spriteId, slot));
            const PetWeapon weapon = petWeapon(pet->get(), m_services.items);
            m_services.characters.drawArmedNpc(canvas, npc, weapon.lookId, weapon.colorCode, at + m_figureSpot.at(), slot, frame);
            if (shown.reborn == 1) {
                if (const auto icon = m_services.pictures.find("Okey3")) {
                    canvas.drawPicture(*icon, {at.x + 23, at.y + 107}); // reborn / job icon position (23, 107)
                }
            }
        } else if (m_player.shape) {
            const formats::NpcInfo& npc = *m_player.shape;
            const int slot = m_services.characters.npcSlot(npc.spriteId, npc.directionKind, kFigureSlot);
            const int frame = m_animation.frame(nowMs, m_services.characters.npcFrameCount(npc.spriteId, slot));
            m_services.characters.drawNpc(canvas, npc, at + m_figureSpot.at(), slot, frame);
            if (job >= 1 && job < kJobIcons.size()) {
                if (const auto icon = m_services.pictures.find(kJobIcons[job])) {
                    canvas.drawPicture(*icon, {at.x + 23, at.y + 107}); // job icon position (23, 107)
                }
            }
        } else {
            const int frame = m_animation.frame(nowMs, m_services.characters.figureFrameCount(m_player.look, kFigureSlot));
            m_services.characters.drawFigure(canvas, m_player.look, at + m_figureSpot.at(), kFigureSlot, frame);
            if (job >= 1 && job < kJobIcons.size()) {
                if (const auto icon = m_services.pictures.find(kJobIcons[job])) {
                    canvas.drawPicture(*icon, {at.x + 23, at.y + 107}); // job icon position (23, 107)
                }
            }
        }

        const double hpRatio = shown.maxHp > 0 ? static_cast<double>(shown.hp) / shown.maxHp : 0.0;
        const double spRatio = shown.maxSp > 0 ? static_cast<double>(shown.sp) / shown.maxSp : 0.0;
        const double expRatio = levelProgressSigned(m_services.formula, shown.level, shown.exp, shown.reborn);
        paintBar(canvas, at, 0, hpRatio);
        paintBar(canvas, at, 1, spRatio);
        paintBar(canvas, at, 2, expRatio);
        // bar texts at x 113, y 273 / 287 / 301 (14 px apart); 200 max text width, 20 line height; * 100 makes a percent
        canvas.drawText({at.x + 113, at.y + 273}, std::to_string(shown.hp) + "/" + std::to_string(shown.maxHp), text, 200, 20);
        canvas.drawText({at.x + 113, at.y + 287}, std::to_string(shown.sp) + "/" + std::to_string(shown.maxSp), text, 200, 20);
        canvas.drawText({at.x + 113, at.y + 301}, std::to_string(static_cast<int>(std::trunc(expRatio * 100))) + "%", text, 200, 20);

        if (pet) {
            if (const auto faith = m_services.pictures.find("Icon_Faith_1")) {
                canvas.drawPicture(*faith, {at.x + 16, at.y + 319}); // amity icon position (16, 319)
            }
            // amity text (63, 317); points-left text (160, 317); 200 max text width, 20 line height
            canvas.drawText({at.x + 63, at.y + 317}, std::to_string(pet->get().amity), text, 200, 20);
            canvas.drawText({at.x + 160, at.y + 317}, "0", text, 200, 20);
        } else {
            if (const auto gold = m_services.pictures.find("Icon_Gold_1")) {
                canvas.drawPicture(*gold, {at.x + 14, at.y + 318}); // gold icon position (14, 318)
            }
            // gold text (45, 318); points-left text (160, 317); 200 max text width, 20 line height
            canvas.drawText({at.x + 45, at.y + 318}, std::to_string(m_status.gold), text, 200, 20);
            canvas.drawText({at.x + 160, at.y + 317}, std::to_string(pointsLeft()), text, 200, 20);
        }
        constexpr int kStatTop = 337; // y of the first stat row (STR), px in the window
        constexpr int kStatStep = 16; // vertical distance between stat rows, px
        for (std::size_t i = 0; i < shown.stats.size(); ++i) {
            const int y = at.y + kStatTop + static_cast<int>(i) * kStatStep;
            // stat number at x 130, combat number at x 44; 200 max text width, 20 line height
            canvas.drawText({at.x + 130, y}, std::to_string(shown.stats[i]), text, 200, 20);
            const bool buffed = !pet && m_buffed && m_buffed(static_cast<int>(i) + 1);
            canvas.drawText(
                {at.x + 44, y},
                std::to_string(shown.combat[i]),
                buffed ? gfx::TextStyle{m_buffedSpot.color(), gfx::TextEffect::Plain, std::nullopt} : text,
                200,
                20
            );
            if (m_spending && !pet) {
                const bool arrows = pointsLeft() > 0;
                // x of the pending points: 164 beside the arrows, else 156
                canvas.drawText({at.x + (arrows ? 164 : 156), y}, (arrows ? "" : "+") + std::to_string(m_pending[i]), text, 200, 20);
            }
        }
        if (pointsLeft() > 0 && !pet) {
            if (const auto button = m_services.pictures.find("Btn_Point_1")) {
                const gfx::Picture& picture = m_services.pictures.picture(*button);
                // the button picture holds 3 frames stacked; the blink cycles through them
                const int frameHeight = picture.height() / 3;
                const int blink = static_cast<int>((nowMs / kPointBlinkMs) % 3);
                // point button position (123, 319)
                canvas.drawPicture(*button, {at.x + 123, at.y + 319}, core::Rect{0, frameHeight * blink, picture.width(), frameHeight});
            }
        }
        if (!pet || pet->get().info.mercenary == 0) {
            // potential number at (172, 90); 20 max text width, 10 line height
            canvas.drawText({at.x + 172, at.y + 90}, std::to_string(shown.potential), text, 20, 10);
        }
    }

    void ItemWindow::useBagItem(int slot) {
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        const auto record = m_services.items.find(item.itemId);
        if (item.empty() || !record) {
            return;
        }
        if (m_autoCombat && m_autoCombat() && m_inBattle && m_inBattle()) {
            m_messages.show("Unable to operate during automatic combat control", 2000); // message display time in ms
            return;
        }
        if (m_useSpecial && m_useSpecial(item.itemId, slot, m_shownOrder)) {
            return;
        }
        if (record->equipSlot == 0) {
            m_messages.show("This item cannot be used", 2000); // message display time in ms
            return;
        }
        const bool fighting = m_inBattle && m_inBattle();
        constexpr int kNotInCombat = 0x100; // item flag bit 0x100: cannot be used in combat
        if (fighting && (record->flags & kNotInCombat) != 0) {
            m_messages.show("Unable to use in combat", 1000); // message display time in ms
            return;
        }
        if (isEquipment(*record)) {
            equipFromBag(slot);
            return;
        }
        constexpr int kVehicleType = 39; // item type 39: vehicles (Airship, Boat, Cabriolet, Hot-air Balloon, ...)
        if (record->type == kVehicleType && m_useVehicle) {
            m_useVehicle(slot);
            return;
        }
        if (StatBuffItems::code(item.itemId) != 0) {
            const std::string refusal = StatBuffItems::refusal(item.itemId, fighting, m_buffMask ? m_buffMask() : 0);
            if (!refusal.empty()) {
                m_messages.show(refusal, 1000); // message display time in ms
                return;
            }
            m_services.connection.send(protocol::strengthBuffUseRequest(slot));
            return;
        }
        constexpr int kNotebook = 100; // item sub type 100: notebook (opens the Notebook window)
        if (record->subType == kNotebook) {
            if (!fighting && m_openNotebook) {
                m_openNotebook();
                close();
            }
            return;
        }
        if (fighting) {
            if (m_chooseBattleItem) {
                m_chooseBattleItem(item.itemId, slot, item.count);
            }
            close();
            return;
        }
        requestUse(slot, m_shownOrder, fighting);
    }

    void ItemWindow::requestUse(int slot, int order, bool fighting, bool remote) {
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        const auto record = m_services.items.find(item.itemId);
        if (item.empty() || !record) {
            return;
        }
        const int place = order == 0 ? 0 : m_pets.placeByOrder(order);
        std::optional<std::reference_wrapper<const Pet>> pet;
        if (place != 0 && m_pets.at(place)) {
            pet = std::cref(*m_pets.at(place));
        }
        UseTarget target;
        target.pet = pet.has_value();
        target.hp = pet ? pet->get().hp : m_status.hp;
        target.maxHp = pet ? pet->get().maxHp : m_status.maxHp;
        target.sp = pet ? pet->get().sp : m_status.sp;
        target.maxSp = pet ? pet->get().maxSp : m_status.maxSp;
        target.exp = pet ? pet->get().exp : m_status.exp;
        target.reborn = pet ? pet->get().reborn : m_status.reborn;
        if (pet) {
            target.tradable = pet->get().tradable;
            target.mercenary = pet->get().info.mercenary != 0;
        }
        if (const UseRefusal refusal = useRefusal(*record, target, fighting); !refusal.text.empty()) {
            m_messages.show(refusal.text, refusal.ms);
            return;
        }
        m_services.connection.send(remote ? protocol::remoteUseItemRequest(slot, 1, place, 0) : protocol::useItemRequest(slot, 1, place, 0));
    }

    void ItemWindow::rightClickBagItem(int slot) {
        const protocol::ItemInstance& item = m_inventory.bagItem(slot);
        if (!item.empty() && item.count > 1 && m_services.videoSettings().modernBag && m_marks.vehicleState(slot) == 0) {
            // the modern way: the bigger half of the stack is taken up and follows the mouse until a click puts it down
            m_screen.startStickyDrag("ItemBag", slot, (item.count + 1) / 2);
            return;
        }
        if (!item.empty() && m_rightClicked) {
            m_rightClicked(item.itemId, slot);
        }
    }

    void ItemWindow::wearBagItem(int slot) {
        if (shownPet()) {
            m_messages.show("Unable to use", 1500); // message display time in ms
        } else {
            equipFromBag(slot);
        }
    }

    void ItemWindow::equipFromBag(int slot) {
        const auto record = m_services.items.find(m_inventory.bagItem(slot).itemId);
        if (!record) {
            return;
        }
        if (const auto pet = shownPet()) {
            const bool inStall = m_petInStall && m_petInStall(shownPlace());
            if (const auto refusal = petEquipRefusal(*record, pet->get(), m_services.npcs, inStall)) {
                if (!refusal->empty()) {
                    m_messages.show(*refusal, 2000); // message display time in ms
                }
                return;
            }
            m_equipSnapshot = std::make_pair(shownPlace(), EquipBonusSnapshot::of(pet->get()));
            m_services.connection.send(protocol::petEquipRequest(shownPlace(), slot));
            return;
        }
        if (const auto refusal = equipRefusal(*record, m_status.level, m_player.look.bodyType, m_status.element)) {
            m_messages.show(*refusal, 2000); // message display time in ms
            return;
        }
        if (isEquipment(*record)) {
            m_services.connection.send(protocol::equipRequest(slot));
        }
    }

    std::optional<int> ItemWindow::draggedBagItem() const {
        const auto& drag = m_screen.context().drag;
        if (!drag || drag->sourceId != m_bag.id()) {
            return std::nullopt;
        }
        const protocol::ItemInstance& item = m_inventory.bagItem(drag->sourceIndex);
        return item.empty() ? std::nullopt : std::optional<int>(item.itemId);
    }

    void ItemWindow::dropped(const ui::EventArgs& args) {
        const bool fromBag = args.source == "ItemBag";
        const bool toBag = args.target == "ItemBag" || args.target == "BankItemBag";
        const bool modern = m_services.videoSettings().modernBag;
        if ((args.partial || args.carried > 0) && !(modern && fromBag && (args.target == "ItemBag" || args.target == "StoreroomCells"))) {
            return; // one of a stack, or a part taken up, goes only from place to place in the bag, or into the Storeroom
        }
        const bool toWorn = args.target == "ItemHead" || args.target == "ItemBody" || args.target == "ItemWeapon" || args.target == "ItemWrist" ||
                            args.target == "ItemFeet" || args.target == "ItemSpecial";
        const bool toWorld = args.target.empty() || args.target == "WorldView";
        constexpr const char* kHotKeys = "HotKeySlots";
        if (args.target == "StoreTank" || args.target == "StoreTankCells") {
            constexpr int kTankColumns = 5; // cells per Fuel Bank row (cell index to row)
            if (fromBag && m_dropOnTank) {
                m_dropOnTank(args.index, args.target == "StoreTankCells" && args.targetIndex > 0 ? (args.targetIndex - 1) / kTankColumns + 1 : 0);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (const auto drop = m_bagDrops.find(args.target); drop != m_bagDrops.end()) {
            if (fromBag) {
                drop->second(args.index);
            } else {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "VehicleRepairSpace") {
            if (fromBag && m_dropOnRepair) {
                m_dropOnRepair(args.index);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "ItemBagCells") {
            if (fromBag && m_dropOnItemBag) {
                m_dropOnItemBag(args);
            }
            return;
        }
        if (args.target == "StoreroomCells") {
            if (fromBag && m_dropOnStoreroom) {
                m_dropOnStoreroom(args);
            }
            return;
        }
        if (args.target == "BankItemBank") {
            if (fromBag && m_dropOnBank) {
                m_dropOnBank(args);
            }
            return;
        }
        if (args.target == "EggSellCells") {
            if (fromBag && m_dropOnEgg && args.targetIndex > 0) {
                m_dropOnEgg(args.index, args.targetIndex);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "CupboardFiveCells" || args.target == "CupboardTenCells") {
            if (fromBag && m_dropOnCupboard && args.targetIndex > 0) {
                m_dropOnCupboard(args.index, args.targetIndex);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "IceBoxCells" || args.target == "PledgeBoxCells") {
            if (fromBag && m_dropOnHouseBox && args.targetIndex > 0) {
                m_dropOnHouseBox(args.target == "PledgeBoxCells", args.index, args.targetIndex);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "BookCaseCells" || args.target == "BookDeskCells") {
            if (fromBag && m_dropOnBookShelf && args.targetIndex > 0) {
                m_dropOnBookShelf(args.target == "BookDeskCells", args.index, args.targetIndex);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "StallCaseCells") {
            if (fromBag && m_dropOnCase) {
                m_dropOnCase(args.index);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == "VendingList" || args.target == "VendingDrop") {
            if (fromBag && m_dropOnVending) {
                m_dropOnVending(args.index);
            } else if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            }
            return;
        }
        if (args.target == kHotKeys) {
            if (!fromBag) {
                m_messages.show("This function only sustains the items carried with", 1500); // message display time in ms
            } else if (const protocol::ItemInstance& item = m_inventory.bagItem(args.index); !item.empty() && m_putOnBar) {
                m_putOnBar(args.targetIndex, item.itemId);
            }
            return;
        }
        if (fromBag) {
            const protocol::ItemInstance& item = m_inventory.bagItem(args.index);
            if (item.empty()) {
                return;
            }
            if (toBag) {
                const int target = args.targetIndex;
                const protocol::ItemInstance& there = target > 0 ? m_inventory.bagItem(target) : protocol::ItemInstance{};
                if (m_marks.vehicleState(args.index) != 0) {
                    return;
                }
                if (modern && args.target == "ItemBag" && !args.ctrl && target > 0 && target != args.index && m_requests) {
                    // the modern way: one put down of what is carried, the part taken up, or else the whole stack; what does not fit
                    // stays carried, and a whole stack on another thing swaps with it and carries that on
                    const int want = args.partial ? 1 : args.carried > 0 ? std::min(args.carried, item.count) : item.count;
                    const int fits = there.empty() || there.itemId == item.itemId ? m_inventory.fitCount(target, item.itemId, want, args.index) : 0;
                    if (fits > 0) {
                        const int sent = m_requests->get().moveItem(args.index, fits, target) ? fits : 0;
                        m_screen.placed(sent); // shown in the target at once, before the server answers
                        if (!args.partial && sent < want) {
                            m_screen.keepCarrying(want - sent);
                        }
                    } else if (!args.partial && want == item.count && !there.empty() && there.itemId != item.itemId &&
                               m_requests->get().swapItems(args.index, target)) {
                        m_screen.keepCarrying(0); // the other thing is carried on from where the stack was
                    } else if (!args.partial) {
                        m_screen.keepCarrying(want);
                    }
                    return;
                }
                if (target > 0 && target != args.index && (there.empty() || there.itemId == item.itemId) && m_requests) {
                    if (args.ctrl) {
                        m_requests->get().askMoveCount(args.index, target);
                    } else {
                        m_requests->get().moveItem(args.index, 1, target);
                    }
                }
            } else if (toWorn) {
                equipFromBag(args.index);
            } else if (args.target.empty() || args.target == "WorldView") {
                if ((!m_dropOnWorld || !m_dropOnWorld(args.index, item.itemId, 1)) && m_requests) {
                    m_requests->get().dropOnGround(args.index, args.ctrl);
                }
            }
            return;
        }
        if (toWorld && m_requests && !shownEquipped(args.index).empty()) {
            m_requests->get().askDiscardEquip(args.index, shownPlace());
            return;
        }
        if (toBag && args.targetIndex > 0 && !shownEquipped(args.index).empty()) {
            requestUnequip(args.index, args.targetIndex);
        }
    }

    void ItemWindow::unequip(int equipSlot) {
        const protocol::ItemInstance& item = shownEquipped(equipSlot);
        if (item.empty()) {
            return;
        }
        if (const int freeSlot = m_inventory.freeSlotFor(item.itemId)) {
            requestUnequip(equipSlot, freeSlot);
        }
    }

    void ItemWindow::unequipFor(int order, int equipSlot, int bagSlot) {
        m_shownOrder = order;
        requestUnequip(equipSlot, bagSlot);
    }

    void ItemWindow::requestUnequip(int equipSlot, int bagSlot) {
        if (const auto pet = shownPet()) {
            const bool inStall = m_petInStall && m_petInStall(shownPlace());
            if (const auto refusal = petUnequipRefusal(pet->get(), inStall)) {
                m_messages.show(*refusal, 2000); // message display time in ms
                return;
            }
            m_equipSnapshot = std::make_pair(shownPlace(), EquipBonusSnapshot::of(pet->get()));
            m_services.connection.send(protocol::petUnequipRequest(shownPlace(), equipSlot, bagSlot));
            return;
        }
        m_services.connection.send(protocol::unequipRequest(equipSlot, bagSlot));
    }

    std::string ItemWindow::equipChangeLines(int place) {
        std::string lines;
        if (m_equipSnapshot && m_equipSnapshot->first == place) {
            if (const auto& pet = m_pets.at(place)) {
                lines = m_equipSnapshot->second.changeLines(*pet);
            }
            m_equipSnapshot.reset();
        }
        return lines;
    }

    void ItemWindow::updateSort(std::uint64_t nowMs) {
        const bool sortShown = m_services.videoSettings().sortButtons;
        m_screen.get<ui::Element>("ItemSort").setVisible(sortShown);
        m_sortLocks.update(sortShown, isOpen());
        if (!isOpen()) {
            m_sorter.stop();
            return;
        }
        if (!m_sorter.isRunning()) {
            return;
        }
        if (const auto said = m_sorter.step(sortCellsOf(m_inventory, m_services.items, m_sortLocks.places()), nowMs, [this](const SortMove& move) {
                m_services.connection.send(protocol::moveItemRequest(move.from, move.count, move.to));
            })) {
            m_messages.show(*said);
        }
    }

}
