#ifndef NORTH_ISLAND_CLIENT_MODELS_REMOTE_REMOTE_CONTROL_H
#define NORTH_ISLAND_CLIENT_MODELS_REMOTE_REMOTE_CONTROL_H

#include "client/services/GameServices.h"
#include "client/models/character/PlayerStatus.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/PetTeam.h"
#include "client/models/remote/AutoBattleSettings.h"
#include "client/models/remote/RemoteBattleView.h"
#include "client/models/remote/RemoteScript.h"
#include "client/models/remote/RemoteSettings.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "core/Bytes.h"
#include "core/Point.h"
#include "gfx/Canvas.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace client {

    /// @brief The Remote Control item: while it runs the client plays for the player (wanders, supplies HP/SP, takes off worn gear, drops items, leaves).
    /// @details Original: item 34058, TH_AutoPlayForm, TForm1.ProcessAutoPlay, StartAutoWander / DoAutoWanderStep. The settings window is RemoteControlWindow.
    class RemoteControl {
        public:
            /// @brief Callbacks into the rest of the client.
            struct Hooks {
                std::function<int()> fightState;
                std::function<int()> teamStatus;
                std::function<std::int32_t()> teamLeaderId;
                std::function<void(int slot, int order)> useBagItem;
                std::function<void(int order, int equipSlot, int bagSlot)> unequip;
                std::function<void(int slot, int count)> dropBagItem;
                std::function<void()> marksChanged;
                std::function<bool()> standing;
                std::function<bool()> worldShown;
                std::function<core::Point()> position;
                std::function<void(core::Point)> walkTo;
                std::function<void()> closeForm;
                std::function<std::int32_t()> playerId;
                std::function<void(const std::string&)> chat; // a line in the chat's system channel
                std::function<bool(int itemId)> keptForDelivery; // Auto Deliver sends it: neither drunk nor discarded
                std::function<std::string()> deliveryStatus; // Auto Deliver's line of the information, empty for none
            };

            /// @brief Creates the remote control.
            /// @param services Game services (connection, item table).
            /// @param inventory The player's bag and equipment.
            /// @param marks Busy marks of the bag places.
            /// @param status Player status (HP, SP, bottles).
            /// @param pets The player's pets.
            /// @param messages Player message line.
            /// @param hooks Callbacks into the client.
            RemoteControl(
                GameServices& services,
                const Inventory& inventory,
                BagMarks& marks,
                const PlayerStatus& status,
                const PetTeam& pets,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Tells whether the Remote Control is running.
            /// @return True while on.
            bool isOn() const { return m_on; }

            /// @brief Tells whether the item the settings window was opened for, or that runs, is the Electron Control.
            /// @return True for the Electron Control (34128).
            bool isElectron() const;

            /// @brief Gets the bag place of the Remote Control item (original FAutoPlayItemSlot).
            /// @return The place, 0 for none.
            int itemSlot() const { return m_itemSlot; }

            /// @brief Gets the settings for change.
            /// @return The settings.
            RemoteSettings& settings() { return m_settings; }

            /// @brief Gets the settings.
            /// @return The settings.
            const RemoteSettings& settings() const { return m_settings; }

            /// @brief Gets the auto battle sets of the Plus form for change (original Player.FAfkSettingsSets; 1 player, 2 pet, [0] unused).
            /// @return The two sets.
            std::array<AutoBattleSettings, 3>& playSets() { return m_playSets; }

            /// @brief Gets the auto battle sets of the Plus form (1 player, 2 pet, [0] unused).
            /// @return The two sets.
            const std::array<AutoBattleSettings, 3>& playSets() const { return m_playSets; }

            /// @brief Tells whether the player is wandering (original FAutoWander).
            /// @return True while wandering.
            bool isWandering() const { return m_wandering; }

            /// @brief Uses the Remote Control item from the bag (original UseGridItem): shows a refusal, else remembers the place and tells the server.
            /// @details Sends packet 23/76.
            /// @param bagSlot Bag place of the item.
            /// @param onVehicle True while on a vehicle.
            /// @param stalled True while a stall is up.
            /// @return True when the settings window opens.
            bool use(int bagSlot, bool onVehicle, bool stalled);

            /// @brief Puts the settings in force, marks the item in use and tells the server (end of original StartAutoPlay).
            /// @details Sends packet 23/78.
            /// @param wander True to start wandering.
            void start(bool wander);

            /// @brief Stops after the player's request and tells the server (original StopAutoPlay).
            /// @details Sends packet 23/79.
            void stop();

            /// @brief Stops because the server said so (original ResetAutoPlayState).
            void reset();

            /// @brief Starts wandering about the current spot (original StartAutoWander).
            void startWander();

            /// @brief Stops wandering (original StopAutoWander).
            void stopWander() { m_wandering = false; }

            /// @brief Runs one step of the automatic play (original TForm1.ProcessAutoPlay and DoAutoWanderStep).
            /// @param nowMs Current time in ms.
            void update(std::uint64_t nowMs);

            /// @brief Asks the Electron Control's script what the commanding fighter does.
            /// @details Not in the original game. Only while the Electron Control (34128) runs with a script; a script error shows on screen.
            /// @param view The battle.
            /// @return The choice; nothing when there is no script, it returned nil, or it failed.
            std::optional<RemoteDecision> decide(const RemoteBattleView& view);

            /// @brief What the player chose for the Electron Control's script.
            struct ScriptChoice {
                std::string path; // as typed; empty for the default file (RemoteScript::pathFor); a relative one is in the user folder
                bool debug = false; // errors, print() and each choice go to the chat and the log
            };

            /// @brief Gets the player's choice for the script (from sd<id xor 7654321>.script).
            /// @return The choice.
            ScriptChoice scriptChoice() const;

            /// @brief Keeps the player's choice for the script and, while the Electron Control runs, loads the script again.
            /// @param choice The choice.
            void setScriptChoice(const ScriptChoice& choice);

            /// @brief Tells whether the Electron Control runs with a script loaded, so the battle's auto asks it.
            /// @return True while on with a script.
            bool scriptOn() const { return m_on && m_script.loaded(); }

            /// @brief Tells debug mode whether the script's choice became the command.
            /// @param decision The script's choice.
            /// @param actorKind 1 the player's choice, 2 the pet's.
            /// @param used True when it is sent, false when it could not be used and the Plus settings decide.
            void scriptOutcome(const RemoteDecision& decision, int actorKind, bool used);

            /// @brief Counts a death of the player (original ShowFightExpResult: experience lost).
            void playerDied();

            /// @brief Counts a death of the pet (original ShowFightExpResult: amity lost).
            void petDied();

            /// @brief Ends the wait for a supply (HP or SP set, the Bottle's fill answered, a fighter left).
            void supplyDone() { m_itemPending = false; }

            /// @brief Lets the worn-out gear be taken off again after an equipment change.
            void equipmentChanged() {
                m_unequipSent = false;
                m_unequipDone = false;
            }

            /// @brief Notes that something arrived in the bag, so the chosen items are dropped again.
            void bagFilled() { m_dropPending = 1; }

            /// @brief Paints the list of what the Remote Control is set to over the left of the screen (original DrawLocationLine).
            /// @param canvas Canvas to paint on.
            void paintInformation(gfx::Canvas& canvas) const;

        private:
            /// @brief Whether a pool is low and which character to supply.
            struct Target {
                bool low = false;
                int order = -1;
            };

            /// @brief Checks whether the player's, else the active pet's HP or SP is under its percent.
            /// @param hp True for HP, false for SP.
            /// @return Whether it is low and the order of the character to supply.
            Target lowOf(bool hp) const;

            /// @brief Finds the bag place of a supply: the first (or with tick 7 the one of the least value) in the range of places.
            /// @param hp True for an HP supply, false for SP.
            /// @return The bag place, 0 for none.
            int findSupply(bool hp) const;

            /// @brief Takes the worn-out gear of the player and the active pet off into the bag.
            void unequipWorn();

            /// @brief Drops the items to keep out of the supply, on the pass after something arrived.
            void dropKept();

            /// @brief Loads the player's script when the item in use is the Electron Control, else drops it.
            void loadScript();

            /// @brief Shows a script error on screen and, in debug mode, in the chat and the log.
            /// @param text The error.
            void scriptError(const std::string& text);

            /// @brief In debug mode, puts a line in the chat and the log.
            /// @param text The line.
            void scriptDebug(const std::string& text);

            /// @brief Takes the player out of the game (original SendAfkAutoStop).
            void leaveGame();

            /// @brief Ends the wandering and leaves the game after too many deaths.
            void stopsItself();

            GameServices& m_services;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            const PlayerStatus& m_status;
            const PetTeam& m_pets;
            PlayerMessages& m_messages;
            Hooks m_hooks;
            RemoteScript m_script;
            bool m_debug = false;
            std::string m_actorName; // who the script last decided for: "player" or "pet"

            RemoteSettings m_settings = RemoteSettings::defaults();
            std::array<AutoBattleSettings, 3> m_playSets{AutoBattleSettings{}, AutoBattleSettings::defaults(), AutoBattleSettings::defaults()};
            bool m_on = false;
            int m_itemSlot = 0;
            int m_deaths = 0;
            int m_petDeaths = 0;
            bool m_itemPending = false;
            std::uint64_t m_itemPendingMs = 0; // when the pending supply was sent, ms
            static constexpr std::uint64_t kSupplyWaitMs = 3000; // a supply with no HP / SP change by then was refused or never sent: try again
            bool m_stopPending = false;
            std::uint64_t m_stopMs = 0;
            bool m_unequipSent = false;
            bool m_unequipDone = false;
            int m_dropPending = 0;
            bool m_wandering = false;
            core::Point m_wanderCenter;
            int m_wanderStep = 0;
            std::uint64_t m_nowMs = 0;
    };

}

#endif
