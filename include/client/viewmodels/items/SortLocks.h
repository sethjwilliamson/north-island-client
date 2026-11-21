#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_SORT_LOCKS_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_SORT_LOCKS_H

#include "client/models/items/SortLockFile.h"
#include "client/services/GameServices.h"
#include "ui/IconGrid.h"
#include "ui/ImageButton.h"
#include "ui/Screen.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <tuple>

namespace client {

    /// @brief The places of a grid a sort leaves alone (qol), and the lock button beside the sort button that picks them.
    /// @details While the button is on, a click on a place of the grid locks or unlocks it instead of moving its thing; a locked
    /// place shows a padlock. A place is locked as a place: what lies there (a thing or nothing) stays there when the grid is sorted.
    /// The locks are kept per server, account and character slot in the user folder (SortLockFile), read again whenever the
    /// character playing changes and written at every change.
    class SortLocks {
        public:
            /// @brief Binds the lock button and the grid, and handles the button's Click and the grid's Pick.
            /// @param services The game services (the user folder and the account playing).
            /// @param screen The screen holding both.
            /// @param buttonName The lock button's element name; its Click handler is named "on" + buttonName.
            /// @param gridName The grid's element name; its Pick handler is named "on" + buttonName + "Pick"; the locks are saved under it.
            /// @param span How many cells of the grid show one place (a store tank row spans its 5 cells); 1 for a cell each.
            SortLocks(GameServices& services, ui::Screen& screen, const std::string& buttonName, const std::string& gridName, int span = 1)
                : m_services(services), m_button(screen.get<ui::ImageButton>(buttonName)), m_grid(screen.get<ui::IconGrid>(gridName)), m_gridName(gridName),
                  m_span(span) {
                screen.on("on" + buttonName, [this](const ui::EventArgs&) { setPicking(!m_picking); });
                screen.on("on" + buttonName + "Pick", [this](const ui::EventArgs& args) { toggle((args.index - 1) / m_span + 1); });
            }

            SortLocks(const SortLocks&) = delete;
            SortLocks& operator=(const SortLocks&) = delete;

            /// @brief Shows or hides the lock button beside the sort button; picking stops when it is hidden or the window closed.
            /// @param shown Whether the sort button shows.
            /// @param open Whether the window is open.
            void update(bool shown, bool open) {
                follow();
                m_button.setVisible(shown);
                if (!shown || !open) {
                    setPicking(false);
                }
            }

            /// @brief Turns picking on or off: the button shows pressed and the grid's clicks pick places.
            /// @param picking True to pick.
            void setPicking(bool picking) {
                m_picking = picking;
                m_button.setSelected(picking);
                m_grid.setPicking(picking);
            }

            /// @brief Whether places are being picked.
            /// @return True while picking.
            bool isPicking() const { return m_picking; }

            /// @brief Locks a place, or unlocks a locked one.
            /// @param place The place (1-based).
            void toggle(int place) {
                follow();
                if (place < 1 || !m_owner) {
                    return;
                }
                if (!m_places.erase(place)) {
                    m_places.insert(place);
                }
                SortLockFile::write(file(), std::get<2>(*m_owner), m_gridName, m_places);
                show();
            }

            /// @brief The locked places.
            /// @return The places (1-based).
            const std::set<int>& places() const { return m_places; }

        private:
            using Owner = std::tuple<std::uint16_t, std::uint32_t, int>; // server id, login id, character slot

            /// @brief The lock file of the account playing.
            /// @return Its path.
            std::filesystem::path file() const {
                return SortLockFile::pathFor(m_services.game.userRoot(), m_services.session.serverId, m_services.session.loginId);
            }

            /// @brief Reads the locks again when another character plays (a relog, a character change); none before a log-in.
            void follow() {
                const AccountSession& session = m_services.session;
                const std::optional<Owner> owner =
                    session.loginId == 0 ? std::nullopt : std::optional<Owner>(Owner{session.serverId, session.loginId, session.playingSlot});
                if (owner == m_owner) {
                    return;
                }
                m_owner = owner;
                m_places = owner ? SortLockFile::read(file(), std::get<2>(*owner), m_gridName) : std::set<int>{};
                show();
            }

            /// @brief Draws the padlocks on the grid's cells of the locked places.
            void show() {
                std::set<int> cells;
                for (const int locked : m_places) {
                    for (int cell = (locked - 1) * m_span + 1; cell <= locked * m_span; ++cell) {
                        cells.insert(cell);
                    }
                }
                m_grid.setLocked(std::move(cells));
            }

            GameServices& m_services;
            ui::ImageButton& m_button;
            ui::IconGrid& m_grid;
            std::string m_gridName;
            int m_span = 1;
            std::optional<Owner> m_owner;
            bool m_picking = false;
            std::set<int> m_places;
    };

}

#endif
