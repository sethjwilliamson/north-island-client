#include "client/viewmodels/login/ServerSelectForm.h"

#include "core/FileSystem.h"
#include "core/Log.h"
#include "gfx/TextRenderer.h"
#include "ui/Tooltip.h"

#include <algorithm>
#include <array>

namespace client {

    namespace {

        constexpr std::array<const char*, 4> kStatusHints{"Not connected", "All right", "Busy", "Crowded"};
        constexpr core::Color kArrowColor{200, 220, 255, 255}; // fold arrow: pale blue
        const char* const kIdleMessage = "Add a server by its IP or host name (:port)";

        /// @brief Cuts a text to a pixel width, ending in ".." when cut.
        /// @param text Text to fit.
        /// @param width Available width in pixels.
        /// @return The fitted text.
        std::string fitted(const std::string& text, int width) {
            const int fit = width / gfx::TextRenderer::charWidth(gfx::TextEffect::Shadow);
            if (static_cast<int>(text.size()) <= fit) {
                return text;
            }
            return fit > 2 ? text.substr(0, static_cast<std::size_t>(fit - 2)) + ".." : std::string(); // 2: width of the ".." ending
        }

        /// @brief Draws a fold arrow, pointing right (folded) or down.
        /// @param canvas Target canvas.
        /// @param at Top-left of the arrow area.
        /// @param folded True for the folded (right) arrow.
        void drawArrow(gfx::Canvas& canvas, core::Point at, bool folded) {
            for (int i = 0; i < 4; ++i) { // arrow is four columns/rows deep
                if (folded) {
                    canvas.fillRect({at.x + 2 + i, at.y + 4 + i, 1, 8 - 2 * i}, kArrowColor); // column i of a right-pointing triangle, 8 px tall at its base
                } else {
                    canvas.fillRect({at.x + i, at.y + 6 + i, 8 - 2 * i, 1}, kArrowColor); // row i of a down-pointing triangle, 8 px wide at its top
                }
            }
        }

        /// @brief Cuts spaces off both ends of a text.
        /// @param text Text to trim.
        /// @return The trimmed text.
        std::string trimmed(const std::string& text) {
            const std::size_t first = text.find_first_not_of(' ');
            return first == std::string::npos ? std::string() : text.substr(first, text.find_last_not_of(' ') - first + 1);
        }

    }

    ServerSelectForm::ServerSelectForm(
        GameServices& services,
        ui::Screen& screen,
        Callbacks callbacks
    )
        : m_services(services),
          m_screen(screen),
          m_callbacks(std::move(callbacks)),
          m_form(screen.load(services.game.layout("ServerSelectForm.xml"), services.loadContext())),
          m_rows(screen.get<ui::DrawingArea>("ServerRows")),
          m_scroll(screen.get<ui::ScrollBar>("ServerRowsScroll")),
          m_nameBox(screen.get<ui::TextBox>("ServerNameBox")),
          m_addressBox(screen.get<ui::TextBox>("ServerAddressBox")),
          m_messageLabel(screen.get<ui::Label>("ServerMessage")),
          m_rowSpot(screen.get<ui::Spot>("ServerRow")),
          m_iconSpot(screen.get<ui::Spot>("ServerIcon")),
          m_sectionAtSpot(screen.get<ui::Spot>("ServerSectionAt")),
          m_groupArrowAtSpot(screen.get<ui::Spot>("ServerGroupArrowAt")),
          m_groupTextAtSpot(screen.get<ui::Spot>("ServerGroupTextAt")),
          m_nameAtSpot(screen.get<ui::Spot>("ServerNameAt")),
          m_groupNameAtSpot(screen.get<ui::Spot>("ServerGroupNameAt")),
          m_addressAtSpot(screen.get<ui::Spot>("ServerAddressAt")),
          m_sectionColorSpot(screen.get<ui::Spot>("ServerSectionColor")),
          m_groupColorSpot(screen.get<ui::Spot>("ServerGroupColor")),
          m_nameColorSpot(screen.get<ui::Spot>("ServerNameColor")),
          m_addressColorSpot(screen.get<ui::Spot>("ServerAddressColor")),
          m_selectedColorSpot(screen.get<ui::Spot>("ServerSelectedColor")),
          m_hoverColorSpot(screen.get<ui::Spot>("ServerHoverColor")) {
        m_rows.setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintRows(canvas, at); });
        screen.get<ui::DrawingArea>("ServerTooltip").setPainter([this](gfx::Canvas& canvas, core::Point, std::uint64_t) { paintTooltip(canvas); });
        screen.on("onServerRowDown", [this](const ui::EventArgs& e) { clickRow(e.position, false); });
        screen.on("onServerRowDouble", [this](const ui::EventArgs& e) { clickRow(e.position, true); });
        screen.on("onServerRowsWheel", [this](const ui::EventArgs& e) { scrollTo(m_top - e.index); });
        screen.on("onServerRowsScroll", [this](const ui::EventArgs& e) { scrollTo(e.index); });
        screen.on("onServerAdd", [this](const ui::EventArgs&) { addServer(); });
        screen.on("onServerRemove", [this](const ui::EventArgs&) { removeServer(); });
        screen.on("onServerRefresh", [this](const ui::EventArgs&) { refresh(); });
        screen.on("onServerNameTab", [this](const ui::EventArgs&) { m_screen.focus("ServerAddressBox"); });
        screen.on("onServerAddressTab", [this](const ui::EventArgs&) { m_screen.focus("ServerNameBox"); });
        screen.on("onNext", [this](const ui::EventArgs&) { connectToSelected(); });
        screen.on("onLeave", [this](const ui::EventArgs&) { m_callbacks.leave(); });

        if (const auto lines = core::readLines(services.game.data("SERVER.INI"))) {
            m_servers = formats::ServerList::parse(*lines);
        } else {
            core::logError("cannot read SERVER.INI");
        }
        m_qol = SavedServers::loadIn(services.game.userRoot());
        m_tree.setCollapsed(m_qol.collapsed);
        rebuild();
    }

    void ServerSelectForm::show() {
        m_form.setVisible(true);
        refresh();
    }

    void ServerSelectForm::hide() {
        m_form.setVisible(false);
        m_groupStatus.clear();
        m_directStatus.clear();
        m_pendingConnect.reset();
        m_tree.select(std::nullopt);
        rebuild();
    }

    void ServerSelectForm::refresh() {
        queryStatuses();
        rebuild();
        showMessage(kIdleMessage);
    }

    void ServerSelectForm::queryStatuses() {
        m_groupStatus.clear();
        for (const formats::ServerGroup& group : m_servers.groups()) {
            auto channel = std::make_unique<ServerStatusChannel>();
            if (!group.servers.empty()) {
                std::uniform_int_distribution<std::size_t> pick(0, group.servers.size() - 1);
                channel->query(group.servers[pick(m_random)].address);
            }
            m_groupStatus.push_back(std::move(channel));
        }
        m_directStatus.clear();
        for (const DirectServer& server : m_qol.direct) {
            m_resolver.request(server.host);
        }
    }

    void ServerSelectForm::rebuild() {
        std::vector<std::pair<int, int>> groups;
        for (const formats::ServerGroup& group : m_servers.groups()) {
            groups.emplace_back(group.number, static_cast<int>(group.servers.size()));
        }
        m_tree.rebuild(static_cast<int>(m_qol.direct.size()), groups);
        const int page = m_rows.bounds().height / m_rowSpot.size().height;
        m_scroll.setRange(static_cast<int>(m_tree.rows().size()), page);
        scrollTo(m_top);
    }

    void ServerSelectForm::scrollTo(int top) {
        const int page = m_rows.bounds().height / m_rowSpot.size().height;
        m_top = std::clamp(top, 0, std::max(0, static_cast<int>(m_tree.rows().size()) - page));
        m_scroll.setPosition(m_top);
    }

    std::optional<ServerTree::Row> ServerSelectForm::rowAt(core::Point local) const {
        if (local.x < 0 || local.y < 0 || local.x >= m_rows.bounds().width) {
            return std::nullopt;
        }
        const int index = m_top + local.y / m_rowSpot.size().height;
        if (local.y >= m_rows.bounds().height || index >= static_cast<int>(m_tree.rows().size())) {
            return std::nullopt;
        }
        return m_tree.rows()[static_cast<std::size_t>(index)];
    }

    void ServerSelectForm::clickRow(core::Point local, bool twice) {
        const std::optional<Row> row = rowAt(local);
        if (!row) {
            return;
        }
        if (row->kind != Row::Kind::Server) {
            if (!twice) {
                m_tree.toggle(*row);
                m_qol.collapsed = m_tree.collapsed();
                saveServers();
                rebuild();
                m_services.playSound("sound/wav0151.wav"); // wav0151: UI click/confirm sound
            }
            return;
        }
        m_tree.select(row);
        showMessage(nameOf(*row) + ": " + addressOf(*row));
        if (twice) {
            connectToSelected();
        }
    }

    void ServerSelectForm::addServer() {
        std::optional<DirectServer> server = SavedServers::parseAddress(trimmed(m_addressBox.text()));
        if (!server) {
            showMessage(m_addressBox.text().empty() ? "Type the server's IP or host name" : "Not an address: an IP or host name, :port");
            m_screen.focus("ServerAddressBox");
            return;
        }
        if (const std::string name = trimmed(m_nameBox.text()); !name.empty()) {
            server->name = name;
        }
        const auto same = std::find_if(m_qol.direct.begin(), m_qol.direct.end(), [&server](const DirectServer& other) {
            return other.host == server->host && other.port == server->port;
        });
        if (same != m_qol.direct.end()) {
            showMessage("That server is in the list already");
            return;
        }
        m_qol.direct.push_back(*server);
        saveServers();
        m_resolver.request(server->host);
        if (m_tree.isCollapsed({Row::Kind::Section, ServerTree::Section::Direct, -1, -1})) {
            m_tree.toggle({Row::Kind::Section, ServerTree::Section::Direct, -1, -1});
            m_qol.collapsed = m_tree.collapsed();
            saveServers();
        }
        rebuild();
        const Row added{Row::Kind::Server, ServerTree::Section::Direct, -1, static_cast<int>(m_qol.direct.size()) - 1};
        m_tree.select(added);
        const int page = m_rows.bounds().height / m_rowSpot.size().height;
        if (const int index = m_tree.selectedIndex(); index >= m_top + page) {
            scrollTo(index - page + 1);
        }
        m_nameBox.clear();
        m_addressBox.clear();
        showMessage("Added " + server->name);
        m_services.playSound("sound/wav0151.wav"); // wav0151: UI click/confirm sound
    }

    void ServerSelectForm::removeServer() {
        const std::optional<Row>& selected = m_tree.selected();
        if (!selected || selected->section != ServerTree::Section::Direct) {
            showMessage("Pick a server you added to remove it");
            return;
        }
        const DirectServer removed = m_qol.direct[static_cast<std::size_t>(selected->index)];
        m_qol.direct.erase(m_qol.direct.begin() + selected->index);
        m_tree.select(std::nullopt);
        saveServers();
        rebuild();
        showMessage("Removed " + removed.name);
        m_services.playSound("sound/wav0151.wav"); // wav0151: UI click/confirm sound
    }

    void ServerSelectForm::saveServers() {
        if (!m_qol.save(SavedServers::fileIn(m_services.game.userRoot()))) {
            core::logWarning("cannot save ", SavedServers::fileIn(m_services.game.userRoot()).string());
        }
    }

    bool ServerSelectForm::hasServerRow() const {
        return std::any_of(m_tree.rows().begin(), m_tree.rows().end(), [](const Row& row) { return row.kind == Row::Kind::Server; });
    }

    bool ServerSelectForm::connectFirst() {
        const auto& rows = m_tree.rows();
        const auto first = std::find_if(rows.begin(), rows.end(), [](const Row& row) { return row.kind == Row::Kind::Server; });
        if (!isVisible() || first == rows.end()) {
            return false;
        }
        m_tree.select(*first);
        connectToSelected();
        return true;
    }

    void ServerSelectForm::connectToSelected() {
        const std::optional<Row>& row = m_tree.selected();
        if (!isVisible() || !row) {
            return;
        }
        m_services.playSound("sound/wav0151.wav"); // wav0151: UI click/confirm sound
        switch (row->section) {
            case ServerTree::Section::Direct:
                connectDirect(m_qol.direct[static_cast<std::size_t>(row->index)], true);
                break; // added server: look up its host, then connect
            case ServerTree::Section::List: // SERVER.INI server: connect at once
                m_callbacks.connect(m_servers.groups()[static_cast<std::size_t>(row->group)].servers[static_cast<std::size_t>(row->index)]);
                break;
        }
    }

    void ServerSelectForm::connectDirect(const DirectServer& server, bool lookUpAgain) {
        if (lookUpAgain) {
            m_resolver.request(server.host);
        }
        const net::HostResolver::Result found = m_resolver.result(server.host);
        if (found.state == net::HostResolver::State::Pending) {
            m_pendingConnect = server;
            showMessage("Looking up " + server.host + "...");
            return;
        }
        m_pendingConnect.reset();
        if (found.state != net::HostResolver::State::Found) {
            showMessage("Cannot find " + server.host);
            return;
        }
        formats::ServerEntry entry;
        entry.name = server.name;
        entry.address = found.address;
        entry.port = server.port;
        m_callbacks.connect(entry);
    }

    void ServerSelectForm::update() {
        if (!isVisible()) {
            return;
        }
        for (auto& channel : m_groupStatus) {
            channel->update();
        }
        for (const DirectServer& server : m_qol.direct) {
            const net::HostResolver::Result found = m_resolver.result(server.host);
            if (server.port == net::GameConnection::kGamePort && found.state == net::HostResolver::State::Found && m_directStatus.count(found.address) == 0) {
                auto channel = std::make_unique<ServerStatusChannel>();
                channel->query(found.address);
                m_directStatus[found.address] = std::move(channel);
            }
        }
        for (auto& [address, channel] : m_directStatus) {
            channel->update();
        }
        if (m_pendingConnect && m_resolver.result(m_pendingConnect->host).state != net::HostResolver::State::Pending) {
            const DirectServer server = *m_pendingConnect;
            connectDirect(server, false);
        }
        m_messageLabel.setText(m_message);
    }

    std::string ServerSelectForm::nameOf(const Row& row) const {
        switch (row.section) {
            case ServerTree::Section::Direct: return m_qol.direct[static_cast<std::size_t>(row.index)].name; // added server's name
            case ServerTree::Section::List: break; // SERVER.INI server: name read below
        }
        return m_servers.groups()[static_cast<std::size_t>(row.group)].servers[static_cast<std::size_t>(row.index)].name;
    }

    std::string ServerSelectForm::addressOf(const Row& row) const {
        switch (row.section) {
            case ServerTree::Section::Direct: return m_qol.direct[static_cast<std::size_t>(row.index)].address(); // added server's host and port
            case ServerTree::Section::List: break; // SERVER.INI server: address read below
        }
        return m_servers.groups()[static_cast<std::size_t>(row.group)].servers[static_cast<std::size_t>(row.index)].address;
    }

    std::optional<int> ServerSelectForm::statusOf(const Row& row) const {
        switch (row.section) {
            case ServerTree::Section::Direct: { // added server: status from its own channel
                const DirectServer& server = m_qol.direct[static_cast<std::size_t>(row.index)];
                if (server.port != net::GameConnection::kGamePort) {
                    return std::nullopt;
                }
                const net::HostResolver::Result found = m_resolver.result(server.host);
                const auto channel = m_directStatus.find(found.address);
                if (found.state != net::HostResolver::State::Found || channel == m_directStatus.end() || channel->second->statuses().empty()) {
                    return 0;
                }
                return channel->second->statuses().begin()->second;
            }
            case ServerTree::Section::List: break; // SERVER.INI server: status from its group's channel below
        }
        if (row.group >= static_cast<int>(m_groupStatus.size())) {
            return 0;
        }
        const formats::ServerEntry& server = m_servers.groups()[static_cast<std::size_t>(row.group)].servers[static_cast<std::size_t>(row.index)];
        return m_groupStatus[static_cast<std::size_t>(row.group)]->status(server.id);
    }

    std::string ServerSelectForm::headerText(const Row& row) const {
        const std::string count = " (" + std::to_string(m_tree.count(row)) + ")";
        if (row.kind == Row::Kind::Group) {
            return m_servers.groups()[static_cast<std::size_t>(row.group)].name + count;
        }
        switch (row.section) {
            case ServerTree::Section::Direct: return "Direct IP / DNS" + count; // header of the added servers
            case ServerTree::Section::List: break; // header of SERVER.INI's section below
        }
        return "SERVER.INI" + count;
    }

    void ServerSelectForm::paintRows(gfx::Canvas& canvas, core::Point at) {
        m_rowsAt = at;
        const int page = m_rows.bounds().height / m_rowSpot.size().height;
        const core::Point mouse = m_screen.mouse();
        const std::optional<Row> hovered = isVisible() ? rowAt({mouse.x - at.x, mouse.y - at.y}) : std::nullopt;
        const gfx::TextEffect effect = gfx::TextEffect::Shadow;
        for (int line = 0; line < page && m_top + line < static_cast<int>(m_tree.rows().size()); ++line) {
            const Row& row = m_tree.rows()[static_cast<std::size_t>(m_top + line)];
            const int y = at.y + line * m_rowSpot.size().height;
            const core::Rect band{at.x, y, m_rows.bounds().width, m_rowSpot.size().height};
            if (m_tree.selected() && *m_tree.selected() == row) {
                canvas.fillRectAlpha(band, m_selectedColorSpot.color());
            } else if (hovered && *hovered == row) {
                canvas.fillRectAlpha(band, m_hoverColorSpot.color());
            }
            const int textY = y + 3; // text top offset inside a row
            switch (row.kind) {
                case Row::Kind::Section: // section header with its fold arrow
                    drawArrow(canvas, {at.x + 4, y + 2}, m_tree.isCollapsed(row)); // section arrow offset (4, 2) in the row
                    canvas.drawText(
                        {at.x + m_sectionAtSpot.at().x, textY},
                        fitted(headerText(row), band.width - m_sectionAtSpot.at().x),
                        {m_sectionColorSpot.color(), effect, std::nullopt}
                    );
                    break;
                case Row::Kind::Group: { // SERVER.INI group header with its fold arrow
                    const formats::ServerGroup& group = m_servers.groups()[static_cast<std::size_t>(row.group)];
                    drawArrow(canvas, {at.x + m_groupArrowAtSpot.at().x, y + 2}, m_tree.isCollapsed(row)); // arrow y offset 2 in the row
                    canvas.drawText(
                        {at.x + m_groupTextAtSpot.at().x, textY},
                        fitted(headerText(row), band.width - m_groupTextAtSpot.at().x),
                        {group.color.value_or(m_groupColorSpot.color()), effect, std::nullopt}
                    );
                    break;
                }
                case Row::Kind::Server: { // server line: name, address, status icon
                    const int nameX = row.section == ServerTree::Section::List ? m_groupNameAtSpot.at().x : m_nameAtSpot.at().x;
                    std::optional<core::Color> color;
                    if (row.section == ServerTree::Section::List) {
                        color = m_servers.groups()[static_cast<std::size_t>(row.group)].servers[static_cast<std::size_t>(row.index)].color;
                    }
                    canvas.drawText(
                        {at.x + nameX, textY},
                        fitted(nameOf(row), m_addressAtSpot.at().x - nameX - 4),
                        {color.value_or(m_nameColorSpot.color()), effect, std::nullopt}
                    ); // 4 px gap before the address column
                    canvas.drawText(
                        {at.x + m_addressAtSpot.at().x, textY},
                        fitted(addressOf(row), m_iconSpot.at().x - m_addressAtSpot.at().x - 2),
                        {m_addressColorSpot.color(), effect, std::nullopt}
                    ); // 2 px gap before the icon column
                    if (const std::optional<int> status = statusOf(row)) {
                        if (const auto icon = m_services.pictures.find("icon_serverSignal")) {
                            canvas.drawPicture(
                                *icon,
                                {at.x + m_iconSpot.at().x, y + 1},
                                core::Rect{0, *status * m_iconSpot.size().width, m_iconSpot.size().width, m_iconSpot.size().height}
                            ); // icon y offset 1 in the row; frames stacked vertically, one per status
                        }
                    }
                    break;
                }
            }
        }
    }

    void ServerSelectForm::paintTooltip(gfx::Canvas& canvas) {
        if (!isVisible()) {
            return;
        }
        const core::Point at = m_rowsAt;
        const core::Point mouse = m_screen.mouse();
        const std::optional<Row> row = rowAt({mouse.x - at.x, mouse.y - at.y});
        if (!row || row->kind != Row::Kind::Server) {
            return;
        }
        const int line = (mouse.y - at.y) / m_rowSpot.size().height;
        const core::Rect icon{
            at.x + m_iconSpot.at().x,
            at.y + line * m_rowSpot.size().height + 1,
            m_iconSpot.size().width,
            m_iconSpot.size().height
        }; // icon y offset 1 in the row
        if (const std::optional<int> status = statusOf(*row);
            status && icon.contains(mouse) && *status >= 0 && *status < static_cast<int>(kStatusHints.size())) {
            ui::drawTooltipAt(canvas, {icon.x + m_iconSpot.pitch().x, icon.y}, kStatusHints[static_cast<std::size_t>(*status)]);
        }
    }

}
