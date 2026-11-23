#ifndef NORTH_ISLAND_CLIENT_MODELS_REMOTE_AUTO_DELIVER_H
#define NORTH_ISLAND_CLIENT_MODELS_REMOTE_AUTO_DELIVER_H

#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/remote/AutoDeliverSettings.h"
#include "formats/ItemTable.h"
#include "protocol/ItemInstance.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace client {

    /// @brief The Electron Control's Auto Deliver: while it runs, a free pet takes the chosen items to one player by pet express.
    /// @details Not in the original game. It sends the express's own packets (25/20, 25/21) and follows its own trips, apart from
    /// the Express window's. A trip leaves when five stacks are ready (a full stack, or an item that does not stack) or with
    /// whatever is listed when the bag is nearly full. It never sends from a place the bag's sort lock holds.
    /// The server delivers what lies in a loaded place when the trip ends, and some of its handlers take items from such a place
    /// (battle, crafting, quests): the pet is called back as soon as a loaded place changes.
    class AutoDeliver {
        public:
            /// @brief Callbacks into the rest of the client.
            struct Hooks {
                std::function<bool()> running; // the Electron Control is on
                std::function<bool()> canSend; // between battles, no trade, no trip of the Express window
                std::function<std::optional<std::int32_t>(const std::string&)> findPlayer;
                std::function<int(int preferred)> freePet; // Npc.dat kind of a pet that may go, 0 for none
                std::function<std::string(int kind)> petName;
                std::function<std::set<int>()> lockedPlaces;
                std::function<void(std::int32_t pet, std::int32_t target, const std::vector<std::pair<int, int>>& stacks)> send;
                std::function<void()> recall;
                std::function<void(const std::string&)> chat;
                std::function<void()> marksChanged;
            };

            static constexpr int kTripStacks = 5; // most stacks of one trip (the server's limit)
            static constexpr int kLowFreePlaces = 5; // with this many free places or fewer, part stacks go too

            /// @brief Creates Auto Deliver, off.
            /// @param items The item table.
            /// @param inventory The player's bag.
            /// @param marks The bag places' marks.
            /// @param hooks Callbacks into the client.
            AutoDeliver(const formats::ItemTable& items, const Inventory& inventory, BagMarks& marks, Hooks hooks);

            /// @brief Gets the settings in force.
            /// @return The settings.
            const AutoDeliverSettings& settings() const { return m_settings; }

            /// @brief Puts settings in force; a wait or pause ends when they differ.
            /// @param settings The settings.
            void setSettings(const AutoDeliverSettings& settings);

            /// @brief Asks for one trip now with whatever is listed, part stacks too, also while the Electron Control is off.
            /// @return Why none can go, or nothing when one is on its way.
            std::optional<std::string> sendNow();

            /// @brief Runs one step.
            /// @param nowMs Current time in ms.
            void update(std::uint64_t nowMs);

            /// @brief Tells whether a trip of its own is asked for or under way.
            /// @return True while the pet is busy for it.
            bool isBusy() const { return m_phase != Phase::Idle; }

            /// @brief Tells whether an item is kept for delivery, so the Remote Control neither drinks nor discards it.
            /// @param itemId The item.
            /// @return True while running with the item listed.
            bool keeps(int itemId) const { return m_settings.on && m_settings.lists(itemId) && m_hooks.running(); }

            /// @brief Takes the server's delivery state (25/20) when it is of its own trip.
            /// @param pet Npc.dat kind of the pet.
            /// @param state 1 the pet is off, 2 it is back.
            /// @param windowBusy True while the Express window waits for or has a trip.
            /// @return True when it was its own; false when it is the Express window's.
            bool deliveryState(std::int32_t pet, int state, bool windowBusy);

            /// @brief Takes the server's express error (25/22) when it is of its own trip.
            /// @param code The error code.
            /// @param itemId The item the error names.
            /// @return True when it was its own.
            bool failed(int code, int itemId);

            /// @brief Notes an express result (25/23).
            /// @param kind 1 received, 2 sent.
            /// @param otherId The other player.
            /// @return True when it is a delivery of its own trip.
            bool result(int kind, std::int32_t otherId);

            /// @brief Counts the stacks the next trip would take with some settings.
            /// @param settings The settings to count with.
            /// @return 0 to kTripStacks.
            int readyCount(const AutoDeliverSettings& settings) const;

            /// @brief What it is doing, in a line.
            /// @return The line; empty when it is off and idle.
            std::string status() const;

        private:
            enum class Phase {
                Idle,
                Sending, // asked, no answer yet
                Away, // the pet is off
            };

            /// @brief A stack as it was loaded.
            struct Load {
                int slot = 0;
                protocol::ItemInstance item;
            };

            /// @brief Finds the stacks that may go, the fullest first.
            /// @param settings The settings to find with.
            /// @param partStacks True to take stacks that are not full too.
            /// @return The bag places.
            std::vector<int> stacksToSend(const AutoDeliverSettings& settings, bool partStacks) const;

            /// @brief Tells whether the bag is nearly full.
            /// @return True with kLowFreePlaces free places or fewer.
            bool bagNearlyFull() const;

            /// @brief Sends a pet off.
            /// @param slots The bag places, at most kTripStacks go.
            /// @param pet The pet.
            /// @param target The recipient's id.
            /// @param asked True for Send now.
            void startTrip(const std::vector<int>& slots, int pet, std::int32_t target, bool asked);

            /// @brief Calls the pet back when the trip must not finish.
            void watchTrip();

            /// @brief Marks or frees the loaded places.
            /// @param held True to mark.
            void hold(bool held);

            /// @brief Frees the places and waits before the next trip.
            /// @param waitMs The wait.
            /// @param waiting Why, for the status; empty for none.
            void endTrip(std::uint64_t waitMs, std::string waiting = {});

            const formats::ItemTable& m_items;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            Hooks m_hooks;
            AutoDeliverSettings m_settings;

            Phase m_phase = Phase::Idle;
            std::vector<Load> m_load;
            std::int32_t m_pet = 0;
            std::int32_t m_target = 0;
            std::string m_to; // the name the trip was sent to
            bool m_asked = false;
            bool m_recalled = false;
            bool m_delivered = false;
            bool m_mayStartLate = false; // a request timed out; its "pet is off" can still come
            int m_error = 0;
            int m_errorItem = 0;
            int m_unanswered = 0;
            std::uint64_t m_sentMs = 0;

            bool m_wasRunning = false;
            bool m_paused = false; // the recipient blocks the player
            std::string m_waiting;
            std::uint64_t m_nextMs = 0;
            std::uint64_t m_nowMs = 0;
    };

}

#endif
