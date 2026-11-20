#ifndef NORTH_ISLAND_CLIENT_RENDERING_HUD_SIMPLE_PARTY_PANELS_H
#define NORTH_ISLAND_CLIENT_RENDERING_HUD_SIMPLE_PARTY_PANELS_H

#include "client/rendering/characters/CharacterRenderer.h"
#include "client/services/party/PartyMembers.h"
#include "gfx/Canvas.h"
#include "gfx/PictureLibrary.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace client {

    /// @brief The qol client's Simple multi-login rows (at most PartyMembers::kSimpleMembers members; PartyPanels is the Advanced
    /// panel): one row per other own character (logged in on this client besides the selected, played one), then a "+" row that
    /// logs another account in while there is room.
    /// @details A left click on a row makes that character the selected one (PartyMembers::select), in the world and in battle; a
    /// right click asks to log it out (WorldScene::askPartyLogOut). Drawn as the world view's pet (TRe_MiniPetInfo): the member's
    /// chat head facing left where the pet's figure goes, then level, HP and SP bars in the pet's pictures; how far its login has
    /// come while it is not in the world. The name shows over the row while the mouse is on it. A row under a window of the shown
    /// screen (a shop, the globe, mail, a pop-up menu) or under a popup's box (the talk box, the maid's menu, an item or pet info
    /// box, a message box; Screen::windowOver) is left out: not drawn, and the clicks there go to the window or popup.
    ///
    /// Placement (place): the rows keep to the view's left edge, between the HUD bars at its top left and bottom left, which grow
    /// with the HUD scale s (100% to 200%) from the view's corners: the own status (HP, SP, EXP, then the gold line: Main_Word_Gold
    /// at y 180, 12 px high, to y 192) down to kStatusBottom · s, its vehicle gauge (StatusVehicle at 0,195,200,100 to y 295,
    /// shown while riding: the icon and rails from y 200) down to kWorldTop · s, and the chat channel list from kChatUp · s above
    /// the view's bottom (y 448 of 600; the chat frame starts lower, from about 465). Room is made for kMostRows rows
    /// (kBlockHeight, 126 px at scale 1, the first row's box starting 1 px above its origin), however many show, so the rows stay
    /// put while characters come and go. The rows' scale shrinks the rows' box from their origin, x kLeft from the view's left.
    /// - World: under the vehicle gauge (kWorldTop · s) at the HUD scale, or smaller down to 1 when the room above the channel list
    ///   is shorter. Else, while riding (the gauge shows), smaller under the gauge down to kLeastScale; when even that does not
    ///   fit, the rows are not shown (not drawn, not hit) until the ride ends: the least bad, as every place at the view's left
    ///   edge then meets the gauge, the status or the chat. Else (not riding) at scale 1 with the rows' bottom on the channel
    ///   list, rising over the gauge's empty place but not over the status; else (less room under the status than kBlockHeight)
    ///   smaller under the status, down to kLeastScale. In an 800 x 600 window (box edges, px):
    ///   - 100%: from y 300 at scale 1, the box 299-425 (channel list 448), riding or not.
    ///   - 110%: not riding at scale 1 from y 308 (box to 433, the list); riding under the gauge (kWorldTop · s = 330) at 0.8125.
    ///   - 125%: kWorldTop · s is 375 (the gauge's place to 369) and the list 410: not riding from y 285 at scale 1 (box 284-410);
    ///     riding not shown, as from 120% (at 119% kLeastScale just fits: box 356-419, list 419).
    ///   - 150%: the status reaches 293 and the list 372: scale 0.625 under the status (box 292-371); riding not shown.
    ///   - From 175% the status runs past the channel list (the HUD bars meet each other) and the rows take kLeastScale under the
    ///     status (accepted: they meet the list); riding not shown.
    ///   A larger window gives the rows the HUD scale again (1920 x 1080 at 200%: kWorldTop · s 600, the list from 776, scale
    ///   1.375).
    /// - Battle: from y kBattleTop of the game screen, at scale 1 at most, smaller down to kLeastScale until the rows clear the
    ///   chat channel list and the command ring. The battle form (BattleMainForm.xml, not anchored, not HUD-scaled) lies at the
    ///   game screen's fixed coordinates: its member panels (136 px from the top) and the own HP, SP, EXP and gold under them (to
    ///   y 199), and the fighters; the vehicle gauge is not shown. The left side's back line (BattleField, row 1) stands at x
    ///   210, 185, 160, 135 with its feet at y 266, 329, 392, 455, and the ring reaches 68 px left of, 96 px above and 39 px below
    ///   a fighter's feet (BattleMainForm kHomes, 28 px buttons), so its nearest buttons are at x 142 (y 170-305), 117
    ///   (233-368), 92 (296-431) and 67 (359-494). At scale 1 the rows' box (x 6-78, y 203-329) lies left of the x 92 button and
    ///   above the x 67 one. In an 800 x 600 window the chat channel list (from 600 - 152 · s) keeps scale 1 up to 175% (y 334)
    ///   and takes the rows to 0.6875 at 200% (y 296). The right side (x 540 and over) and the auto-cancel button (762,526) are
    ///   far off.
    class SimplePartyPanels {
        public:
            static constexpr int kLeft = 8; // x of the rows' origin from the view's left at scale 1, px
            static constexpr int kStatusBottom = 195; // HUD px from the view's top: 3 px under the own status's gold line (y 180 to 192)
            static constexpr int kWorldTop = 300; // HUD px from the view's top: 5 px under the vehicle gauge's place (195 + 100)
            static constexpr int kBattleTop = 204; // first row's y in battle, game screen px: 4 px under the battle gold line (y 187 to 199)
            static constexpr int kChatUp = 152; // HUD px from the view's bottom up to the chat channel list's top (600 - 448)
            static constexpr int kRowWidth = 72; // width of a row, px
            static constexpr int kRowHeight = 32; // row pitch, px
            static constexpr int kMostRows = PartyMembers::kSimpleMembers + 1; // rows at most: kSimpleMembers members' rows, or fewer and the "+" row
            static constexpr int kBlockHeight = kMostRows * kRowHeight - 2; // px from the first row's box top to the last's bottom (2 px gap after the last)
            static constexpr float kLeastScale = 0.5f; // the rows are never drawn smaller, factor
            static constexpr float kScaleStep = 0.0625f; // the rows' scale shrinks by 1/16 at a time until they fit (1 and kLeastScale are tried as well)
            static constexpr std::uint64_t kBlinkMs = 500; // ms the attention outline is on, then off

            /// @brief Creates the party rows painter.
            /// @param pictures Picture library for the pet pictures.
            /// @param characters Character renderer used for the chat heads.
            SimplePartyPanels(
                gfx::PictureLibrary& pictures,
                CharacterRenderer& characters
            )
                : m_pictures(pictures),
                  m_characters(characters) {}

            /// @brief Draws the rows, the "+" row and the hovered row's hint.
            /// @param canvas Target canvas.
            /// @param members The party members, a row each.
            /// @param loginRow Draw the "+" row after the members' rows.
            /// @param battle The played account is in a battle (the rows go higher and clear the command ring, see the class).
            /// @param riding The played account's status shows the vehicle gauge (WorldScene::vehicleGaugeShown): the rows stay
            /// under it, see the class.
            /// @param view The played account's screen (the window in the screen's coordinates).
            /// @param hudScale Size of the HUD.
            /// @param mouse Mouse position in the screen's coordinates, if any.
            /// @param nowMs The current time in milliseconds (the attention outline's blink).
            /// @param covered Tells whether a window or a popup's box of the shown screen lies over a row's box (in the screen's
            /// coordinates): that row is neither drawn nor hit, so the window or popup shows and takes the clicks there.
            void draw(
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
            );

            /// @brief Finds the member whose row a point of the screen is on, as last drawn.
            /// @param point Point in the screen's coordinates.
            /// @return Member index, -1 for none (the "+" row included).
            int memberAt(core::Point point) const;

            /// @brief Tells whether a point of the screen is on the "+" row, as last drawn.
            /// @param point Point in the screen's coordinates.
            /// @return True on the "+" row.
            bool loginRowAt(core::Point point) const;

            /// @brief Draws a member's name in a hint box (like every hint) above one of its windows drawn over the selected character's screen, with a frame around it.
            /// @param canvas Target canvas.
            /// @param window Window rectangle.
            /// @param name The character's name.
            static void drawWindowTag(gfx::Canvas& canvas, const core::Rect& window, const std::string& name);

        private:
            /// @brief Where the rows go: the first row's origin on the screen and the rows' scale.
            struct Placement {
                core::Point origin;
                float scale = 1.0f;
            };

            /// @brief Places the rows clear of the HUD bars (and in battle of the command ring); see the class.
            /// @param view The played account's screen (the window in the screen's coordinates).
            /// @param hudScale Size of the HUD.
            /// @param battle The played account is in a battle.
            /// @param riding The vehicle gauge shows (not asked in battle).
            /// @return The placement, or none when the rows have no room (riding, see the class).
            static std::optional<Placement> place(const core::Rect& view, float hudScale, bool battle, bool riding);

            /// @brief Gives a row's box (the members' rows' and the "+" row's backing) in the rows' own coordinates.
            /// @param row Row index.
            /// @return The box: 2 px left of and 1 up from the row's origin, 2 px shorter than the row pitch.
            static core::Rect rowBox(int row);

            /// @brief Converts a point of the screen to the rows' own coordinates (unscaled, from the first row's origin).
            /// @param point Point in the screen's coordinates.
            /// @return Point in the rows' coordinates.
            core::Point toRows(core::Point point) const;

            /// @brief Gives a row's box on the screen, where it was last placed.
            /// @param row Row index.
            /// @return The box in the screen's coordinates.
            core::Rect screenBox(int row) const;

            /// @brief Finds the row a point of the screen is on, as last drawn (rows under a window not counted).
            /// @param point Point in the screen's coordinates.
            /// @return Row index (the "+" row is the one after the members'), -1 for none.
            int rowAt(core::Point point) const;

            /// @brief Draws the "+" row.
            /// @param canvas Target canvas.
            /// @param at Top-left of the row.
            /// @param hovered The mouse is on it.
            void drawLoginRow(gfx::Canvas& canvas, core::Point at, bool hovered);

            /// @brief Draws one member's row: chat head, level, HP and SP bars, or its login state.
            /// @param canvas Target canvas.
            /// @param member The member.
            /// @param at Top-left of the row.
            void drawRow(gfx::Canvas& canvas, const PartyMemberView& member, core::Point at);

            gfx::PictureLibrary& m_pictures;
            CharacterRenderer& m_characters;
            Placement m_place; // where the rows were last drawn
            int m_memberRows = 0; // members' rows as last drawn
            bool m_loginRow = false; // the "+" row was last drawn
            std::vector<bool> m_covered; // per row as last drawn: a window or a popup's box of the shown screen lay over it (not drawn, not hit)
    };

}

#endif
