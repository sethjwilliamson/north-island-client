#ifndef NORTH_ISLAND_CLIENT_MODELS_EVENTS_EVENT_RUNNER_H
#define NORTH_ISLAND_CLIENT_MODELS_EVENTS_EVENT_RUNNER_H

#include "client/services/GameServices.h"
#include "client/models/events/EventResult.h"
#include "client/viewmodels/trade/NpcStoreWindow.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "client/viewmodels/chat/TalkDialog.h"
#include "core/Bytes.h"
#include "core/Point.h"
#include "formats/SceneSurface.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <utility>

namespace client {

    /// @brief Client side of the server-run map events (TEventManage): NPC and door events and their results.
    /// @details Original: TEventManage; a click on an NPC with events sends 20/1, a door sends 20/8, results arrive as 20/1..6,
    /// the next one is asked for with 20/6, dialog answers go as 20/9 (Confirm 20, Cancel 21, choice 30+n, Close 40), 20/8 ends the event.
    class EventRunner {
        public:
            /// @brief Callbacks into the world and the UI that the runner needs.
            struct Hooks {
                std::function<void(const core::Bytes&)> send;
                std::function<std::optional<core::Point>(int npcIndex)> npcPosition;
                std::function<std::optional<TalkDialog::Speaker>(int npcIndex)> npcSpeaker;
                std::function<core::Point()> playerPosition;
                std::function<TalkDialog::Speaker()> playerSpeaker;
                std::function<void(core::Point target, int npcIndex)> face;
                std::function<void(const EventResult&)> worldResult;
                std::function<bool()> npcsWalking;
                std::function<bool(const EventResult&)> movie;
                std::function<bool()> fighting;
                std::function<void(int dataId, int code)> answered; // the player answered a dialog (not called for mirrored events)
                std::function<void()> ended; // the player's own event ended (not called for mirrored events)
            };

            /// @brief Creates the runner over the shared services and windows.
            /// @param services Game data (items, talk lines).
            /// @param talk Talk dialog used for lines, questions and choices.
            /// @param store Window used for NPC stores.
            /// @param messages Short message line.
            /// @param hooks World and network callbacks.
            EventRunner(
                GameServices& services,
                TalkDialog& talk,
                NpcStoreWindow& store,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Whether an event runs (the player cannot walk then).
            /// @details Original: FEventRunning.
            /// @return True while an event runs.
            bool isRunning() const { return m_running; }

            /// @brief The NPC whose event runs.
            /// @details Original: FEventNpcIndex.
            /// @return NPC index, 0 for none.
            int eventNpc() const { return m_eventNpc; }

            /// @brief The door whose event runs.
            /// @details Original: FEventDoorIndex.
            /// @return Door index, 0 for none.
            int eventDoor() const { return m_eventDoor; }

            /// @brief Sets the map's surfaces (dialogs and stores) by index.
            /// @param surfaces Surfaces of the map keyed by data id.
            void setSurfaces(std::map<int, formats::SceneSurface> surfaces) { m_surfaces = std::move(surfaces); }

            /// @brief Starts the event of a clicked NPC (or moves a running event on).
            /// @details Original: StartNpcClickEvent; sends 20/1 and sets FClickConsumed.
            /// @param npcIndex Index of the clicked NPC.
            /// @param eventCount Number of events the NPC has.
            /// @param halfSize Trigger area of the NPC.
            /// @param standing Whether the NPC stands still.
            /// @return True when the NPC's event was asked for.
            bool npcClicked(int npcIndex, int eventCount, core::Point halfSize, bool standing);

            /// @brief The player came near an NPC whose events start so.
            /// @details Original: CheckNpcProximityTrigger.
            /// @param npcIndex Index of the NPC.
            void npcNear(int npcIndex);

            /// @brief A click while an event runs moves it on.
            /// @details Original: StartNpcClickEvent without an NPC.
            void clicked();

            /// @brief The player walked into a door with events (sends 20/8).
            /// @details Original: CheckDoorTrigger.
            /// @param doorIndex Index of the door.
            void doorEntered(int doorIndex);

            /// @brief Handles one event result from the server (20/1..6).
            /// @details Original: Inter_RunResults.
            /// @param result The result.
            void result(const EventResult& result);

            /// @brief Handles 20/7 and 20/9..20/19: the server lets the event go on, waits or tells something.
            /// @param sub Sub id of the 20/x packet.
            /// @param value Packet value (unused).
            void signal(int sub, std::int32_t value);

            /// @brief 20/8: the event is over.
            void finished();

            /// @brief Asks for the next result when one is wanted.
            /// @details Original: UpdateEventTick.
            void update();

            /// @brief Marks an event as running because the server was asked for something that ends with 20/8 (a tent entered).
            /// @details Original: FEventRunning := 1.
            void startedElsewhere() { m_running = true; }

            /// @brief Starts an NPC's or a door's event because the party leader started it (qol NPC mirror): no proximity check,
            /// talk lines move on by themselves and dialogs are answered with the leader's answers.
            /// @param trigger How the leader started it; the same 20/x trigger is sent.
            /// @param index Index of the NPC or the door.
            /// @return True when the event was asked for.
            bool startMirrored(PartyTrigger trigger, int index);

            /// @brief The leader answered a dialog; the mirrored event answers its own matching dialog the same way.
            /// @param dataId The leader's dialog surface.
            /// @param code The leader's answer code.
            void leaderAnswered(int dataId, int code);

            /// @brief The leader's event ended; a mirrored dialog left without a leader's answer is closed.
            void leaderEnded();

            /// @brief A form the event opened went (the sale form), so the next result is asked for.
            /// @details Original: FRequestNextResult.
            void moveOn() { m_requestNext = true; }

        private:
            /// @brief Shows a talk line of an NPC (mode 3) or the player (mode 7), result kind 1.
            /// @param result The talk result.
            void talk(const EventResult& result);

            /// @brief Shows a dialog with choices or Confirm / Cancel, or a store, result kind 6.
            /// @param result The surface result.
            void surface(const EventResult& result);

            /// @brief Sends the answer of a dialog (20/9).
            /// @details Original: InterChoiceOk / Cancel / Select / Close.
            /// @param code Answer code (Confirm 20, Cancel 21, choice 30+n, Close 40).
            void answer(int code);

            /// @brief Answers the waiting mirrored dialog with the leader's next answer, or closes it when the leader's
            /// dialog differs or the leader's event has ended.
            void answerMirrored();

            /// @brief Where the talk looks: the event's NPC, another NPC, or a step from the player.
            /// @param option Look mode (1 event NPC, 2 other NPC, 3 step from the player).
            /// @param target NPC index or direction step.
            /// @return The point to look at, if any.
            std::optional<core::Point> lookTarget(int option, int target) const;

            GameServices& m_services;
            TalkDialog& m_talk;
            NpcStoreWindow& m_store;
            PlayerMessages& m_messages;
            Hooks m_hooks;
            std::map<int, formats::SceneSurface> m_surfaces;
            bool m_running = false;
            bool m_requestNext = false;
            bool m_waitingReply = false;
            bool m_waitingChoice = false;
            bool m_lineOpen = false;
            int m_eventNpc = 0;
            int m_eventDoor = 0;
            int m_surfaceId = 0; // the open dialog surface, 0 for none
            bool m_mirrored = false;
            bool m_leaderEnded = false;
            std::optional<int> m_mirrorSurface; // the mirrored dialog waiting for the leader's answer
            std::deque<std::pair<int, int>> m_leaderAnswers; // the leader's answers (surface, code) not yet used
    };

}

#endif
