#include "client/rules/Skills.h"
#include "client/scenes/world/WorldScene.h"

namespace client {

    namespace {

        constexpr int kNotInCombat = 0x100; // item flag bit 0x100: cannot be used in combat
        constexpr int kLockedMs = 2000; // message display time, ms

    }

    RemoteControl::Hooks WorldScene::remoteHooks() {
        RemoteControl::Hooks hooks;
        hooks.fightState = [this] { return m_fightState; };
        hooks.teamStatus = [this] {
            return m_teams.isMember(m_player.id) ? 2 : m_teams.isLeader(m_player.id) ? 1 : 0;
        }; // FTeamStatus: 2 member, 1 leader, 0 none
        hooks.teamLeaderId = [this] { return m_teams.leaderOf(m_player.id).value_or(0); };
        hooks.useBagItem = [this](int slot, int order) { m_itemWindow.requestUse(slot, order, false, true); };
        hooks.unequip = [this](int order, int equipSlot, int bagSlot) { m_itemWindow.unequipFor(order, equipSlot, bagSlot); };
        hooks.dropBagItem = [this](int slot, int count) { m_bagRequests.dropItem(slot, count); };
        hooks.marksChanged = [this] { bagMarksChanged(); };
        hooks.standing = [this] { return !m_player.walking; };
        hooks.worldShown = [this] { return !screenFrozen(); };
        hooks.position = [this] { return m_player.position; };
        hooks.walkTo = [this](core::Point point) { walkOwnPlayer(point); };
        hooks.closeForm = [this] { m_remoteWindow.close(); };
        hooks.playerId = [this] { return m_player.id; };
        hooks.chat = [this](const std::string& text) { m_chat.add(10, text); }; // chat channel 10: system line
        hooks.keptForDelivery = [this](int itemId) { return m_autoDeliver.keeps(itemId); };
        hooks.deliveryStatus = [this] { return m_remote.isElectron() ? m_autoDeliver.status() : std::string(); };
        return hooks;
    }

    RemoteControlWindow::Hooks WorldScene::remoteWindowHooks() {
        RemoteControlWindow::Hooks hooks;
        hooks.level = [this] { return ownLevel(); };
        hooks.enoughOnlineTime = [this] { return m_enoughOnlineTime; };
        hooks.onVehicle = [this] { return m_player.vehicle.has_value(); };
        hooks.playerId = [this] { return m_player.id; };
        hooks.systemMessage = [this](const std::string& text) { m_chat.add(10, text); }; // chat channel 10: system line
        hooks.openDeliver = [this] { m_autoDeliverWindow.toggle(); };
        hooks.starting = [this] { m_autoDeliverWindow.remoteStarting(); };
        return hooks;
    }

    RemotePlusWindow::Hooks WorldScene::remotePlusHooks() {
        RemotePlusWindow::Hooks hooks;
        hooks.skillLevel = [this](int key) {
            const auto found = m_status.skills.find(key);
            return found != m_status.skills.end() ? found->second.level : 0;
        };
        hooks.roleSkill = [this] { return roleSkillId(m_player.look.bodyType, m_player.look.head); };
        hooks.remoteFormOpen = [this] { return m_remoteWindow.isOpen(); };
        hooks.remoteItem = [this] { return m_inventory.bagItem(m_remote.itemSlot()).itemId; };
        return hooks;
    }

    bool WorldScene::useRemoteControl(int bagSlot) {
        if (const auto notice = m_bagMarks.lockNotice(bagSlot, BagMarks::kCompound)) {
            m_messages.show(notice->text, notice->ms);
            return false;
        }
        if (!m_bagMarks.isAvailable(bagSlot)) {
            m_messages.show("Unable to operate when the item is locked", kLockedMs);
            return false;
        }
        const auto record = m_services.items.find(m_inventory.bagItem(bagSlot).itemId);
        if (m_fightState != 0 && record && (record->flags & kNotInCombat) != 0) {
            m_messages.show("Unable to use in combat", 1000); // message display time in ms
            return false;
        }
        const bool stalled = m_player.stall.has_value() || m_itemStall.isOpen() || m_petStall.isOpen();
        if (!m_remote.use(bagSlot, m_player.vehicle.has_value(), stalled)) {
            return false;
        }
        m_remoteWindow.open();
        m_itemWindow.close();
        return true;
    }

    void WorldScene::onRemoteReset() { m_remote.reset(); }

}
