#include "client/models/events/EventRunner.h"

#include "core/Log.h"
#include "protocol/BattleRequests.h"
#include "protocol/WorldRequests.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace client {

    namespace {

        constexpr int kNear = 170; // max distance in px (each axis) between player and NPC to start its event
        constexpr int kOkAnswer = 20; // answer code sent for Confirm
        constexpr int kCancelAnswer = 21; // answer code sent for Cancel
        constexpr int kChoiceAnswer = 30; // answer code base; the chosen option number is added
        constexpr int kCloseAnswer = 40; // answer code sent when the dialog is closed
        constexpr int kPromptEntry = 1; // surface entry kind 1: prompt text
        constexpr int kAnswerEntry = 2; // surface entry kind 2: Confirm/Cancel answer
        constexpr int kChoiceEntry = 3; // surface entry kind 3: choice line (or store item)
        constexpr int kTalkSource = 1; // entry source 1: text from the talk table
        constexpr int kItemSource = 2; // entry source 2: item id from the item table
        constexpr int kDialogSurface = 1; // surface kind 1: dialog
        constexpr int kEquipmentStoreSurface = 2; // surface kind 2: equipment store (other kinds: goods store)

    }

    EventRunner::EventRunner(
        GameServices& services,
        TalkDialog& talk,
        NpcStoreWindow& store,
        PlayerMessages& messages,
        Hooks hooks
    )
        : m_services(services),
          m_talk(talk),
          m_store(store),
          m_messages(messages),
          m_hooks(std::move(hooks)) {}

    bool EventRunner::npcClicked(int npcIndex, int eventCount, core::Point halfSize, bool standing) {
        if (m_requestNext) {
            return false;
        }
        const bool fighting = m_hooks.fighting && m_hooks.fighting();
        if (m_running && !fighting && !m_waitingReply) {
            m_requestNext = true;
            return false;
        }
        if (m_waitingReply || fighting || eventCount <= 0) {
            return false;
        }
        if (halfSize.x != 0 && halfSize.y != 0) {
            const auto at = m_hooks.npcPosition(npcIndex);
            const core::Point player = m_hooks.playerPosition();
            if (!standing || !at || std::abs(player.x - at->x) >= kNear || std::abs(player.y - at->y) >= kNear) {
                return false;
            }
        }
        m_eventNpc = npcIndex;
        m_running = true;
        m_hooks.send(protocol::npcClickedRequest(npcIndex));
        m_waitingReply = true;
        return true;
    }

    bool EventRunner::startMirrored(PartyTrigger trigger, int index) {
        if (m_running || m_waitingReply || (m_hooks.fighting && m_hooks.fighting())) {
            return false;
        }
        m_mirrored = true;
        m_leaderEnded = false;
        m_leaderAnswers.clear();
        m_running = true;
        switch (trigger) {
            case PartyTrigger::NpcClick:
                m_eventNpc = index;
                m_hooks.send(protocol::npcClickedRequest(index));
                break;
            case PartyTrigger::NpcNear:
                m_eventNpc = index;
                m_hooks.send(protocol::npcNearRequest(index));
                break;
            case PartyTrigger::Door:
                m_eventDoor = index;
                m_hooks.send(protocol::doorEnteredRequest(index));
                break;
        }
        m_waitingReply = true;
        return true;
    }

    void EventRunner::leaderAnswered(int dataId, int code) {
        if (!m_mirrored) {
            return;
        }
        m_leaderAnswers.emplace_back(dataId, code);
        answerMirrored();
    }

    void EventRunner::leaderEnded() {
        if (!m_mirrored) {
            return;
        }
        m_leaderEnded = true;
        answerMirrored();
    }

    void EventRunner::answerMirrored() {
        if (!m_mirrorSurface) {
            return;
        }
        int code = kCloseAnswer;
        if (!m_leaderAnswers.empty()) {
            const auto [dataId, leaderCode] = m_leaderAnswers.front();
            m_leaderAnswers.pop_front();
            if (dataId == *m_mirrorSurface) {
                code = leaderCode;
            } else {
                core::logInfo("mirrored event: dialog ", *m_mirrorSurface, " is not the leader's ", dataId, ", closed");
            }
        } else if (!m_leaderEnded) {
            return;
        }
        m_mirrorSurface.reset();
        answer(code);
    }

    void EventRunner::npcNear(int npcIndex) {
        m_eventNpc = npcIndex;
        m_running = true;
        m_hooks.send(protocol::npcNearRequest(npcIndex));
        m_waitingReply = true;
    }

    void EventRunner::clicked() {
        if (m_requestNext) {
            return;
        }
        if (m_running && !(m_hooks.fighting && m_hooks.fighting()) && !m_waitingReply) {
            m_requestNext = true;
        }
    }

    void EventRunner::doorEntered(int doorIndex) {
        m_running = true;
        m_eventDoor = doorIndex;
        m_hooks.send(protocol::doorEnteredRequest(doorIndex));
        m_waitingReply = true;
    }

    void EventRunner::result(const EventResult& result) {
        m_waitingReply = false;
        m_running = true;
        switch (result.kind) {
            case 0: // 20/x result kind 0: item gain / world result, then next
                m_requestNext = true;
                if (result.mode == 1) {
                    const auto item = m_services.items.find(result.target);
                    m_messages.show((item ? item->name : std::string()) + std::to_string(result.value), 1000); // message display time in ms; mode 1: item gain
                } else if (result.mode == 3 || result.mode == 8) { // result modes carried out by the world
                    m_hooks.worldResult(result);
                }
                break;
            case 1: // result kind 1: talk line
                talk(result);
                break;
            // result kind 2: warp
            case 2: m_requestNext = true; break;
            // result kind 5: movie
            case 5:
                if (m_hooks.movie(result) && result.option >= 1 && result.option <= 4) { // movie screen mode options 1..4
                    m_requestNext = true;
                }
                break;
            // result kind 3: fight (sends 11/250)
            case 3:
                m_hooks.send(protocol::eventBattleRequest());
                m_waitingReply = true;
                break;
            case 4: // result kind 4: world result, then next
            case 11: // result kind 11: world result, then next
            case 14: // result kind 14: world result, then next
            case 15: // result kind 15: world result, then next
                m_requestNext = true;
                m_hooks.worldResult(result);
                break;
            case 6: // result kind 6: dialog or store
                surface(result);
                break;
            default: // unknown result kind
                core::logInfo("map event result of kind ", result.kind, " is not handled yet");
                break;
        }
    }

    std::optional<core::Point> EventRunner::lookTarget(int option, int target) const {
        switch (option) {
            case 1: // look at the event's NPC
                return m_hooks.npcPosition(m_eventNpc);
            case 2: // look at another NPC
                return m_hooks.npcPosition(target);
            case 3: { // look a step from the player
                // offsets in pixels of the eight steps (index 0 unused), 10 px each
                static const core::Point kSteps[] = {{0, 0}, {0, -10}, {10, -10}, {10, 0}, {10, 10}, {0, 10}, {-10, 10}, {-10, 0}, {-10, -10}};
                if (target < 1 || target > 8) { // eight directions
                    return std::nullopt;
                }
                const core::Point player = m_hooks.playerPosition();
                return core::Point{player.x + kSteps[target].x, player.y + kSteps[target].y};
            }
            default: return std::nullopt;
        }
    }

    void EventRunner::talk(const EventResult& result) {
        if (m_mirrored) {
            m_requestNext = true;
            return;
        }
        m_requestNext = false;
        std::optional<core::Point> look;
        int facingNpc = 0;
        if (result.mode == 7) { // mode 7: the player speaks
            look = lookTarget(result.option, result.target);
        } else if (result.mode == 3 && result.target != 0) { // mode 3: an NPC speaks
            look = m_hooks.npcPosition(result.target);
            facingNpc = result.option == 1 ? result.target : 0;
        }
        m_hooks.face(look.value_or(core::Point{}), facingNpc);
        const std::string& text = m_services.talks.text(result.dataId);
        const auto closed = [this] {
            m_lineOpen = false;
            clicked();
        };
        TalkDialog::Timing modal;
        modal.modal = true;
        if (result.mode == 3) { // mode 3: an NPC speaks
            m_talk.say(text, m_hooks.npcSpeaker(result.target), closed, modal);
            m_lineOpen = true;
        } else if (result.mode == 7) { // mode 7: the player speaks
            m_talk.say(text, m_hooks.playerSpeaker(), closed, modal);
            m_lineOpen = true;
        }
    }

    void EventRunner::surface(const EventResult& result) {
        if (result.option == 0) {
            m_requestNext = true;
        } else if (result.option == 1) {
            m_requestNext = false;
        }
        m_waitingChoice = true;
        std::optional<core::Point> look;
        int facingNpc = 0;
        if (result.mode == 3) { // mode 3: an NPC asks
            look = m_hooks.npcPosition(result.target);
            if (!look) {
                return;
            }
            facingNpc = result.target;
        } else if (result.mode == 7 && m_eventNpc != 0) { // mode 7: the player asks
            look = m_hooks.npcPosition(m_eventNpc);
        }
        m_hooks.face(look.value_or(core::Point{}), facingNpc);
        const auto found = m_surfaces.find(result.dataId);
        if (found == m_surfaces.end()) {
            answer(kCloseAnswer);
            return;
        }
        const formats::SceneSurface& surface = found->second;
        m_surfaceId = surface.kind == kDialogSurface ? result.dataId : 0;
        if (m_mirrored) {
            if (surface.kind != kDialogSurface) {
                answer(kCloseAnswer);
                return;
            }
            m_mirrorSurface = result.dataId;
            answerMirrored();
            return;
        }
        const auto entryText = [this, &surface](const formats::SceneSurface::Entry& entry) -> std::string {
            if (entry.source == kTalkSource) {
                return m_services.talks.text(entry.id);
            }
            if (entry.source == kItemSource) {
                const auto item = m_services.items.find(entry.id);
                if (!item) {
                    return {};
                }
                std::string text = item->name + " $" + std::to_string(item->price);
                if (surface.kind == kDialogSurface && entry.kind == kChoiceEntry) {
                    text += "      $" + std::to_string(item->rank);
                }
                return text;
            }
            return {};
        };
        if (surface.kind != kDialogSurface) {
            std::vector<int> items;
            for (const formats::SceneSurface::Entry& entry : surface.entries) {
                if (entry.kind == kChoiceEntry && entry.source == kItemSource) {
                    items.push_back(entry.id);
                }
            }
            m_store.show(surface.kind == kEquipmentStoreSurface ? NpcStoreWindow::Kind::Equipment : NpcStoreWindow::Kind::Goods, items, [this] {
                answer(kCloseAnswer);
            });
            return;
        }
        std::string prompt;
        std::vector<std::string> choices;
        int answers = 0;
        for (const formats::SceneSurface::Entry& entry : surface.entries) {
            if (entry.kind == kPromptEntry) {
                prompt += entryText(entry);
            } else if (entry.kind == kAnswerEntry) {
                ++answers;
            } else if (entry.kind == kChoiceEntry) {
                choices.push_back(entryText(entry));
            }
        }
        auto speaker = result.target == 0 ? std::optional<TalkDialog::Speaker>(m_hooks.playerSpeaker()) : m_hooks.npcSpeaker(result.target);
        if (result.target == 0) {
            speaker->wide = false;
        }
        if (choices.empty()) {
            if (answers == 2) { // two answer entries: Confirm and Cancel only
                m_talk.ask(prompt, speaker, [this] { answer(kOkAnswer); }, [this] { answer(kCancelAnswer); });
            } else {
                m_talk.askOrClose(prompt, speaker, [this] { answer(kOkAnswer); }, [this] { answer(kCancelAnswer); }, [this] { answer(kCloseAnswer); });
            }
            return;
        }
        m_talk.choose(prompt, speaker, choices, [this](int choice) { answer(kChoiceAnswer + choice); }, [this] { answer(kCloseAnswer); });
    }

    void EventRunner::answer(int code) {
        m_hooks.send(protocol::eventAnswerRequest(code));
        m_waitingChoice = false;
        if (!m_mirrored && m_surfaceId != 0 && m_hooks.answered) {
            m_hooks.answered(m_surfaceId, code);
        }
        m_surfaceId = 0;
    }

    void EventRunner::signal(int sub, std::int32_t value) {
        (void)value;
        switch (sub) {
            case 7: // 20/7: event goes on, ask for the next result
            case 10: // 20/10: event goes on, ask for the next result
            case 11: // 20/11: event goes on, ask for the next result
            case 13: // 20/13: event goes on, ask for the next result
            case 14: // 20/14: event goes on, ask for the next result
            case 15: // 20/15: event goes on, ask for the next result
            case 16: // 20/16: event goes on, ask for the next result
            case 17: // 20/17: event goes on, ask for the next result
                m_waitingReply = false;
                m_requestNext = true;
                break;
            case 9: // 20/9: server reply received, wait for the player's answer
                m_waitingReply = false;
                break;
            case 12: // 20/12: event running, waiting for the server's next reply
                m_running = true;
                m_waitingReply = true;
                break;
            case 18: // 20/18: no quiz question now (shows a message)
                m_messages.show("I have no questions for you now. Come to me in 2 hours.", 2000); // message display time in ms
                break;
            default: // other 20/x signals: nothing to do
                break;
        }
    }

    void EventRunner::finished() {
        if (m_lineOpen) {
            m_lineOpen = false;
            m_talk.close();
        }
        m_waitingChoice = false;
        m_running = false;
        m_requestNext = false;
        m_waitingReply = false;
        m_eventNpc = 0;
        m_eventDoor = 0;
        m_surfaceId = 0;
        if (!m_mirrored && m_hooks.ended) {
            m_hooks.ended();
        }
        m_mirrored = false;
        m_leaderEnded = false;
        m_mirrorSurface.reset();
        m_leaderAnswers.clear();
    }

    void EventRunner::update() {
        if (m_requestNext && m_hooks.npcsWalking()) {
            return;
        }
        if (m_requestNext && !m_waitingReply && !m_waitingChoice) {
            m_requestNext = false;
            m_hooks.send(protocol::nextEventResultRequest());
            m_waitingReply = true;
        }
    }

}
