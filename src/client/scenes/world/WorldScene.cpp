#include "client/scenes/world/WorldScene.h"
#include "client/services/photo/ScreenPhoto.h"

#include "client/actions/MessageDispatcher.h"
#include "client/rules/PetWeapon.h"
#include "client/rules/ChristmasMaps.h"
#include "client/rules/VehicleSeats.h"
#include "client/rules/MountSpeedBonus.h"
#include "client/rules/Vitality.h"
#include "client/rules/Experience.h"
#include "client/rules/FleeKillNotice.h"
#include "client/rules/NpcVisibility.h"
#include "client/rules/GmIds.h"
#include "client/services/settings/TalkSettings.h"
#include "client/rules/FurnitureKinds.h"
#include "client/rules/UtensilSeat.h"
#include "protocol/CastleRequests.h"
#include "protocol/GuildRequests.h"
#include "protocol/MarriageRequests.h"
#include "protocol/MallMessages.h"
#include "protocol/PetRequests.h"
#include "protocol/StatusEffectRequests.h"
#include "protocol/VehicleRequests.h"
#include "client/rules/Radio.h"
#include "client/rules/Skills.h"
#include "client/data/StarterRoles.h"
#include "gfx/TextStyle.h"
#include "client/rules/CharacterActions.h"
#include "client/rules/NpcMovement.h"
#include "client/rules/PathFinder.h"
#include "client/rules/Walking.h"
#include "core/Between.h"
#include "core/FileSystem.h"
#include "formats/ServerList.h"
#include "core/Log.h"
#include "formats/GamePixels.h"
#include "gfx/RenderDevice.h"
#include "core/StringUtil.h"
#include "protocol/BattleRequests.h"
#include "protocol/ChatRequest.h"
#include "protocol/HotKeyRequests.h"
#include "protocol/PlayerId.h"
#include "protocol/SystemRequests.h"
#include "protocol/EmotionRequests.h"
#include "protocol/FriendRequests.h"
#include "protocol/TeamRequests.h"
#include "protocol/StallRequests.h"
#include "protocol/UtensilStallRequests.h"
#include "protocol/TradeRequests.h"
#include "protocol/WalkRequest.h"
#include "protocol/ItemRequests.h"
#include "protocol/ShopRequests.h"
#include "protocol/WorldRequests.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>

namespace client {

    namespace {

        constexpr std::uint64_t kEnterTimeoutMs = 5000; // longest time the "entering the world" state may last before it is ended, ms
        constexpr std::int32_t kSecondCharacterOffset = 4500000; // player ids from 4500000 up belong to character slot 2 of an account
        constexpr int kHorseAction = 18; // action group 18: riding (eight-way group)
        constexpr std::uint64_t kChatBlockMs = 8000; // how long chat stays blocked after the server's chat-blocked notice, ms
        constexpr int kFadeSteps = 15; // frames of the black fade-in after a map change
        constexpr int kDarkStep = 15; // fully dimmed step of the black dim overlay (steps 0..15)
        constexpr int kFadeAlphaPerStep = 12; // black overlay alpha (0..255) added per fade step
        constexpr int kMusicFadeStep = 10; // fade step at which the map music starts
        constexpr std::uint64_t kHeldWalkMs = 400; // repeat interval of walking while the mouse or key is held, ms
        constexpr std::uint64_t kDoorCheckMs = 200; // how often the player is checked against doors, ms
        constexpr int kMapChangeMusicFrames = 4; // frames to wait after a map change before the music switches
        constexpr int kFacingSouth = 4; // direction 4: facing down the screen (south)
        constexpr int kRadioSlot = 6; // equip slot 6: the radio set
        constexpr int kRadioSp = 15; // SP cost of one world-channel message
        constexpr int kWorldRadio = 3; // radio kind 3 (item 34132): lines show as "(All)"
        constexpr int kNoWhisperMap = 10701; // map 10701: whispering refused (name not in the tables)
        const core::Color kOwnNameColor{255, 255, 0, 255}; // yellow (RGBA): own name label
        const core::Color kOtherNameColor{0, 255, 255, 255}; // cyan (RGBA): other players' name labels

    }

    WorldScene::WorldScene(
        GameServices& services
    )
        : m_services(services),
          m_screen(services.pictures),
          m_layout(m_screen.load(services.game.layout("WorldScene.xml"), services.loadContext())),
          m_backdrop(m_screen.get<ui::Element>("WorldBackdrop")),
          m_view(m_screen.get<ui::DrawingArea>("WorldView")),
          m_ground(services.pictures, services.items),
          m_specFloor(services.pictures, services.items),
          m_mapArchive(formats::DataArchive::open(services.game.data("Data/Ground.MMG"))),
          m_objectArchive(formats::DataArchive::open(services.game.data("Data/Wem.MMG"))),
          m_painter(services.characters, services.pictures, services.items),
          m_effects(services.pictures),
          m_inventory(services.items),
          m_bank(services.items),
          m_pets(services.formula, services.npcs, services.items),
          m_battle(
              services,
              m_messages,
              {[this](const core::Bytes& bytes) { m_services.connection.send(bytes); },
               [this](std::int32_t id) -> std::optional<std::pair<CharacterLook, std::string>> {
                   if (id == m_player.id) {
                       return std::make_pair(m_player.look, m_player.name);
                   }
                   const auto found = m_others.find(id);
                   if (found == m_others.end()) {
                       return std::nullopt;
                   }
                   return std::make_pair(found->second.look, found->second.name);
               },
               [this](int role) -> std::optional<formats::NpcInfo> {
                   auto npc = findNpc(role);
                   return npc ? std::optional<formats::NpcInfo>(npc->get().info) : std::nullopt;
               },
               [this](std::int32_t ownerId, int npcId) -> std::optional<std::string> {
                   if (ownerId != m_player.id) {
                       return std::nullopt;
                   }
                   const int place = m_pets.placeOf(npcId);
                   const auto& pet = place != 0 ? m_pets.at(place) : std::nullopt;
                   return pet ? std::optional<std::string>(pet->name) : std::nullopt;
               },
               [this](const std::string& line) { m_chat.add(0, line, m_player.id); },
               [this](const Fighter& fighter) { return skillUserOf(fighter); },
               [this] { m_battleSkills.toggle(); },
               [this] { m_itemWindow.toggle(); },
               [this](int skillId, bool pet) { m_battleSkills.pushRecent(skillId, pet); },
               [this] { return m_map ? m_map->id() : 0; },
               [this](bool pet) { return m_battleSkills.recent(pet); },
               [this](int itemId) { return m_inventory.nextSlotWith(itemId); },
               [this] { m_itemWindow.close(); },
               [this](std::int32_t ownerId, int npcId) -> std::optional<std::pair<int, std::int32_t>> {
                   const int place = ownerId == m_player.id ? m_pets.placeOf(npcId) : 0;
                   const auto& pet = place != 0 ? m_pets.at(place) : std::nullopt;
                   if (!pet) {
                       return std::nullopt;
                   }
                   const PetWeapon weapon = petWeapon(*pet, m_services.items);
                   return weapon.lookId > 0 ? std::optional<std::pair<int, std::int32_t>>(std::make_pair(weapon.lookId, weapon.colorCode)) : std::nullopt;
               },
               [this] { return m_remote.isOn(); },
               [this](int kind) { return m_remote.playSets()[static_cast<std::size_t>(kind)]; },
               [this] {
                   const auto& pet = m_pets.at(m_pets.activePlace());
                   return pet ? pet->info.npcId : 0;
               },
               [this] { return m_systemWindow.settings().battleSwitch == 0; }}
          ),
          m_starSky(
              services,
              {[this](bool shown) { setHudShown(shown); },
               [this](bool shown) { m_starSkyWindow.setQuitShown(shown); },
               [this] { m_services.music.playLooped(m_services.game.data("Sound/BGM0017.wav")); },
               [this] { playMapMusic(); },
               [this]() -> std::optional<core::Point> { return m_camera.toScreen(m_player.position); },
               [this](int index) -> std::optional<core::Point> {
                   const auto npc = std::find_if(m_npcs.begin(), m_npcs.end(), [index](const MapNpc& n) { return n.index == index; });
                   return npc != m_npcs.end() ? std::optional<core::Point>(m_camera.toScreen(npc->body.position)) : std::nullopt;
               },
               [this] { return m_fightState != 0 && m_fightState != 4; }}
          ), // fight state 4: watching a battle
          m_starSkyWindow(services, m_screen, m_starSky, m_roleIntro),
          m_gobang(services, m_screen, gobangHooks()),
          m_miniMap(services, m_screen, m_player, m_npcs),
          m_mainStatus(services, m_screen, m_status, m_player.look, m_pets),
          m_battleMain(services, m_screen, m_battle, m_status),
          m_battleSkills(services, m_screen, m_battle, m_status, m_pets, m_player.look),
          m_chat(services, m_screen),
          m_hotKeys(
              services,
              m_screen,
              m_messages,
              {[this](int itemId) { return m_inventory.nextSlotWith(itemId); },
               [this](int itemId) { return m_inventory.isStackable(itemId); },
               [this](int skillId) {
                   const auto skill = m_services.skills.find(skillId);
                   const auto learnt = skill ? m_status.skills.find(skill->key) : m_status.skills.end();
                   return learnt != m_status.skills.end() && learnt->second.level > 0;
               },
               [this](int skillId) {
                   for (int order = 1; order <= m_pets.count(); ++order) {
                       const auto& pet = m_pets.at(m_pets.placeByOrder(order));
                       if (pet && std::find(pet->info.skills.begin(), pet->info.skills.end(), skillId) != pet->info.skills.end()) {
                           return true;
                       }
                   }
                   return false;
               },
               [this] { return roleSkillId(m_player.look.bodyType, m_player.look.head); },
               [this](int skillId) -> std::optional<bool> {
                   constexpr int kWatching = 4; // fight state 4: watching a battle
                   if (!m_battle.isOn() || m_fightState == 0 || m_fightState == kWatching) {
                       return std::nullopt;
                   }
                   return m_battle.hotKeySkillUsable(skillId);
               },
               [this] { return m_map ? m_map->id() : 0; },
               [this] { return m_fightState != 0; },
               [this](int itemId, int bagSlot) { m_battle.chooseItem(itemId, bagSlot, m_inventory.bagItem(bagSlot).count); },
               [this](int skillId) { m_battle.useListedSkill(skillId); },
               [this](int bagSlot) { m_services.connection.send(protocol::useItemRequest(bagSlot, 1, 0, 0)); },
               [this](int kind, int id) { aimHotKey(kind, id); },
               [this] { return m_status.sp; },
               [this]() -> std::optional<int> {
                   constexpr int kClothesSlot = 2; // equip slot 2: clothes
                   const int clothes = starterEquipment(roleIndex(m_player.look.bodyType, m_player.look.head))[1];
                   return clothes != m_inventory.equipped(kClothesSlot).itemId ? std::optional<int>(clothes) : std::nullopt;
               }}
          ),
          m_mainButtons(services, m_screen),
          m_functionButtons(services, m_screen, m_status),
          m_timeEffects(
              services,
              m_screen,
              m_statusEffects,
              {[this] { return m_instances.state(); },
               [this] { return m_instances.here(); },
               [this] { return m_fightState != 0; },
               [this] { m_instanceWindow.askEnter(); },
               [this] { m_instanceWindow.askLeave(); },
               [this] { return m_remote.isOn(); },
               [this] { m_remote.stop(); },
               [this] { return m_puppetState.mode; },
               [this] { showPuppetForm(); },
               [this](int tag) { askTimeLeft(tag); },
               [this] { return inTrainingIslandMap(); },
               [this] { return onTrojanBattleMap(); },
               [this] { leaveTrojanWar(); },
               [this] { return m_sandglass.active(); },
               [this] { showSandglassEffects(); },
               [this] { return inHouseFloorRoom(); },
               [this] { m_remoteWindow.open(); }}
          ),
          m_inputBar(
              services,
              m_screen,
              {[this](int channel, const std::string& text) { return sendChat(channel, text); },
               [this](int channel) { return chatChannelAllowed(channel); },
               [this] { m_mailBox.open(); },
               [this](const std::string& name) { return worldPlayerNamed(name); },
               [this] { return m_player.id; },
               [this](std::int32_t id) { return speakerName(id); },
               [this](const std::string& text, std::int32_t id) { m_chat.add(10, text, id); }, // chat channel 10: notice line
               [this](const std::string& code) { m_sendMail.typeEmote(code); },
               [this] { m_services.connection.send(protocol::quitViewingRequest()); },
               [this](std::int32_t id) { return playerChosen(id); }}
          ),
          m_itemWindow(services, m_screen, m_status, m_inventory, m_pets, m_bagMarks, m_player, m_messages, m_itemInfo),
          m_skillWindow(services, m_screen, m_status, m_pets, m_player, m_messages),
          m_systemWindow(
              services,
              m_screen,
              {[this] { askToLeave(false); },
               [this] { askToLeave(true); },
               [this](const SystemSettings& settings) { applyChatColors(settings); },
               [this] { returnToSpot(); },
               [this] { m_changePassword.open(); },
               [this] { m_blacklist.toggle(); },
               [this] { m_msgSkin.toggle(); }}
          ),
          m_groupWindow(
              services,
              m_screen,
              m_pets,
              m_teams,
              {[this] { return m_player.id; },
               [this](std::int32_t id) { return teamMate(id); },
               [this] { return m_fightState != 0 && m_fightState != 4; },
               [this] { return m_player.stall.has_value(); }, // fight state 4: watching a battle
               [this](bool pending) { return leaderRefusal(pending); },
               [this] { return m_player.vehicle.has_value(); }},
              m_messages,
              m_talk,
              {[this] { return m_guild.exists(); },
               [this] { return m_guildWindow.isOpen(); },
               [this] { return m_guildWindow.position(); },
               [this](core::Point at) { m_guildWindow.open(at); },
               [this] { m_guildWindow.close(); },
               [this] { m_guildWindow.formShown(); }},
              {[this] { return m_instanceWindow.isOpen(); },
               [this] { return m_instanceWindow.position(); },
               [this](core::Point at) { m_instanceWindow.open(at); },
               [this] { m_instanceWindow.close(); }}
          ),
          m_publicBoard(false, noticeBoardHooks()),
          m_guildBoard(true, noticeBoardHooks()),
          m_noticeRead(
              services,
              m_screen,
              m_publicBoard,
              m_guildBoard,
              {[this] { return m_player.id; },
               [this] {
                   constexpr int kLeadershipPlaces = 5; // the first 5 guild member places are leaders
                   const int own = m_guild.indexOf(m_player.id);
                   return own >= 0 && own < kLeadershipPlaces;
               },
               [this](bool guild, std::int32_t authorId, const std::string& name) { mailNoticeAuthor(guild, authorId, name); }}
          ),
          m_noticeSend(services, m_screen, m_messages, m_publicBoard, m_guildBoard),
          m_noticeBoardWindow(
              services,
              m_screen,
              m_publicBoard,
              m_noticeRead,
              {[this] { return m_player.id; }, [this] { return m_status.level; }, [this] { closeNoticeForms(); }}
          ),
          m_top100(top100Hooks()),
          m_top100Window(services, m_screen, m_top100),
          m_instances(services.marks, services.npcs, instancesHooks()),
          m_instanceCreate(services, m_screen, m_instances, m_messages),
          m_instanceDetail(services, m_screen, m_instances),
          m_instanceWindow(services, m_screen, m_instances, m_instanceCreate, m_instanceGoals, m_talk, instanceWindowHooks()),
          m_guildWindow(
              services,
              m_screen,
              m_guild,
              m_messages,
              m_talk,
              m_guildBoard,
              m_noticeRead,
              {[this] { return m_player.id; },
               [this](std::int32_t id) { return isOwn(id) ? m_socialWindow.onlineStatus() : worldOnlineState(id); },
               [this] { return TalkDialog::Speaker{m_player.look, m_player.name}; },
               [this] { return m_player.nickname; },
               [this] { return m_status.level; },
               [this] { return m_status.job; },
               [this] { return m_castleOwner; },
               [this] { m_groupWindow.showParty(m_guildWindow.position()); },
               [this] { startGuildInvite(); },
               [this](core::Point at) { m_guildRules.toggle(at, m_guild.rules()); },
               [this] { m_guildMyAuthority.toggle(); },
               [this](int selection) { m_guildAuthority.toggle(selection); },
               [this] { return m_guildAuthority.isOpen(); },
               [this](std::int32_t id) { m_sendMail.openToGuildMember(id); },
               [this] { m_sendMail.openToCheckedMembers(); },
               [this] { m_groupWindow.showInstances(m_guildWindow.position()); },
               [this]() -> std::optional<formats::EmblemBlock> {
                   if (const auto own = m_ensigns.find(m_player.guildId)) {
                       return own->get().data;
                   }
                   return std::nullopt;
               },
               [this] { castleTeleport(); }}
          ),
          m_guildRules(services, m_screen, m_messages),
          m_guildMyAuthority(services, m_screen, m_guild, {[this] { return m_player.id; }, [this] { return m_player.name; }}),
          m_guildAuthority(services, m_screen, m_guild, [this] { return m_player.id; }),
          m_guildCreate(services, m_screen, m_messages, [this] { m_eventRunner.moveOn(); }),
          m_marriage(services.items, marriageHooks()),
          m_socialWindow(
              services,
              m_screen,
              m_friends,
              m_messages,
              m_talk,
              {[this](std::int32_t id) { return worldOnlineState(id); },
               [this](int number) { playEmotion(number); },
               [this](int number) { playAction(number); },
               [this](const std::string& text) {
                   if (m_chatSilence.isMailSilenced()) {
                       m_chat.add(10, ChatSilence::refusal()); // chat channel 10: notice line
                       return;
                   }
                   m_inputBar.setText(text);
               },
               [this](int heartNumber) { return nearestHeart(heartNumber); },
               [this](std::int32_t id) { m_sendMail.openTo(id); },
               [this] { m_sendMail.openToChecked(); },
               [this] { return m_marriage.spouseId() != 0; },
               [this](core::Point at) {
                   m_marriage.requestCard();
                   m_marryCard.open(at);
               },
               [this](std::int32_t id) { m_inputBar.setWhisperTarget(id); }}
          ),
          m_marryApplication(services, m_screen, m_marriage),
          m_marryDivorce(services, m_screen, m_marriage),
          m_marryCard(services, m_screen, m_marriage, marryCardHooks()),
          m_career(services, m_screen, careerHooks()),
          m_beforeCareer(services, m_screen, careerHooks()),
          m_camps(services.items, campsHooks()),
          m_vehicles(services.items, vehiclesHooks()),
          m_vehicleWrecks(services.pictures, services.items),
          m_vehicleRepair(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              {[this](const core::Bytes& bytes) { m_services.connection.send(bytes); },
               [this] { bagMarksChanged(); },
               [this] { return static_cast<int>(m_mallShop.points); },
               [this](const std::string& text, std::function<void()> onOk) {
                   TalkDialog::Timing timing;
                   timing.wrapChars = 32; // wrap the text at 32 characters
                   timing.exclusive = false;
                   timing.timeoutMs = 10000; // close after 10000 ms
                   m_talk.ask(text, std::nullopt, std::move(onOk), [] {}, std::move(timing));
               }}
          ),
          m_campStyle(services, m_screen, m_bagMarks, campStyleHooks()),
          m_npcTeam(services, m_screen),
          m_luckyWheel(services, m_screen, m_itemInfo, luckyWheelHooks(), [this] { return static_cast<int>(m_mallShop.points); }),
          m_luckyCards(services, m_screen, sportHooks(), [this] { return m_gameStart.isOpen(); }),
          m_memory(services, m_screen, sportHooks(), [this] { return m_gameStart.isOpen(); }),
          m_hitMouse(services, m_screen, sportHooks()),
          m_boxing(services, m_screen, sportHooks(), [this] { return m_gameStart.isOpen(); }),
          m_jigsaw(services, m_screen, sportHooks(), [this] { return m_gameStart.isOpen(); }),
          m_dolls(services, m_screen, sportHooks(), [this] { return m_gameStart.isOpen(); }),
          m_hunter(services, m_screen, sportHooks(), [this] { return static_cast<int>(m_player.look.bodyType); }),
          m_digHole(
              services,
              m_screen,
              sportHooks(),
              [this] { return m_gameStart.isOpen(); },
              [this] { return m_talk.isOpen(); }
          ),
          m_pumpkin(
              services,
              m_screen,
              sportHooks(),
              [this] { return m_gameStart.isOpen(); },
              [this]() -> const CharacterLook& { return m_player.look; }
          ),
          m_scotd(services, m_screen, sportHooks(), [this]() -> const CharacterLook& { return m_player.look; }),
          m_throw(services, m_screen, sportHooks(), [this]() -> const CharacterLook& { return m_player.look; }),
          m_movie(services, m_screen, movieHooks()),
          m_sheep(services, m_screen, sportHooks()),
          m_lucky(
              services,
              m_screen,
              sportHooks(),
              [this] { return m_gameStart.isOpen(); },
              [this]() -> const CharacterLook& { return m_player.look; }
          ),
          m_quiz(services, m_screen, quizHooks(), [this]() -> const CharacterLook& { return m_player.look; }),
          m_compass(services, m_screen, compassHooks(), [this]() -> const CharacterLook& { return m_player.look; }),
          m_fortuneEgg(services, m_screen, sportHooks()),
          m_slotMachine(services, m_screen, sportHooks()),
          m_gameStart(services, m_screen, gameStartHooks()),
          m_compoundWindow(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_status,
              m_messages,
              m_talk,
              [this] { bagMarksChanged(); },
              [this] { return m_map ? m_map->id() : 0; }
          ),
          m_moneyMenu(
              services,
              m_screen,
              m_status,
              m_messages,
              {[this] { return m_map ? m_map->id() : 0; },
               [this] { return m_teams.leaderOf(m_player.id).has_value(); },
               [this] { m_throwMoney.toggle(); },
               [this](int mode) { setMouseMode(mode); },
               [this](int kind) {
                   dismountOwnPet();
                   if (kind == 1) {
                       m_itemStall.toggle();
                   } else if (kind == 2) { // stall kind 2: pet stall (1 item stall, else utensil stall)
                       m_petStall.toggle();
                   } else {
                       m_utensilStall.openFromMenu();
                   }
               },
               [this] { return standsOnFlatGround(true); },
               [this] { return m_player.stall ? static_cast<int>(m_player.stall->kind) + 1 : 0; },
               [this] { return onTrojanWarMap(); },
               [this] { return isQuizZoneMap(m_map ? m_map->id() : 0); },
               [this] { return m_player.bathing; },
               [this] { return (sceneFlags() & 2) != 0; }}
          ), // scene flag bit 2
          m_throwMoney(services, m_screen, m_status, m_messages, [this] { return m_map ? m_map->id() : 0; }),
          m_itemTrade(services, m_screen, m_inventory, m_bagMarks, m_status, m_messages, [this] { bagMarksChanged(); }),
          m_utensilTrade(services, m_screen, m_inventory, m_bagMarks, m_status, m_messages, m_itemInfo, utensilTradeHooks()),
          m_petTrade(services, m_screen, m_pets, m_status, m_messages, m_petInfo),
          m_house(
              services.pictures,
              services.items,
              0,
              {[this](const std::string& text) { m_messages.show(text, 2000); }, // message display time in ms
               [this](const std::string& sound) { m_services.playSound(sound); },
               [this](int slot, bool stalled) {
                   m_bagMarks.setInFurnitureStall(slot, stalled);
                   bagMarksChanged();
               },
               [this](int piece, int place) { m_furnitureBuy.reclaimed(piece, place); },
               [this] { return m_player.id; },
               [this](int piece, std::int32_t price) {
                   m_eggSell.priceChanged(piece, price);
                   m_eggBuy.priceChanged(piece, price);
               },
               [this](const StallCaseChange& change) {
                   if (change.own) {
                       m_stallCase.changed(change);
                   } else {
                       m_furnitureBuy.caseChanged(change);
                   }
               },
               [this](const protocol::PetRecord& pet, int place) {
                   if (m_pets.put(place, pet)) {
                       m_groupWindow.refresh();
                       m_petStorage.refresh();
                   }
                   m_bed.refresh();
               },
               [this] {
                   if (m_bed.isOpen()) {
                       m_bed.refresh();
                   }
               },
               [this](int restrained) { m_carCase.shown(restrained); },
               [this](const protocol::FurnitureCupboardItem& stored) {
                   m_carCase.parked(stored);
                   m_showPlatform.parked(stored);
               },
               [this](int cell) { m_carCase.left(cell); },
               [this](int restrained, bool garage) { m_showPlatform.opened(restrained, garage); },
               [this](bool) { m_showPlatform.close(); }}
          ),
          m_housePainter(m_house, services.items),
          m_waymark(services.pictures),
          m_houseEditor(m_house, services.items, houseEditorHooks()),
          m_makeMaterials(m_inventory, m_bagMarks, m_house.furniture(), m_bookShelves),
          m_computer(services, m_screen, m_makeJobs, m_house.furniture(), m_makeMaterials, m_messages, computerHooks()),
          m_npcMake(services, m_screen, m_pets, m_player, m_messages, [this] { m_computer.partnersClosed(); }),
          m_makeInfo(services, m_screen, m_house.furniture(), m_makeJobs, m_makeMaterials, m_messages, makeInfoHooks()),
          m_makeThing(services, m_screen, m_house.furniture(), m_makeJobs, m_makeMaterials, m_makeInfo, m_messages, makeThingHooks()),
          m_stallDrops(services.items, m_inventory, m_bagMarks),
          m_stallPrice(services, m_screen, m_bagMarks, m_messages, stallPriceHooks()),
          m_furnitureBuy(services, m_screen, m_house, m_inventory, m_messages, furnitureBuyHooks()),
          m_vending(services, m_screen, m_house, vendingHooks()),
          m_eggSell(services, m_screen, m_house, m_inventory, m_bagMarks, m_messages, eggSellHooks()),
          m_eggBuy(services, m_screen, m_house, m_inventory, m_messages, eggBuyHooks()),
          m_stallCase(services, m_screen, m_house, m_inventory, m_bagMarks, m_messages, stallCaseHooks()),
          m_cupboard(services, m_screen, m_house, m_inventory, m_bagMarks, m_messages, m_itemInfo, cupboardHooks()),
          m_iceBox(services, m_screen, m_house, m_houseBoxes, m_inventory, m_bagMarks, m_messages, m_itemInfo, iceBoxHooks()),
          m_pledgeBox(services, m_screen, m_houseBoxes, m_inventory, m_bagMarks, m_messages, m_itemInfo, pledgeBoxHooks()),
          m_musicCenter(services, m_screen, musicCenterHooks()),
          m_gasCan(GasStorageWindow::Tank::Big, services, m_screen, m_inventory, m_bagMarks, m_messages, gasStorageHooks()),
          m_smallGas(GasStorageWindow::Tank::Small, services, m_screen, m_inventory, m_bagMarks, m_messages, gasStorageHooks()),
          m_voyageCompass(services, m_screen, m_bagMarks, m_messages, voyageCompassHooks()),
          m_globe(services, m_screen),
          m_bookShelf(services, m_screen, m_bookShelves, m_inventory, m_messages, m_itemInfo, bookShelfHooks()),
          m_blueprint(services, m_screen),
          m_homeAdorn(services, m_screen, m_house.pillars(), homeAdornHooks()),
          m_portrait(services, m_screen, portraitHooks()),
          m_doorplate(services, m_screen, m_house.doorplates(), doorplateHooks()),
          m_bed(services, m_screen, m_house, m_pets, m_messages, bedHooks()),
          m_dye(services, m_screen, m_player.look),
          m_dismantle(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_fightState != 0; }, [this] { return m_itemWindow.bounds(); }, [this] { bagMarksChanged(); }}
          ),
          m_confuseStone(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_fightState != 0; },
               [this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk, std::function<void()> onCancel) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       std::move(onCancel),
                       TalkDialog::Timing{32, 0, {}, false}
                   ); // Timing: wrap at 32 chars, no time-out
               }}
          ),
          m_reinforce(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       [] {},
                       TalkDialog::Timing{32, 5000, [] {}, false}
                   ); // Timing: wrap at 32 chars, time out after 5000 ms
               }}
          ),
          m_magicBox(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       [] {},
                       TalkDialog::Timing{32, 5000, [] {}, false}
                   ); // Timing: wrap at 32 chars, time out after 5000 ms
               }}
          ),
          m_packing(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       [] {},
                       TalkDialog::Timing{32, 5000, [] {}, false}
                   ); // Timing: wrap at 32 chars, time out after 5000 ms
               }}
          ),
          m_refer(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(text, std::nullopt, std::move(onOk), [] {}, TalkDialog::Timing{32, 0, {}, false}); // Timing: wrap at 32 chars, no time-out
               },
               [this] { return m_fightState != 0; }}
          ),
          m_repair(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this] { return m_fightState != 0; }}
          ),
          m_insteadEcis(services, m_screen, m_friends, m_guild, m_inventory, m_bagMarks, m_messages, m_puppetState, insteadEcisHooks()),
          m_memCard(services, m_screen, m_friends, m_guild, m_messages, m_puppetState, memCardHooks()),
          m_sewingKit(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this] { return m_itemWindow.bounds(); },
               [this] { bagMarksChanged(); },
               [this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       [] {},
                       TalkDialog::Timing{32, 10000, [] {}, false}
                   ); // Timing: wrap at 32 chars, time out after 10000 ms
               },
               [this] { return static_cast<int>(m_player.look.bodyType); }}
          ),
          m_giftExchange(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              {[this] { return m_fightState != 0; },
               [this](const std::string& text, std::function<void()> onOk, std::function<void()> onCancel) {
                   m_talk.ask(
                       text,
                       std::nullopt,
                       std::move(onOk),
                       std::move(onCancel),
                       TalkDialog::Timing{32, 0, {}, false}
                   ); // Timing: wrap at 32 chars, no time-out
               },
               [this](const std::string& line) { m_chat.add(0, line, m_player.id); }}
          ),
          m_pointStore(
              services,
              m_screen,
              m_inventory,
              m_messages,
              m_countBox,
              {[this](const std::string& text, std::function<void()> onOk) {
                   m_talk.ask(text, std::nullopt, std::move(onOk), [] {}, TalkDialog::Timing{32, 0, {}, false}); // Timing: wrap at 32 chars, no time-out
               },
               [this](int channel, const std::string& line) { m_chat.add(channel, line, channel == 0 ? m_player.id : 0); },
               [this] { m_services.connection.send(protocol::imPointsRequest(0)); },
               [this] {
                   m_moveLocked = false;
                   m_eventRunner.moveOn();
               },
               [this] { return m_talk.isOpen(); }}
          ),
          m_picnic(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_pets,
              m_messages,
              {[this]() -> std::optional<core::Rect> {
                   if (!m_itemWindow.isOpen()) {
                       return std::nullopt;
                   }
                   return m_itemWindow.bounds();
               },
               [this] { return m_fightState; },
               [this] { return m_player.name; },
               [this] {
                   const auto& active = m_pets.at(m_pets.activePlace());
                   return active ? active->info.name : std::string();
               },
               [this](const std::string& line) { m_chat.add(10, line, m_player.id); }, // chat channel 10: notice line
               [this](const std::string& text, int ms) { m_centreMessages.show(text, ms); },
               [this](bool on) {
                   m_picnicAim = on;
                   m_view.setCursor(on ? ui::CursorKind::Magic : ui::CursorKind::Normal);
               },
               [this](bool sp) { return sp ? m_status.sp < m_status.maxSp : m_status.hp < m_status.maxHp; },
               [this] { bagMarksChanged(); }}
          ),
          m_expBox(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              {[this]() -> std::optional<core::Rect> {
                   if (!m_itemWindow.isOpen()) {
                       return std::nullopt;
                   }
                   return m_itemWindow.bounds();
               },
               [this] { return m_fightState; },
               [this] { return m_activeSport != 0; },
               [this](const std::string& text, int ms) { m_centreMessages.show(text, ms); },
               [this] { bagMarksChanged(); },
               [this] { return m_services.session.onPvpServer; },
               [this] { return m_statusEffects.doubleExpEnded(); }}
          ),
          m_blackBoard(
              services,
              m_screen,
              m_messages,
              {[this] { return m_house.furniture().ownerId(); }, [this] { return m_player.id; }, [this] { return m_player.name; }}
          ),
          m_idAndPw(services, m_screen, m_messages),
          m_reFill(
              services,
              m_screen,
              m_messages,
              [this](const std::string& text, std::function<void()> onOk) {
                  m_talk.ask(
                      text,
                      std::nullopt,
                      std::move(onOk),
                      [] {},
                      TalkDialog::Timing{32, 10000, [] {}, false}
                  ); // Timing: wrap at 32 chars, time out after 10000 ms
              }
          ),
          m_announce(services, m_screen, [this] { m_eventRunner.moveOn(); }),
          m_leaveWord(services, m_screen, m_messages, leaveWordHooks()),
          m_galaxyRank(services, m_screen),
          m_npcUpgrade(
              services,
              m_screen,
              m_pets,
              m_inventory,
              m_messages,
              {[this](const core::Bytes& bytes) { m_services.connection.send(bytes); }, [this] { m_eventRunner.moveOn(); }}
          ),
          m_property(
              services,
              m_screen,
              m_messages,
              {[this] { return m_mallShop.points; },
               [this](const std::string& text, std::function<void()> onOk) {
                   TalkDialog::Timing timing;
                   timing.wrapChars = 32; // wrap the text at 32 characters
                   timing.exclusive = false;
                   m_talk.ask(text, std::nullopt, std::move(onOk), [] {}, std::move(timing));
               },
               [this](const std::string& line) { m_chat.add(0, line, m_player.id); },
               [this](const core::Bytes& message) { m_services.connection.send(message); }}
          ),
          m_cafeId(services, m_screen, [this] { m_services.connection.close(); }),
          m_gps(services, m_screen, m_inventory, m_bagMarks, m_messages, gpsHooks()),
          m_gpsEffect(services.pictures),
          m_express(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_pets,
              m_friends,
              m_guild,
              m_messages,
              {[this](const std::string& name) { return worldPlayerNamed(name); },
               [this](std::int32_t id) { return speakerName(id); },
               [this](std::int32_t id, const std::string& text) { m_chat.add(10, text, id); },
               [this] { return teamNames(); }, // chat channel 10: notice line
               [this] { return m_player.id; },
               [this] {
                   const auto& active = m_pets.at(m_pets.activePlace());
                   return active ? active->info.npcId : 0;
               },
               [this] { bagMarksChanged(); }}
          ),
          m_forage(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_pets,
              m_messages,
              {[this] { return m_itemWindow.bounds(); },
               [this](int place) { return m_pets.ridingPlace() != place && !m_petStall.petInStall(place) && m_pets.activePlace() != place; },
               [this] { bagMarksChanged(); },
               [this] { return m_player.id; }}
          ),
          m_fishTank(
              services,
              m_screen,
              m_messages,
              {[this] { return m_player.id == m_house.furniture().ownerId(); }, [this] { m_fishAnnotate.show(); }, [this](int mode) { m_fishBuy.show(mode); }}
          ),
          m_fishBuy(services, m_screen, m_fishTank),
          m_fishAnnotate(services, m_screen),
          m_piano(services, m_screen, {[this] { return m_pianoSeated; }, [this] { playMapMusic(); }}),
          m_maid(
              services,
              m_screen,
              {[this](const core::Bytes& message) { m_services.connection.send(message); },
               [this](int furnIndex) { return m_house.furniture().piece(furnIndex).itemId; },
               [this](int itemId) {
                   for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
                       if (m_inventory.bagItem(slot).itemId == itemId && m_bagMarks.isAvailable(slot)) {
                           return slot;
                       }
                   }
                   return 0;
               },
               [this](int furnIndex, int itemId) { changeMaidPiece(furnIndex, itemId); },
               [this] { m_bankItems.show([this] { m_maid.formClosed(); }); },
               [this] { m_petStorage.show([this] { m_maid.formClosed(); }); }}
          ),
          m_retainFur(services, m_screen, m_mainButtons, m_house.furniture(), retainFurHooks()),
          m_itemStall(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              {[this] { bagMarksChanged(); },
               [this] { return m_map ? m_map->id() : 0; },
               [this] { return m_teams.leaderOf(m_player.id).has_value(); },
               [this] { return m_player.stall && m_player.stall->kind == StallSign::Kind::Items; },
               [this] { return m_player.stall.has_value(); },
               [this] { return m_player.riding.has_value(); },
               [this](const std::string& line) { m_chat.add(10, line, m_player.id); },
               [this] { return m_player.vehicle.has_value(); }, // chat channel 10: notice line
               [this] { return m_player.fishing; },
               [this] { return m_remote.isOn(); }}
          ),
          m_countBox(services, m_screen),
          m_stallGoods(services, m_screen, m_inventory, m_status, m_messages, m_countBox),
          m_npcStore(services, m_screen, m_inventory, m_status, m_messages, m_centreMessages, m_countBox),
          m_npcSale(services, m_screen, m_inventory, m_bagMarks, m_status, m_messages, [this] { bagMarksChanged(); }),
          m_innAnswer(services, m_screen),
          m_bagRequests(services, m_inventory, m_bagMarks, m_messages, m_countBox, bagRequestHooks()),
          m_bankItems(
              services,
              m_screen,
              m_inventory,
              m_bank,
              m_bagMarks,
              m_messages,
              [this] { bagMarksChanged(); },
              m_bagRequests
          ),
          m_bankMoney(services, m_screen, m_status, m_bankGold, m_messages),
          m_atmDeposit(services, m_screen, m_status, m_atmGold, m_messages),
          m_atmList(
              services,
              m_screen,
              m_friends,
              m_guild,
              m_messages,
              {[this](const std::string& name) { return worldPlayerNamed(name); },
               [this] { return m_player.id; },
               [this](const std::string& name) { m_atmTransfer.setName(name); }}
          ),
          m_atmTransfer(
              services,
              m_screen,
              m_atmGold,
              m_messages,
              {[this](const std::string& name) { return worldPlayerNamed(name); },
               [this](const std::string& text, std::function<void()> onOk, std::function<void()> onCancel) {
                   TalkDialog::Timing timing;
                   timing.wrapChars = 32; // wrap the text at 32 characters
                   timing.exclusive = false;
                   m_talk.ask(text, std::nullopt, std::move(onOk), std::move(onCancel), std::move(timing));
               },
               [this] { m_atmList.open(); },
               [this] { m_atmList.close(); }}
          ),
          m_pointExchange(
              services,
              m_screen,
              m_messages,
              {[this] { return m_mallShop.points; },
               [this] { return m_status.gold; },
               [this] { return m_map ? m_map->id() : 0; },
               [this] { return m_player.id == m_house.furniture().ownerId(); },
               [this](const std::string& text, std::function<void()> onOk, std::function<void()> onCancel) {
                   TalkDialog::Timing timing;
                   timing.wrapChars = 32; // wrap the text at 32 characters
                   timing.exclusive = false;
                   m_talk.ask(text, std::nullopt, std::move(onOk), std::move(onCancel), std::move(timing));
               },
               [this](const std::string& line) { m_chat.add(0, line, 10); },
               [this](std::int32_t gold) { // system line, sender id 10
                   m_status.gold = gold;
                   refreshHud();
               },
               [this](std::int32_t gold) { m_atmGold = static_cast<std::uint32_t>(gold); },
               [this](std::int32_t id) { return speakerName(id); }}
          ),
          m_warehouse(services, m_screen, m_inventory, m_messages),
          m_adminItems(
              services,
              m_screen,
              [this](const std::string& line) { m_services.connection.send(protocol::chatRequest(2, line)); }
          ), // chat sub id 2: local
          m_saleNet(services, m_screen, m_saleRequestPending),
          m_saleMenu(services, m_screen, m_inventory, m_saleStore, m_status, m_saleGold, m_saleRequestPending, m_messages),
          m_guildWeaponBank(services, m_screen, m_inventory, m_guildWeaponCounts),
          m_petStorage(services, m_screen, m_pets, m_messages),
          m_petStall(
              services,
              m_screen,
              m_pets,
              m_status,
              m_messages,
              {[this] { return m_map ? m_map->id() : 0; },
               [this] { return m_teams.leaderOf(m_player.id).has_value(); },
               [this] { return m_player.stall && m_player.stall->kind == StallSign::Kind::Pets; },
               [this] { return m_player.stall.has_value(); },
               [this](const std::string& line) { m_chat.add(10, line, m_player.id); },
               [this] { return m_player.vehicle.has_value(); }, // chat channel 10: notice line
               [this] { return m_player.fishing; },
               [this] { return m_remote.isOn(); }}
          ),
          m_stallPets(services, m_screen, m_pets, m_status, m_messages, m_petInfo),
          m_utensilStall(services, m_screen, m_inventory, m_bagMarks, m_messages, m_itemInfo, utensilStallHooks()),
          m_buyUtensil(services, m_screen, m_inventory, m_status, m_messages, m_itemInfo, [this] { return m_activeSport != 0; }),
          m_blacklist(
              services,
              m_screen,
              m_messages,
              [this](const std::string& name) -> std::optional<std::int32_t> {
                  for (const auto& [id, record] : m_otherRecords) {
                      if (record.name == name) {
                          return id;
                      }
                  }
                  return std::nullopt;
              }
          ),
          m_mallShop(services.items),
          m_mall(
              services,
              m_screen,
              m_mallShop,
              m_status,
              m_player,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_talk,
              m_itemInfo,
              {[this] { return m_fightState != 0; },
               [this] { return m_map ? m_map->id() : 0; },
               [this] { bagMarksChanged(); },
               [this] { m_cart.toggle(); },
               [this] { m_cart.show(); },
               [this] { m_cart.hide(); },
               [this](int itemId, std::uint64_t nowMs) { return m_luckyBags.request(m_services.items, itemId, nowMs); },
               [this](int boxId) { m_articleInfo.show(boxId); },
               [this] { m_articleInfo.hide(); }}
          ),
          m_luckyBags(services.game.data("Data/Lbd.dat"), [this](const core::Bytes& message) { m_services.connection.send(message); }),
          m_articleInfo(services, m_screen, m_luckyBags, m_itemInfo),
          m_cart(
              services,
              m_screen,
              m_mallShop,
              m_status,
              m_inventory,
              m_messages,
              {[this] { return m_fightState != 0; }, [this] { m_mall.show(); }, [this] { m_mall.hide(); }}
          ),
          m_quests(services, m_screen, m_marks),
          m_monsterBook(services.npcs),
          m_monsterBookWindow(services, m_screen, m_monsterBook),
          m_notebook(
              services,
              m_screen,
              [this] { m_quests.toggleFromNotebook(); },
              [this] { m_monsterBookWindow.toggle(); },
              [this] { openStarSky(); }
          ),
          m_msgSkin(services, m_screen, m_talk),
          m_sendMail(
              services,
              m_screen,
              m_friends,
              m_guild,
              m_marriage,
              {[this](const SavedMail& mail) { m_mailBox.keepSent(mail); },
               [this] { m_mailBox.open(); },
               [this](core::Point position) { placeMailForms(position); },
               [this](bool toMail) { m_inputBar.setEmotesToMail(toMail); },
               [this] { return m_player.id; }}
          ),
          m_mailBox(
              services,
              m_screen,
              m_friends,
              m_guild,
              m_marriage,
              m_messages,
              {[this](std::int32_t id, const std::string& name) { return m_blacklist.contains(id, name); },
               [this](std::int32_t id) {
                   const auto found = m_others.find(id);
                   return found != m_others.end() ? found->second.name : std::string();
               },
               [this](const std::string& line) { m_chat.add(0, line); },
               [this](std::int32_t id, core::Point position) { m_sendMail.reply(id, position); },
               [this](std::int32_t id, core::Point position) {
                   m_sendMail.setPosition(position);
                   m_sendMail.openToGuildMember(id);
               },
               [this](std::int32_t id) -> std::optional<CharacterLook> {
                   const auto found = m_others.find(id);
                   return found != m_others.end() ? std::optional<CharacterLook>(found->second.look) : std::nullopt;
               },
               [this](core::Point position) { placeMailForms(position); },
               [this](const std::string& name, core::Point position) { m_furMail.openTo(name, position); },
               [this] { return m_furMail.hasMailbox(); },
               [this] { return m_furMail.takeOver(); },
               [this](const SavedMail& mail) { m_furMail.openSent(mail); },
               [this] { toggleFurMail(); }}
          ),
          m_furMail(
              services,
              m_screen,
              m_friends,
              m_guild,
              m_marriage,
              m_messages,
              {[this] { m_mailBox.open(); },
               [this](core::Point position) { placeMailForms(position); },
               [this](bool toMail) { m_inputBar.setEmotesToMail(toMail); },
               [this] {
                   if (m_sendMail.isOpen()) {
                       m_sendMail.hide();
                   }
               },
               [this] { return m_mailBox.takeOverFurReader(); },
               [this](const SavedMail& mail) { m_mailBox.keepSent(mail); },
               [this](std::int32_t id, const std::string& text) { m_sendMail.sendToFriendNamed(id, text); },
               [this](std::int32_t id, const std::string& text) { m_sendMail.sendToMemberNamed(id, text); },
               [this] { return m_player.id; }}
          ),
          m_changePassword(
              services,
              m_screen,
              m_messages,
              [this](const std::string& text, std::function<void()> onOk, std::function<void()> onCancel) {
                  m_dimTarget = kDarkStep;
                  m_dimStep = std::max(m_dimStep, 1);
                  const auto undim = [this] { m_dimTarget = 0; };
                  m_talk.ask(
                      text,
                      TalkDialog::Speaker{m_player.look, m_player.name},
                      [onOk, undim] {
                          onOk();
                          undim();
                      },
                      [onCancel, undim] {
                          onCancel();
                          undim();
                      }
                  );
              }
          ),
          m_potentialWindow(
              services,
              m_screen,
              m_status,
              m_pets,
              m_inventory,
              m_bagMarks,
              m_player,
              m_messages,
              m_talk,
              {[this] { return m_fightState != 0; }, [this] { m_itemWindow.close(); }, [this](const core::Bytes& bytes) { m_services.connection.send(bytes); }}
          ),
          m_potentialReturn(
              services,
              m_screen,
              m_pets,
              m_inventory,
              m_messages,
              m_talk,
              {[this] { return m_fightState != 0; }, [this] { m_itemWindow.close(); }, [this](const core::Bytes& bytes) { m_services.connection.send(bytes); }}
          ),
          m_storeTank(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              {[this](const core::Bytes& bytes) { m_services.connection.send(bytes); },
               [this] { return m_fightState != 0; },
               [this] { return m_player.vehicle ? m_player.vehicle->itemId : 0; }}
          ),
          m_capsule(services, m_screen, m_inventory, m_bagMarks, m_messages, m_itemInfo, capsuleHooks()),
          m_parking(
              services,
              m_screen,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              [this](const core::Bytes& bytes) { m_services.connection.send(bytes); }
          ),
          m_showPlatform(services, m_screen, m_house, m_itemInfo, [this](const core::Bytes& bytes) { m_services.connection.send(bytes); }),
          m_carCase(
              services,
              m_screen,
              m_house,
              m_inventory,
              m_bagMarks,
              m_messages,
              m_itemInfo,
              m_showPlatform,
              [this](const core::Bytes& bytes) { m_services.connection.send(bytes); }
          ),
          m_transport(services, m_screen, m_messages, transportHooks()),
          m_transmit(services.pictures),
          m_effectLights(services.pictures, m_random),
          m_timedLight(services.pictures),
          m_treasureLights(services.pictures),
          m_wishEffects(services.pictures, [this](const std::string& sound) { m_services.playSound(sound); }),
          m_throwShow(
              services.pictures,
              [this](const std::string& sound) { m_services.playSound(sound); },
              [this](int kind, core::Point target) { hitByThrow(kind, target); }
          ),
          m_itemInfo(services, m_screen, m_status),
          m_petInfo(services, m_screen),
          m_rightClickMenu(services, m_screen, m_inventory, m_status, m_messages, m_bagRequests, [this](int slot) { m_storeTank.open(slot); }),
          m_talk(services, m_screen),
          m_roleIntro(
              services,
              m_screen,
              {[this] { return m_player.look.bodyType; },
               [this] { return m_player.look.head; },
               [this] {
                   m_starSky.setRoleInfoShown(false);
                   m_starSkyWindow.setQuitShown(true);
               }}
          ),
          m_eventRunner(
              services,
              m_talk,
              m_npcStore,
              m_messages,
              {[this](const core::Bytes& bytes) { m_services.connection.send(bytes); },
               [this](int index) -> std::optional<core::Point> {
                   auto npc = findNpc(index);
                   return npc ? std::optional<core::Point>(npc->get().body.position) : std::nullopt;
               },
               [this](int index) -> std::optional<TalkDialog::Speaker> {
                   auto npc = findNpc(index);
                   return npc ? std::optional<TalkDialog::Speaker>(TalkDialog::Speaker{npc->get().info, npc->get().info.name, true}) : std::nullopt;
               },
               [this] { return m_player.position; },
               [this] { return ownSpeaker(true); },
               [this](core::Point target, int npcIndex) { faceEventTarget(target, npcIndex); },
               [this](const EventResult& result) { runWorldResult(result); },
               [this] { return std::any_of(m_npcs.begin(), m_npcs.end(), [](const MapNpc& npc) { return npc.eventWalking; }); },
               [this](const EventResult& result) { return startEventMovie(result); },
               [this] { return m_fightState != 0; },
               [this](int dataId, int code) {
                   if (m_services.partyEventAnswered) {
                       m_services.partyEventAnswered(dataId, code);
                   }
               },
               [this] {
                   if (m_services.partyEventEnded) {
                       m_services.partyEventEnded();
                   }
               }}
          ),
          m_rebornWish(
              services,
              m_inventory,
              m_talk,
              {[this](int flagBit) { return m_marks.hasFlag(flagBit); },
               [this] { return m_pets.count() < PetTeam::kPlaces; },
               [this](const std::string& text, int ms) { m_messages.show(text, ms); },
               [this] {
                   m_moveLocked = false;
                   m_eventRunner.moveOn();
               }}
          ),
          m_instanceGoals(services, m_screen, m_instances),
          m_messages(services, m_screen),
          m_centreMessages(services, m_screen, "CentreMessages.xml"),
          m_disconnect(services, m_screen, {[this] { m_next = SceneKind::Login; }, [this] { m_next = SceneKind::Quit; }}),
          m_activityInfo(services, m_screen),
          m_activities(services, m_screen, m_messages, m_activityInfo, activitiesHooks()),
          m_lottery(services, m_screen, m_inventory, m_messages, m_itemInfo, [this](const core::Bytes& message) { m_services.connection.send(message); }),
          m_magicMap(services, m_screen, m_bagMarks, m_messages, magicMapHooks()),
          m_magnet(services, m_screen, m_friends, m_guild, m_inventory, m_bagMarks, m_messages, magnetHooks()),
          m_blessBalloon(services, m_screen, m_friends, m_guild, m_inventory, m_bagMarks, m_messages, blessBalloonHooks()),
          m_blessMessage(services, m_screen, m_messages, blessMessageHooks()),
          m_loveFlower(services, m_screen, m_friends, m_guild, m_inventory, m_bagMarks, m_messages, loveFlowerHooks()),
          m_loveFlowerMessage(services, m_screen, m_messages, loveFlowerMessageHooks()),
          m_itemBag(services, m_screen, m_inventory, m_bagMarks, m_messages, m_countBox, itemBagHooks()),
          m_storeroom(services, m_screen, m_inventory, m_bagMarks, m_messages, m_countBox, storeroomHooks()),
          m_remote(services, m_inventory, m_bagMarks, m_status, m_pets, m_messages, remoteHooks()),
          m_remotePlus(services, m_screen, m_remote, m_pets, m_messages, remotePlusHooks()),
          m_remoteWindow(services, m_screen, m_remote, m_remotePlus, m_inventory, m_pets, m_status, m_messages, remoteWindowHooks()),
          m_makeQuestions(m_makeJobs, m_talk, m_messages, makeQuestionHooks()) {
        if (const auto sceneData = core::readFile(services.game.data("Data/SceneData.Dat"))) {
            m_scenes = formats::SceneTable::parse(*sceneData);
        }
        m_events = formats::EventArchive::open(services.game.data("Data/eve.Emg"));
        m_itemWindow.setNotebook([this] { m_notebook.open(); });
        m_itemWindow.setRightClick([this](int itemId, int slot) { rightClickItem(itemId, slot); });
        m_itemWindow.setBuffedStat([this](int stat) {
            const int level = m_statusEffects.strengthLevel(stat);
            return level > 0 && level <= 5 && StatusEffects::strengthValue(stat, level) > 0; // 5: highest strengthen level
        });
        m_itemWindow.setBuffMask([this] { return m_statusEffects.strengthMask(); });
        m_itemWindow.setSpecialItems([this](int itemId, int slot, int petOrder) {
            endThrowAim();
            return useSpecialItem(itemId, slot) || useRequestItem(itemId, slot, petOrder);
        });
        m_itemWindow.setUseVehicle([this](int slot) { useVehicleItem(slot); });
        m_itemWindow.setRepairDrop([this](int slot) { m_vehicleRepair.put(slot); });
        m_itemWindow.setBagDrop("CapsuleImage", [this](int slot) { m_capsule.put(slot); });
        for (int cell = 1; cell <= 4; ++cell) { // the 4 cells of a parking lot / car case
            m_itemWindow.setBagDrop("ParkingCell" + std::to_string(cell), [this, cell](int slot) { m_parking.put(slot, cell); });
            m_itemWindow.setBagDrop("CarCaseCell" + std::to_string(cell), [this, cell](int slot) { m_carCase.put(slot, cell); });
        }
        m_itemWindow.setTankDrop([this](int slot, int row) { m_storeTank.put(slot, row); });
        m_mainStatus.setVehicleGauge([this] { return vehicleGauge(); });
        m_itemWindow.setBankDrop([this](const ui::EventArgs& args) { m_bankItems.bagItemDropped(args); });
        m_itemWindow.setVendingDrop([this](int slot) { dropOnVending(slot); });
        m_itemWindow.setEggDrop([this](int slot, int place) { m_eggSell.drop(slot, place); });
        m_itemWindow.setBagRequests(m_bagRequests);
        m_itemWindow.setItemBagDrop([this](const ui::EventArgs& args) { m_itemBag.bagItemDropped(args); });
        m_itemWindow.setStoreroomDrop([this](const ui::EventArgs& args) { m_storeroom.bagItemDropped(args); });
        m_itemWindow.setAutoCombat([this] { return m_remote.isOn() && m_remote.settings().ticks[1]; });
        m_battleMain.setAutoFight([this] { return (m_remote.isOn() && m_remote.settings().ticks[1]) || m_services.autoBattle(); });
        m_battleMain.setToWindow([this](core::Point point) { return isViewZoomed() ? ViewZoom::toWindow(point, m_camera.view(), m_services.view) : point; });
        for (int cell = 1; cell <= RemoteSettings::kKeepItems; ++cell) {
            m_itemWindow.setBagDrop("RemoteItem" + std::to_string(cell), [this, cell](int slot) {
                m_remoteWindow.setKeptItem(cell, m_inventory.bagItem(slot).itemId);
            });
        }
        for (int key = 1; key <= 9; ++key) { // the 9 keys of the Remote Control Plus
            m_itemWindow.setBagDrop("PlusKey" + std::to_string(key), [this, key](int slot) { m_remotePlus.dropItem(key, m_inventory.bagItem(slot).itemId); });
        }
        m_skillWindow.setPlusDrop([this](int key, int skillId) { m_remotePlus.dropSkill(key, skillId); });
        m_hotKeys.setPlusDrop([this](int key, int skillId) { m_remotePlus.dropSkill(key, skillId); });
        for (const char* target : {"Forage", "ForageTarget1", "ForageTarget2", "ForageTarget3"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_forage.addTarget(slot); });
        }
        for (const char* target : {"Dismantle", "DismantleImage"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_dismantle.put(slot); });
        }
        for (const char* target : {"ConfuseStone", "ConfuseStoneEquip"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_confuseStone.putEquip(slot); });
        }
        m_itemWindow.setBagDrop("ConfuseStoneStone", [this](int slot) { m_confuseStone.putStone(slot); });
        m_itemWindow.setBagDrop("SewingKitPlace1", [this](int slot) { m_sewingKit.put(slot, 1); });
        m_itemWindow.setBagDrop("SewingKitPlace2", [this](int slot) { m_sewingKit.put(slot, 2); }); // second place of the Sewing Tools
        for (const char* target : {"SewingKitPlace3", "SewingKit"}) {
            m_itemWindow.setBagDrop(target, [](int) {});
        }
        for (const char* target : {"Packing", "PackingImage"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_packing.put(slot); });
        }
        for (const char* target : {"ReferWindow", "ReferImage"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_refer.put(slot); });
        }
        for (const char* target : {"RepairWindow", "RepairCells"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_repair.put(slot); });
        }
        for (const char* target : {"MagicBox", "MagicBoxImage"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_magicBox.put(slot); });
        }
        for (const char* target : {"Reinforce", "ReinforceImage"}) {
            m_itemWindow.setBagDrop(target, [this](int slot) { m_reinforce.put(slot); });
        }
        m_itemWindow.setBagDrop("PicnicCells", [this](int slot) { m_picnic.put(slot); });
        m_itemWindow.setBagDrop("ExpBoxCells", [this](int slot) { m_expBox.put(slot); });
        m_itemWindow.setExpressOpen([this](const Pet& pet) { openPetExpress(pet); });
        m_itemWindow.setPotentialToggle([this] { m_potentialWindow.toggle(); });
        m_itemWindow.setPetInStall([this](int place) { return m_petStall.petInStall(place); });
        m_itemWindow.setShownHook([this] { m_mainButtons.clearItemAlert(); });
        m_itemWindow.setCaseDrop([this](int slot) { m_stallCase.putItem(slot, false, 0); });
        m_itemWindow.setCupboardDrop([this](int slot, int place) { m_cupboard.drop(slot, place); });
        m_itemWindow.setHouseBoxDrop([this](bool locker, int slot, int place) {
            if (locker) {
                m_pledgeBox.drop(slot, place);
            } else {
                m_iceBox.drop(slot, place);
            }
        });
        m_itemWindow.setBookShelfDrop([this](bool desk, int slot, int place) { m_bookShelf.drop(desk, slot, place); });
        m_itemWindow.setHotKeyDrop([this](int slot, int itemId) { m_hotKeys.putItem(slot, itemId); });
        m_itemWindow.setWorldDrop([this](int slot, int itemId, int count) {
            return m_house.inFloorRoom() && m_fightState == 0 &&
                   m_houseEditor.dropBagItem(slot, itemId, count, worldMouse(), m_housePainter.hoveredSlot(), m_camera);
        });
        m_skillWindow.setHotKeyDrop([this](int slot, int skillId) { m_hotKeys.putSkill(slot, skillId); });
        m_skillWindow.setSend([this](const core::Bytes& message) { m_services.connection.send(message); });
        m_skillWindow.setBattlePick([this](int skillId) -> std::optional<bool> {
            if (!m_battle.isOn()) {
                return std::nullopt;
            }
            return m_battle.useListedSkill(skillId, true);
        });
        m_itemWindow.setBattle(
            [this] { return m_battle.isOn() && m_fightState != 0; },
            [this](int itemId, int slot, int count) { m_battle.chooseItem(itemId, slot, count); }
        );
        m_view.setPainter([this](gfx::Canvas& canvas, core::Point, std::uint64_t nowMs) {
            const std::optional<core::Rect> skillPart = m_battleShown ? m_battle.skillZoomPart() : std::nullopt;
            const std::optional<core::Rect> skillZoom = skillPart ? std::optional<core::Rect>(SkillZoom::sourceIn(*skillPart, m_services.view)) : std::nullopt;
            const bool frozen = !m_battleShown && m_dimStep == kDarkStep && m_dimTarget == kDarkStep;
            if (!frozen) {
                m_frozenWorld.reset();
            }
            if (frozen && m_frozenWorld) {
                canvas.drawTexture(*m_frozenWorld, canvas.window().position());
            } else {
                followView(nowMs);
                if (skillZoom) {
                    canvas.beginZoom(m_services.view);
                } else if (isViewZoomed()) {
                    canvas.beginZoom(m_camera.view());
                }
                if (m_battleShown) {
                    m_battle.paintGround(canvas);
                } else {
                    paintWorld(canvas, nowMs);
                }
                constexpr int kDimAlpha = 12; // black overlay alpha (0..255) per dim step
                int dim = m_dimStep;
                if (const auto amount = canvas.betweenBeats();
                    amount && !frozen && m_dimStepMs >= canvas.beatMs() && std::abs(m_dimStep - m_dimStepBefore) == 1) {
                    dim = core::between(m_dimStepBefore, m_dimStep, *amount);
                }
                if (dim > 0 && canvas.exactAlphaFills()) {
                    formats::Image frame = canvas.device().readFrame();
                    formats::alphaFill16(frame, {0, 0, 0, static_cast<std::uint8_t>(dim * kDimAlpha)});
                    if (auto dimmed = gfx::Texture::create(canvas.device(), frame)) {
                        canvas.drawTexture(*dimmed, canvas.window().position());
                        if (frozen) {
                            m_frozenWorld = std::move(dimmed);
                        }
                    }
                } else if (dim > 0) {
                    canvas.fillRect(canvas.window(), {0, 0, 0, static_cast<std::uint8_t>(dim * kDimAlpha)});
                    // a renderer that reads back slowly (Vulkan) keeps drawing the dimmed world instead of freezing a copy of it
                    if (frozen && canvas.device().readsBackQuickly()) {
                        m_frozenWorld = canvas.copyWindow();
                    }
                }
            }
            if (m_fightState != 0) {
                m_battle.paintField(canvas, m_screen.mouse(), nowMs);
                if (skillZoom) {
                    canvas.endZoom(*skillZoom); // the labels are drawn over the stretched frame (DrawFightScene after StretchBufToScreen)
                }
                if (m_battleShown) {
                    m_battle.paintLabels(canvas, m_screen.mouse());
                }
            }
            if (m_innSleep != InnSleep::Awake) {
                paintInnSleep(canvas, nowMs);
            }
            canvas.endZoom(skillZoom ? *skillZoom : m_camera.view());
        });
        m_screen.on("onMapMouseDown", [this](const ui::EventArgs& local) {
            const ui::EventArgs args = onScreen(local);
            if (m_maid.isActive()) {
                return;
            }
            if (m_battle.isOn()) {
                if (m_battleShown) {
                    m_battle.click(args.position);
                }
                return;
            }
            m_mouseHeld = true;
            m_mouse = args.position;
            if (m_house.inFloorRoom() && m_fightState == 0 && !m_teams.isMember(m_player.id) && !m_player.stall && m_activeSport == 0) {
                if (const auto door = m_waymark.click(m_camera, args.position)) {
                    m_mouseHeld = false;
                    jumpToWaymark(*door);
                    return;
                }
            }
            if (m_activeSport != 0) {
                m_mouseHeld = false;
                return;
            }
            if (m_eventRunner.isRunning()) {
                m_mouseHeld = false;
                m_eventRunner.clicked();
                return;
            }
            if (m_homeAdorn.press(args.position, m_camera)) {
                m_mouseHeld = false;
                return;
            }
            if (m_house.inFloorRoom() && m_fightState == 0 && m_housePainter.hoveredShelfItem() != 0) {
                m_mouseHeld = false;
                m_pressedShelfItem = m_housePainter.hoveredShelfItem();
                return;
            }
            if (m_house.inFloorRoom() && m_fightState == 0 && m_housePainter.hoveredCaseItem() != 0) {
                m_mouseHeld = false;
                m_pressedCaseItem = m_housePainter.hoveredCaseItem();
                return;
            }
            if (pressVisitorPiece()) {
                m_mouseHeld = false;
                return;
            }
            if (m_house.inFloorRoom() && m_fightState == 0 && m_housePainter.hoveredSlot() != 0 &&
                m_houseEditor.press(m_housePainter.hoveredSlot(), args.position, m_camera)) {
                m_mouseHeld = false;
                return;
            }
            if (m_fightState == 0 && m_inWorld && m_makeJobs.petSlot() == 0 && m_makeJobs.activeSlot() != 0) {
                m_mouseHeld = false;
                m_makeQuestions.askStop();
                return;
            }
            if (m_player.vehicle && VehicleKinds::teleportKind(m_player.vehicle->itemId) != 0 && ownPlayerAt(m_mouse)) {
                m_transport.open(VehicleKinds::teleportKind(m_player.vehicle->itemId));
                if (!m_hotKeyAim) {
                    m_mouseHeld = false;
                    return;
                }
            }
            if (m_hotKeyAim) {
                if (ownPlayerAt(m_mouse)) {
                    m_mouseHeld = false;
                    useHotKeyOn(std::nullopt);
                    return;
                }
                if (const auto pet = ownPetAt(m_mouse)) {
                    m_mouseHeld = false;
                    useHotKeyOn(*pet);
                    return;
                }
                if (playerAt(m_mouse)) {
                    m_mouseHeld = false;
                    endHotKeyAim();
                    return;
                }
            }
            if (m_picnicAim) {
                if (ownPlayerAt(m_mouse)) {
                    m_mouseHeld = false;
                    m_picnic.useOn(1, 0);
                    return;
                }
                if (const auto pet = ownPetAt(m_mouse)) {
                    m_mouseHeld = false;
                    m_picnic.useOn(4, *pet); // target kind 4: a pet (1 is the player)
                    return;
                }
            }
            if (m_mouseMode == kThrowMouseMode) {
                m_mouseHeld = false;
                throwAtMouse();
                return;
            }
            if (m_mouseMode != 0) {
                const int mode = m_mouseMode;
                setMouseMode(0);
                if (const auto playerId = playerAt(m_mouse)) {
                    m_mouseHeld = false;
                    if (mode == 1) {
                        askToPk(*playerId);
                    } else if (mode == 3) { // mouse mode 3: watch a fight
                        askToWatchFight(*playerId);
                    } else if (mode == 4) { // mouse mode 4: join a fight
                        askToJoinFight(*playerId);
                    } else if (mode == 2) { // mouse mode 2: ask to join a team
                        askToJoin(*playerId);
                    } else if (mode == 5) { // mouse mode 5: ask to be friends
                        askToBeFriends(*playerId);
                    } else if (mode == 9) { // mouse mode 9: trade items
                        askToTradeItems(*playerId);
                    } else if (mode == 10) { // mouse mode 10: trade pets
                        askToTradePets(*playerId);
                    } else if (mode == 19) { // mouse mode 19: trade utensils
                        askToTradeUtensils(*playerId);
                    } else if (mode == 18) { // mouse mode 18: invite to the guild
                        inviteToGuild(*playerId);
                    } else {
                        inviteToTeam(*playerId);
                    }
                    return;
                }
                if (const auto npc = mode == 1 ? npcAt(m_mouse) : std::nullopt) {
                    m_mouseHeld = false;
                    askToFightNpc(*npc);
                    return;
                }
            }
            if (stallPlateAt(m_mouse)) {
                m_mouseHeld = false;
                return;
            }
            if (specFloorHovered()) {
                m_mouseHeld = false;
                m_specFloorPressed = true;
                return;
            }
            if (const int ground = m_ground.indexAt(m_mouse, m_camera)) {
                m_mouseHeld = false;
                m_pressedGround = ground;
                return;
            }
            if (const auto npc = npcAt(m_mouse)) {
                m_mouseHeld = false;
                clickNpc(*npc);
                return;
            }
            if (const auto camp = campAt(m_mouse)) {
                m_mouseHeld = false;
                clickCamp(*camp);
                return;
            }
            if (const auto vehicle = vehicleAt(m_mouse)) {
                m_mouseHeld = false;
                clickVehicle(*vehicle);
                return;
            }
            if (canWalk()) {
                if (m_player.stall) {
                    m_messages.show("Unable to move when you are setting a stall", 1500); // message display time in ms
                } else {
                    m_walkMark.start(walkTo(m_camera.toMap(m_mouse)));
                }
            }
        });
        m_screen.on("onMapDoubleClick", [this](const ui::EventArgs& local) {
            const ui::EventArgs args = onScreen(local);
            if (m_fightState == 0) {
                if (const auto playerId = playerAt(args.position)) {
                    m_inputBar.setWhisperTarget(*playerId);
                }
            }
            if (const auto owner = stallPlateAt(args.position)) {
                openStallOf(*owner);
            }
        });
        m_screen.on("onMapRightMouseDown", [this](const ui::EventArgs&) {
            if (m_battleShown && m_battle.isOn() && m_fightState != 0 && m_fightState != 4) { // fight state 4: watching a battle
                m_battle.cancelCommand();
            }
            if (m_player.vehicle && !m_teams.isMember(m_player.id) && m_fightState == 0 && m_map) {
                const int attribute = m_map->tileAttribute(m_ownTile);
                if (attribute == 0 || attribute == formats::MapFile::kShallowWater) {
                    parkRiddenVehicle();
                } else {
                    m_messages.show("Unable to cancel transportation vehicle here", 2000); // message display time in ms
                }
            }
            bool taken = false;
            if (m_house.inFloorRoom() && m_fightState == 0 && m_housePainter.hoveredShelfItem() != 0) {
                askStopSelling(m_housePainter.hoveredShelfItem(), false);
                taken = true;
            }
            if (m_house.inFloorRoom() && m_fightState == 0 && m_housePainter.hoveredSlot() != 0) {
                m_houseEditor.rightClick(m_housePainter.hoveredSlot());
                taken = true;
            }
            if (const auto camp = campAt(worldMouse()); camp && m_camps.camps().at(*camp).own && m_camps.ownMap() == (m_map ? m_map->id() : 0)) {
                takeBackTent();
                taken = true;
            }
            if (!taken) {
                faceMouse();
            }
            setMouseMode(0);
            m_hotKeyAim.reset();
            if (m_picnicAim) {
                m_picnic.cancelAim();
            }
        });
        m_screen.on("onFunctionMall", [this](const ui::EventArgs&) { m_mall.toggle(); });
        m_screen.on("onFunctionPk", [this](const ui::EventArgs&) { startBattleMode(1); });
        m_screen.on("onFunctionJoinCombat", [this](const ui::EventArgs&) { startBattleMode(4); }); // mouse mode 4: join a fight
        m_screen.on("onFunctionWatchCombat", [this](const ui::EventArgs&) { startBattleMode(3); }); // mouse mode 3: watch a fight
        m_screen.on("onFunctionTeam", [this](const ui::EventArgs&) { startTeamMode(); });
        m_screen.on("onFunctionFriends", [this](const ui::EventArgs&) { startFriendMode(); });
        m_screen.on("onFunctionGuildInvite", [this](const ui::EventArgs&) {
            constexpr int kNoGuildMap = 10072; // map 10072: Survival Isle (guild invite refused)
            if (m_map && m_map->id() == kNoGuildMap) {
                m_messages.show("Cannot use this function", 1000); // message display time in ms
            } else {
                startGuildInvite();
            }
        });
        m_screen.on("onFunctionMoney", [this](const ui::EventArgs&) { toggleWindow('m'); });
        m_screen.on("onFunctionSchedule", [this](const ui::EventArgs&) { toggleActivities(); });
        m_screen.on("onFunctionDraw", [this](const ui::EventArgs&) { openLottery(); });
        m_screen.on("onMapMouseDrag", [this](const ui::EventArgs& local) {
            const ui::EventArgs args = onScreen(local);
            m_mouse = args.position;
            m_homeAdorn.drag(args.position);
            m_houseEditor.dragTo(args.position, m_camera);
        });
        m_screen.on("onMapMouseUp", [this](const ui::EventArgs& local) {
            const ui::EventArgs args = onScreen(local);
            m_mouseHeld = false;
            const int pressedItem = m_pressedShelfItem;
            m_pressedShelfItem = 0;
            const int pressedCase = m_pressedCaseItem;
            m_pressedCaseItem = 0;
            const int pressedPiece = m_pressedVisitorPiece;
            m_pressedVisitorPiece = 0;
            if (pressedPiece != 0 && m_housePainter.hoveredSlot() == pressedPiece) {
                clickVisitorPiece(pressedPiece);
            }
            if (pressedItem != 0 && m_housePainter.hoveredShelfItem() == pressedItem) {
                clickShelfItem(pressedItem);
            }
            if (pressedCase != 0 && m_housePainter.hoveredCaseItem() == pressedCase) {
                clickCaseItem(pressedCase);
            }
            m_homeAdorn.release(args.position);
            m_houseEditor.release(args.position, m_housePainter.hoveredSlot(), m_camera);
            if (m_specFloorPressed && specFloorHovered()) {
                pickSpecFloorThing();
            }
            m_specFloorPressed = false;
            const int pressed = m_pressedGround;
            m_pressedGround = 0;
            if (pressed != 0 && m_ground.indexAt(args.position, m_camera) == pressed) {
                pickUp(pressed);
            }
        });
        m_screen.context().playSound = [this](const std::string& sound) { m_services.playSound(sound); };
        m_screen.on("onMainRetained", [this](const ui::EventArgs&) { m_retainFur.toggle(); });
        m_screen.on("onItemWindowEscape", [this](const ui::EventArgs&) { m_itemWindow.close(); });
        m_screen.on("onSkillWindowEscape", [this](const ui::EventArgs&) { m_skillWindow.close(); });
        m_screen.on("onSettingsWindowEscape", [this](const ui::EventArgs&) { m_systemWindow.close(); });
        m_screen.on("onGroupWindowEscape", [this](const ui::EventArgs&) { m_groupWindow.close(); });
        m_screen.on("onSocialWindowEscape", [this](const ui::EventArgs&) { m_socialWindow.close(); });
        m_screen.on("onCompoundWindowEscape", [this](const ui::EventArgs&) { m_compoundWindow.close(); });
        m_screen.on("onMoneyMenuEscape", [this](const ui::EventArgs&) { m_moneyMenu.close(); });
        m_screen.on("onMainItem", [this](const ui::EventArgs&) { toggleWindow('i'); });
        m_screen.on("onMainSkill", [this](const ui::EventArgs&) { toggleWindow('s'); });
        m_screen.on("onMainSystem", [this](const ui::EventArgs&) { toggleWindow('y'); });
        m_screen.on("onMainGroup", [this](const ui::EventArgs&) { toggleWindow('g'); });
        m_screen.on("onMainSocial", [this](const ui::EventArgs&) { toggleWindow('e'); });
        m_screen.on("onMainCompound", [this](const ui::EventArgs&) { toggleWindow('c'); });
        m_talk.setExclusiveHook([this](bool exclusive) { showGameWindows(!exclusive); });
        m_talk.setTagSource([this] { return m_player.name; }, [this] { return m_castleOwner; });
        m_chat.setSpeakerLooks([this](std::int32_t playerId) -> std::optional<CharacterLook> {
            if (playerId == m_player.id) {
                return m_player.look;
            }
            const auto found = m_others.find(playerId);
            if (found == m_others.end() && m_marriage.spouseId() != 0 && playerId == m_marriage.spouseId()) {
                return m_marriage.card().look;
            }
            return found != m_others.end() ? std::optional<CharacterLook>(found->second.look) : std::nullopt;
        });
        m_chat.setObserver([this](const std::string& text) { m_seenTexts.add(text); });
        m_messages.setObserver([this](const std::string& text) { m_seenTexts.add(text); });
        m_centreMessages.setObserver([this](const std::string& text) { m_seenTexts.add(text); });
        m_systemWindow.load();
        m_screen.setKeyPreview([this](const ui::KeyEvent& key) { return onKey(key); });
        m_screen.setTextFallback([this] {
            if (m_sendMail.isOpen()) {
                m_screen.focus("SendMailText");
            } else if (m_inputBar.isVisible()) {
                m_screen.focus("InputText");
            }
            if (m_noticeSend.isOpen()) {
                m_screen.focus("NoticeSendText");
            }
        });

        m_services.music.stop();
        const auto& record = m_services.session.ownPlayer;
        if (!record || !m_mapArchive || !m_objectArchive) {
            core::logError("cannot enter the world: ", record ? "the map archives are missing" : "no player record");
            m_next = SceneKind::Login;
            return;
        }
        m_player.id = record->playerId;
        m_house.furniture().setOwnerId(m_player.id);
        m_msgSkin.load(m_player.id);
        m_chat.setOwnId(m_player.id);
        m_chat.setWhisperHook([this](std::int32_t id, bool fromHead) { m_inputBar.setWhisperTarget(id, fromHead); });
        m_chat.apply(TalkSettings::load(SystemSettings::fileFor(services.game.userRoot(), m_services.session.loginId), m_services.session.loginId));
        m_inputBar.setChannel(2); // chat channel 2: local
        m_services.connection.send(protocol::talkFormReadyRequest());
        constexpr std::int32_t kMailFolderKey = 121; // XOR key that scrambles the player id in the mail log file name
        const int serverFolder = m_services.session.serverGrouped ? m_services.session.serverId / 100 * 100
                                                                  : m_services.session.serverId; // grouped servers share the folder of their hundred
        m_mailBox.load(services.game.userRoot() / ("Server" + std::to_string(serverFolder)) / ("MailBoxMsgLog" + std::to_string(m_player.id ^ kMailFolderKey)));
        importMailLogs(MailLogImporter::Kind::MailBox);
        m_activities.requestSchedule();
        m_player.name = record->name;
        m_player.nickname = record->nickname;
        m_player.pvpWins = record->pvpWins;
        m_player.pvpRank = record->pvpRank;
        m_player.arenaVictory = record->arenaVictory;
        m_player.nameColor = kOwnNameColor;
        m_player.look = CharacterLook::fromAppearance(record->appearance, services.items);
        m_socialWindow.loadProfile(*record);
        m_socialWindow.loadHotkeys(services.game.user(std::to_string(m_player.id) + "hotkey"));
        m_socialWindow.showFriendTab();
        constexpr std::int32_t kSaveFileKey = 121; // XOR key that scrambles the player id in the saveNNN.dat file name
        m_compoundWindow.setSettingsFile(services.game.user("save" + std::to_string(m_player.id ^ kSaveFileKey) + ".dat"), m_player.id);
        enterMap(record->mapId, {record->x, record->y});
        m_chat.add(0, "Welcome to[" + m_services.session.serverName + "]Server");
    }

    void WorldScene::enterMap(int mapId, core::Point position) {
        m_map = WorldMap::load(mapId, mapFileOf(mapId), *m_mapArchive, *m_objectArchive, m_services.pictures, houseBackground(mapId));
        if (!m_map) {
            m_next = SceneKind::Login;
            return;
        }
        m_house.setPlace(isHouseMap(mapId), m_houseRoom);
        closeHouseForms();
        m_waymark.setVisible(m_house.inFloorRoom());
        m_player.position = position;
        m_ground.clear();
        stopAction(m_player);
        m_player.direction = kFacingSouth;
        startWalking(m_player, {}, m_nowMs);
        for (const auto& [id, other] : m_others) {
            m_mapPets.remove(id);
            m_trails.remove(id);
        }
        m_others.clear();
        m_mateVitals.clear();
        m_teams.keepOnly(m_player.id);
        m_trails.scatter(m_player.id, position, m_random);
        m_mapPets.ownerPlaced(m_player.id);
        constexpr int kWeddingHall = 10085; // map 10085: Wedding Hall
        m_mapPets.setHidden(m_player.id, false);
        m_mapPets.setHallMap(mapId == kWeddingHall);
        m_camps.clear();
        m_effectLights.clear();
        m_specFloor.hide();
        m_vehicles.clear();
        if (!m_entering) {
            m_miniMap.showMap(*m_map, sceneType());
            m_miniMap.raise();
        }
        placeEventObjects();
        m_camera.centerOn(position, m_map->width(), m_map->height());
    }

    void WorldScene::placeEventObjects() {
        m_npcs.clear();
        m_doors.clear();
        const auto scene = m_events && m_map ? m_events->scene(mapFileOf(m_map->id())) : std::nullopt;
        if (!scene) {
            m_miniMap.setDoors(m_doors);
            return;
        }
        m_doors = scene->doors;
        for (const formats::SceneDoor& door : m_doors) {
            m_waymark.setDoor({door.area.x, door.area.y});
        }
        m_eventRunner.setSurfaces(scene->surfaces);
        m_miniMap.setDoors(m_doors);
        for (const formats::SceneDoor& door : m_doors) {
            if (door.cursorKind() == 0 || (door.lightPosition.x == 0 && door.lightPosition.y == 0)) {
                continue;
            }
            DoorLight light;
            light.picture = m_services.pictures.find(door.cursorKind() == 1 ? "DoorLight" : "SmallDoorLight").value_or(gfx::PictureId{});
            light.frames = door.cursorKind() == 1 ? 13 : 4; // frame count of the door light picture (large 13, small 4)
            light.position = door.lightPosition;
            if (light.picture.isValid()) {
                m_map->addDoorLight(light);
            }
        }
        constexpr int kDirections[] = {0, 0, 7, 6, 5, 4, 3, 2, 1}; // placed NPC direction 1..8 (index) -> client direction 0..7
        for (const formats::SceneNpc& placed : scene->npcs) {
            constexpr int kUnknownNpcSprite = 1000; // sprite used for an NPC id that is not in the NPC table
            formats::NpcInfo unknown;
            constexpr std::int32_t kPlainColors = 444444444; // colour code with all nine colour digits set to 4 (default colours)
            unknown.spriteId = kUnknownNpcSprite;
            unknown.colors.fill(kPlainColors);
            const auto info = m_services.npcs.find(placed.npcId).value_or(unknown);
            MapNpc npc;
            npc.index = placed.index;
            npc.info = info;
            npc.visible = (placed.flags & 1) != 0;
            npc.showState = npc.visible ? 1 : 2; // show state 1: shown for good, 2: hidden for good
            constexpr int kHideableNpc = 38010; // NPC 38010 (not in the NPC table): hidden while NPCs are hidden (original FHumId = 38010)
            if (placed.npcId == kHideableNpc && m_npcsHidden) {
                npc.visible = false;
            }
            npc.home = placed.position;
            npc.drawOffsetY = placed.drawOffsetY;
            npc.speed = placed.speed >= 1 && placed.speed <= 10 ? placed.speed : 2; // speed 1..10, default 2
            npc.body.name = info.name;
            npc.body.position = placed.position;
            npc.eventCount = static_cast<int>(placed.eventIds.size());
            npc.mapSign = placed.mapSign;
            npc.startsNear = (placed.triggerHalfSize.x == 0 || placed.triggerHalfSize.y == 0) &&
                             std::any_of(placed.eventIds.begin(), placed.eventIds.end(), [&scene](int id) { return scene->nearEvents.count(id) != 0; });
            npc.triggerHalfSize = placed.triggerHalfSize;
            const bool sized = placed.triggerHalfSize.x != 0 && placed.triggerHalfSize.y != 0;
            npc.body.direction =
                !sized && placed.direction >= 1 && placed.direction <= 8 ? kDirections[placed.direction] : 4; // placed direction 1..8, else direction 4 (south)
            npc.body.pixelsPerMs = npc.speed / 8.0 * 0.16; // 0.16 pixels/ms is the player's pace; speed is in eighths of it
            npc.ownPace = npc.body.pixelsPerMs;
            npc.eventWalks = placed.eventWalks;
            npc.homeDirection = npc.body.direction;
            startRoutine(npc, placed);
            m_npcs.push_back(npc);
        }
    }

    void WorldScene::startRoutine(MapNpc& npc, const formats::SceneNpc& placed) {
        NpcRoutine& routine = npc.routine;
        const auto& path = placed.path;
        switch (placed.moveKind) {
            case 2: // move kind 2: patrol along the placed path
                routine.kind = NpcRoutine::Kind::Patrol;
                routine.end = placed.pathEnd == 1   ? NpcRoutine::End::Again
                              : placed.pathEnd == 2 ? NpcRoutine::End::Back // path end 1: again, 2: back, else stop
                                                    : NpcRoutine::End::Stop;
                routine.points.push_back(placed.position);
                routine.waits.push_back(0);
                for (const formats::SceneNpc::PathPoint& point : path) {
                    routine.points.push_back(point.position);
                    routine.waits.push_back(point.value);
                }
                break;
            case 3: // move kind 3: wander inside the rectangle of the first two path points
                if (path.size() >= 2) { // the area needs two corner points
                    routine.kind = NpcRoutine::Kind::Wander;
                    routine.area = {
                        placed.position.x + path[0].position.x,
                        placed.position.y + path[0].position.y,
                        std::abs(path[0].position.x - path[1].position.x),
                        std::abs(path[0].position.y - path[1].position.y)
                    };
                    routine.wanderWaitMs = path[0].value;
                }
                break;
            default: // any other move kind: the NPC stands where it is placed
                if (npc.visible) {
                    m_map->occupy(placed.position);
                }
                if (const int frames = m_services.characters.npcFrameCount(
                        npc.info.spriteId,
                        m_services.characters.npcSlot(npc.info.spriteId, npc.info.directionKind, npc.body.actionSlot())
                    );
                    frames > 0) {
                    npc.body.animation.startAt(std::uniform_int_distribution<int>(0, frames - 1)(m_random));
                }
                break;
        }
    }

    std::optional<std::reference_wrapper<MapNpc>> WorldScene::findNpc(int index) {
        const auto found = std::find_if(m_npcs.begin(), m_npcs.end(), [index](const MapNpc& npc) { return npc.index == index; });
        if (found == m_npcs.end()) {
            return std::nullopt;
        }
        return std::ref(*found);
    }

    void WorldScene::onNpcMoves(int npcIndex, core::Point to, bool walk) {
        auto npc = findNpc(npcIndex);
        if (!npc || !m_map) {
            return;
        }
        if (walk) {
            startWalking(npc->get().body, findPath(*m_map, npc->get().body.position, to), m_nowMs);
        } else {
            npc->get().body.position = to;
            startWalking(npc->get().body, {}, m_nowMs);
        }
    }

    void WorldScene::checkMoveEvents(std::uint64_t nowMs) {
        if (!m_inWorld || nowMs - m_doorCheckMs < kDoorCheckMs) {
            return;
        }
        m_doorCheckMs = nowMs;
        if (m_chaseNpc != 0) {
            checkChase();
        }
        checkDoors();
        checkNearNpcs(nowMs);
    }

    void WorldScene::checkDoors() {
        const auto door = std::find_if(m_doors.begin(), m_doors.end(), [this](const formats::SceneDoor& candidate) {
            return candidate.area.contains(m_player.position) && !candidate.eventIds.empty();
        });
        if (door == m_doors.end()) {
            m_currentDoor = 0;
            return;
        }
        if (door->index == m_currentDoor || m_eventRunner.isRunning()) {
            return;
        }
        m_currentDoor = door->index;
        m_mouseHeld = false;
        // members first: a door that warps takes the team along, and a member's own 20/8 must reach the server before that
        if (m_services.partyEventStarted) {
            m_services.partyEventStarted(PartyTrigger::Door, door->index);
        }
        m_eventRunner.doorEntered(door->index);
    }

    void WorldScene::checkNearNpcs(std::uint64_t nowMs) {
        constexpr int kNear = 40; // distance on each axis within which a "starts near" NPC triggers its event, px
        constexpr std::uint64_t kAgainMs = 6000; // an NPC cannot trigger again for this long after its event ended, ms
        if (m_eventRunner.isRunning()) {
            return;
        }
        for (const MapNpc& npc : m_npcs) {
            if (!npc.visible || !npc.startsNear || std::abs(npc.body.position.x - m_player.position.x) > kNear ||
                std::abs(npc.body.position.y - m_player.position.y) > kNear || (npc.nearEndedMs != 0 && nowMs - npc.nearEndedMs < kAgainMs)) {
                continue;
            }
            m_nearNpc = npc.index;
            m_eventRunner.npcNear(npc.index);
            if (m_services.partyEventStarted) {
                m_services.partyEventStarted(PartyTrigger::NpcNear, npc.index);
            }
            return;
        }
    }

    void WorldScene::onMapChanged(std::int32_t playerId, int mapId, core::Point position) {
        if (static_cast<std::uint32_t>(protocol::stripServerOffset(playerId)) != m_services.session.loginId) {
            const auto moved =
                setWorldPlayerMap(m_otherRecords, playerId, mapId, static_cast<std::uint16_t>(position.x), static_cast<std::uint16_t>(position.y));
            if (m_map && mapId == m_map->id() && moved) {
                placeOther(moved->get());
            } else if (!m_map || mapId != m_map->id()) {
                m_others.erase(playerId);
            }
            return;
        }
        m_maid.reset();
        m_prevMapId = m_map ? m_map->id() : 0;
        const bool tripEnded = m_transmit.hidesPlayer() && VehicleKinds::teleportKind(m_player.vehicle ? m_player.vehicle->itemId : 0) != m_transport.kind();
        m_inWorld = false;
        m_mouseHeld = false;
        m_lastWalkSent.reset();
        m_services.music.stop();
        const int facing = m_player.direction;
        enterMap(mapId, position);
        m_player.direction = facing;
        m_ownTile = {position.x / formats::MapFile::kCellSize, position.y / formats::MapFile::kCellSize};
        if (tripEnded) {
            m_transmit.arrive(m_camera.toScreen(m_player.position), m_nowMs);
        }
        m_gpsEffect.arrive(m_camera.toScreen(m_player.position), m_nowMs);
        m_services.connection.send(protocol::mapLoadedRequest());
        m_musicDelay = kMapChangeMusicFrames;
    }

    void WorldScene::onEventResult(const EventResult& result) {
        m_movie.director().serverMessage();
        m_eventRunner.result(result);
    }

    void WorldScene::onEventSignal(int sub, std::int32_t value) {
        m_movie.director().serverMessage();
        m_eventRunner.signal(sub, value);
    }

    void WorldScene::onNpcEmotion(int npcIndex, int emotionId) {
        if (auto npc = findNpc(npcIndex)) {
            npc->get().body.emotion.show(emotionId, m_nowMs);
        }
    }

    void WorldScene::onNpcAction(int npcIndex, int action, int holdFrame, bool needsFrames) {
        auto npc = findNpc(npcIndex);
        if (!npc) {
            return;
        }
        if (needsFrames) {
            const int group = actionGroup(action);
            const int directions = group == 0 || group == 8 || group == 18 || group == 46 || group == 54 || group == 66
                                       ? 8
                                       : 2; // action groups 0, 8, 18, 46, 54, 66 have 8 direction slots, the others 2
            bool found = false;
            for (int k = 0; k < directions && !found; ++k) {
                found = m_services.characters.npcFrameCount(npc->get().info.spriteId, action + k) > 0;
            }
            if (!found) {
                return;
            }
        }
        npc->get().actionHoldFrame = holdFrame;
        startAction(npc->get().body, action);
        npc->get().held = false;
        m_npcFacings.restore(npcIndex);
    }

    void WorldScene::onStoreBought(int result) { m_npcStore.bought(result); }

    void WorldScene::onStoreSold(int result) { m_npcSale.sold(result); }

    void WorldScene::onSaleOpened(bool goods) {
        m_npcSale.show(goods, [this] { m_eventRunner.moveOn(); });
    }

    void WorldScene::onBankGold(std::int32_t gold) { m_bankGold = gold; }

    void WorldScene::onBankGoldMoved(std::int32_t amount) {
        constexpr std::int32_t kMostInBank = 9999999; // largest gold balance in the bank (9,999,999)
        if ((amount > 0 && kMostInBank - m_bankGold >= amount) || (amount < 0 && m_bankGold >= -amount)) {
            m_bankGold += amount;
        }
    }

    void WorldScene::onBankFull() { m_messages.show("The warehouse is full.", 2000); } // message display time in ms

    void WorldScene::onBankOpened(bool items) {
        if (items) {
            m_bankItems.show([this] { m_eventRunner.moveOn(); });
        } else {
            m_bankMoney.show([this] { m_eventRunner.moveOn(); });
        }
    }

    void WorldScene::onBankGoldDone(bool deposit) { m_bankMoney.done(deposit); }

    void WorldScene::onBankItemPut(const protocol::BagItemRecord& record) {
        if (record.slot >= 1 && record.slot <= Inventory::kBagSlots) {
            m_bank.addToBag(record.slot, record.item);
            m_bankItems.refresh();
        }
    }

    void WorldScene::onBankItemAdded(const protocol::ItemInstance& item) {
        protocol::ItemInstance rest = item;
        for (int tries = 0; rest.count > 0 && tries < Inventory::kBagSlots; ++tries) {
            int slot = 0;
            if (m_bank.isStackable(item.itemId)) {
                for (int at = 1; at <= Inventory::kBagSlots && slot == 0; ++at) {
                    const protocol::ItemInstance& there = m_bank.bagItem(at);
                    if (there.itemId == item.itemId && there.count < Inventory::kMaxStack) {
                        slot = at;
                    }
                }
            }
            if (slot == 0) {
                slot = m_bank.freeSlotFor(item.itemId);
            }
            if (slot == 0) {
                break;
            }
            rest.count -= m_bank.addToBag(slot, rest);
        }
        m_bankItems.refresh();
        m_warehouse.refresh();
    }

    void WorldScene::onBankItemRemoved(int itemId, int count) {
        m_bank.removeFromBag(itemId, count);
        m_bankItems.refresh();
    }

    void WorldScene::onBankSlotDecreased(int slot, int count) {
        if (slot >= 1 && slot <= Inventory::kBagSlots) {
            m_bank.takeFromBag(slot, count);
            m_bankItems.refresh();
        }
    }

    void WorldScene::onBankItemsDone(bool deposit) { m_bankItems.done(deposit); }

    void WorldScene::onBankCleared() {
        for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
            m_bank.takeFromBag(slot, Inventory::kMaxStack);
        }
        m_bankItems.refresh();
    }

    void WorldScene::onGroundItems(const std::vector<protocol::GroundItemRecord>& records) { m_ground.setList(records); }

    namespace {
        constexpr int kBodyHeight = 90; // assumed height of a character's body, px
    }

    void WorldScene::onGroundItemAdded(int itemId, core::Point position, std::int32_t extra, int appearMode) {
        const int index = m_ground.addItem(itemId, position, extra);
        if (index != 0 && appearMode == 1) {
            flyFromPlayer(index);
        }
    }

    void WorldScene::onGroundGoldAdded(std::int32_t gold, core::Point position, bool dropped) {
        const int index = m_ground.addGold(gold, position);
        if (index != 0 && dropped) {
            flyFromPlayer(index);
        }
    }

    void WorldScene::flyFromPlayer(int groundIndex) {
        const auto found = m_ground.things().find(groundIndex);
        if (found == m_ground.things().end()) {
            return;
        }
        const core::Point feet = m_camera.toScreen(m_player.position);
        m_ground.setActive(groundIndex, false);
        m_flights.drop(
            groundIndex,
            found->second.flightPicture,
            {feet.x, feet.y - kBodyHeight / 2}, // the item leaves from the middle of the body
            m_camera.toScreen(found->second.position),
            m_nowMs
        );
    }

    void WorldScene::onGroundItemRemoved(int index, bool pickedUp) {
        const auto removed = m_ground.remove(index);
        if (removed && pickedUp) {
            const core::Point feet = m_camera.toScreen(m_player.position);
            m_flights.pickUp(
                removed->flightPicture,
                m_camera.toScreen(removed->position),
                {feet.x, feet.y - kBodyHeight / 5},
                m_nowMs
            ); // the item flies to a point a fifth of the body height above the feet
        }
    }

    void WorldScene::pickUp(int groundIndex) {
        constexpr int kNear = 170; // farthest distance on each axis from which an item can be picked up, px
        const auto found = m_ground.things().find(groundIndex);
        if (found == m_ground.things().end()) {
            return;
        }
        const GroundItems::Item& item = found->second;
        if (std::abs(item.position.x - m_player.position.x) > kNear || std::abs(item.position.y - m_player.position.y) > kNear) {
            m_messages.show("too far away", 1500); // message display time in ms
            return;
        }
        if (!item.gold && m_inventory.placeFor(item.itemId, 1) == 0) {
            m_messages.show("Unable to lay items any more.", 1200); // message display time in ms
            return;
        }
        m_services.connection.send(protocol::pickUpRequest(groundIndex));
    }

    void WorldScene::onItemObtained(const protocol::ItemInstance& item) {
        protocol::ItemInstance rest = item;
        for (int tries = 0; rest.count > 0 && tries < Inventory::kBagSlots; ++tries) {
            int slot = 0;
            if (m_inventory.isStackable(item.itemId)) {
                for (int at = 1; at <= Inventory::kBagSlots && slot == 0; ++at) {
                    const protocol::ItemInstance& there = m_inventory.bagItem(at);
                    if (there.itemId == item.itemId && there.count < Inventory::kMaxStack) {
                        slot = at;
                    }
                }
            }
            if (slot == 0) {
                slot = m_inventory.freeSlotFor(item.itemId);
            }
            if (slot == 0) {
                m_messages.show("Unable to increase, your items have reached the upper limit.", 2000); // message display time in ms
                break;
            }
            const int added = m_inventory.addToBag(slot, rest);
            if (added == 0) {
                break;
            }
            rest.count -= added;
        }
        m_remote.bagFilled();
        inventoryChanged();
        const auto record = m_services.items.find(item.itemId);
        if (!record) {
            return;
        }
        constexpr int kRoleAbilityType = 20; // item type 20: food and ability items (Angel's Feather, Banana Boat, Black Tea, ...)
        constexpr int kRoleGearSubType = 149; // item sub type 149: role gear (no name table for sub types)
        constexpr int kVoucherSubType = 146; // item sub type 146: vouchers
        constexpr int kRiceBallSubType = 66; // item sub type 66: rice balls
        const bool exempt = record->type == kRoleAbilityType || record->subType == kRoleGearSubType || record->subType == kVoucherSubType ||
                            record->subType == kRiceBallSubType;
        if (record->petNpcId != 0 && !exempt && m_pets.placeOf(record->petNpcId) != 0) {
            return;
        }
        switch (m_activeSport) {
            case 6: // sport 6: fortune egg game
            case 7: // sport 7: claw machine
            case 8: // sport 8: slot machine
            case 9: // sport 9: boxing game
            case 10: // sport 10: slot machine
            case 11: // sport 11: jigsaw game
            case 12: // sport 12: jigsaw game
            case 14: // sport 14: jigsaw game
            case 19: // sport 19: slot machine
            case 20: // sport 20: dig hole game
            case 21: // sport ids whose games show their own messages: no obtain line here
                return;
            default: // any other sport (or none): show the obtain line
                break;
        }
        std::string name = record->name;
        if (record->equipSlot >= 1 && record->equipSlot <= 5 && item.fusionLevel > 0) { // equip slots 1..5 show their fusion level as +N
            name += "+" + std::to_string(item.fusionLevel);
        }
        m_centreMessages.show("Obtain " + std::to_string(item.count) + " Quantity " + name, 1300); // message display time in ms
        m_services.playSound("sound/wav0152.wav");
    }

    void WorldScene::onWarehouseItem(const protocol::BagItemRecord& record) { m_warehouse.add(record); }

    void WorldScene::onWarehouseSlotCleared(int slot) { m_warehouse.clear(slot); }

    void WorldScene::onWarehouseOpened() {
        m_warehouse.show([this] { m_eventRunner.moveOn(); });
    }

    void WorldScene::onSaleStoreItem(const protocol::BagItemRecord& record) {
        m_saleStore.add(record, m_inventory.isStackable(record.item.itemId));
        m_saleMenu.refresh();
    }

    void WorldScene::onSaleStoreCleared(int slot) {
        m_saleStore.clear(slot);
        m_saleMenu.refresh();
    }

    void WorldScene::onSaleNetList(const protocol::SaleNetList& list) { m_saleNet.show(list); }

    void WorldScene::onSaleMenuOpened() {
        m_saleMenu.show([this] {
            m_saleNet.hide();
            m_eventRunner.moveOn();
        });
    }

    void WorldScene::onSaleRequestDone() { m_saleRequestPending = 0; }

    void WorldScene::onSaleWebError(int code) {
        if (code == 1) {
            m_messages.show("Unable to connect to the website.", 2000); // message display time in ms
        }
    }

    void WorldScene::onSaleGoldChanged(std::int32_t amount, bool added) {
        constexpr std::int64_t kMostGold = 2000000000; // largest gold amount in the sale window (2 billion)
        const std::int64_t sum = static_cast<std::int64_t>(m_saleGold) + (added ? amount : -static_cast<std::int64_t>(amount));
        if (sum >= 0 && sum <= kMostGold) {
            m_saleGold = static_cast<std::int32_t>(sum);
        }
    }

    void WorldScene::onStoredPets(const std::vector<protocol::PetRecord>& pets) { m_petStorage.setStored(pets); }

    void WorldScene::onPetStorageOpened() {
        m_petStorage.show([this] { m_eventRunner.moveOn(); });
    }

    void WorldScene::onPetStored(int place, int petSlot) { m_petStorage.stored(place, petSlot); }

    void WorldScene::onStoredPetTaken(int place, int petSlot) {
        m_petStorage.taken(place, petSlot);
        m_groupWindow.refresh();
    }

    void WorldScene::onInnPetSwapped(int petSlot, int place) {
        m_petStorage.swapped(petSlot, place);
        m_groupWindow.refresh();
    }

    void WorldScene::onStoredPetCleared(int place) { m_petStorage.cleared(place); }

    void WorldScene::onPetStorageDone() { m_petStorage.done(); }

    void WorldScene::onPetStorageRefused(bool storerHasIt) {
        m_messages.show(storerHasIt ? "The Pet Storer has the same pet." : "The same pet", 2000); // message display time in ms
    }

    void WorldScene::onInnPrice(std::int32_t price) {
        TalkDialog::Timing timing;
        timing.wrapChars = 32; // wrap the text at 32 characters
        timing.exclusive = false;
        if (price == -1) {
            timing.modal = true;
            m_talk.say("You don't need to rest in the hotel", std::nullopt, [this] { declineInnRest(); }, std::move(timing));
            return;
        }
        if (price < 0) {
            return;
        }
        m_innPrice = price;
        char cost[16]; // room for a price printed in 6 columns
        std::snprintf(cost, sizeof cost, "%6d", static_cast<int>(price));
        const std::string text = price == 0 ? std::string("You can have a free hotel room to rest ") : std::string("You need to rest.") + cost + " Gold";
        m_talk.ask(text + ", Want to rest? ", std::nullopt, [this] { acceptInnRest(); }, [this] { declineInnRest(); }, std::move(timing));
    }

    void WorldScene::acceptInnRest() {
        if (m_status.gold < m_innPrice) {
            m_messages.show("Not enough money", 2000); // message display time in ms
            declineInnRest();
            return;
        }
        m_services.connection.send(protocol::innRestRequest());
    }

    void WorldScene::declineInnRest() { m_eventRunner.moveOn(); }

    void WorldScene::onInnAnswer(int answer) {
        if (answer == 1) { // inn answer 1: not enough money
            m_innAnswer.show("You don't have enough money.", [this] { m_eventRunner.moveOn(); });
        } else if (answer == 2) { // inn answer 2: the event goes on
            m_eventRunner.moveOn();
        }
    }

    void WorldScene::onBattleStart(const protocol::BattleStart& start) {
        if (!m_scenes || !m_map) {
            return;
        }
        const auto own = std::find_if(start.fighters.begin(), start.fighters.end(), [this](const protocol::FighterRecord& record) {
            return record.kind == 2 && record.id == m_player.id; // fighter kind 2: a player
        });
        // fighter state 100 becomes fight state 5 (waiting); no own fighter means fight state 1 (fighting)
        m_fightState = own != start.fighters.end() ? (own->state == 100 ? 5 : own->state) : 1;
        m_gps.hide();
        if (m_starSky.active()) {
            m_starSky.battleStarted();
            m_roleIntro.close();
            setHudShown(true);
        }
        m_mouseHeld = false;
        m_battleShown = false;
        m_battle.start(start, m_player.id, *m_scenes, m_map->file().tint());
        saveExpBeforeFight();
        constexpr int kArena = 10991; // map 10991: the arena (name not in the tables)
        int music = 0;
        if (start.eventBattle != 0) {
            if (const auto scene = m_events ? m_events->scene(mapFileOf(m_map->id())) : std::nullopt) {
                const auto found = scene->battleMusic.find(start.eventBattle);
                music = found != scene->battleMusic.end() ? found->second : 0;
            }
        }
        const bool arenaMusic =
            music == 2 || music == 3 || (music != 1 && m_map->id() == kArena); // battle music ids 2 and 3 are the arena theme, 1 the normal one
        m_services.music.playLooped(m_services.game.data(arenaMusic ? "Sound/BGM0003.wav" : "Sound/BGM0014.wav"));
        m_dimTarget = kDarkStep;
        m_dimStep = std::max(m_dimStep, 1);
    }

    void WorldScene::showBattle() {
        m_battleShown = true;
        m_dimStep = 0;
        m_dimTarget = 0;
        m_miniMap.setHidden(true);
        if (m_fightState != 4) { // fight state 4: watching a battle
            m_functionButtons.hide();
            m_mainStatus.slideOut(m_nowMs, [this] { m_battleMain.slideIn(m_nowMs); });
        }
    }

    void WorldScene::adviseRecovery() {
        constexpr int kSpecialSlot = 6; // equip slot 6: the special slot (also where the radio set goes)
        constexpr int kPetKeeperKind = 28; // item sub type 28: pet keeper
        const int special = m_inventory.equipped(kSpecialSlot).itemId;
        if (special != 0) {
            const auto item = m_services.items.find(special);
            if (!item || item->subType != kPetKeeperKind) {
                return;
            }
        }
        if (m_status.hp <= 1) {
            m_chat.add(0, "(System Prompt):Press Alt+1 to recover HP and SP gradually");
        }
    }

    void WorldScene::leaveBattle() {
        const bool fought = m_fightState == 1;
        m_fightState = 0;
        m_battle.end();
        adviseRecovery();
        m_battleShown = false;
        m_battleClosing = true;
        m_dimStep = kDarkStep;
        m_dimTarget = 0;
        m_miniMap.setHidden(false);
        m_functionButtons.show();
        m_battleMain.hide();
        m_mainStatus.slideIn(m_nowMs);
        m_view.setCursor(ui::CursorKind::Normal);
        playMapMusic();
        if (fought && m_eventRunner.isRunning()) {
            m_eventRunner.moveOn();
        }
    }

    SkillUser WorldScene::skillUserOf(const Fighter& fighter) const {
        constexpr int kWeaponSlot = 3; // equip slot 3: weapon
        constexpr int kClothesSlot = 2; // equip slot 2: clothes
        SkillUser user;
        user.kind = fighter.kind;
        user.sp = fighter.sp;
        if (fighter.kind == 4) { // fighter kind 4: a pet
            const int place = m_pets.activePlace();
            const auto& pet = place != 0 ? m_pets.at(place) : std::nullopt;
            if (pet) {
                user.npc = pet->info;
                user.petSkillLevels = pet->skillLevels;
                for (const int itemId : pet->equipmentIds()) {
                    const auto item = m_services.items.find(itemId);
                    if (item && item->equipSlot == kWeaponSlot) {
                        user.weaponId = itemId;
                        user.weapon = item;
                    }
                }
            }
            return user;
        }
        user.bodyType = m_player.look.bodyType;
        user.head = m_player.look.head;
        user.weaponId = m_inventory.equipped(kWeaponSlot).itemId;
        user.weapon = m_services.items.find(user.weaponId);
        user.clothesId = m_inventory.equipped(kClothesSlot).itemId;
        user.skillLevel = [this](int key) {
            const auto found = m_status.skills.find(key);
            return found != m_status.skills.end() ? found->second.level : 0;
        };
        return user;
    }

    std::vector<std::int32_t> WorldScene::battlePanelIds() const {
        std::vector<std::int32_t> ids;
        if (!m_battle.isOn()) {
            return ids;
        }
        for (const int slot : m_battle.members()) {
            const auto fighter = m_battle.field().fighter(slot);
            ids.push_back(fighter ? fighter->get().id : 0);
        }
        return ids;
    }

    void WorldScene::saveExpBeforeFight() {
        m_expBefore[0] = m_status.exp;
        for (int place = 1; place <= PetTeam::kPlaces; ++place) {
            const auto& pet = m_pets.at(place);
            m_expBefore[static_cast<std::size_t>(place)] = pet ? pet->exp : -1;
            m_amityBefore[static_cast<std::size_t>(place)] = pet ? pet->amity : -1;
        }
    }

    void WorldScene::showFightExpResult() {
        const auto expLine = [](const std::string& name, std::int64_t gained) {
            return gained >= 0 ? name + " obtain Exp:" + std::to_string(gained) : name + " Exp loss " + std::to_string(-gained);
        };
        const auto add = [](std::string& message, const std::string& line) { message += (message.empty() ? "" : "\r") + line; };
        std::string message;
        if (const std::int64_t gained = m_status.exp - m_expBefore[0]; gained != 0) {
            message = expLine(m_player.name, gained);
            if (gained < 0) {
                m_remote.playerDied();
            }
        }
        const int newLevel = levelFromExp(m_services.formula, m_status.exp, m_status.reborn);
        const bool levelledUp = newLevel > levelFromExp(m_services.formula, m_expBefore[0], m_status.reborn);
        if (levelledUp) {
            message += "\r" + m_player.name + " level upgrade to " + std::to_string(newLevel) + " level";
            m_status.level = newLevel;
            m_player.floatText.show("LevelUP", m_nowMs);
        }
        for (int place = 1; place <= PetTeam::kPlaces; ++place) {
            auto& pet = m_pets.at(place);
            const std::int64_t expBefore = m_expBefore[static_cast<std::size_t>(place)];
            if (!pet || expBefore == -1) {
                continue;
            }
            if (const std::int64_t gained = pet->exp - expBefore; gained != 0) {
                add(message, expLine(pet->name, gained));
            }
            if (const int lost = m_amityBefore[static_cast<std::size_t>(place)] - pet->amity; lost > 0) {
                add(message, pet->name + " Amity decreases " + std::to_string(lost));
                m_remote.petDied();
            }
            const int petLevel = levelFromExp(m_services.formula, pet->exp, pet->reborn);
            if (petLevel > levelFromExp(m_services.formula, expBefore, pet->reborn)) {
                message += "\r" + pet->name + " level upgrade to " + std::to_string(petLevel) + " level";
                pet->level = petLevel;
                showPetFloatText(place, "LevelUp");
            }
        }
        if (levelledUp) {
            m_services.playSound("sound/wav0153.wav");
        }
        if (!message.empty()) {
            constexpr int kResultMs = 6000; // message display time in ms
            m_messages.show(message, kResultMs);
        }
        refreshHud();
    }

    void WorldScene::showPetFloatText(int place, const std::string& text) {
        if (m_pets.activePlace() != place) {
            return;
        }
        for (MapPet& pet : m_mapPets.pets()) {
            if (pet.ownerId == m_player.id) {
                pet.body.floatText.show(text, m_nowMs);
            }
        }
    }

    void WorldScene::onPlayerLevelUp(std::int32_t playerId) {
        if (const auto found = m_others.find(playerId); found != m_others.end()) {
            found->second.floatText.show("LevelUP", m_nowMs);
        } else if (static_cast<std::uint32_t>(protocol::stripServerOffset(playerId)) == m_services.session.loginId) {
            m_services.playSound("sound/wav0153.wav");
            m_player.floatText.show("LevelUP", m_nowMs);
        }
    }

    void WorldScene::onRecentBattleSkills(const std::array<int, 3>& skills) { m_battleSkills.setRecent(skills); } // the 3 most recent battle skills

    void WorldScene::aimHotKey(int kind, int id) {
        setMouseMode(0);
        m_hotKeyAim = HotKeySlot{kind, id};
        m_view.setCursor(ui::CursorKind::Magic);
    }

    void WorldScene::endHotKeyAim() {
        m_hotKeyAim.reset();
        m_view.setCursor(ui::CursorKind::Normal);
    }

    bool WorldScene::ownPlayerAt(core::Point screenPoint) { return ownFigureArea().contains(screenPoint); }

    core::Rect WorldScene::ownFigureArea() {
        core::Point feet = m_camera.toScreen(m_player.position);
        int slot = m_player.actionSlot();
        if (m_player.vehicle) {
            slot = VehicleKinds::ridePoseSlot(m_player.vehicle->kind) + m_player.direction;
            const core::Point shift = VehicleSeats::offset(m_player.look.bodyType, slot, m_player.vehicle->seat, m_player.vehicle->kind);
            feet = {feet.x + shift.x, feet.y + shift.y};
        }
        return m_services.characters.figureFrameArea(m_player.look, feet, slot);
    }

    std::optional<int> WorldScene::ownPetAt(core::Point screenPoint) {
        for (const MapPet& pet : m_mapPets.pets()) {
            if (pet.ownerId != m_player.id || !pet.visible) {
                continue;
            }
            const int slot = m_services.characters.npcSlot(pet.info.spriteId, pet.info.directionKind, pet.body.actionSlot());
            const core::Size size = m_services.characters.npcFrameSize(pet.info.spriteId, slot);
            const core::Point feet = m_camera.toScreen(pet.body.position);
            if (core::Rect{feet.x - size.width / 2, feet.y - size.height + 32, size.width, size.height}.contains(
                    screenPoint
                )) { // 32: the figure's box ends 32 pixels below the feet
                const int place = m_pets.placeOf(pet.info.npcId);
                return place != 0 ? std::optional<int>(place) : std::nullopt;
            }
        }
        return std::nullopt;
    }

    void WorldScene::useHotKeyOn(std::optional<int> petPlace) {
        const HotKeySlot held = *m_hotKeyAim;
        endHotKeyAim();
        if (m_fightState != 0) {
            return;
        }
        const auto pet = petPlace ? m_pets.at(*petPlace) : std::optional<Pet>{};
        if (petPlace && !pet) {
            return;
        }
        const std::int32_t hp = pet ? pet->hp : m_status.hp;
        const std::int32_t maxHp = pet ? pet->maxHp : m_status.maxHp;
        const int sp = pet ? pet->sp : m_status.sp;
        const int maxSp = pet ? pet->maxSp : m_status.maxSp;
        constexpr int kItem = 1;
        if (held.kind == kItem) {
            const auto item = m_services.items.find(held.id);
            if (!item || m_inventory.nextSlotWith(held.id) == 0) {
                return;
            }
            bool needed = false;
            for (const int kind : item->attributeKinds) {
                constexpr int kHp = 25; // item attribute kind 25: HP
                constexpr int kSp = 26; // item attribute kind 26: SP
                constexpr int kAmity = 64; // item attribute kind 64: pet amity
                constexpr int kAnyTime = 36; // item attribute kind 36: experience (always counts as needed)
                if (kind == kHp) {
                    needed = hp < maxHp;
                } else if (kind == kSp) {
                    needed = sp < maxSp;
                } else if (kind == kAmity) {
                    if (!pet) {
                        m_messages.show("This item cannot be used by the player", 1500); // message display time in ms
                        return;
                    }
                    constexpr int kMostAmity = 100; // pet amity is at most 100
                    needed = pet->amity < kMostAmity;
                } else if (kind == kAnyTime) {
                    needed = true;
                }
                if (needed) {
                    break;
                }
            }
            if (!needed) {
                m_messages.show("Don't need it now", 1500); // message display time in ms
                return;
            }
        } else {
            const auto skill = m_services.skills.find(held.id);
            const int type = skill ? skill->skillType : 0;
            // skill types 6 and 14 cost SP and need a pet target; 6: SP recovery, 7: HP recovery, 14: both
            const bool needsSp = type == 6 || type == 14;
            if (needsSp && !pet) {
                m_messages.show("Unable to use the spell to yourself", 1200); // message display time in ms
                return;
            }
            // skill type 6: SP recovery, 7: HP recovery, 14: both
            const bool healed = type == 6 ? sp >= maxSp : type == 7 ? hp >= maxHp : type == 14 ? hp >= maxHp && sp >= maxSp : false;
            if (healed) {
                m_messages.show("Healing is not needed now", 1200); // message display time in ms
                return;
            }
        }
        constexpr int kPlayerTarget = 2; // hot key target kind 2: a player (fighter kind 2)
        constexpr int kPetTarget = 4; // hot key target kind 4: a pet (fighter kind 4)
        m_services.connection.send(
            protocol::hotKeyUseRequest(held.kind, held.id, 0, pet ? kPetTarget : kPlayerTarget, petPlace.value_or(0), pet ? pet->info.npcId : 0)
        );
    }

    void WorldScene::onImPoints(std::int32_t points) {
        if (m_mallShop.paymentPending) {
            m_mallShop.points = points;
            m_cart.pointsArrived();
            return;
        }
        m_mallShop.points = points;
        m_pointStore.pointsArrived(points);
        if (const auto scene = sportScene(m_activeSport)) {
            scene->get().pointsArrived(points);
        }
    }

    void WorldScene::onMallCatalog(const std::vector<protocol::MallRow>& rows) { m_mallShop.load(rows); }

    void WorldScene::onMallCatalogUpdate(const std::vector<protocol::MallRow>& rows) {
        const bool rearranged = m_mallShop.update(rows);
        m_mall.showFirstTab();
        m_messages.show("Item list has been updated", 1500); // message display time in ms
        if (rearranged) {
            m_messages.show("The cart has been rearranged", 1500); // message display time in ms
        }
    }

    void WorldScene::onMallPointsSpent(std::int32_t points, std::int32_t spent, int itemId, int count) {
        m_mallShop.points = points;
        if (spent == 0) {
            return;
        }
        const auto item = m_services.items.find(itemId);
        const std::string name = item ? item->name : std::string();
        const std::string counted = count > 0 ? " " + std::to_string(count) + ", " : std::string();
        m_services.playSound("sound/wav0152.wav");
        m_chat.add(10, "Buy " + name + counted + " Points deducted successfully:" + std::to_string(spent), m_player.id); // chat channel 10: notice line
        m_chat.add(10, "Remaining points:" + std::to_string(points), m_player.id); // chat channel 10: notice line
    }

    void WorldScene::onMallBonusPoints(std::int32_t points) {
        if (!m_inWorld) {
            return;
        }
        m_chat.add(10, "Remaining bonus:" + std::to_string(points), m_player.id); // chat channel 10: notice line
        if (points >= 50000) { // warn from 50000 bonus points; the account keeps at most 60000
            m_chat.add(10, "Please pay attention to the bonus, the max bonus is: 60000", 0); // chat channel 10: notice line
        }
    }

    void WorldScene::onMallBonusSpent(std::int32_t points, std::int32_t spent, int itemId, int count) {
        if (spent == 0) {
            return;
        }
        const auto item = m_services.items.find(itemId);
        const std::string counted = count > 0 ? std::to_string(count) + "Quantity" : std::string();
        m_services.playSound("sound/wav0152.wav");
        m_chat.add(
            10,
            "Buy " + (item ? item->name : std::string()) + counted + " Succeed, bonus deducted:" + std::to_string(spent),
            m_player.id
        ); // chat channel 10: notice line
        if (FurnitureKinds::checkHomeDiscard(m_services.items, itemId) == FurnitureKinds::Discard::Placeable) {
            m_chat.add(10, "Furniture has been stored in the tent's pick-up area", m_player.id); // chat channel 10: notice line
        }
        m_chat.add(10, "Remaining bonus:" + std::to_string(points), m_player.id); // chat channel 10: notice line
    }

    void WorldScene::onMallBought(
        int itemId,
        int category,
        int count,
        int key,
        bool ok
    ) {
        if (itemId == 0 || count == 0 || key == 0) {
            return;
        }
        if (!ok) {
            const auto item = m_services.items.find(itemId);
            m_messages.show("Purchasing [" + (item ? item->name : std::string()) + "] failed", 1500); // message display time in ms
            return;
        }
        m_mallShop.bought(itemId, category, count, key);
    }

    void WorldScene::onMallRefused(int reason) {
        if (reason == 1) { // shop refusal 1: the server is busy
            m_messages.show("The server is busy", 1500); // message display time in ms
        } else if (reason == 2) { // shop refusal 2: the setting forbids buying
            m_messages.show("Can't buy due to the setting", 1500); // message display time in ms
        }
        if (reason >= 1 && reason <= 3) { // shop refusals 1..3 end the pending payment
            m_mallShop.paymentPending = false;
        }
    }

    void WorldScene::onMallGamesTab(bool open) { m_mall.setGamesTab(open); }

    void WorldScene::onCastleNotice(const std::string& text, bool inChat) {
        if (inChat) {
            m_chat.add(0, text, m_player.id);
        } else {
            m_messages.show(text, 3000); // message display time in ms
        }
    }

    void WorldScene::onCastleOwner(const std::string& guild) { m_castleOwner = guild; }

    void WorldScene::onGoldEaglePrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) {
        if (replace) {
            m_goldEagle.list(castle, entries);
        } else {
            m_goldEagle.update(castle, entries);
        }
    }

    void WorldScene::onGoldEaglePrefixRemoved(int castle, int slot) { m_goldEagle.remove(castle, slot); }

    void WorldScene::onDefenseNpcPrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) {
        if (replace) {
            m_defenseNpc.list(castle, entries);
        } else {
            m_defenseNpc.update(castle, entries);
        }
    }

    void WorldScene::onDefenseNpcPrefixRemoved(int castle, int slot) { m_defenseNpc.remove(castle, slot); }

    void WorldScene::onStoneEaglePrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) {
        if (replace) {
            m_stoneEagle.list(castle, entries);
        } else {
            m_stoneEagle.update(castle, entries);
        }
    }

    void WorldScene::onStoneEaglePrefixRemoved(int castle, int slot) { m_stoneEagle.remove(castle, slot); }

    void WorldScene::onMallForgeResult(int outcome, int bagSlot, int fusionLevel) { m_mall.showForgeResult(outcome, bagSlot, fusionLevel); }

    void WorldScene::onHotKeys(const std::vector<protocol::HotKeyRecord>& records) { m_hotKeys.load(records); }

    void WorldScene::onMarkCount(int markId, int change) {
        if (change >= 0) {
            m_marks.add(markId, change);
        } else {
            m_marks.subtract(markId, -change);
        }
    }

    void WorldScene::onMarkDeleted(int markId) { m_marks.remove(markId); }

    void WorldScene::onMarkFlag(int flagBit, int value) { m_marks.setFlag(flagBit, value == 1); }

    void WorldScene::onMarkSlot(int slot, int markId, int count) { m_marks.setSlot(slot, markId, count); }

    void WorldScene::onMarkFlagByte(int byteIndex, int bits) { m_marks.setFlagByte(byteIndex, bits); }

    void WorldScene::onQuestsFull() { m_messages.show("The quest volume is full.", 2000); } // message display time in ms

    void WorldScene::toggleQuests() {
        constexpr int kNotebook = 100; // item sub type 100: the quest notebook
        for (int slot = 1; slot <= Inventory::kColumns * Inventory::kRows; ++slot) {
            const auto item = m_services.items.find(m_inventory.bagItem(slot).itemId);
            if (item && item->subType == kNotebook) {
                m_quests.toggle();
                return;
            }
        }
        m_messages.show("Operation unavailable, you have no quest records", 1200); // message display time in ms
    }

    void WorldScene::onPlayerFightState(std::int32_t playerId, int state) {
        if (const auto found = m_others.find(playerId); found != m_others.end()) {
            found->second.fightState = state;
        }
    }

    void WorldScene::onFightersJoin(const std::vector<protocol::FighterRecord>& fighters) {
        constexpr int kWatching = 4; // fight state 4: watching a battle
        constexpr int kWaiting = 5; // fight state 5: waiting (own fighter state 100)
        m_battle.join(fighters, m_fightState == kWatching || m_fightState == kWaiting);
    }

    void WorldScene::onPlayerLeftBattle(std::int32_t playerId) {
        constexpr int kWatching = 4; // fight state 4: watching a battle
        if (playerId == m_player.id) {
            if (m_fightState == kWatching) {
                leaveBattle();
            }
            return;
        }
        m_battle.playerLeft(playerId, m_nowMs);
    }

    void WorldScene::onBattleType(int type) { m_battle.setType(type); }

    void WorldScene::onBattleTimeReset() { m_battle.resetTimeLimit(m_nowMs); }

    void WorldScene::onBattleTimeLimit() { m_battle.startTimeLimit(m_nowMs); }

    void WorldScene::onBattleRound() { m_battle.startRound(m_nowMs); }

    void WorldScene::onBattleActions(const std::vector<protocol::BattleAction>& actions) { m_battle.play(actions, m_nowMs); }

    void WorldScene::onBattleItemRound() { m_battle.playItems(m_nowMs); }

    void WorldScene::onFighterCast(const protocol::FighterCast& cast) { m_battle.castAttribute(cast); }

    void WorldScene::onFighterMarks(int count) { m_battle.marks(count); }

    void WorldScene::onFighterDamage(int kind, int row, int col, std::int32_t damage) { m_battle.setFighterDamage(kind, row, col, damage); }

    void WorldScene::onFighterAttackArea(int row, int col, int area) { m_battle.setAttackArea(row, col, area); }

    void WorldScene::onFighterAttribute(int row, int col, int attribute, std::int32_t value) { m_battle.setAttribute(row, col, attribute, value); }

    void WorldScene::onFighterWeaponLooks(const std::vector<protocol::FighterWeaponLook>& looks) { m_battle.setWeaponLooks(looks); }

    void WorldScene::onFighterLeaves(int row, int col) { m_battle.leave(row, col, m_nowMs); }

    void WorldScene::onFighterStatus(
        int row,
        int col,
        int kind,
        int skillId,
        int place
    ) { m_battle.setStatus(row, col, kind, skillId, place); }

    void WorldScene::onFighterKnockedOut(int row, int col) { m_battle.knockOut(row, col, m_nowMs); }

    void WorldScene::onFighterActed(int row, int col) { m_battle.acted(row, col); }

    void WorldScene::onInnSleep() {
        m_innSleep = InnSleep::FallingAsleep;
        m_dimTarget = kDarkStep;
        m_dimStep = std::max(m_dimStep, 1);
        m_services.music.playLooped(m_services.game.data("Sound/wav0149.wav"));
    }

    void WorldScene::paintInnSleep(gfx::Canvas& canvas, std::uint64_t nowMs) {
        constexpr int kBedItem = 39011; // item 39011: the inn bed (Big5 name: rattan bed, not opened)
        constexpr int kSleepSlot = 26; // action slot 26: lying asleep (stops on its last frame)
        constexpr std::uint64_t kZStepMs = 300; // time each phase of the sleeping "Z" animation lasts, ms
        constexpr int kLetters = 26; // letters (A..Z) stacked vertically in the Word_Red_64U picture
        const auto bed = m_services.items.find(kBedItem);
        if (!bed) {
            return;
        }
        const auto bedPicture = m_services.pictures.find(std::to_string(bed->iconId));
        core::Rect bedArea{0, 0, 32, 32}; // one 32x32 cell
        if (bedPicture && (bed->cellWidth != 1 || bed->cellHeight != 1)) {
            const gfx::Picture& picture = m_services.pictures.picture(*bedPicture);
            // cells are 32 pixels with a 2 pixel gap between them
            const int width = bed->cellWidth * 32 + (bed->cellWidth - 1) * 2;
            const int height = bed->cellHeight * 32 + (bed->cellHeight - 1) * 2;
            // the cells sit in the middle of the picture, which has a 2 pixel border
            bedArea = {std::max(0, (picture.width() - 2) / 2 - width / 2), std::max(0, (picture.height() - 2) / 2 - height / 2), width, height};
        }
        const int spanX = bedArea.width + 1;
        const int spanY = bedArea.height + 1;
        if (bedPicture) {
            canvas.drawPicture(*bedPicture, {(Camera::kScreenWidth - spanX) / 2, (Camera::kScreenHeight - spanY) / 2}, bedArea);
        }
        const core::Point feet{Camera::kScreenWidth / 2, Camera::kScreenHeight / 2 - 10}; // the feet are 10 pixels above the screen middle
        const int frames = m_services.characters.figureFrameCount(m_player.look, kSleepSlot);
        m_services.characters.drawFigure(canvas, m_player.look, feet, kSleepSlot, std::max(0, frames - 1));
        m_playerLight.setPosition(m_camera.toScreen(m_player.position));
        m_playerLight.draw(canvas, nowMs);
        const auto letters = m_services.pictures.find("Word_Red_64U");
        if (!letters) {
            return;
        }
        const gfx::Picture& font = m_services.pictures.picture(*letters);
        const int letterHeight = font.height() / kLetters;
        const core::Rect zSource{0, ('Z' - 'A') * letterHeight, font.width(), letterHeight};
        const int baseY = (Camera::kScreenHeight - spanY) / 2 - 40; // the Z starts 40 pixels above the bed's top

        /// @brief One "Z" of the sleep animation: its offset and picture scale.
        struct Z {
            core::Point at;
            double scale;
        };

        // the 3 growing Zs: pixel offset from the base point and picture scale
        static const Z kZs[] = {{{0, 30}, 0.2}, {{10, -5}, 0.4}, {{25, -60}, 0.7}};
        const int frame = static_cast<int>(nowMs / kZStepMs % 4); // 4 phases: nothing, then the 3 Zs
        if (frame == 0) {
            return;
        }
        const Z& z = kZs[frame - 1];
        canvas.drawPictureScaled(
            *letters,
            {feet.x + z.at.x, baseY + z.at.y, static_cast<int>(font.width() * z.scale), static_cast<int>(letterHeight * z.scale)},
            zSource
        );
    }

    void WorldScene::updateInnSleep(std::uint64_t nowMs) {
        constexpr std::uint64_t kSleepMs = 5000; // how long the player stays asleep at the inn, ms
        if (m_innSleep == InnSleep::FallingAsleep && m_dimStep == kDarkStep) {
            m_innSleep = InnSleep::Asleep;
            m_innWakeMs = nowMs + kSleepMs;
        } else if (m_innSleep == InnSleep::Asleep && nowMs >= m_innWakeMs) {
            m_innSleep = InnSleep::Awake;
            playMapMusic();
            m_dimTarget = 0;
            const auto leader = m_teams.leaderOf(m_player.id);
            if (m_eventRunner.isRunning() && (!leader || *leader == m_player.id)) {
                m_eventRunner.moveOn();
            }
        }
    }

    void WorldScene::onEventFinished() {
        for (const NpcFacings::Entry& entry : m_npcFacings.restoreAll()) {
            if (auto npc = findNpc(entry.npcIndex)) {
                npc->get().body.direction = entry.direction;
                npc->get().held = false;
            }
        }
        m_movie.director().serverMessage();
        m_eventRunner.finished();
        if (auto npc = findNpc(m_nearNpc)) {
            npc->get().nearEndedMs = m_nowMs;
        }
        m_nearNpc = 0;
        m_moveLocked = false;
    }

    void WorldScene::runWorldResult(const EventResult& result) {
        // pixel offsets of the 8 steps (10 pixels each); entry 0 is no move
        static const core::Point kSteps[] = {{0, 0}, {0, -10}, {10, -10}, {10, 0}, {10, 10}, {0, 10}, {-10, 10}, {-10, 0}, {-10, -10}};
        const auto stepFrom = [](core::Point from, int step) {
            return step >= 1 && step <= 8 ? core::Point{from.x + kSteps[step].x, from.y + kSteps[step].y} : from;
        };
        switch (result.kind) {
            case 4: // event result 4: an NPC walks one of its event walks
                if (auto npc = findNpc(result.target); npc && result.mode == 3) { // mode 3: event walk
                    const auto walk = npc->get().eventWalks.find(result.value);
                    if (walk == npc->get().eventWalks.end()) {
                        break;
                    }
                    constexpr double kEventPace = 10 / 8.0 * 0.16; // event walk speed in px per ms: the 0.16 walk speed times 10/8
                    std::vector<core::Point> points = walk->second;
                    if (result.option == 2) { // option 2: start at the walk's first point and walk the rest
                        npc->get().body.position = points.front();
                        points.erase(points.begin());
                    } else if (result.option != 1 && result.option != 3) { // option 3: walk, then hide on arrival
                        break;
                    }
                    npc->get().hideOnArrival = result.option == 3; // option 3: hide at the end of the walk
                    npc->get().eventWalking = true;
                    npc->get().held = false;
                    npc->get().body.pixelsPerMs = kEventPace;
                    if (!points.empty()) {
                        startWalking(npc->get().body, points, m_nowMs);
                    }
                }
                break;
            case 0: // event result 0: NPC show/hide, chase, frame, message box, join-team question
                if (result.mode == 8) { // mode 8: message box
                    eventMessageBox(result);
                } else if (result.option == 1) {
                    askNpcJoinTeam();
                } else if (result.option == 3 || result.option == 4) { // option 3: hide the NPC, 4: show it
                    const int seconds = std::clamp(static_cast<int>(result.value), 0, 65535); // 65535 seconds means for good
                    if (result.option == 3) { // option 3: hide
                        onNpcHidden(result.target, seconds);
                    } else {
                        onNpcRevealed(result.target, seconds);
                    }
                } else if (result.option == 2) { // option 2: chase the player
                    if (auto npc = findNpc(result.target)) {
                        m_chaseNpc = npc->get().index;
                        npc->get().held = false;
                        npc->get().body.pixelsPerMs = m_player.pixelsPerMs * 0.5 * npc->get().speed; // half the player's pace
                    }
                } else if (result.option == 5) { // option 5: set the NPC's frame
                    onNpcFrame(result.target, result.value);
                }
                break;
            case 11: // event result 11: turn an NPC (mode = NPC) towards the player, another NPC or a step
                if (auto npc = findNpc(result.mode); npc && npc->get().visible) {
                    npc->get().held = false;
                    m_npcFacings.restore(npc->get().index);
                    core::Point to = npc->get().body.position;
                    if (result.option == 1) {
                        to = m_player.position;
                    } else if (result.option == 2) { // option 2: towards another NPC
                        auto other = findNpc(result.value);
                        to = other ? other->get().body.position : to;
                    } else if (result.option == 3) { // option 3: towards a step from the NPC
                        to = stepFrom(npc->get().body.position, result.value);
                    }
                    npc->get().body.direction = directionTowards(npc->get().body.position, to, npc->get().body.direction);
                }
                break;
            case 14: // event result 14: turn the player towards an NPC (mode 1) or a step (mode 2)
                if (result.mode == 1) {
                    if (auto npc = findNpc(result.target)) {
                        m_player.direction = directionTowards(m_player.position, npc->get().body.position, m_player.direction);
                    }
                } else if (result.mode == 2) { // mode 2: towards a step from the player
                    m_player.direction = directionTowards(m_player.position, stepFrom(m_player.position, result.target), m_player.direction);
                }
                break;
            case 15: // event result 15: play BGM<value> (mode 1) or the map music (mode 2)
                if (result.mode == 1) {
                    char name[16]; // room for "BGM" and 4 digits
                    std::snprintf(name, sizeof name, "BGM%04d", static_cast<int>(result.value));
                    m_services.music.playLooped(m_services.game.data(std::string("Sound/") + name + ".wav"));
                } else if (result.mode == 2) { // mode 2: back to the map music
                    playMapMusic();
                }
                break;
            default: // other event result kinds are not handled here
                break;
        }
    }

    void WorldScene::askNpcJoinTeam() {
        auto npc = findNpc(m_eventRunner.eventNpc());
        if (!npc || m_pets.count() >= PetTeam::kPlaces || m_pets.placeOf(npc->get().info.npcId) != 0) {
            return;
        }
        m_npcTeam.ask(npc->get().info.name);
    }

    void WorldScene::eventMessageBox(const EventResult& result) {
        const auto info = m_services.npcs.find(result.target);
        const std::string name = info ? info->name : std::string();
        if (result.option == 1) {
            m_services.messageBox("The forces of darkness have grown...", "^__^" + name + " joined");
        } else if (result.option == 2) { // option 2: the NPC leaves the team (option 1 is joining)
            m_services.messageBox("The forces of darkness have weakened...", "=__=" + name + " left");
        }
    }

    void WorldScene::faceEventTarget(core::Point target, int npcIndex) {
        m_player.direction = directionTowards(m_player.position, target, m_player.direction);
        for (MapPet& pet : m_mapPets.pets()) {
            if (pet.ownerId == m_player.id) {
                pet.body.direction = directionTowards(pet.body.position, target, pet.body.direction);
            }
        }
        auto npc = findNpc(npcIndex);
        if (!npc || npc->get().body.action != 0) {
            return;
        }
        if (m_npcFacings.save(npc->get().index, npc->get().body.direction)) {
            npc->get().held = true;
            npc->get().body.walking = false;
        }
        if (npc->get().info.type != 6) { // NPC type 6 does not turn towards the player
            npc->get().body.direction = directionTowards(npc->get().body.position, m_player.position, npc->get().body.direction);
        }
    }

    std::optional<int> WorldScene::npcAt(core::Point screenPoint) {
        std::optional<int> found;
        int foundY = 0;
        for (const MapNpc& npc : m_npcs) {
            if (!npc.visible) {
                continue;
            }
            const int slot = m_services.characters.npcSlot(npc.info.spriteId, npc.info.directionKind, npc.body.actionSlot());
            if (m_services.characters.npcContains(npc.info, m_camera.toScreen(npc.body.position), slot, 0, screenPoint) &&
                (!found || npc.body.position.y > foundY)) {
                found = npc.index;
                foundY = npc.body.position.y;
            }
        }
        return found;
    }

    void WorldScene::clickNpc(int npcIndex) {
        auto npc = findNpc(npcIndex);
        if (!npc || m_inputLocked) {
            return;
        }
        constexpr int kNear = 170; // farthest distance on each axis from which an NPC can be clicked, px
        if (std::abs(npc->get().body.position.x - m_player.position.x) > kNear || std::abs(npc->get().body.position.y - m_player.position.y) > kNear) {
            m_messages.show("too far away", 1500); // message display time in ms
            return;
        }
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return;
        }
        if (m_player.walking || m_player.stall) {
            return;
        }
        if (m_teams.isMember(m_player.id)) {
            return;
        }
        if (m_eventRunner.npcClicked(npcIndex, npc->get().eventCount, npc->get().triggerHalfSize, !npc->get().body.walking)) {
            m_screen.consumeNextRelease();
            if (m_services.partyEventStarted) {
                m_services.partyEventStarted(PartyTrigger::NpcClick, npcIndex);
            }
        }
        startAutoCollect(npc->get());
    }

    bool WorldScene::mirrorEvent(PartyTrigger trigger, int index) {
        if (m_inputLocked) {
            return false;
        }
        if (trigger == PartyTrigger::Door) {
            const bool hasEvents = std::any_of(m_doors.begin(), m_doors.end(), [index](const formats::SceneDoor& door) {
                return door.index == index && !door.eventIds.empty();
            });
            if (!hasEvents || !m_eventRunner.startMirrored(trigger, index)) {
                return false;
            }
            m_currentDoor = index;
            return true;
        }
        auto npc = findNpc(index);
        if (!npc || npc->get().eventCount <= 0 || !m_eventRunner.startMirrored(trigger, index)) {
            return false;
        }
        if (trigger == PartyTrigger::NpcNear) {
            m_nearNpc = index;
        }
        return true;
    }

    void WorldScene::onMapSign(int kind, int index, int sign) {
        if (kind == 1) {
            if (auto npc = findNpc(index)) {
                npc->get().mapSign = sign;
            }
        } else if (kind == 2) { // sign kind 2: a door of the mini map (kind 1 is an NPC)
            m_miniMap.setDoorSign(index, sign);
        }
    }

    void WorldScene::onNpcFrame(int npcIndex, int frame) {
        if (auto npc = findNpc(npcIndex)) {
            const int old = npc->get().holdFrame;
            npc->get().holdFrame = frame;
            if (m_inWorld && npc->get().info.stateSoundNo > 0 && old == 0 && frame == 1) {
                std::string number = std::to_string(npc->get().info.stateSoundNo);
                number.insert(0, number.size() < 4 ? 4 - number.size() : 0, '0'); // sound numbers are padded to 4 digits
                m_services.playSound("sound/wav" + number + ".wav");
            }
        }
    }

    void WorldScene::onNpcState(
        int npcIndex,
        int holdFrame,
        core::Point at,
        int showState,
        std::uint32_t hideMs,
        int action
    ) {
        auto npc = findNpc(npcIndex);
        if (!npc) {
            return;
        }
        if (npc->get().isSceneObject()) {
            npc->get().holdFrame = holdFrame;
        } else if (action > 0) {
            onNpcAction(npcIndex, action, holdFrame, false);
        }
        onNpcMoves(npcIndex, at, false);
        if (showState == 1 || showState == 2) { // show state 1: shown for good, 2: hidden for good
            onNpcShowState(npcIndex, showState);
        }
        if (hideMs != 0) {
            onNpcHidden(npcIndex, static_cast<int>((hideMs + 500) / 1000)); // milliseconds rounded to seconds
        }
    }

    void WorldScene::onNpcShowState(int npcIndex, int state) {
        if (auto npc = findNpc(npcIndex)) {
            npc->get().showState = state;
            npc->get().showEndMs = 0;
            npc->get().visible = state != 2 && state != 4; // show state 2: hidden for good, 4: hidden for a time
        }
    }

    void WorldScene::onNpcHidden(int npcIndex, int seconds) {
        if (auto npc = findNpc(npcIndex)) {
            constexpr int kForGood = 65535; // seconds value 65535 means "for good"
            npc->get().showState = seconds == kForGood ? 2 : 4; // show state 2: hidden for good, 4: hidden for a time
            npc->get().showEndMs = m_nowMs + static_cast<std::uint64_t>(seconds) * 1000; // seconds to ms
            npc->get().visible = false;
        }
    }

    void WorldScene::onNpcRevealed(int npcIndex, int seconds) {
        if (auto npc = findNpc(npcIndex)) {
            constexpr int kForGood = 65535; // seconds value 65535 means "for good"
            npc->get().showState = seconds == kForGood ? 1 : 3; // show state 1: shown for good, 3: shown for a time
            npc->get().showEndMs = m_nowMs + static_cast<std::uint64_t>(seconds) * 1000; // seconds to ms
            npc->get().visible = true;
        }
    }

    void WorldScene::updateNpcVisibility(MapNpc& npc, std::uint64_t nowMs) {
        if (npc.showState == 4) { // show state 4: hidden for a time
            if (nowMs < npc.showEndMs) {
                return;
            }
            npc.showState = 1;
        }
        if (npc.showState == 3 && nowMs >= npc.showEndMs) { // show state 3: shown for a time, then hidden for good (2)
            npc.showState = 2; // show state 2: hidden for good
        }
        if (shouldHideMapNpc(m_map->id(), npc.info.npcId, m_fleeKillActive)) {
            npc.showState = 2; // show state 2: hidden for good
        }
        if (shouldShowMapNpc(m_map->id(), npc.info.npcId, m_fleeKillActive)) {
            npc.showState = 1;
        }
        npc.visible = npc.showState != 2 && npc.showState != 4; // show state 2: hidden for good, 4: hidden for a time
    }

    void WorldScene::checkChase() {
        auto npc = findNpc(m_chaseNpc);
        if (!npc) {
            m_chaseNpc = 0;
            return;
        }
        if (npc->get().body.position == npc->get().home) {
            npc->get().body.direction = npc->get().homeDirection;
            npc->get().body.pixelsPerMs = npc->get().ownPace;
            m_chaseNpc = 0;
        } else if (!npc->get().body.walking) {
            startWalking(npc->get().body, findPath(*m_map, npc->get().body.position, npc->get().home), m_nowMs);
        }
    }

    void WorldScene::finishEntering() {
        if (!m_entering) {
            return;
        }
        m_entering = false;
        m_services.session.onPvpServer = m_services.session.relogin && m_services.session.relogin->toPvp;
        m_services.session.relogin.reset();
        m_backdrop.setVisible(false);
        m_fadeStep = kFadeSteps;
        m_miniMap.showMap(*m_map, sceneType());
        m_mainStatus.show();
        m_inputBar.show();
        m_hotKeys.show();
        m_chat.show();
        m_mainButtons.show();
        m_functionButtons.show();
        refreshHud();
    }

    void WorldScene::refreshHud() {
        m_mainStatus.refresh();
        m_functionButtons.refresh();
        m_itemWindow.refresh();
    }

    bool WorldScene::onKey(const ui::KeyEvent& key) {
        if (m_movie.active()) {
            return true;
        }
        if (key.ctrl && (key.key == ui::Key::I || key.key == ui::Key::S || key.key == ui::Key::T || key.key == ui::Key::E)) {
            toggleWindow(key.key == ui::Key::I ? 'i' : key.key == ui::Key::S ? 's' : key.key == ui::Key::T ? 'g' : 'e');
            return true;
        }
        if (key.ctrl && key.key == ui::Key::G) {
            toggleWindow('m');
            return true;
        }
        if (key.ctrl && key.key == ui::Key::M) {
            toggleQuests();
            return true;
        }
        if (key.key == ui::Key::Delete && !m_screen.isTyping()) {
            m_chat.clear();
            return true;
        }
        if (m_activeSport == m_scotd.kind() && !key.ctrl && !key.alt) {
            switch (key.key) {
                case ui::Key::Left: m_scotd.keyDown(ScotdGame::Key::Left); return true; // Left arrow: move left in the Scotd game
                case ui::Key::Right: m_scotd.keyDown(ScotdGame::Key::Right); return true; // Right arrow: move right
                case ui::Key::Up: m_scotd.keyDown(ScotdGame::Key::Up); return true; // Up arrow: move up
                case ui::Key::Down: m_scotd.keyDown(ScotdGame::Key::Down); return true; // Down arrow: move down
                case ui::Key::Q: m_scotd.keyDown(ScotdGame::Key::Quit); return true; // Q: quit the game
                case ui::Key::Space: m_scotd.keyDown(ScotdGame::Key::Space); return true; // Space: the game's action key
                default: break; // other keys fall through to the normal handling
            }
        }
        if (!key.alt && key.key >= ui::Key::F1 && key.key <= ui::Key::F8) {
            m_hotKeys.use(static_cast<int>(key.key) - static_cast<int>(ui::Key::F1) + 1);
            return true;
        }
        const int digit = key.key >= ui::Key::Digit1 && key.key <= ui::Key::Digit8 ? static_cast<int>(key.key) - static_cast<int>(ui::Key::Digit1) + 1 : 0;
        if (digit != 0 && key.ctrl && digit <= EmoteHotkeys::kPhrases) {
            m_socialWindow.usePhrase(digit);
            return true;
        }
        if (digit != 0 && key.alt) {
            m_socialWindow.useSlot(digit);
            return true;
        }
        if (key.alt && key.key == ui::Key::P) {
            takePhoto();
            return true;
        }
        if (m_inWorld && !key.ctrl && !key.alt && m_piano.isOpen() && m_piano.keyDown(key)) {
            return true;
        }
        if (key.key == ui::Key::Escape && m_screen.closeTopOnEscape()) {
            m_services.playSound("sound/wav0154.wav");
            return true;
        }
        return false;
    }

    void WorldScene::takePhoto() {
        const std::filesystem::path folder = m_services.game.data("photo");
        std::error_code ignored;
        std::filesystem::create_directories(folder, ignored);
        const std::filesystem::path file = ScreenPhoto::chooseFile(folder, std::time(nullptr));
        PhotoCheck check;
        check.playerId = m_player.id;
        check.mapId = m_map ? m_map->id() : 0;
        check.position = m_player.position;
        for (const auto& [id, other] : m_others) {
            check.players.push_back({id, other.position});
        }
        m_services.saveScreenshot(file, check);
        m_messages.show(ScreenPhoto::savedMessage(file), 2000); // message display time in ms
    }

    void WorldScene::toggleWindow(char window) {
        switch (window) {
            case 'i': m_itemWindow.toggle(); break; // i: item window
            case 's': m_skillWindow.toggle(); break; // s: skill window
            case 'g': m_groupWindow.toggle(); break; // g: group (party) window
            case 'e': m_socialWindow.toggle(); break; // e: social window
            case 'c': m_compoundWindow.toggle(); break; // c: compound window
            case 'm': m_moneyMenu.toggle(); break; // m: money menu
            default: m_systemWindow.toggle(); break; // any other letter: system window
        }
    }

    void WorldScene::inventoryChanged() {
        syncOwnPetWeapon();
        m_itemWindow.refresh();
        m_compoundWindow.refresh();
        m_mall.refresh();
        m_itemTrade.refresh();
        m_utensilTrade.refresh();
        m_itemStall.refresh();
        m_npcSale.refresh();
        m_bankItems.refresh();
        m_saleMenu.refresh();
        m_guildWeaponBank.refresh();
    }

    void WorldScene::bagMarksChanged() { inventoryChanged(); }

    void WorldScene::showGameWindows(bool shown) {
        for (const char* layer :
             {"ItemWindowLayer",
              "SkillWindowLayer",
              "SettingsWindowLayer",
              "GroupWindowLayer",
              "GuildWindowLayer",
              "GuildRulesLayer",
              "GuildMyAuthorityLayer",
              "GuildAuthorityLayer",
              "GuildCreateLayer",
              "SocialWindowLayer",
              "CompoundWindowLayer",
              "MoneyMenuLayer",
              "ThrowMoneyLayer",
              "ItemTradeLayer",
              "UtensilTradeLayer",
              "PetTradeLayer",
              "ItemStallLayer",
              "StallGoodsLayer",
              "NpcStoreLayer",
              "NpcSaleLayer",
              "InnAnswerLayer",
              "BankItemLayer",
              "BankMoneyLayer",
              "WarehouseLayer",
              "SaleMenuLayer",
              "SaleNetLayer",
              "GuildWeaponBankLayer",
              "PetStorageLayer",
              "CountBoxLayer",
              "PetStallLayer",
              "StallPetsLayer",
              "UtensilStallLayer",
              "BuyUtensilLayer",
              "ChangePasswordLayer",
              "BlacklistLayer",
              "MsgSkinLayer",
              "SendMailLayer",
              "FurMailLayer",
              "StoreTankLayer",
              "CapsuleLayer",
              "ParkingLayer",
              "ShowPlatformLayer",
              "CarCaseLayer",
              "TransportLayer",
              "PotentialReturnLayer",
              "MailBoxLayer",
              "ReadMailLayer",
              "ReadFurMailLayer",
              "OnlineTipsLayer",
              "NoticeBoardLayer",
              "NoticeReadLayer",
              "NoticeSendLayer",
              "Top100Layer",
              "ActivitiesLayer",
              "ActivityInfoLayer",
              "LotteryLayer",
              "LotteryPrizesLayer",
              "MagicMapLayer",
              "MagnetLayer",
              "PackingLayer",
              "InsteadEcisLayer",
              "MemCardLayer",
              "ItemBagLayer",
              "RemoteControlLayer",
              "InstanceWindowLayer",
              "InstanceCreateLayer",
              "InstanceMessageLayer",
              "InstanceDetailLayer",
              "MarryApplicationLayer",
              "MarryDivorceLayer",
              "MarryCardLayer",
              "CareerLayer",
              "BeforeCareerLayer",
              "CampStyleLayer",
              "MakeThingLayer",
              "MakeThingInfoLayer",
              "StallPriceLayer",
              "FurnitureBuyLayer",
              "VendingLayer",
              "EggSellLayer",
              "EggBuyLayer",
              "StallCaseLayer",
              "CupboardLayer",
              "BedLayer",
              "PianoLayer",
              "DyeLayer",
              "ExpressLayer",
              "ForageLayer",
              "FishTankLayer",
              "FishBuyLayer",
              "FishAnnotateLayer"}) {
            m_screen.get<ui::Element>(layer).setVisible(shown);
        }
    }

    void WorldScene::askToLeave(bool exitGame) {
        m_dimTarget = kDarkStep;
        m_dimStep = std::max(m_dimStep, 1);
        m_talk.ask(
            exitGame ? "Are you sure you want to close the game?" : "Are you sure you want to quit?",
            TalkDialog::Speaker{m_player.look, m_player.name},
            [this, exitGame] { leave(exitGame); },
            [this] { m_dimTarget = 0; }
        );
    }

    void WorldScene::leave(bool exitGame) {
        m_systemWindow.close();
        m_mailBox.save();
        m_chat.settings()
            .save(SystemSettings::fileFor(m_services.game.userRoot(), m_services.session.loginId), m_services.session.loginId, m_inputBar.channel());
        m_services.music.stop();
        if (!exitGame) {
            m_services.connection.send(protocol::stopExpressRequest());
        }
        m_services.connection.send(protocol::stopCollectingRequest());
        m_services.connection.close();
        m_next = exitGame ? SceneKind::Quit : SceneKind::Login;
    }

    void WorldScene::applyChatColors(const SystemSettings& settings) {
        // chat channels in the order of the colour settings: 2 local, 3 whisper, 5 guild, 6 ally, 1 world
        constexpr std::array<int, 5> kChannels = {2, 3, 5, 6, 1};
        for (std::size_t i = 0; i < kChannels.size(); ++i) {
            m_chat.setChannelColor(kChannels[i], chatColor(settings.channelColors[i]));
        }
    }

    void WorldScene::dressPlayer() {
        std::array<std::uint16_t, Inventory::kEquipmentSlots> looks = m_inventory.equipmentIds();
        for (int slot = 1; slot <= Inventory::kEquipmentSlots; ++slot) {
            if (const int look = m_inventory.equipped(slot).lookItemId) {
                looks[static_cast<std::size_t>(slot - 1)] = static_cast<std::uint16_t>(look);
            }
        }
        const CharacterLook& look = m_player.look;
        const int job = look.job;
        m_player.look = CharacterLook::dressed(look.bodyType, look.head, look.basePattern, looks, m_services.items);
        m_player.look.job = job;
    }

    void WorldScene::announceEquipment(int itemId, bool putOn, int petPlace) {
        if (const auto item = m_services.items.find(itemId); item && item->equipSlot > 0) {
            m_messages.show(
                "[ " + item->name + " ] " + (putOn ? "have equiped" : "have unequiped") +
                    (petPlace > 0 ? m_itemWindow.equipChangeLines(petPlace) : std::string()),
                3000
            ); // message display time in ms
        }
    }

    void WorldScene::onBagItems(const std::vector<protocol::BagItemRecord>& records) {
        for (const protocol::BagItemRecord& record : records) {
            m_inventory.addToBag(record.slot, record.item);
        }
        inventoryChanged();
    }

    void WorldScene::onBagItemAdded(const protocol::BagItemRecord& record) {
        m_inventory.addToBag(record.slot, record.item);
        m_services.playSound("sound/wav0152.wav");
        m_remote.bagFilled();
        inventoryChanged();
    }

    void WorldScene::onBagItemsRemoved(int itemId, int count) {
        m_inventory.removeFromBag(itemId, count);
        inventoryChanged();
    }

    void WorldScene::onBagSlotDecreased(int slot, int count) {
        m_inventory.takeFromBag(slot, count);
        inventoryChanged();
    }

    void WorldScene::onBagItemsMoved(int fromSlot, int count, int toSlot) {
        m_inventory.moveInBag(fromSlot, count, toSlot);
        inventoryChanged();
    }

    void WorldScene::onEquipment(const std::vector<protocol::ItemInstance>& items) {
        for (const protocol::ItemInstance& item : items) {
            m_inventory.equip(item);
        }
        dressPlayer();
        inventoryChanged();
    }

    void WorldScene::onEquipmentRemoved(int slot, bool destroyed) {
        if (destroyed) {
            announceEquipment(m_inventory.equipped(slot).itemId, false);
        }
        m_inventory.clearEquipment(slot);
        dressPlayer();
        inventoryChanged();
    }

    void WorldScene::onUnequipped(int equipSlot, int bagSlot) {
        const int itemId = m_inventory.equipped(equipSlot).itemId;
        m_inventory.unequipToBag(equipSlot, bagSlot);
        m_remote.equipmentChanged();
        announceEquipment(itemId, false);
        dressPlayer();
        inventoryChanged();
    }

    void WorldScene::onEquippedFromBag(int bagSlot, int freeSlot) {
        const int itemId = m_inventory.bagItem(bagSlot).itemId;
        m_inventory.equipFromBag(bagSlot, freeSlot);
        m_remote.equipmentChanged();
        announceEquipment(itemId, true);
        dressPlayer();
        inventoryChanged();
    }

    void WorldScene::onPetEquipmentRemoved(int place, int slot, bool told) {
        const auto& pet = m_pets.at(place);
        if (!pet || slot < 1 || slot > 6) { // equipment slots 1..6
            return;
        }
        if (told) {
            announceEquipment(pet->worn[static_cast<std::size_t>(slot)].itemId, false, place);
        }
        m_pets.clearEquipment(place, slot);
        inventoryChanged();
    }

    void WorldScene::onPetUnequipped(int place, int equipSlot, int bagSlot) {
        const auto& pet = m_pets.at(place);
        if (!pet || equipSlot < 1 || equipSlot > 6) { // equipment slots 1..6
            return;
        }
        const protocol::ItemInstance item = pet->worn[static_cast<std::size_t>(equipSlot)];
        if (!item.empty()) {
            m_inventory.addToBag(bagSlot, item);
            announceEquipment(item.itemId, false, place);
            m_pets.clearEquipment(place, equipSlot);
        }
        m_remote.equipmentChanged();
        inventoryChanged();
    }

    void WorldScene::onPetEquippedFromBag(int place, int bagSlot) {
        const protocol::ItemInstance item = m_inventory.bagItem(bagSlot);
        if (item.empty() || !m_pets.at(place)) {
            return;
        }
        m_inventory.takeFromBag(bagSlot, 1);
        const protocol::ItemInstance before = m_pets.swapEquipment(place, item);
        if (!before.empty()) {
            m_inventory.addToBag(bagSlot, before);
        }
        announceEquipment(item.itemId, true, place);
        m_remote.equipmentChanged();
        inventoryChanged();
    }

    void WorldScene::onPetEquipmentSet(int place, const protocol::ItemInstance& item) {
        if (m_pets.equip(place, item)) {
            inventoryChanged();
        }
    }

    void WorldScene::onEquipmentWear(int place, int slot, int wear) {
        constexpr int kDestroyed = 250; // wear value 250: the equipment is destroyed
        if (slot < 1 || slot > 6) { // equipment slots 1..6
            return;
        }
        std::string name;
        if (place == 0) {
            m_inventory.setEquipmentWear(slot, wear);
            const auto record = m_services.items.find(m_inventory.equipped(slot).itemId);
            name = record ? record->name : "";
        } else {
            auto& pet = m_pets.at(place);
            if (!pet) {
                return;
            }
            pet->worn[static_cast<std::size_t>(slot)].wear = wear;
            const auto record = m_services.items.find(pet->worn[static_cast<std::size_t>(slot)].itemId);
            name = pet->name + " " + (record ? record->name : "");
        }
        if (wear == kDestroyed && m_fightState == 0) {
            m_messages.show(name + "Destroyed", 2000); // message display time in ms
        }
    }

    void WorldScene::onEquipBroken(int owner, int itemId) {
        constexpr int kDestroyed = 250; // wear value 250: the equipment is destroyed
        const auto record = m_services.items.find(itemId);
        if (!record || record->equipSlot < 1 || record->equipSlot > 6) { // equipment slots 1..6
            return;
        }
        const std::size_t slot = static_cast<std::size_t>(record->equipSlot);
        std::string name;
        if (owner == 0) {
            m_inventory.setEquipmentWear(record->equipSlot, kDestroyed);
            name = record->name;
        } else {
            auto& pet = m_pets.at(owner);
            if (!pet) {
                return;
            }
            pet->worn[slot].wear = kDestroyed;
            name = pet->name + " " + record->name;
        }
        if (m_fightState != 0) {
            m_messages.show(name + "Destroyed", 2000); // message display time in ms
        }
    }

    void WorldScene::onCrystalMounted(int bagSlot, int stoneId) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots) {
            return;
        }
        m_inventory.setInsertItem(bagSlot, stoneId);
        const auto stone = m_services.items.find(stoneId);
        const auto piece = m_services.items.find(m_inventory.bagItem(bagSlot).itemId);
        m_messages.show(
            (stone ? stone->name : std::string()) + " has been mounted[ " + (piece ? piece->name : std::string()) + " ]",
            2000
        ); // message display time in ms
        m_compoundWindow.showResult(m_inventory.bagItem(bagSlot).itemId, bagSlot);
    }

    void WorldScene::onPotionUsedAmount(int bagSlot, int amount, bool add) {
        if (bagSlot < 1 || bagSlot > Inventory::kBagSlots) {
            return;
        }
        if (add) {
            m_inventory.addUsedAmount(bagSlot, amount);
            m_services.playSound("sound/wav0152.wav");
        } else {
            m_inventory.setUsedAmount(bagSlot, amount);
        }
        inventoryChanged();
    }

    void WorldScene::onBottlePool(int kind, int amount) {
        m_remote.supplyDone();
        if (kind == 1 || kind == 3) { // bottle kinds 1 and 3: the HP bottle
            m_status.hpBottle = amount;
        } else if (kind == 2 || kind == 4) { // bottle kinds 2 and 4: the SP bottle
            m_status.spBottle = amount;
        }
        if (kind == 1 || kind == 2) { // bottle kinds 1 and 2 come with the sound
            m_services.playSound("sound/wav0152.wav");
        }
    }

    void WorldScene::onPotentialResult(int petPlace, int potential) {
        int before = m_status.potential;
        if (petPlace == 0) {
            m_status.potential = potential;
        } else if (auto& pet = m_pets.at(petPlace)) {
            before = pet->potential;
            pet->potential = potential;
        } else {
            return;
        }
        m_potentialWindow.result(petPlace, before, potential);
        m_potentialReturn.petChanged(petPlace);
    }

    void WorldScene::onPotentialReturned(int petPlace) {
        if (auto& pet = m_pets.at(petPlace)) {
            pet->potential = 0;
        } else {
            return;
        }
        m_potentialReturn.petChanged(petPlace);
        m_potentialWindow.petChanged(petPlace);
    }

    void WorldScene::onItemUsed() { m_services.playSound("sound/wav0152.wav"); }

    int WorldScene::sceneType() const {
        const auto scene = m_scenes && m_map ? m_scenes->find(m_map->id()) : std::nullopt;
        return scene ? scene->type : 0;
    }

    SceneEffects::Conditions WorldScene::effectConditions() const {
        SceneEffects::Conditions conditions;
        if (!m_map) {
            return conditions;
        }
        const auto scene = m_scenes ? m_scenes->find(m_map->id()) : std::nullopt;
        conditions.mapId = m_map->id();
        conditions.sceneEffect = scene ? scene->effect : 0;
        conditions.christmas = m_christmasOn && isChristmasMap(m_map->id());
        conditions.wedding = m_weddingFlag;
        return conditions;
    }

    void WorldScene::onChristmasAnnounced(const std::string& text) { m_chat.add(0, text); }

    void WorldScene::onNpcVanished(int npcIndex) {
        constexpr int kVanishingNpc = 38010; // NPC 38010 (not in the NPC table): shows the vanish effect (picture L10016)
        constexpr int kFrames = 15; // frames in the L10016 vanish picture
        auto npc = findNpc(npcIndex);
        if (!npc || npc->get().info.npcId != kVanishingNpc) {
            return;
        }
        const auto picture = m_services.pictures.find("L10016");
        if (!picture) {
            return;
        }
        LightEffect::Setup setup;
        setup.picture = *picture;
        setup.frameMs = 30; // frame time in ms
        setup.firstFrame = 1;
        setup.lastFrame = kFrames;
        setup.frameCount = kFrames;
        setup.motion = LightEffect::Motion::Once;
        setup.loop = false;
        setup.alpha = 30; // light alpha (0..255)
        setup.offsetY = 25; // pixels below the position
        m_vanishAt = npc->get().body.position;
        m_vanish.start(setup, m_camera.toScreen(m_vanishAt), m_nowMs);
    }

    void WorldScene::onChristmasEvent(bool on) {
        m_christmasOn = on;
        playMapMusic();
    }

    void WorldScene::onServerRelogin(bool toPvp) {
        ServerRelogin relogin;
        relogin.account = m_services.session.loginName;
        relogin.password = m_services.session.password;
        relogin.slot = m_player.id < kSecondCharacterOffset ? 1 : 2; // character slot 1 or 2 of the account
        relogin.toPvp = toPvp;
        relogin.delayMs = toPvp ? 3000 : 5000; // wait before the relogin in ms (3 s to the PvP server, 5 s otherwise)
        if (toPvp) {
            const auto lines = core::readLines(m_services.game.data("SERVER.INI"));
            const auto pvp = lines ? formats::ServerList::parse(*lines).pvpServer() : std::nullopt;
            if (!pvp) {
                core::logError("87/1: SERVER.INI has no PvP server (group 95)");
                return;
            }
            relogin.address = pvp->address;
            relogin.lineName = pvp->name;
        }
        m_services.session.relogin = relogin;
        leave(false);
    }

    void WorldScene::onNewYearEvent(bool on) { m_newYear.setOn(on); }

    void WorldScene::onLucky7Count(int count) { m_newYear.setLucky7Count(count); }

    void WorldScene::playMapMusic() {
        if (!m_scenes || !m_map) {
            return;
        }
        if (m_christmasOn && isChristmasMap(m_map->id())) {
            m_services.music.playLooped(m_services.game.data("Sound/BGM0028.wav"));
        } else if (const auto scene = m_scenes->find(m_map->id()); scene && !scene->music.empty()) {
            m_services.music.playLooped(m_services.game.data("Sound/" + scene->music + ".wav"));
        }
    }

    bool WorldScene::screenFrozen() const { return m_battleShown || (m_dimStep == kDarkStep && m_dimTarget == kDarkStep); }

    int WorldScene::ownLevel() const {
        const auto character = m_services.session.character(m_services.session.playingSlot);
        return character ? character->level : 1;
    }

    bool WorldScene::canWalk() const {
        return !m_entering && m_inWorld && !m_eventRunner.isRunning() && !m_moveLocked && !m_inputLocked && !m_npcMaking && m_map &&
               !m_teams.isMember(m_player.id) && !m_quiz.isOpen() && m_fightState == 0 && m_activeSport == 0 && m_innSleep == InnSleep::Awake && !m_darkMode &&
               !m_remote.isWandering() && !m_mall.isOpen() && !m_cart.isOpen() && !m_maid.isActive() && !m_reFill.isOpen();
    }

    bool WorldScene::faceMouse() {
        if (m_fightState != 0 || !m_map) {
            return false;
        }
        const bool playing = m_player.action != 0;
        const bool making = m_makeJobs.petSlot() == 0 && m_makeJobs.activeSlot() != 0;
        const bool stalling = m_player.stall && !m_player.fishing;
        if (!playing && !making && !stalling) {
            return false;
        }
        if (playing && m_player.usedUtensil != 0 && UtensilSeat::sitKind(m_house.furniture().piece(m_player.usedUtensil).itemId) == 0) {
            return true;
        }
        if (playing && m_inputLocked) {
            return true;
        }
        const core::Point point = m_camera.toMap(worldMouse());
        bool turn = true;
        if (playing && m_player.fishing) {
            constexpr int kWaterReach = 6; // how many tiles away the mouse may be from the player on an axis to turn while fishing
            constexpr int kWater = 2; // tile attribute 2: water
            const core::Point tile{point.x / formats::MapFile::kCellSize, point.y / formats::MapFile::kCellSize};
            const core::Point own{m_player.position.x / formats::MapFile::kCellSize, m_player.position.y / formats::MapFile::kCellSize};
            turn = (std::abs(tile.x - own.x + 1) <= kWaterReach || std::abs(tile.y - own.y + 1) <= kWaterReach) && m_map->tileAttribute(tile) == kWater;
        }
        if (turn) {
            faceAction(m_player, directionTowards(m_player.position, point, m_player.direction));
        }
        return true;
    }

    core::Point WorldScene::walkTo(core::Point mapPoint, bool byKeyboard) {
        m_lastWalkMs = m_nowMs;
        m_ownTile = {m_player.position.x / formats::MapFile::kCellSize, m_player.position.y / formats::MapFile::kCellSize};
        std::vector<core::Point> path;
        core::Point lastCorner = m_player.position;
        if (m_house.inFloorRoom()) {
            std::vector<core::Point> cells = houseWalkPath(m_player.position, mapPoint);
            if (cells.size() < 2) { // a path needs a start and one more cell
                startWalking(m_player, {}, m_nowMs);
                return m_player.position;
            }
            lastCorner = cells[cells.size() - 2]; // the cell before the last one
            path.assign(cells.begin() + 1, cells.end());
        } else {
            path = findPath(*m_map, m_player.position, mapPoint, m_player.walkClass());
            if (path.empty()) {
                return mapPoint;
            }
            lastCorner = path.size() > 1 ? path[path.size() - 2] : m_player.position; // the point before the last one
        }
        if (!byKeyboard && m_mapPets.isHidden(m_player.id)) {
            m_mapPets.setHidden(m_player.id, false);
            if (m_marriage.spouseId() != 0) {
                m_services.connection.send(protocol::petShownRequest(m_player.id));
            }
        }
        if (m_player.fishing) {
            stopFishing();
        }
        stopAutoCollect();
        leaveBath();
        stopOwnAction();
        leaveSeat();
        if (m_guildWork.stopLabour()) {
            m_services.connection.send(protocol::stopLabourRequest());
        }
        if (m_guildWork.stopCraft()) {
            m_services.connection.send(protocol::stopCraftRequest());
        }
        const core::Point destination = path.back();
        startWalking(m_player, std::move(path), m_nowMs);
        if (m_lastWalkSent && m_lastWalkSent->x == destination.x && m_lastWalkSent->y == destination.y) {
            walkIntoWaterOrOut(mapPoint);
            return destination;
        }
        m_lastWalkSent = destination;
        sendWalk(false, directionTowards(lastCorner, destination, m_player.direction), destination);
        return destination;
    }

    void WorldScene::sendWalk(bool stopped, int direction, core::Point destination) {
        protocol::WalkRequest request;
        request.stopped = stopped;
        request.direction = direction;
        request.destination = destination;
        request.check =
            static_cast<std::uint8_t>(static_cast<std::uint32_t>(m_services.session.loginId) % 13 + ownLevel()); // check byte: login id mod 13 plus the level
        request.random = static_cast<std::uint8_t>(std::uniform_int_distribution<int>(0, 219)(m_random)); // random byte of 0..219
        m_services.connection.send(request.encode());
    }

    void WorldScene::onWalkStopped(bool locked) {
        m_moveLocked = locked;
        if (!m_player.walking) {
            return;
        }
        startWalking(m_player, {}, m_nowMs);
        m_lastWalkSent = m_player.position;
        sendWalk(true, m_player.direction, m_player.position);
    }

    void WorldScene::onPlayerJumped(std::int32_t playerId, core::Point position) {
        const bool own = static_cast<std::uint32_t>(protocol::stripServerOffset(playerId)) == m_services.session.loginId;
        if (own) {
            placeCharacter(m_player, position);
            return;
        }
        const auto found = m_others.find(playerId);
        if (found == m_others.end()) {
            return;
        }
        placeCharacter(found->second, position);
    }

    void WorldScene::placeCharacter(MapCharacter& character, core::Point position, bool followWithCamera) {
        character.position = position;
        character.direction = kFacingSouth;
        startWalking(character, {}, m_nowMs);
        m_trails.at(character.id, position, true);
        m_mapPets.ownerPlaced(character.id);
        if (followWithCamera && isOwn(character.id) && m_map) {
            m_camera.centerOn(position, m_map->width(), m_map->height());
        }
    }

    void WorldScene::moveCharacters(std::uint64_t nowMs) {
        if (m_mouseHeld && m_player.walking && canWalk() && !m_player.stall && nowMs - m_lastWalkMs >= kHeldWalkMs) {
            m_walkMark.start(walkTo(m_camera.toMap(m_mouse)));
        }
        const auto stand = [this](MapCharacter& character) {
            if (character.horse) {
                startAction(character, kHorseAction);
            }
            const int attribute = m_map->cellAttribute(character.position);
            if ((attribute & formats::MapFile::kWall) == 0) {
                character.cellAttr = attribute;
            }
        };
        const core::Point screenBefore = m_camera.toScreen(m_player.position);
        const bool wasWalking = m_player.walking;
        const WalkSteps steps = m_services.freeFrames ? WalkSteps::SubPixel : WalkSteps::Whole;
        const core::Point step = walkStep(m_player, nowMs, steps);
        if (wasWalking || step.x != 0 || step.y != 0) {
            m_ownTile = {m_player.position.x / formats::MapFile::kCellSize, m_player.position.y / formats::MapFile::kCellSize};
        }
        stand(m_player);
        if (step.x != 0 || step.y != 0) {
            m_camera.follow(step, screenBefore, m_map->width(), m_map->height());
        }
        for (auto& [id, other] : m_others) {
            walkStep(other, nowMs, steps);
            stand(other);
            m_trails.at(id, other.position, false);
        }
        m_trails.at(m_player.id, m_player.position, false);
        followLeaders(nowMs);
        driveMaker();
        m_mapPets.update(nowMs, steps);
        for (MapPet& pet : m_mapPets.pets()) {
            stand(pet.body);
        }
        for (MapNpc& npc : m_npcs) {
            updateNpcVisibility(npc, nowMs);
            moveNpc(npc, nowMs, m_random, steps);
            if (npc.eventWalking && !npc.body.walking) {
                npc.eventWalking = false;
                npc.body.pixelsPerMs = npc.ownPace;
                if (npc.hideOnArrival) {
                    npc.hideOnArrival = false;
                    onNpcHidden(npc.index, 65535); // 65535 seconds means for good
                }
            }
        }
    }

    void WorldScene::walkWithKeys(std::uint64_t nowMs) {
        if (!canWalk() || m_mouseMode != 0 || m_player.stall || m_screen.isTyping() || nowMs - m_keyWalkMs < kHeldWalkMs) {
            return;
        }
        const bool left = m_screen.isKeyHeld(ui::Key::Left);
        const bool right = m_screen.isKeyHeld(ui::Key::Right);
        const bool up = m_screen.isKeyHeld(ui::Key::Up);
        const bool down = m_screen.isKeyHeld(ui::Key::Down);
        if (!left && !right && !up && !down) {
            return;
        }
        m_keyWalkMs = nowMs;
        if (const auto scene = m_scenes ? m_scenes->find(m_map->id()) : std::nullopt;
            scene && (scene->type == 8 || scene->type == 14) && !m_player.vehicle) { // scene types 8 and 14: no key walking without a vehicle
            return;
        }
        if (m_makeJobs.petSlot() == 0 && m_makeJobs.activeSlot() != 0) {
            m_makeQuestions.askStop();
            return;
        }
        constexpr int kKeyStep = 80; // distance walked per arrow-key press, px
        core::Point target = m_player.walking && !m_player.path.empty() ? m_player.path.back() : m_player.position;
        if (left) {
            target.x = m_player.position.x - kKeyStep;
        }
        if (right) {
            target.x = m_player.position.x + kKeyStep;
        }
        if (up) {
            target.y = m_player.position.y - kKeyStep;
        }
        if (down) {
            target.y = m_player.position.y + kKeyStep;
        }
        walkTo(target, true);
    }

    void WorldScene::onEnterWorldFinished() { finishEntering(); }

    void WorldScene::onWorldReady() {
        m_inWorld = true;
        m_luckyBags.load();
        stopFishing();
        m_remote.settings().ticks[2] = false; // remote setting tick 2 (the wander option) is cleared
        showHouseTitle();
    }

    void WorldScene::onPlayerAppeared(const protocol::OtherPlayerRecord& record, bool entering) {
        (void)entering;
        m_otherRecords[record.playerId] = record;
        if (!m_map || record.mapId != m_map->id()) {
            m_others.erase(record.playerId);
            return;
        }
        placeOther(record);
    }

    void WorldScene::placeOther(const protocol::OtherPlayerRecord& record) {
        MapCharacter& character = m_others[record.playerId];
        character.id = record.playerId;
        character.name = record.name;
        character.nickname = record.nickname;
        character.pvpWins = record.pvpWins;
        character.pvpRank = record.pvpRank;
        character.arenaVictory = record.arenaVictory;
        character.nameColor = kOtherNameColor;
        character.look = CharacterLook::fromAppearance(record.appearance, m_services.items, record.job);
        character.position = {record.x, record.y};
        character.direction = kFacingSouth;
        startWalking(character, {}, m_nowMs);
        m_trails.scatter(record.playerId, character.position, m_random);
        m_mapPets.ownerPlaced(record.playerId);
    }

    void WorldScene::onPlayerWalks(std::int32_t playerId, core::Point to, std::optional<int> direction) {
        if (!m_map) {
            return;
        }
        const auto found = m_others.find(playerId);
        if (found == m_others.end()) {
            if (!direction) {
                m_player.position = to;
                startWalking(m_player, {}, m_nowMs);
                m_camera.centerOn(to, m_map->width(), m_map->height());
            }
            return;
        }
        MapCharacter& character = found->second;
        const core::Point centre = m_camera.toMap({Camera::kScreenWidth / 2, Camera::kScreenHeight / 2});
        std::vector<core::Point> path;
        if (std::abs(to.x - centre.x) < Camera::kScreenWidth && std::abs(to.y - centre.y) < Camera::kScreenHeight) {
            if (m_house.inFloorRoom()) {
                const std::vector<core::Point> cells = houseWalkPath(character.position, to);
                if (cells.size() >= 2) { // a path needs a start and one more cell
                    path.assign(cells.begin() + 1, cells.end());
                }
            } else {
                path = findPath(*m_map, character.position, to, character.walkClass());
            }
        }
        if (path.empty()) {
            character.position = to;
            if (direction) {
                character.direction = *direction;
            }
            startWalking(character, {}, m_nowMs);
            return;
        }
        startWalking(character, std::move(path), m_nowMs, direction);
    }

    void WorldScene::onPlayerLeft(std::int32_t playerId) {
        const std::string name = speakerName(playerId);
        if (m_inputBar.channel() == 3 && !m_inputBar.whisperName().empty() && m_inputBar.whisperName() == name) { // input channel 3: whisper
            m_chat.add(10, "target to whisper " + name + "offline", playerId); // chat channel 10: notice line
        }
        m_inputBar.removeWhisperName(name);
        m_others.erase(playerId);
        m_otherRecords.erase(playerId);
        m_mateVitals.erase(playerId);
        m_mapPets.remove(playerId);
        m_trails.remove(playerId);
        m_effectLights.remove(playerId);
    }

    void WorldScene::onChat(int channel, std::int32_t speakerId, const std::string& text) {
        if (!m_systemWindow.chatChannelOn(channel)) {
            return;
        }
        if (const auto record = m_otherRecords.find(speakerId);
            channel != 0 && record != m_otherRecords.end() && m_blacklist.contains(speakerId, record->second.name)) {
            return;
        }
        static const char* const kChannelNames[] = {"", "(World)", "(Local)", "(Whisper)", "(GM)", "(Team)", "(guild)", "(Ally)"};
        if (channel == 0) {
            m_chat.add(0, speakerId == 0 ? "(System Prompt):" + text : text);
            return;
        }
        m_chat.add(channel, std::string(kChannelNames[channel]) + speakerName(speakerId) + ":" + text, speakerId);
        if (channel == 3 && speakerId != m_player.id && !speakerName(speakerId).empty()) { // chat channel 3: whisper
            m_inputBar.addWhisperName(speakerName(speakerId), false);
        }
        if (channel == 1 || channel == 2 || channel == 3 || channel == 5 || channel == 6) { // chat channels: 1 world, 2 local, 3 whisper, 5 team, 6 guild
            if (const auto other = m_others.find(speakerId); other != m_others.end()) {
                other->second.chat.show(text, m_nowMs, other->second.fightState != 0);
            }
        }
    }

    std::string WorldScene::speakerName(std::int32_t speakerId) const {
        if (speakerId == m_player.id) {
            return m_player.name;
        }
        if (const auto found = m_others.find(speakerId); found != m_others.end()) {
            return found->second.name;
        }
        const auto record = m_otherRecords.find(speakerId);
        if (record != m_otherRecords.end()) {
            return record->second.name;
        }
        if (m_services.session.serverGrouped) {
            if (const int friendIndex = m_friends.indexOf(speakerId); friendIndex != 0) {
                return m_friends.at(friendIndex).record.name;
            }
            if (const int member = m_guild.indexOf(speakerId); member >= 0) {
                return m_guild.at(member).record.name;
            }
        }
        if (m_marriage.spouseId() != 0 && speakerId == m_marriage.spouseId()) {
            return m_marriage.card().name;
        }
        return {};
    }

    InputBar::Sent WorldScene::sendChat(int typedChannel, const std::string& text) {
        using Sent = InputBar::Sent;
        if (m_services.partyChat && m_services.partyChat(typedChannel, text)) {
            return Sent::Sent;
        }
        if (m_chat.isHidden()) {
            m_messages.show("Chat window is closed.", 2000); // message display time in ms
            return Sent::Kept;
        }
        const std::string whisper = parseChatCommand(text);
        const int channel = m_inputBar.channel() == 3 ? 3 : typedChannel; // input channel 3: whisper stays whisper
        const auto refuse = [this](const std::string& message) {
            m_chat.add(10, message); // chat channel 10: notice line
            return Sent::Kept;
        };
        const auto sendOn = [this, &text](int sub) {
            m_services.connection.send(protocol::chatRequest(sub, text));
            m_player.chat.show(text, m_nowMs, m_player.fightState != 0);
        };
        const auto silenced = [this](int silencedChannel) { return m_chatSilence.isSilenced(silencedChannel); };
        switch (channel) {
            case 1: { // typed channel 1: world chat (needs a radio set and SP)
                if (silenced(1)) {
                    return refuse(ChatSilence::refusal());
                }
                if (!m_systemWindow.chatChannelOn(1)) {
                    return refuse("Unable to say when you close the World Channel.");
                }
                const int radio = radioKind(m_inventory.equipped(kRadioSlot).itemId);
                if (radio == 0) {
                    return refuse("(System Prompt):World channel cannot be used without the radio set");
                }
                if (m_status.sp < kRadioSp) {
                    return refuse("(System Prompt):Not enough SP, cannot use the Radio Set");
                }
                sendOn(1);
                if (radio == kWorldRadio) {
                    m_chat.add(1, "(All)" + m_player.name + ":" + text, m_player.id);
                } else {
                    m_chat.add(1, "(World)" + m_player.name + ":" + text, m_player.id);
                }
                return Sent::Sent;
            }
            case 2: // typed channel 2: local chat
                if (!m_systemWindow.chatChannelOn(2)) { // channel switch 2: local
                    return refuse("Unable to say when you close the Whisper Channel.");
                }
                if (silenced(2)) { // chat channel 2: local
                    return refuse(ChatSilence::refusal());
                }
                if (m_chatBlockedMs != 0 && m_nowMs - m_chatBlockedMs < kChatBlockMs) {
                    m_messages.show("System announcement, you cannot post again in the Local Channel' for 8 seconds.", 2000); // message display time in ms
                    return Sent::Kept;
                }
                if (m_player.trojanWar) {
                    return Sent::Kept;
                }
                sendOn(2); // chat sub id 2: local
                m_chat.add(2, "(Local)" + m_player.name + ":" + text, m_player.id); // chat channel 2: local
                return Sent::Sent;
            case 3: { // typed channel 3: whisper
                if (!m_systemWindow.chatChannelOn(3)) { // channel switch 3: whisper
                    return refuse("Unable to whisper after turning off the whisper channel");
                }
                if (silenced(3)) { // chat channel 3: whisper
                    return refuse(ChatSilence::refusal());
                }
                if (m_map && m_map->id() == kNoWhisperMap) {
                    m_messages.show("Unable to use the function here.", 1200); // message display time in ms
                    return Sent::Kept;
                }
                if (m_inputBar.whisperName().empty()) {
                    m_inputBar.dropWhisperTarget();
                    return refuse("Target unavailable to whisper");
                }
                const std::int32_t target = m_inputBar.whisperTarget();
                if (target == 0) {
                    m_chat.add(10, "Target unavailable to whisper"); // chat channel 10: notice line
                    return Sent::Sent;
                }
                const bool online = m_otherRecords.count(target) != 0 || (m_services.session.serverGrouped && m_friends.indexOf(target) != 0);
                if (!online) {
                    m_inputBar.dropWhisperTarget();
                    return refuse("The player isn't online");
                }
                const std::string& line = whisper.empty() ? text : whisper;
                m_services.connection.send(protocol::whisperRequest(target, line));
                m_player.chat.show(line, m_nowMs, m_player.fightState != 0);
                return Sent::Sent;
            }
            case 4: // typed channel 4: team chat (sub id and channel switch 5)
                if (!m_systemWindow.chatChannelOn(5)) { // channel switch 5: team
                    return refuse("Unable to say when you close the Team Channel.");
                }
                if (!m_teams.inTeam(m_player.id)) {
                    m_chat.add(10, "(System Prompt):Cannot use team channel without joining a team"); // chat channel 10: notice line
                    m_inputBar.setChannel(2); // back to the local channel
                    return Sent::Dropped;
                }
                if (silenced(4)) { // chat channel 4: team
                    return refuse(ChatSilence::refusal());
                }
                sendOn(5); // chat sub id 5: team
                return Sent::Sent;
            case 5: // typed channel 5: guild chat (sub id and channel switch 6)
                if (!m_systemWindow.chatChannelOn(6)) { // channel switch 6: guild
                    return refuse("Unable to say when you close the Guild Channel.");
                }
                if (m_guild.leaderId() == 0) {
                    m_chat.add(10, "(System Prompt):Cannot use guild channel without joining a guild"); // chat channel 10: notice line
                    m_inputBar.setChannel(2); // back to the local channel
                    return Sent::Kept;
                }
                if (silenced(5)) { // chat channel 5: guild
                    return refuse(ChatSilence::refusal());
                }
                sendOn(6); // chat sub id 6: guild
                return Sent::Sent;
            default: // any other channel: kept in the field
                return Sent::Kept;
        }
    }

    std::string WorldScene::parseChatCommand(const std::string& text) {
        if (text.empty() || text[0] != '/') {
            return {};
        }
        const std::size_t space = text.find(' ');
        const std::string command = space == std::string::npos ? std::string() : text.substr(0, space);
        if (!core::equalsIgnoreCase(command, "/msg")) {
            return {};
        }
        const std::string rest = text.substr(space + 1);
        const std::size_t nameEnd = rest.find(' ');
        const std::string name = nameEnd == std::string::npos ? std::string() : rest.substr(0, nameEnd);
        m_inputBar.setChannel(3); // input channel 3: whisper
        const auto target = worldPlayerNamed(name);
        if (!target) {
            m_inputBar.dropWhisperTarget();
            return {};
        }
        m_inputBar.aimWhisper(*target);
        m_chat.add(4, "(GM):target to whisper " + name, 0); // chat channel 4: GM
        return rest.substr(nameEnd + 1);
    }

    std::optional<std::int32_t> WorldScene::worldPlayerNamed(const std::string& name) const {
        if (name == m_player.name) {
            return m_player.id;
        }
        if (m_marriage.spouseId() != 0 && name == m_marriage.card().name && m_marriage.card().onlineState != 3) { // online state 3: offline
            return m_marriage.spouseId();
        }
        for (const auto& [id, record] : m_otherRecords) {
            if (record.name == name) {
                return id;
            }
        }
        return std::nullopt;
    }

    bool WorldScene::chatChannelAllowed(int channel) {
        if (channel == 4 && !m_teams.inTeam(m_player.id)) { // typed channel 4: team
            m_chat.add(10, "(System Prompt):Cannot use party channel without joining a party"); // chat channel 10: notice line
            return false;
        }
        if (channel == 5 && m_guild.leaderId() == 0) { // typed channel 5: guild
            m_chat.add(10, "(System Prompt):Cannot use guild channel without joining a guild"); // chat channel 10: notice line
            return false;
        }
        return true;
    }

    void WorldScene::onOwnAttributes(const protocol::OwnAttributes& attributes) {
        m_status.element = attributes.element;
        m_status.hp = attributes.hp;
        m_status.maxHp = attributes.maxHp;
        m_status.sp = attributes.sp;
        m_status.maxSp = attributes.maxSp;
        m_status.stats = attributes.stats;
        m_status.exp = attributes.exp;
        m_status.reborn = attributes.reborn;
        m_status.job = attributes.job;
        m_status.skillPoints = attributes.skillPoints;
        m_status.statPoints = attributes.statPoints;
        m_status.potential = attributes.potential;
        m_player.guildId = attributes.guildId;
        m_status.skills.clear();
        for (const protocol::LearnedSkill& skill : attributes.skills) {
            m_status.skills[skill.key] = skill;
        }
        m_status.level = levelFromExp(m_services.formula, attributes.exp, attributes.reborn);
        refreshHud();
    }

    /// @brief Maps a server attribute id to its place in the five base stats.
    /// @param attribute Server attribute id (28 STR, 29 CON, 27 INT, 33 WIS, 30 AGI).
    /// @return Index 0..3 for STR, CON, INT, WIS; 4 for AGI and anything else.
    std::size_t statIndex(int attribute) {
        switch (attribute) {
            case 28: // attribute 28: strength
                return 0;
            case 29: // attribute 29: constitution
                return 1;
            case 27: // attribute 27: intelligence
                return 2;
            case 33: // attribute 33: wisdom
                return 3; // stat index 3
            default: // attribute 30: agility
                return 4; // stat index 4
        }
    }

    void WorldScene::skillChanged(int skillId, int level) {
        const auto skill = m_services.skills.find(skillId);
        if (!skill || skill->key == 0) {
            return;
        }
        protocol::LearnedSkill& learned = m_status.skills[skill->key];
        const int old = learned.level;
        learned.key = skill->key;
        learned.level = level;
        if (level > old) {
            m_messages.show(
                old == 0 ? "Learn skill: " + skill->name : "Skill[ " + skill->name + " ]The upper limit level is: " + std::to_string(level),
                3000
            ); // message display time in ms
            m_services.playSound("sound/wav0150.wav");
        }
    }

    bool WorldScene::isOwn(std::int32_t playerId) const {
        return static_cast<std::uint32_t>(protocol::stripServerOffset(playerId)) == m_services.session.loginId;
    }

    void WorldScene::onPetList(const std::vector<protocol::PetRecord>& pets) {
        m_pets.setList(pets);
        m_groupWindow.refresh();
        m_petStorage.refresh();
        m_itemWindow.refresh();
        m_skillWindow.refresh();
        m_potentialWindow.refreshPets();
        m_potentialReturn.refreshPets();
    }

    void WorldScene::onPetJoined(const protocol::PetJoined& joined) {
        if (!isOwn(joined.ownerId)) {
            return;
        }
        const int place = m_pets.join(joined.pet);
        if (place == 0) {
            return;
        }
        m_messages.show(m_pets.at(place)->name + " join your team", 1500); // message display time in ms
        m_services.playSound("sound/wav0152.wav");
        m_groupWindow.refresh();
        m_petStorage.refresh();
        m_itemWindow.refresh();
        m_skillWindow.refresh();
        m_potentialWindow.refreshPets();
        m_potentialReturn.refreshPets();
    }

    void WorldScene::onPetLeft(std::int32_t ownerId, int place) {
        if (!isOwn(ownerId) || !m_pets.at(place)) {
            return;
        }
        const std::string name = m_pets.at(place)->name;
        if (m_pets.activePlace() == place) {
            m_mapPets.recall(m_player.id, m_nowMs);
        }
        m_pets.remove(place);
        m_messages.show(name + " leave your team", 1500); // message display time in ms
        m_services.playSound("sound/wav0152.wav");
        m_groupWindow.refresh();
        m_petStorage.refresh();
        m_itemWindow.refresh();
        m_skillWindow.refresh();
        m_potentialWindow.refreshPets();
        m_potentialReturn.refreshPets();
    }

    void WorldScene::onPetRenamed(std::int32_t ownerId, int place, const std::string& name) {
        if (isOwn(ownerId) && m_pets.at(place)) {
            m_pets.at(place)->name = name;
            m_mapPets.rename(m_player.id, m_pets.at(place)->info.npcId, name);
            m_groupWindow.refresh();
        }
    }

    void WorldScene::onActivePet(int npcId, bool silent) {
        (void)silent;
        m_pets.setActive(npcId);
        m_remote.equipmentChanged();
        const auto& pet = m_pets.at(m_pets.activePlace());
        if (pet) {
            m_mapPets.show(m_player.id, pet->info, pet->name, pet->tradable, m_nowMs);
            syncOwnPetWeapon();
        } else {
            m_mapPets.recall(m_player.id, m_nowMs);
        }
        m_groupWindow.refresh();
    }

    void WorldScene::onDarkMode(bool dark) {
        m_darkMode = dark;
        if (dark) {
            LightEffect::Setup setup;
            setup.picture = m_services.pictures.find("Camp").value_or(gfx::PictureId{});
            setup.frameMs = 60; // frame time in ms
            setup.loop = false;
            setup.offsetY = 25; // pixels below the position
            m_playerLight.start(setup, m_camera.toScreen(m_player.position), m_nowMs);
        } else {
            m_playerLight.stop();
        }
    }

    void WorldScene::onRevertItemVariants() {
        constexpr int kFirstVariant = 20748; // first of the item variants 20748..20750 (not in the item table)
        constexpr int kLastVariant = 20750; // last variant item id, reverted to kItem (original RevertItemVariantsTo20747)
        constexpr int kItem = 20747; // item 20747 (not in the item table): what the variants turn back into
        m_inventory.replaceItemIds(kFirstVariant, kLastVariant, kItem);
        m_bank.replaceItemIds(kFirstVariant, kLastVariant, kItem);
        m_warehouse.replaceItemIds(kFirstVariant, kLastVariant, kItem);
        m_pets.replaceWornIds(kFirstVariant, kLastVariant, kItem);
        m_petStorage.replaceWornIds(kFirstVariant, kLastVariant, kItem);
    }

    void WorldScene::onPetSublimateFailed(int code) {
        if (code == 1) {
            m_messages.show("Sublimation pills cannot be used on this pet any more.",
                            2000); // message display time in ms
        }
    }

    void WorldScene::onForceLeaveWater() { m_services.connection.send(protocol::leaveWaterRequest()); }

    void WorldScene::onNpcsHidden(bool hidden) { m_npcsHidden = hidden; }

    void WorldScene::onPreviousMap(int mapId) { m_prevMapId = mapId; }

    void WorldScene::onCityBattleDeadPet(int place, bool dead) {
        m_pets.setCityBattleDead(place, dead);
        m_groupWindow.refresh();
    }

    void WorldScene::onScenePets(const std::vector<protocol::ScenePet>& pets) {
        for (const protocol::ScenePet& pet : pets) {
            const auto info = m_services.npcs.find(pet.npcId);
            if (!info || isOwn(pet.playerId)) {
                continue;
            }
            if (pet.mounted) {
                mountHorse(pet.playerId, pet.npcId);
                continue;
            }
            m_mapPets.show(pet.playerId, *info, pet.name.empty() ? info->name : pet.name, pet.tradable, m_nowMs);
            if (pet.wornItem != 0) {
                onScenePetEquipped(pet.playerId, pet.wornItem);
            }
        }
    }

    void WorldScene::onPetsMounted(const protocol::PetsMounted& pets) {
        for (const protocol::PetsMounted::Entry& entry : pets.pets) {
            if (entry.mounted) {
                mountHorse(pets.playerId, entry.npcId);
            }
        }
    }

    void WorldScene::mountHorse(std::int32_t playerId, int npcId) {
        const auto info = m_services.npcs.find(npcId);
        const auto rider = characterOf(playerId);
        if (!info || !rider || isOwn(playerId)) {
            return;
        }
        constexpr double kWalkSpeed = 0.16; // base walking speed, px per ms
        rider->get().horse = *info;
        rider->get().pixelsPerMs = kWalkSpeed + MountSpeedBonus::of(info->spriteId, info->bodyType, info->level);
        startAction(rider->get(), kHorseAction);
    }

    void WorldScene::onStarsOwned(const core::Bytes& payload) { m_starSky.loadOwned(payload); }

    void WorldScene::onStarAcquired(int star, int npcIndex) { m_starSky.acquired(star, npcIndex, m_nowMs); }

    void WorldScene::openStarSky() {
        if (m_starSky.active()) {
            return;
        }
        setHudShown(false);
        m_starSky.open(m_nowMs);
    }

    void WorldScene::onItemThrown(int itemId, int row, int col, const std::vector<std::pair<int, int>>& targets) {
        m_battle.throwItem(itemId, row, col, targets);
    }

    void WorldScene::onMonsterMet(int npcId) { m_monsterBook.add(npcId); }

    void WorldScene::onMonsterBits(const core::Bytes& payload) { m_monsterBook.addBits(payload); }

    void WorldScene::onPetMounted(std::int32_t playerId, int place, int npcId) {
        const auto info = m_services.npcs.find(npcId);
        const auto rider = characterOf(playerId);
        if (!info || !rider || place < 1 || place > PetTeam::kPlaces) {
            return;
        }
        rider->get().riding = RiddenPet{place, *info, m_services.characters.rideHeight(*info)};
        if (isOwn(playerId)) {
            m_pets.setRiding(place);
            m_groupWindow.refresh();
        }
    }

    void WorldScene::onPetRides(const std::vector<protocol::PetRide>& rides) {
        for (const protocol::PetRide& ride : rides) {
            const auto rider = characterOf(ride.playerId);
            if (!rider) {
                break;
            }
            if (const auto info = m_services.npcs.find(ride.npcId)) {
                rider->get().riding = RiddenPet{ride.place, *info, m_services.characters.rideHeight(*info)};
            }
        }
    }

    void WorldScene::onPlayerRidingFlag(std::int32_t playerId, bool riding) {
        if (const auto player = characterOf(playerId)) {
            player->get().ridingFlag = riding;
        }
    }

    void WorldScene::onScenePetEquipped(std::int32_t playerId, int itemId) {
        constexpr int kWeaponSlot = 3; // equip slot 3: weapon
        const auto record = m_services.items.find(itemId);
        if (isOwn(playerId) || !record || record->equipSlot != kWeaponSlot) {
            return;
        }
        for (const MapPet& pet : m_mapPets.pets()) {
            if (pet.ownerId == playerId && pet.following) {
                m_mapPets.setWeapon(playerId, weaponOfItem(itemId, pet.info.bodyType, m_services.items));
                return;
            }
        }
    }

    void WorldScene::onScenePetUnequipped(std::int32_t playerId) {
        constexpr int kWeaponSlot = 3; // equip slot 3: weapon
        if (isOwn(playerId)) {
            const int place = m_pets.activePlace();
            if (m_pets.at(place)) {
                m_pets.clearEquipment(place, kWeaponSlot);
                inventoryChanged();
            }
            return;
        }
        m_mapPets.setWeapon(playerId, {});
    }

    void WorldScene::syncOwnPetWeapon() {
        const auto& pet = m_pets.at(m_pets.activePlace());
        m_mapPets.setWeapon(m_player.id, pet ? petWeapon(*pet, m_services.items) : PetWeapon{});
    }

    void WorldScene::dismountOwnPet() {
        const auto& pet = m_pets.at(m_pets.ridingPlace());
        if (pet) {
            m_services.connection.send(protocol::dismountPetRequest(m_pets.ridingPlace(), pet->info.npcId));
        }
    }

    void WorldScene::onPetDismounted(std::int32_t playerId) {
        if (const auto rider = characterOf(playerId)) {
            rider->get().riding.reset();
            if (rider->get().horse) {
                rider->get().horse.reset();
                rider->get().pixelsPerMs = 0.16; // the player's walking pace in pixels per ms
                stopAction(rider->get());
            }
            if (isOwn(playerId)) {
                m_pets.setRiding(0);
                m_groupWindow.refresh();
            }
        }
    }

    void WorldScene::onScenePetRests(std::int32_t playerId) { m_mapPets.recall(playerId, m_nowMs); }

    void WorldScene::onPetAttributeChanged(int place, int attribute, std::int32_t value, std::int32_t extra) {
        const auto& pet = m_pets.at(place);
        if (!pet) {
            return;
        }
        constexpr int kAmity = 64; // pet attribute 64: amity
        constexpr int kSkillLevel = 110; // pet attribute 110: a skill's level (extra = skill id)
        constexpr int kHp = 25; // pet attribute 25: HP
        constexpr int kSp = 26; // pet attribute 26: SP
        if (attribute == kHp || attribute == kSp) {
            m_remote.supplyDone();
        }
        if (attribute == kAmity && value != pet->amity) {
            m_messages.show(
                pet->name + (pet->amity > value ? " Amity decreases " : " Amity increases ") + std::to_string(std::abs(pet->amity - value)),
                2000
            ); // message display time in ms
        }
        if (attribute == kSkillLevel) {
            petSkillChanged(place, extra, value);
        }
        m_pets.setAttribute(place, attribute, value, extra);
        m_groupWindow.refresh();
        m_itemWindow.refresh();
        if (attribute != kSkillLevel) {
            m_skillWindow.refresh();
        }
    }

    void WorldScene::petSkillChanged(int place, int skillId, int level) {
        const auto& pet = m_pets.at(place);
        const auto skill = m_services.skills.find(skillId);
        if (!pet || !skill) {
            return;
        }
        for (std::size_t i = 0; i < pet->info.skills.size(); ++i) {
            const int old = pet->skillLevels[i];
            if (pet->info.skills[i] != skillId || level <= old) {
                continue;
            }
            m_messages.show(
                old == 0 ? "Learn skill: " + skill->name : "Skill[ " + skill->name + " ]The upper limit level is: " + std::to_string(level),
                3000
            ); // message display time in ms
            m_services.playSound("sound/wav0150.wav");
        }
    }

    void WorldScene::onAttributeChanged(int attribute, std::int32_t value, std::int32_t extra) {
        switch (attribute) {
            case 110: // attribute 110: a skill's level (extra = skill id)
                skillChanged(extra, value);
                m_skillWindow.refresh();
                break;
            case 111: // attribute 111: a skill's experience (extra = skill id)
                if (const auto skill = m_services.skills.find(extra); skill && skill->key != 0) {
                    m_status.skills[skill->key].key = skill->key;
                    m_status.skills[skill->key].exp = value;
                }
                break;
            case 24: // attribute 24: element
                m_status.element = value;
                break;
            case 25: // attribute 25: HP
                m_remote.supplyDone();
                m_status.hp = std::clamp(value, 0, std::max(m_status.maxHp, 0));
                break;
            case 26: // attribute 26: SP
                m_remote.supplyDone();
                m_status.sp = std::clamp(value, 0, std::max(std::min(m_status.maxSp, 65535), 0)); // SP is held to 16 bits (65535)
                break;
            case 28: // attribute 28: strength
            case 29: // attribute 29: constitution
            case 27: // attribute 27: intelligence
            case 33: // attribute 33: wisdom
            case 30: // attribute 30: agility
                m_status.stats[statIndex(attribute)] = value;
                break;
            case 41: // attribute 41: combat value 0
            case 42: // attribute 42: combat value 1
            case 43: // attribute 43: combat value 2
            case 44: // attribute 44: combat value 3
            case 45: // attribute 45: combat value 4
                m_status.combat[static_cast<std::size_t>(attribute - 41)] = value; // 41 is the first combat attribute
                break;
            case 35: // attribute 35: level
                m_status.level = value;
                break;
            case 36: // attribute 36: experience
                m_status.exp = value;
                break;
            case 37: // attribute 37: skill points
                m_status.skillPoints = value;
                break;
            case 38: // attribute 38: stat points
                if (m_status.statPoints < value) {
                    m_mainButtons.alertItem();
                }
                m_status.statPoints = value;
                break;
            case 205: // attribute 205: maximum HP
                m_status.maxHp = value;
                m_status.hp = std::min(m_status.hp, m_status.maxHp);
                break;
            case 206: // attribute 206: maximum SP
                m_status.maxSp = value;
                m_status.sp = std::min(m_status.sp, m_status.maxSp);
                break;
            default: // other attributes are not shown here
                return;
        }
        refreshHud();
    }

    void WorldScene::onGoldChanged(std::int32_t amount, bool absolute) {
        constexpr std::int64_t kMostGold = 9999999; // largest gold a player carries (9,999,999)
        if (absolute) {
            m_status.gold = amount;
        } else {
            m_status.gold = std::clamp<std::int64_t>(m_status.gold + amount, 0, kMostGold);
            const bool told = !m_bankMoney.isOpen() && !m_itemTrade.isOpen();
            if (amount > 0 && told) {
                m_messages.show("Obtain " + std::to_string(amount) + " Gold", 2000); // message display time in ms
                m_services.playSound("sound/wav0152.wav");
            } else if (amount < 0 && told) {
                m_messages.show("Decrease " + std::to_string(-amount) + " Gold", 2000); // message display time in ms
            }
        }
        refreshHud();
    }

    void WorldScene::paintWeather(gfx::Canvas& canvas, bool under, std::uint64_t nowMs) {
        constexpr int kTileWidth = 800; // width of one weather tile, px (the original screen width)
        constexpr int kTileHeight = 600; // height of one weather tile, px (the original screen height)
        const core::Rect window = canvas.window();
        const core::Point origin = canvas.origin();
        const bool stepping = canvas.isAnimationFrame();
        const auto tileIndex = [](int at, int size) { return at >= 0 ? at / size : -((-at + size - 1) / size); };
        const auto paint = [&](int across, int down) {
            canvas.setOrigin({origin.x + across * kTileWidth, origin.y + down * kTileHeight});
            canvas.setAnimationFrame(stepping && across == 0 && down == 0);
            if (under) {
                m_effects.drawUnder(canvas, m_camera.origin(), nowMs, effectConditions());
            } else {
                m_effects.drawOver(canvas, m_camera.origin(), nowMs, effectConditions());
            }
        };
        paint(0, 0);
        for (int down = tileIndex(window.top(), kTileHeight); down <= tileIndex(window.bottom() - 1, kTileHeight); ++down) {
            for (int across = tileIndex(window.left(), kTileWidth); across <= tileIndex(window.right() - 1, kTileWidth); ++across) {
                if (across != 0 || down != 0) {
                    paint(across, down);
                }
            }
        }
        canvas.setOrigin(origin);
        canvas.setAnimationFrame(stepping);
    }

    void WorldScene::paintLocation(gfx::Canvas& canvas) {
        constexpr int kRight = 790; // right edge of the location text, px from the HUD corner's left
        constexpr int kY = 535; // top of the location text, px from the HUD corner's top (original 800x600 screen)
        constexpr int kCharWidth = 8; // width of one text character, px
        const std::string server = m_services.session.serverLineName.substr(0, 10); // the server line name is cut to 10 characters
        const auto scene = m_scenes ? m_scenes->find(m_map->id()) : std::nullopt;
        const bool inHouse = isHouseMap(m_map->id()) && !m_houseTitle.empty();
        const std::string text = server + " " +
                                 (inHouse ? m_houseTitle
                                  : scene ? scene->name
                                          : std::string()) +
                                 " X:" + std::to_string(m_player.position.x) + " Y:" + std::to_string(m_player.position.y);
        const core::Rect window = canvas.window();
        // 800x600 is the size of the original screen, whose top-left corner is the origin of the HUD
        const core::Point corner{window.right() - 800, window.bottom() - 600};
        const int y = corner.y + 600 - static_cast<int>((600 - kY) * m_screen.hudScale());
        canvas.drawText(
            {corner.x + kRight - static_cast<int>(text.size()) * kCharWidth, y},
            text,
            {{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}
        ); // text colour: opaque white
    }

    void WorldScene::onKicked(int reason) {
        core::logInfo("disconnected by the server (reason ", reason, ")");
        showDisconnect(disconnectNotice(reason, m_player.id));
    }

    void WorldScene::showDisconnect(const DisconnectNotice& notice) {
        m_disconnect.show(notice);
        m_dimTarget = kDarkStep;
        m_dimStep = std::max(m_dimStep, 1);
    }

    void WorldScene::onUnhandled(protocol::MessageId id) {
        core::logInfo("server message ", static_cast<int>(id.action), '/', static_cast<int>(id.sub), " is not handled yet");
    }

    void WorldScene::handleConnectionEvents() {
        while (const auto event = m_services.connection.nextEvent()) {
            if (*event == net::GameConnection::Event::Disconnected) {
                core::logInfo("the connection to the server closed");
                if (!m_disconnect.isOpen()) {
                    showDisconnect(DisconnectNotice{"Intermit to connect to the Server", DisconnectNotice::Buttons::PreviousAndQuit});
                }
            } else if (*event == net::GameConnection::Event::Reset) {
                m_disconnect.show("Connection Failed");
            }
        }
    }

    void WorldScene::followView(std::uint64_t nowMs) {
        const core::Rect part = m_services.zoom.shown(nowMs);
        if (!(part == m_services.view)) {
            m_camera.setView(part);
            if (m_map) {
                m_camera.centerOn(m_player.position, m_map->width(), m_map->height());
            }
            return;
        }
        if (m_camera.view() == m_services.view) {
            return;
        }
        m_camera.setView(m_services.view);
        if (m_map) {
            m_camera.centerOn(m_player.position, m_map->width(), m_map->height());
        }
    }

    bool WorldScene::isViewZoomed() const { return !(m_camera.view() == m_services.view); }

    core::Point WorldScene::worldMouse() const {
        return isViewZoomed() ? ViewZoom::fromWindow(m_screen.mouse(), m_camera.view(), m_services.view) : m_screen.mouse();
    }

    void WorldScene::update(std::uint64_t nowMs) {
        m_services.zoom.setOutAllowed(!m_battle.isOn());
        followView(nowMs);
        m_nowMs = nowMs;
        constexpr std::uint64_t kDimStepMs = 20; // time between two dim steps, ms
        if (m_dimStep != m_dimTarget && nowMs - m_dimStepMs >= kDimStepMs) {
            m_dimStepBefore = m_dimStep;
            m_dimStep += m_dimTarget > m_dimStep ? 1 : -1;
            m_dimStepMs = nowMs;
        }
        if (m_enteredMs == 0) {
            m_enteredMs = nowMs;
        }
        updateInnSleep(nowMs);
        m_instances.tick(nowMs);
        m_services.connection.update(nowMs);
        handleConnectionEvents();
        MessageContext context{*this, m_services.connection, m_services.session.loginId};
        const auto battleTurning = [this] { return (m_battle.isOn() && !m_battleShown) || m_battleClosing || m_battle.ownGone(); };
        while (m_innSleep == InnSleep::Awake && !battleTurning()) {
            const auto payload = m_services.connection.nextMessage();
            if (!payload) {
                break;
            }
            dispatchMessage(*payload, context);
        }
        if (m_entering && nowMs - m_enteredMs > kEnterTimeoutMs) {
            finishEntering();
        }
        if (m_fadeStep > 0) {
            --m_fadeStep;
            if (m_fadeStep == kMusicFadeStep) {
                playMapMusic();
            }
        }
        m_chat.update(nowMs);
        updateAutoCollect(nowMs);
        m_battle.update(nowMs);
        if (m_battle.takeOwnGone()) {
            leaveBattle();
        }
        m_mainStatus.update(nowMs);
        m_retainFur.update();
        m_piano.update();
        m_express.update();
        m_fishTank.update(nowMs);
        m_battleMain.update(nowMs);
        m_battleSkills.update();
        if (m_battleShown) {
            m_view.setCursor(m_battle.cursor());
        }
        if (m_battle.isOn() && !m_battleShown && m_dimTarget == kDarkStep && m_dimStep == kDarkStep) {
            showBattle();
        }
        if (m_battleClosing && m_dimStep == 0) {
            m_battleClosing = false;
            showFightExpResult();
        }
        if (!m_movie.active()) {
            m_eventRunner.update();
        }
        updateTreasureJump(nowMs);
        constexpr int kWatching = 4; // fight state 4: watching a battle
        m_inputBar.update(nowMs, m_mailBox.envelopeBlinks(), m_fightState == kWatching);
        m_quests.update();
        m_starSkyWindow.update();
        m_hotKeys.update();
        m_cart.update();
        m_talk.update(nowMs);
        m_movie.update(nowMs);
        m_itemWindow.update();
        m_itemWindow.updateSort(nowMs);
        m_bankItems.updateSort(nowMs);
        m_itemBag.updateSort(nowMs);
        m_storeroom.updateSort(nowMs);
        m_picnic.updateSort(nowMs);
        m_expBox.updateSort(nowMs);
        m_storeTank.updateSort(nowMs);
        if (m_services.videoSettings().sortButtons && !m_sortAsked) {
            m_sortAsked = true;
            m_services.serverSorts = false;
            m_services.connection.send(protocol::sortQueryRequest());
        }
        m_rightClickMenu.update();
        m_storeTank.update();
        m_capsule.update();
        m_dismantle.update();
        m_confuseStone.update();
        m_reinforce.update();
        m_magicBox.update();
        m_packing.update();
        m_refer.update();
        m_repair.update();
        m_utensilTrade.update();
        m_utensilStall.update();
        m_buyUtensil.update();
        m_sewingKit.update();
        m_giftExchange.update();
        m_parking.update();
        m_showPlatform.update();
        m_magnet.update();
        m_remote.update(nowMs);
        m_remoteWindow.update();
        m_remotePlus.update();
        m_carCase.update();
        markTeamLeaders();
        m_mall.update();
        m_articleInfo.update();
        m_cupboard.update();
        m_iceBox.update();
        m_pledgeBox.update();
        m_musicCenter.update(nowMs);
        m_gasCan.update();
        m_smallGas.update();
        m_bookShelf.update();
        m_homeAdorn.update();
        m_portrait.update();
        m_computer.update();
        resetTrojanWarFlag();
        m_expBox.checkAutoUse();
        m_doorplate.update();
        m_luckyWheel.update();
        m_luckyCards.update(nowMs);
        m_fortuneEgg.update(nowMs);
        m_slotMachine.update(nowMs);
        if (m_musicDelay > 0 && --m_musicDelay == 0) {
            playMapMusic();
        }
        if (m_map) {
            if (m_fightState == 0) {
                walkWithKeys(nowMs);
            }
            if (!m_services.freeFrames) {
                moveCharacters(nowMs);
            }
            if (m_transmit.update(m_camera.toScreen(m_player.position), nowMs)) {
                m_services.connection.send(protocol::transmitRequest(m_transmit.place()));
            }
            if (const std::vector<std::uint32_t> ids = m_ensigns.takeRequest(nowMs); !ids.empty()) {
                m_services.connection.send(protocol::guildBadgeListRequest(ids));
            }
            if (m_gpsEffect.update(m_camera.toScreen(m_player.position), nowMs)) {
                m_services.connection.send(m_teamFormTeleport ? protocol::castleTeleportRequest() : m_gps.teleportRequest());
                m_teamFormTeleport = false;
            }
            if (m_fightState == 0 && !m_teams.isMember(m_player.id)) {
                checkMoveEvents(nowMs);
            }
        }
        constexpr int kFightMessageTop = 40; // top of the message stack during a battle, px
        constexpr int kWorldMessageTop = 10; // top of the message stack in the world, px
        m_messages.keepBelow(m_fightState != 0 ? kFightMessageTop : kWorldMessageTop);
        m_screen.update(nowMs);
    }

    void WorldScene::paintWorld(gfx::Canvas& canvas, std::uint64_t nowMs) {
        if (m_entering || !m_map || m_activeSport != 0 || m_quiz.isOpen() || m_movie.shown()) {
            if (m_activeSport != 0 && !m_entering && m_map && !m_quiz.isOpen() && !m_movie.shown()) {
                m_effects.drawUnder(canvas, m_camera.origin(), nowMs, effectConditions());
                m_effects.drawOver(canvas, m_camera.origin(), nowMs, effectConditions());
            }
            return;
        }
        std::vector<std::reference_wrapper<MapCharacter>> characters{std::ref(m_player)};
        for (auto& [id, other] : m_others) {
            characters.push_back(std::ref(other));
        }
        // the 9 HumInfo switches: 0 own pet names, 1 other pet names, 2 other pets, 3 own nickname, 4 own title, 5 other names, 6 other nicknames, 7 other titles, 8 tents
        const std::array<int, 9>& shown = m_systemWindow.settings().humInfo;
        WorldPainter::Display display;
        display.ownId = m_player.id;
        for (const auto& [id, other] : m_others) {
            if (m_teams.isMember(id)) {
                display.teamMembers.insert(id);
            }
        }
        display.ownPetNames = shown[0] != 0;
        display.otherPetNames = shown[1] != 0 && shown[2] != 0; // switch 1 (pet names) needs switch 2 (other pets)
        display.otherPets = shown[2] != 0; // switch 2: other pets
        display.ownNickname = shown[3] != 0; // switch 3: own nickname
        display.ownTitle = shown[4] != 0; // switch 4: own title
        display.otherTitles = shown[7] != 0; // switch 7: other titles
        display.otherNames = shown[5] != 0; // switch 5: other names
        display.otherNicknames = shown[6] != 0; // switch 6: other nicknames
        display.onPvpServer = m_services.session.onPvpServer;
        display.humInfo = !m_talk.isMainOpen() && m_dimStep == 0 && !m_darkMode;
        if (m_inWorld && m_screen.hoveredId() == m_view.id()) {
            display.mouse = worldMouse();
        }
        display.showTents = shown[8] != 0; // switch 8: tents
        display.badgeOf = [this](std::uint32_t guildId) -> std::optional<std::reference_wrapper<const formats::EmblemBlock>> {
            if (const auto ensign = m_ensigns.find(guildId)) {
                return std::cref(ensign->get().data);
            }
            return std::nullopt;
        };
        display.trojanWarMap = onTrojanWarMap();
        display.goldEaglePrefix = [this](int ordinal) { return m_goldEagle.prefix(m_map ? m_map->id() : 0, ordinal); };
        display.defenseNpcPrefix = [this](int ordinal) { return m_defenseNpc.prefix(m_map ? m_map->id() : 0, ordinal); };
        display.stoneEaglePrefix = [this](int ordinal) { return m_stoneEagle.prefix(m_map ? m_map->id() : 0, ordinal); };
        for (auto& [id, other] : m_others) {
            other.trojanTarget = trojanWarTarget(other);
        }
        display.mapId = m_map->id();
        display.sceneType = sceneType();
        display.underStanding = [this, nowMs](gfx::Canvas& under) {
            for (const std::int32_t owner :
                 m_vehicleWrecks.draw(under, m_camera.origin(), nowMs, [this](std::int32_t id) { return characterOf(id).has_value(); })) {
                m_services.connection.send(protocol::wreckSeenRequest(owner));
            }
            m_vehicles.drawFlights(under, nowMs);
            paintWeather(under, true, nowMs);
            if (specFloorShown()) {
                m_specFloor.draw(under, ownHouse(), m_camera, specFloorHovered());
            }
        };
        setHouseDisplay(display, nowMs);
        display.hideOwnPlayer = m_transmit.hidesPlayer() || m_gpsEffect.hidesPlayer();
        display.afterPlayer = [this, nowMs](gfx::Canvas& over, std::int32_t playerId) { m_effectLights.draw(over, playerId, m_camera.origin(), nowMs); };
        display.afterOwnPlayer = [this, nowMs](gfx::Canvas& over, core::Point feet) {
            m_transmit.draw(over, nowMs);
            m_timedLight.draw(over, feet, nowMs);
            m_gpsEffect.draw(over, nowMs);
            if (m_makeJobs.petSlot() == 0) {
                paintMakeBar(over, feet);
            }
            paintGuildWork(over, feet);
        };
        display.afterMakerPet = [this](gfx::Canvas& over, core::Point feet) { paintMakeBar(over, feet); };
        display.overStanding = [this, nowMs](gfx::Canvas& over) {
            m_treasureLights.draw(over, m_camera.origin(), nowMs);
            drawFireworks(over);
            if (m_vanish.isActive()) {
                m_vanish.setPosition(m_camera.toScreen(m_vanishAt));
                m_vanish.draw(over, nowMs);
            }
            if (m_wishEffects.draw(over, m_camera.origin(), nowMs, [this](std::int32_t id) -> std::optional<core::Point> {
                    const auto character = characterOf(id);
                    return character ? std::optional<core::Point>(character->get().position) : std::nullopt;
                })) {
                m_blessMessage.setDone();
            }
        };
        refreshCampOwners();
        m_hoveredGround = m_inWorld && m_screen.hoveredId() == m_view.id() ? m_ground.indexAt(worldMouse(), m_camera) : 0;
        const auto overPeople = [&](gfx::Canvas& over) {
            if (m_fightState == 0) {
                for (const int landed : m_flights.update(over, nowMs)) {
                    m_ground.setActive(landed, true);
                }
            }
            m_waymark.paint(over, m_camera, m_inWorld && m_screen.hoveredId() == m_view.id() ? std::optional<core::Point>(worldMouse()) : std::nullopt);
            paintWeather(over, false, nowMs);
            if (const int frame = m_walkMark.update(nowMs); frame != 0) {
                if (const auto arrow = m_services.pictures.find("Arrow" + std::to_string(frame))) {
                    constexpr int kArrowSize = 64; // width and height of the walk-target arrow frame, px
                    const core::Point at = m_camera.toScreen(m_walkMark.position());
                    over.drawPicture(*arrow, {at.x - kArrowSize / 2, at.y - kArrowSize / 2}, core::Rect{0, 0, kArrowSize, kArrowSize});
                }
            }
            if (m_hoveredGround != 0 && display.humInfo) {
                m_ground.drawHint(over, m_hoveredGround, worldMouse());
            }
            if (specFloorHovered() && display.humInfo) {
                m_specFloor.drawHint(over, ownHouse(), worldMouse());
            }
        };
        const bool zoomed = canvas.isZooming();
        if (zoomed) {
            display.beforeLabels = [&](gfx::Canvas& zoomedCanvas) {
                overPeople(zoomedCanvas);
                zoomedCanvas.endZoom(m_camera.view());
            };
            display.onWindow = [part = m_camera.view(), window = m_services.view](core::Point point) { return ViewZoom::toWindow(point, part, window); };
        }
        m_painter.paint(
            canvas,
            *m_map,
            m_camera,
            characters,
            m_npcs,
            m_mapPets.pets(),
            m_camps.camps(),
            m_vehicles.placedVehicles(),
            m_ground,
            m_hoveredGround,
            m_inWorld,
            display,
            nowMs
        );
        if (!zoomed) {
            overPeople(canvas);
        }
        m_screenLight.draw(canvas, nowMs);
        if (m_dimStep == 0) {
            paintLocation(canvas);
            m_remote.paintInformation(canvas);
        }
        if (m_fadeStep > 0) {
            canvas.fillRect(canvas.window(), {0, 0, 0, static_cast<std::uint8_t>(m_fadeStep * kFadeAlphaPerStep)});
        }
    }

    void WorldScene::draw(gfx::Canvas& canvas, std::uint64_t nowMs) {
        followView(nowMs);
        m_battle.beginPresent(nowMs);
        m_screen.draw(canvas, nowMs);
        m_battle.endPresent();
    }

    void WorldScene::move(std::uint64_t nowMs) {
        if (!m_services.freeFrames || !m_map) {
            return;
        }
        m_nowMs = std::max(m_nowMs, nowMs);
        moveCharacters(nowMs);
    }

    bool WorldScene::freeFrames() const { return m_services.freeFrames && m_map && m_inWorld && !m_entering; }

    std::string WorldScene::playerName(std::int32_t playerId) const {
        if (isOwn(playerId)) {
            return m_player.name;
        }
        const auto found = m_others.find(playerId);
        return found != m_others.end() ? found->second.name : std::string();
    }

    TalkDialog::Speaker WorldScene::ownSpeaker(bool portrait) const {
        constexpr int kClothesSlot = 2; // equip slot 2: clothes
        TalkDialog::Speaker speaker{m_player.look, m_player.name, portrait};
        speaker.wide = wearsStarterClothes(m_player.look.bodyType, m_player.look.head, m_inventory.equipped(kClothesSlot).itemId);
        return speaker;
    }

    std::optional<TalkDialog::Speaker> WorldScene::speakerOf(std::int32_t playerId) const {
        if (isOwn(playerId)) {
            return TalkDialog::Speaker{m_player.look, m_player.name};
        }
        const auto found = m_others.find(playerId);
        if (found == m_others.end()) {
            return std::nullopt;
        }
        return TalkDialog::Speaker{found->second.look, found->second.name};
    }

    std::optional<std::reference_wrapper<MapCharacter>> WorldScene::characterOf(std::int32_t playerId) {
        if (isOwn(playerId)) {
            return std::ref(m_player);
        }
        const auto found = m_others.find(playerId);
        if (found == m_others.end()) {
            return std::nullopt;
        }
        return std::ref(found->second);
    }

    void WorldScene::onRebornWishMessage(const core::Bytes& payload) { m_rebornWish.receive(payload); }

    void WorldScene::onPvpVictories(std::int32_t playerId, int wins) {
        if (const auto character = characterOf(playerId)) {
            character->get().pvpWins = wins;
        }
    }

    void WorldScene::onPvpRank(std::int32_t playerId, int rank) {
        if (const auto character = characterOf(playerId)) {
            character->get().pvpRank = rank;
        }
    }

    void WorldScene::onArenaVictory(std::int32_t playerId, int victories) {
        if (const auto character = characterOf(playerId)) {
            character->get().arenaVictory = victories;
        }
    }

    void WorldScene::onFleeKillEvent(int code) {
        if (code == 1) {
            m_fleeKillActive = true;
        } else if (code == 3) { // flee-kill event 3: it ends (1 starts it)
            m_fleeKillActive = false;
        }
        const auto notice = fleeKillNotice(code);
        if (!notice) {
            return;
        }
        if (notice->resetsCursor) {
            setMouseMode(0);
        }
        if (notice->chat) {
            m_chat.add(0, notice->text, m_player.id);
        } else {
            m_messages.show(notice->text, notice->durationMs);
        }
    }

    void WorldScene::onSurvivalVictory(int victories) { m_player.survivalVictory = victories; }

    void WorldScene::teamNotice(std::int32_t playerId, const std::string& text, int durationMs) {
        m_messages.show(text, durationMs);
        m_chat.add(10, text, playerId); // chat channel 10: notice line
    }

    std::optional<TeamMate> WorldScene::teamMate(std::int32_t playerId) const {
        if (playerId == 0) {
            return std::nullopt;
        }
        if (isOwn(playerId)) {
            return TeamMate{
                m_player.name,
                m_player.nickname,
                m_player.look,
                m_player.look,
                m_status.level,
                m_status.reborn,
                m_status.job,
                m_status.element,
                m_status.hp,
                m_status.sp,
                m_status.maxHp,
                m_status.maxSp
            };
        }
        const auto character = m_others.find(playerId);
        const auto record = m_otherRecords.find(playerId);
        if (character == m_others.end() || record == m_otherRecords.end()) {
            return std::nullopt;
        }
        const CharacterLook& look = character->second.look;
        TeamMate mate;
        mate.name = character->second.name;
        mate.nickname = record->second.nickname;
        mate.look = CharacterLook::dressed(look.bodyType, look.head, look.basePattern, {}, m_services.items);
        mate.sceneLook = look;
        mate.level = record->second.level;
        mate.reborn = record->second.reborn;
        mate.job = record->second.job;
        mate.element = record->second.element;
        const MateVitals vitals = m_mateVitals.count(playerId) != 0 ? m_mateVitals.at(playerId) : MateVitals{};
        const int total = totalLevel(mate.level, mate.reborn);
        mate.hp = vitals.hp();
        mate.sp = vitals.sp();
        mate.maxHp = vitals.maxHp(m_services.formula, total);
        mate.maxSp = vitals.maxSp(m_services.formula, total);
        return mate;
    }

    void WorldScene::onWorldPlayerAttribute(std::int32_t playerId, int attribute, std::int32_t value, std::int32_t extra) {
        (void)extra;
        constexpr int kJob = 31; // player attribute 31: job
        constexpr int kLevel = 35; // player attribute 35: level
        constexpr int kReborn = 39; // player attribute 39: rebirth
        if (isOwn(playerId)) {
            return;
        }
        if (const auto record = m_otherRecords.find(playerId); record != m_otherRecords.end()) {
            if (attribute == kLevel) {
                record->second.level = value;
            } else if (attribute == kJob) {
                record->second.job = value;
            } else if (attribute == kReborn) {
                record->second.reborn = value;
            }
        }
        m_mateVitals[playerId].set(attribute, value);
    }

    void WorldScene::teamChanged(std::int32_t playerId, bool inTeam) { m_mapPets.setInTeam(playerId, inTeam); }

    void WorldScene::markTeamLeaders() {
        m_player.teamLeader = m_teams.isLeader(m_player.id);
        for (auto& [id, character] : m_others) {
            character.teamLeader = m_teams.isLeader(id);
        }
    }

    void WorldScene::startTeamMode() {
        if (m_functionButtons.teamMode() == 2) { // team mode 2: the invite cursor is on
            setMouseMode(16); // mouse mode 16: invite to the team
            return;
        }
        if (m_teams.isMember(m_player.id)) {
            m_messages.show("Please leave your team first and then join the other team.", 1000); // message display time in ms
            return;
        }
        setMouseMode(2); // mouse mode 2: ask to join a team
    }

    void WorldScene::setMouseMode(int mode) {
        m_mouseMode = mode;
        switch (mode) {
            case 0: // mouse mode 0: none
                m_view.setCursor(ui::CursorKind::Normal);
                break;
            case 1: // mouse mode 1: PK
                m_view.setCursor(ui::CursorKind::Attack);
                break;
            case 3: // mouse mode 3: watch a fight
                m_view.setCursor(ui::CursorKind::Watch);
                break;
            case 4: // mouse mode 4: join a fight
                m_view.setCursor(ui::CursorKind::Join);
                break;
            case kThrowMouseMode: // mouse mode of throwing an item
                m_view.setCursor(ui::CursorKind::Throw);
                break;
            default: // any other mouse mode: the hand
                m_view.setCursor(ui::CursorKind::Hand);
                break;
        }
    }

    void WorldScene::startBattleMode(int mode) {
        if (m_teams.isMember(m_player.id)) {
            constexpr int kPk = 1; // mouse mode 1: PK
            constexpr int kWatch = 3; // mouse mode 3: watch a battle
            m_messages.show(
                mode == kPk      ? "Only the leader of the team can PK."
                : mode == kWatch ? "The teammate is unable to view battle"
                                 : "The teammate is unable to join battle",
                mode == kPk ? 2000 : 1000
            ); // message display time in ms
            return;
        }
        setMouseMode(mode);
    }

    std::int32_t WorldScene::battleTarget(std::int32_t playerId) const {
        return m_teams.isMember(playerId) ? m_teams.leaderOf(playerId).value_or(playerId) : playerId;
    }

    void WorldScene::askToPk(std::int32_t playerId) {
        const auto found = m_others.find(playerId);
        if (found == m_others.end() || !m_map) {
            return;
        }
        const MapCharacter& target = found->second;
        const auto scene = m_scenes ? m_scenes->find(m_map->id()) : std::nullopt;
        if (scene && (scene->flags & 1) != 0) {
            m_messages.show("Unable to attack other player in this scene", 2000); // message display time in ms
            return;
        }
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return;
        }
        if (target.fishing) {
            m_messages.show("The target is fishing", 2000); // message display time in ms
            return;
        }
        constexpr int kPkReach = 270; // farthest distance on each axis from which a PK can be started, px
        if (std::abs(target.position.x - m_player.position.x) > kPkReach || std::abs(target.position.y - m_player.position.y) > kPkReach) {
            m_messages.show("too far away", 1500); // message display time in ms
            return;
        }
        constexpr int kWatching = 4; // fight state 4: watching a battle
        if (target.fightState == kWatching) {
            m_messages.show("Unable to attack, the target is watching combat.", 1500); // message display time in ms
            return;
        }
        if (!m_systemWindow.pkAllowed()) {
            m_messages.show("Unable to PK other players after turning off PK.", 1000); // message display time in ms
            return;
        }
        if (target.stall) {
            m_messages.show("unable to PK with players who are setting a stall.", 1000); // message display time in ms
            return;
        }
        if (m_teams.isMember(playerId) && m_teams.leaderOf(playerId) == m_player.id) {
            m_messages.show("Unable to PK with teammate", 1000); // message display time in ms
            return;
        }
        constexpr int kPk = 2; // battle request kind 2: PK a player
        constexpr int kJoin = 5; // battle request kind 5: join a running battle
        m_services.connection.send(protocol::battleRequest(target.fightState != 0 ? kJoin : kPk, battleTarget(playerId), 0));
    }

    void WorldScene::askToJoinFight(std::int32_t playerId) {
        const auto found = m_others.find(playerId);
        if (found == m_others.end() || found->second.fightState == 0) {
            return;
        }
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return;
        }
        constexpr int kNear = 170; // farthest distance on each axis from which a battle can be joined, px
        const MapCharacter& target = found->second;
        if (std::abs(target.position.x - m_player.position.x) > kNear || std::abs(target.position.y - m_player.position.y) > kNear) {
            m_messages.show("too far away", 1500); // message display time in ms
            return;
        }
        constexpr int kJoin = 5; // battle request kind 5: join a running battle
        m_services.connection.send(protocol::battleRequest(kJoin, battleTarget(playerId), 0));
    }

    void WorldScene::askToWatchFight(std::int32_t playerId) {
        const auto found = m_others.find(playerId);
        if (found == m_others.end() || found->second.fightState == 0) {
            return;
        }
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return;
        }
        constexpr int kWatch = 4; // battle request kind 4: watch a running battle
        m_services.connection.send(protocol::battleRequest(kWatch, battleTarget(playerId), 0));
    }

    void WorldScene::askToFightNpc(int npcIndex) {
        const auto scene = m_scenes && m_map ? m_scenes->find(m_map->id()) : std::nullopt;
        auto npc = findNpc(npcIndex);
        if (!npc || (scene && (scene->flags & 1) != 0)) {
            return;
        }
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return;
        }
        if (npc->get().info.noPk == 1) {
            m_messages.show("PK is prohibited", 1200); // message display time in ms
            return;
        }
        constexpr int kMapNpc = 3; // battle request kind 3: fight a map NPC
        m_services.connection.send(protocol::battleRequest(kMapNpc, npc->get().info.npcId, npcIndex));
    }

    std::optional<std::int32_t> WorldScene::playerAt(core::Point screenPoint) {
        std::optional<std::int32_t> found;
        int foundY = 0;
        for (auto& [id, other] : m_others) {
            const core::Rect area = m_services.characters.figureFrameArea(other.look, m_camera.toScreen(other.position), other.actionSlot());
            if (area.contains(screenPoint) && (!found || other.position.y > foundY)) {
                found = id;
                foundY = other.position.y;
            }
        }
        return found;
    }

    bool WorldScene::canTeamUpWith(std::int32_t playerId, bool joining) {
        if (!m_systemWindow.partyAllowed()) {
            m_messages.show("Party Option disabled, unable to form Party", 1200); // message display time in ms
            return false;
        }
        const auto target = characterOf(playerId);
        if (!target || !m_map) {
            return false;
        }
        const MapCharacter& other = target->get();
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 2000); // message display time in ms
            return false;
        }
        if (m_autoCollecting) {
            m_messages.show("Collect automatically, cancel the movement", 2000); // message display time in ms
            return false;
        }
        if (other.fishing) {
            m_messages.show("The target is fishing", 2000); // message display time in ms
            return false;
        }
        if (other.autoCollecting) {
            m_messages.show("The target is collecting automatically", 2000); // message display time in ms
            return false;
        }
        const int partners = static_cast<int>(m_teams.members(playerId).size());
        if (joining) {
            if (other.vehicle) {
                if (m_player.riding) {
                    m_messages.show("Unable to use vehicle while riding a pet", 1500); // message display time in ms
                    return false;
                }
                if (partners + 1 >= VehicleKinds::seatCount(other.vehicle->kind)) {
                    m_messages.show("The loading number reaches the upper limit", 1500); // message display time in ms
                    return false;
                }
            }
            if (m_player.vehicle) {
                m_messages.show("Please cancel vehicle first", 1500); // message display time in ms
                return false;
            }
        } else {
            if (other.vehicle) {
                m_messages.show("The player has used a vehicle", 1500); // message display time in ms
                return false;
            }
            if (m_player.vehicle && other.riding) {
                m_messages.show("The player has ridden a pet", 1500); // message display time in ms
                return false;
            }
            if (m_player.vehicle && static_cast<int>(m_teams.members(m_player.id).size()) + 1 >= VehicleKinds::seatCount(m_player.vehicle->kind)) {
                m_messages.show("The loading number reaches the upper limit", 1500); // message display time in ms
                return false;
            }
        }
        const core::Point at = target->get().position;
        if (at != m_player.position && findPath(*m_map, m_player.position, at, m_player.walkClass()).empty()) {
            m_messages.show("Unable to teamup", 1500); // message display time in ms
            return false;
        }
        return true;
    }

    void WorldScene::askToJoin(std::int32_t playerId) {
        if (m_teams.inTeam(m_player.id)) {
            return;
        }
        const std::int32_t target = m_teams.leaderOf(playerId).value_or(playerId);
        if (!canTeamUpWith(target, true)) {
            return;
        }
        m_teamTarget = target;
        m_services.connection.send(protocol::joinTeamRequest(target));
    }

    void WorldScene::inviteToTeam(std::int32_t playerId) {
        if (m_teams.isMember(m_player.id) || m_teams.inTeam(playerId) || !canTeamUpWith(playerId, false)) {
            return;
        }
        m_teamTarget = playerId;
        m_services.connection.send(protocol::inviteRequest(playerId));
    }

    void WorldScene::followLeaders(std::uint64_t nowMs) {
        for (const auto& [leaderId, members] : m_teams.all()) {
            const auto leader = characterOf(leaderId);
            if (!leader || !leader->get().walking) {
                continue;
            }
            for (std::size_t i = 0; i < members.size(); ++i) {
                const auto found = characterOf(members[i]);
                if (!found) {
                    continue;
                }
                MapCharacter& member = found->get();
                const core::Point goal = leader->get().vehicle ? leader->get().position : m_trails.point(leaderId, i + 1);
                const bool heading = member.walking && !member.path.empty() && member.path.back() == goal;
                if (!heading && member.position != goal) {
                    if (isOwn(members[i])) {
                        stopOwnAction();
                    }
                    startWalking(member, {goal}, nowMs);
                }
            }
        }
    }

    void WorldScene::onTeamJoinRequest(std::int32_t requesterId) {
        const auto speaker = speakerOf(requesterId);
        if (!speaker) {
            return;
        }
        const auto answer = [this, requesterId](int value) { m_services.connection.send(protocol::joinAnswerRequest(value, requesterId)); };
        m_talk.ask(
            "Please let me join your team.",
            speaker,
            [this, answer, requesterId] {
                if (m_teams.members(m_player.id).size() >= Teams::kMostMembers) {
                    return;
                }
                const auto requester = characterOf(requesterId);
                if (m_player.vehicle && VehicleKinds::seatCount(m_player.vehicle->kind) <= static_cast<int>(m_teams.members(m_player.id).size()) + 1) {
                    m_messages.show("The loading number reaches the upper limit", 1500); // message display time in ms
                } else if (requester && !isOwn(requesterId) && requester->get().vehicle) {
                    m_messages.show("The player has used a vehicle", 1500); // message display time in ms
                } else if (m_player.vehicle && requester && requester->get().riding) {
                    m_messages.show("The player has ridden a pet", 1500); // message display time in ms
                } else {
                    answer(1);
                }
            },
            [answer] { answer(2); }, // answer 2: refused
            {32, 5000, [this, answer] { // Timing: wrap at 32 chars, time out after 5000 ms
                 m_messages.show("You haven't answer the message for a long time. It is now closed.", 1000); // message display time in ms
                 answer(3); // answer 3: no response
             }}
        );
    }

    void WorldScene::onTeamBusy() { m_messages.show("The target is busy now.", 2000); } // message display time in ms

    void WorldScene::onTeamJoinAnswer(int answer, std::int32_t leaderId) {
        if (leaderId != m_teamTarget) {
            return;
        }
        const std::string name = playerName(leaderId);
        if (answer == 1) {
            m_messages.show(name + " Accept you", 1000); // message display time in ms
        } else if (answer == 2) { // answer 2: refused
            m_messages.show(name + " Refuse you", 1000); // message display time in ms
        } else if (answer == 3) { // answer 3: no response
            m_messages.show(name + "did not respond", 1000); // message display time in ms
        }
    }

    void WorldScene::onTeamLeft(std::int32_t playerId) {
        const auto ownLeader = m_teams.leaderOf(m_player.id);
        const std::vector<std::int32_t> before = m_teams.leave(playerId);
        if (before.empty()) {
            return;
        }
        const auto driver = characterOf(before.front());
        if (driver && driver->get().vehicle) {
            unloadPassengers(driver->get());
        }
        m_groupWindow.playerLeft(playerId, isOwn(playerId));
        for (const std::int32_t id : before) {
            if (!m_teams.inTeam(id)) {
                if (const auto mate = characterOf(id)) {
                    mate->get().fanIcon = false;
                }
            }
        }
        const std::int32_t leaderId = before.front();
        const bool ownTeam = ownLeader && *ownLeader == leaderId;
        if (playerId == leaderId) {
            if (ownTeam) {
                teamNotice(playerId, playerName(leaderId) + " Disband the team", 1200); // notice display time in ms
            }
            for (std::int32_t id : before) {
                teamChanged(id, false);
            }
        } else {
            if (ownTeam) {
                teamNotice(playerId, playerName(playerId) + " Left the team", 1200); // notice display time in ms
            }
            teamChanged(playerId, false);
            if (!m_teams.inTeam(leaderId)) {
                teamChanged(leaderId, false);
            }
        }
        if (isOwn(playerId) || !m_teams.inTeam(m_player.id)) {
            m_functionButtons.setTeamMode(1);
        }
        if (playerId != leaderId && driver && driver->get().vehicle) {
            loadPassengers(driver->get());
        }
        m_groupWindow.refresh();
    }

    void WorldScene::onTeamJoined(std::int32_t leaderId, std::int32_t memberId) {
        const auto leader = characterOf(leaderId);
        const auto member = characterOf(memberId);
        if (!leader || !member || !m_teams.join(leaderId, memberId)) {
            return;
        }
        teamChanged(leaderId, true);
        teamChanged(memberId, true);
        MapCharacter& body = member->get();
        body.position = leader->get().position;
        startWalking(body, {}, m_nowMs);
        if (leader->get().vehicle) {
            loadPassengers(leader->get());
        }
        m_trails.at(memberId, body.position, true);
        if (isOwn(memberId) && m_map) {
            m_camera.centerOn(body.position, m_map->width(), m_map->height());
        }
        if (m_teams.leaderOf(m_player.id) == leaderId) {
            teamNotice(memberId, "welcome [" + playerName(memberId) + "] to join into the team", 2000); // notice display time in ms
        }
        if (isOwn(leaderId)) {
            m_functionButtons.setTeamMode(2); // team mode 2: the invite cursor mode
        }
        m_groupWindow.refresh();
    }

    void WorldScene::onSceneTeams(const std::vector<protocol::SceneTeam>& teams) {
        for (const protocol::SceneTeam& team : teams) {
            m_teams.set(team);
            teamChanged(team.leaderId, true);
            for (std::int32_t member : team.memberIds) {
                teamChanged(member, true);
            }
            if (const auto leader = characterOf(team.leaderId); leader && leader->get().vehicle) {
                loadPassengers(leader->get());
            }
        }
        m_groupWindow.refresh();
    }

    void WorldScene::onTeamInvite(std::int32_t inviterId) {
        if (m_services.joinsTeamOf(inviterId)) {
            if (!m_teams.inTeam(m_player.id)) {
                m_services.connection.send(protocol::inviteAnswerRequest(1, inviterId));
            }
            return;
        }
        const auto speaker = speakerOf(inviterId);
        if (!speaker) {
            return;
        }
        const auto answer = [this, inviterId](int value) { m_services.connection.send(protocol::inviteAnswerRequest(value, inviterId)); };
        m_talk.ask(
            "Would you like to join my team? It would be my honor.",
            speaker,
            [this, answer, inviterId] {
                if (m_teams.inTeam(m_player.id)) {
                    return;
                }
                const auto inviter = characterOf(inviterId);
                if (m_player.vehicle) {
                    m_messages.show("Please cancel vehicle first", 1200); // message display time in ms
                } else if (
                    inviter && inviter->get().vehicle &&
                    VehicleKinds::seatCount(inviter->get().vehicle->kind) <= static_cast<int>(m_teams.members(inviterId).size()) + 1
                ) {
                    m_messages.show("The loading number reaches the upper limit", 1500); // message display time in ms
                } else if (inviter && inviter->get().vehicle && m_player.riding) {
                    m_messages.show("The player has used a vehicle", 1500); // message display time in ms
                } else {
                    answer(1);
                }
            },
            [answer] { answer(2); }, // answer 2: refused
            {32, 5000, [this, answer] { // Timing: wrap at 32 chars, time out after 5000 ms
                 m_messages.show("You haven't answer the message for a long time. It is now closed.", 1000); // message display time in ms
                 answer(3); // answer 3: no response
             }}
        );
    }

    void WorldScene::onTeamInviteAnswer(int answer, std::int32_t inviteeId) {
        if (m_others.count(inviteeId) == 0) {
            return;
        }
        const std::string name = playerName(m_teamTarget);
        if (answer == 1) {
            m_messages.show(name + "Accept your invitation", 1000); // message display time in ms
        } else if (answer == 2) { // answer 2: refused
            m_messages.show(name + "Refuse your invitation", 1000); // message display time in ms
        } else if (answer == 3) { // answer 3: no response
            m_messages.show(name + "did not respond", 1000); // message display time in ms
        }
    }

    void WorldScene::startFriendMode() {
        constexpr int kNoFriendsMap = 10072; // map 10072: Survival Isle (friend requests refused)
        if (m_map && m_map->id() == kNoFriendsMap) {
            m_messages.show("Cannot use this function", 1000); // message display time in ms
            return;
        }
        if (m_friends.count() >= FriendList::kMostFriends) {
            return;
        }
        setMouseMode(5); // mouse mode 5: ask to be friends
    }

    void WorldScene::askToBeFriends(std::int32_t playerId) {
        if (m_player.fishing) {
            m_messages.show("Fishing, cancel the action", 1500); // message display time in ms
            return;
        }
        if (isGmId(playerId)) {
            m_messages.show("You are unable to add him to your friends.", 1500); // message display time in ms
            return;
        }
        if (m_friends.isFriend(playerId)) {
            m_messages.show("This player is already your friend.", 2000); // message display time in ms
            return;
        }
        m_services.connection.send(protocol::addFriendRequest(playerId));
    }

    protocol::FriendRecord WorldScene::friendRecordOf(std::int32_t playerId) const {
        protocol::FriendRecord record;
        record.id = playerId;
        const auto found = m_otherRecords.find(playerId);
        if (found == m_otherRecords.end()) {
            return record;
        }
        const protocol::OtherPlayerRecord& player = found->second;
        formats::ColorPattern colors;
        colors.apply(player.appearance.colorCode1, 1);
        colors.apply(player.appearance.colorCode2, 5); // colour code 2 starts at digit 5 of the pattern
        const auto pack = [&colors](std::initializer_list<std::size_t> digits) {
            std::int32_t code = 0;
            for (std::size_t digit : digits) {
                code = code * 10 + colors.digit(digit); // decimal digits
            }
            return code;
        };
        record.name = player.name;
        record.level = player.level;
        record.job = player.job;
        record.reborn = player.reborn;
        record.element = player.element;
        record.bodyType = player.appearance.bodyType;
        record.head = player.appearance.head;
        // the pattern digits that make up friend colour codes 1 and 5
        record.colorCode1 = pack({0, 1, 2, 3, 4, 5, 6, 7, 8});
        record.colorCode5 = pack({9, 10, 11, 39, 40, 41, 24, 25, 26});
        record.nickname = player.nickname;
        return record;
    }

    int WorldScene::worldOnlineState(std::int32_t playerId) const {
        const auto found = m_otherRecords.find(playerId);
        return found != m_otherRecords.end() ? found->second.onlineStatus : 3; // online state 3: offline
    }

    void WorldScene::onFriendRequest(std::int32_t playerId) {
        const auto answer = [this, playerId](int value) { m_services.connection.send(protocol::friendAnswerRequest(playerId, value)); };
        const auto refuse = [this, answer] {
            m_services.playSound("sound/wav0151.wav");
            answer(2); // answer 2: refused
        };
        m_talk.ask(
            "Can I be your friend?",
            speakerOf(playerId),
            [this, answer, refuse] {
                if (m_friends.count() < FriendList::kMostFriends) {
                    answer(1);
                } else {
                    m_messages.show("Unable to add because you have too many friends", 2000); // message display time in ms
                    refuse();
                }
            },
            refuse,
            {32, 10000, [answer] { answer(3); }, false}
        ); // Timing: wrap at 32 chars, time out after 10000 ms with answer 3 (no response)
    }

    void WorldScene::onFriendAnswer(std::int32_t playerId, int answer) {
        const std::string name = playerName(playerId);
        if (answer == 1) {
            m_friends.add(friendRecordOf(playerId));
            m_messages.show("Player " + name + " Accept the friends' invitation", 2000); // message display time in ms
        } else if (answer == 2) { // answer 2: refused
            m_messages.show("Player " + name + " Refuse to add the friend", 2000); // message display time in ms
        } else if (answer == 3) { // answer 3: no response
            m_messages.show("Player " + name + "No response", 2000); // message display time in ms
        } else {
            m_messages.show("Player " + name + "The target is busy now.", 2000); // message display time in ms
        }
    }

    void WorldScene::onFriendRemoved(std::int32_t friendId) {
        if (const auto name = m_friends.remove(friendId)) {
            m_messages.show("Player " + *name + " Exit from the friend list", 2000); // message display time in ms
        }
    }

    void WorldScene::onFriendList(const std::vector<protocol::FriendRecord>& friends) {
        m_friends.replace(friends);
        importMailLogs(MailLogImporter::Kind::Friend);
    }

    void WorldScene::importMailLogs(MailLogImporter::Kind kind) {
        constexpr std::int32_t kMailFolderKey = 121; // XOR key that scrambles the player id in the mail log file name
        const int serverFolder = m_services.session.serverGrouped ? m_services.session.serverId / 10 * 10
                                                                  : m_services.session.serverId; // grouped servers share the folder of their ten
        m_mailBox.importLogs(m_services.game.userRoot() / ("Server" + std::to_string(serverFolder)), m_player.id ^ kMailFolderKey, kind);
    }

    void WorldScene::onGuildName(std::int32_t playerId, const std::string& guild) { m_friends.setGuild(playerId, guild); }

    void WorldScene::onFriendInfo(const protocol::FriendRecord& record) { m_friends.update(record); }

    void WorldScene::onFriendStates(const std::vector<protocol::FriendState>& states) {
        for (const protocol::FriendState& state : states) {
            m_friends.setOnlineState(state.id, state.state);
        }
    }

    void WorldScene::onFriendEntry(const protocol::FriendRecord& record) { m_friends.put(record); }

    void WorldScene::onFriendListFull() { m_messages.show("Target's friends list is full", 1500); } // message display time in ms

    void WorldScene::onMail(std::int32_t senderId, double sendTime, const std::string& text) {
        m_mailBox.receive(senderId, sendTime, text);
        m_services.playSound("sound/wav0155.wav");
    }

    void WorldScene::onMailboxFull() { m_messages.show("The target's mailbox is full!!", 2000); } // message display time in ms

    void WorldScene::onSentMailFull() { m_messages.show("Letters sent have been full", 2000); } // message display time in ms

    void WorldScene::onFurMail(std::int32_t senderId, const std::string& senderName, double sendTime, const std::string& text) {
        m_mailBox.receive(senderId, sendTime, text, 0, senderName);
        m_services.playSound("sound/wav0155.wav");
    }

    void WorldScene::onFurMailResult(int code) { m_furMail.result(code); }

    void WorldScene::onFurMailBox() { m_furMail.openAtMailbox(); }

    void WorldScene::onStoreTank(int kind, int row, int itemId, int count) { m_storeTank.received(kind, row, itemId, count); }

    void WorldScene::onServerSort(int op, int grid, int result) {
        if (op == 0) {
            m_services.serverSorts = true;
            return;
        }
        if (op == 1 && static_cast<protocol::SortGrid>(grid) == protocol::SortGrid::Bag) {
            for (int slot = 1; slot <= Inventory::kBagSlots; ++slot) {
                m_inventory.takeFromBag(slot, Inventory::kMaxStack);
            }
            inventoryChanged();
            return;
        }
        if (op != 2) { // sort op 2: the answer for a grid
            return;
        }
        switch (static_cast<protocol::SortGrid>(grid)) {
            case protocol::SortGrid::Bag: m_itemWindow.sortAnswered(result); break; // sort grid: the bag
            case protocol::SortGrid::Bank: m_bankItems.sortAnswered(result); break; // sort grid: the bank items
            case protocol::SortGrid::ItemBag: m_itemBag.sortAnswered(result); break; // sort grid: the item bag
            case protocol::SortGrid::Storeroom: m_storeroom.sortAnswered(result); break; // sort grid: the storeroom
            case protocol::SortGrid::Picnic: m_picnic.sortAnswered(result); break; // sort grid: the picnic box
            case protocol::SortGrid::ExpBag: m_expBox.sortAnswered(result); break; // sort grid: the experience bag
            case protocol::SortGrid::StoreTank: m_storeTank.sortAnswered(result); break; // sort grid: the store tank
            default: break; // any other grid: ignored
        }
    }

    void WorldScene::toggleFurMail() {
        constexpr int kNoFormMap = 10701; // map 10701: furniture mail form refused (name not in the tables)
        if (m_map && m_map->id() == kNoFormMap) {
            m_messages.show("Unable to use the function here.", 1200); // message display time in ms
        } else if (m_furMail.isOpen()) {
            m_furMail.hide();
        } else {
            m_furMail.show();
        }
    }

    void WorldScene::placeMailForms(core::Point position) {
        m_sendMail.setPosition(position);
        m_furMail.setPosition(position);
        m_mailBox.placeReadForm(position);
    }

    void WorldScene::playEmotion(int number) {
        constexpr int kEmotionBase = 100; // emotion ids on the wire are the emote number plus 100
        m_player.emotion.show(kEmotionBase + number, m_nowMs);
        m_services.connection.send(protocol::emotionRequest(kEmotionBase + number));
    }

    void WorldScene::playAction(int number) {
        const int group = emoteActionGroup(number);
        if (m_moveLocked || m_eventRunner.isRunning() || m_player.riding || m_player.vehicle) {
            return;
        }
        startAction(m_player, group);
        m_services.connection.send(protocol::actionRequest(m_player.actionSlot()));
    }

    void WorldScene::stopOwnAction() {
        if (stopAction(m_player)) {
            m_services.connection.send(protocol::stopActionRequest());
        }
    }

    void WorldScene::onAction(std::int32_t playerId, int slot) {
        if (const auto character = characterOf(playerId); character && !character->get().riding && !character->get().vehicle) {
            startActionSlot(character->get(), slot);
        }
    }

    std::optional<int> WorldScene::nearestHeart(int heartNumber) const {
        std::optional<int> nearest;
        for (const auto& [id, character] : m_others) {
            const auto record = m_otherRecords.find(id);
            if (record == m_otherRecords.end() || record->second.heartNumber + heartNumber != 9 || // heart numbers of a pair add up to 9
                character.look.bodyType % 2 == m_player.look.bodyType % 2) {
                continue;
            }
            const int distance = std::abs(character.position.x - m_player.position.x) + std::abs(character.position.y - m_player.position.y);
            if (!nearest || distance < *nearest) {
                nearest = distance;
            }
        }
        return nearest;
    }

    void WorldScene::onNickname(std::int32_t playerId, const std::string& nickname) {
        if (const auto character = characterOf(playerId)) {
            character->get().nickname = nickname;
        }
        if (const auto record = m_otherRecords.find(playerId); record != m_otherRecords.end()) {
            record->second.nickname = nickname;
        }
        if (const int index = m_friends.indexOf(playerId)) {
            m_friends.at(index).record.nickname = nickname;
        }
    }

    void WorldScene::onHeartNumber(std::int32_t playerId, int heartNumber) {
        if (const auto record = m_otherRecords.find(playerId); record != m_otherRecords.end()) {
            record->second.heartNumber = heartNumber;
        }
    }

    void WorldScene::onCompoundResult(int itemId, int bagSlot) { m_compoundWindow.showResult(itemId, bagSlot); }

    void WorldScene::onTradeRefused(int reason) {
        switch (reason) {
            case 1: // 25/43 reason 1: a player is below level 10
                m_messages.show("Someone less than level 10", 2000); // message display time in ms
                break;
            case 2: // 25/43 reason 2: a player has not been online long enough
                m_messages.show("Someone doesn't have enough time, cannot trade\xA1I", 2000); // message display time in ms
                break;
            case 3: // 25/43 reason 3: the target's online time is not enough
                m_messages.show("The online time of the target is not enough", 2000); // message display time in ms
                break;
            case 4: // 25/43 reason 4: the target is below level 10
                m_messages.show("The target player is not level 10, cannot trade with him", 2000); // message display time in ms
                break;
            default: // other reasons: no message
                break;
        }
    }

    void WorldScene::onTradeOnlineTime(bool enough) {
        m_enoughOnlineTime = enough;
        m_moneyMenu.setEnoughOnlineTime(enough);
    }

    bool WorldScene::mayTradeWith(std::int32_t playerId) {
        constexpr int kLeastLevel = 10; // lowest level that may trade
        if (const auto target = m_others.find(playerId); target != m_others.end() && target->second.fishing) {
            m_messages.show("The target is fishing", 2000); // message display time in ms
            return false;
        }
        if (m_player.stall) {
            m_messages.show("Operation canceled because you are keeping a stall", 2000); // message display time in ms
            return false;
        }
        if (const auto target = m_others.find(playerId); target != m_others.end() && target->second.stall) {
            m_messages.show("Operation canceled because the other party is keeping a stall", 2000); // message display time in ms
            return false;
        }
        if (m_player.vehicle) {
            m_messages.show("Put away transportation vehicle before trading\xA1I", 1200); // message display time in ms
            return false;
        }
        const auto record = m_otherRecords.find(playerId);
        if (record != m_otherRecords.end() && record->second.level < kLeastLevel) {
            m_messages.show("The player less than level 10", 1500); // message display time in ms
            return false;
        }
        if (isGmId(playerId)) {
            m_messages.show("Unable to trade with Cupid", 1200); // message display time in ms
            return false;
        }
        return true;
    }

    void WorldScene::askToTradeItems(std::int32_t playerId) {
        if (mayTradeWith(playerId)) {
            m_services.connection.send(protocol::itemTradeRequest(playerId));
        }
    }

    void WorldScene::askToTradePets(std::int32_t playerId) {
        if (mayTradeWith(playerId)) {
            m_services.connection.send(protocol::petTradeRequest(playerId));
        }
    }

    void WorldScene::onPetTradeStart(std::int32_t partnerId) {
        const auto found = m_others.find(partnerId);
        if (found != m_others.end()) {
            m_petTrade.open(m_player.name, found->second.name);
        }
    }

    void WorldScene::onPetTradeResult(int result) { m_petTrade.showResult(result); }

    void WorldScene::onPetTradeOffer(const protocol::PetTradeOffer& offer) { m_petTrade.showPartnerOffer(offer); }

    void WorldScene::onItemTradeStart(std::int32_t partnerId) {
        const auto found = m_others.find(partnerId);
        if (found != m_others.end()) {
            m_itemTrade.open(found->second.name);
        }
    }

    void WorldScene::onItemTradeResult(int result) { m_itemTrade.showResult(result); }

    void WorldScene::onItemTradeOffer(const protocol::TradeOffer& offer) { m_itemTrade.showPartnerOffer(offer); }

    void WorldScene::onOwnItemStall(const protocol::OwnStall& stall) {
        m_player.stall = StallSign{StallSign::Kind::Items, stall.name, stall.sign};
        for (const protocol::OwnStall::Price& price : stall.prices) {
            m_bagMarks.setPriced(price.bagSlot, true);
        }
        m_itemStall.stallOpened(stall);
        bagMarksChanged();
    }

    void WorldScene::onItemStalls(const std::vector<protocol::StallSignRecord>& stalls) {
        for (const protocol::StallSignRecord& record : stalls) {
            const auto found = m_others.find(record.playerId);
            if (found != m_others.end()) {
                found->second.stall = StallSign{StallSign::Kind::Items, record.name, record.sign};
            }
        }
    }

    void WorldScene::onItemStallClosed(std::int32_t playerId) {
        if (playerId == m_player.id) {
            m_player.stall.reset();
            m_bagMarks.clearPriced();
            m_itemStall.stallClosed();
            bagMarksChanged();
            return;
        }
        const auto found = m_others.find(playerId);
        if (found != m_others.end()) {
            found->second.stall.reset();
            if (m_stallGoods.isOpen() && m_stallGoods.ownerId() == playerId) {
                m_stallGoods.close();
                m_messages.show("The target has pulled his stall", 2000); // message display time in ms
            }
        }
    }

    void WorldScene::onOwnPetStall(const protocol::OwnStall& stall) {
        m_player.stall = StallSign{StallSign::Kind::Pets, stall.name, stall.sign};
        m_petStall.stallOpened();
    }

    void WorldScene::onPetStalls(const std::vector<protocol::StallSignRecord>& stalls) {
        for (const protocol::StallSignRecord& record : stalls) {
            const auto found = m_others.find(record.playerId);
            if (found != m_others.end()) {
                found->second.stall = StallSign{StallSign::Kind::Pets, record.name, record.sign};
            }
        }
    }

    void WorldScene::onPetStallClosed(std::int32_t playerId) {
        if (playerId == m_player.id) {
            m_player.stall.reset();
            m_petStall.stallClosed();
            return;
        }
        const auto found = m_others.find(playerId);
        if (found != m_others.end()) {
            found->second.stall.reset();
            if (m_stallPets.isOpen() && m_stallPets.ownerId() == playerId) {
                m_stallPets.close();
                m_messages.show("The target has pulled his stall", 2000); // message display time in ms
            }
        }
    }

    void WorldScene::onPetStallGoods(const protocol::PetStallGoods& goods) { m_stallPets.show(goods); }

    void WorldScene::onPetStallSale(const protocol::StallSale& sale) { m_petStall.showSale(sale); }

    void WorldScene::onStallGoods(const protocol::StallGoods& goods) {
        const auto found = m_others.find(goods.ownerId);
        if (found == m_others.end()) {
            return;
        }
        m_stallGoods.show(goods, found->second.name, found->second.stall ? found->second.stall->name : std::string());
    }

    void WorldScene::onStallSale(const protocol::StallSale& sale) { m_itemStall.showSale(sale); }

    std::optional<std::int32_t> WorldScene::stallPlateAt(core::Point screenPoint) {
        if (m_player.stall && m_painter.stallPlateArea(m_camera.toScreen(m_player.position)).contains(screenPoint)) {
            return m_player.id;
        }
        for (const auto& [id, other] : m_others) {
            if (other.stall && m_painter.stallPlateArea(m_camera.toScreen(other.position)).contains(screenPoint)) {
                return id;
            }
        }
        return std::nullopt;
    }

    void WorldScene::openStallOf(std::int32_t playerId) {
        if (playerId == m_player.id) {
            if (m_player.stall && m_player.stall->kind == StallSign::Kind::Items) {
                m_itemStall.open();
            } else if (m_player.stall && m_player.stall->kind == StallSign::Kind::Pets) {
                m_petStall.open();
            } else if (m_player.stall && m_player.stall->kind == StallSign::Kind::Furniture) {
                m_utensilStall.open();
            }
            return;
        }
        const auto found = m_others.find(playerId);
        if (found == m_others.end() || !found->second.stall) {
            return;
        }
        if (found->second.stall->kind == StallSign::Kind::Items) {
            m_services.connection.send(protocol::browseItemStallRequest(playerId));
        } else if (found->second.stall->kind == StallSign::Kind::Pets) {
            m_services.connection.send(protocol::browsePetStallRequest(playerId));
        } else if (found->second.stall->kind == StallSign::Kind::Furniture) {
            m_services.connection.send(protocol::browseUtensilStallRequest(playerId));
        }
    }

    int WorldScene::sceneFlags() const {
        const auto scene = m_scenes && m_map ? m_scenes->find(m_map->id()) : std::nullopt;
        return scene ? scene->flags : 0;
    }

    void WorldScene::onPasswordChanged(int result) { m_changePassword.showResult(result); }

    void WorldScene::onBlacklist(int result, const std::vector<protocol::BlacklistEntry>& entries) { m_blacklist.update(result, entries); }

    void WorldScene::onBlockedBy(int code) {
        if (code >= 1 && code <= 9) { // block codes 1..9 are all told the same way
            m_messages.show("You are blocked by this player", 2000); // message display time in ms
        }
    }

    void WorldScene::onSwitches(const std::array<int, 4>& switches, int channels) {
        m_systemWindow.setServerStates(switches, channels);
    } // the 4 server switches

    void WorldScene::onPlayerRefused(int reason) {
        static const char* const kReasons[] = {
            "",
            "The target hasn't turned on PK mode.",
            "The target hasn't turned on battle view.",
            "Unable to watch combat while answering  the North Star God's questions",
            "Unable to watch the special battle",
            "Unable to watch the battle if you are not an ally during city gate battle",
            "Unable to watch the battle if you are not in the same group during city gate battle",
            "The player is in the hotel.",
            "The target is busy now.",
            "Viewing battle is forbidden during this combat",
            "Player has not enabled Party Option",
        };
        if (reason >= 1 && reason <= 10) { // reasons 1..10 index the table above
            m_messages.show(kReasons[reason], 2000); // message display time in ms
        }
        setMouseMode(0);
    }

    void WorldScene::onReturnSpotRecorded(int code) {
        if (code != 0) {
            m_returnSpotRecorded = code == 1;
        }
    }

    void WorldScene::returnToSpot() {
        constexpr int kHookMap = 10009; // map 10009: Maze (return refused while hooking)
        constexpr int kPrisonMap = 10004; // map 10004: Prison
        constexpr int kNoReturnFlag = 4; // scene flag bit 4: returning to the revival spot is not allowed
        const int map = m_map ? m_map->id() : 0;
        if (map == kHookMap) {
            m_messages.show("Return unvailable while in hook", 1500); // message display time in ms
            return;
        }
        if (map == kPrisonMap) {
            m_messages.show("Return unavailble while in prison", 1500); // message display time in ms
            return;
        }
        if (m_eventRunner.isRunning()) {
            m_messages.show("Return unavilable while implementing", 1500); // message display time in ms
            return;
        }
        if (m_player.stall) {
            m_messages.show("Return unavilable while keeping a stall", 1500); // message display time in ms
            return;
        }
        if (m_petTrade.isOpen() || m_itemTrade.isOpen()) {
            m_messages.show("Return unavilable while trading", 2000); // message display time in ms
            return;
        }
        if ((sceneFlags() & kNoReturnFlag) != 0) {
            m_messages.show("Unable to return to this place in this scene", 2000); // message display time in ms
            return;
        }
        TalkDialog::Timing timing;
        timing.wrapChars = 32; // wrap the text at 32 characters
        timing.exclusive = false;
        m_talk.choose(
            "To which revival spot",
            TalkDialog::Speaker{m_player.look, m_player.name},
            {"Starter's beach ", "Recorded spot ", "Carnie "},
            [this](int choice) { chooseReturnSpot(choice); },
            [] {},
            std::move(timing)
        );
    }

    void WorldScene::chooseReturnSpot(int choice) {
        constexpr int kCarnivalMap = 11094; // map 11094: Carnie
        constexpr int kSeaScene = 14; // scene type 14: water (sea) scene
        constexpr int kTeamPkMap1 = 12064; // map 12064: 2F, the Capitol
        constexpr int kTeamPkMap2 = 11073; // map 11073: 2F, Activity Centre
        constexpr std::uint64_t kAnswerMs = 8000; // time to answer the return question, ms
        static const char* const kNames[] = {"Starter's beach ", "Recorded spot ", "Carnie "};
        if (choice < 0 || choice > 2) { // choices 0..2: starter's beach, recorded spot, carnival
            return;
        }
        const int map = m_map ? m_map->id() : 0;
        if (choice == 1 && !m_returnSpotRecorded) {
            m_messages.show("Haven't recorded", 2000); // message display time in ms
            return;
        }
        if (choice == 2) { // choice 2: the carnival
            const auto scene = m_scenes && m_map ? m_scenes->find(map) : std::nullopt;
            if (map == kCarnivalMap) {
                m_messages.show("Already in carnie", 1500); // message display time in ms
                return;
            }
            if (scene && scene->type == kSeaScene) {
                m_messages.show("Can't use in the water scene", 1500); // message display time in ms
                return;
            }
            if (map == kTeamPkMap1 || map == kTeamPkMap2) {
                m_messages.show("Can't use it in the scene", 1500); // message display time in ms
                return;
            }
        }
        const int spot = choice + 1;
        TalkDialog::Timing timing;
        timing.timeoutMs = kAnswerMs;
        m_talk.ask(
            std::string("Want to back? ") + kNames[choice] + "Yes or No?",
            TalkDialog::Speaker{m_player.look, m_player.name},
            [this, spot] {
                if (const auto leader = m_teams.leaderOf(m_player.id)) {
                    m_services.connection.send(protocol::leaveTeamRequest(*leader));
                }
                m_services.connection.send(protocol::returnToSpotRequest(spot));
            },
            [] {},
            std::move(timing)
        );
    }

    bool WorldScene::standsOnFlatGround(bool readStoredTile) const {
        if (!m_map) {
            return false;
        }
        const int cell = formats::MapFile::kCellSize;
        const core::Point at = m_player.position;
        const int roundedX = (at.x + cell - 1) / cell - 1;
        const int roundedY = (at.y + cell - 1) / cell - 1;
        const core::Point tile = readStoredTile ? m_ownTile : core::Point{at.x / cell, at.y / cell};
        return m_map->file().attribute(roundedX, roundedY) == 0 && m_map->file().attribute(tile.x, tile.y) == 0;
    }

    void WorldScene::onEmotion(std::int32_t playerId, int emotionId) {
        if (const auto character = characterOf(playerId)) {
            character->get().emotion.show(emotionId, m_nowMs);
        }
    }

    std::optional<GroupWindow::LeaderRefusal> WorldScene::leaderRefusal(bool pending) const {
        if (m_eventRunner.isRunning()) {
            return GroupWindow::LeaderRefusal{"You can not change the team leader, you are currently doing something", 4000}; // notice display time in ms
        }
        if (m_fightState != 0) {
            return GroupWindow::LeaderRefusal{"During the battle, you can not change the team leader", 4000}; // notice display time in ms
        }
        if (m_player.vehicle) {
            return GroupWindow::LeaderRefusal{"You cannot change the team leader when using a vehicle", 4000}; // notice display time in ms
        }
        if (pending) {
            return GroupWindow::LeaderRefusal{"Asks the target to be the new team leader", 4000}; // notice display time in ms
        }
        if (m_makeJobs.activeSlot() != 0) {
            return GroupWindow::LeaderRefusal{"You cannot change the team leader during manufacturing", 2000}; // notice display time in ms
        }
        if (m_house.inFloorRoom()) {
            return GroupWindow::LeaderRefusal{"Unable to use at home", 2000}; // notice display time in ms
        }
        if (m_activeSport != 0) {
            return GroupWindow::LeaderRefusal{"You cannot change the team leader while Playing mini-games", 2000}; // notice display time in ms
        }
        return std::nullopt;
    }

    void WorldScene::onTeamFanIcon(const std::vector<std::int32_t>& ids, bool show) {
        for (const std::int32_t id : ids) {
            const auto character = characterOf(id);
            if (!show) {
                if (character) {
                    character->get().fanIcon = false;
                }
                continue;
            }
            const auto leader = m_teams.leaderOf(id);
            if (!character || !leader || !characterOf(*leader)) {
                continue;
            }
            std::vector<std::int32_t> team = m_teams.members(*leader);
            team.push_back(*leader);
            for (const std::int32_t member : team) {
                if (const auto mate = characterOf(member)) {
                    mate->get().fanIcon = false;
                }
            }
            character->get().fanIcon = true;
        }
    }

    void WorldScene::onTeamShapeRefusal(int reason) {
        if (reason == 1) { // 13/14 reason 1: no vehicle while shapeshifted
            m_messages.show("If you have shape shifted you will be unable to ride a vehicle.", 2000); // message display time in ms
        } else if (reason == 2 || reason == 3) { // 13/14 reasons 2 and 3: no bathing while shapeshifted
            m_messages.show("If you have shapeshifted you will be unable to bathe.", 2000); // message display time in ms
        }
    }

    void WorldScene::onTeamLeaderOffer(std::int32_t leaderId) {
        if (m_others.count(leaderId) == 0 || m_teams.leaderOf(m_player.id) != leaderId) {
            m_services.connection.send(protocol::leaderAnswerRequest(2)); // leader answer 2: refused
            return;
        }
        if (m_services.takesLeadFrom(leaderId)) {
            m_services.connection.send(protocol::leaderAcceptRequest());
            return;
        }
        TalkDialog::Timing timing;
        timing.timeoutMs = 5000; // close after 5000 ms
        timing.onTimeout = [this] { m_services.connection.send(protocol::leaderAnswerRequest(3)); }; // leader answer 3: no response
        m_talk.ask(
            "Do you want to be the team leader?",
            std::nullopt,
            [this] { m_services.connection.send(protocol::leaderAcceptRequest()); },
            [this] { m_services.connection.send(protocol::leaderAnswerRequest(2)); },
            timing
        ); // leader answer 2: refused
    }

    void WorldScene::onTeamLeaderAnswer(int answer, std::int32_t memberId) {
        m_groupWindow.leaderOfferEnded();
        if (answer == 2) { // leader answer 2: refused
            m_messages.show(playerName(memberId) + "Refuse to be the team leader", 2000); // message display time in ms
        } else if (answer == 3) { // leader answer 3: no response
            m_messages.show("did not respond.", 2000); // message display time in ms
        } else if (answer == 5) { // leader answer 5: busy
            m_messages.show("The target is busy now.", 2000); // message display time in ms
        }
    }

    void WorldScene::onTeamLeaderChanged(std::int32_t newLeaderId, std::int32_t oldLeaderId) {
        const auto leader = characterOf(newLeaderId);
        const auto member = characterOf(oldLeaderId);
        if (!leader || !member || !m_teams.changeLeader(oldLeaderId, newLeaderId)) {
            return;
        }
        MapCharacter& body = member->get();
        body.position = leader->get().position;
        startWalking(body, {}, m_nowMs);
        m_trails.at(oldLeaderId, body.position, true);
        if (isOwn(oldLeaderId) && m_map) {
            m_camera.centerOn(body.position, m_map->width(), m_map->height());
        }
        m_groupWindow.leaderOfferEnded();
        if (isOwn(newLeaderId)) {
            teamNotice(oldLeaderId, playerName(oldLeaderId) + "You are now the team leader", 2000); // notice display time in ms
            m_functionButtons.setTeamMode(2); // team mode 2: the invite cursor mode
        } else if (m_teams.leaderOf(m_player.id) == newLeaderId) {
            teamNotice(oldLeaderId, playerName(newLeaderId) + "To be the team leader", 2000); // notice display time in ms
        }
        m_groupWindow.refresh();
    }

    void WorldScene::onTeamJoinError(int code) {
        if (code == 1) { // join error 1: the target team is full
            m_messages.show("The target team is full.", 1200); // message display time in ms
        } else if (code == 2) { // join error 2: the own team is full
            m_messages.show("Your team is full.", 1200); // message display time in ms
        } else if (code == 3) { // join error 3: the target is busy
            m_messages.show("Unable to team up, the target is busy now.", 1200); // message display time in ms
        }
    }

}
