#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_CHAT_TALK_DIALOG_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_CHAT_TALK_DIALOG_H

#include "client/services/GameServices.h"
#include "client/models/character/CharacterLook.h"
#include "client/models/character/FigureAnimation.h"
#include "client/rendering/characters/PortraitBlink.h"
#include "client/viewmodels/chat/MessageZoom.h"
#include "core/Size.h"
#include "formats/NpcInfo.h"
#include "ui/DrawingArea.h"
#include "ui/Element.h"
#include "ui/ImageButton.h"
#include "ui/ListBox.h"
#include "ui/MessageTags.h"
#include "ui/Panel.h"
#include "ui/ScrollBar.h"
#include "ui/Screen.h"
#include "ui/Spot.h"

#include "client/services/input/PadAsk.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace client {

    /// @brief Options of one talk box question: wrap width, answer timeout, exclusivity and the movie/compass variants.
    struct TalkDialogTiming {
        std::size_t wrapChars = 40; // default line length of the main box in characters
        std::uint64_t timeoutMs = 0;
        std::function<void()> onTimeout;
        bool exclusive = true;
        bool indentWrapped = false;
        bool compassBox = false;
        std::function<int()> shownNumber;
        std::string okPicture;
        std::string closePicture;
        bool movie = false;
        bool movieScreen = false;
        bool movieBottom = false;
        bool modal = false;
    };

    /// @brief The game's talk box (TalkDialog.xml, MainTalkMsgForm): wrapped text, the speaker's figure or face, and Confirm/Cancel or a choice list with Close.
    /// @details Original: TSe_TalkMsgFormPlus.CalcMsgPanelSize, ShowTalkOkCancel, ShowTalkChoiceClose; sits centred at the top.
    class TalkDialog {
        public:
            /// @brief Who speaks: a player (drawn at the left) or an NPC such as a pet (at the right), as figure or face.
            /// @details Original: SetTalkSpeaker's AShowPortrait shows the face instead of the figure when the NPC has one.
            struct Speaker {
                std::variant<CharacterLook, formats::NpcInfo> figure;
                std::string name;
                bool portrait = false;
                bool wide = false;
            };

            /// @brief Loads TalkDialog.xml into the screen and wires the buttons and painter.
            /// @param services Game services (layouts, pictures, characters, sounds).
            /// @param screen Screen the dialog's controls are loaded into.
            TalkDialog(
                GameServices& services,
                ui::Screen& screen
            );

            /// @brief Whether the box is showing.
            /// @return True while the frame is visible.
            bool isOpen() const { return m_frame.isVisible(); }

            /// @brief The box's frame as it is now (it grows while opening).
            /// @return The frame rectangle.
            core::Rect frameBounds() const { return m_frame.bounds(); }

            /// @brief Whether the main box (MainTalkMsgForm, an exclusive question or words) is up.
            /// @return True when open and exclusive.
            bool isMainOpen() const { return isOpen() && m_timing.exclusive; }

            /// @brief Whether the box is open and done opening, so its buttons or choices are shown (test scripts wait for it).
            /// @return True when it can be answered.
            bool isReady() const { return isOpen() && !m_zoom; }

            /// @brief Answers the box as a click would (test scripts): a number picks that choice (1 is the first), Confirm, Cancel or
            /// Close presses that button, Next closes words that wait for a click.
            /// @param what The answer.
            /// @param result Receives what was answered, or why it could not be.
            /// @return False, with the reason, when the box does not take that answer now.
            bool scriptAnswer(const std::string& what, std::string& result);

            /// @brief Sets the hook told when an exclusive question opens (true) or is answered (false); the scene hides its windows meanwhile.
            /// @param hook Callback receiving the exclusive state.
            void setExclusiveHook(std::function<void(bool)> hook) { m_exclusive = std::move(hook); }

            /// @brief Sets what the text tags put in: the player's name (#n) and the castle owner's guild (#g).
            /// @param playerName Supplies the player's name.
            /// @param castleOwner Supplies the castle owner's guild name.
            void setTagSource(std::function<std::string()> playerName, std::function<std::string()> castleOwner) {
                m_playerName = std::move(playerName);
                m_castleOwner = std::move(castleOwner);
            }

            using Timing = TalkDialogTiming;

            /// @brief Asks a question answered with Confirm or Cancel; the box closes either way (ShowTalkOkCancel).
            /// @param text Message text (may carry tags).
            /// @param speaker Optional speaker shown beside the text.
            /// @param onOk Called on Confirm.
            /// @param onCancel Called on Cancel.
            /// @param timing Box options.
            void ask(
                const std::string& text,
                const std::optional<Speaker>& speaker,
                std::function<void()> onOk,
                std::function<void()> onCancel,
                Timing timing = {}
            );

            /// @brief Asks a question with Confirm, Close and Cancel buttons (ShowTalkOkCancelClose).
            /// @param text Message text (may carry tags).
            /// @param speaker Optional speaker shown beside the text.
            /// @param onOk Called on Confirm.
            /// @param onCancel Called on Cancel.
            /// @param onClose Called on Close.
            /// @param timing Box options.
            void askOrClose(
                const std::string& text,
                const std::optional<Speaker>& speaker,
                std::function<void()> onOk,
                std::function<void()> onCancel,
                std::function<void()> onClose,
                Timing timing = {}
            );

            /// @brief Shows words without buttons; a click on the box closes it (ShowShortMsgCb, no time limit).
            /// @param text Message text (may carry tags).
            /// @param speaker Optional speaker shown beside the text.
            /// @param onClosed Called when the box is clicked closed.
            /// @param timing Box options.
            void say(const std::string& text, const std::optional<Speaker>& speaker, std::function<void()> onClosed, Timing timing = {});

            /// @brief Offers a list of choices plus Close (ShowTalkChoiceClose); the box closes on an answer.
            /// @param text Message text (may carry tags).
            /// @param speaker Optional speaker shown beside the text.
            /// @param choices Choice labels.
            /// @param onChoice Called with the chosen index counted from 0.
            /// @param onClose Called on Close.
            /// @param timing Box options.
            void choose(
                const std::string& text,
                const std::optional<Speaker>& speaker,
                const std::vector<std::string>& choices,
                std::function<void(int)> onChoice,
                std::function<void()> onClose,
                Timing timing = {}
            );

            /// @brief Closes the box without an answer, giving the scene its windows back if it was exclusive.
            void close();

            /// @brief Answers the box from a game pad: Accept confirms, goes on or takes the chosen entry (the first press on a
            /// list only marks its first entry), Cancel cancels or closes, Up and Down move through a list of choices.
            /// @param ask What the pad asks.
            /// @return True when the box took it (always while it is open, but for Up and Down without a list).
            bool padAsk(PadAsk ask);

            /// @brief Whether a list of choices is up, which Up and Down step through.
            /// @return True while choosing.
            bool padSteps() const { return isOpen() && (m_onChoice != nullptr || (m_onOk != nullptr && m_onCancel != nullptr)); }

            /// @brief Tells the box whether a game pad is in use: a yes/no question then shows which answer its accept button takes.
            /// @param on True while the pad is the input in use.
            /// @param hand The hand that points at the answer.
            void setPad(bool on, const ui::PadPointer& hand) {
                m_padOn = on;
                m_padHand = hand;
            }

            /// @brief Sets the main box's frame skin (FMsgSkin): 1 plain frame panel22, 2 Form_Talk_3.
            /// @param skin Skin number.
            void setSkin(int skin) { m_skin = skin; }

            /// @brief Gets the current frame skin.
            /// @return Skin number.
            int skin() const { return m_skin; }

            /// @brief Hides or reveals the box under a screen laid over the world, without ending the question.
            /// @param covered True to hide.
            void setCovered(bool covered) { m_layer.setVisible(!covered); }

            /// @brief Advances the opening animation and ends a question left unanswered past its timeout.
            /// @param nowMs Current time in milliseconds.
            void update(std::uint64_t nowMs);

        private:
            /// @brief Lays out and opens the box for the text and speaker, then the buttons or choices (CalcMsgPanelSize).
            /// @param text Message text (may carry tags).
            /// @param speaker Optional speaker.
            /// @param timing Box options.
            void show(const std::string& text, const std::optional<Speaker>& speaker, Timing timing);

            /// @brief Ends the question: the box goes and the scene gets its windows back (ResetTalkButtons' end).
            void finish();

            /// @brief Closes the box and calls the Confirm or Cancel handler.
            /// @param ok True for Confirm, false for Cancel.
            void answer(bool ok);

            /// @brief Handles a click on the box: words without buttons close (TSe_TalkMsgFormPlus.HandleMouseUp, CloseShortMsg).
            void clicked();

            /// @brief Closes the box and reports the chosen list entry.
            /// @param choice Index of the choice, negative for none.
            void pick(int choice);

            /// @brief Closes the box and calls the Close handler.
            void dismiss();

            /// @brief Paints the text lines, the speaker and the compass number.
            /// @param canvas Target canvas.
            /// @param at Top-left of the content area.
            /// @param nowMs Current time in milliseconds.
            void paint(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Paints the speaker's animated figure and its name.
            /// @param canvas Target canvas.
            /// @param at Top-left of the content area.
            /// @param nowMs Current time in milliseconds.
            void paintSpeaker(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Paints the speaker's blinking face and name (DrawTalkSpeakerPortrait).
            /// @param canvas Target canvas.
            /// @param at Top-left of the content area.
            /// @param nowMs Current time in milliseconds.
            void paintPortrait(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Paints the blinking arrow of words that wait for a click.
            /// @param canvas Target canvas.
            /// @param at Top-left of the content area.
            /// @param nowMs Current time in milliseconds.
            void paintNextIcon(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Paints the speaker's name centred at a point, white on skin 1, else dark plain letters.
            /// @param canvas Target canvas.
            /// @param at Centre of the name's top edge.
            void paintName(gfx::Canvas& canvas, core::Point at);

            /// @brief Picks the sprite slot (pose) of the speaker's figure.
            /// @return Slot index.
            int speakerSlot() const;

            /// @brief Whether the wide frame (Form_Talk_1 / 2) is used.
            /// @return True for a movie box or a player in starter clothes.
            bool wideFrame() const { return m_timing.movie || m_wide; }

            /// @brief Runs one frame of the opening zoom (AnimMsgOpenZoom); once the text shows, buttons and choices appear.
            /// @param nowMs Current time in milliseconds.
            void openStep(std::uint64_t nowMs);

            GameServices& m_services;
            ui::Screen& m_screen;
            ui::Panel& m_frame;
            ui::Element& m_layer;
            ui::Element& m_modalCatcher;
            ui::DrawingArea& m_content;
            ui::ImageButton& m_ok;
            ui::ImageButton& m_cancel;
            ui::ImageButton& m_close;
            ui::ListBox& m_choices;
            ui::ScrollBar& m_choiceScroll;
            const ui::Spot& m_movieBoxSpot;
            const ui::Spot& m_wideBoxSpot;
            const ui::Spot& m_skinSourceSpot;
            const ui::Spot& m_faceSpot;
            const ui::Spot& m_choiceSpot;
            const ui::Spot& m_textSpot;
            const ui::Spot& m_redSpot;
            const ui::Spot& m_nameSpot;
            const ui::Spot& m_npcNameSpot;
            std::vector<std::string> m_lines;
            std::optional<ui::MessageTags> m_tags;
            std::function<std::string()> m_playerName;
            std::function<std::string()> m_castleOwner;
            std::optional<Speaker> m_speaker;
            core::Size m_speakerSize;
            bool m_portrait = false;
            bool m_wide = false;
            int m_targetWidth = 0;
            int m_targetHeight = 0;
            int m_skin = 1;
            std::function<void()> m_onOk;
            bool m_padOn = false; // a game pad is the input in use
            bool m_padNo = false; // the pad stands on the question's Cancel, not its Confirm
            bool m_padFocused = false; // the screen's pad highlight is on an answer
            ui::PadPointer m_padHand; // the hand at the answer
            std::function<void()> m_onCancel;
            std::function<void(int)> m_onChoice;
            std::function<void()> m_onClose;
            gfx::PictureId m_okPicture;
            gfx::PictureId m_closePicture;
            std::function<void()> m_onSaid;
            std::function<void(bool)> m_exclusive;
            Timing m_timing;
            std::optional<MessageZoom> m_zoom;
            core::Rect m_openTarget;
            bool m_choosing = false;
            std::uint64_t m_nowMs = 0;
            std::uint64_t m_askedMs = 0;
            FigureAnimation m_animation;
            PortraitBlink m_blink;
    };

}

#endif
