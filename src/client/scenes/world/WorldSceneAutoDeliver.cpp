#include "client/scenes/world/WorldScene.h"
#include "protocol/ExpressRequests.h"

#include <algorithm>

namespace client {

    AutoDeliver::Hooks WorldScene::autoDeliverHooks() {
        AutoDeliver::Hooks hooks;
        hooks.running = [this] { return m_remote.isOn() && m_remote.isElectron(); };
        hooks.canSend = [this] {
            return m_inWorld && m_fightState == 0 && !m_battle.isOn() && !screenFrozen() && !m_itemTrade.isOpen() && !m_express.isBusy() &&
                   !expressUseRefusal();
        };
        hooks.findPlayer = [this](const std::string& name) {
            const auto id = worldPlayerNamed(name);
            return id == m_player.id ? std::nullopt : id; // never oneself
        };
        hooks.freePet = [this](int preferred) {
            for (int place = 1; place <= PetTeam::kPlaces; ++place) {
                const auto& pet = m_pets.at(place);
                if (!pet || pet->info.type != PetTeam::kExpressType || (preferred != 0 && pet->info.npcId != preferred) || m_pets.ridingPlace() == place ||
                    m_pets.cityBattleDead(place)) {
                    continue;
                }
                if (!petExpressRefusal(place)) {
                    return pet->info.npcId;
                }
            }
            return 0;
        };
        hooks.petName = [this](int kind) {
            const int place = m_pets.placeOf(kind);
            return place != 0 && m_pets.at(place) ? m_pets.at(place)->name : std::string("The pet");
        };
        hooks.lockedPlaces = [this] { return m_itemWindow.lockedPlaces(); };
        hooks.send = [this](std::int32_t pet, std::int32_t target, const std::vector<std::pair<int, int>>& stacks) {
            m_services.connection.send(protocol::expressStartRequest(pet, target, stacks));
        };
        hooks.recall = [this] { m_services.connection.send(protocol::expressStopRequest()); };
        hooks.chat = [this](const std::string& text) { m_chat.add(10, text); }; // chat channel 10: system line
        hooks.marksChanged = [this] {
            bagMarksChanged();
            m_express.refresh();
        };
        return hooks;
    }

    AutoDeliverWindow::Hooks WorldScene::autoDeliverWindowHooks() {
        AutoDeliverWindow::Hooks hooks;
        hooks.remoteFormOpen = [this] { return m_remoteWindow.isOpen() && m_remote.isElectron(); };
        hooks.ownName = [this] { return m_player.name; };
        hooks.online = [this](const std::string& name) { return worldPlayerNamed(name).has_value(); };
        hooks.names = [this] { return deliveryNames(); };
        hooks.refusal = [this] { return expressUseRefusal(); };
        hooks.showBag = [this](int left) {
            const bool wasOpen = m_itemWindow.isOpen();
            if (!wasOpen) {
                m_itemWindow.toggle();
            }
            const core::Rect bag = m_itemWindow.bounds();
            m_bagBeforeDeliver = BagBeforeDeliver{wasOpen, {bag.x, bag.y}};
            m_itemWindow.setPosition({std::clamp(left, 0, std::max(0, Camera::kScreenWidth - bag.width)), bag.y});
        };
        hooks.putBagBack = [this] {
            if (!m_bagBeforeDeliver) {
                return;
            }
            m_itemWindow.setPosition(m_bagBeforeDeliver->at);
            if (!m_bagBeforeDeliver->open) {
                m_itemWindow.close();
            }
            m_bagBeforeDeliver.reset();
        };
        return hooks;
    }

    std::vector<std::string> WorldScene::deliveryNames() const {
        std::vector<std::string> names;
        const auto add = [&](const std::string& name) {
            if (!name.empty() && name != m_player.name && std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
            }
        };
        if (m_services.otherOwnNames) {
            for (const std::string& name : m_services.otherOwnNames()) {
                add(name);
            }
        }
        for (const std::string& name : teamNames()) {
            add(name);
        }
        for (int index = 1; index <= m_friends.count(); ++index) {
            add(m_friends.at(index).record.name);
        }
        for (int index = 0; index < m_guild.count(); ++index) {
            const protocol::GuildMemberRecord& member = m_guild.at(index).record;
            if (member.id != 0) {
                add(member.name);
            }
        }
        return names;
    }

}
