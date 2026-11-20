#ifndef NORTH_ISLAND_CLIENT_RENDERING_HUD_PARTY_PANELS_H
#define NORTH_ISLAND_CLIENT_RENDERING_HUD_PARTY_PANELS_H

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

    /// @brief The qol client's party panel: a panel at the view's left edge with one row per other own character (logged in on this
    /// client besides the selected, played one), and under it a login button that logs another account in.
    /// @details The panel is the server list's (ServerSelectForm.xml): the panel10 skin, its left border off the view's left edge,
    /// and the Rail_H5 / bar_H4 scroll bar inside its right edge. Btn_ArrowL_8 right of its top right corner slides it out of
    /// sight to the left (collapse); Btn_ArrowR_8 at the view's left edge then slides it back. The arrows are the monster
    /// book's (MonsterBookWindow.xml, 21 x 21), drawn as ImageButtons are: normal, mouse over and pressed looks stacked down the
    /// picture. The panel and the arrows lie on the login window's white lined backing (form_module_1's lines: white and pale
    /// blue, a pixel each), kLined around the panel, as a tab with rounded right corners around each arrow, and as a tab with rounded bottom corners
    /// around the login button (the character select's Btn_Login, 56 x 20) under the panel's bottom right corner. While the panel
    /// is out of sight its arrow blinks a red outline when a member asks for attention.
    ///
    /// Grouping (order): a team of two or more own characters shows as a group: its leader's row (when the leader is an own
    /// character, else its first member's) heads it and the others are moved right (kIndent) under it, joined to it by a grey blue
    /// connector from the middle of its chat head to the middle of theirs. The selected character's team comes first, with the selected character's own
    /// row in it (PartyMemberView::played), so that a group looks the same whichever of its characters is selected (heading it
    /// when no own character leads); out of such a team the selected character's own row comes first alone. Rows of characters
    /// in no such team stay as they were. There are as many rows as characters:
    /// when more than fit show, the rows scroll (the mouse wheel over the panel, the scroll bar's thumb dragged, or a click on
    /// the bar above or below it for a page; the bar blinks a red outline above or below the thumb while a row out of sight that
    /// way asks for attention).
    ///
    /// A left click on a row makes that character the selected one (PartyMembers::select), in the world and in battle; a right
    /// click asks to log it out (WorldScene::askPartyLogOut). A row is drawn as the world view's pet (TRe_MiniPetInfo): the
    /// member's chat head facing left where the pet's figure goes, then level, HP and SP bars in the pet's pictures; how far its
    /// login has come while it is not in the world. The name shows over the row while the mouse is on it. Clicks anywhere on
    /// the panel are its own (they do not walk the character). When a window of the shown screen (a shop, the globe, mail, a
    /// pop-up menu) or a popup's box (the talk box, the maid's menu, an item or pet info box, a message box; Screen::windowOver)
    /// lies over the panel, the panel is left out: not drawn, and the clicks there go to the window or popup.
    ///
    /// Placement (place): the panel keeps to the view's left edge, between the HUD bars at its top left and bottom left, which
    /// grow with the HUD scale s (100% to 200%) from the view's corners: the own status (to the gold line, y 192) down to
    /// kStatusBottom · s, its vehicle gauge (StatusVehicle at 0,195,200,100, shown while riding) down to kWorldTop · s, and the
    /// chat channel list from kChatUp · s above the view's bottom (y 448 of 600). The panel holds kShownRows rows whatever the
    /// count (more scroll) and its place is kept for the login button whether it shows or not, so it stays put while characters
    /// come and go. The panel (with its tabs) shrinks by its scale from the rows' origin, x kLeft from the view's left and kPadTop
    /// under the panel's top.
    /// - World and battle alike: its bottom on the chat channel list, at the HUD scale or smaller (down to kLeastScale) until its
    ///   top clears the own status (y 209 at 800 x 600 and 100%). In battle it may lie over a command ring's buttons at the left
    ///   side's back line: collapsed, it leaves them free.
    /// - Riding in the world (the vehicle gauge shows): under the gauge, smaller down to kLeastScale; when even that does not
    ///   fit, the panel is not shown until the ride ends.
    class PartyPanels {
        public:
            static constexpr int kLeft = 8; // x of the rows' origin from the view's left at scale 1, px
            static constexpr int kPanelOut = 7; // px of the panel left of the view's left edge: its left border (panel10's) is out of sight
            static constexpr int kPanelRight = 129; // x of the panel's right edge from the rows' origin, px
            static constexpr int kPadTop = 8; // px from the panel's top to the rows' origin
            static constexpr int kPadBottom = 6; // px from the last row's box to the panel's bottom
            static constexpr int kBarInset = 4; // px of the scroll bar from the panel's top, right and bottom edges (as in the server list)
            static constexpr int kArrowGap = 8; // px from the panel's right edge to the collapse arrow
            static constexpr int kLined = 4; // px of the white lined backing around the panel (top, right, bottom) and the arrows
            static constexpr int kStatusBottom = 195; // HUD px from the view's top: 3 px under the own status's gold line (y 180 to 192)
            static constexpr int kWorldTop = 300; // HUD px from the view's top: 5 px under the vehicle gauge's place (195 + 100)
            static constexpr int kChatUp = 152; // HUD px from the view's bottom up to the chat channel list's top (600 - 448)
            static constexpr int kRowWidth = 72; // width of a row, px
            static constexpr int kRowHeight = 32; // row pitch, px
            static constexpr int kShownRows = 6; // rows the panel holds, however many there are (more scroll)
            static constexpr int kIndent = 20; // px a grouped member's row is moved right of its group's head
            static constexpr float kLeastScale = 0.5f; // the panel is never drawn smaller, factor
            static constexpr float kScaleStep = 0.0625f; // the panel's scale shrinks by 1/16 at a time until it fits (1 and kLeastScale are tried as well)
            static constexpr std::uint64_t kBlinkMs = 500; // ms the attention outline is on, then off
            static constexpr std::uint64_t kSlideMs = 160; // ms the panel takes to slide out of sight or back

            /// @brief Creates the party panel painter.
            /// @param pictures Picture library for the pictures.
            /// @param characters Character renderer used for the chat heads.
            PartyPanels(
                gfx::PictureLibrary& pictures,
                CharacterRenderer& characters
            )
                : m_pictures(pictures),
                  m_characters(characters) {}

            /// @brief Draws the panel (or, collapsed, its arrow), its rows and the hovered row's hint.
            /// @param canvas Target canvas.
            /// @param members The party members, a row each (the played character's own row among them, shown in its team's group).
            /// @param loginRow Draw the login button under the panel.
            /// @param battle The played account is in a battle (placed as in the world, not riding).
            /// @param riding The played account's status shows the vehicle gauge (WorldScene::vehicleGaugeShown): the panel stays
            /// under it, see the class.
            /// @param view The played account's screen (the window in the screen's coordinates).
            /// @param hudScale Size of the HUD.
            /// @param mouse Mouse position in the screen's coordinates, if any.
            /// @param nowMs The current time in milliseconds (the attention outline's blink, the slide).
            /// @param covered Tells whether a window or a popup's box of the shown screen lies over a box (in the screen's
            /// coordinates): the panel under one is neither drawn nor hit, so the window or popup shows and takes the clicks there.
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
            /// @return Member index, -1 for none.
            int memberAt(core::Point point) const;

            /// @brief Tells whether a point of the screen is on the login button, as last drawn.
            /// @param point Point in the screen's coordinates.
            /// @return True on the login button.
            bool loginRowAt(core::Point point) const;

            /// @brief Takes a mouse button press on the panel off the rows: an arrow slides the panel out of sight or back, the
            /// scroll bar's thumb starts a drag and the bar beside it scrolls a page; elsewhere on the panel it is only taken.
            /// @param point Point in the screen's coordinates.
            /// @param act The press acts (the left button); else it is only taken.
            /// @return True when the press was on the panel or an arrow (taken).
            bool press(core::Point point, bool act);

            /// @brief Tells whether the scroll bar's thumb is being dragged.
            /// @return True while dragged.
            bool dragging() const { return m_dragging; }

            /// @brief Moves the dragged thumb (and the rows with it).
            /// @param point The mouse in the screen's coordinates.
            void drag(core::Point point);

            /// @brief Ends a press: the thumb's drag and an arrow's pressed look.
            void release();

            /// @brief Tells whether a point of the screen is on the panel, as last drawn: the mouse wheel there scrolls the rows.
            /// @param point Point in the screen's coordinates.
            /// @return True on the panel.
            bool panelAt(core::Point point) const;

            /// @brief Scrolls the rows (kept within the rows at the next draw).
            /// @param lines Lines to scroll, down when positive.
            void scrollBy(int lines);

            /// @brief Draws a member's name in a hint box (like every hint) above one of its windows drawn over the selected character's screen, with a frame around it.
            /// @param canvas Target canvas.
            /// @param window Window rectangle.
            /// @param name The character's name.
            static void drawWindowTag(gfx::Canvas& canvas, const core::Rect& window, const std::string& name);

        private:
            /// @brief Where the panel goes: the rows' origin on the screen and the panel's scale.
            struct Placement {
                core::Point origin;
                float scale = 1.0f;
            };

            /// @brief One line of the rows: a member's row, in the order shown.
            struct Line {
                int member = -1; // member index
                bool indented = false; // a grouped member under its group's head
                bool head = false; // heads a group: the connector runs down from it
                bool linkUp = false; // the connector comes in from above (the line before)
                bool linkDown = false; // the connector goes on to the line after
            };

            /// @brief The pictures' sizes the panel's layout needs (an arrow's, the scroll bar's width).
            struct Sizes {
                int arrowWidth = 21;
                int arrowHeight = 21; // one look of an arrow (its looks are stacked down its picture)
                int barWidth = 17;
                int loginWidth = 56; // the login button's
                int loginHeight = 20; // one look of the login button (as CharacterSelectForm.xml sizes it)
            };

            /// @brief Places the panel clear of the HUD bars; see the class.
            /// @param view The played account's screen (the window in the screen's coordinates).
            /// @param hudScale Size of the HUD.
            /// @param battle The played account is in a battle.
            /// @param riding The vehicle gauge shows (not asked in battle).
            /// @param sizes The pictures' sizes.
            /// @return The placement, or none when the panel has no room (riding, see the class).
            static std::optional<Placement> place(const core::Rect& view, float hudScale, bool battle, bool riding, const Sizes& sizes);

            /// @brief Gives the panel's box for a number of rows, in the rows' own coordinates.
            /// @param rows Rows in sight.
            /// @return The box (its left border left of the view).
            static core::Rect panelBox(int rows);

            /// @brief Gives the white lined backing's box around the panel (top, right, bottom), in the rows' own coordinates.
            /// @param rows Rows in sight.
            /// @return The box.
            static core::Rect backBox(int rows);

            /// @brief Gives the white lined tab around the collapse arrow (from the panel's right edge), in the rows' own coordinates.
            /// @param sizes The pictures' sizes.
            /// @return The box.
            static core::Rect collapseTab(const Sizes& sizes);

            /// @brief Gives the white lined tab around the expand arrow (from past the view's left edge), in the rows' own coordinates.
            /// @return The box.
            core::Rect expandTab() const;

            /// @brief Draws the white lined backing over a box.
            /// @param canvas Target canvas.
            /// @param box The box, in the scaled drawing's coordinates.
            /// @param roundTop Round its top right corner.
            /// @param roundBottom Round its bottom right corner.
            /// @param roundLeft Round the left corners too (those asked for on the right), a pixel less.
            static void drawLined(gfx::Canvas& canvas, const core::Rect& box, bool roundTop, bool roundBottom, bool roundLeft = false);

            /// @brief Gives the login button's box (under the panel's bottom right, kLined under the backing), in the rows' own
            /// coordinates.
            /// @param sizes The pictures' sizes.
            /// @return The box.
            static core::Rect loginBox(const Sizes& sizes);

            /// @brief Gives the white lined tab around the login button (from the backing's top, its bottom corners rounded), in
            /// the rows' own coordinates.
            /// @param sizes The pictures' sizes.
            /// @return The box.
            static core::Rect loginTab(const Sizes& sizes);

            /// @brief Tells whether a point in the rows' own coordinates is on the panel as last drawn (its backing or a tab).
            /// @param at The point.
            /// @return True on it.
            bool onBlock(core::Point at) const;

            /// @brief Gives the box around the lined backing and the collapse arrow's and the login button's tabs, in the rows' own
            /// coordinates.
            /// @param rows Rows in sight.
            /// @param sizes The pictures' sizes.
            /// @return The box.
            static core::Rect blockBox(int rows, const Sizes& sizes);

            /// @brief Tells whether the panel with a number of rows (and its collapse arrow) clears the chat channel list at a
            /// placement.
            /// @param rows Rows in sight.
            /// @param sizes The pictures' sizes.
            /// @param placement The placement.
            /// @param chatTop The chat channel list's top on the screen.
            /// @return True when clear.
            static bool fits(int rows, const Sizes& sizes, const Placement& placement, int chatTop);

            /// @brief Converts a box in the rows' own coordinates to the screen at a placement.
            /// @param box The box.
            /// @param placement The placement.
            /// @return The box on the screen.
            static core::Rect onScreen(const core::Rect& box, const Placement& placement);

            /// @brief Orders the rows: the played character's team first (a group with its own row, when other own characters are
            /// in it), then the other members in login order, each other team of own characters together where its first member
            /// comes (its leader heading it when an own character, else its first member).
            /// @param members The party members (the played character's own row among them, PartyMemberView::played).
            /// @return The lines.
            static std::vector<Line> order(const std::vector<PartyMemberView>& members);

            /// @brief Gives a row's box in the rows' own coordinates.
            /// @param row Row index.
            /// @return The box: 2 px left of and 1 up from the row's origin, 2 px shorter than the row pitch.
            static core::Rect rowBox(int row);

            /// @brief Converts a point of the screen to the rows' own coordinates (unscaled, from the rows' origin).
            /// @param point Point in the screen's coordinates.
            /// @return Point in the rows' coordinates.
            core::Point toRows(core::Point point) const;

            /// @brief Gives a shown row's box in the rows' own coordinates, as last drawn (moved right when indented).
            /// @param slot Shown row index (0 is the first row in sight).
            /// @return The box.
            core::Rect slotBox(int slot) const;

            /// @brief Finds the line a point of the screen is on, as last drawn (only while the panel is out and shown).
            /// @param point Point in the screen's coordinates.
            /// @return Line index (in m_lines), -1 for none.
            int rowAt(core::Point point) const;

            /// @brief Gives the scroll bar's box in the rows' own coordinates, as last drawn.
            /// @return The box.
            core::Rect barBox() const;

            /// @brief Gives the scroll bar thumb's top and height within the bar, as last drawn.
            /// @return Top (from the bar's top) and height, px.
            std::pair<int, int> thumb() const;

            /// @brief Gives the collapse arrow's box (right of the panel's top right corner) in the rows' own coordinates.
            /// @return The box.
            core::Rect collapseBox() const;

            /// @brief Gives the expand arrow's box (at the view's left edge, where the panel's top is) in the rows' own coordinates.
            /// @return The box.
            core::Rect expandBox() const;

            /// @brief Draws a button (an arrow, the login button) in one of its looks, stacked down its picture.
            /// @param canvas Target canvas.
            /// @param name The button's picture.
            /// @param at Its top left, in the scaled drawing's coordinates.
            /// @param height One look's height.
            /// @param state 0 normal, 1 mouse over, 2 pressed.
            void drawButton(gfx::Canvas& canvas, const char* name, core::Point at, int height, int state);

            /// @brief Draws the scroll bar (Rail_H5 track, bar_H4 thumb as the server list's ScrollBar draws them).
            /// @param canvas Target canvas.
            /// @param box The bar's box, in the scaled drawing's coordinates.
            /// @param state The thumb's look: 0 normal, 1 mouse over, 2 dragged.
            void drawBar(gfx::Canvas& canvas, const core::Rect& box, int state);

            /// @brief Draws the connector piece of a line: down its left from the line above or the head, and over to the row.
            /// @param canvas Target canvas.
            /// @param line The line.
            /// @param at Top-left of the line (unindented).
            static void drawLink(gfx::Canvas& canvas, const Line& line, core::Point at);

            /// @brief Draws one member's row: chat head, level, HP and SP bars, or its login state.
            /// @param canvas Target canvas.
            /// @param member The member.
            /// @param at Top-left of the row.
            void drawRow(gfx::Canvas& canvas, const PartyMemberView& member, core::Point at);

            /// @brief Gives the pictures' sizes (the defaults while a picture is missing).
            /// @return The sizes.
            Sizes sizes() const;

            gfx::PictureLibrary& m_pictures;
            CharacterRenderer& m_characters;
            Placement m_place; // where the panel was last drawn
            Sizes m_sizes; // the pictures' sizes, as last drawn
            std::vector<Line> m_lines; // the lines in their order, as last drawn
            int m_scroll = 0; // the first line in sight
            int m_shown = 0; // lines in sight at most, as last drawn
            bool m_placed = false; // the panel had room and no window lay over it, as last drawn
            bool m_open = true; // the panel is out (else collapsed to its arrow at the view's left edge)
            float m_slide = 1; // how far the panel is out: 1 out, 0 out of sight (it moves toward m_open)
            std::uint64_t m_slideMs = 0; // when the slide was last moved
            bool m_dragging = false; // the scroll bar's thumb is dragged
            int m_dragFromY = 0; // the mouse's y in the rows' coordinates where the drag started
            int m_dragFromScroll = 0; // the first line in sight when the drag started
            bool m_arrowPressed = false; // an arrow is pressed (its pressed look until the button is released)
            bool m_loginShown = false; // the login button shows, as last drawn
    };

}

#endif
