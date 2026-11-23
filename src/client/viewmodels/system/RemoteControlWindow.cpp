#include "client/viewmodels/system/RemoteControlWindow.h"

#include "client/rules/FurnitureKinds.h"
#include "client/rules/ItemInfo.h"
#include "core/FileSystem.h"
#include "core/Log.h"
#include "gfx/TextRenderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace client {

    namespace {

        constexpr int kRemoteControl = 34058; // item 34058: Remote Control
        constexpr int kMinLevel = 10; // lowest player level that may use the keep-item function
        constexpr int kTicks = RemoteSettings::kTicks;
        constexpr int kMessageMs = 2000; // message time, ms
        constexpr int kVehicleType = 39; // item type 39: vehicles (Airship, Boat, Cabriolet, Hot-air Balloon, ...)
        constexpr int kTentSubType = 104; // item sub type 104: tents (original IsTentItem)
        constexpr int kInstrumentType = FurnitureKinds::kInstrumentType;
        constexpr int kFurnitureType = FurnitureKinds::kFurnitureType;
        constexpr int kHpSpMaxSubType = 123; // item sub type 123: restores HP and SP fully (hint " HP/SP: Max")
        constexpr int kFlagA = 1; // item flag bit 0x01: asks for confirmation before discarding
        constexpr int kFlagB = 2; // item flag bit 0x02: cannot be transferred
        constexpr int kFlagC = 128; // item flag bit 0x80: cannot be discarded
        constexpr int kItemCell = 30; // width and height of a kept-item icon cell, px
        constexpr int kThumbHeight = 12; // height of one thumb state in the thumb picture, px (states stacked vertically)
        constexpr int kThumbCap = 8; // width of the thumb's left and right caps, px
        constexpr int kThumbWidth = 15; // width of the thumb picture, px
        constexpr std::array<int, 4> kRailY = {162, 184, 228, 250}; // y of the rails of the 4 sliders (player HP, SP, pet HP, SP), px
        constexpr std::array<int, 4> kPercentY = {157, 179, 223, 245}; // y of the percent texts of the 4 sliders, px
        constexpr int kRailX = 106; // x of the slider rails, px
        constexpr int kRailHintHeight = 6; // height of the rail area that shows the amount hint, px
        constexpr int kPercentRight = 211; // right edge x of the percent texts, px
        constexpr std::uint16_t kTextColor = 2113; // RGB565 0x0841: near black

        /// @brief Loads RemoteControlWindow.xml into the screen.
        /// @param services Shared game services (layout path and load context).
        /// @param screen Screen the layout is loaded into.
        /// @return The RemoteControlWindow window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("RemoteControlWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("RemoteControlWindow");
        }

        /// @brief Collects the four slider drawing areas.
        /// @param screen Screen holding the controls.
        /// @return RemoteSlider1..4.
        std::array<std::reference_wrapper<ui::DrawingArea>, 4> sliderAreas(ui::Screen& screen) {
            return {
                screen.get<ui::DrawingArea>("RemoteSlider1"),
                screen.get<ui::DrawingArea>("RemoteSlider2"),
                screen.get<ui::DrawingArea>("RemoteSlider3"),
                screen.get<ui::DrawingArea>("RemoteSlider4")
            };
        }

        /// @brief Collects the drawing areas of the items-to-keep cells.
        /// @param screen Screen holding the controls.
        /// @return RemoteItem1..5.
        std::array<std::reference_wrapper<ui::DrawingArea>, RemoteSettings::kKeepItems> keptAreas(ui::Screen& screen) {
            return {
                screen.get<ui::DrawingArea>("RemoteItem1"),
                screen.get<ui::DrawingArea>("RemoteItem2"),
                screen.get<ui::DrawingArea>("RemoteItem3"),
                screen.get<ui::DrawingArea>("RemoteItem4"),
                screen.get<ui::DrawingArea>("RemoteItem5")
            };
        }

        /// @brief Collects the tick check boxes.
        /// @param screen Screen holding the controls.
        /// @return RemoteCheck1..11.
        std::array<std::reference_wrapper<ui::CheckBox>, kTicks> checks(ui::Screen& screen) {
            return {
                screen.get<ui::CheckBox>("RemoteCheck1"),
                screen.get<ui::CheckBox>("RemoteCheck2"),
                screen.get<ui::CheckBox>("RemoteCheck3"),
                screen.get<ui::CheckBox>("RemoteCheck4"),
                screen.get<ui::CheckBox>("RemoteCheck5"),
                screen.get<ui::CheckBox>("RemoteCheck6"),
                screen.get<ui::CheckBox>("RemoteCheck7"),
                screen.get<ui::CheckBox>("RemoteCheck8"),
                screen.get<ui::CheckBox>("RemoteCheck9"),
                screen.get<ui::CheckBox>("RemoteCheck10"),
                screen.get<ui::CheckBox>("RemoteCheck11")
            };
        }

        /// @brief Reads the number a control name ends in (RemoteSlider3 gives 3).
        /// @param name Control name.
        /// @param prefix Leading part of the name to skip.
        /// @return The trailing number.
        int numberAfter(const std::string& name, const std::string& prefix) { return std::stoi(name.substr(prefix.size())); }

        /// @brief Names an item attribute kind for the kept-item hint.
        /// @details Original: the attribute names of SetAutoPlayItem's hint.
        /// @param kind Attribute kind of the item record.
        /// @param skip Set true when the kind is to be left out of the hint.
        /// @return The display name, or an empty string for an unknown kind.
        std::string attributeName(int kind, bool& skip) {
            skip = false;
            switch (kind) {
                case 25: return "HP"; // attribute kind 25: HP
                case 26: return "SP"; // attribute kind 26: SP
                case 207: return "MaxHp"; // attribute kind 207: max HP
                case 208: return "MaxSp"; // attribute kind 208: max SP
                case 210: return "ATK"; // attribute kind 210: attack
                case 211: return "DEF"; // attribute kind 211: defence
                case 215: return "MATK"; // attribute kind 215: magic attack
                case 216: return "MDEF"; // attribute kind 216: magic defence
                case 214: return "SPD"; // attribute kind 214: speed
                case 64: return "Amity Degree"; // attribute kind 64: pet amity
                case 100: return "resistance of shoot"; // attribute kind 100: shoot resistance
                case 250: skip = true; return {}; // attribute kind 250: left out of the hint
                default: return {}; // any other kind: no name
            }
        }

    }

    RemoteControlWindow::RemoteControlWindow(
        GameServices& services,
        ui::Screen& screen,
        RemoteControl& remote,
        RemotePlusWindow& plus,
        const Inventory& inventory,
        const PetTeam& pets,
        const PlayerStatus& status,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_screen(screen),
          m_remote(remote),
          m_plus(plus),
          m_inventory(inventory),
          m_pets(pets),
          m_status(status),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_window(loadWindow(services, screen)),
          m_sliderAreas(sliderAreas(screen)),
          m_keptAreas(keptAreas(screen)),
          m_checks(checks(screen)),
          m_from(screen.get<ui::TextBox>("RemoteFrom")),
          m_to(screen.get<ui::TextBox>("RemoteTo")),
          m_deathLimit(screen.get<ui::TextBox>("RemoteDeathLimit")),
          m_petDeathLimit(screen.get<ui::TextBox>("RemotePetDeathLimit")),
          m_leaveMinutes(screen.get<ui::TextBox>("RemoteLeaveMinutes")),
          m_plusSet(screen.get<ui::ImageButton>("RemotePlusSet")),
          m_deliverSet(screen.get<ui::ImageButton>("RemoteDeliverSet")),
          m_rail(services.pictures.find("Panel41").value_or(gfx::PictureId{})),
          m_railFill(services.pictures.find("Panel40").value_or(gfx::PictureId{})),
          m_thumb(services.pictures.find("bar_w4").value_or(gfx::PictureId{})),
          m_base(services.pictures.find("Icon_FormBase_1").value_or(gfx::PictureId{})) {
        setNumber(m_from, 1);
        setNumber(m_to, 50); // last bag place
        setNumber(m_deathLimit, 10); // default death limit
        setNumber(m_petDeathLimit, 10); // default pet death limit
        setNumber(m_leaveMinutes, 120); // default minutes before leaving
        m_ticks.fill(true);
        m_ticks[7] = false; // tick 7 starts off
        for (PercentSlider& slider : m_sliders) {
            slider.setPosition(50); // default percent
        }
        screen.get<ui::DrawingArea>("RemotePage").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintPage(canvas, at); });
        screen.get<ui::DrawingArea>("RemoteHints").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintHints(canvas, at); });
        for (std::size_t slider = 0; slider < m_sliderAreas.size(); ++slider) {
            m_sliderAreas[slider].get().setPainter([this, slider](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
                paintThumb(canvas, at, slider, nowMs);
            });
        }
        for (int cell = 1; cell <= RemoteSettings::kKeepItems; ++cell) {
            m_keptAreas[static_cast<std::size_t>(cell - 1)].get().setPainter([this, cell](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                paintKept(canvas, at, cell);
            });
        }
        screen.on("onRemoteClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onRemoteOk", [this](const ui::EventArgs&) { startRemote(); });
        screen.on("onRemotePlus", [this](const ui::EventArgs&) {
            if (m_inventory.bagItem(m_remote.itemSlot()).itemId != kRemoteControl) {
                m_plus.toggle();
            }
        });
        screen.on("onRemoteDeliver", [this](const ui::EventArgs&) {
            if (m_remote.isElectron()) {
                m_hooks.openDeliver();
            }
        });
        screen.on("onRemoteStop", [this](const ui::EventArgs&) { m_remote.stop(); });
        screen.on("onRemoteCheck", [this](const ui::EventArgs& args) { toggle(numberAfter(args.source, "RemoteCheck")); });
        screen.on("onRemoteSliderDown", [this](const ui::EventArgs& args) {
            const auto slider = static_cast<std::size_t>(numberAfter(args.source, "RemoteSlider") - 1);
            m_sliders[slider].press(args.position.x);
            m_held[slider] = true;
        });
        screen.on("onRemoteSliderDrag", [this](const ui::EventArgs& args) {
            const auto slider = static_cast<std::size_t>(numberAfter(args.source, "RemoteSlider") - 1);
            m_sliders[slider].drag(args.position.x);
        });
        screen.on("onRemoteSliderUp", [this](const ui::EventArgs& args) {
            const auto slider = static_cast<std::size_t>(numberAfter(args.source, "RemoteSlider") - 1);
            m_sliders[slider].release();
            m_held[slider] = false;
        });
        screen.on("onRemoteItemClear", [this](const ui::EventArgs& args) { clearKept(numberAfter(args.source, "RemoteItem")); });
    }

    int RemoteControlWindow::numberIn(const ui::TextBox& box) {
        try {
            return std::stoi(box.text());
        } catch (const std::exception&) {
            return 0;
        }
    }

    void RemoteControlWindow::setNumber(ui::TextBox& box, int value) { box.setText(std::to_string(value)); }

    void RemoteControlWindow::resetDefaults() {
        RemoteSettings& settings = m_remote.settings();
        settings = RemoteSettings::defaults();
        settings.deathLimit = 3; // reset death limit
        settings.petDeathLimit = 3; // reset pet death limit
        for (PercentSlider& slider : m_sliders) {
            slider.setPosition(50); // reset percent (half)
        }
        for (int cell = 1; cell <= RemoteSettings::kKeepItems; ++cell) {
            clearKept(cell);
        }
        // play sets 1 and 2 are the two sets of the Plus form (set 0 is unused)
        m_remote.playSets()[1] = AutoBattleSettings::defaults();
        m_remote.playSets()[2] = AutoBattleSettings::defaults();
    }

    void RemoteControlWindow::loadFile() {
        resetDefaults();
        m_plus.loadFromPlayer();
        const std::filesystem::path path = RemoteFile::pathFor(m_services.game.userRoot(), m_hooks.playerId());
        const auto file = RemoteFile::read(path);
        if (!file) {
            saveFile();
            m_plus.save();
            return;
        }
        if (file->version != RemoteFile::kVersion && file->version != RemoteFile::kOlderVersion) {
            if (RemoteFile::kVersion > file->version) {
                m_versionUpdated = true;
            }
            std::error_code error;
            std::filesystem::remove(path, error);
            return;
        }
        RemoteSettings settings = file->settings;
        if (file->version != RemoteFile::kVersion) {
            settings.ticks[kTicks] = true;
            const auto& pet = m_pets.at(m_pets.activePlace());
            const auto percent = [](int amount, int most) {
                // old files stored absolute amounts; convert to a percentage of the maximum (capped at 100)
                return most <= amount ? 100 : static_cast<int>(std::ceil(static_cast<double>(amount) / most * 100));
            };
            settings.hpPercent = percent(settings.hpPercent, m_status.maxHp);
            settings.spPercent = percent(settings.spPercent, m_status.maxSp);
            settings.petHpPercent = percent(settings.petHpPercent, pet ? pet->maxHp : 0);
            settings.petSpPercent = percent(settings.petSpPercent, pet ? pet->maxSp : 0);
        }
        m_remote.settings() = settings;
        for (std::size_t set = 1; set <= 2; ++set) { // the two Plus sets
            m_plus.formSets()[set] = file->plusSets[set];
        }
        setNumber(m_from, settings.itemFrom);
        setNumber(m_to, settings.itemTo);
        for (int cell = 1; cell <= RemoteSettings::kKeepItems; ++cell) {
            m_kept[static_cast<std::size_t>(cell)] = settings.keepItems[static_cast<std::size_t>(cell)];
        }
    }

    void RemoteControlWindow::saveFile() {
        RemoteFile file;
        file.settings = m_remote.settings();
        file.plusSets = m_plus.formSets();
        if (!file.write(RemoteFile::pathFor(m_services.game.userRoot(), m_hooks.playerId()))) {
            core::logWarning("cannot write the Remote Control's settings");
        }
    }

    void RemoteControlWindow::setCheck(int tick, bool checked) {
        m_ticks[static_cast<std::size_t>(tick)] = checked;
        m_checks[static_cast<std::size_t>(tick - 1)].get().setChecked(checked);
    }

    void RemoteControlWindow::open() {
        if (m_reload) {
            loadFile();
            m_reload = false;
        }
        const RemoteSettings& settings = m_remote.settings();
        for (int tick = 1; tick <= kTicks; ++tick) {
            setCheck(tick, settings.ticks[static_cast<std::size_t>(tick)]);
        }
        setNumber(m_deathLimit, settings.deathLimit);
        setNumber(m_petDeathLimit, settings.petDeathLimit);
        setNumber(m_leaveMinutes, settings.leaveMinutes);
        m_sliders[0].setPosition(settings.hpPercent);
        m_sliders[1].setPosition(settings.spPercent);
        m_sliders[2].setPosition(settings.petHpPercent);
        m_sliders[3].setPosition(settings.petSpPercent);
        // sliders 0..3: player HP, player SP, pet HP, pet SP
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
        for (int cell = 1; cell <= RemoteSettings::kKeepItems; ++cell) {
            setKeptItem(cell, m_kept[static_cast<std::size_t>(cell)]);
        }
        if (m_versionUpdated) {
            m_hooks.systemMessage("(System Prompt): Remote control version updated");
            m_versionUpdated = false;
        }
    }

    void RemoteControlWindow::close() { m_window.setVisible(false); }

    void RemoteControlWindow::update() {
        if (!m_window.isVisible()) {
            return;
        }
        const int itemId = m_inventory.bagItem(m_remote.itemSlot()).itemId;
        if (itemId == 0) {
            close();
            return;
        }
        m_plusSet.setVisible(itemId != kRemoteControl);
        m_deliverSet.setVisible(m_remote.isElectron());
    }

    void RemoteControlWindow::toggle(int tick) {
        const bool now = !m_ticks[static_cast<std::size_t>(tick)];
        m_ticks[static_cast<std::size_t>(tick)] = now;
        if (now) {
            // tick 3 is the supply tick, tick 4 is Leave when no supply is left
            if (m_ticks[4] && !m_ticks[3]) {
                setCheck(3, true);
            }
        } else if (!m_ticks[3] && m_ticks[4]) {
            setCheck(4, false);
        }
        setCheck(tick, now);
    }

    void RemoteControlWindow::setKeptItem(int cell, int itemId) {
        if (cell < 1 || cell > RemoteSettings::kKeepItems || itemId == 0) {
            return;
        }
        if (m_hooks.level() < kMinLevel) {
            m_messages.show("You can't use the function, because your level is less than 10", kMessageMs);
            return;
        }
        if (!m_hooks.enoughOnlineTime()) {
            m_messages.show("Can't use the function, because your online time is not enough.", kMessageMs);
            return;
        }
        const auto record = m_services.items.find(itemId);
        if (!record) {
            return;
        }
        const bool unusable = record->type == kInstrumentType || record->type == kFurnitureType || FurnitureKinds::isLargeItem(m_services.items, itemId) ||
                              record->type == kVehicleType || (record->flags & kFlagA) != 0 || (record->flags & kFlagB) != 0 || (record->flags & kFlagC) != 0 ||
                              record->subType == kTentSubType || FurnitureKinds::isFloorItem(m_services.items, itemId) ||
                              FurnitureKinds::isWallpaperItem(m_services.items, itemId) || itemId == kRemoteControl;
        if (unusable) {
            m_messages.show("This function is unable to set this item", kMessageMs);
            return;
        }
        std::string hint;
        if (bottleKind(*record) == 0) {
            for (std::size_t index = 0; index < 2; ++index) { // the item's two attributes
                if (record->subType == kHpSpMaxSubType) {
                    hint = " HP/SP: Max";
                    break;
                }
                bool skip = false;
                const std::string name = attributeName(record->attributeKinds[index], skip);
                if (skip) {
                    continue;
                }
                if (index == 1 && !name.empty()) {
                    hint += ",";
                }
                const int value = record->attributeValues[index];
                // 100 is the neutral attribute value; above it is a bonus, below a penalty
                if (value == 0 || value == 100) {
                    continue;
                }
                hint += value - 100 > 0 ? " " + name + "+" + std::to_string(value - 100) : " " + name + "-" + std::to_string(100 - value);
            }
        }
        hint = record->name + hint;
        for (int other = 1; other <= RemoteSettings::kKeepItems; ++other) {
            if (m_shown[static_cast<std::size_t>(other)] == itemId) {
                clearKept(other);
            }
        }
        m_kept[static_cast<std::size_t>(cell)] = itemId;
        m_shown[static_cast<std::size_t>(cell)] = itemId;
        m_keptAreas[static_cast<std::size_t>(cell - 1)].get().setHint(hint);
    }

    void RemoteControlWindow::clearKept(int cell) {
        if (cell < 1 || cell > RemoteSettings::kKeepItems) {
            return;
        }
        m_kept[static_cast<std::size_t>(cell)] = 0;
        m_shown[static_cast<std::size_t>(cell)] = 0;
        m_keptAreas[static_cast<std::size_t>(cell - 1)].get().setHint({});
    }

    void RemoteControlWindow::startRemote() {
        if (m_hooks.onVehicle()) {
            m_messages.show("You are using a vehicle", 1500); // message time in ms
            return;
        }
        RemoteSettings& settings = m_remote.settings();
        for (ui::TextBox& box : std::initializer_list<std::reference_wrapper<ui::TextBox>>{m_deathLimit, m_petDeathLimit, m_leaveMinutes, m_from, m_to}) {
            if (box.text().empty()) {
                setNumber(box, 0);
            }
        }
        // tick 3 is the supply tick, tick 4 is Leave when no supply is left
        if (!m_ticks[3]) {
            m_ticks[4] = false;
        }
        for (int tick = 1; tick <= kTicks; ++tick) {
            settings.ticks[static_cast<std::size_t>(tick)] = m_ticks[static_cast<std::size_t>(tick)];
        }
        settings.hpPercent = m_sliders[0].position();
        settings.spPercent = m_sliders[1].position();
        settings.petHpPercent = m_sliders[2].position();
        settings.petSpPercent = m_sliders[3].position();
        const auto limited = [](ui::TextBox& box) {
            int value = numberIn(box);
            // 40000 is the largest value the three boxes take
            if (value > 40000) {
                setNumber(box, 40000);
                value = 40000;
            }
            return value;
        };
        const int deaths = numberIn(m_deathLimit);
        const int petDeaths = numberIn(m_petDeathLimit);
        const int minutes = numberIn(m_leaveMinutes);
        // ticks 5, 6 and 8 belong to the death limit, pet death limit and leave-minutes boxes
        if (deaths == 0) {
            m_ticks[5] = false;
            settings.ticks[5] = false;
            setCheck(5, false);
        }
        if (petDeaths == 0) {
            m_ticks[6] = false;
            settings.ticks[6] = false;
            setCheck(6, false);
        }
        if (minutes == 0) {
            m_ticks[8] = false;
            settings.ticks[8] = false;
            setCheck(8, false);
        }
        settings.deathLimit = limited(m_deathLimit);
        settings.petDeathLimit = limited(m_petDeathLimit);
        settings.leaveMinutes = limited(m_leaveMinutes);
        const int firstBefore = numberIn(m_from);
        if (firstBefore <= 0) {
            setNumber(m_from, 1);
        }
        // 50 is the last bag place
        if (firstBefore > 50) {
            setNumber(m_from, 50);
        }
        settings.itemFrom = numberIn(m_from);
        const int lastBefore = numberIn(m_to);
        // 50 is the last bag place
        if (lastBefore <= 0) {
            setNumber(m_to, 50);
        }
        if (lastBefore > 50) {
            setNumber(m_to, 50);
        }
        if (settings.itemFrom > lastBefore) {
            setNumber(m_to, numberIn(m_from));
        }
        settings.itemTo = numberIn(m_to);
        settings.keepItems = m_kept;
        m_hooks.starting();
        m_remote.start(settings.ticks[2]); // tick 2 is passed to start
        m_plus.save();
        saveFile();
        if (m_inventory.bagItem(m_remote.itemSlot()).itemId == kRemoteControl) {
            // plain Remote Control has no Plus form: play sets 1 and 2 go back to defaults
            m_remote.playSets()[1] = AutoBattleSettings::defaults();
            m_remote.playSets()[2] = AutoBattleSettings::defaults();
        }
        close();
    }

    void RemoteControlWindow::paintPage(gfx::Canvas& canvas, core::Point at) {
        const gfx::Picture& fill = canvas.pictures().picture(m_railFill);
        for (std::size_t slider = 0; slider < m_sliders.size(); ++slider) {
            const int percent = m_sliders[slider].position();
            canvas.drawPicture(m_rail, {at.x + kRailX, at.y + kRailY[slider]});
            const int width = static_cast<int>(std::trunc(fill.width() * (percent / 100.0))); // percent to fraction
            if (width > 0) {
                canvas.drawPicture(m_railFill, {at.x + kRailX + 1, at.y + kRailY[slider] + 1}, core::Rect{0, 0, width, fill.height()});
            }
            const std::string text = std::to_string(percent) + "%";
            const int textWidth = gfx::TextRenderer::textWidth(text, gfx::TextEffect::Plain);
            canvas.drawText(
                {at.x + kPercentRight - textWidth, at.y + kPercentY[slider]},
                text,
                {core::Color::fromRgb565(kTextColor), gfx::TextEffect::Plain, std::nullopt},
                200, // text area width, px
                12 // text area height, px
            ); // text box width and height
        }
        if (m_inventory.bagItem(m_remote.itemSlot()).itemId == kRemoteControl && m_base.isValid()) {
            // four pixel offsets of the Plus button's corners, painted over with the form colour
            for (const core::Point corner : {core::Point{115, 45}, {115, 47}, {125, 45}, {125, 47}}) { // x, y in px
                canvas.drawPicture(m_base, {at.x + corner.x, at.y + corner.y});
            }
        }
    }

    void RemoteControlWindow::paintThumb(gfx::Canvas& canvas, core::Point at, std::size_t slider, std::uint64_t nowMs) {
        PercentSlider& bar = m_sliders[slider];
        const core::Point mouse = m_screen.mouse();
        // 10 is the slider area's height in pixels
        const core::Rect barRect{at.x, at.y, PercentSlider::kWidth, 10};
        const bool onThumb = core::Rect{at.x + bar.thumbPos(), at.y, PercentSlider::kThumbSize, 10}.contains(mouse);
        bar.update(nowMs, m_held[slider] && barRect.contains(mouse), onThumb);
        if (!m_thumb.isValid()) {
            return;
        }
        const int state = bar.thumbState(onThumb);
        const int y = at.y - 3; // thumb sits 3 pixels above the rail's top
        canvas.drawPicture(m_thumb, {at.x + bar.thumbPos(), y}, core::Rect{0, state * kThumbHeight, kThumbCap, kThumbHeight});
        canvas.drawPicture(
            m_thumb,
            {at.x + bar.thumbPos() + PercentSlider::kThumbSize - kThumbCap, y},
            core::Rect{kThumbWidth - kThumbCap, state * kThumbHeight, kThumbCap, kThumbHeight}
        );
    }

    void RemoteControlWindow::paintKept(gfx::Canvas& canvas, core::Point at, int cell) {
        const int itemId = m_shown[static_cast<std::size_t>(cell)];
        const auto record = itemId != 0 ? m_services.items.find(itemId) : std::nullopt;
        if (!record) {
            return;
        }
        if (const auto icon = m_services.pictures.find(std::to_string(record->iconId))) {
            const gfx::Picture& picture = m_services.pictures.picture(*icon);
            canvas.drawPicture(*icon, at, core::Rect{0, 0, std::min(picture.width(), kItemCell), std::min(picture.height(), kItemCell)});
        }
    }

    void RemoteControlWindow::paintHints(gfx::Canvas& canvas, core::Point at) {
        const core::Point mouse = m_screen.mouse();
        const auto& pet = m_pets.at(m_pets.activePlace());
        for (std::size_t slider = 0; slider < m_sliders.size(); ++slider) {
            const core::Point rail{at.x + kRailX, at.y + kRailY[slider]};
            if (!core::Rect{rail.x, rail.y, PercentSlider::kWidth, kRailHintHeight}.contains(mouse)) {
                continue;
            }
            const int percent = m_sliders[slider].position();
            int most = 0;
            switch (slider) {
                case 0: most = m_status.maxHp; break; // slider 0: player max HP
                case 1: most = m_status.maxSp; break; // slider 1: player max SP
                case 2: most = pet ? pet->maxHp : 0; break; // slider 2: pet max HP
                default: most = pet ? pet->maxSp : 0; break; // slider 3: pet max SP
            }
            const std::string text =
                std::to_string(static_cast<int>(std::ceil(most * percent / 100.0))) + "(" + std::to_string(percent) + "%)"; // percent to fraction
            const int width = gfx::TextRenderer::textWidth(text, gfx::TextEffect::Plain);
            // hint box: 19 pixels above the rail, 17 high, text width plus 7 wide
            const core::Rect box{rail.x, rail.y - 19, width + 7, 17};
            canvas.fillRectAlpha(box, {61, 139, 249, 200}); // translucent blue fill (RGBA)
            canvas.drawRectOutline(box, {0, 0, 128, 255}); // dark blue outline (RGBA)
            // text 3 pixels in from the box, white RGBA, text area 400 x 16
            canvas.drawText({box.x + 3, box.y}, text, {{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}, 400, 16);
        }
    }

}
