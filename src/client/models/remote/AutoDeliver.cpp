#include "client/models/remote/AutoDeliver.h"

#include "client/rules/ExpressItems.h"

#include <algorithm>
#include <cstddef>

namespace client {

    namespace {

        constexpr std::uint64_t kAnswerMs = 5000; // a request with no answer by then was refused: the server says nothing in a battle or a trade
        constexpr std::uint64_t kRetryMs = 60000; // wait after a refusal, ms
        constexpr std::uint64_t kSoonMs = 3000; // wait after a recall or an unanswered request, ms
        constexpr std::uint64_t kBetweenTripsMs = 1000; // wait after a delivery, ms
        constexpr int kUnansweredLimit = 3; // unanswered requests in a row before waiting kRetryMs
        constexpr int kLoadedBusy = 2; // busy kind 2: loaded for the express, as in the Express window
        // 25/22 error codes
        constexpr int kCannotGo = 1;
        constexpr int kOffline = 2;
        constexpr int kBagFull = 3;
        constexpr int kInMinigame = 7;
        constexpr int kBlocked = 8;

    }

    AutoDeliver::AutoDeliver(const formats::ItemTable& items, const Inventory& inventory, BagMarks& marks, Hooks hooks)
        : m_items(items),
          m_inventory(inventory),
          m_marks(marks),
          m_hooks(std::move(hooks)) {}

    void AutoDeliver::setSettings(const AutoDeliverSettings& settings) {
        if (settings == m_settings) {
            return;
        }
        m_settings = settings;
        m_paused = false;
        m_waiting.clear();
        m_nextMs = 0;
        m_unanswered = 0;
    }

    std::vector<int> AutoDeliver::stacksToSend(const AutoDeliverSettings& settings, bool partStacks) const {
        std::vector<int> slots;
        if (!settings.hasItems()) {
            return slots;
        }
        const std::set<int> locked = m_hooks.lockedPlaces();
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            if (item.empty() || !settings.lists(item.itemId) || locked.count(slot) != 0) {
                continue;
            }
            if (m_marks.useMark(slot) != 0 || !m_marks.isAvailable(slot) || m_marks.vehicleState(slot) != 0 || m_marks.isHidden(slot)) {
                continue;
            }
            if (expressRefusal(m_items, item)) {
                continue;
            }
            if (!partStacks && m_inventory.isStackable(item.itemId) && item.count < Inventory::kMaxStack) {
                continue;
            }
            slots.push_back(slot);
        }
        const auto fullness = [this](int slot) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slot);
            return m_inventory.isStackable(item.itemId) ? static_cast<int>(item.count) : Inventory::kMaxStack;
        };
        std::stable_sort(slots.begin(), slots.end(), [&fullness](int a, int b) { return fullness(a) > fullness(b); });
        return slots;
    }

    bool AutoDeliver::bagNearlyFull() const {
        int free = 0;
        for (int cell = 1; cell <= Inventory::kBagSlots; ++cell) {
            free += m_inventory.occupantOf(cell) == 0 ? 1 : 0;
        }
        return free <= kLowFreePlaces;
    }

    int AutoDeliver::readyCount(const AutoDeliverSettings& settings) const {
        return std::min(kTripStacks, static_cast<int>(stacksToSend(settings, false).size()));
    }

    void AutoDeliver::hold(bool held) {
        for (const Load& load : m_load) {
            m_marks.setUseMark(load.slot, held ? BagMarks::kExpress : 0);
            m_marks.setBusyKind(load.slot, held ? kLoadedBusy : 0);
        }
        m_hooks.marksChanged();
    }

    void AutoDeliver::startTrip(const std::vector<int>& slots, int pet, std::int32_t target, bool asked) {
        m_load.clear();
        std::vector<std::pair<int, int>> stacks;
        for (std::size_t i = 0; i < slots.size() && i < static_cast<std::size_t>(kTripStacks); ++i) {
            const protocol::ItemInstance& item = m_inventory.bagItem(slots[i]);
            m_load.push_back({slots[i], item});
            stacks.emplace_back(slots[i], item.count);
        }
        m_phase = Phase::Sending;
        m_pet = pet;
        m_target = target;
        m_to = m_settings.recipient;
        m_asked = asked;
        m_recalled = m_delivered = m_mayStartLate = m_paused = false;
        m_error = 0;
        m_sentMs = m_nowMs;
        m_waiting.clear();
        hold(true);
        m_hooks.send(pet, target, stacks);
    }

    std::optional<std::string> AutoDeliver::sendNow() {
        if (isBusy()) {
            return "The pet is already delivering";
        }
        if (m_settings.recipient.empty()) {
            return "Choose who to deliver to";
        }
        const auto target = m_hooks.findPlayer(m_settings.recipient);
        if (!target) {
            return m_settings.recipient + " is not online";
        }
        if (!m_hooks.canSend()) {
            return "Cannot deliver right now";
        }
        const std::vector<int> slots = stacksToSend(m_settings, true);
        if (slots.empty()) {
            return "None of the chosen items can be sent";
        }
        const int pet = m_hooks.freePet(m_settings.petKind);
        if (pet == 0) {
            return "No pet is free to deliver";
        }
        startTrip(slots, pet, *target, true);
        return std::nullopt;
    }

    void AutoDeliver::watchTrip() {
        if (m_recalled || m_delivered || m_error != 0) {
            return;
        }
        bool stop = !m_asked && (!m_settings.on || !m_hooks.running());
        stop = stop || m_settings.recipient != m_to;
        stop = stop || m_hooks.findPlayer(m_to) != m_target; // gone, or the account plays another character now
        for (const Load& load : m_load) {
            const protocol::ItemInstance& item = m_inventory.bagItem(load.slot);
            stop = stop || !m_settings.lists(load.item.itemId) || item.itemId != load.item.itemId || item.count < load.item.count ||
                   item.fusionLevel != load.item.fusionLevel || item.insertItemId != load.item.insertItemId || item.lookItemId != load.item.lookItemId;
        }
        if (stop) {
            m_recalled = true;
            m_hooks.recall();
        }
    }

    void AutoDeliver::update(std::uint64_t nowMs) {
        m_nowMs = nowMs;
        const bool running = m_hooks.running();
        if (running && !m_wasRunning) { // started again: try again
            m_paused = false;
            m_waiting.clear();
            m_nextMs = 0;
            m_unanswered = 0;
        }
        m_wasRunning = running;
        if (m_phase != Phase::Idle) {
            watchTrip();
            if (m_phase == Phase::Sending && nowMs - m_sentMs >= kAnswerMs) {
                m_mayStartLate = true;
                if (++m_unanswered >= kUnansweredLimit) {
                    m_unanswered = 0;
                    endTrip(kRetryMs, "waiting, the pet cannot set off");
                } else {
                    endTrip(kSoonMs);
                }
            }
            return;
        }
        if (!running || !m_settings.on || m_paused || m_settings.recipient.empty() || nowMs < m_nextMs) {
            return;
        }
        m_waiting.clear();
        if (!m_hooks.canSend()) {
            return;
        }
        const bool nearlyFull = bagNearlyFull();
        const std::vector<int> slots = stacksToSend(m_settings, nearlyFull);
        if (slots.empty() || (!nearlyFull && slots.size() < static_cast<std::size_t>(kTripStacks))) {
            return;
        }
        const auto target = m_hooks.findPlayer(m_settings.recipient);
        if (!target) {
            return; // the status says so; sent as soon as the player is online
        }
        const int pet = m_hooks.freePet(m_settings.petKind);
        if (pet == 0) {
            m_waiting = "waiting for a free pet";
            return;
        }
        startTrip(slots, pet, *target, false);
    }

    void AutoDeliver::endTrip(std::uint64_t waitMs, std::string waiting) {
        hold(false);
        m_phase = Phase::Idle;
        m_nextMs = m_nowMs + waitMs;
        m_waiting = std::move(waiting);
    }

    bool AutoDeliver::deliveryState(std::int32_t pet, int state, bool windowBusy) {
        if (pet != m_pet) {
            return false;
        }
        if (state == 1) {
            if (m_phase == Phase::Idle) {
                if (!m_mayStartLate || windowBusy) {
                    return false;
                }
                hold(true);
            }
            m_mayStartLate = false;
            m_unanswered = 0;
            m_phase = Phase::Away;
            return true;
        }
        if (state != 2 || m_phase == Phase::Idle) {
            return false;
        }
        if (m_error != 0) {
            std::string waiting;
            switch (m_error) {
                case kCannotGo: {
                    const auto record = m_items.find(m_errorItem);
                    waiting = "waiting, cannot send " + (record ? record->name : std::string("an item"));
                    break;
                }
                case kOffline: waiting = "waiting, " + m_to + " is offline"; break;
                case kBagFull: waiting = "waiting, " + m_to + "'s bag is full"; break;
                case kInMinigame: waiting = "waiting, " + m_to + " is in a game"; break;
                case kBlocked:
                    m_paused = true;
                    m_hooks.chat("Auto Deliver paused: " + m_to + " has you on the blacklist");
                    break;
                default: waiting = "waiting, the express was refused"; break;
            }
            endTrip(kRetryMs, std::move(waiting));
        } else if (m_delivered) {
            endTrip(kBetweenTripsMs);
        } else if (m_recalled) {
            endTrip(kSoonMs);
        } else {
            endTrip(kRetryMs, "waiting, the pet was called back"); // by the Express window's Stop
        }
        return true;
    }

    bool AutoDeliver::failed(int code, int itemId) {
        if (m_phase == Phase::Idle) {
            return false;
        }
        m_error = code;
        m_errorItem = itemId;
        if (m_phase == Phase::Sending) { // refused at once: no state follows
            deliveryState(m_pet, 2, false);
        }
        return true;
    }

    bool AutoDeliver::result(int kind, std::int32_t otherId) {
        if (kind != 2 || m_phase == Phase::Idle || otherId != m_target) { // kind 2: delivered
            return false;
        }
        m_delivered = true;
        return true;
    }

    std::string AutoDeliver::status() const {
        if (m_phase != Phase::Idle) {
            if (m_recalled) {
                return "calling the pet back";
            }
            return m_hooks.petName(m_pet) + " delivering to " + m_to;
        }
        if (!m_settings.on || m_settings.recipient.empty()) {
            return {};
        }
        if (m_paused) {
            return "paused, " + m_settings.recipient + " blocks you";
        }
        if (!m_hooks.findPlayer(m_settings.recipient)) {
            return "waiting, " + m_settings.recipient + " is offline";
        }
        if (!m_waiting.empty()) {
            return m_waiting;
        }
        return m_settings.recipient + ", " + std::to_string(readyCount(m_settings)) + "/" + std::to_string(kTripStacks) + " ready";
    }

}
