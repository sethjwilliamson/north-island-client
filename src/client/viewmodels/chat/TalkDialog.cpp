#include "client/viewmodels/chat/TalkDialog.h"

#include "client/rendering/hud/GameNumbers.h"
#include "gfx/TextStyle.h"
#include "ui/TextWrap.h"

#include <algorithm>

namespace client {

    namespace {

        constexpr int kMarginLeft = 50; // box left margin around the text, px
        constexpr int kMarginRight = 50; // box right margin around the text, px
        constexpr int kMarginTop = 31; // box top margin above the text, px
        constexpr int kMarginBottom = 31; // box bottom margin below the text, px
        constexpr int kLineHeight = 15; // text line height, px
        constexpr int kLineSpacing = 3; // extra gap between painted text lines, px
        constexpr int kTop = 25; // y of the box top on screen, px
        constexpr int kMovieWrapChars = 45; // line length of the wide/movie box, characters
        constexpr int kWideFaceLeft = 167; // x of the wide box's player face centre, px
        constexpr int kWideFaceFromBottom = 30; // wide box face distance above the box bottom, px
        constexpr int kMovieScreenDrop = 25; // wide box moves down this much on the movie screen, px
        constexpr int kMovieBottomTop = 425 + 25; // y of a box shown at the bottom of a movie, px
        constexpr int kWideFaceSlot = 3; // body action slot used for the wide face (walking, direction 3)
        constexpr int kMovieMarginTop = 45; // text top margin in the wide/movie box, px
        constexpr int kNpcFaceFromRightInMovie = 60 + 48; // NPC face centre distance from the box right edge in a movie, px
        constexpr int kMovieFigureFromRight = 100; // NPC figure distance from the box right edge in a movie, px
        constexpr int kMovieFigureDrop = 20; // NPC figure moves down this much in a movie, px
        constexpr int kMovieNameDrop = 10; // NPC name moves down this much in a movie, px
        constexpr int kCompassTop = 10; // y of the compass box top, px
        constexpr int kCompassShorter = 11; // compass box is this much shorter than the others, px
        constexpr int kCompassNumberAbove = 33; // compass number distance above the box bottom, px
        constexpr int kCompassNumberStyle = 10; // digit style number passed to drawGameNumber for the compass count
        constexpr int kCompassCloseRow = 40; // extra height for the Close button row of the compass box, px
        constexpr int kScreenWidth = 800; // game screen width, px
        constexpr int kPlayerSlot = 13; // body action slot of a speaking player: standing (8..15), direction 5 (south-east)
        constexpr int kNpcSlot = 11; // default body action slot of a speaking NPC: standing (8..15), direction 3 (south-west)
        constexpr int kLargeNpcExtraHeight = 68; // extra box height for a large (spriteSize 1) NPC, px
        constexpr int kPortraitWidth = 120; // extra box width for a face portrait, px
        constexpr int kPortraitLowHeight = 130; // boxes lower than this get the portrait height, px
        constexpr int kPortraitHeight = 156; // minimum box height with a face portrait, px
        constexpr int kPlayerFaceSlot = 2; // face bank slot 2: large face (select/creation screens)
        constexpr int kNpcFaceSlot = 0; // face bank slot 0: status panel face
        constexpr int kNpcFaceFromRight = 60 + 13; // NPC face centre distance from the box right edge, px
        constexpr int kPlayerFaceFromLeft = 45; // player face centre offset from the box left edge, px
        constexpr int kPortraitNameDrop = 36; // name below the face centre, px
        constexpr int kNextIconStepMs = 250; // "next" arrow animation frame time, ms
        constexpr int kLargeNpcNameDrop = 40; // name moves down this much for a large NPC, px
        constexpr int kLowFigureDrop = 96 - 10; // feet drop for NPCs whose weapon value is 1, minus the figure height, px
        constexpr int kChoiceLineChars = 49; // characters per wrapped line of a choice, characters
        constexpr int kMostChoiceHeight = 100; // tallest the choice list gets before it scrolls, px

        /// @brief Loads TalkDialog.xml into the screen and returns its TalkFrame panel.
        /// @param services Game services providing the layout and load context.
        /// @param screen Screen the layout is loaded into.
        /// @return The TalkFrame panel.
        ui::Panel& loadFrame(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("TalkDialog.xml"), services.loadContext());
            return screen.get<ui::Panel>("TalkFrame");
        }

    }

    TalkDialog::TalkDialog(
        GameServices& services,
        ui::Screen& screen
    )
        : m_services(services),
          m_screen(screen),
          m_frame(loadFrame(services, screen)),
          m_layer(screen.get<ui::Element>("TalkLayer")),
          m_modalCatcher(screen.get<ui::Element>("TalkModalCatcher")),
          m_content(screen.get<ui::DrawingArea>("TalkContent")),
          m_ok(screen.get<ui::ImageButton>("TalkOk")),
          m_cancel(screen.get<ui::ImageButton>("TalkCancel")),
          m_close(screen.get<ui::ImageButton>("TalkClose")),
          m_choices(screen.get<ui::ListBox>("TalkChoices")),
          m_choiceScroll(screen.get<ui::ScrollBar>("TalkChoiceScroll")),
          m_movieBoxSpot(screen.get<ui::Spot>("TalkMovieBox")),
          m_wideBoxSpot(screen.get<ui::Spot>("TalkWideBox")),
          m_skinSourceSpot(screen.get<ui::Spot>("TalkSkinSource")),
          m_faceSpot(screen.get<ui::Spot>("TalkFace")),
          m_choiceSpot(screen.get<ui::Spot>("TalkChoiceList")),
          m_textSpot(screen.get<ui::Spot>("TalkTextColor")),
          m_redSpot(screen.get<ui::Spot>("TalkRedColor")),
          m_nameSpot(screen.get<ui::Spot>("TalkNameColor")),
          m_npcNameSpot(screen.get<ui::Spot>("TalkNpcNameColor")) {
        m_okPicture = m_ok.picture();
        m_closePicture = m_close.picture();
        m_content.setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) { paint(canvas, at, nowMs); });
        screen.on("onTalkOk", [this](const ui::EventArgs&) { answer(true); });
        screen.on("onTalkCancel", [this](const ui::EventArgs&) { answer(false); });
        screen.on("onTalkClose", [this](const ui::EventArgs&) { dismiss(); });
        screen.on("onTalkChoice", [this](const ui::EventArgs& args) { pick(args.index); });
        screen.on("onTalkClicked", [this](const ui::EventArgs&) { clicked(); });
    }

    void TalkDialog::ask(
        const std::string& text,
        const std::optional<Speaker>& speaker,
        std::function<void()> onOk,
        std::function<void()> onCancel,
        Timing timing
    ) {
        m_choices.clear();
        m_onChoice = nullptr;
        m_onClose = nullptr;
        m_onSaid = nullptr;
        m_onOk = onOk ? std::move(onOk) : [] {};
        m_onCancel = onCancel ? std::move(onCancel) : [] {};
        m_padNo = false;
        show(text, speaker, std::move(timing));
    }

    void TalkDialog::choose(
        const std::string& text,
        const std::optional<Speaker>& speaker,
        const std::vector<std::string>& choices,
        std::function<void(int)> onChoice,
        std::function<void()> onClose,
        Timing timing
    ) {
        m_choices.clear();
        for (const std::string& choice : choices) {
            m_choices.insertItem(m_choices.itemCount(), choice);
        }
        m_onOk = nullptr;
        m_onCancel = nullptr;
        m_onSaid = nullptr;
        m_onChoice = std::move(onChoice);
        m_onClose = std::move(onClose);
        show(text, speaker, std::move(timing));
    }

    void TalkDialog::askOrClose(
        const std::string& text,
        const std::optional<Speaker>& speaker,
        std::function<void()> onOk,
        std::function<void()> onCancel,
        std::function<void()> onClose,
        Timing timing
    ) {
        m_choices.clear();
        m_onChoice = nullptr;
        m_onSaid = nullptr;
        m_onOk = std::move(onOk);
        m_onCancel = std::move(onCancel);
        m_onClose = std::move(onClose);
        show(text, speaker, std::move(timing));
    }

    void TalkDialog::say(const std::string& text, const std::optional<Speaker>& speaker, std::function<void()> onClosed, Timing timing) {
        m_choices.clear();
        m_onOk = nullptr;
        m_onCancel = nullptr;
        m_onChoice = nullptr;
        m_onClose = nullptr;
        m_onSaid = std::move(onClosed);
        show(text, speaker, std::move(timing));
    }

    void TalkDialog::clicked() {
        if (m_onOk || m_onCancel || m_onClose || m_onChoice) {
            return;
        }
        const std::function<void()> handler = m_onSaid;
        finish();
        if (handler) {
            handler();
        }
    }

    void TalkDialog::show(const std::string& text, const std::optional<Speaker>& speaker, Timing timing) {
        if (isOpen() && m_timing.exclusive && !timing.exclusive && m_exclusive) {
            m_exclusive(false);
        }
        m_timing = std::move(timing);
        m_askedMs = 0;
        m_ok.setPicture(m_timing.okPicture.empty() ? m_okPicture : m_services.pictures.find(m_timing.okPicture).value_or(m_okPicture));
        m_close.setPicture(m_timing.closePicture.empty() ? m_closePicture : m_services.pictures.find(m_timing.closePicture).value_or(m_closePicture));
        m_wide = !m_timing.movie && m_timing.exclusive && !m_timing.compassBox && speaker && speaker->wide;
        if (wideFrame()) {
            m_timing.wrapChars = kMovieWrapChars;
        }
        const char* frame = wideFrame() ? (m_skin == 1 ? "Form_Talk_1" : "Form_Talk_2") : m_timing.exclusive && m_skin == 2 ? "Form_Talk_3" : "panel22";
        m_frame.setSkinSource(wideFrame() ? std::nullopt : std::optional<core::Rect>(m_skinSourceSpot.rect()));
        m_frame.setSkinPicture(m_services.pictures.find(frame).value_or(gfx::PictureId{}), m_services.pictures);
        m_tags.emplace(text, m_playerName ? m_playerName() : std::string(), m_castleOwner ? m_castleOwner() : std::string());
        m_lines = ui::wrapText(m_tags->text(), m_timing.wrapChars, m_timing.indentWrapped);
        if (const auto sound = m_tags->sound()) {
            m_services.playSound("sound/" + *sound + ".wav");
        }
        m_speaker = speaker;
        m_portrait = speaker && speaker->portrait &&
                     (std::holds_alternative<CharacterLook>(speaker->figure) || std::get<formats::NpcInfo>(speaker->figure).faceSpriteId > 0);
        m_blink.restart();
        std::size_t longest = 0;
        for (const std::string& line : m_lines) {
            longest = std::max(longest, line.size());
        }
        int width = static_cast<int>(longest) * 8 + kMarginLeft + kMarginRight + 10; // 8: pixel width of a character; 10: extra horizontal padding
        int height = static_cast<int>(m_lines.size()) * kLineHeight + kMarginTop + kMarginBottom + 4; // extra vertical padding in pixels
        if (m_speaker) {
            if (std::holds_alternative<CharacterLook>(m_speaker->figure)) {
                m_speakerSize = m_services.characters.figureFrameSize(std::get<CharacterLook>(m_speaker->figure), kPlayerSlot);
            } else {
                m_speakerSize = m_services.characters.npcFrameSize(std::get<formats::NpcInfo>(m_speaker->figure).spriteId, speakerSlot());
            }
            if (m_portrait) {
                if (height < kPortraitLowHeight) {
                    height = kPortraitHeight;
                }
                width += kPortraitWidth;
            } else {
                height = std::max(height, m_speakerSize.height + 70); // room for the figure plus its name under it
                width += m_speakerSize.width + 30; // gap between the figure and the text
            }
            if (std::holds_alternative<formats::NpcInfo>(m_speaker->figure) &&
                std::get<formats::NpcInfo>(m_speaker->figure).spriteSize == 1) { // spriteSize 1: large NPC
                height += kLargeNpcExtraHeight;
            }
        }
        const bool choosing = m_onChoice != nullptr;
        const int closeWidth = m_close.bounds().width;
        if (m_onOk || m_onCancel) {
            height += 40; // row for the Confirm/Cancel buttons
            width = std::max(width, 170); // minimum box width with buttons
        } else if (m_timing.compassBox && !choosing) {
            height += kCompassCloseRow;
        }
        const int aside = m_onClose ? closeWidth / 2 : 0;
        // 20: gap from the middle to each button; 45: button row distance from the box bottom
        m_ok.setPosition({width / 2 - aside - 20 - m_ok.bounds().width, height - 45});
        m_cancel.setPosition({width / 2 + aside + 20, height - 45}); // same gap and row as Confirm
        if (choosing) {
            const core::Point panel{14, height - 18}; // choices panel x offset and overlap with the text's bottom
            const int panelWidth = width - 33; // side margins of the choices panel
            int rowLines = 0;
            for (int index = 0; index < m_choices.itemCount(); ++index) {
                rowLines = std::max(rowLines, static_cast<int>(m_choices.itemText(index).size()) / kChoiceLineChars);
            }
            const int rowHeight = rowLines * 10 + 20; // 20: one-line row height; 10: each further wrapped line
            const int rows = m_choices.itemCount() * rowHeight;
            const bool scrolls = rows > kMostChoiceHeight;
            const int listHeight = scrolls ? kMostChoiceHeight : rows;
            const int listWidth = panelWidth - (scrolls ? 40 : 12); // list width loss with / without the scroll bar
            const core::Point list = panel + m_choiceSpot.at();
            m_choices.setRowHeight(rowHeight);
            m_choices.setBounds({list.x, list.y, listWidth, listHeight});
            m_choices.setTopIndex(0);
            // 2: gap between the list and its scroll bar
            m_choiceScroll.setBounds({list.x + listWidth + 2, list.y, m_choiceScroll.bounds().width, listHeight});
            m_choiceScroll.setVisible(scrolls);
            m_choices.syncScrollBar(m_choiceScroll);
            height += listHeight + 8 + 32; // 8: padding under the list; 32: room for the Close button row
        }
        if (m_onClose) {
            m_close.setPosition({(width - closeWidth) / 2, height - 45}); // button row distance from the box bottom
        }
        height += 5; // bottom padding of the box
        m_targetWidth = width;
        m_targetHeight = height;
        const int shownWidth = width + width % 2; // rounded up to an even width
        if (m_timing.compassBox) {
            height -= kCompassShorter;
            m_targetHeight = height;
        }
        const int top = m_timing.compassBox ? kCompassTop : m_timing.movieBottom ? kMovieBottomTop : kTop;
        m_frame.setBounds({(kScreenWidth - width) / 2, top, shownWidth, height});
        m_content.setBounds({0, 0, shownWidth, height});
        if (wideFrame()) {
            core::Rect box = m_timing.movie ? m_movieBoxSpot.rect() : m_wideBoxSpot.rect();
            if (m_wide && m_timing.movieScreen) {
                box.y += kMovieScreenDrop;
            }
            m_targetWidth = box.width;
            m_targetHeight = box.height;
            m_frame.setBounds(box);
            m_content.setBounds({0, 0, box.width, box.height});
        }
        m_choices.setShowHoverBar(!m_timing.compassBox);
        m_choices.setFocusFrame(!m_timing.compassBox);
        m_choices.setAlwaysFrame(!m_timing.compassBox);
        m_choosing = choosing;
        m_openTarget = m_frame.bounds();
        // 8: pixel width of a character
        m_zoom.emplace(
            MessageZoom::Shape{
                kMarginLeft,
                kMarginRight,
                kMarginTop,
                kMarginBottom,
                static_cast<int>(longest) * 8,
                static_cast<int>(m_lines.size()) * kLineHeight
            },
            m_nowMs
        );
        m_ok.setVisible(false);
        m_cancel.setVisible(false);
        m_close.setVisible(false);
        m_choices.setVisible(false);
        m_frame.setVisible(true);
        m_modalCatcher.setVisible(m_timing.modal);
        openStep(m_nowMs);
        if (m_screen.context().bringAbovePopups) {
            m_screen.context().bringAbovePopups(m_layer.id());
        }
        if (m_exclusive && m_timing.exclusive) {
            m_exclusive(true);
        }
    }

    void TalkDialog::openStep(std::uint64_t nowMs) {
        if (!m_zoom) {
            return;
        }
        const core::Rect shown = m_zoom->advance(nowMs, m_openTarget);
        m_frame.setBounds(shown);
        m_content.setBounds({0, 0, shown.width, shown.height});
        if (m_zoom->textVisible()) {
            m_zoom.reset();
            m_frame.setBounds(m_openTarget);
            m_content.setBounds({0, 0, m_openTarget.width, m_openTarget.height});
            m_ok.setVisible(m_onOk != nullptr);
            m_cancel.setVisible(m_onCancel != nullptr);
            m_close.setVisible(m_onClose != nullptr && !m_timing.compassBox);
            m_choices.setVisible(m_choosing);
        }
    }

    void TalkDialog::close() {
        const bool wasExclusive = isOpen() && m_timing.exclusive;
        m_zoom.reset();
        m_frame.setVisible(false);
        m_modalCatcher.setVisible(false);
        m_ok.setVisible(false);
        m_cancel.setVisible(false);
        m_close.setVisible(false);
        m_choices.setVisible(false);
        m_choiceScroll.setVisible(false);
        m_onOk = nullptr;
        m_onCancel = nullptr;
        m_onChoice = nullptr;
        m_onClose = nullptr;
        m_onSaid = nullptr;
        if (wasExclusive && m_exclusive) {
            m_exclusive(false);
        }
    }

    void TalkDialog::finish() { close(); }

    bool TalkDialog::padAsk(PadAsk ask) {
        if (!isOpen()) {
            return false;
        }
        const bool steps = ask == PadAsk::Up || ask == PadAsk::Down || ask == PadAsk::Left || ask == PadAsk::Right;
        if (m_zoom) {
            return !steps; // still opening: there is nothing to answer yet
        }
        if (m_onChoice) {
            switch (ask) {
                case PadAsk::Up:
                case PadAsk::Down:
                    m_choices.stepPadRow(ask == PadAsk::Up ? -1 : 1);
                    m_choices.syncScrollBar(m_choiceScroll);
                    break;
                case PadAsk::Accept:
                    if (m_choices.padRow() >= 0) {
                        pick(m_choices.padRow());
                    } else {
                        m_choices.stepPadRow(1); // nothing marked yet: mark the first entry
                    }
                    break;
                case PadAsk::Cancel:
                    if (m_onClose && m_close.isVisible()) {
                        dismiss();
                    }
                    break;
                default: // Interact: the box is in the way
                    break;
            }
            return true;
        }
        if (steps) {
            if (m_onOk && m_onCancel) { // a question: left and right go between Confirm and Cancel
                if (ask == PadAsk::Left || ask == PadAsk::Right) {
                    m_padNo = ask == PadAsk::Right;
                }
                return true;
            }
            return false;
        }
        if (ask == PadAsk::Interact) {
            return true; // the box is in the way
        }
        if (m_onOk || m_onCancel || m_onClose) {
            if (ask == PadAsk::Accept ? m_onOk != nullptr : m_onCancel != nullptr) {
                answer(ask == PadAsk::Accept && !(m_padNo && m_onCancel != nullptr));
            } else if (m_onClose) {
                dismiss();
            }
            return true;
        }
        clicked(); // words without buttons go on
        return true;
    }

    bool TalkDialog::scriptAnswer(const std::string& what, std::string& result) {
        if (!isReady()) {
            result = isOpen() ? "the box is still opening" : "no talk box is open";
            return false;
        }
        if (!what.empty() && what.size() < 4 && std::all_of(what.begin(), what.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            const int choice = std::stoi(what);
            if (!m_onChoice || choice < 1 || choice > m_choices.itemCount()) {
                result = "the box has " + std::to_string(m_onChoice ? m_choices.itemCount() : 0) + " choice(s)";
                return false;
            }
            result = "choice " + what + " \"" + m_choices.itemText(choice - 1) + "\"";
            pick(choice - 1);
            return true;
        }
        const auto press = [&](ui::ImageButton& button, const std::function<void()>& run) {
            if (!button.isVisible()) {
                result = "the box shows no " + what + " button";
                return false;
            }
            result = what;
            run();
            return true;
        };
        if (what == "Confirm") {
            return press(m_ok, [this] { answer(true); });
        }
        if (what == "Cancel") {
            return press(m_cancel, [this] { answer(false); });
        }
        if (what == "Close") {
            return press(m_close, [this] { dismiss(); });
        }
        if (what == "Next") {
            if (m_onOk || m_onCancel || m_onClose || m_onChoice) {
                result = "the box waits for an answer, not a click";
                return false;
            }
            result = "next";
            clicked();
            return true;
        }
        result = "not an answer: a choice number, Confirm, Cancel, Close or Next";
        return false;
    }

    void TalkDialog::pick(int choice) {
        const std::function<void(int)> handler = m_onChoice;
        finish();
        if (handler && choice >= 0) {
            handler(choice);
        }
    }

    void TalkDialog::dismiss() {
        const std::function<void()> handler = m_onClose;
        finish();
        if (handler) {
            handler();
        }
    }

    void TalkDialog::update(std::uint64_t nowMs) {
        m_nowMs = nowMs;
        if (isOpen()) {
            openStep(nowMs);
        }
        // for a game pad a question shows the answer its accept button takes
        const bool focused = m_padOn && isOpen() && !m_zoom && m_onOk != nullptr && m_onCancel != nullptr && m_ok.isVisible();
        if (focused) {
            m_screen.setPadFocus(m_padNo ? "TalkCancel" : "TalkOk", m_padHand);
        } else if (m_padFocused) {
            m_screen.setPadFocus({});
        }
        m_padFocused = focused;
        if (!isOpen() || m_timing.timeoutMs == 0) {
            return;
        }
        if (m_askedMs == 0) {
            m_askedMs = nowMs;
            return;
        }
        if (nowMs - m_askedMs < m_timing.timeoutMs) {
            return;
        }
        const std::function<void()> handler = m_timing.onTimeout;
        finish();
        if (handler) {
            handler();
        }
    }

    void TalkDialog::answer(bool ok) {
        const std::function<void()> handler = ok ? m_onOk : m_onCancel;
        finish();
        if (handler) {
            handler();
        }
    }

    void TalkDialog::paint(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        if (m_zoom) {
            return;
        }
        const int width = m_targetWidth;
        for (std::size_t row = 0; row < m_lines.size(); ++row) {
            const std::string& line = m_lines[row];
            const int x = (width - static_cast<int>(line.size()) * 8) / 2 + at.x - 3; // 8: pixel width of a character; 3: left nudge
            const int y =
                at.y + (wideFrame() ? kMovieMarginTop : kMarginTop) + 2 + (kLineHeight + kLineSpacing) * static_cast<int>(row); // 2: text baseline nudge
            const bool dark = m_timing.exclusive && m_skin != 1;
            const gfx::TextStyle style = dark ? gfx::TextStyle{m_npcNameSpot.color(), gfx::TextEffect::Plain, std::nullopt}
                                              : gfx::TextStyle{m_textSpot.color(), gfx::TextEffect::Shadow, std::nullopt};
            if (!m_tags || !m_tags->tagged()) {
                canvas.drawText({x, y}, line, style, width - 50 - 60, 20); // text clip width (side margins 50 and 60) and line box height
                continue;
            }
            const gfx::TextStyle red{m_redSpot.color(), gfx::TextEffect::Shadow, std::nullopt};
            for (const ui::MessageTags::Segment& piece : m_tags->segments(m_lines, row)) {
                // 8: pixel width of a character; 50 and 60: side margins of the clip width; 20: line box height
                canvas.drawText({x + static_cast<int>(piece.from) * 8, y}, line.substr(piece.from, piece.length), piece.red ? red : style, width - 50 - 60, 20);
            }
        }
        if (m_speaker) {
            if (m_portrait) {
                paintPortrait(canvas, at, nowMs);
            } else {
                paintSpeaker(canvas, at, nowMs);
            }
            paintNextIcon(canvas, at, nowMs);
        }
        if (m_timing.compassBox && m_timing.shownNumber) {
            if (const int number = m_timing.shownNumber(); number > -1) {
                drawGameNumber(
                    canvas,
                    "Num_White5_1",
                    {m_frame.bounds().x + m_frame.bounds().width / 2, m_targetHeight - kCompassNumberAbove},
                    number,
                    kCompassNumberStyle,
                    true
                );
            }
        }
    }

    void TalkDialog::paintNextIcon(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        if (m_onOk || m_onCancel || m_onClose || m_onChoice) {
            return;
        }
        const auto icon = m_services.pictures.find("Icon_Nextone_1");
        if (!icon) {
            return;
        }
        const gfx::Picture& picture = m_services.pictures.picture(*icon);
        const int height = picture.height() / 2;
        const int frame = static_cast<int>(nowMs / kNextIconStepMs % 2);
        // 22: arrow x offset from the left; 25 and 7: arrow distance from the box bottom
        canvas.drawPicture(*icon, {at.x + 22, at.y + m_targetHeight - 25 - height + 7}, core::Rect{0, frame * height, picture.width(), height});
    }

    void TalkDialog::paintName(gfx::Canvas& canvas, core::Point at) {
        const int nameLength = static_cast<int>(m_speaker->name.size());
        // 4: half a character width (centres the name); 8: character width; 20: line box height
        canvas.drawText(
            {at.x - nameLength * 4, at.y},
            m_speaker->name,
            m_skin == 1 ? gfx::TextStyle{m_nameSpot.color(), gfx::TextEffect::Shadow, std::nullopt}
                        : gfx::TextStyle{m_npcNameSpot.color(), gfx::TextEffect::Plain, std::nullopt},
            nameLength * 8,
            20
        );
    }

    void TalkDialog::paintPortrait(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        const int frame = m_blink.frame(nowMs);
        core::Point face;
        if (std::holds_alternative<CharacterLook>(m_speaker->figure)) {
            const auto& look = std::get<CharacterLook>(m_speaker->figure);
            constexpr int kExpressionFrames[] = {0, 3, 2, 4}; // portrait frame per expression tag value 0..3
            const int expression = m_tags ? m_tags->expression() : 0;
            if (m_wide) {
                const int width = m_services.characters.figureFrameSize(look, kWideFaceSlot).width;
                face = {kWideFaceLeft + width / 2, at.y + m_targetHeight - kWideFaceFromBottom + (m_timing.movieScreen ? kMovieScreenDrop : 0)};
                m_services.characters.drawPortrait(canvas, look, face, 0, kWideFaceSlot);
                if (expression != 0) {
                    m_services.characters.drawPortrait(canvas, look, face, kExpressionFrames[expression], kWideFaceSlot);
                }
                if (m_tags && m_tags->tagged()) {
                    paintName(canvas, {face.x, face.y + kPortraitNameDrop});
                }
                return;
            }
            const int width = m_services.characters.figureFrameSize(look, kPlayerFaceSlot).width;
            face = {at.x + width / 2 + kPlayerFaceFromLeft, at.y + m_faceSpot.at().y};
            m_services.characters.drawPortrait(canvas, look, face, expression != 0 ? kExpressionFrames[expression] : frame, kPlayerFaceSlot);
        } else {
            const auto& npc = std::get<formats::NpcInfo>(m_speaker->figure);
            face = {at.x + m_frame.bounds().width - (m_timing.movie ? kNpcFaceFromRightInMovie : kNpcFaceFromRight), at.y + m_faceSpot.at().y};
            m_services.characters.drawFaceSprite(canvas, npc.faceSpriteId, face, kNpcFaceSlot, frame, CharacterRenderer::npcPattern(npc));
        }
        paintName(canvas, {face.x, face.y + kPortraitNameDrop});
    }

    int TalkDialog::speakerSlot() const {
        if (!m_speaker || !std::holds_alternative<formats::NpcInfo>(m_speaker->figure)) {
            return kPlayerSlot;
        }
        const auto& npc = std::get<formats::NpcInfo>(m_speaker->figure);
        return m_services.characters.npcSlot(npc.spriteId, npc.directionKind, kNpcSlot);
    }

    void TalkDialog::paintSpeaker(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        if (std::holds_alternative<CharacterLook>(m_speaker->figure)) {
            const auto& look = std::get<CharacterLook>(m_speaker->figure);
            const core::Point feet{
                at.x + 30 + m_speakerSize.width / 2,
                at.y + m_speakerSize.height + 20
            }; // 30: figure left margin; 20: feet drop below the figure height
            const int count = m_services.characters.figureFrameCount(look, kPlayerSlot);
            int frame = m_animation.frame(nowMs, count);
            if (const int expression = m_tags ? m_tags->expression() : 0; expression != 0) {
                constexpr int kExpressionFrames[] = {0, 3, 2, 4}; // figure frame per expression tag value 0..3
                frame = std::min(kExpressionFrames[expression], std::max(count - 1, 0));
            }
            m_services.characters.drawFigure(canvas, look, feet, kPlayerSlot, frame);
            paintName(canvas, {feet.x, feet.y + 16}); // name drop below the feet
            return;
        }
        const auto& npc = std::get<formats::NpcInfo>(m_speaker->figure);
        const int slot = speakerSlot();
        // 30: NPC right margin; 20: feet drop below the figure height
        core::Point feet{at.x + m_targetWidth - m_speakerSize.width / 2 - (m_timing.movie ? kMovieFigureFromRight : 30), at.y + m_speakerSize.height + 20};
        const int frame = m_animation.frame(nowMs, m_services.characters.npcFrameCount(npc.spriteId, slot));
        const int drop = npc.weapon == 1 ? kLowFigureDrop - m_speakerSize.height : 0;
        m_services.characters.drawNpc(canvas, npc, {feet.x, feet.y + drop + (m_timing.movie ? kMovieFigureDrop : 0)}, slot, frame);
        if (npc.spriteSize == 1) {
            feet.y += kLargeNpcNameDrop;
        }
        if (m_timing.movie) {
            feet.y += kMovieNameDrop;
        }
        paintName(canvas, {feet.x, feet.y + 16}); // name drop below the feet
    }

}
