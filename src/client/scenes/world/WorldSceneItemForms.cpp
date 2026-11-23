#include "client/scenes/world/WorldScene.h"

#include "core/ByteReader.h"
#include "protocol/ItemRequests.h"

namespace client {

    void WorldScene::onFishTankMessage(const core::Bytes& payload) {
        constexpr int kHouseRoomThing = 13; // sub id 13 of the fish tank message: a visitor's special-floor thing in a house room
        if (payload.size() > 2 && payload[2] == kHouseRoomThing) {
            core::ByteReader reader(payload, 3);
            const int room = reader.u8();
            const int place = reader.u8();
            const int itemId = reader.u16();
            const int entering = reader.u8();
            if (!reader.failed()) {
                m_specFloor.showVisitor(room, place, itemId, entering == 1, m_houseRoom);
            }
            return;
        }
        m_fishTank.receive(payload);
    }

    void WorldScene::onMaidAction(const core::Bytes& payload) { m_maid.received(payload); }

    void WorldScene::onLuckyBagMessage(const core::Bytes& payload) { m_luckyBags.received(payload); }

    void WorldScene::onPicnicMessage(const core::Bytes& payload) { m_picnic.receive(payload); }

    void WorldScene::onExpBoxMessage(const core::Bytes& payload) { m_expBox.receive(payload); }

    void WorldScene::onBlackBoardMessage(const core::Bytes& payload) { m_blackBoard.receive(payload); }

    void WorldScene::onIdAndPwForm(int mode) { m_idAndPw.open(mode); }

    void WorldScene::onAnnounceForm(bool open, const std::string& text) {
        if (open) {
            m_announce.open();
        } else {
            m_announce.setText(text);
        }
    }

    LeaveWordWindow::Hooks WorldScene::leaveWordHooks() {
        LeaveWordWindow::Hooks hooks;
        hooks.ownId = [this] { return m_player.id; };
        hooks.ownName = [this] { return m_player.name; };
        hooks.writerOf = [this](std::int32_t id) -> std::optional<LeaveWordWindow::Writer> {
            if (isOwn(id)) {
                return LeaveWordWindow::Writer{m_player.name, m_player.look};
            }
            const auto found = m_otherRecords.find(id);
            if (found == m_otherRecords.end()) {
                return std::nullopt;
            }
            return LeaveWordWindow::Writer{found->second.name, CharacterLook::fromAppearance(found->second.appearance, m_services.items)};
        };
        hooks.playerNamed = [this](const std::string& name) { return worldPlayerNamed(name); };
        hooks.whisper = [this](std::int32_t id) { m_inputBar.setWhisperTarget(id); };
        hooks.closed = [this] { m_eventRunner.moveOn(); };
        return hooks;
    }

    void WorldScene::onLeaveWordMessage(const core::Bytes& payload) { m_leaveWord.received(payload); }

    void WorldScene::onCompoundFacility(const std::vector<std::pair<int, int>>& changes) {
        for (const auto& [product, facility] : changes) {
            const int recipe = m_services.compounds.indexOfProduct(product);
            if (m_services.items.find(product) && recipe != 0 && (facility == 0 || m_services.items.find(facility))) {
                m_services.compounds.setFacility(recipe, facility);
            }
        }
    }

    void WorldScene::onGalaxyRankMessage(const core::Bytes& payload) { m_galaxyRank.received(payload); }

    void WorldScene::onNpcUpgradeForm() { m_npcUpgrade.open(); }

    void WorldScene::onPropertyMessage(const core::Bytes& payload) { m_property.received(payload); }

    void WorldScene::onReFillForm(bool open) {
        if (open) {
            m_reFill.open();
        } else {
            m_reFill.close();
        }
    }

    void WorldScene::onCafeIdForm() { m_cafeId.open(); }

    void WorldScene::onPointStoreMessage(const core::Bytes& payload) { m_pointStore.receive(payload); }

    void WorldScene::onGiftCardMessage(const core::Bytes& payload) { m_giftExchange.receive(payload); }

    void WorldScene::onDismantled(int container, int slot, std::int32_t wear, int petPlace) {
        m_dismantle.result(slot);
        onThingWear(container, slot, wear, petPlace);
        // container 1: the bag
        if (container == 1 && slot >= 1 && slot <= Inventory::kBagSlots && !m_inventory.bagItem(slot).empty() &&
            !m_inventory.isStackable(m_inventory.bagItem(slot).itemId)) {
            m_inventory.setInsertItem(slot, 0);
            inventoryChanged();
        }
    }

    void WorldScene::onSewingKitResult(int code, int bagSlot, int look) {
        m_sewingKit.result(code, bagSlot, look);
        inventoryChanged();
    }

    void WorldScene::onMagicBoxResult(int code) {
        m_magicBox.result(code);
        inventoryChanged();
    }

    void WorldScene::onReferResult(const core::Bytes& payload) { m_refer.result(payload); }

    void WorldScene::onPackingResult(int code) {
        m_packing.result(code);
        inventoryChanged();
    }

    void WorldScene::onReinforceResult(int code, int bagSlot, int level) {
        m_reinforce.result(code, bagSlot, level);
        inventoryChanged();
    }

    void WorldScene::onConfuseStoneResult(int step, int bagSlot, int stoneItemId) {
        if (bagSlot >= 1 && bagSlot <= Inventory::kBagSlots) {
            m_inventory.setInsertItem(bagSlot, stoneItemId);
            inventoryChanged();
        }
        m_confuseStone.result(step, bagSlot);
    }

    void WorldScene::onForageFound(int itemId, int count) { m_forage.found(itemId, count); }

    void WorldScene::onForageBone(int boneSlot) { m_forage.markBone(boneSlot); }

    void WorldScene::onForageEnded(int boneSlot, int petPlace, int code) { m_forage.ended(boneSlot, petPlace, code); }

    void WorldScene::onPlayerColors(std::int32_t playerId, int part, std::int32_t code) {
        m_leaveWord.colorsChanged(part, code);
        if (isOwn(playerId)) {
            m_player.look.basePattern.apply(code, part);
            dressPlayer();
            return;
        }
        if (const auto character = characterOf(playerId)) {
            character->get().look.basePattern.apply(code, part);
            character->get().look.pattern.apply(code, part);
        }
    }

    void WorldScene::openPetExpress(const Pet& pet) {
        if (const auto refusal = expressUseRefusal()) {
            m_messages.show(*refusal, 2000); // message display time in ms
            return;
        }
        if (const auto refusal = petExpressRefusal(m_pets.placeOf(pet.info.npcId))) {
            m_messages.show(*refusal, 1200); // message display time in ms
            return;
        }
        m_express.open(pet);
    }

    std::optional<std::string> WorldScene::expressUseRefusal() const {
        if (ownLevel() < 10) { // minimum player level for the pet express
            return "Level 10 is required";
        }
        if (!m_enoughOnlineTime) {
            return "Not enough time online, cannot use this function";
        }
        return std::nullopt;
    }

    std::optional<std::string> WorldScene::petExpressRefusal(int place) const {
        if (place == 0) {
            return std::nullopt;
        }
        if (m_pets.activePlace() == place) {
            return "Pets in a fight cannot be expressed";
        }
        if (m_petTrade.offeredPlace() == place) {
            return "Pet in trade cannot be expressed";
        }
        if (m_petStall.petInStall(place)) {
            return "Pet being sold cannot be delivered";
        }
        if (m_forage.isForaging(place)) {
            return "The pet looking for items can't be delievered";
        }
        return std::nullopt;
    }

    std::vector<std::string> WorldScene::teamNames() const {
        std::vector<std::string> names;
        const auto leader = m_teams.leaderOf(m_player.id);
        if (!leader) {
            return names;
        }
        std::vector<std::int32_t> ids{*leader};
        for (const std::int32_t member : m_teams.members(*leader)) {
            ids.push_back(member);
        }
        for (const std::int32_t id : ids) {
            if (id != m_player.id && !speakerName(id).empty()) {
                names.push_back(speakerName(id));
            }
        }
        return names;
    }

    void WorldScene::onExpressState(std::int32_t petKind, int state) {
        if (!m_autoDeliver.deliveryState(petKind, state, m_express.isBusy())) {
            m_express.deliveryState(petKind, state);
        }
    }

    void WorldScene::onNpcWalkBy(std::int32_t npcId, int mode) {
        if (const auto info = m_services.npcs.find(npcId); info && m_map) {
            m_mapPets.npcWalkBy(m_player.id, *info, mode, m_map->width(), m_nowMs);
        }
    }

    void WorldScene::onPetWalk(std::int32_t petKind, int mode) {
        const auto npc = m_services.npcs.find(static_cast<int>(petKind));
        if (!npc || !m_map || (mode == 1 && m_pets.placeOf(static_cast<int>(petKind)) == 0)) {
            return;
        }
        m_mapPets.petWalk(m_player.id, *npc, mode, m_map->width(), m_nowMs);
    }

    void WorldScene::onExpressFailed(int code, int itemId) {
        if (!m_autoDeliver.failed(code, itemId)) {
            m_express.failed(code, itemId);
        }
    }

    void WorldScene::onExpressResult(int kind, std::int32_t otherId, int itemId, int count) {
        const bool automatic = m_autoDeliver.result(kind, otherId);
        m_express.result(kind, otherId, itemId, count, !automatic);
    }

}
