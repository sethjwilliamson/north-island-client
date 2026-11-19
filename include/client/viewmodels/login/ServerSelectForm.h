#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_LOGIN_SERVER_SELECT_FORM_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_LOGIN_SERVER_SELECT_FORM_H

#include "client/services/GameServices.h"
#include "client/services/ServerStatusChannel.h"
#include "client/services/servers/SavedServers.h"
#include "client/viewmodels/login/ServerTree.h"
#include "formats/ServerList.h"
#include "net/HostResolver.h"
#include "ui/DrawingArea.h"
#include "ui/Element.h"
#include "ui/Label.h"
#include "ui/Screen.h"
#include "ui/ScrollBar.h"
#include "ui/Spot.h"
#include "ui/TextBox.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>

namespace client {

    /// @brief Server list of two foldable sections (added servers, SERVER.INI groups) with status icons; Next or a double click connects.
    /// @details Original: ServerSelectForm.xml, redone for qol; added servers live in saved-servers.json and are looked up by HostResolver.
    class ServerSelectForm {
        public:
            /// @brief Hooks the form calls to connect or leave.
            struct Callbacks {
                std::function<void(const formats::ServerEntry& server)> connect;
                std::function<void()> leave;
            };

            /// @brief Loads ServerSelectForm.xml, reads SERVER.INI and the qol server file.
            /// @param services Shared game services.
            /// @param screen Screen the layout is loaded into.
            /// @param callbacks Connect and leave hooks.
            ServerSelectForm(
                GameServices& services,
                ui::Screen& screen,
                Callbacks callbacks
            );

            /// @brief Shows the form and asks for the statuses again.
            void show();

            /// @brief Tells whether the list shows a server row (test scripts wait for it).
            /// @return True when there is one.
            bool hasServerRow() const;

            /// @brief Selects the first server row and connects to it, as a double click on it does (test scripts).
            /// @return False when the form is hidden or the list has no server row.
            bool connectFirst();

            /// @brief Hides the form, forgets the statuses and clears the selection.
            /// @details Original: HideCtrl.
            void hide();

            /// @brief Tells whether the form is shown.
            /// @return True when visible.
            bool isVisible() const { return m_form.isVisible(); }

            /// @brief Per-frame work: status channels, host lookups and the message line.
            void update();

        private:
            using Row = ServerTree::Row;

            /// @brief Rebuilds the tree rows and fixes the scroll range.
            void rebuild();

            /// @brief Asks for the statuses again, rebuilds and shows the idle message.
            void refresh();

            /// @brief Starts the status queries for every group and added server.
            void queryStatuses();

            /// @brief Scrolls the list so the given row is at the top, clamped.
            /// @param top Index of the wanted top row.
            void scrollTo(int top);

            /// @brief Finds the row under a point in the list.
            /// @param local Point relative to the list.
            /// @return The row, or nothing.
            std::optional<Row> rowAt(core::Point local) const;

            /// @brief Handles a click: folds sections and groups, selects servers, connects on a double click.
            /// @param local Click point relative to the list.
            /// @param twice True for a double click.
            void clickRow(core::Point local, bool twice);

            /// @brief Adds a server typed by address and name.
            void addServer();

            /// @brief Removes the selected added server.
            void removeServer();

            /// @brief Connects to the selected server.
            void connectToSelected();

            /// @brief Connects to an added server once its host's address is known.
            /// @param server The added server.
            /// @param lookUpAgain True to look the host up again (a failed lookup is retried).
            void connectDirect(const DirectServer& server, bool lookUpAgain);

            /// @brief Saves the added servers and fold state.
            void saveServers();

            /// @brief Sets the message line text.
            /// @param text Message to show.
            void showMessage(const std::string& text) { m_message = text; }

            /// @brief Gives a server row's name.
            /// @param row A server row.
            /// @return The server name.
            std::string nameOf(const Row& row) const;

            /// @brief Gives a server row's address.
            /// @param row A server row.
            /// @return The address text.
            std::string addressOf(const Row& row) const;

            /// @brief Gives a server row's status icon frame.
            /// @param row A server row.
            /// @return The frame, or nothing when no icon shows.
            std::optional<int> statusOf(const Row& row) const;

            /// @brief Gives the text of a section or group row with its count.
            /// @param row A section or group row.
            /// @return The header text.
            std::string headerText(const Row& row) const;

            /// @brief Paints the visible rows.
            /// @param canvas Target canvas.
            /// @param at Origin of the list area.
            void paintRows(gfx::Canvas& canvas, core::Point at);

            /// @brief Paints the tooltip of the status icon under the mouse.
            /// @details Original: DrawServerTooltip.
            /// @param canvas Target canvas.
            void paintTooltip(gfx::Canvas& canvas);

            GameServices& m_services;
            ui::Screen& m_screen;
            Callbacks m_callbacks;
            ui::Element& m_form;
            ui::DrawingArea& m_rows;
            ui::ScrollBar& m_scroll;
            ui::TextBox& m_nameBox;
            ui::TextBox& m_addressBox;
            ui::Label& m_messageLabel;
            const ui::Spot& m_rowSpot;
            const ui::Spot& m_iconSpot;
            const ui::Spot& m_sectionAtSpot;
            const ui::Spot& m_groupArrowAtSpot;
            const ui::Spot& m_groupTextAtSpot;
            const ui::Spot& m_nameAtSpot;
            const ui::Spot& m_groupNameAtSpot;
            const ui::Spot& m_addressAtSpot;
            const ui::Spot& m_sectionColorSpot;
            const ui::Spot& m_groupColorSpot;
            const ui::Spot& m_nameColorSpot;
            const ui::Spot& m_addressColorSpot;
            const ui::Spot& m_selectedColorSpot;
            const ui::Spot& m_hoverColorSpot;
            formats::ServerList m_servers;
            SavedServers m_qol;
            ServerTree m_tree;
            net::HostResolver m_resolver;
            std::vector<std::unique_ptr<ServerStatusChannel>> m_groupStatus;
            std::map<std::string, std::unique_ptr<ServerStatusChannel>> m_directStatus;
            std::optional<DirectServer> m_pendingConnect;
            std::string m_message;
            core::Point m_rowsAt;
            int m_top = 0;
            std::mt19937 m_random{std::random_device{}()};
    };

}

#endif
