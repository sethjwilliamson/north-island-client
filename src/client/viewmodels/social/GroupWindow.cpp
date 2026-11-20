#include "client/viewmodels/social/GroupWindow.h"
#include "client/rules/ElementNames.h"

#include "client/rendering/hud/GameNumbers.h"
#include "client/rules/TextFilter.h"
#include "client/rules/Vitality.h"
#include "core/Log.h"
#include "gfx/TextStyle.h"
#include "protocol/PetRequests.h"
#include "protocol/TeamRequests.h"
#include "ui/Tooltip.h"

#include <array>
#include <cmath>
#include <string>

namespace client {

    namespace {

        constexpr int kRowPitch = 34; // y step between pet rows, px
        constexpr core::Point kPetInfo{36, 72}; // first pet row info block position inside the window, px
        constexpr int kRest = 0; // pet assign state 0: rest
        constexpr int kFight = 1; // pet assign state 1: fight
        constexpr int kRide = 2; // pet assign state 2: ride
        constexpr int kRoleExclusiveGear = 149; // item sub type 149: role gear (no name table for sub types)

        /// @brief Loads GroupWindow.xml into the screen and returns its window.
        /// @param services Shared game services (layout and load context).
        /// @param screen Screen the layout is loaded into.
        /// @return The GroupWindow window.
        ui::Window& loadWindow(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("GroupWindow.xml"), services.loadContext());
            return screen.get<ui::Window>("GroupWindow");
        }

        /// @brief Reads the number that follows a prefix in an element name.
        /// @param name Element name such as "GroupPetName2".
        /// @param prefix The name prefix to strip.
        /// @return The number after the prefix, 0 when the name does not start with the prefix.
        int numberAfter(const std::string& name, const std::string& prefix) { return name.rfind(prefix, 0) == 0 ? std::stoi(name.substr(prefix.size())) : 0; }

        /// @brief Gives the top left corner of a pet row's info block.
        /// @param window Top left corner of the window.
        /// @param order Pet order (1 to 4).
        /// @return The corner of the info block.
        core::Point rowInfo(core::Point window, int order) { return {window.x + kPetInfo.x, window.y + kPetInfo.y + (order - 1) * kRowPitch}; }

        constexpr int kMatePortraitSlot = 2; // portrait pose slot drawn for a teammate
        constexpr int kTimedQuestionMs = 5000; // time a question stays open, ms
        const std::array<const char*, 7> kJobIcons = {"", "Icon_Asn", "Icon_Samurai", "Icon_Knight", "Icon_Sage", "Icon_Clergyman", "Icon_Prophet"};
        const std::array<const char*, 7> kJobNames = {"", "Killer ", "Warrior ", "Knight ", "Wit ", "Priest ", "Seer "};

        /// @brief Tells whether a job number has an icon and a name.
        /// @param job Job number.
        /// @return True for jobs 1 to 6.
        bool validJob(int job) { return job >= 1 && job < static_cast<int>(kJobIcons.size()); }

    }

    GroupWindow::GroupWindow(
        GameServices& services,
        ui::Screen& screen,
        PetTeam& pets,
        const Teams& teams,
        Players players,
        PlayerMessages& messages,
        TalkDialog& talk,
        GuildTab guildTab,
        InstanceTab instanceTab
    )
        : m_services(services),
          m_screen(screen),
          m_pets(pets),
          m_teams(teams),
          m_players(std::move(players)),
          m_messages(messages),
          m_talk(talk),
          m_guildTab(std::move(guildTab)),
          m_instanceTab(std::move(instanceTab)),
          m_window(loadWindow(services, screen)),
          m_commands(screen.get<ui::Window>("PetCommand")),
          m_leave(screen.get<ui::ImageButton>("GroupLeaveTeam")),
          m_guildButton(screen.get<ui::ImageButton>("GroupTabGuild")),
          m_instancesButton(screen.get<ui::ImageButton>("GroupTabInstances")),
          m_partyMirrorButton(screen.get<ui::ImageButton>("GroupPartyMirror")),
          m_instancesAloneSpot(screen.get<ui::Spot>("GroupInstancesAlone")),
          m_instancesWithGuildSpot(screen.get<ui::Spot>("GroupInstancesWithGuild")),
          m_petPartsSpot(screen.get<ui::Spot>("GroupPetParts")),
          m_commandSpot(screen.get<ui::Spot>("GroupCommandAt")),
          m_mateRowSpot(screen.get<ui::Spot>("GroupMateRow")),
          m_matePortraitSpot(screen.get<ui::Spot>("GroupMatePortrait")),
          m_nameColorSpot(screen.get<ui::Spot>("GroupNameColor")),
          m_fixedNameColorSpot(screen.get<ui::Spot>("GroupFixedNameColor")),
          m_editingColorSpot(screen.get<ui::Spot>("GroupEditingColor")) {
        for (int slot = 1; slot <= 3; ++slot) { // three teammate rows besides the own player
            const std::string n = std::to_string(slot);
            m_mateRows.push_back(
                {screen.get<ui::ImageButton>("GroupMateKick" + n),
                 screen.get<ui::ImageButton>("GroupMateLead" + n),
                 screen.get<ui::DrawingArea>("GroupMateJob" + n),
                 screen.get<ui::DrawingArea>("GroupMateElement" + n)}
            );
            m_mateRows.back().job.get().setPainter([this, slot](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                const auto mate = m_players.mate(shownMate(slot));
                if (mate && validJob(mate->job)) {
                    if (const auto icon = m_services.pictures.find(kJobIcons[static_cast<std::size_t>(mate->job)])) {
                        canvas.drawPicture(*icon, at);
                    }
                }
            });
            m_mateRows.back().element.get().setPainter([this, slot](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                if (const auto mate = m_players.mate(shownMate(slot))) {
                    if (const auto icon = m_services.pictures.find("Icon_Element_" + std::to_string(mate->element) + "_1")) {
                        canvas.drawPicture(*icon, at);
                    }
                }
            });
        }
        screen.get<ui::DrawingArea>("GroupTeam").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintTeam(canvas, at); });
        screen.on("onGroupLeaveTeam", [this](const ui::EventArgs&) { askLeave(); });
        screen.on("onGroupMateKick", [this](const ui::EventArgs& args) { askKick(numberAfter(args.source, "GroupMateKick")); });
        screen.on("onGroupMateLead", [this](const ui::EventArgs& args) { askLead(numberAfter(args.source, "GroupMateLead")); });
        screen.on("onGroupPartyMirror", [this](const ui::EventArgs&) {
            if (m_services.partyMirrorsNpcs && m_services.setPartyMirrorsNpcs) {
                m_services.setPartyMirrorsNpcs(!m_services.partyMirrorsNpcs());
                refreshTeam();
            }
        });
        for (int order = 1; order <= PetTeam::kPlaces; ++order) {
            const std::string n = std::to_string(order);
            m_rows.push_back(
                {screen.get<ui::TextBox>("GroupPetName" + n),
                 screen.get<ui::Element>("GroupPetNameBack" + n),
                 screen.get<ui::DrawingArea>("GroupPetElement" + n),
                 screen.get<ui::ImageButton>("GroupPetAssign" + n),
                 screen.get<ui::ImageButton>("GroupPetExile" + n)}
            );
            m_petInfos.emplace_back(services, PetInfoPainter::Style{m_petPartsSpot.at(), true, false});
            ui::DrawingArea& element = m_rows.back().element;
            element.setPainter([this, order](gfx::Canvas& canvas, core::Point at, std::uint64_t) {
                const auto& pet = m_pets.at(m_pets.placeByOrder(order));
                if (!pet) {
                    return;
                }
                if (const auto icon = m_services.pictures.find("Icon_Element_" + std::to_string(pet->info.element) + "_1")) {
                    canvas.drawPicture(*icon, at);
                }
            });
        }
        screen.get<ui::ImageButton>("GroupTabTeam").setSelected(true);
        screen.get<ui::DrawingArea>("GroupPets").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) { paintPets(canvas, at, nowMs); });
        screen.get<ui::DrawingArea>("GroupHints").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintHints(canvas, at); });
        screen.on("onGroupWindowClose", [this](const ui::EventArgs&) { close(); });
        screen.on("onGroupWindowClick", [this](const ui::EventArgs&) {
            closeCommands();
            commitRename();
        });
        screen.on("onGroupInstances", [this](const ui::EventArgs&) {
            const core::Rect bounds = m_window.bounds();
            showInstances({bounds.x, bounds.y});
        });
        screen.on("onGroupGuild", [this](const ui::EventArgs&) { showGuild(); });
        screen.on("onGroupPetAssign", [this](const ui::EventArgs& args) { openCommands(numberAfter(args.source, "GroupPetAssign")); });
        screen.on("onGroupPetExile", [this](const ui::EventArgs& args) { askExile(numberAfter(args.source, "GroupPetExile")); });
        screen.on("onGroupPetName", [this](const ui::EventArgs& args) { beginRename(numberAfter(args.source, "GroupPetName")); });
        screen.on("onGroupPetRenamed", [this](const ui::EventArgs&) { commitRename(); });
        screen.on("onPetCommand", [this](const ui::EventArgs& args) { command(numberAfter(args.source, "PetCommand")); });
    }

    void GroupWindow::open() {
        m_guildTab.shown();
        refresh();
        if (m_onInstanceTab) {
            m_instanceTab.open(m_instanceTab.position());
            return;
        }
        if (m_onGuildTab && m_guildTab.exists()) {
            m_guildTab.open(m_guildTab.position());
            return;
        }
        m_onGuildTab = false;
        m_window.setVisible(true);
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(m_window.id());
        }
    }

    void GroupWindow::close() {
        commitRename();
        closeCommands();
        m_window.setVisible(false);
        m_guildTab.close();
        m_instanceTab.close();
    }

    void GroupWindow::showGuild() {
        commitRename();
        closeCommands();
        const core::Rect bounds = m_window.bounds();
        m_onGuildTab = true;
        m_window.setVisible(false);
        m_guildTab.open({bounds.x, bounds.y});
    }

    void GroupWindow::showParty(core::Point at) {
        m_onGuildTab = false;
        m_onInstanceTab = false;
        m_guildTab.close();
        m_instanceTab.close();
        const core::Rect bounds = m_window.bounds();
        m_window.setBounds({at.x, at.y, bounds.width, bounds.height});
        open();
    }

    void GroupWindow::showGuildAt(core::Point at) {
        m_onInstanceTab = false;
        m_instanceTab.close();
        if (!m_guildTab.exists()) {
            return;
        }
        m_onGuildTab = true;
        m_window.setVisible(false);
        m_guildTab.open(at);
    }

    void GroupWindow::showInstances(core::Point at) {
        commitRename();
        closeCommands();
        m_onGuildTab = false;
        m_onInstanceTab = true;
        m_window.setVisible(false);
        m_guildTab.close();
        m_instanceTab.open(at);
    }

    void GroupWindow::guildGone() {
        if (m_guildTab.isOpen()) {
            showParty(m_guildTab.position());
        }
        m_onGuildTab = false;
        refresh();
    }

    void GroupWindow::toggle() {
        if (isOpen()) {
            close();
        } else {
            open();
        }
    }

    void GroupWindow::setAssignPicture(int order, int state) {
        m_assignStates[static_cast<std::size_t>(order)] = state;
        const char* name = state == kFight ? "btn_AssignFight_1" : state == kRide ? "Btn_AssignRide_1" : "btn_AssignRest_1";
        m_rows[static_cast<std::size_t>(order - 1)].assign.get().setPicture(m_services.pictures.find(name).value_or(gfx::PictureId{}));
    }

    void GroupWindow::refresh() {
        const bool inGuild = m_guildTab.exists();
        m_guildButton.setVisible(inGuild);
        const core::Rect instances = m_instancesButton.bounds();
        m_instancesButton.setBounds({inGuild ? m_instancesWithGuildSpot.at().x : m_instancesAloneSpot.at().x, instances.y, instances.width, instances.height});
        const int count = m_pets.count();
        for (int order = 1; order <= PetTeam::kPlaces; ++order) {
            PetRow& row = m_rows[static_cast<std::size_t>(order - 1)];
            setAssignPicture(order, kRest);
            const int place = order <= count ? m_pets.placeByOrder(order) : 0;
            const auto& pet = m_pets.at(place);
            row.name.get().setVisible(pet.has_value());
            row.element.get().setVisible(pet.has_value());
            row.exile.get().setVisible(order <= count);
            row.assign.get().setVisible(false);
            if (!pet) {
                continue;
            }
            const bool fights = pet->tradable == 1 || pet->tradable == 2;
            row.assign.get().setVisible(fights || m_pets.activePlace() != 0);
            if (place == m_pets.activePlace()) {
                setAssignPicture(order, kFight);
            }
            if (place == m_pets.ridingPlace()) {
                setAssignPicture(order, kRide);
            } else if (place != m_pets.activePlace() && m_pets.cityBattleDead(place)) {
                row.assign.get().setPicture(m_services.pictures.find("btn_Dead").value_or(gfx::PictureId{}));
            }
            row.name.get().setHint(pet->tradable == 1 ? "Able to rename" : "");
            if (row.name.get().isReadOnly()) {
                row.name.get().setFontColor(pet->tradable == 1 ? m_nameColorSpot.color() : m_fixedNameColorSpot.color());
                row.name.get().setTextFromStart(pet->name);
            }
            row.element.get().setHint(elementName(pet->info.element));
        }
        if (m_renameOrder != 0 && !m_pets.at(m_pets.placeByOrder(m_renameOrder))) {
            commitRename();
        }
        if (m_commandOrder != 0 && !m_pets.at(m_pets.placeByOrder(m_commandOrder))) {
            closeCommands();
        }
        refreshTeam();
    }

    void GroupWindow::refreshTeam() {
        const std::int32_t own = m_players.ownId();
        const auto leader = m_teams.leaderOf(own);
        m_memberIds.fill(0);
        m_leave.setVisible(leader.has_value());
        const bool mirrors = m_services.partyMirrorsNpcs && m_services.partyMirrorsNpcs();
        m_partyMirrorButton.setVisible(m_services.partyMirrorsNpcs && m_services.videoSettings().partyLogin != ExtraSettings::PartyLogin::Off);
        m_partyMirrorButton.setText(mirrors ? "Mirror on" : "Mirror off");
        m_partyMirrorButton.setSelected(mirrors);
        for (MateRow& row : m_mateRows) {
            row.kick.get().setVisible(false);
            row.lead.get().setVisible(false);
        }
        if (leader) {
            const std::vector<std::int32_t>& members = m_teams.members(*leader);
            const bool leading = *leader == own;
            m_leave.setPicture(m_services.pictures.find(leading ? "btn_DismissTeam_1" : "btn_LeaveTeam_1").value_or(gfx::PictureId{}));
            m_leave.setHint(leading ? "Disband the team" : "Leave the team");
            if (leading) {
                for (std::size_t i = 0; i < members.size() && i < 3; ++i) { // at most 3 members are shown
                    m_memberIds[i + 1] = members[i];
                    m_mateRows[i].kick.get().setVisible(true);
                    m_mateRows[i].lead.get().setVisible(true);
                }
            } else {
                m_memberIds[0] = *leader;
                std::size_t next = 1;
                for (std::int32_t member : members) {
                    if (member != own && next < m_memberIds.size()) {
                        m_memberIds[next++] = member;
                    }
                }
            }
        }
        for (int slot = 1; slot <= 3; ++slot) { // three teammate rows
            const auto mate = m_players.mate(shownMate(slot));
            MateRow& row = m_mateRows[static_cast<std::size_t>(slot - 1)];
            row.job.get().setVisible(mate.has_value() && validJob(mate->job));
            row.job.get().setHint(mate && validJob(mate->job) ? kJobNames[static_cast<std::size_t>(mate->job)] : "");
            row.element.get().setVisible(mate.has_value());
            row.element.get().setHint(mate ? elementName(mate->element) : "");
        }
    }

    std::int32_t GroupWindow::shownMate(int index) const {
        const std::int32_t own = m_players.ownId();
        int count = 0;
        for (std::int32_t id : m_memberIds) {
            if (id != 0 && id != own && ++count == index) {
                return id;
            }
        }
        return 0;
    }

    void GroupWindow::askLeave() {
        const std::int32_t own = m_players.ownId();
        const auto leader = m_teams.leaderOf(own);
        if (!leader) {
            return;
        }
        const std::int32_t leaderId = *leader;
        const auto send = [this, leaderId] { m_services.connection.send(protocol::leaveTeamRequest(leaderId)); };
        const bool leading = leaderId == own;
        const auto speaker = m_players.mate(leaderId);
        std::optional<TalkDialog::Speaker> who;
        if (speaker) {
            who = TalkDialog::Speaker{speaker->sceneLook, speaker->name};
        }
        m_talk.ask(leading ? "Are you sure you want to disband the team?" : "Are you sure you want to leave the team?", who, send, nullptr);
    }

    void GroupWindow::askKick(int slot) {
        const std::int32_t memberId = slot >= 1 && slot <= 3 ? m_memberIds[static_cast<std::size_t>(slot)] : 0; // valid slots are 1 to 3
        const auto mate = m_players.mate(memberId);
        if (!mate) {
            return;
        }
        m_talk.ask(
            "Are you sure to invite? " + mate->name + " Are you leaving the team?",
            TalkDialog::Speaker{mate->sceneLook, mate->name},
            [this, memberId] { m_services.connection.send(protocol::kickMemberRequest(memberId)); },
            nullptr,
            {40, kTimedQuestionMs, nullptr}
        ); // 40: question line length in characters
    }

    bool GroupWindow::mayOfferLead() {
        if (const auto refusal = m_players.leaderRefusal(m_newLeaderId != 0)) {
            m_messages.show(refusal->text, refusal->ms);
            return false;
        }
        return true;
    }

    void GroupWindow::appointLeader(std::int32_t memberId) {
        if (memberId == 0 || !m_players.mate(memberId) || m_players.ownId() != m_teams.leaderOf(m_players.ownId()).value_or(0) || !mayOfferLead()) {
            return;
        }
        m_newLeaderId = memberId;
        m_services.connection.send(protocol::offerLeadRequest(memberId));
    }

    void GroupWindow::askLead(int slot) {
        const std::int32_t memberId = slot >= 1 && slot <= 3 ? m_memberIds[static_cast<std::size_t>(slot)] : 0; // valid slots are 1 to 3
        const auto mate = m_players.mate(memberId);
        if (!mate || m_players.ownId() != m_teams.leaderOf(m_players.ownId()).value_or(0) || !mayOfferLead()) {
            return;
        }
        m_talk.ask(
            "Are you sure to invite? " + mate->name + " Want to be the new team leader?",
            TalkDialog::Speaker{mate->sceneLook, mate->name},
            [this, memberId] {
                if (m_players.ownId() == m_teams.leaderOf(m_players.ownId()).value_or(0) && mayOfferLead()) {
                    m_newLeaderId = memberId;
                    m_services.connection.send(protocol::offerLeadRequest(memberId));
                }
            },
            nullptr,
            {40, kTimedQuestionMs, nullptr}
        ); // 40: question line length in characters
    }

    void GroupWindow::paintTeam(gfx::Canvas& canvas, core::Point at) {
        const auto draw = [&](const char* name, core::Point where) {
            if (const auto id = m_services.pictures.find(name)) {
                canvas.drawPicture(*id, where);
            }
        };
        const auto gauge = [&](const char* frame, const char* bar, core::Point framePos, core::Point barPos, double value, double most) {
            draw(frame, framePos);
            const auto id = m_services.pictures.find(bar);
            if (!id || most <= 0) {
                return;
            }
            const gfx::Picture& picture = m_services.pictures.picture(*id);
            const int width = static_cast<int>(std::ceil(picture.width() * value / most));
            if (width > 0) {
                canvas.drawPicture(*id, barPos, core::Rect{0, 0, width, picture.height()});
            }
        };
        for (int slot = 1; slot <= 3; ++slot) { // three teammate rows
            const auto mate = m_players.mate(shownMate(slot));
            if (!mate) {
                continue;
            }
            const int x = at.x + m_mateRowSpot.at().x;
            const int y = at.y + m_mateRowSpot.at().y + (slot - 1) * m_mateRowSpot.pitch().y;
            m_services.characters.drawPortrait(canvas, mate->look, {x + m_matePortraitSpot.at().x, y + m_matePortraitSpot.at().y}, 0, kMatePortraitSlot);
            // pixel offsets inside a teammate row: name at +75 (box 138 x 20), level digits at +169,16, job icon at +76,44
            canvas.drawText({x + 75, y}, mate->name, {core::Color::fromRgb565(2113), gfx::TextEffect::Plain, std::nullopt}, 138, 20);
            drawLevelDigits(canvas, mate->level, {x + 169, y + 16});
            if (validJob(mate->job)) {
                draw("Icon_Career", {x + 76, y + 44});
            }
            // pixel offsets: HP/SP label at +77,17 / +77,31; gauge frame at +100,19 / +100,33 and bar inside it at +102,21 / +102,35
            draw("Word_Hp_2", {x + 77, y + 17});
            gauge("Panel34_1", "Panel31_1", {x + 100, y + 19}, {x + 102, y + 21}, mate->hp, mate->maxHp);
            draw("Word_Sp_2", {x + 77, y + 31});
            gauge("Panel35_1", "Panel32_1", {x + 100, y + 33}, {x + 102, y + 35}, mate->sp, mate->maxSp);
        }
    }

    void GroupWindow::openCommands(int order) {
        if (!m_pets.at(m_pets.placeByOrder(order))) {
            return;
        }
        m_commandOrder = order;
        m_commands.setVisible(true);
    }

    void GroupWindow::closeCommands() {
        m_commandOrder = 0;
        m_commands.setVisible(false);
    }

    void GroupWindow::command(int choice) {
        const int order = m_commandOrder;
        closeCommands();
        const int place = m_pets.placeByOrder(order);
        const auto& pet = m_pets.at(place);
        if (!pet) {
            return;
        }
        if (m_pets.cityBattleDead(place) && choice - 1 != kRide) {
            m_messages.show("Dead pets cannot join again during a city battle", 1200); // message display time in ms
            return;
        }
        switch (choice - 1) {
            case kRest: // command 1: rest, calls back the fighting pet and gets off a ridden pet
                if (m_pets.activePlace() == place) {
                    m_services.connection.send(protocol::petRestRequest());
                }
                dismount(place);
                break;
            case kFight: // command 2: fight, sends this pet out and gets off a ridden pet
                if (m_pets.activePlace() != place) {
                    m_services.connection.send(protocol::petFightRequest(pet->info.npcId));
                }
                dismount(place);
                break;
            case kRide: // command 3: ride this pet
                ride(place);
                break;
            default: // unknown command: nothing
                break;
        }
        commitRename();
    }

    void GroupWindow::ride(int place) {
        const auto& pet = m_pets.at(place);
        if (!pet) {
            return;
        }
        if (m_pets.cityBattleDead(place)) {
            m_messages.show("Unable to ride", 1200); // message display time in ms
            return;
        }
        if (m_players.fighting()) {
            m_messages.show("Unable to click during the combat", 2000); // message display time in ms
            return;
        }
        if (m_players.usesVehicle()) {
            m_messages.show("Unable to use a vehicle while riding a pet", 2000); // message display time in ms
            return;
        }
        if (m_players.keepsStall()) {
            m_messages.show("Unable to ride pet while keeping stall", 2000); // message display time in ms
            return;
        }
        if (m_pets.activePlace() == place) {
            m_services.connection.send(protocol::petRestRequest());
        }
        m_services.connection.send(protocol::ridePetRequest(place, pet->info.npcId));
    }

    void GroupWindow::dismount(int place) {
        const auto& pet = m_pets.at(place);
        if (pet && m_pets.ridingPlace() == place) {
            m_services.connection.send(protocol::dismountPetRequest(place, pet->info.npcId));
        }
    }

    void GroupWindow::askExile(int order) {
        closeCommands();
        const int place = m_pets.placeByOrder(order);
        const auto& pet = m_pets.at(place);
        if (!pet) {
            return;
        }
        if (m_pets.cityBattleDead(place)) {
            m_messages.show("A dead pet cannot be dismissed during a city battle", 1200); // message display time in ms
            return;
        }
        if (m_pets.ridingPlace() == place) {
            m_messages.show("Cannot to dismiss while mounting!", 1500); // message display time in ms
            return;
        }
        int gear = 0;
        for (int itemId : pet->equipmentIds()) {
            const auto record = m_services.items.find(itemId);
            if (!record || record->petNpcId == 0 || record->subType == kRoleExclusiveGear) {
                ++gear;
            }
        }
        if (gear > 0) {
            m_messages.show("Please take off " + pet->name + " gear, and then exile it", 2000); // message display time in ms
            return;
        }
        const int npcId = pet->info.npcId;
        m_talk.ask(
            "Are you sure you want to exile? " + pet->name + "\xA1H",
            TalkDialog::Speaker{pet->info, pet->name},
            [this, npcId] {
                const int now = m_pets.placeOf(npcId);
                if (now != 0) {
                    m_services.connection.send(protocol::exilePetRequest(now));
                }
            },
            nullptr
        );
    }

    void GroupWindow::beginRename(int order) {
        closeCommands();
        if (m_renameOrder == order) {
            return;
        }
        commitRename();
        const auto& pet = m_pets.at(m_pets.placeByOrder(order));
        if (!pet || pet->tradable != 1) {
            return;
        }
        PetRow& row = m_rows[static_cast<std::size_t>(order - 1)];
        row.name.get().setReadOnly(false);
        row.name.get().setFontColor(m_editingColorSpot.color());
        row.nameBack.get().setVisible(true);
        m_nameBeforeRename = row.name.get().text();
        m_renameOrder = order;
        m_screen.focus(row.name.get().name());
    }

    void GroupWindow::commitRename() {
        if (m_renameOrder == 0) {
            return;
        }
        PetRow& row = m_rows[static_cast<std::size_t>(m_renameOrder - 1)];
        const std::string typed = row.name.get().text();
        const int place = m_pets.placeByOrder(m_renameOrder);
        if (!typed.empty() && typed != m_nameBeforeRename && isTextClean(typed) && m_pets.at(place)) {
            m_services.connection.send(protocol::renamePetRequest(place, typed));
        }
        row.name.get().setReadOnly(true);
        row.nameBack.get().setVisible(false);
        m_renameOrder = 0;
        m_nameBeforeRename.clear();
        refresh();
    }

    void GroupWindow::paintPets(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs) {
        for (int order = 1; order <= m_pets.count(); ++order) {
            const auto& pet = m_pets.at(m_pets.placeByOrder(order));
            if (!pet) {
                break;
            }
            m_petInfos[static_cast<std::size_t>(order - 1)].paint(canvas, rowInfo(at, order), *pet, nowMs);
        }
        if (m_commandOrder != 0) {
            const core::Rect bounds = m_commands.bounds();
            m_commands.setBounds(
                {at.x + m_commandSpot.at().x, at.y + m_commandSpot.at().y - bounds.height + (m_commandOrder - 1) * kRowPitch, bounds.width, bounds.height}
            );
        }
    }

    void GroupWindow::paintHints(gfx::Canvas& canvas, core::Point at) {
        if (m_screen.hoveredId() != m_window.id()) {
            return;
        }
        const core::Point mouse = m_screen.mouse();
        std::string hint;
        core::Point hintAnchor;
        for (int order = 1; order <= m_pets.count(); ++order) {
            const auto& pet = m_pets.at(m_pets.placeByOrder(order));
            if (!pet) {
                break;
            }
            const core::Point info = rowInfo(at, order);
            const PetInfoPainter& painter = m_petInfos[static_cast<std::size_t>(order - 1)];
            const core::Rect hp = painter.hpBar(info);
            const core::Rect sp = painter.spBar(info);
            if (hp.contains(mouse)) {
                hint = "Hp:" + std::to_string(pet->hp) + "/" + std::to_string(pet->maxHp);
                hintAnchor = {hp.x, hp.y + hp.height};
            } else if (sp.contains(mouse)) {
                hint = "Sp:" + std::to_string(pet->sp) + "/" + std::to_string(pet->maxSp);
                hintAnchor = {sp.x, sp.y + sp.height};
            }
        }
        for (int slot = 1; slot <= 3; ++slot) { // three teammate rows
            const auto mate = m_players.mate(shownMate(slot));
            if (!mate) {
                continue;
            }
            // HP/SP gauge hit boxes of a teammate row: x 130, y 282 / 296, row pitch 74, size 64 x 8
            const core::Rect hp{at.x + 130, at.y + 282 + (slot - 1) * 74, 64, 8};
            const core::Rect sp{at.x + 130, at.y + 296 + (slot - 1) * 74, 64, 8};
            if (hp.contains(mouse)) {
                hint = "Hp:" + std::to_string(mate->hp) + "/" + std::to_string(mate->maxHp);
                hintAnchor = {hp.x, hp.y + hp.height};
            } else if (sp.contains(mouse)) {
                hint = "Sp:" + std::to_string(mate->sp) + "/" + std::to_string(mate->maxSp);
                hintAnchor = {sp.x, sp.y + sp.height};
            }
        }
        if (!hint.empty()) {
            ui::drawTooltipAt(canvas, hintAnchor, hint);
        }
    }

}
