#include "client/models/remote/RemoteControl.h"

#include "client/rules/ItemInfo.h"
#include "core/Log.h"
#include "gfx/TextRenderer.h"
#include "protocol/RemoteRequests.h"
#include "protocol/TeamRequests.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

namespace client {

    namespace {

        constexpr int kItemBattleType = 24; // item type 24: battle items (Bomb, Bowling, Banana Peel, ...), never used as supplies
        constexpr int kHpAttribute = 25; // item attribute kind 25: restores HP
        constexpr int kSpAttribute = 26; // item attribute kind 26: restores SP
        constexpr int kWornOut = 200; // wear value from which gear counts as damaged
        constexpr std::uint64_t kStopHoldMs = 3000; // time the stop request is held, ms
        constexpr int kMessageMs = 1500; // on-screen message time, ms
        constexpr int kElectronControl = 34128; // item 34128: Electron Control, the remote that runs a battle script
        constexpr int kScriptMessageMs = 5000; // on-screen time of the script's messages and errors, ms

        // wander offsets from the wander centre in world px {x, y}, indexed by wander step 1..8 (index 0 unused)
        constexpr std::array<core::Point, 9> kWanderOffsets = {
            {{0, 0}, {0, 0}, {-200, -100}, {-200, 0}, {-100, 200}, {-150, 150}, {100, -100}, {200, 50}, {150, 150}}
        };

        constexpr int kLabelX = 5; // x of the status text, px
        constexpr int kFirstLineY = 200; // y of the first status line, px
        constexpr int kLineStep = 15; // line spacing of the status text, px
        constexpr std::uint16_t kYellow = 65504; // RGB565 0xFFE0: yellow (value of an enabled option)

        /// @brief Computes the amount that is a percent of a maximum, rounded up.
        /// @param max The maximum.
        /// @param percent The percent.
        /// @return Ceil(max * percent / 100).
        int percentOf(int max, int percent) { return static_cast<int>(std::ceil(static_cast<double>(max) * percent / 100)); } // percent divisor

    }

    RemoteControl::RemoteControl(
        GameServices& services,
        const Inventory& inventory,
        BagMarks& marks,
        const PlayerStatus& status,
        const PetTeam& pets,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_inventory(inventory),
          m_marks(marks),
          m_status(status),
          m_pets(pets),
          m_messages(messages),
          m_hooks(std::move(hooks)),
          m_script([this](const std::string& line) { scriptDebug(line); }) {}

    bool RemoteControl::use(int bagSlot, bool onVehicle, bool stalled) {
        if (m_on) {
            m_messages.show("Close the Remote Control in use", kMessageMs);
            return false;
        }
        if (onVehicle) {
            m_messages.show("You are using a vehicle", kMessageMs);
            return false;
        }
        if (stalled) {
            m_messages.show("Operation availabe after revoking stall", 2000); // message time in ms
            return false;
        }
        // default death limit 3; ticks 5 and 6 (leave when the player / the pet died too often) are forced on
        m_settings.deathLimit = m_settings.deathLimit == 0 ? 3 : m_settings.deathLimit;
        m_settings.petDeathLimit = m_settings.petDeathLimit == 0 ? 3 : m_settings.petDeathLimit;
        m_settings.ticks[5] = true;
        m_settings.ticks[6] = true;
        m_itemSlot = bagSlot;
        m_services.connection.send(protocol::remoteOpenRequest(bagSlot));
        return true;
    }

    void RemoteControl::start(bool wander) {
        m_deaths = 0;
        m_petDeaths = 0;
        if (wander) {
            startWander();
        } else {
            stopWander();
        }
        m_marks.setUseMark(m_itemSlot, 10); // use mark 10: the item shows as in use
        m_on = true;
        loadScript();
        int ticks = 0;
        for (int tick = 1; tick <= RemoteSettings::kTicks; ++tick) {
            if (m_settings.ticks[static_cast<std::size_t>(tick)]) {
                ticks |= 1 << (tick - 1);
            }
        }
        m_services.connection.send(
            protocol::remoteStartRequest(
                ticks,
                m_settings.hpPercent,
                m_settings.spPercent,
                m_settings.petHpPercent,
                m_settings.petSpPercent,
                m_settings.deathLimit,
                m_settings.petDeathLimit,
                m_settings.leaveMinutes,
                m_settings.itemFrom,
                m_settings.itemTo
            )
        );
        m_unequipSent = false;
        m_unequipDone = false;
        m_dropPending = 1;
        m_hooks.marksChanged();
    }

    void RemoteControl::reset() {
        m_on = false;
        m_script.unload();
        stopWander();
        m_deaths = 0;
        m_petDeaths = 0;
        m_marks.setUseMark(m_itemSlot, 0);
        m_itemSlot = 0;
        m_itemPending = false;
        m_unequipSent = false;
        m_unequipDone = false;
        m_dropPending = 0;
        m_stopPending = false;
        m_stopMs = 0;
        m_hooks.marksChanged();
        m_hooks.closeForm();
    }

    void RemoteControl::stop() {
        reset();
        m_services.connection.send(protocol::remoteStopRequest());
    }

    bool RemoteControl::isElectron() const { return m_itemSlot != 0 && m_inventory.bagItem(m_itemSlot).itemId == kElectronControl; }

    void RemoteControl::loadScript() {
        if (!isElectron() || !m_hooks.playerId) {
            m_script.unload();
            return;
        }
        const ScriptChoice choice = scriptChoice();
        m_debug = choice.debug;
        const std::filesystem::path path = choice.path.empty() ? RemoteScript::pathFor(m_services.game.userRoot(), m_hooks.playerId())
                                                               : m_services.game.userRoot() / std::filesystem::path(choice.path);
        if (!choice.path.empty() && !std::filesystem::exists(path)) {
            m_script.unload();
            scriptError("Script not found: " + path.string());
            return;
        }
        if (m_script.load(path)) {
            m_messages.show("Electron Control script loaded", kScriptMessageMs);
            scriptDebug("loaded " + path.string());
        } else if (!m_script.error().empty()) {
            scriptError(m_script.error());
        } else {
            scriptDebug("no script at " + path.string() + ": the Plus settings decide");
        }
    }

    RemoteControl::ScriptChoice RemoteControl::scriptChoice() const {
        ScriptChoice choice;
        if (!m_hooks.playerId) {
            return choice;
        }
        std::ifstream file(RemoteScript::choiceFor(m_services.game.userRoot(), m_hooks.playerId()), std::ios::binary);
        // line 1 the path, line 2 "debug" when debug mode is on
        std::string lines[2];
        for (std::string& line : lines) {
            std::getline(file, line);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
        }
        choice.path = lines[0];
        choice.debug = lines[1] == "debug";
        return choice;
    }

    void RemoteControl::setScriptChoice(const ScriptChoice& choice) {
        if (!m_hooks.playerId) {
            return;
        }
        const std::filesystem::path file = RemoteScript::choiceFor(m_services.game.userRoot(), m_hooks.playerId());
        std::error_code ignored;
        if (choice.path.empty() && !choice.debug) {
            std::filesystem::remove(file, ignored);
        } else {
            std::ofstream(file, std::ios::binary | std::ios::trunc) << choice.path << '\n' << (choice.debug ? "debug\n" : "");
        }
        m_debug = choice.debug;
        if (m_on) {
            loadScript();
        }
    }

    std::optional<RemoteDecision> RemoteControl::decide(const RemoteBattleView& view) {
        if (!m_on || !m_script.loaded()) {
            return std::nullopt;
        }
        m_actorName = view.actorKind == 2 ? "pet" : "player"; // actor kind 2: the pet
        auto decision = m_script.decide(view);
        if (!m_script.error().empty()) {
            scriptError(m_script.error());
        } else if (!decision) {
            scriptDebug(m_actorName + ": nil, the Plus settings decide");
        }
        return decision;
    }

    void RemoteControl::scriptOutcome(const RemoteDecision& decision, int actorKind, bool used) {
        if (!m_debug) {
            return;
        }
        std::string what;
        if (decision.item) {
            const auto item = m_services.items.find(decision.id);
            what = (item ? item->name : "item") + " (" + std::to_string(decision.id) + ")";
        } else {
            const auto skill = m_services.skills.find(decision.id);
            what = (skill ? skill->name : "skill") + " (" + std::to_string(decision.id) + ")";
        }
        scriptDebug(
            std::string(actorKind == 2 ? "pet" : "player") + ": " + what + " on slot " + std::to_string(decision.targetSlot) +
            (used ? "" : " cannot be used now: the Plus settings decide")
        );
    }

    void RemoteControl::scriptError(const std::string& text) {
        m_messages.show(text, kScriptMessageMs);
        if (m_debug) {
            core::logWarning("Electron Control script: ", text);
            if (m_hooks.chat) {
                m_hooks.chat("[script] " + text);
            }
        }
    }

    void RemoteControl::scriptDebug(const std::string& text) {
        if (!m_debug) {
            return;
        }
        core::logInfo("Electron Control script: ", text);
        if (m_hooks.chat) {
            m_hooks.chat("[script] " + text);
        }
    }

    void RemoteControl::startWander() {
        if (!m_wandering) {
            m_wanderCenter = m_hooks.position();
            m_wanderStep = 2; // the second place of the offsets list
            if (m_hooks.teamStatus() != 2) { // team status 2: member (members do not walk on their own)
                m_hooks.walkTo(m_wanderCenter + kWanderOffsets[static_cast<std::size_t>(m_wanderStep)]);
            }
            m_wandering = true;
        }
    }

    void RemoteControl::playerDied() {
        if (m_on && m_settings.ticks[5]) { // tick 5: leave when the player died too often
            ++m_deaths;
        }
    }

    void RemoteControl::petDied() {
        if (m_on && m_settings.ticks[6]) { // tick 6: leave when the pet died too often
            ++m_petDeaths;
        }
    }

    RemoteControl::Target RemoteControl::lowOf(bool hp) const {
        Target target;
        const int level = hp ? m_settings.hpPercent : m_settings.spPercent;
        const int petLevel = hp ? m_settings.petHpPercent : m_settings.petSpPercent;
        const int now = hp ? m_status.hp : m_status.sp;
        const int most = hp ? m_status.maxHp : m_status.maxSp;
        if (percentOf(most, level) > now && now < most) {
            target.low = true;
            target.order = 0;
            return target;
        }
        const auto& pet = m_pets.at(m_pets.activePlace());
        if (pet) {
            const int petNow = hp ? pet->hp : pet->sp;
            const int petMost = hp ? pet->maxHp : pet->maxSp;
            if (percentOf(petMost, petLevel) > petNow && petNow < petMost) {
                target.low = true;
                target.order = pet->order;
            }
        }
        return target;
    }

    int RemoteControl::findSupply(bool hp) const {
        int best = 0;
        int least = 65535; // larger than any attribute value
        const int attribute = hp ? kHpAttribute : kSpAttribute;
        for (int slot = m_settings.itemFrom; slot <= m_settings.itemTo && slot <= Inventory::kBagSlots; ++slot) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            if (item.itemId == 0 || item.count == 0) {
                continue;
            }
            const auto record = m_services.items.find(item.itemId);
            if (m_hooks.keptForDelivery(item.itemId)) {
                continue;
            }
            if (!record || record->type == kItemBattleType || m_marks.lockNotice(slot, 0) || !m_marks.isAvailable(slot)) {
                continue;
            }
            bool candidate = false;
            std::size_t which = 0;
            const int kind = bottleKind(*record);
            // bottle kinds 1 HP / 2 SP; 3 / 4 the special HP / SP bottles (usable with something in them)
            if (kind == (hp ? 1 : 2)) {
                candidate = true;
            } else if (kind == (hp ? 3 : 4) && (hp ? m_status.hpBottle : m_status.spBottle) > 0) {
                candidate = true;
            } else {
                // an item has two attribute slots; the potion must raise the attribute by more than 100 (value 100 is the cut-off)
                for (std::size_t index = 0; index < 2; ++index) {
                    which = index;
                    if (record->attributeKinds[index] == attribute && record->attributeValues[index] > 100) {
                        candidate = true;
                        break;
                    }
                }
            }
            if (m_settings.ticks[10]) { // tick 10: discard the chosen items
                for (int keep = 1; keep <= RemoteSettings::kKeepItems; ++keep) {
                    const int kept = m_settings.keepItems[static_cast<std::size_t>(keep)];
                    if (kept != 0 && item.itemId == kept) {
                        candidate = false;
                        break;
                    }
                }
            }
            if (!candidate) {
                continue;
            }
            if (!m_settings.ticks[7]) { // tick 7: take the supply of the least value
                return slot;
            }
            if (record->attributeValues[which] < least) {
                least = record->attributeValues[which];
                best = slot;
            }
        }
        return best;
    }

    void RemoteControl::unequipWorn() {
        Inventory bag = m_inventory;
        const auto& pet = m_pets.at(m_pets.activePlace());
        for (int slot = 1; slot <= Inventory::kEquipmentSlots; ++slot) {
            std::optional<std::reference_wrapper<const protocol::ItemInstance>> worn;
            int order = 0;
            if (m_inventory.equipped(slot).wear >= kWornOut) {
                worn = std::cref(m_inventory.equipped(slot));
            } else if (pet && pet->worn[static_cast<std::size_t>(slot)].wear >= kWornOut) {
                worn = std::cref(pet->worn[static_cast<std::size_t>(slot)]);
                order = pet->order;
            }
            if (worn) {
                const int free = bag.freeSlotFor(worn->get().itemId);
                if (free == 0) {
                    m_unequipDone = true;
                    break;
                }
                m_hooks.unequip(order, slot, free);
                protocol::ItemInstance placed;
                placed.itemId = worn->get().itemId;
                placed.count = 1;
                bag.addToBag(free, placed);
                m_unequipSent = true;
            }
            if (slot == Inventory::kEquipmentSlots) {
                m_unequipDone = true;
            }
        }
    }

    void RemoteControl::dropKept() {
        if (m_dropPending == 2) { // drop pending state 2: the pass after something arrived
            for (int keep = 1; keep <= RemoteSettings::kKeepItems; ++keep) {
                const int kept = m_settings.keepItems[static_cast<std::size_t>(keep)];
                if (kept == 0 || m_hooks.keptForDelivery(kept)) {
                    continue;
                }
                for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
                    if (m_inventory.bagItem(slot).itemId == kept && !m_marks.lockNotice(slot, 0) && m_marks.isAvailable(slot)) {
                        m_hooks.dropBagItem(slot, m_inventory.bagItem(slot).count);
                    }
                }
            }
            m_dropPending = 0;
        } else if (m_dropPending == 1) {
            ++m_dropPending;
        }
    }

    void RemoteControl::leaveGame() {
        if (m_hooks.teamStatus() != 0) {
            m_services.connection.send(protocol::leaveTeamRequest(m_hooks.teamLeaderId()));
        }
        m_services.connection.send(protocol::remoteLeaveRequest());
    }

    void RemoteControl::update(std::uint64_t nowMs) {
        m_nowMs = nowMs;
        const int fight = m_hooks.fightState();
        if (m_itemPending && fight != 0 && fight != 4) { // fight state 4 keeps the wait for a supply
            m_itemPending = false;
        }
        if (m_itemPending && fight == 0 && nowMs - m_itemPendingMs >= kSupplyWaitMs) {
            // no HP / SP change came: the use was refused (or never sent), so the wait would last until the remote is reset
            m_itemPending = false;
        }
        if (!m_on) {
            return;
        }
        // team status 2 is member; the round has 8 places (steps 1..8)
        if (m_wandering && m_hooks.teamStatus() != 2 && m_hooks.standing() && m_hooks.worldShown()) {
            m_wanderStep = (m_wanderStep & 7) + 1;
            m_hooks.walkTo(m_wanderCenter + kWanderOffsets[static_cast<std::size_t>(m_wanderStep)]);
        }
        if (fight != 0) {
            return;
        }
        if (m_settings.ticks[3] && !m_itemPending) { // tick 3: auto supply after a battle
            bool used = false;
            bool nothing = false;
            const Target hpLow = lowOf(true);
            if (hpLow.low) {
                if (const int slot = findSupply(true)) {
                    m_hooks.useBagItem(slot, hpLow.order);
                    m_itemPending = true;
                    m_itemPendingMs = nowMs;
                    used = true;
                } else {
                    nothing = true;
                }
            }
            if (!used) {
                const Target spLow = lowOf(false);
                if (spLow.low) {
                    if (const int slot = findSupply(false)) {
                        m_hooks.useBagItem(slot, spLow.order);
                        m_itemPending = true;
                        m_itemPendingMs = nowMs;
                    } else {
                        nothing = true;
                    }
                }
            }
            if (m_settings.ticks[4] && nothing && !m_stopPending) { // tick 4: leave when no supply is left
                stopWander();
                m_services.connection.send(protocol::remoteGaveUpRequest());
                leaveGame();
                m_stopPending = true;
                m_stopMs = nowMs;
                return;
            }
        }
        if (m_settings.ticks[9] && !m_unequipDone && !m_unequipSent) { // tick 9: take off damaged gear
            unequipWorn();
        }
        if (m_settings.ticks[10]) { // tick 10: discard the chosen items
            dropKept();
        }
        if (m_stopPending && nowMs - m_stopMs >= kStopHoldMs) {
            m_stopPending = false;
        }
        if (m_settings.ticks[5] && m_deaths >= m_settings.deathLimit && !m_stopPending) { // tick 5: leave when the player died too often
            stopsItself();
            return;
        }
        if (m_settings.ticks[6] && m_petDeaths >= m_settings.petDeathLimit && !m_stopPending) { // tick 6: leave when the pet died too often
            stopsItself();
        }
    }

    void RemoteControl::stopsItself() {
        stopWander();
        leaveGame();
        m_stopPending = true;
        m_stopMs = m_nowMs;
    }

    void RemoteControl::paintInformation(gfx::Canvas& canvas) const {
        if (!m_on || !m_settings.ticks[11]) { // tick 11: show the information over the screen
            return;
        }
        const auto white = [](std::uint16_t color) { return gfx::TextStyle{core::Color::fromRgb565(color), gfx::TextEffect::Shadow, std::nullopt}; };
        int y = kFirstLineY;
        const auto line = [&](const std::string& label, const std::string& value, bool on) {
            // 65535 is white in RGB565; 400 the text width limit and 12 the line height
            canvas.drawText({kLabelX, y}, label, white(65535), 400, 12);
            if (!value.empty()) {
                const int width = gfx::TextRenderer::textWidth(label, gfx::TextEffect::Plain);
                canvas.drawText({kLabelX + width, y}, value, white(on ? kYellow : 65535), 400, 12); // same 65535 white / 400 width / 12 height
            }
            y += kLineStep;
        };
        const std::string colon = "\xA1G";
        const auto onOff = [](bool on) { return std::string(on ? " On" : " Off"); };
        // the ticks[n] below are the tick numbers of RemoteSettings (1 fight, 2 wander, 3 supply, 4 no supply, 5/6 deaths, 8 leave time, 9 damaged gear, 10 discard)
        const bool fight = m_settings.ticks[1];
        const bool playerSet = m_playSets[1].flags[0] != 0;
        const bool petSet = m_playSets[2].flags[0] != 0;
        line(" Auto Attack" + colon, onOff(fight), fight);
        line(" Set the level of the advanced quests" + colon, onOff(playerSet), playerSet);
        line(" Set the pet's level advanced quest" + colon, onOff(petSet), petSet);
        line(" Auto Wander" + colon, onOff(m_settings.ticks[2]), m_settings.ticks[2]);
        line(" Auto Restore" + colon, onOff(m_settings.ticks[3]), m_settings.ticks[3]);
        line(" Unable to revive" + colon, onOff(m_settings.ticks[4]), m_settings.ticks[4]);
        line(" You died many times and have to leave" + colon, onOff(m_settings.ticks[5]), m_settings.ticks[5]);
        line(" Your pet died many times and has to leave" + colon, onOff(m_settings.ticks[6]), m_settings.ticks[6]);
        const bool restore = m_settings.ticks[3];
        const auto& pet = m_pets.at(m_pets.activePlace());
        line(
            " Enough HP" + colon,
            std::to_string(percentOf(m_status.maxHp, m_settings.hpPercent)) + "(" + std::to_string(m_settings.hpPercent) + "%)",
            restore
        );
        line(
            " Enough SP" + colon,
            std::to_string(percentOf(m_status.maxSp, m_settings.spPercent)) + "(" + std::to_string(m_settings.spPercent) + "%)",
            restore
        );
        line(
            " Enough HP of pet" + colon,
            (pet ? std::to_string(percentOf(pet->maxHp, m_settings.petHpPercent)) : std::string("0")) + "(" + std::to_string(m_settings.petHpPercent) + "%)",
            restore
        );
        line(
            " Enough SP of pet" + colon,
            (pet ? std::to_string(percentOf(pet->maxSp, m_settings.petSpPercent)) : std::string("0")) + "(" + std::to_string(m_settings.petSpPercent) + "%)",
            restore
        );
        line(" Number of deaths" + colon, std::to_string(m_deaths) + "/" + std::to_string(m_settings.deathLimit), m_settings.ticks[5]);
        line(" Number of pet's deaths" + colon, std::to_string(m_petDeaths) + "/" + std::to_string(m_settings.petDeathLimit), m_settings.ticks[6]);
        line(" Set the time for leaving" + colon, onOff(m_settings.ticks[8]), m_settings.ticks[8]);
        line(" Take off damaged equipment" + colon, onOff(m_settings.ticks[9]), m_settings.ticks[9]);
        line(" Automatically discard the selected item: ", onOff(m_settings.ticks[10]), m_settings.ticks[10]);
        if (const std::string delivery = m_hooks.deliveryStatus(); !delivery.empty()) {
            line(" Auto Deliver" + colon, " " + delivery, true);
        }
    }

}
