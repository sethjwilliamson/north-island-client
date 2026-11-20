#include "client/rendering/hud/PartyPanels.h"

#include "gfx/TextRenderer.h"
#include "ui/TiledFrame.h"
#include "ui/Tooltip.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace client {

    namespace {

        constexpr core::Point kHead{12, 17}; // px: chat-head position within a row
        constexpr core::Point kParts{26, 2}; // px: origin of the level/HP/SP parts within a row
        constexpr core::Point kLevelWord{0, 2}; // px: Lv word, relative to the parts origin
        constexpr core::Point kLevelDigits{17, 2}; // px: level digits, relative to the parts origin
        constexpr core::Point kHpWord{0, 10}; // px: HP word, relative to the parts origin
        constexpr core::Point kHpFrame{13, 11}; // px: HP bar frame, relative to the parts origin
        constexpr core::Point kHpBar{14, 12}; // px: HP bar fill, relative to the parts origin
        constexpr core::Point kSpWord{0, 18}; // px: SP word, relative to the parts origin
        constexpr core::Point kSpFrame{13, 19}; // px: SP bar frame, relative to the parts origin
        constexpr core::Point kSpBar{14, 20}; // px: SP bar fill, relative to the parts origin
        constexpr int kDigitStep = 5; // px: x advance per level digit
        constexpr int kDigitPitch = 6; // px: height of one digit row in the digit picture
        const core::Color kCommanding{255, 230, 90, 255}; // yellow outline of the commanding member
        const core::Color kAttention{255, 70, 70, 255}; // red blinking outline of a member that needs the player
        const core::Color kWindowTag{120, 200, 255, 255}; // light blue outline of another character's window
        const core::Color kLinedWhite{255, 255, 255, 255}; // the login window's lined backing (form_module_1): white lines...
        const core::Color kLinedBlue{231, 243, 255, 255}; // ...between pale blue ones, a pixel each
        constexpr std::array<int, 3> kRound{3, 2, 1}; // px left off the lines nearest a rounded corner, from the corner's line on
        const core::Color kLink{98, 135, 163, 255}; // grey blue connector of a group
        constexpr int kLinkX = 10; // px: x of a group's connector in a line (under the middle of the head's chat head, x 1..20)
        constexpr int kTickY = 15; // px: y in a line where the connector turns to a member's row (the middle of its chat head, y 7..25)
        constexpr int kTickEnd = 1; // px: x past a member's row box where the turn ends (at its chat head's left edge)
        constexpr int kLinkWide = 2; // px: the connector's thickness
        // the member's name and state lines in the chat's and the hints' look: shadowed text
        const gfx::TextStyle kState{{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}; // white text with shadow
        const gfx::TextStyle kGone{{255, 120, 120, 255}, gfx::TextEffect::Shadow, std::nullopt}; // red text with shadow
        const std::string kLoginHint = "Log in another account"; // the login button's hint
        const char* const kLoginPicture = "Btn_Login"; // the character select's Login button (CharacterSelectForm.xml, 56 x 20)

        /// @brief Scales a length in the rows' coordinates to the screen.
        /// @param length The length, px at scale 1.
        /// @param scale The rows' scale.
        /// @return The length on the screen, px.
        int scaled(int length, float scale) { return static_cast<int>(std::lround(length * scale)); }

        /// @brief Fill ratio of a pet-style bar: Ceil(value / most * 100) %, at most 100.
        /// @param value Current value.
        /// @param most Maximum value.
        /// @return Ratio from 0 to 1.
        double barRatio(double value, double most) {
            if (most <= 0) {
                return 0;
            }
            return std::clamp(std::ceil(value / most * 100.0), 0.0, 100.0) / 100.0; // ratio to percent and back
        }

        /// @brief Tells whether a member is in the world or in a battle (its pet-style row can show).
        /// @param member The member.
        /// @return True when in the world or in battle.
        bool inWorld(const PartyMemberView& member) {
            return member.state == PartyMemberView::State::InWorld || member.state == PartyMemberView::State::InBattle;
        }

        /// @brief Tells whether a box on the screen clears the chat channel list (ends above its top).
        /// @param block The box on the screen.
        /// @param chatTop The chat channel list's top on the screen.
        /// @return True when clear.
        bool blockClear(const core::Rect& block, int chatTop) { return block.bottom() <= chatTop; }

    }

    std::optional<PartyPanels::Placement> PartyPanels::place(const core::Rect& view, float hudScale, bool battle, bool riding, const Sizes& sizes) {
        const float hud = hudScale > 0 ? hudScale : 1.0f;
        const int chatTop = view.bottom() - scaled(kChatUp, hud);
        const int statusBottom = view.y + scaled(kStatusBottom, hud);
        const core::Rect block = blockBox(kShownRows, sizes);
        // the lined backing's top at top: the panel's kLined under it, the rows' origin kPadTop under that
        const auto at = [&view](int top, float scale) { return Placement{{view.x + scaled(kLeft, scale), top + scaled(kLined + kPadTop, scale)}, scale}; };
        // the scales from most down to least: most less whole kScaleStep steps, and 1 and least themselves (most need not be
        // a step multiple away from them, a HUD scale of 110% say)
        std::vector<float> scales{kLeastScale};
        if (kLeastScale <= 1.0f && 1.0f <= hud) {
            scales.push_back(1.0f);
        }
        const int steps = static_cast<int>(std::floor((hud - kLeastScale) / kScaleStep));
        for (int step = 0; step <= steps; ++step) {
            scales.push_back(hud - static_cast<float>(step) * kScaleStep);
        }
        std::sort(scales.begin(), scales.end(), std::greater<>());
        if (riding && !battle) {
            // the vehicle gauge shows: the panel stays under it, smaller down to kLeastScale, or is not shown at all
            const int gaugeBottom = view.y + scaled(kWorldTop, hud);
            for (const float scale : scales) {
                if (fits(kShownRows, sizes, at(gaugeBottom, scale), chatTop)) {
                    return at(gaugeBottom, scale);
                }
            }
            return std::nullopt;
        }
        // in the world and in battle alike: its bottom on the chat channel list, at the largest scale whose top clears the status
        for (const float scale : scales) {
            if (const int top = chatTop - scaled(block.height, scale); top >= statusBottom) {
                return at(top, scale);
            }
        }
        return at(statusBottom, kLeastScale);
    }

    core::Rect PartyPanels::panelBox(int rows) {
        const int left = -kLeft - kPanelOut;
        const int bottom = rowBox(rows - 1).bottom() + kPadBottom;
        return {left, -kPadTop, kPanelRight - left, bottom + kPadTop};
    }

    core::Rect PartyPanels::backBox(int rows) {
        const core::Rect panel = panelBox(rows);
        return {panel.x, panel.y - kLined, panel.width + kLined, panel.height + 2 * kLined};
    }

    core::Rect PartyPanels::collapseTab(const Sizes& sizes) {
        const int right = kPanelRight + kArrowGap + sizes.arrowWidth + kLined;
        return {kPanelRight, -kPadTop - kLined, right - kPanelRight, sizes.arrowHeight + 2 * kLined};
    }

    core::Rect PartyPanels::expandTab() const {
        const int left = -kLeft - kPanelOut;
        const core::Rect arrow = expandBox();
        return {left, arrow.y - kLined, arrow.right() + kLined - left, arrow.height + 2 * kLined};
    }

    core::Rect PartyPanels::loginBox(const Sizes& sizes) {
        const core::Rect back = backBox(kShownRows);
        return {kPanelRight - sizes.loginWidth, back.bottom() + kLined, sizes.loginWidth, sizes.loginHeight};
    }

    core::Rect PartyPanels::loginTab(const Sizes& sizes) {
        // from the backing's top (its lines go on the same) to kLined under the button, kLined either side of it
        const core::Rect back = backBox(kShownRows);
        const core::Rect button = loginBox(sizes);
        return {button.x - kLined, back.y, button.width + 2 * kLined, button.bottom() + kLined - back.y};
    }

    core::Rect PartyPanels::blockBox(int rows, const Sizes& sizes) {
        const core::Rect back = backBox(rows);
        const core::Rect tab = collapseTab(sizes);
        const core::Rect login = loginTab(sizes);
        return {back.x, back.y, tab.right() - back.x, std::max({back.bottom(), tab.bottom(), login.bottom()}) - back.y};
    }

    bool PartyPanels::onBlock(core::Point at) const {
        return backBox(m_shown).contains(at) || collapseTab(m_sizes).contains(at) || (m_loginShown && loginTab(m_sizes).contains(at));
    }

    void PartyPanels::drawLined(gfx::Canvas& canvas, const core::Rect& box, bool roundTop, bool roundBottom, bool roundLeft) {
        // the login window's white lines, a white one first; the right end's corners rounded as asked (and the left end's, less)
        const auto cutAt = [](int fromCorner) { return fromCorner < static_cast<int>(kRound.size()) ? kRound[static_cast<std::size_t>(fromCorner)] : 0; };
        for (int row = 0; row < box.height; ++row) {
            const int cut = std::max(roundTop ? cutAt(row) : 0, roundBottom ? cutAt(box.height - 1 - row) : 0);
            const int leftCut = roundLeft ? std::max(0, cut - 1) : 0; // a pixel less round than the right end (as drawn in the design)
            canvas.fillRect({box.x + leftCut, box.y + row, box.width - cut - leftCut, 1}, row % 2 == 0 ? kLinedWhite : kLinedBlue);
        }
    }

    bool PartyPanels::fits(int rows, const Sizes& sizes, const Placement& placement, int chatTop) {
        return blockClear(onScreen(blockBox(rows, sizes), placement), chatTop);
    }

    core::Rect PartyPanels::onScreen(const core::Rect& box, const Placement& placement) {
        return {
            placement.origin.x + scaled(box.x, placement.scale),
            placement.origin.y + scaled(box.y, placement.scale),
            scaled(box.width, placement.scale),
            scaled(box.height, placement.scale)
        };
    }

    PartyPanels::Sizes PartyPanels::sizes() const {
        Sizes sizes;
        if (const auto arrow = m_pictures.find("Btn_ArrowL_8")) {
            // square looks stacked down the picture (21 x 21 each, as the monster book's ImageButtons size them)
            const gfx::Picture& picture = m_pictures.picture(*arrow);
            sizes.arrowWidth = picture.width();
            sizes.arrowHeight = std::min(picture.width(), picture.height());
        }
        if (const auto login = m_pictures.find(kLoginPicture)) {
            sizes.loginWidth = m_pictures.picture(*login).width();
        }
        if (const auto rail = m_pictures.find("Rail_H5")) {
            sizes.barWidth = m_pictures.picture(*rail).width();
        }
        return sizes;
    }

    core::Rect PartyPanels::barBox() const {
        const core::Rect panel = panelBox(std::max(1, m_shown));
        return {kPanelRight - kBarInset - m_sizes.barWidth, panel.y + kBarInset, m_sizes.barWidth, panel.height - 2 * kBarInset};
    }

    std::pair<int, int> PartyPanels::thumb() const {
        // as the server list's ScrollBar: the thumb as long as the page is of all the lines, at least two caps
        constexpr int kThumbCap = 7; // px: bar_H4's top and bottom caps (ThumbCap)
        const int height = barBox().height;
        const int lines = static_cast<int>(m_lines.size());
        const int most = std::max(lines, m_shown);
        const int size = std::max(most > 0 ? static_cast<int>(static_cast<double>(m_shown) / most * height) : height, kThumbCap * 2);
        const int top = most > m_shown ? static_cast<int>(static_cast<double>(m_scroll) / (most - m_shown) * (height - size)) : 0;
        return {top, size};
    }

    core::Rect PartyPanels::collapseBox() const { return {kPanelRight + kArrowGap, -kPadTop, m_sizes.arrowWidth, m_sizes.arrowHeight}; }

    core::Rect PartyPanels::expandBox() const { return {-kLeft + 2, -kPadTop, m_sizes.arrowWidth, m_sizes.arrowHeight}; }

    bool PartyPanels::press(core::Point point, bool act) {
        if (!m_placed || (m_slide > 0 && m_slide < 1)) {
            return false; // sliding: nothing to press
        }
        const core::Point at = toRows(point);
        if (!m_open) {
            if (!expandTab().contains(at)) {
                return false;
            }
            if (act) {
                m_open = true;
                m_arrowPressed = true;
            }
            return true;
        }
        if (collapseTab(m_sizes).contains(at)) {
            if (act) {
                m_open = false;
                m_arrowPressed = true;
                m_dragging = false;
            }
            return true;
        }
        if (!onBlock(at)) {
            return false;
        }
        const core::Rect bar = barBox();
        if (act && bar.contains(at) && static_cast<int>(m_lines.size()) > m_shown) {
            const auto [top, size] = thumb();
            const int y = at.y - bar.y;
            if (y < top) {
                scrollBy(-m_shown); // a page up
            } else if (y >= top + size) {
                scrollBy(m_shown); // a page down
            } else {
                m_dragging = true;
                m_dragFromY = at.y;
                m_dragFromScroll = m_scroll;
            }
        }
        return true;
    }

    void PartyPanels::drag(core::Point point) {
        if (!m_dragging) {
            return;
        }
        const auto [top, size] = thumb();
        (void)top;
        const int room = barBox().height - size;
        const int hidden = static_cast<int>(m_lines.size()) - m_shown;
        if (room <= 0 || hidden <= 0) {
            return;
        }
        const int moved = toRows(point).y - m_dragFromY;
        m_scroll = std::clamp(m_dragFromScroll + static_cast<int>(std::lround(static_cast<double>(moved) / room * hidden)), 0, hidden);
    }

    void PartyPanels::release() {
        m_dragging = false;
        m_arrowPressed = false;
    }

    bool PartyPanels::panelAt(core::Point point) const { return m_placed && m_open && m_slide >= 1 && onBlock(toRows(point)); }

    void PartyPanels::scrollBy(int lines) {
        m_scroll = std::clamp(m_scroll + lines, 0, std::max(0, static_cast<int>(m_lines.size()) - m_shown));
    }

    void PartyPanels::drawButton(gfx::Canvas& canvas, const char* name, core::Point at, int height, int state) {
        if (const auto button = m_pictures.find(name)) {
            const gfx::Picture& picture = m_pictures.picture(*button);
            const int frames = std::max(1, picture.height() / height);
            canvas.drawPicture(*button, at, core::Rect{0, std::min(state, frames - 1) * height, picture.width(), height});
        }
    }

    void PartyPanels::drawBar(gfx::Canvas& canvas, const core::Rect& box, int state) {
        // the server list's bar (ServerSelectForm.xml): Track="Rail_H5" TrackCap="16" Thumb="bar_H4" ThumbSource="0,0,9,21"
        // ThumbCap="7" ThumbInset="3", the track's caps whole and its middle repeated, the thumb's likewise
        constexpr int kTrackCap = 16;
        constexpr core::Rect kThumbSource{0, 0, 9, 21};
        constexpr int kThumbCap = 7;
        constexpr int kThumbInset = 3;
        if (const auto rail = m_pictures.find("Rail_H5")) {
            const gfx::Picture& track = m_pictures.picture(*rail);
            const int cap = std::min(kTrackCap, box.height / 2);
            canvas.drawPicture(*rail, box.position(), core::Rect{0, 0, track.width(), cap});
            canvas.drawPicture(*rail, {box.x, box.bottom() - cap}, core::Rect{0, track.height() - cap, track.width(), cap});
            const int middleSource = track.height() - kTrackCap * 2;
            const int middle = box.height - cap * 2;
            for (int y = 0; middleSource > 0 && y < middle; y += middleSource) {
                canvas.drawPicture(*rail, {box.x, box.y + cap + y}, core::Rect{0, kTrackCap, track.width(), std::min(middleSource, middle - y)});
            }
        }
        const auto bar = m_pictures.find("bar_H4");
        if (!bar) {
            return;
        }
        const auto [top, size] = thumb();
        const int sourceX = kThumbSource.x + state * kThumbSource.width;
        const int x = box.x + (box.width - kThumbSource.width) / 2;
        const int y = box.y + top;
        canvas.drawPicture(*bar, {x, y + kThumbInset}, core::Rect{sourceX, kThumbSource.y, kThumbSource.width, kThumbCap});
        const int middleSource = kThumbSource.height - kThumbCap * 2;
        const int middle = size - kThumbCap * 2;
        for (int part = 0; middleSource > 0 && part < middle; part += middleSource) {
            canvas.drawPicture(*bar, {x, y + kThumbCap + part}, core::Rect{sourceX, kThumbSource.y + kThumbCap, kThumbSource.width, std::min(middleSource, middle - part)});
        }
        canvas.drawPicture(
            *bar,
            {x, y + size - kThumbCap - kThumbInset},
            core::Rect{sourceX, kThumbSource.y + kThumbSource.height - kThumbCap, kThumbSource.width, kThumbCap}
        );
    }

    void PartyPanels::draw(
        gfx::Canvas& canvas,
        const std::vector<PartyMemberView>& members,
        bool loginRow,
        bool battle,
        bool riding,
        const core::Rect& view,
        float hudScale,
        std::optional<core::Point> mouse,
        std::uint64_t nowMs,
        const std::function<bool(const core::Rect& box)>& covered
    ) {
        // the slide toward out or out of sight, kSlideMs for the whole way
        if (m_slideMs != 0 && nowMs > m_slideMs) {
            const float step = static_cast<float>(nowMs - m_slideMs) / static_cast<float>(kSlideMs);
            m_slide = m_open ? std::min(1.0f, m_slide + step) : std::max(0.0f, m_slide - step);
        }
        m_slideMs = nowMs;
        m_lines = order(members);
        m_loginShown = loginRow;
        m_sizes = sizes();
        const int lineCount = static_cast<int>(m_lines.size());
        const std::optional<Placement> placed = place(view, hudScale, battle, riding, m_sizes);
        m_placed = placed.has_value();
        if (!placed) {
            m_dragging = false;
            return;
        }
        m_place = *placed;
        m_shown = kShownRows; // the panel's size whatever the count: more lines scroll
        scrollBy(0); // kept within the rows
        const bool blinkOn = nowMs / kBlinkMs % 2 == 0; // on for kBlinkMs, then off as long
        const core::Point mouseAt = mouse ? toRows(*mouse) : core::Point{-100000, -100000};
        const auto calling = [&](int from, int to) {
            for (int index = std::max(0, from); index < std::min(to, lineCount); ++index) {
                const int member = m_lines[static_cast<std::size_t>(index)].member;
                if (member >= 0 && members[static_cast<std::size_t>(member)].attention) {
                    return true;
                }
            }
            return false;
        };
        const auto arrowState = [&](const core::Rect& box) { return box.contains(mouseAt) ? (m_arrowPressed ? 2 : 1) : 0; };
        if (m_slide <= 0) {
            // collapsed: only the arrow that brings the panel back
            const core::Rect box = expandBox();
            if (covered && covered(onScreen(expandTab(), m_place))) {
                m_placed = false;
                return;
            }
            canvas.beginScale(m_place.origin, m_place.scale);
            drawLined(canvas, expandTab().translated(m_place.origin), true, true);
            const core::Rect drawn = box.translated(m_place.origin);
            drawButton(canvas, "Btn_ArrowR_8", drawn.position(), m_sizes.arrowHeight, arrowState(box));
            if (blinkOn && calling(0, lineCount)) {
                canvas.drawRectOutline({drawn.x - 1, drawn.y - 1, drawn.width + 2, drawn.height + 2}, kAttention, 2);
            }
            canvas.endScale();
            return;
        }
        if (covered && covered(onScreen(blockBox(m_shown, m_sizes), m_place))) {
            // a window or a popup lies over the panel: it is left out (not drawn, not hit)
            m_placed = false;
            m_dragging = false;
            return;
        }
        // sliding: the panel and its arrow moved left by the part not out yet (out of sight past the view's left edge)
        const core::Rect block = blockBox(m_shown, m_sizes);
        const int away = static_cast<int>(std::lround((1.0f - m_slide) * static_cast<float>(block.right())));
        const core::Point shift = m_place.origin - core::Point{away, 0};
        const bool out = m_slide >= 1;
        canvas.beginScale(m_place.origin, m_place.scale);
        drawLined(canvas, backBox(m_shown).translated(shift), false, false); // its top right goes on into the arrow's tab
        drawLined(canvas, collapseTab(m_sizes).translated(shift), true, true);
        if (m_loginShown) {
            drawLined(canvas, loginTab(m_sizes).translated(shift), false, true, true); // its bottom corners round, its top in the backing
        }
        if (const auto skin = m_pictures.find("panel10")) {
            // the server list's box: panel10, SkinSource 0,0,50,50, Borders 7,7,7,7
            ui::TiledFrame(*skin, core::Rect{0, 0, 50, 50}, 7, 7, 7, 7).draw(canvas, panelBox(m_shown).translated(shift));
        }
        const core::Rect bar = barBox();
        drawBar(canvas, bar.translated(shift), m_dragging ? 2 : (out && bar.contains(mouseAt) ? 1 : 0));
        if (lineCount > m_shown && blinkOn) {
            // a row out of sight that asks for attention: the bar above or below the thumb blinks
            const auto [top, size] = thumb();
            const core::Rect drawnBar = bar.translated(shift);
            if (calling(0, m_scroll)) {
                canvas.drawRectOutline({drawnBar.x, drawnBar.y, drawnBar.width, std::max(top, 4)}, kAttention, 2);
            }
            if (calling(m_scroll + m_shown, lineCount)) {
                const int below = std::max(drawnBar.height - top - size, 4);
                canvas.drawRectOutline({drawnBar.x, drawnBar.bottom() - below, drawnBar.width, below}, kAttention, 2);
            }
        }
        drawButton(canvas, "Btn_ArrowL_8", collapseBox().translated(shift).position(), m_sizes.arrowHeight, out ? arrowState(collapseBox()) : 0);
        const bool onLogin = out && mouse && loginRowAt(*mouse);
        if (m_loginShown) {
            // normal or mouse over only: it opens the login window on the press, which then takes the release
            const bool overLogin = out && loginBox(m_sizes).contains(mouseAt);
            drawButton(canvas, kLoginPicture, loginBox(m_sizes).translated(shift).position(), m_sizes.loginHeight, overLogin ? 1 : 0);
        }
        for (int slot = 0; slot < m_shown && m_scroll + slot < lineCount; ++slot) {
            const Line& line = m_lines[static_cast<std::size_t>(m_scroll + slot)];
            const core::Point at = shift + core::Point{0, slot * kRowHeight};
            const core::Point rowAtPoint = at + core::Point{line.indented ? kIndent : 0, 0};
            drawLink(canvas, line, at);
            const PartyMemberView& member = members[static_cast<std::size_t>(line.member)];
            const core::Rect area = rowBox(0).translated(rowAtPoint);
            drawRow(canvas, member, rowAtPoint);
            if (member.attention && blinkOn) {
                canvas.drawRectOutline({area.x - 1, area.y - 1, area.width + 2, area.height + 2}, kAttention, 2); // around the yellow one, 2 px thick
            }
            if (member.commanding) {
                canvas.drawRectOutline(area, kCommanding, 1);
            }
        }
        canvas.endScale();
        if (!out || !mouse) {
            return;
        }
        const int hovered = memberAt(*mouse);
        if (hovered >= 0 && inWorld(members[static_cast<std::size_t>(hovered)])) {
            ui::drawHintBox(canvas, *mouse, members[static_cast<std::size_t>(hovered)].name);
        } else if (onLogin) {
            ui::drawHintBox(canvas, *mouse, kLoginHint);
        }
    }

    core::Rect PartyPanels::rowBox(int row) { return {-2, row * kRowHeight - 1, kRowWidth, kRowHeight - 2}; }

    core::Point PartyPanels::toRows(core::Point point) const {
        return {
            static_cast<int>(std::floor((point.x - m_place.origin.x) / m_place.scale)),
            static_cast<int>(std::floor((point.y - m_place.origin.y) / m_place.scale))
        };
    }

    core::Rect PartyPanels::slotBox(int slot) const {
        const auto line = static_cast<std::size_t>(m_scroll + slot);
        const bool indented = line < m_lines.size() && m_lines[line].indented;
        return rowBox(slot).translated({indented ? kIndent : 0, 0});
    }

    int PartyPanels::rowAt(core::Point point) const {
        if (!m_placed || !m_open || m_slide < 1) {
            return -1;
        }
        const core::Point at = toRows(point);
        for (int slot = 0; slot < m_shown && m_scroll + slot < static_cast<int>(m_lines.size()); ++slot) {
            if (slotBox(slot).contains(at)) {
                return m_scroll + slot;
            }
        }
        return -1;
    }

    int PartyPanels::memberAt(core::Point point) const {
        const int line = rowAt(point);
        return line >= 0 ? m_lines[static_cast<std::size_t>(line)].member : -1;
    }

    bool PartyPanels::loginRowAt(core::Point point) const {
        return m_placed && m_open && m_slide >= 1 && m_loginShown && loginBox(m_sizes).contains(toRows(point));
    }

    std::vector<PartyPanels::Line> PartyPanels::order(const std::vector<PartyMemberView>& members) {
        std::vector<Line> lines;
        std::vector<bool> placed(members.size(), false);
        // a group: its leader heading it when among them, else the first of them; the others under it
        const auto addGroup = [&](std::vector<int> team, std::int32_t leaderId) {
            const auto leader = std::find_if(team.begin(), team.end(), [&](int row) { return members[static_cast<std::size_t>(row)].id == leaderId; });
            const int head = leader != team.end() ? *leader : team.front();
            Line top{head};
            top.head = true;
            top.linkDown = true;
            lines.push_back(top);
            placed[static_cast<std::size_t>(head)] = true;
            team.erase(std::remove(team.begin(), team.end(), head), team.end());
            for (std::size_t k = 0; k < team.size(); ++k) {
                Line line{team[k]};
                line.indented = true;
                line.linkUp = true;
                line.linkDown = k + 1 < team.size();
                lines.push_back(line);
                placed[static_cast<std::size_t>(team[k])] = true;
            }
        };
        // the played character first: its team's group (its own row heading it unless an own character leads, then its mates),
        // else its own row alone
        const auto played = std::find_if(members.begin(), members.end(), [](const PartyMemberView& member) { return member.played; });
        if (played != members.end()) {
            const auto self = static_cast<int>(played - members.begin());
            placed[static_cast<std::size_t>(self)] = true;
            std::vector<int> team{self};
            for (std::size_t index = 0; index < members.size(); ++index) {
                if (!members[index].played && inWorld(members[index]) && members[index].inTeam) {
                    team.push_back(static_cast<int>(index));
                }
            }
            if (played->inTeam && team.size() >= 2) {
                addGroup(team, played->leaderId);
            } else {
                lines.push_back({self});
            }
        }
        // the others in login order, a team of own characters together where its first member comes
        for (std::size_t index = 0; index < members.size(); ++index) {
            if (placed[index]) {
                continue;
            }
            const PartyMemberView& member = members[index];
            std::vector<int> team;
            if (inWorld(member) && member.leaderId != 0) {
                for (std::size_t other = index; other < members.size(); ++other) {
                    if (!placed[other] && inWorld(members[other]) && members[other].leaderId == member.leaderId) {
                        team.push_back(static_cast<int>(other));
                    }
                }
            }
            if (team.size() < 2) {
                lines.push_back({static_cast<int>(index)});
                placed[index] = true;
                continue;
            }
            addGroup(team, member.leaderId);
        }
        return lines;
    }

    void PartyPanels::drawLink(gfx::Canvas& canvas, const Line& line, core::Point at) {
        const int x = at.x + kLinkX;
        const int top = at.y + rowBox(0).y - 2; // through the gap from the line above
        const int bottom = at.y + rowBox(0).bottom(); // to this line's box bottom
        const int tick = at.y + kTickY; // where it turns to the row
        if (line.head) {
            constexpr int kUnderHead = 26; // px: the connector starts under the head's chat head
            canvas.fillRect({x, at.y + kUnderHead, kLinkWide, bottom - (at.y + kUnderHead)}, kLink);
        }
        if (line.linkUp) {
            canvas.fillRect({x, top, kLinkWide, tick + kLinkWide - top}, kLink);
        }
        if (line.linkDown && !line.head) {
            canvas.fillRect({x, tick, kLinkWide, bottom - tick}, kLink);
        }
        if (line.indented) {
            canvas.fillRect({x, tick, at.x + kIndent + kTickEnd - x, kLinkWide}, kLink);
        }
    }

    void PartyPanels::drawRow(gfx::Canvas& canvas, const PartyMemberView& member, core::Point at) {
        const auto draw = [&](const char* name, core::Point offset) {
            if (const auto id = m_pictures.find(name)) {
                canvas.drawPicture(*id, at + kParts + offset);
            }
        };
        const auto bar = [&](const char* name, core::Point offset, double ratio) {
            const auto id = m_pictures.find(name);
            if (!id) {
                return;
            }
            const int width = static_cast<int>(m_pictures.picture(*id).width() * ratio);
            if (width > 0) {
                canvas.drawPicture(*id, at + kParts + offset, core::Rect{0, 0, width, m_pictures.picture(*id).height()});
            }
        };
        if (!inWorld(member)) {
            const bool loggingIn = member.state == PartyMemberView::State::LoggingIn;
            canvas.drawText({at.x, at.y + 2}, member.name.empty() ? member.account : member.name, kState); // name line, 2 pixels down
            canvas.drawText({at.x, at.y + 16}, loggingIn ? "logging in..." : member.reason, loggingIn ? kState : kGone); // state line, 16 pixels down
            return;
        }
        if (member.look) {
            m_characters.drawChatHead(canvas, *member.look, at + kHead, true);
        }
        draw("Word_Lv_1", kLevelWord);
        if (const auto digits = m_pictures.find("Pet_Num_Lv_White_1")) {
            const gfx::Picture& picture = m_pictures.picture(*digits);
            const std::string text = std::to_string(member.level);
            for (std::size_t i = 0; i < text.size(); ++i) {
                const core::Point place = at + kParts + kLevelDigits;
                canvas.drawPicture(
                    *digits,
                    {place.x + static_cast<int>(i) * kDigitStep, place.y},
                    core::Rect{0, (text[i] - '0') * kDigitPitch, picture.width(), picture.height() / 10}
                ); // the picture is a column of ten digits
            }
        }
        draw("Word_Hp_1", kHpWord);
        draw("Pet_Frame_Hp", kHpFrame);
        bar("Pet_Bmp_Hp", kHpBar, barRatio(member.hp, member.maxHp));
        draw("Word_Sp_1", kSpWord);
        draw("Pet_Frame_Sp", kSpFrame);
        bar("Pet_Bmp_Sp", kSpBar, barRatio(member.sp, member.maxSp));
    }

    void PartyPanels::drawWindowTag(gfx::Canvas& canvas, const core::Rect& window, const std::string& name) {
        canvas.drawRectOutline(window, kWindowTag, 2); // outline thickness in pixels
        ui::drawHintBox(canvas, window.position(), name);
    }

}
