#include "client/rendering/hud/SimplePartyPanels.h"

#include "gfx/TextRenderer.h"
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
        const core::Color kLoginBack{0, 0, 0, 110}; // the "+" row's backing: black at 110/255 strength
        const core::Color kLoginEdge{170, 170, 170, 255}; // grey outline of the "+" row
        const core::Color kLoginBackHovered{255, 255, 255, 50}; // the "+" row's backing under the mouse: white at 50/255 strength
        const core::Color kLoginEdgeHovered{255, 255, 255, 255}; // white outline of the "+" row under the mouse
        const std::string kLoginLabel = "+ Log in"; // the "+" row's label, centred in its box
        // the member's name and state lines in the chat's and the hints' look: shadowed text
        const gfx::TextStyle kState{{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}; // white text with shadow
        const gfx::TextStyle kGone{{255, 120, 120, 255}, gfx::TextEffect::Shadow, std::nullopt}; // red text with shadow
        const gfx::TextStyle kPlusText{{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}; // white text with shadow
        const std::string kLoginHint = "Log in another account"; // the "+" row's hint
        // the left side's back line (BattleField, row 1): the fighters' feet, px of the game screen
        constexpr std::array<core::Point, 4> kBackLineFeet{{{210, 266}, {185, 329}, {160, 392}, {135, 455}}};
        constexpr core::Point kRingReach{68, 96}; // px the command ring reaches left of and above a fighter's feet (BattleMainForm kHomes)
        constexpr int kRingBelow = 39; // px the ring reaches below a fighter's feet: its lowest buttons 11 px down, 28 px high

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

    }

    std::optional<SimplePartyPanels::Placement> SimplePartyPanels::place(const core::Rect& view, float hudScale, bool battle, bool riding) {
        const float hud = hudScale > 0 ? hudScale : 1.0f;
        const int chatTop = view.bottom() - scaled(kChatUp, hud);
        const int statusBottom = view.y + scaled(kStatusBottom, hud);
        const auto at = [&view](int top, float scale) { return Placement{{view.x + scaled(kLeft, scale), top}, scale}; };
        const auto clear = [&](const Placement& placement) {
            // the box of kMostRows rows on the screen
            const core::Rect block{
                placement.origin.x + scaled(rowBox(0).x, placement.scale),
                placement.origin.y + scaled(rowBox(0).y, placement.scale),
                scaled(kRowWidth, placement.scale),
                scaled(kBlockHeight, placement.scale)
            };
            if (block.bottom() > chatTop) {
                return false;
            }
            return !battle || std::none_of(kBackLineFeet.begin(), kBackLineFeet.end(), [&block](core::Point feet) {
                       const core::Rect ring{feet.x - kRingReach.x, feet.y - kRingReach.y, 2 * kRingReach.x, kRingReach.y + kRingBelow};
                       return !block.intersected(ring).isEmpty();
                   });
        };
        // the largest scale from most down to least at which the rows fit with their first row at top: most less whole kScaleStep
        // steps, and 1 and least themselves (most need not be a step multiple away from them, a HUD scale of 110% say)
        const auto fit = [&](int top, float most, float least) -> std::optional<Placement> {
            if (most < least) {
                return std::nullopt;
            }
            std::vector<float> scales{least};
            if (least <= 1.0f && 1.0f <= most) {
                scales.push_back(1.0f);
            }
            const int steps = static_cast<int>(std::floor((most - least) / kScaleStep));
            for (int step = 0; step <= steps; ++step) {
                scales.push_back(most - static_cast<float>(step) * kScaleStep);
            }
            std::sort(scales.begin(), scales.end(), std::greater<>());
            for (const float scale : scales) {
                if (clear(at(top, scale))) {
                    return at(top, scale);
                }
            }
            return std::nullopt;
        };
        if (battle) {
            // the battle form's own status lies at the game screen's fixed coordinates, not HUD-scaled
            return fit(kBattleTop, 1.0f, kLeastScale).value_or(at(kBattleTop, kLeastScale));
        }
        const int gaugeBottom = view.y + scaled(kWorldTop, hud);
        if (const auto underGauge = fit(gaugeBottom, hud, 1.0f)) {
            return *underGauge;
        }
        if (riding) {
            // the vehicle gauge shows: the rows stay under it, smaller down to kLeastScale, or are not shown at all
            return fit(gaugeBottom, 1.0f, kLeastScale);
        }
        // at scale 1 the rows' box reaches kBlockHeight - 1 px below the first row's origin (its box starts 1 px above it)
        if (const int top = chatTop - (kBlockHeight + rowBox(0).y); top >= statusBottom) {
            return at(top, 1.0f);
        }
        return fit(statusBottom, 1.0f, kLeastScale).value_or(at(statusBottom, kLeastScale));
    }

    core::Rect SimplePartyPanels::rowBox(int row) { return {-2, row * kRowHeight - 1, kRowWidth, kRowHeight - 2}; }

    core::Point SimplePartyPanels::toRows(core::Point point) const {
        return {
            static_cast<int>(std::floor((point.x - m_place.origin.x) / m_place.scale)),
            static_cast<int>(std::floor((point.y - m_place.origin.y) / m_place.scale))
        };
    }

    core::Rect SimplePartyPanels::screenBox(int row) const {
        const core::Rect box = rowBox(row);
        const core::Point topLeft = m_place.origin + core::Point{scaled(box.x, m_place.scale), scaled(box.y, m_place.scale)};
        return {topLeft.x, topLeft.y, scaled(box.width, m_place.scale), scaled(box.height, m_place.scale)};
    }

    int SimplePartyPanels::rowAt(core::Point point) const {
        const core::Point at = toRows(point);
        for (int row = 0; row < static_cast<int>(m_covered.size()); ++row) {
            if (!m_covered[static_cast<std::size_t>(row)] && rowBox(row).contains(at)) {
                return row;
            }
        }
        return -1;
    }

    int SimplePartyPanels::memberAt(core::Point point) const {
        const int row = rowAt(point);
        return row < m_memberRows ? row : -1;
    }

    bool SimplePartyPanels::loginRowAt(core::Point point) const { return m_loginRow && rowAt(point) == m_memberRows; }

    void SimplePartyPanels::drawLoginRow(gfx::Canvas& canvas, core::Point at, bool hovered) {
        const core::Rect area = rowBox(0).translated(at);
        canvas.fillRectAlpha(area, kLoginBack);
        if (hovered) {
            canvas.fillRectAlpha(area, kLoginBackHovered);
        }
        canvas.drawRectOutline(area, hovered ? kLoginEdgeHovered : kLoginEdge, 1);
        const int width = gfx::TextRenderer::textWidth(kLoginLabel, kPlusText.effect);
        const core::Point label{
            area.x + (area.width - width) / 2, // centred across the box
            area.y + (area.height - gfx::TextRenderer::kLineHeight) / 2 // centred down the box
        };
        canvas.drawText(label, kLoginLabel, kPlusText);
    }

    void SimplePartyPanels::drawRow(gfx::Canvas& canvas, const PartyMemberView& member, core::Point at) {
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

    void SimplePartyPanels::draw(
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
        const std::optional<Placement> placed = place(view, hudScale, battle, riding);
        m_memberRows = static_cast<int>(members.size());
        m_loginRow = loginRow;
        // no room for the rows: every row is left out, as under a window
        m_covered.assign(static_cast<std::size_t>(m_memberRows + (loginRow ? 1 : 0)), !placed);
        if (!placed) {
            return;
        }
        m_place = *placed;
        for (int row = 0; row < static_cast<int>(m_covered.size()); ++row) {
            m_covered[static_cast<std::size_t>(row)] = covered && covered(screenBox(row));
        }
        const bool blinkOn = nowMs / kBlinkMs % 2 == 0; // on for kBlinkMs, then off as long
        canvas.beginScale(m_place.origin, m_place.scale);
        for (std::size_t index = 0; index < members.size(); ++index) {
            if (m_covered[index]) {
                continue;
            }
            const PartyMemberView& member = members[index];
            const core::Point at = m_place.origin + core::Point{0, static_cast<int>(index) * kRowHeight};
            const core::Rect area = rowBox(0).translated(at);
            drawRow(canvas, member, at);
            if (member.attention && blinkOn) {
                canvas.drawRectOutline({area.x - 1, area.y - 1, area.width + 2, area.height + 2}, kAttention, 2); // around the yellow one, 2 px thick
            }
            if (member.commanding) {
                canvas.drawRectOutline(area, kCommanding, 1);
            }
        }
        const bool onLoginRow = mouse && loginRowAt(*mouse);
        if (loginRow && !m_covered.back()) {
            drawLoginRow(canvas, m_place.origin + core::Point{0, m_memberRows * kRowHeight}, onLoginRow);
        }
        canvas.endScale();
        const int hovered = mouse ? memberAt(*mouse) : -1;
        if (hovered >= 0 && inWorld(members[static_cast<std::size_t>(hovered)])) {
            ui::drawHintBox(canvas, *mouse, members[static_cast<std::size_t>(hovered)].name);
        } else if (onLoginRow) {
            ui::drawHintBox(canvas, *mouse, kLoginHint);
        }
    }

    void SimplePartyPanels::drawWindowTag(gfx::Canvas& canvas, const core::Rect& window, const std::string& name) {
        canvas.drawRectOutline(window, kWindowTag, 2); // outline thickness in pixels
        ui::drawHintBox(canvas, window.position(), name);
    }

}
