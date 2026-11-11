#ifndef NORTH_ISLAND_CLIENT_SCENES_WORLD_WORLD_SCENE_H
#define NORTH_ISLAND_CLIENT_SCENES_WORLD_WORLD_SCENE_H

#include "client/services/GameServices.h"
#include "client/scenes/Scene.h"
#include "client/services/ServerEvents.h"
#include "client/services/input/SeenTexts.h"
#include "client/subsystems/battle/Battle.h"
#include "client/models/character/MonsterBook.h"
#include "client/models/events/NoticeBoard.h"
#include "client/models/events/Top100.h"
#include "client/models/social/FriendList.h"
#include "client/models/guild/EnsignBook.h"
#include "client/models/guild/AllyList.h"
#include "client/models/guild/Guild.h"
#include "client/models/character/PlayerStatus.h"
#include "client/rendering/painters/InstanceGoalsPainter.h"
#include "client/models/instances/Instances.h"
#include "client/models/world/Camps.h"
#include "client/models/travel/Vehicles.h"
#include "client/rendering/effects/TransmitEffect.h"
#include "client/models/travel/VehicleWrecks.h"
#include "client/models/social/Marriage.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/PetTeam.h"
#include "client/models/pets/RebornWish.h"
#include "client/rendering/effects/StarSky.h"
#include "client/models/character/QuestMarks.h"
#include "client/models/remote/RemoteControl.h"
#include "client/models/social/MateVitals.h"
#include "client/models/social/TeamMate.h"
#include "client/models/social/Teams.h"
#include "gfx/Texture.h"
#include "client/models/chat/ChatSilence.h"
#include "client/rendering/effects/FireworkShow.h"
#include "client/rendering/effects/ThrowShow.h"
#include "client/models/character/StatusEffects.h"
#include "client/viewmodels/chat/ChatWindow.h"
#include "client/viewmodels/minigames/CompassWindow.h"
#include "client/viewmodels/crafting/CompoundWindow.h"
#include "client/viewmodels/hud/FunctionButtons.h"
#include "client/viewmodels/mail/FurMailWindow.h"
#include "client/viewmodels/social/GroupWindow.h"
#include "client/viewmodels/guild/GuildAuthorityForm.h"
#include "client/viewmodels/guild/GuildCreateForm.h"
#include "client/viewmodels/guild/GuildMyAuthorityForm.h"
#include "client/viewmodels/guild/GuildRulesForm.h"
#include "client/viewmodels/guild/GuildWindow.h"
#include "client/viewmodels/instances/InstanceCreateForm.h"
#include "client/viewmodels/instances/InstanceDetailForm.h"
#include "client/viewmodels/instances/InstanceWindow.h"
#include "client/viewmodels/marriage/MarryApplicationForm.h"
#include "client/viewmodels/marriage/MarryCardWindow.h"
#include "client/viewmodels/marriage/MarryDivorceForm.h"
#include "client/viewmodels/hud/HotKeyBar.h"
#include "client/viewmodels/chat/BlacklistWindow.h"
#include "client/viewmodels/character/CareerWindow.h"
#include "client/viewmodels/login/ChangePasswordWindow.h"
#include "client/rules/BagRequests.h"
#include "client/viewmodels/menus/CountBox.h"
#include "client/viewmodels/mall/MallCartWindow.h"
#include "client/viewmodels/mall/MallWindow.h"
#include "client/viewmodels/character/MonsterBookWindow.h"
#include "client/viewmodels/overlays/RoleIntroWindow.h"
#include "client/viewmodels/overlays/StarSkyWindow.h"
#include "client/viewmodels/chat/MsgSkinWindow.h"
#include "client/viewmodels/events/NoticeBoardWindow.h"
#include "client/viewmodels/events/NoticeReadForm.h"
#include "client/viewmodels/events/NoticeSendForm.h"
#include "client/viewmodels/character/NotebookWindow.h"
#include "client/viewmodels/character/QuestWindow.h"
#include "client/models/events/EventRunner.h"
#include "client/viewmodels/events/ActivitiesWindow.h"
#include "client/viewmodels/events/ActivityInfoWindow.h"
#include "client/viewmodels/events/LotteryWindow.h"
#include "client/viewmodels/travel/MagicMapWindow.h"
#include "client/viewmodels/items/ItemBagWindow.h"
#include "client/viewmodels/house/StoreroomWindow.h"
#include "client/viewmodels/social/MagnetWindow.h"
#include "client/viewmodels/social/BlessBalloonWindow.h"
#include "client/viewmodels/social/BlessMessageWindow.h"
#include "client/viewmodels/social/LoveFlowerWindow.h"
#include "client/viewmodels/social/LoveFlowerMessageWindow.h"
#include "client/viewmodels/system/RemoteControlWindow.h"
#include "client/viewmodels/system/RemotePlusWindow.h"
#include "client/viewmodels/bank/BankItemWindow.h"
#include "client/viewmodels/bank/AtmDepositWindow.h"
#include "client/viewmodels/bank/AtmListWindow.h"
#include "client/viewmodels/bank/AtmTransferWindow.h"
#include "client/viewmodels/bank/BankMoneyWindow.h"
#include "client/viewmodels/battle/BattleMainForm.h"
#include "client/viewmodels/battle/BattleSkillWindow.h"
#include "client/viewmodels/character/BeforeCareerWindow.h"
#include "client/viewmodels/items/CampStyleWindow.h"
#include "client/viewmodels/minigames/GameStartWindow.h"
#include "client/viewmodels/minigames/GobangWindow.h"
#include "client/viewmodels/pets/PetStorageWindow.h"
#include "client/viewmodels/guild/GuildWeaponBankWindow.h"
#include "client/viewmodels/trade/SaleMenuWindow.h"
#include "client/viewmodels/trade/SaleNetWindow.h"
#include "client/viewmodels/bank/WarehouseWindow.h"
#include "client/viewmodels/items/AdminItemsWindow.h"
#include "client/viewmodels/chat/InnAnswerWindow.h"
#include "client/viewmodels/mail/MailBoxWindow.h"
#include "client/viewmodels/crafting/MakeQuestions.h"
#include "client/viewmodels/crafting/MakeThingInfoWindow.h"
#include "client/viewmodels/crafting/MakeThingWindow.h"
#include "client/viewmodels/house/EggBuyWindow.h"
#include "client/viewmodels/house/BedWindow.h"
#include "client/viewmodels/crafting/BlueprintWindow.h"
#include "client/viewmodels/house/BookShelfWindow.h"
#include "client/viewmodels/house/ComputerWindow.h"
#include "client/viewmodels/house/HomeAdornWindow.h"
#include "client/viewmodels/house/PortraitWindow.h"
#include "client/viewmodels/crafting/ReferWindow.h"
#include "client/viewmodels/crafting/RepairWindow.h"
#include "client/viewmodels/house/DoorplateWindow.h"
#include "client/models/mall/LuckyBags.h"
#include "client/viewmodels/mall/ArticleInfoWindow.h"
#include "client/viewmodels/menus/MaidMenu.h"
#include "client/viewmodels/house/PianoWindow.h"
#include "client/viewmodels/crafting/ConfuseStoneWindow.h"
#include "client/viewmodels/items/MagicBoxWindow.h"
#include "client/viewmodels/crafting/PackingWindow.h"
#include "client/viewmodels/items/InsteadEcisWindow.h"
#include "client/viewmodels/items/MemCardWindow.h"
#include "client/models/items/PuppetState.h"
#include "client/viewmodels/crafting/ReinforceWindow.h"
#include "client/viewmodels/crafting/SewingKitWindow.h"
#include "client/viewmodels/crafting/DismantleWindow.h"
#include "client/viewmodels/crafting/DyeWindow.h"
#include "client/viewmodels/pets/ExpressWindow.h"
#include "client/viewmodels/fish/FishAnnotateWindow.h"
#include "client/viewmodels/fish/FishBuyWindow.h"
#include "client/viewmodels/fish/FishTankWindow.h"
#include "client/viewmodels/items/GiftExchangeWindow.h"
#include "client/viewmodels/travel/GpsWindow.h"
#include "client/viewmodels/house/BlackBoardWindow.h"
#include "client/viewmodels/login/CafeIdWindow.h"
#include "client/viewmodels/items/ExpBoxWindow.h"
#include "client/viewmodels/login/IdAndPwWindow.h"
#include "client/viewmodels/events/AnnounceWindow.h"
#include "client/viewmodels/events/LeaveWordWindow.h"
#include "client/viewmodels/events/GalaxyRankWindow.h"
#include "client/viewmodels/pets/NpcUpgradeWindow.h"
#include "client/models/guild/GuildWork.h"
#include "client/viewmodels/bank/PropertyWindow.h"
#include "client/viewmodels/system/ReFillWindow.h"
#include "client/models/house/SpecFloorThing.h"
#include "client/viewmodels/items/PicnicWindow.h"
#include "client/viewmodels/bank/PointExchangeWindow.h"
#include "client/viewmodels/events/PointStoreWindow.h"
#include "client/rendering/effects/GpsTeleportEffect.h"
#include "client/viewmodels/pets/ForageWindow.h"
#include "client/viewmodels/house/RetainFurWindow.h"
#include "client/viewmodels/travel/VehicleRepairWindow.h"
#include "client/viewmodels/house/CupboardWindow.h"
#include "client/viewmodels/travel/GlobeWindow.h"
#include "client/viewmodels/house/IceBoxWindow.h"
#include "client/viewmodels/house/PledgeBoxWindow.h"
#include "client/viewmodels/house/MusicCenterWindow.h"
#include "client/viewmodels/house/GasStorageWindow.h"
#include "client/viewmodels/travel/VoyageCompassWindow.h"
#include "client/viewmodels/house/EggSellWindow.h"
#include "client/viewmodels/house/StallCaseWindow.h"
#include "client/viewmodels/house/FurnitureBuyWindow.h"
#include "client/viewmodels/house/StallPriceWindow.h"
#include "client/viewmodels/house/VendingWindow.h"
#include "client/viewmodels/trade/NpcSaleWindow.h"
#include "client/viewmodels/crafting/NpcMakeWindow.h"
#include "client/viewmodels/trade/NpcStoreWindow.h"
#include "client/viewmodels/mail/SendMailWindow.h"
#include "client/viewmodels/items/StoreTankWindow.h"
#include "client/viewmodels/travel/CapsuleWindow.h"
#include "client/viewmodels/house/CarCaseWindow.h"
#include "client/viewmodels/login/DisconnectForm.h"
#include "client/viewmodels/house/ShowPlatformWindow.h"
#include "client/viewmodels/travel/TransportWindow.h"
#include "client/viewmodels/travel/ParkingWindow.h"
#include "client/viewmodels/trade/ItemStallWindow.h"
#include "client/viewmodels/trade/PetStallWindow.h"
#include "client/viewmodels/trade/StallGoodsWindow.h"
#include "client/viewmodels/trade/StallPetsWindow.h"
#include "client/viewmodels/trade/ItemTradeWindow.h"
#include "client/viewmodels/trade/UtensilTradeWindow.h"
#include "client/viewmodels/trade/UtensilStallWindow.h"
#include "client/viewmodels/trade/BuyUtensilWindow.h"
#include "client/viewmodels/items/ItemWindow.h"
#include "client/viewmodels/menus/NpcTeamMenu.h"
#include "client/viewmodels/minigames/FortuneEggWindow.h"
#include "client/viewmodels/minigames/LuckyCardsWindow.h"
#include "client/viewmodels/minigames/QuizWindow.h"
#include "client/viewmodels/minigames/LuckyWheelWindow.h"
#include "client/viewmodels/minigames/BoxingWindow.h"
#include "client/models/house/BookShelves.h"
#include "client/models/house/HouseEditor.h"
#include "client/models/crafting/MakeJobs.h"
#include "client/rules/MakeMaterials.h"
#include "client/rules/StallDropRules.h"
#include "client/models/house/HouseInterior.h"
#include "client/rendering/house/HousePainter.h"
#include "client/models/house/TrainingPet.h"
#include "client/rendering/house/HouseWaymark.h"
#include "client/viewmodels/minigames/DigHoleWindow.h"
#include "client/viewmodels/overlays/MovieLayer.h"
#include "client/viewmodels/minigames/DollWindow.h"
#include "client/viewmodels/minigames/HitMouseWindow.h"
#include "client/viewmodels/minigames/HunterWindow.h"
#include "client/viewmodels/minigames/PumpkinWindow.h"
#include "client/viewmodels/minigames/ScotdWindow.h"
#include "client/viewmodels/minigames/JigsawWindow.h"
#include "client/viewmodels/minigames/LuckyWindow.h"
#include "client/viewmodels/minigames/MemoryGameWindow.h"
#include "client/viewmodels/minigames/SheepWindow.h"
#include "client/viewmodels/minigames/SlotMachineWindow.h"
#include "client/viewmodels/minigames/ThrowWindow.h"
#include "client/viewmodels/chat/InputBar.h"
#include "client/viewmodels/items/ItemInfoPopup.h"
#include "client/viewmodels/character/PotentialReturnWindow.h"
#include "client/viewmodels/character/PotentialWindow.h"
#include "client/viewmodels/menus/RightClickMenu.h"
#include "client/viewmodels/hud/MainButtons.h"
#include "client/viewmodels/hud/MainStatus.h"
#include "client/viewmodels/hud/MiniMap.h"
#include "client/viewmodels/menus/MoneyMenu.h"
#include "client/viewmodels/pets/PetInfoPopup.h"
#include "client/viewmodels/trade/PetTradeWindow.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "client/viewmodels/character/SkillWindow.h"
#include "client/viewmodels/social/SocialWindow.h"
#include "client/viewmodels/system/SettingsWindow.h"
#include "client/viewmodels/events/Top100Window.h"
#include "client/viewmodels/chat/TalkDialog.h"
#include "client/viewmodels/hud/TimeEffectIcons.h"
#include "client/models/house/Sandglass.h"
#include "client/viewmodels/menus/ThrowMoneyBox.h"
#include "client/models/guild/DefenseNpcPrefixes.h"
#include "client/models/events/NewYearEvent.h"
#include "client/models/guild/GoldEaglePrefixes.h"
#include "client/models/guild/StoneEaglePrefixes.h"
#include "client/rendering/effects/PlayerEffectLights.h"
#include "client/rendering/effects/TimedLight.h"
#include "client/rendering/effects/TreasureLights.h"
#include "client/models/world/TreasureNpcJump.h"
#include "client/models/world/Camera.h"
#include "client/models/world/WalkMark.h"
#include "client/rendering/effects/SceneEffects.h"
#include "client/rendering/world/GroundFlights.h"
#include "client/models/world/GroundItems.h"
#include "client/models/world/MapCharacter.h"
#include "client/models/world/MapNpc.h"
#include "client/models/world/NpcFacings.h"
#include "client/models/world/PetsOnMap.h"
#include "client/models/world/PlayerTrails.h"
#include "client/models/world/WorldMap.h"
#include "client/rendering/effects/WishEffects.h"
#include "client/rendering/world/WorldPainter.h"
#include "client/models/world/WorldPlayerRecords.h"
#include "formats/DataArchive.h"
#include "formats/EventArchive.h"
#include "formats/NpcTable.h"
#include "formats/SceneTable.h"
#include "ui/DrawingArea.h"
#include "ui/Element.h"
#include "ui/Screen.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <vector>

namespace client {

    /// @brief The world scene (WorldScene.xml): the map the own player is on, with the players around.
    /// @details Entering (EnterGameWorld / FinishEnterGame): the map loads at once while the login backdrop stays up; server 1/11 (or 5 s
    /// without it) fades the map in from black and starts the map's music; players are drawn and the own player may walk once server 5/4
    /// puts them in the world. A click on the map walks there (client 6/1); holding the button keeps walking towards the mouse every 400 ms;
    /// the arrow keys walk 80 pixels that way every 400 ms. Walking into a door asks the server (20/8); it answers with the event's results
    /// and, for a warp, the new map (12), which is loaded and confirmed (12/1).
    class WorldScene : public Scene, private ServerEvents, private ScriptProbe {
        public:
            /// @brief Builds the scene from the shared game services.
            /// @param services The game services (assets, connection, settings, view).
            explicit WorldScene(
                GameServices& services
            );

            /// @brief Per-frame logic update of the scene.
            /// @param nowMs Current time in milliseconds.
            void update(std::uint64_t nowMs) override;

            /// @brief Draws the scene.
            /// @param canvas Target canvas.
            /// @param nowMs Current time in milliseconds.
            void draw(gfx::Canvas& canvas, std::uint64_t nowMs) override;

            /// @brief Reacts to a window resize: the camera shows the new view, centred on the own player again.
            /// @param nowMs Current time in milliseconds.
            void followView(std::uint64_t nowMs);

            /// @brief Whether the qol zoom shows a part of the canvas other than the window.
            /// @return True when the view is zoomed.
            bool isViewZoomed() const;

            /// @brief The mouse position on the map, taking the qol zoom into account.
            /// @return The mouse point on the map.
            core::Point worldMouse() const;

            /// @brief Turns a mouse event of the map's area (which fills the window) into one with its point on the game's screen.
            /// @param local The event with a point local to the map area.
            /// @return The event with the point on the game's screen.
            ui::EventArgs onScreen(const ui::EventArgs& local) const {
                ui::EventArgs args = local;
                args.position = local.position + m_view.bounds().position();
                if (isViewZoomed()) {
                    args.position = ViewZoom::fromWindow(args.position, m_camera.view(), m_services.view);
                }
                return args;
            }

            /// @brief Per-frame movement step of the scene.
            /// @param nowMs Current time in milliseconds.
            void move(std::uint64_t nowMs) override;

            /// @brief Whether the scene runs on free (uncapped) frames.
            /// @return True when free frames are allowed.
            bool freeFrames() const override;

            /// @brief Until when the battle holds the frame pace.
            /// @return The time in milliseconds the battle holds until.
            std::uint64_t heldUntilMs() const override { return m_battle.heldUntilMs(); }

            /// @brief The scene's UI screen.
            /// @return The screen.
            ui::Screen& screen() override { return m_screen; }

            /// @brief The scene to switch to, if any.
            /// @return The next scene kind, or none.
            std::optional<SceneKind> nextScene() const override { return m_next; }

            /// @brief The test-script probe of this scene (the scene itself).
            /// @return A reference to the script probe.
            std::optional<std::reference_wrapper<ScriptProbe>> scriptProbe() override { return std::ref(static_cast<ScriptProbe&>(*this)); }

            /// @brief The own player's name (qol multi-login: what the party panels show of a member).
            /// @return The name.
            const std::string& ownName() const { return m_player.name; }

            /// @brief The own player's look (qol multi-login party panels).
            /// @return The look.
            const CharacterLook& ownLook() const { return m_player.look; }

            /// @brief The players the battle's panels show at the top, in their places (none out of a battle).
            /// @return The player ids.
            std::vector<std::int32_t> battlePanelIds() const;

            /// @brief The own player's status.
            /// @return The status.
            const PlayerStatus& ownStatus() const { return m_status; }

            /// @brief Whether a battle is on.
            /// @return True in a battle.
            bool inBattle() const { return m_battle.isOn(); }

            /// @brief Whether the server has put the own player in the world (5/4).
            /// @return True when in the world.
            bool inWorld() const { return m_inWorld; }

            /// @brief The own player's id.
            /// @return The id.
            std::int32_t ownId() const { return m_player.id; }

            /// @brief The map the player is on.
            /// @return The map id (0 before one is loaded).
            int ownMapId() const { return mapId(); }

            /// @brief The teams on the map.
            /// @return The teams.
            const Teams& teams() const { return m_teams; }

            /// @brief The own fighter commanding in a fight.
            /// @return The fighter (0: none, between the rounds, or every own fighter has its command).
            int battleActor() const { return m_battle.isOn() ? m_battle.actor() : 0; }

            /// @brief Adds a party member's chat line to the leader's chat window, unless the leader heard it already.
            /// @param channel Chat channel.
            /// @param line The line.
            /// @param memberName Name of the party member who heard it.
            void addPartyLine(int channel, const std::string& line, const std::string& memberName) {
                constexpr std::size_t kRecentLines = 60; // how many of the latest chat lines to compare for a repeat
                if (!m_chat.heardRecently(channel, line, kRecentLines)) {
                    m_chat.add(channel, "[" + memberName + "]" + line, 0, false);
                }
            }

            /// @brief Shows a party member's message in the leader's top or middle box, grouped with those shown just before.
            /// @param centre True for the middle box, false for the top one.
            /// @param text The message.
            /// @param durationMs How long it shows, in milliseconds.
            void addPartyMessage(bool centre, const std::string& text, int durationMs) {
                (centre ? m_centreMessages : m_messages).showGrouped(text, durationMs);
            }

            /// @brief Says what the leader typed as this member (its own checks; a refusal is a line of its chat).
            /// @param typedChannel The channel typed.
            /// @param text The text.
            void sayAsPartyMember(int typedChannel, const std::string& text) { sendChat(typedChannel, text); }

            /// @brief Starts the event of the NPC or door the party leader started (qol NPC mirror), even as a team follower, who
            /// is never near it nor checks doors and NPC proximity.
            /// @param trigger How the leader started it.
            /// @param index Index of the NPC or the door.
            /// @return True when the event was asked for.
            bool mirrorEvent(PartyTrigger trigger, int index);

            /// @brief Passes the party leader's dialog answer to the mirrored event.
            /// @param dataId The leader's dialog surface.
            /// @param code The leader's answer code.
            void mirrorEventAnswer(int dataId, int code) { m_eventRunner.leaderAnswered(dataId, code); }

            /// @brief Tells the mirrored event that the party leader's event ended.
            void mirrorLeaderEventEnded() { m_eventRunner.leaderEnded(); }

        private:
            /// @brief Loads a map and puts the own player at a position.
            /// @param mapId Map id.
            /// @param position Start position on the map.
            void enterMap(int mapId, core::Point position);

            /// @brief ScriptProbe (WorldSceneScript.cpp): where a scripted object is on the screen.
            /// @param object The object to find.
            /// @return Its sighting, or none.
            std::optional<ScriptSighting> sight(const ScriptObjectRef& object) override;

            /// @brief ScriptProbe: starts a walk of the own player to a map point.
            /// @param mapPoint The destination on the map.
            /// @param reason Receives why the walk was refused.
            /// @return True when the walk started.
            bool startWalk(core::Point mapPoint, std::string& reason) override;

            /// @brief ScriptProbe: whether the own player is walking.
            /// @return True while walking.
            bool isWalking() const override { return m_player.walking; }

            /// @brief ScriptProbe: the own player's map position.
            /// @return The position.
            core::Point ownPosition() const override { return m_player.position; }

            /// @brief ScriptProbe: the id of the loaded map.
            /// @return The map id (0 when none).
            int mapId() const override { return m_map ? m_map->id() : 0; }

            /// @brief ScriptProbe: a mark in the stream of shown texts.
            /// @return The count of texts seen so far.
            std::uint64_t textMark() const override { return m_seenTexts.count(); }

            /// @brief ScriptProbe: whether a text was shown since a mark.
            /// @param mark A value from textMark().
            /// @param text The text to look for.
            /// @return True when it was shown after the mark.
            bool textSince(std::uint64_t mark, const std::string& text) const override { return m_seenTexts.containsSince(mark, text); }

            /// @brief The screen point of the first thing the hit test takes for the object, inside an area, where the world view takes the press.
            /// @param area The screen area to search.
            /// @param hits The hit test of the object.
            /// @return The point, or none.
            std::optional<core::Point> scriptPointIn(const core::Rect& area, const std::function<bool(core::Point)>& hits);

            /// @brief Script sighting of a map NPC.
            /// @param npc The NPC.
            /// @return Its sighting, or none.
            std::optional<ScriptSighting> sightNpc(const MapNpc& npc);

            /// @brief Script sighting of a piece of house furniture.
            /// @param furnIndex Furniture index.
            /// @return Its sighting, or none.
            std::optional<ScriptSighting> sightFurniture(int furnIndex);

            /// @brief Places the objects the map's events put on it.
            void placeEventObjects();

            /// @brief The player accepts the inn's rest. Original: TINNAskForm.AcceptInnRest.
            void acceptInnRest();

            /// @brief The player declines the inn's rest. Original: TINNAskForm.DeclineInnRest.
            void declineInnRest();

            /// @brief Steps the inn sleep: dark, five seconds asleep, light again. Original: UpdateInnSleep.
            /// @param nowMs Current time in milliseconds.
            void updateInnSleep(std::uint64_t nowMs);

            /// @brief Steps the treasure NPC's jump.
            /// @param nowMs Current time in milliseconds.
            void updateTreasureJump(std::uint64_t nowMs);

            /// @brief Whether the player is in the own house (UpdateSpecFloorThing's check).
            /// @return True in the own house.
            bool ownHouse() const;

            /// @brief Whether the thing of a special house room shows for the player (UpdateSpecFloorThing).
            /// @return True when shown.
            bool specFloorShown() const;

            /// @brief Whether the mouse is on the special room's thing (UpdateSpecFloorThing).
            /// @return True when hovered.
            bool specFloorHovered() const;

            /// @brief Picks up the special room's thing. Original: TGdThingInSpecFloor.HandleMouseUp.
            void pickSpecFloorThing();

            /// @brief Draws the player asleep in a bed in the middle of the dark screen. Original: DrawInnSleepScene.
            /// @param canvas Target canvas.
            /// @param nowMs Current time in milliseconds.
            void paintInnSleep(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Checks what the own player's move triggers: doors, then NPCs near. Original: CheckPlayerMoveEvents.
            /// @param nowMs Current time in milliseconds.
            void checkMoveEvents(std::uint64_t nowMs);

            /// @brief Checks whether the own player walked into a door.
            void checkDoors();

            /// @brief Checks the NPC an event sends back to its post (FChaseNpcIndex).
            void checkChase();

            /// @brief Updates whether an NPC is hidden or shown.
            /// @param npc The NPC.
            /// @param nowMs Current time in milliseconds.
            void updateNpcVisibility(MapNpc& npc, std::uint64_t nowMs);

            /// @brief Starts the events of NPCs the own player comes near.
            /// @param nowMs Current time in milliseconds.
            void checkNearNpcs(std::uint64_t nowMs);

            /// @brief Near enough and with room in the bag, asks to pick a ground thing up. Original: TGdThing.HandleMouseUp.
            /// @param groundIndex Index of the ground thing.
            void pickUp(int groundIndex);

            /// @brief Finds a map NPC by its index.
            /// @param index NPC index.
            /// @return The NPC, or none.
            std::optional<std::reference_wrapper<MapNpc>> findNpc(int index);

            /// @brief Starts an NPC's routine (its walking or idle pattern).
            /// @param npc The NPC.
            /// @param placed The NPC as the scene table places it.
            void startRoutine(MapNpc& npc, const formats::SceneNpc& placed);

            /// @brief Ends the entering phase: the map fades in and its music starts (FinishEnterGame).
            void finishEntering();

            /// @brief Plays the music of the current map.
            void playMapMusic();

            /// @brief Handles the connection's pending events (packets, disconnects).
            void handleConnectionEvents();

            /// @brief Draws the map with its things and people.
            /// @param canvas Target canvas.
            /// @param nowMs Current time in milliseconds.
            void paintWorld(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Whether the own player may walk now.
            /// @return True when walking is allowed.
            bool canWalk() const;

            /// @brief Turns the own player to face the mouse.
            /// @return True when the facing was changed.
            bool faceMouse();

            /// @brief Changes the item a Maid piece of furniture holds.
            /// @param furnIndex Furniture index.
            /// @param itemId New item id.
            void changeMaidPiece(int furnIndex, int itemId);

            /// @brief Walks the own player to a map point. Original: WalkToMouse.
            /// @param mapPoint The destination on the map.
            /// @param byKeyboard True when the walk comes from the arrow keys.
            /// @return The destination the own player was left with (Player.FDestX / FDestY), where the click's walk mark goes.
            core::Point walkTo(core::Point mapPoint, bool byKeyboard = false);

            /// @brief The path through a floor room from one map point to another. Original: FindHouseMovePath and CellPathToScreen.
            /// @param from Start map point.
            /// @param to Destination map point.
            /// @return The middles of the cells, the first the one the walk starts in (empty: nowhere to go).
            std::vector<core::Point> houseWalkPath(core::Point from, core::Point to) const;

            /// @brief Walks the own player with the arrow keys.
            /// @param nowMs Current time in milliseconds.
            void walkWithKeys(std::uint64_t nowMs);

            /// @brief Sends the own player's walk to the server (client 6/1).
            /// @param stopped True when the player has stopped.
            /// @param direction Facing direction.
            /// @param destination Walk destination.
            void sendWalk(bool stopped, int direction, core::Point destination);

            /// @brief Sends a typed chat line (with the channel checks).
            /// @param typedChannel The channel typed.
            /// @param text The text.
            /// @return What became of the line.
            InputBar::Sent sendChat(int typedChannel, const std::string& text);

            /// @brief Parses a chat command. Original: ParseChatCommand.
            /// @param text The typed text.
            /// @return The words of a "/msg name words" ("" for none).
            std::string parseChatCommand(const std::string& text);

            /// @brief SetChatChannel's checks: team and guild channels need a team or a guild.
            /// @param channel Chat channel.
            /// @return True when the channel may be used.
            bool chatChannelAllowed(int channel);

            /// @brief Finds a world player by name.
            /// @param name Player name.
            /// @return The player id, or none.
            std::optional<std::int32_t> worldPlayerNamed(const std::string& name) const;

            /// @brief The name of a chat speaker.
            /// @param speakerId Speaker id.
            /// @return The name.
            std::string speakerName(std::int32_t speakerId) const;

            /// @brief Draws the location line (map name).
            /// @param canvas Target canvas.
            void paintLocation(gfx::Canvas& canvas);

            /// @brief Draws the weather under or over the people.
            /// @param canvas Target canvas.
            /// @param under True for the weather under the people, false for over.
            /// @param nowMs Current time in milliseconds.
            void paintWeather(gfx::Canvas& canvas, bool under, std::uint64_t nowMs);

            /// @brief Refreshes the HUD from the player's state.
            void refreshHud();

            /// @brief Handles a key press.
            /// @param key The key event.
            /// @return True when the scene used the key.
            bool onKey(const ui::KeyEvent& key);

            /// @brief Rebuilds the own figure after a change of what it wears.
            void dressPlayer();

            /// @brief Announces an item put on or taken off.
            /// @param itemId Item id.
            /// @param putOn True when put on, false when taken off.
            /// @param petPlace Pet place when a pet's item (0: the player's own).
            void announceEquipment(int itemId, bool putOn, int petPlace = 0);

            /// @brief Reacts to a change of the bag.
            void inventoryChanged();

            /// @brief Syncs the weapon of the own pet out on the map.
            void syncOwnPetWeapon();

            /// @brief Opens or closes a game window by its hotkey letter.
            /// @param window The window's letter.
            void toggleWindow(char window);

            /// @brief Takes a screenshot.
            void takePhoto();

            /// @brief Log Out / Exit: darkens the world and steps the game windows aside while the question is asked.
            /// @details Original: StartScreenTransition 1, the talk box drawn alone; Cancel brings the windows back.
            /// @param exitGame True to exit the game, false to log out.
            void askToLeave(bool exitGame);

            /// @brief Leaves the world.
            /// @param exitGame True to exit the game, false to log out.
            void leave(bool exitGame);

            /// @brief Shows or hides the game windows.
            /// @param shown True to show them.
            void showGameWindows(bool shown);

            /// @brief Places the mail forms where the last one was closed. Original: SetMailFormsX / SetMailFormsY.
            /// @param position The position for the mail forms.
            void placeMailForms(core::Point position);

            /// @brief Opens or closes the furniture mail window.
            void toggleFurMail();

            /// @brief Imports the older client's logs of a kind into the mail box. Original: ImportMailFormLogs / ImportMailBoxLogs.
            /// @param kind Which logs to import.
            void importMailLogs(MailLogImporter::Kind kind);

            /// @brief The NPC drawn under a screen point (the one lowest on the map).
            /// @param screenPoint Screen point.
            /// @return The NPC index, or none.
            std::optional<int> npcAt(core::Point screenPoint);

            /// @brief A click on an NPC: its events start when the player is near enough. Original: HandleHumanClick for an NPC.
            /// @param npcIndex NPC index.
            void clickNpc(int npcIndex);

            /// @brief Carries out an event result the world handles (EventRunner::Hooks::worldResult).
            /// @param result The event result.
            void runWorldResult(const EventResult& result);

            /// @brief Asks whether an NPC should join the team.
            void askNpcJoinTeam();

            /// @brief Shows an event's message box.
            /// @param result The event result carrying the message.
            void eventMessageBox(const EventResult& result);

            /// @brief Turns the NPC and the player to face an event's target. Original: FaceEventTarget.
            /// @param target Target point.
            /// @param npcIndex NPC index.
            void faceEventTarget(core::Point target, int npcIndex);

            /// @brief Applies the chat colours of the settings.
            /// @param settings The system settings.
            void applyChatColors(const SystemSettings& settings);

            /// @brief Moves the characters on the map one step.
            /// @param nowMs Current time in milliseconds.
            void moveCharacters(std::uint64_t nowMs);

            /// @brief The own player's level.
            /// @return The level.
            int ownLevel() const;

            /// @brief The server kicked the player.
            /// @param reason Kick reason code.
            void onKicked(int reason) override;

            /// @brief Shows the form of a disconnection over the darkened world. Original: TDZ_WillCloseForm1.
            /// @param notice The disconnection notice.
            void showDisconnect(const DisconnectNotice& notice);

            /// @brief Entering the world is finished.
            void onEnterWorldFinished() override;

            /// @brief The server says the world is ready (players are drawn and may walk).
            void onWorldReady() override;

            /// @brief The room a player's map change names (action 12's room bytes), in WorldSceneHouse.cpp.
            /// @param playerId Player id.
            /// @param room Room number.
            /// @param sizesGiven Whether the sizes follow.
            /// @param sizes Sizes of the floor rooms.
            // five floor room sizes
            void onHouseRoom(std::int32_t playerId, int room, bool sizesGiven, const std::array<int, 5>& sizes) override;

            /// @brief A furniture message (action 62).
            /// @param payload Message payload.
            void onFurnitureMessage(const core::Bytes& payload) override;

            /// @brief A manufacturing message.
            /// @param payload Message payload.
            void onManufactureMessage(const core::Bytes& payload) override;

            /// @brief Another player appeared on the map.
            /// @param record The player's record.
            /// @param entering True when the player is just entering.
            void onPlayerAppeared(const protocol::OtherPlayerRecord& record, bool entering) override;

            /// @brief Another player walks.
            /// @param playerId Player id.
            /// @param to Destination.
            /// @param direction Facing direction, when given.
            void onPlayerWalks(std::int32_t playerId, core::Point to, std::optional<int> direction) override;

            /// @brief Another player left the map.
            /// @param playerId Player id.
            void onPlayerLeft(std::int32_t playerId) override;

            /// @brief A player gained a level.
            /// @param playerId Player id.
            void onPlayerLevelUp(std::int32_t playerId) override;

            /// @brief A system announcement.
            /// @param announce The announcement.
            void onSystemAnnounce(const SystemAnnounce& announce) override;

            /// @brief An ATM balance or transfer notice.
            /// @param kind Notice kind.
            /// @param amount Amount of gold.
            /// @param peerId Id of the other player, when one is involved.
            void onAtmBalance(int kind, std::int32_t amount, std::int32_t peerId) override;

            /// @brief The server opens an ATM form.
            /// @param form Which form.
            void onAtmForm(int form) override;

            /// @brief The gold exchange rate.
            /// @param rate The rate.
            void onExchangeRate(std::int32_t rate) override;

            /// @brief A gold exchange result.
            /// @param gold The gold now held.
            /// @param gained The gold gained.
            void onExchangeGold(std::int32_t gold, std::int32_t gained) override;

            /// @brief An exchange fee notice.
            /// @param kind Notice kind.
            /// @param playerId Player id.
            /// @param value The fee or amount.
            void onExchangeFee(int kind, std::int32_t playerId, std::int32_t value) override;

            /// @brief A player uses a house utensil.
            /// @param playerId Player id.
            /// @param furnIndex Furniture index.
            /// @param seat Seat number.
            void onUtensilUsed(std::int32_t playerId, int furnIndex, int seat) override;

            /// @brief A player left a house utensil.
            /// @param playerId Player id.
            void onUtensilLeft(std::int32_t playerId) override;

            /// @brief The system forbade local chat (2/8, BlockLocalChatBySystem).
            void onChatBlocked() override;

            /// @brief A chat line from a speaker.
            /// @param speakerId Speaker id.
            /// @param name Speaker name.
            /// @param flags Line flags.
            /// @param text The text.
            void onChatLine(std::int32_t speakerId, const std::string& name, int flags, const std::string& text) override;

            /// @brief A player's level is told.
            /// @param playerId Player id.
            /// @param level The level.
            void onPlayerLevel(std::int32_t playerId, int level) override;

            /// @brief The result of sending a flower.
            /// @param result Result code.
            void onFlowerSent(int result) override;

            /// @brief A flower was received.
            /// @param senderId Sender id.
            void onFlowerReceived(std::int32_t senderId) override;

            /// @brief Players start or stop auto collecting.
            /// @param playerIds The players.
            /// @param collecting True when collecting starts.
            void onAutoCollecting(const std::vector<std::int32_t>& playerIds, bool collecting) override;

            /// @brief The player was silenced.
            /// @param kind Silence kind.
            /// @param mask Which channels are silenced.
            /// @param minutes Duration in minutes.
            void onChatSilenced(int kind, int mask, std::uint32_t minutes) override;

            /// @brief The silence was lifted.
            /// @param mailToo True when mail is released as well.
            void onChatSilenceReleased(bool mailToo) override;

            /// @brief A helper recovered someone's HP and SP.
            /// @param casterId Helper id.
            /// @param targetId Target id.
            /// @param hp HP recovered.
            /// @param sp SP recovered.
            void onHelpRecover(std::int32_t casterId, std::int32_t targetId, std::int32_t hp, int sp) override;

            /// @brief A recovery notice for the player or a pet.
            /// @param petPlace Pet place (0: the player).
            /// @param hp HP recovered.
            /// @param sp SP recovered.
            void onRecoverNotice(int petPlace, std::int32_t hp, int sp) override;

            /// @brief A voucher item was used.
            /// @param itemId Item id.
            void onVoucherItem(int itemId) override;

            /// @brief A praise was received.
            /// @param grade Praise grade.
            void onPraise(int grade) override;

            /// @brief A double-exp state notice.
            /// @param kind Notice kind.
            /// @param minutes Minutes left, when given.
            void onDoubleExp(int kind, std::optional<int> minutes) override;

            /// @brief A lucky grass state notice.
            /// @param kind Notice kind.
            /// @param minutes Minutes.
            void onLuckyGrass(int kind, int minutes) override;

            /// @brief A spa buns state notice.
            /// @param kind Notice kind.
            /// @param value The value, when given.
            void onSpaBuns(int kind, std::optional<int> value) override;

            /// @brief An exp potion state notice.
            /// @param kind Notice kind.
            /// @param first First value of the notice.
            /// @param minutes Minutes.
            /// @param rate Exp rate.
            void onExpPotion(int kind, int first, int minutes, int rate) override;

            /// @brief A training item state notice.
            /// @param kind Notice kind.
            /// @param effect Effect id.
            /// @param minutes Minutes.
            /// @param sound Whether to play the item sound.
            void onTrainingItem(int kind, int effect, int minutes, bool sound) override;

            /// @brief A worn item broke.
            /// @param owner Owner (the player or a pet place).
            /// @param itemId Item id.
            void onEquipBroken(int owner, int itemId) override;

            /// @brief The server asks whether to discard an item.
            /// @param reason Reason code.
            /// @param bagSlot Bag place.
            /// @param itemId Item id.
            /// @param count Item count.
            void onDiscardAsked(int reason, int bagSlot, int itemId, int count) override;

            /// @brief A player sets off fireworks.
            /// @param playerId Player id.
            /// @param kind Firework kind.
            /// @param variant Variant.
            void onFirework(std::int32_t playerId, int kind, int variant) override;

            /// @brief A throw effect lands.
            /// @param kind Throw kind.
            /// @param playerId Thrower id.
            /// @param x Target x.
            /// @param y Target y.
            void onThrowEffect(int kind, std::int32_t playerId, int x, int y) override;

            /// @brief A player bathes in a tub.
            /// @param playerId Player id.
            /// @param furnIndex Furniture index of the tub.
            /// @param seat Seat number.
            /// @param sitterId Id of the other bather in the seat.
            void onBathing(std::int32_t playerId, int furnIndex, int seat, std::int32_t sitterId) override;

            /// @brief A player's bath ended.
            /// @param playerId Player id.
            void onBathEnd(std::int32_t playerId) override;

            /// @brief A crystal was mounted.
            /// @param bagSlot Bag place.
            /// @param stoneId Stone item id.
            void onCrystalMounted(int bagSlot, int stoneId) override;

            /// @brief The minutes left of the strength buffs.
            /// @param minutes Minutes per buff.
            // five strength buffs
            void onStrengthBuffTimes(const std::array<int, 5>& minutes) override;

            /// @brief Which strength buffs are on.
            /// @param mask Bit mask of the buffs.
            /// @param sound Whether to play a sound.
            void onStrengthBuffMask(std::uint32_t mask, bool sound) override;

            /// @brief Exp gain was paused or resumed.
            /// @param sub Sub id of the notice.
            /// @param paused True when paused.
            void onExpPause(int sub, bool paused) override;

            /// @brief A stat was enhanced.
            /// @param stat Stat id.
            void onStatEnhanced(int stat) override;

            /// @brief A body shift notice.
            /// @param sub Sub id.
            /// @param value Value.
            void onBodyShift(int sub, int value) override;

            /// @brief A player transforms into an NPC figure.
            /// @param playerId Player id.
            /// @param npcId NPC id of the figure.
            void onHumanTransform(std::int32_t playerId, int npcId) override;

            /// @brief Another player put on or took off an item.
            /// @param playerId Player id.
            /// @param itemId Item id.
            /// @param worn True when put on.
            void onPlayerEquipChanged(std::int32_t playerId, int itemId, bool worn) override;

            /// @brief The full list of items another player wears.
            /// @param playerId Player id.
            /// @param items Item ids.
            void onPlayerEquipList(std::int32_t playerId, const std::vector<int>& items) override;

            /// @brief A player's job (class).
            /// @param playerId Player id.
            /// @param job Job id.
            void onPlayerJob(std::int32_t playerId, int job) override;

            /// @brief A player's online status.
            /// @param playerId Player id.
            /// @param status Status code.
            void onPlayerOnlineStatus(std::int32_t playerId, int status) override;

            /// @brief The name of an item of Item.dat; the lines of the time effects name some.
            /// @param itemId Item id.
            /// @return The item's name.
            std::string itemNameOf(int itemId) const;

            /// @brief Asks to confirm the teleport to the training island.
            void confirmTrainingTeleport();

            /// @brief Asks the server for the time left of a time effect.
            /// @param tag The time effect's tag.
            void askTimeLeft(int tag);

            /// @brief Whether the player is on a training island map.
            /// @return True on one.
            bool inTrainingIslandMap() const;

            /// @brief Switches the body shift on or off.
            void toggleBodyShift();

            /// @brief A system line for the own player.
            /// @param text The line.
            void onOwnSystemLine(const std::string& text) override;

            /// @brief Plays a sound effect.
            /// @param relative Sound file path relative to the data folder.
            void onSoundEffect(const std::string& relative) override;

            /// @brief An event announcement.
            void onEventAnnounce() override;

            /// @brief A Corton place ranking entry.
            /// @param place Place.
            /// @param playerId Player id.
            /// @param name Player name.
            void onCortonPlace(int place, std::int32_t playerId, const std::string& name) override;

            /// @brief A talk-text notice.
            /// @param talkId Talk text id.
            /// @param durationMs Duration in milliseconds.
            void onTalkNotice(int talkId, int durationMs) override;

            /// @brief Skills were forgotten.
            /// @param skills Skill ids.
            void onSkillsForgotten(const std::vector<int>& skills) override;

            /// @brief Text floats over the player or the pet.
            /// @param pet True for the pet.
            /// @param text The text.
            void onFloatText(bool pet, const std::string& text) override;

            /// @brief A training is over.
            /// @param playerId Player id.
            /// @param flag Result flag.
            void onTrainingOver(std::int32_t playerId, int flag) override;

            /// @brief The training progress.
            /// @param pet True for a pet's training.
            /// @param place Pet place.
            /// @param percent Progress in percent.
            void onTrainingPercent(bool pet, int place, int percent) override;

            /// @brief The training was reset.
            /// @param pet True for a pet's training.
            /// @param place Pet place.
            void onTrainingReset(bool pet, int place) override;

            /// @brief The training is busy.
            void onTrainingBusy() override;

            /// @brief Exp from an event.
            /// @param value Exp amount.
            void onEventExp(std::int32_t value) override;

            /// @brief A contest level-up.
            /// @param pet True for a pet.
            /// @param level The new level.
            void onContestLevelUp(bool pet, std::int32_t level) override;

            /// @brief The server stopped the own player's walk (server 6/2).
            /// @param locked True when walking stays locked until the event ends.
            void onWalkStopped(bool locked) override;

            /// @brief A player jumped to a position.
            /// @param playerId Player id.
            /// @param position New position.
            void onPlayerJumped(std::int32_t playerId, core::Point position) override;

            /// @brief An NPC moves.
            /// @param npcIndex NPC index.
            /// @param to Destination.
            /// @param walk True to walk, false to jump there.
            void onNpcMoves(int npcIndex, core::Point to, bool walk) override;

            /// @brief An NPC's show state changed.
            /// @param npcIndex NPC index.
            /// @param state Show state.
            void onNpcShowState(int npcIndex, int state) override;

            /// @brief An NPC is hidden for a time.
            /// @param npcIndex NPC index.
            /// @param seconds Seconds hidden.
            void onNpcHidden(int npcIndex, int seconds) override;

            /// @brief An NPC is revealed for a time.
            /// @param npcIndex NPC index.
            /// @param seconds Seconds revealed.
            void onNpcRevealed(int npcIndex, int seconds) override;

            /// @brief An NPC is set to a frame.
            /// @param npcIndex NPC index.
            /// @param frame Frame number.
            void onNpcFrame(int npcIndex, int frame) override;

            /// @brief The full state of an NPC.
            /// @param npcIndex NPC index.
            /// @param holdFrame Frame the NPC holds.
            /// @param at Position.
            /// @param showState Show state.
            /// @param hideMs Milliseconds the NPC stays hidden.
            /// @param action Action id.
            void onNpcState(
                int npcIndex,
                int holdFrame,
                core::Point at,
                int showState,
                std::uint32_t hideMs,
                int action
            ) override;

            /// @brief A map sign changed.
            /// @param kind Sign kind.
            /// @param index Index.
            /// @param sign Sign value.
            void onMapSign(int kind, int index, int sign) override;

            /// @brief A player changed map (server action 12).
            /// @param playerId Player id.
            /// @param mapId New map id.
            /// @param position Position on the new map.
            void onMapChanged(std::int32_t playerId, int mapId, core::Point position) override;

            /// @brief The results of an event the server answered with.
            /// @param result The event result.
            void onEventResult(const EventResult& result) override;

            /// @brief An event signal.
            /// @param sub Sub id.
            /// @param value Signal value.
            void onEventSignal(int sub, std::int32_t value) override;

            /// @brief An NPC shows an emotion.
            /// @param npcIndex NPC index.
            /// @param emotionId Emotion id.
            void onNpcEmotion(int npcIndex, int emotionId) override;

            /// @brief An NPC plays an action.
            /// @param npcIndex NPC index.
            /// @param action Action id.
            /// @param holdFrame Frame to hold at the end.
            /// @param needsFrames Whether the action needs frame updates.
            void onNpcAction(int npcIndex, int action, int holdFrame, bool needsFrames) override;

            /// @brief The result of buying from an NPC store.
            /// @param result Result code.
            void onStoreBought(int result) override;

            /// @brief The result of selling to an NPC store.
            /// @param result Result code.
            void onStoreSold(int result) override;

            /// @brief A sale form opened.
            /// @param goods True for the goods sale, false for the other.
            void onSaleOpened(bool goods) override;

            /// @brief The gold held in the bank.
            /// @param gold Gold amount.
            void onBankGold(std::int32_t gold) override;

            /// @brief Gold moved to or from the bank.
            /// @param amount Gold amount moved.
            void onBankGoldMoved(std::int32_t amount) override;

            /// @brief The bank is full.
            void onBankFull() override;

            /// @brief The bank opened.
            /// @param items True for the item bank, false for the money bank.
            void onBankOpened(bool items) override;

            /// @brief A bank gold transaction finished.
            /// @param deposit True for a deposit, false for a withdrawal.
            void onBankGoldDone(bool deposit) override;

            /// @brief An item was put into a bank place.
            /// @param record The bag item record.
            void onBankItemPut(const protocol::BagItemRecord& record) override;

            /// @brief An item was added to the bank.
            /// @param item The item.
            void onBankItemAdded(const protocol::ItemInstance& item) override;

            /// @brief Items were removed from the bank.
            /// @param itemId Item id.
            /// @param count Count removed.
            void onBankItemRemoved(int itemId, int count) override;

            /// @brief A bank place's count decreased.
            /// @param slot Bank place.
            /// @param count Count removed.
            void onBankSlotDecreased(int slot, int count) override;

            /// @brief A bank item transaction finished.
            /// @param deposit True for a deposit, false for a withdrawal.
            void onBankItemsDone(bool deposit) override;

            /// @brief The bank's items were cleared.
            void onBankCleared() override;

            /// @brief The things on the ground of the map.
            /// @param records Ground item records.
            void onGroundItems(const std::vector<protocol::GroundItemRecord>& records) override;

            /// @brief An item appeared on the ground.
            /// @param itemId Item id.
            /// @param position Position.
            /// @param extra Extra value of the item.
            /// @param appearMode How it appears.
            void onGroundItemAdded(int itemId, core::Point position, std::int32_t extra, int appearMode) override;

            /// @brief Gold appeared on the ground.
            /// @param gold Gold amount.
            /// @param position Position.
            /// @param dropped True when dropped by a player.
            void onGroundGoldAdded(std::int32_t gold, core::Point position, bool dropped) override;

            /// @brief A thing that appears flies from the player. Original: AddDropAnim.
            /// @param groundIndex Index of the ground thing.
            void flyFromPlayer(int groundIndex);

            /// @brief A ground thing was removed.
            /// @param index Ground index.
            /// @param pickedUp True when picked up.
            void onGroundItemRemoved(int index, bool pickedUp) override;

            /// @brief An item was obtained.
            /// @param item The item.
            void onItemObtained(const protocol::ItemInstance& item) override;

            /// @brief A warehouse item.
            /// @param record The bag item record.
            void onWarehouseItem(const protocol::BagItemRecord& record) override;

            /// @brief A warehouse place was cleared.
            /// @param slot Warehouse place.
            void onWarehouseSlotCleared(int slot) override;

            /// @brief The warehouse opened.
            void onWarehouseOpened() override;

            /// @brief An item of the sale store.
            /// @param record The bag item record.
            void onSaleStoreItem(const protocol::BagItemRecord& record) override;

            /// @brief A sale store place was cleared.
            /// @param slot Store place.
            void onSaleStoreCleared(int slot) override;

            /// @brief The list of the web sale.
            /// @param list The sale list.
            void onSaleNetList(const protocol::SaleNetList& list) override;

            /// @brief The sale menu opened.
            void onSaleMenuOpened() override;

            /// @brief A sale request is done.
            void onSaleRequestDone() override;

            /// @brief A web sale error.
            /// @param code Error code.
            void onSaleWebError(int code) override;

            /// @brief The sale gold changed.
            /// @param amount Gold amount.
            /// @param added True when added, false when taken.
            void onSaleGoldChanged(std::int32_t amount, bool added) override;

            /// @brief The guild weapon bank's count of a kind.
            /// @param kind Weapon kind.
            /// @param count Count.
            void onGuildWeaponCount(int kind, std::int32_t count) override;

            /// @brief An item decreased.
            /// @param itemId Item id.
            /// @param kind Kind of the decrease.
            void onItemDecreased(int itemId, int kind) override;

            /// @brief Plays the item sound.
            void onItemSound() override;

            /// @brief Items were gained into the bag.
            /// @param records Bag item records.
            void onBagItemsGained(const std::vector<protocol::BagItemRecord>& records) override;

            /// @brief Gifts that did not fit into a full bag.
            /// @param items The gift items.
            void onBagFullGifts(const std::vector<protocol::ItemInstance>& items) override;

            /// @brief Exp increased.
            /// @param playerId Player id.
            /// @param petPlace Pet place (0: the player).
            /// @param exp Exp gained.
            void onExpIncreased(std::int32_t playerId, int petPlace, std::int32_t exp) override;

            /// @brief An item glows.
            /// @param itemId Item id.
            void onItemGlows(int itemId) override;

            /// @brief The result of a recommendation.
            /// @param result Result code.
            void onRecommendResult(int result) override;

            /// @brief The recommendation points.
            /// @param points Points.
            void onRecommendPoints(std::int32_t points) override;

            /// @brief A Carnie team invitation was refused.
            /// @param playerId Player id who refused.
            void onCarnieTeamRefused(std::int32_t playerId) override;

            /// @brief The auto heal potion uses left.
            /// @param times Uses.
            void onAutoHealPotion(int times) override;

            /// @brief The soldier time left.
            /// @param minutes Minutes.
            void onSoldierTime(int minutes) override;

            /// @brief A pet on the map changed its kind.
            /// @param playerId Owner id.
            /// @param petKind Old kind.
            /// @param newKind New kind.
            void onScenePetChanged(std::int32_t playerId, int petKind, int newKind) override;

            /// @brief An auto pill was used.
            /// @param kind Pill kind.
            /// @param count Count used.
            void onAutoPillUsed(int kind, int count) override;

            /// @brief A fighter abstains from the fight.
            /// @param playerId Player id.
            void onFighterAbstains(std::int32_t playerId) override;

            /// @brief A pet's gear goes back home.
            /// @param npcId Pet NPC id.
            void onPetGearBackHome(int npcId) override;

            /// @brief A fight help notice.
            /// @param mode Help mode.
            /// @param helperId Helper id.
            /// @param kind Help kind.
            void onFightHelp(int mode, std::int32_t helperId, int kind) override;

            /// @brief A fighter moved on the battle field.
            /// @param fromRow Source row.
            /// @param fromCol Source column.
            /// @param toRow Target row.
            /// @param toCol Target column.
            void onFighterMoved(int fromRow, int fromCol, int toRow, int toCol) override;

            /// @brief The inn's price for a rest.
            /// @param price Price in gold.
            void onInnPrice(std::int32_t price) override;

            /// @brief The inn's answer to the rest request.
            /// @param answer Answer code.
            void onInnAnswer(int answer) override;

            /// @brief The inn sleep starts.
            void onInnSleep() override;

            /// @brief The pets in storage.
            /// @param pets Pet records.
            void onStoredPets(const std::vector<protocol::PetRecord>& pets) override;

            /// @brief The pet storage opened.
            void onPetStorageOpened() override;

            /// @brief A pet was stored.
            /// @param place Storage place.
            /// @param petSlot Pet place in the team.
            void onPetStored(int place, int petSlot) override;

            /// @brief A stored pet was taken.
            /// @param place Storage place.
            /// @param petSlot Pet place in the team.
            void onStoredPetTaken(int place, int petSlot) override;

            /// @brief A pet was swapped with a stored one.
            /// @param petSlot Pet place in the team.
            /// @param place Storage place.
            void onInnPetSwapped(int petSlot, int place) override;

            /// @brief A stored pet place was cleared.
            /// @param place Storage place.
            void onStoredPetCleared(int place) override;

            /// @brief A pet storage request is done.
            void onPetStorageDone() override;

            /// @brief A pet storage request was refused.
            /// @param storerHasIt True when the storer already has the pet.
            void onPetStorageRefused(bool storerHasIt) override;

            /// @brief An event finished.
            void onEventFinished() override;

            /// @brief The skills used most recently in battle.
            /// @param skills Skill ids.
            // the three most recent battle skills
            void onRecentBattleSkills(const std::array<int, 3>& skills) override;

            /// @brief Fighters join the battle.
            /// @param fighters Fighter records.
            void onFightersJoin(const std::vector<protocol::FighterRecord>& fighters) override;

            /// @brief The weapon looks of the fighters.
            /// @param looks Weapon looks.
            void onFighterWeaponLooks(const std::vector<protocol::FighterWeaponLook>& looks) override;

            /// @brief The hotkey bar contents.
            /// @param records Hotkey records.
            void onHotKeys(const std::vector<protocol::HotKeyRecord>& records) override;

            /// @brief The mall (IM) points.
            /// @param points Points.
            void onImPoints(std::int32_t points) override;

            /// @brief The mall catalog.
            /// @param rows Catalog rows.
            void onMallCatalog(const std::vector<protocol::MallRow>& rows) override;

            /// @brief Mall points were spent.
            /// @param points Points left.
            /// @param spent Points spent.
            /// @param itemId Item bought.
            /// @param count Count bought.
            void onMallPointsSpent(std::int32_t points, std::int32_t spent, int itemId, int count) override;

            /// @brief The mall bonus points.
            /// @param points Bonus points.
            void onMallBonusPoints(std::int32_t points) override;

            /// @brief Mall bonus points were spent.
            /// @param points Bonus points left.
            /// @param spent Bonus points spent.
            /// @param itemId Item bought.
            /// @param count Count bought.
            void onMallBonusSpent(std::int32_t points, std::int32_t spent, int itemId, int count) override;

            /// @brief The result of a mall purchase.
            /// @param itemId Item id.
            /// @param category Mall category.
            /// @param count Count.
            /// @param key Purchase key.
            /// @param ok True when the purchase succeeded.
            void onMallBought(
                int itemId,
                int category,
                int count,
                int key,
                bool ok
            ) override;

            /// @brief A mall purchase was refused.
            /// @param reason Reason code.
            void onMallRefused(int reason) override;

            /// @brief The mall's games tab opens or closes.
            /// @param open True to open.
            void onMallGamesTab(bool open) override;

            /// @brief An update of the mall catalog.
            /// @param rows Updated rows.
            void onMallCatalogUpdate(const std::vector<protocol::MallRow>& rows) override;

            /// @brief The result of a mall forge.
            /// @param outcome Outcome code.
            /// @param bagSlot Bag place.
            /// @param fusionLevel Fusion level reached.
            void onMallForgeResult(int outcome, int bagSlot, int fusionLevel) override;

            /// @brief The guild's info (CY_Organ.pas), in WorldSceneGuild.cpp.
            /// @param info The guild info.
            void onGuildInfo(const protocol::GuildInfo& info) override;

            /// @brief A player's guild title.
            /// @param playerId Player id.
            /// @param guildId Guild id.
            /// @param guild Guild name.
            void onGuildTitle(std::int32_t playerId, std::uint32_t guildId, const std::string& guild) override;

            /// @brief A player's PvP victories.
            /// @param playerId Player id.
            /// @param wins Win count.
            void onPvpVictories(std::int32_t playerId, int wins) override;

            /// @brief A player's PvP rank.
            /// @param playerId Player id.
            /// @param rank Rank.
            void onPvpRank(std::int32_t playerId, int rank) override;

            /// @brief A player's arena victories.
            /// @param playerId Player id.
            /// @param victories Victory count.
            void onArenaVictory(std::int32_t playerId, int victories) override;

            /// @brief The flee-killing event begins or ends (77/1 begins it, 77/3 ends it).
            /// @param code Event code.
            void onFleeKillEvent(int code) override;

            /// @brief The survival victories.
            /// @param victories Victory count.
            void onSurvivalVictory(int victories) override;

            /// @brief The list of guild badges that exist.
            /// @param listed The badge records.
            void onBadgeList(const std::vector<BadgeRecord>& listed) override;

            /// @brief Guild badges received.
            /// @param badges The badge records.
            void onBadges(const std::vector<BadgeRecord>& badges) override;

            /// @brief The own guild's badge.
            /// @param version Badge version.
            /// @param pixels Badge pixel data.
            void onOwnBadge(int version, const core::Bytes& pixels) override;

            /// @brief A guild's badge was reset to a sample.
            /// @param guildId Guild id.
            /// @param sampleNumber Sample badge number.
            void onBadgeReset(std::uint32_t guildId, int sampleNumber) override;

            /// @brief The guild creation form opens.
            void onGuildCreateForm() override;

            /// @brief The result of creating a guild.
            /// @param result Result code.
            void onGuildCreateResult(int result) override;

            /// @brief A guild invitation arrived.
            /// @param inviterId Inviter id.
            void onGuildInvite(std::int32_t inviterId) override;

            /// @brief The answer to a guild invitation.
            /// @param targetId Invited player id.
            /// @param answer Answer code.
            void onGuildInviteAnswer(std::int32_t targetId, int answer) override;

            /// @brief A guild member is gone.
            /// @param id Member id.
            void onGuildMemberGone(std::int32_t id) override;

            /// @brief A guild mail arrived.
            /// @param senderId Sender id.
            /// @param sendTime Send time.
            /// @param text Mail text.
            void onGuildMail(std::int32_t senderId, double sendTime, const std::string& text) override;

            /// @brief The result of leaving a guild.
            /// @param result Result code.
            /// @param id Member id.
            void onGuildLeaveResult(int result, std::int32_t id) override;

            /// @brief A member joined the guild.
            /// @param id Member id.
            void onGuildMemberJoined(std::int32_t id) override;

            /// @brief The guild was disbanded.
            /// @param result Result code.
            void onGuildDisbanded(int result) override;

            /// @brief The guild rules text.
            /// @param rules The rules.
            void onGuildRules(const std::string& rules) override;

            /// @brief The date the guild will be disbanded.
            /// @param date The date.
            void onGuildDisbandDate(double date) override;

            /// @brief A vice lord was appointed or dismissed.
            /// @param id Member id.
            /// @param appointed True when appointed.
            void onGuildVice(std::int32_t id, bool appointed) override;

            /// @brief The guild has a new lord.
            /// @param mode Change mode.
            /// @param id New lord's id.
            void onGuildNewLord(int mode, std::int32_t id) override;

            /// @brief A guild position change failed.
            /// @param code Error code.
            void onGuildPositionError(int code) override;

            /// @brief The lord offers the vice post.
            /// @param lordId Lord's id.
            void onGuildViceOffer(std::int32_t lordId) override;

            /// @brief The lord offers the lord post.
            /// @param lordId Lord's id.
            void onGuildLordOffer(std::int32_t lordId) override;

            /// @brief A guild post was accepted.
            /// @param post Post id.
            /// @param id Member id.
            void onGuildPostAccepted(int post, std::int32_t id) override;

            /// @brief A member's contribution.
            /// @param id Member id.
            /// @param contribution Contribution value.
            void onGuildContribution(std::int32_t id, std::int32_t contribution) override;

            /// @brief A member's guild right changed.
            /// @param id Member id.
            /// @param right Right id.
            /// @param on True when granted.
            void onGuildRight(std::int32_t id, int right, bool on) override;

            /// @brief The guild welcome.
            void onGuildWelcome() override;

            /// @brief The online states of the guild members.
            /// @param states The member states.
            void onGuildStates(const std::vector<protocol::FriendState>& states) override;

            /// @brief Notice board rows (MK_Menu.pas), in WorldSceneNotices.cpp.
            /// @param guild True for the guild's board, false for the public one.
            /// @param rows The rows.
            void onNoticeRows(bool guild, const std::vector<protocol::NoticeRow>& rows) override;

            /// @brief Notice board page info.
            /// @param guild True for the guild's board.
            /// @param info Page info.
            /// @param restoreAsked True when a restore was asked.
            void onNoticePages(bool guild, const protocol::NoticePageInfo& info, bool restoreAsked) override;

            /// @brief The content of a notice.
            /// @param guild True for the guild's board.
            /// @param content The notice content.
            void onNoticeContent(bool guild, const protocol::NoticeContent& content) override;

            /// @brief The notice write form opens.
            /// @param guild True for the guild's board.
            void onNoticeWriteForm(bool guild) override;

            /// @brief The notice board opened.
            void onNoticeBoardOpened() override;

            /// @brief A notice board message.
            /// @param text The message.
            /// @param durationMs Duration in milliseconds.
            void onNoticeMessage(const std::string& text, int durationMs) override;

            /// @brief The hooks the notice boards call back into the scene with.
            /// @return The hooks.
            NoticeBoard::Hooks noticeBoardHooks();

            /// @brief The Top 100 opened (LD_Top100.pas).
            void onTop100Opened() override;

            /// @brief The Top 100 list.
            /// @param payload List payload.
            void onTop100List(const core::Bytes& payload) override;

            /// @brief The hooks the Top 100 calls back into the scene with.
            /// @return The hooks.
            Top100::Hooks top100Hooks();

            /// @brief A marriage message (MK_Menu.pas, 45/x), in WorldSceneMarriage.cpp.
            /// @param payload Message payload.
            void onMarriageMessage(const core::Bytes& payload) override;

            /// @brief The hooks marriage calls back into the scene with.
            /// @return The hooks.
            Marriage::Hooks marriageHooks();

            /// @brief UseGridItem for the items the scene handles: the divorce item, the Transfer Mirror.
            /// @param itemId Item id.
            /// @param bagSlot Bag place.
            /// @return True when the scene handled the item.
            bool useSpecialItem(int itemId, int bagSlot);

            /// @brief A tent message (Re_Camp.pas, 65/x), in WorldSceneCamps.cpp.
            /// @param payload Message payload.
            void onTentMessage(const core::Bytes& payload) override;

            /// @brief A fish tank message (23/158, 23/159).
            /// @param payload Message payload.
            void onFishTankMessage(const core::Bytes& payload) override;

            /// @brief A Maid action message.
            /// @param payload Message payload.
            void onMaidAction(const core::Bytes& payload) override;

            /// @brief A lucky bag message.
            /// @param payload Message payload.
            void onLuckyBagMessage(const core::Bytes& payload) override;

            /// @brief A GPS mark was set.
            /// @param point GPS point number.
            /// @param map Map id.
            /// @param x Map x.
            /// @param y Map y.
            void onGpsMark(int point, int map, int x, int y) override;

            /// @brief A picnic message.
            /// @param payload Message payload.
            void onPicnicMessage(const core::Bytes& payload) override;

            /// @brief A reborn wish message.
            /// @param payload Message payload.
            void onRebornWishMessage(const core::Bytes& payload) override;

            /// @brief An exp box message.
            /// @param payload Message payload.
            void onExpBoxMessage(const core::Bytes& payload) override;

            /// @brief A black board message.
            /// @param payload Message payload.
            void onBlackBoardMessage(const core::Bytes& payload) override;

            /// @brief The id and password form opens.
            /// @param mode Form mode.
            void onIdAndPwForm(int mode) override;

            /// @brief The cafe id form opens.
            void onCafeIdForm() override;

            /// @brief A point store message.
            /// @param payload Message payload.
            void onPointStoreMessage(const core::Bytes& payload) override;

            /// @brief A gift card message.
            /// @param payload Message payload.
            void onGiftCardMessage(const core::Bytes& payload) override;

            /// @brief An item was dismantled.
            /// @param container Container id.
            /// @param slot Place in the container.
            /// @param wear Wear of the item.
            /// @param petPlace Pet place when a pet's item.
            void onDismantled(int container, int slot, std::int32_t wear, int petPlace) override;

            /// @brief A step of the confuse stone.
            /// @param step Step number.
            /// @param bagSlot Bag place.
            /// @param stoneItemId Stone item id.
            void onConfuseStoneResult(int step, int bagSlot, int stoneItemId) override;

            /// @brief The result of reinforcing an item.
            /// @param code Result code.
            /// @param bagSlot Bag place.
            /// @param level Level reached.
            void onReinforceResult(int code, int bagSlot, int level) override;

            /// @brief The result of the magic box.
            /// @param code Result code.
            void onMagicBoxResult(int code) override;

            /// @brief The result of packing.
            /// @param code Result code.
            void onPackingResult(int code) override;

            /// @brief The result of the Magnifying Glass.
            /// @param payload Result payload.
            void onReferResult(const core::Bytes& payload) override;

            /// @brief A Memory Card / puppet training packet (WorldScenePuppet.cpp).
            /// @param packet The packet.
            void onPuppetPacket(const protocol::PuppetPacket& packet) override;

            /// @brief The hooks the puppet substitute window calls back with.
            /// @return The hooks.
            InsteadEcisWindow::Hooks insteadEcisHooks();

            /// @brief The hooks the Memory Card window calls back with.
            /// @return The hooks.
            MemCardWindow::Hooks memCardHooks();

            /// @brief A world player as the Memory Card's forms hold one.
            /// @param playerId Player id.
            /// @return The person, or none.
            std::optional<protocol::PuppetPerson> puppetPersonOf(std::int32_t playerId) const;

            /// @brief Shows the puppet training form. Original: ShowPuppetTrainForm.
            void showPuppetForm();

            /// @brief The result of the sewing kit.
            /// @param code Result code.
            /// @param bagSlot Bag place.
            /// @param look New look.
            void onSewingKitResult(int code, int bagSlot, int look) override;

            /// @brief A forage found an item.
            /// @param itemId Item id.
            /// @param count Count.
            void onForageFound(int itemId, int count) override;

            /// @brief A forage ended.
            /// @param boneSlot Bone's bag place.
            /// @param petPlace Pet place.
            /// @param code End code.
            void onForageEnded(int boneSlot, int petPlace, int code) override;

            /// @brief A forage bone was used.
            /// @param boneSlot Bone's bag place.
            void onForageBone(int boneSlot) override;

            /// @brief A player's colour of a body part changed.
            /// @param playerId Player id.
            /// @param part Body part.
            /// @param code Colour code.
            void onPlayerColors(std::int32_t playerId, int part, std::int32_t code) override;

            /// @brief The state of a pet express.
            /// @param petKind Pet kind.
            /// @param state State.
            void onExpressState(std::int32_t petKind, int state) override;

            /// @brief A pet walk mode.
            /// @param petKind Pet kind.
            /// @param mode Walk mode.
            void onPetWalk(std::int32_t petKind, int mode) override;

            /// @brief An NPC walks by.
            /// @param npcId NPC id.
            /// @param mode Walk mode.
            void onNpcWalkBy(std::int32_t npcId, int mode) override;

            /// @brief A pet express failed.
            /// @param code Failure code.
            /// @param itemId Item id.
            void onExpressFailed(int code, int itemId) override;

            /// @brief The result of a pet express.
            /// @param kind Result kind.
            /// @param otherId Other player's id.
            /// @param itemId Item id.
            /// @param count Count.
            void onExpressResult(int kind, std::int32_t otherId, int itemId, int count) override;

            /// @brief A tent style change was refused.
            /// @param code Refusal code.
            void onTentStyleRefused(int code) override;

            /// @brief The tent style chooser opens.
            /// @param bagSlot Bag place.
            /// @param set Style set.
            void onTentStyleChooser(int bagSlot, int set) override;

            /// @brief The hooks the camps call back into the scene with.
            /// @return The hooks.
            Camps::Hooks campsHooks();

            /// @brief The hooks the camp style window calls back with.
            /// @return The hooks.
            CampStyleWindow::Hooks campStyleHooks();

            /// @brief Refreshes the owners of the camps on the map.
            void refreshCampOwners();

            /// @brief Whether the player is on a Trojan war map.
            /// @return True on one.
            bool onTrojanWarMap() const;

            /// @brief Whether a player is a disguised one of the other side, whom the own player does not see. Original: IsTrojanWarTarget.
            /// @param other The other player.
            /// @return True when hidden from the own player.
            bool trojanWarTarget(const MapCharacter& other) const;

            /// @brief Whether the player is on a Trojan battle map (UpdateTimeEffectIcons resets the flag outside them).
            /// @return True on one.
            bool onTrojanBattleMap() const;

            /// @brief The Leave battle icon (HandleTimeEffectClick 6).
            void leaveTrojanWar();

            /// @brief The guild window's teleport to the castle, with the GPS's checks and light.
            /// @details Original: TSe_TeamForm.RequestTeleport (FTeamFormTeleport: 70/7 at its end).
            void castleTeleport();

            /// @brief Resets the Trojan war flag outside the battle maps.
            void resetTrojanWarFlag();

            /// @brief The camp under a screen point.
            /// @param screenPoint Screen point.
            /// @return The camp's place, or none.
            std::optional<int> campAt(core::Point screenPoint);

            /// @brief A click on a camp.
            /// @param slot Camp place.
            void clickCamp(int slot);

            /// @brief Takes the own tent back.
            void takeBackTent();

            /// @brief Uses a tent item.
            /// @param itemId Item id.
            /// @param bagSlot Bag place.
            /// @return True when handled.
            bool useTentItem(int itemId, int bagSlot);

            /// @brief A right click on a bag item.
            /// @param itemId Item id.
            /// @param bagSlot Bag place.
            void rightClickItem(int itemId, int bagSlot);

            /// @brief A vehicle was placed on the map (DZ_Human.pas CH_TTrafficManage, 15/x and 23/102), in WorldSceneVehicles.cpp.
            /// @param bagSlot Bag place.
            /// @param ownerId Owner id.
            /// @param itemId Item id.
            /// @param position Position.
            /// @param board True when the owner boards it.
            void onVehiclePlaced(
                int bagSlot,
                std::int32_t ownerId,
                int itemId,
                core::Point position,
                bool board
            ) override;

            /// @brief A player boarded a vehicle.
            /// @param bagSlot Bag place.
            /// @param playerId Player id.
            /// @param itemId Item id.
            void onVehicleBoarded(int bagSlot, std::int32_t playerId, int itemId) override;

            /// @brief A player left a vehicle.
            /// @param bagSlot Bag place.
            /// @param playerId Player id.
            void onVehicleDismounted(int bagSlot, std::int32_t playerId) override;

            /// @brief A vehicle's wear and fuel.
            /// @param bagSlot Bag place.
            /// @param ownerId Owner id.
            /// @param wear Wear.
            /// @param fuel Fuel.
            void onVehicleWear(int bagSlot, std::int32_t ownerId, std::int32_t wear, int fuel) override;

            /// @brief The vehicle is out of fuel.
            void onVehicleNoFuel() override;

            /// @brief The result of a vehicle repair.
            /// @param code Result code.
            /// @param wear Wear after the repair.
            void onVehicleRepair(int code, std::int32_t wear) override;

            /// @brief The parking lot's vehicles.
            /// @param cells Parked vehicle cells.
            void onParkList(const std::vector<protocol::ParkedVehicle>& cells) override;

            /// @brief One parking cell changed.
            /// @param cell The parked vehicle cell.
            void onParkSlot(const protocol::ParkedVehicle& cell) override;

            /// @brief The result of a parking request.
            /// @param code Result code.
            /// @param itemId Vehicle item id.
            void onParkResult(int code, int itemId) override;

            /// @brief The parking lot was cleared.
            void onParkClear() override;

            /// @brief The parking window opens.
            /// @param code Open code.
            void onParkOpen(int code) override;

            /// @brief The riders on the map.
            /// @param riders Rider records.
            void onSceneRiders(const std::vector<protocol::SceneRider>& riders) override;

            /// @brief The own vehicle was taken back.
            /// @param bagSlot Bag place.
            void onVehicleTakenBack(int bagSlot) override;

            /// @brief A vehicle was removed from the map.
            /// @param ownerId Owner id.
            /// @param itemId Item id.
            void onVehicleRemoved(std::int32_t ownerId, int itemId) override;

            /// @brief A vehicle was destroyed.
            /// @param ownerId Owner id.
            /// @param itemId Item id.
            void onVehicleDestroyed(std::int32_t ownerId, int itemId) override;

            /// @brief The vehicles standing on the map.
            /// @param vehicles Vehicle records.
            void onSceneVehicles(const std::vector<protocol::SceneVehicle>& vehicles) override;

            /// @brief The hooks the vehicles call back into the scene with.
            /// @return The hooks.
            Vehicles::Hooks vehiclesHooks();

            /// @brief The hooks the capsule window calls back with.
            /// @return The hooks.
            CapsuleWindow::Hooks capsuleHooks();

            /// @brief The hooks the transport window calls back with.
            /// @return The hooks.
            TransportWindow::Hooks transportHooks();

            /// @brief UseGridItem for a vehicle of the bag: the checks, then 15/9.
            /// @param bagSlot Bag place.
            void useVehicleItem(int bagSlot);

            /// @brief Whether an item is a fishing rod. Original: IsFishingRodItem (item sub type 107 with a rank, which is the rod's kind).
            /// @param item The item record.
            /// @return True for a fishing rod.
            static bool isFishingRod(const formats::ItemRecord& item);

            /// @brief UseGridItem for a fishing rod: StartFishing or StopFishing; the item window closes.
            /// @param bagSlot Bag place.
            /// @param rodKind The rod's kind.
            void useFishingRod(int bagSlot, int rodKind);

            /// @brief StartFishing: the refusals, then the pose facing the water and 23/53.
            void startFishing();

            /// @brief StopFishing: 23/54 and the rod's mark freed.
            void stopFishing();

            /// @brief The own fishing is over (the rod's mark freed).
            void clearFishing();

            /// @brief A player started fishing.
            /// @param playerId Player id.
            /// @param rodKind The rod's kind.
            void onFishingStarted(std::int32_t playerId, int rodKind) override;

            /// @brief A player stopped fishing.
            /// @param playerId Player id.
            void onFishingStopped(std::int32_t playerId) override;

            /// @brief UseGridItem for a fuel of a vehicle and a voucher for one: the checks before the item is used.
            /// @param item The item record.
            /// @return True when refused.
            bool vehicleSupplyRefused(const formats::ItemRecord& item);

            /// @brief The status panel's picture of the vehicle ridden.
            /// @return The gauge, or none (for a passenger).
            std::optional<VehicleGauge> vehicleGauge() const;

            /// @brief Mounts a vehicle, with the passengers of a driver. Original: MountVehicle (LoadVehiclePassengers).
            /// @param rider The rider.
            /// @param vehicle The vehicle.
            void mountVehicle(MapCharacter& rider, RiddenVehicle vehicle);

            /// @brief Dismounts a vehicle, with the passengers of a driver. Original: DismountVehicle (UnloadVehiclePassengers).
            /// @param rider The rider.
            void dismountVehicle(MapCharacter& rider);

            /// @brief Loads the passengers of a driver. Original: LoadVehiclePassengers.
            /// @param driver The driver.
            void loadPassengers(MapCharacter& driver);

            /// @brief Unloads the passengers of a driver. Original: UnloadVehiclePassengers.
            /// @param driver The driver.
            void unloadPassengers(MapCharacter& driver);

            /// @brief The vehicle standing on the map under a screen point (the hover target of kind 19).
            /// @param screenPoint Screen point.
            /// @return The vehicle's place, or none.
            std::optional<int> vehicleAt(core::Point screenPoint);

            /// @brief A click on a vehicle. Original: HandleHumanClick of a vehicle.
            /// @param slot Vehicle place.
            void clickVehicle(int slot);

            /// @brief Tries to board a vehicle standing on the map. Original: TryBoardSceneVehicle.
            /// @param slot Vehicle place.
            void tryBoardVehicle(int slot);

            /// @brief Tries to board the own vehicle. Original: TryBoardOwnVehicle.
            /// @param slot Vehicle place.
            void tryBoardOwnVehicle(int slot);

            /// @brief Parks the vehicle ridden. Original: TryParkRiddenVehicle.
            void parkRiddenVehicle();

            /// @brief Rides a vehicle item of the bag. Original: RideVehicleItem.
            /// @param bagSlot Bag place.
            void rideVehicleItem(int bagSlot);

            /// @brief FindMovePath's vehicle logic for a repeated walk to the same destination.
            /// @details Walking into the water with a boat in the bag puts it on the water (15/14); walking from the water onto land leaves the boat (15/10).
            /// @param clicked The clicked map point.
            void walkIntoWaterOrOut(core::Point clicked);

            /// @brief Puts a character at a map position. Original: SetHumanPos of a player.
            /// @param character The character.
            /// @param position The position.
            /// @param followWithCamera Whether the camera follows the character.
            void placeCharacter(MapCharacter& character, core::Point position, bool followWithCamera = true);

            /// @brief Whether water lies within 6 cells of the player.
            /// @return True when near water.
            bool nearWater() const;

            /// @brief Whether a map is a quiz zone.
            /// @param mapId Map id.
            /// @return True for a quiz zone map.
            static bool isQuizZoneMap(int mapId);

            /// @brief A mini game starts (CY_Sport.pas, 57/x and 71/x), in WorldSceneSports.cpp.
            /// @param kind Game kind.
            /// @param npcId NPC id.
            /// @param level Game level.
            /// @param scene Scene id.
            void onSportStart(int kind, int npcId, int level, int scene) override;

            /// @brief The mini game ends.
            void onSportEnd() override;

            /// @brief A story movie starts.
            /// @param index Movie index.
            void onStoryMovie(int index) override;

            /// @brief The movie finished.
            void onMovieFinish() override;

            /// @brief A lucky wheel message.
            /// @param payload Message payload.
            void onWheelMessage(const core::Bytes& payload) override;

            /// @brief A Christmas event announcement.
            /// @param text The announcement.
            void onChristmasAnnounced(const std::string& text) override;

            /// @brief An NPC vanished.
            /// @param npcIndex NPC index.
            void onNpcVanished(int npcIndex) override;

            /// @brief The Christmas event turns on or off.
            /// @param on True when on.
            void onChristmasEvent(bool on) override;

            /// @brief The server asks the client to log in again.
            /// @param toPvp True when to the PvP server.
            void onServerRelogin(bool toPvp) override;

            /// @brief The New Year event turns on or off.
            /// @param on True when on.
            void onNewYearEvent(bool on) override;

            /// @brief The lucky 7 count.
            /// @param count Count.
            void onLucky7Count(int count) override;

            /// @brief The conditions the scene effects depend on.
            /// @return The conditions.
            SceneEffects::Conditions effectConditions() const;

            /// @brief The type of the current scene.
            /// @return The scene type.
            int sceneType() const;

            /// @brief A poke game message.
            /// @param payload Message payload.
            void onPokeMessage(const core::Bytes& payload) override;

            /// @brief A quiz game message.
            /// @param payload Message payload.
            void onQuizMessage(const core::Bytes& payload) override;

            /// @brief A compass game message.
            /// @param payload Message payload.
            void onCompassMessage(const core::Bytes& payload) override;

            /// @brief A Five in a Row message.
            /// @param payload Message payload.
            void onGobangMessage(const core::Bytes& payload) override;

            /// @brief The hooks the quiz game calls back with.
            /// @return The hooks.
            QuizGame::Hooks quizHooks();

            /// @brief The hooks the compass game calls back with.
            /// @return The hooks.
            CompassGame::Hooks compassHooks();

            /// @brief A slot machine message.
            /// @param payload Message payload.
            void onMachineMessage(const core::Bytes& payload) override;

            /// @brief A GM question to answer (Player.FGmQuestion).
            /// @param kind Question kind.
            /// @param question The question text.
            /// @param choices The answer choices.
            void onGmQuestion(int kind, const std::string& question, const std::vector<std::string>& choices) override;

            /// @brief The qol server's GM status (16/200).
            /// @param flags Status flags (bit 0: GM).
            void onGmStatus(int flags) override;

            /// @brief Answers the GM question.
            /// @param answer The chosen answer.
            void answerGmQuestion(int answer);

            /// @brief The hooks the mini games call back with.
            /// @return The hooks.
            GameHooks sportHooks();

            /// @brief The hooks the movie director calls back with.
            /// @return The hooks.
            MovieDirector::Hooks movieHooks();

            /// @brief Starts the movie an event result names.
            /// @param result The event result.
            /// @return True when a movie started.
            bool startEventMovie(const EventResult& result);

            /// @brief The hooks the lucky wheel calls back with.
            /// @return The hooks.
            LuckyWheel::Hooks luckyWheelHooks();

            /// @brief The hooks the game start window calls back with.
            /// @return The hooks.
            GameStartWindow::Hooks gameStartHooks();

            /// @brief Ends the running mini game.
            /// @param result The game's result.
            void endSportGame(int result);

            /// @brief The sport scene of a game kind (CY_Sport.pas: what FActiveSport dispatches the update, draw, mouse hooks and 71/x messages by).
            /// @param kind Game kind.
            /// @return The scene, or none for the games that are forms.
            std::optional<std::reference_wrapper<SportScene>> sportScene(int kind);

            /// @brief Shows or hides the HUD.
            /// @param shown True to show it.
            void setHudShown(bool shown);

            /// @brief Closes the open game windows.
            void closeWindows();

            /// @brief The career form opens (MZ_Menu.pas, 79/x), in WorldSceneCareer.cpp.
            void onCareerForm() override;

            /// @brief The result of a career change.
            /// @param code Result code.
            void onCareerResult(int code) override;

            /// @brief The hooks the career forms call back with.
            /// @return The hooks.
            CareerHooks careerHooks();

            /// @brief Shows the class form of the line the stats suggest, or the first choice. Original: THuman.ShowCareerForm.
            /// @param changeMode The change mode.
            void showCareerForm(int changeMode);

            /// @brief The bag place of the first item of a kind. Original: FindBagItemSlotByType.
            /// @param subType Item sub type.
            /// @return The bag place (0 when none).
            int bagSlotOfKind(int subType) const;

            /// @brief The hooks the marriage card window calls back with.
            /// @return The hooks.
            MarryCardWindow::Hooks marryCardHooks();

            /// @brief The hooks the leave word window calls back with.
            /// @return The hooks.
            LeaveWordWindow::Hooks leaveWordHooks();

            /// @brief Words of the priest or of one of the couple. Original: WeddingTalkMsgForm.
            /// @param text The words.
            /// @param person The speaker, or none for the priest.
            /// @param durationMs How long it shows, in milliseconds.
            /// @param indentWrapped Whether wrapped lines are indented.
            void weddingSays(const std::string& text, const std::optional<TeamMate>& person, int durationMs, bool indentWrapped);

            /// @brief An instance message (MZ_Menu.pas, 85/x), in WorldSceneInstances.cpp.
            /// @param payload Message payload.
            void onInstanceMessage(const core::Bytes& payload) override;

            /// @brief The hooks the instances call back with.
            /// @return The hooks.
            Instances::Hooks instancesHooks();

            /// @brief The hooks the instance window calls back with.
            /// @return The hooks.
            InstanceWindow::Hooks instanceWindowHooks();

            /// @brief Whether the player may not be taken somewhere now (says why). Original: CanTeleportNow.
            /// @return True when teleporting is refused.
            bool cannotTeleport();

            /// @brief The hooks the GPS window calls back with.
            /// @return The hooks.
            GpsWindow::Hooks gpsHooks();

            /// @brief Why the GPS use is refused now, if it is.
            /// @param teleport True for a teleport, false for a plain GPS use.
            /// @return The refusal, or none.
            std::optional<GpsWindow::Refusal> gpsRefusal(bool teleport);

            /// @brief Whether teleporting is forbidden on a map.
            /// @param mapId Map id.
            /// @return True when no teleport.
            bool isNoTeleportMap(int mapId) const;

            /// @brief The checks of the buttons of the function row that open windows (HandleFuncBtnClick).
            /// @return True when allowed (false: told why).
            bool functionButtonAllowed();

            /// @brief Opens or shuts the activities schedule (function button 8).
            void toggleActivities();

            /// @brief Opens the online draw (function button 9).
            void openLottery();

            /// @brief Uses a magic map item. Original: UseMagicMapItem (23/168 and 23/169).
            /// @param bagSlot Bag place.
            /// @param mode Use mode.
            void useMagicMapItem(int bagSlot, int mode);

            /// @brief The hooks the magic map window calls back with.
            /// @return The hooks.
            MagicMapWindow::Hooks magicMapHooks();

            /// @brief Why a magic map use is refused now.
            /// @return The refusal text ("" when allowed).
            std::string magicMapRefusal();

            /// @brief The wear of a thing changed.
            /// @param container Container id.
            /// @param slot Place in the container.
            /// @param wear Wear.
            /// @param petPlace Pet place when a pet's item.
            void onThingWear(int container, int slot, std::int32_t wear, int petPlace) override;

            /// @brief The magic map is released.
            /// @param bagSlot Bag place.
            void onMagicMapRelease(int bagSlot) override;

            /// @brief A Magic Magnet / Carnie Voucher notice (WorldSceneMagnet.cpp).
            /// @param kind Notice kind.
            /// @param id Player id.
            /// @param arg Extra argument.
            void onMagnetNotice(int kind, std::int32_t id, int arg) override;

            /// @brief The hooks the magnet window calls back with.
            /// @return The hooks.
            MagnetWindow::Hooks magnetHooks();

            /// @brief Asks how to use a Magic Magnet.
            /// @param bagSlot Bag place.
            void chooseMagnetUse(int bagSlot);

            /// @brief Whether items cannot be used here now (says why).
            /// @return True when unusable.
            bool itemUnusableHere();

            /// @brief Uses a Carnie Voucher.
            /// @param bagSlot Bag place.
            void useCarnieVoucher(int bagSlot);

            /// @brief UseGridItem's uses that only send a request of their own (WorldSceneItemUses.cpp).
            /// @param itemId Item id.
            /// @param bagSlot Bag place.
            /// @param petOrder Pet order the item is used on.
            /// @return True when handled.
            bool useRequestItem(int itemId, int bagSlot, int petOrder);

            /// @brief Whether the bag has a free cell.
            /// @return True when one is free.
            bool bagHasFreeCell() const;

            /// @brief The bag place of a Purple Key.
            /// @return The bag place (0 when none).
            int purpleKeySlot() const;

            /// @brief Uses a forget scroll on a pet.
            /// @param bagSlot Bag place.
            /// @param petOrder Pet order.
            void useForgetScroll(int bagSlot, int petOrder);

            /// @brief Asks to confirm forgetting an ability.
            /// @param bagSlot Bag place of the scroll.
            /// @param row Ability row.
            void confirmForgetAbility(int bagSlot, int row);

            /// @brief Uses a pet ability swap scroll.
            /// @param bagSlot Bag place.
            /// @param petOrder Pet order.
            void usePetSwapScroll(int bagSlot, int petOrder);

            /// @brief Selects the pet ability to forget in a swap.
            /// @param bagSlot Bag place of the scroll.
            /// @param place Pet place.
            /// @param row Ability row.
            void selectPetAbilityToForget(int bagSlot, int place, int row);

            /// @brief Selects the pet ability to add in a swap.
            /// @param bagSlot Bag place of the scroll.
            /// @param place Pet place.
            /// @param forgotten The ability row to forget.
            /// @param added The ability row to add.
            void selectPetAbilityToAdd(int bagSlot, int place, int forgotten, int added);

            /// @brief Why a body shift use is refused now, if it is.
            /// @param riceball True for the riceball, false for the icon.
            /// @return The refusal text, or none.
            std::optional<std::string> bodyShiftRefusal(bool riceball) const;

            /// @brief Uses a riceball (body shift).
            /// @param bagSlot Bag place.
            void useRiceball(int bagSlot);

            /// @brief Uses the body shift icon.
            void useBodyShiftIcon();

            /// @brief Throws the aimed item at the mouse.
            void throwAtMouse();

            /// @brief Ends the throw aiming cursor.
            void endThrowAim();

            /// @brief Uses a Win Plate.
            /// @param bagSlot Bag place.
            void useWinPlate(int bagSlot);

            /// @brief Uses a Time Mantle.
            /// @param bagSlot Bag place.
            void useTimeMantle(int bagSlot);

            /// @brief Uses a Delay Clock.
            /// @param bagSlot Bag place.
            void useDelayClock(int bagSlot);

            /// @brief Uses an escape button item.
            /// @param bagSlot Bag place.
            /// @param itemId Item id.
            void useEscapeButton(int bagSlot, int itemId);

            /// @brief Uses a Yes/No board.
            /// @param bagSlot Bag place.
            void useYesNoBoard(int bagSlot);

            /// @brief Draws the fireworks.
            /// @param canvas Target canvas.
            void drawFireworks(gfx::Canvas& canvas);

            /// @brief Puts a character into the throwing pose.
            /// @param thrower The thrower.
            /// @param to The target point.
            /// @param own True for the own player.
            void throwPose(MapCharacter& thrower, core::Point to, bool own);

            /// @brief Reacts to a thrown item hitting a target.
            /// @param kind Throw kind.
            /// @param target Target point.
            void hitByThrow(int kind, core::Point target);

            /// @brief Steps the throw actions of the characters.
            void updateThrowActions();

            /// @brief Whether an item is a bathtub (WorldSceneBath.cpp).
            /// @param itemId Item id.
            /// @return True for a bathtub.
            bool isBathtub(int itemId) const;

            /// @brief The cell a bather is depth sorted by.
            /// @param player The bather.
            /// @return The cell, or none.
            std::optional<core::Point> bathSortCell(const MapCharacter& player) const;

            /// @brief The bathtub piece with a spray.
            /// @return The piece index.
            int sprayBathPiece() const;

            /// @brief Draws the spray's water around a bather.
            /// @param canvas Target canvas.
            /// @param player The bather.
            /// @param afterFigure True when drawn after the figure, false before.
            void drawSprayExtras(gfx::Canvas& canvas, const MapCharacter& player, bool afterFigure);

            /// @brief The own player gets into a bathtub.
            /// @param furnIndex Furniture index.
            void useBathtub(int furnIndex);

            /// @brief The own player leaves the bath.
            void leaveBath();

            /// @brief Starts auto collecting at a gather NPC (the Vacuum Cleaner).
            /// @param npc The gather NPC.
            void startAutoCollect(const MapNpc& npc);

            /// @brief Steps the auto collecting.
            /// @param nowMs Current time in milliseconds.
            void updateAutoCollect(std::uint64_t nowMs);

            /// @brief Stops the auto collecting.
            void stopAutoCollect();

            /// @brief Places another player on the map from its record.
            /// @param record The player's record.
            void placeOther(const protocol::OtherPlayerRecord& record);

            /// @brief A player as the Magic Magnet's form holds one.
            /// @param record The player's record.
            /// @return The magnet player, or none.
            std::optional<MagnetPlayer> magnetPlayer(const protocol::OtherPlayerRecord& record) const;

            /// @brief The result of a Bliss Lamp balloon (WorldSceneWishes.cpp).
            /// @param outcome Outcome code.
            /// @param sender Sender id.
            /// @param target Target id.
            /// @param text The wish text.
            void onBlessBalloonResult(int outcome, std::int32_t sender, std::int32_t target, const std::string& text) override;

            /// @brief The result of a Passion Bouquet.
            /// @param outcome Outcome code.
            /// @param sender Sender id.
            /// @param target Target id.
            /// @param text The message text.
            void onLoveFlowerResult(int outcome, std::int32_t sender, std::int32_t target, const std::string& text) override;

            /// @brief The hooks the love flower window calls back with.
            /// @return The hooks.
            LoveFlowerWindow::Hooks loveFlowerHooks();

            /// @brief The hooks the love flower message window calls back with.
            /// @return The hooks.
            LoveFlowerMessageWindow::Hooks loveFlowerMessageHooks();

            /// @brief The hooks the bless balloon window calls back with.
            /// @return The hooks.
            BlessBalloonWindow::Hooks blessBalloonHooks();

            /// @brief The hooks the bless message window calls back with.
            /// @return The hooks.
            BlessMessageWindow::Hooks blessMessageHooks();

            /// @brief Uses a wish item (Bliss Lamp, Passion Bouquet).
            /// @param itemId Item id.
            /// @param bagSlot Bag place.
            /// @return True when handled.
            bool useWishItem(int itemId, int bagSlot);

            /// @brief SetWhisperTarget's start: the name form of the lamp takes the player clicked.
            /// @param playerId Player id.
            /// @return False when there is no such form open to take it.
            bool playerChosen(std::int32_t playerId);

            /// @brief A thing expired. Original: RecvThingExpired (WorldSceneExpiry.cpp).
            /// @param container Container id.
            /// @param slot Place in the container.
            /// @param petPlace Pet place when a pet's item.
            /// @param newItemId Item it turns into.
            /// @param itemId The expired item.
            /// @param owner Owner name.
            void onThingExpired(
                int container,
                int slot,
                int petPlace,
                int newItemId,
                int itemId,
                const std::string& owner
            ) override;

            /// @brief A Space Contract notice (WorldSceneItemBag.cpp).
            /// @param kind Notice kind.
            /// @param flags Flags.
            /// @param hours Hours left.
            void onSpaceContract(int kind, int flags, std::int32_t hours) override;

            /// @brief Items moved in or out of the Item Bag.
            /// @param kind Move kind.
            /// @param first First place.
            /// @param second Second place.
            /// @param count Count.
            void onItemBagMoved(int kind, int first, int second, int count) override;

            /// @brief The Item Bag's contents.
            /// @param records Bag item records.
            void onItemBagList(const std::vector<protocol::BagItemRecord>& records) override;

            /// @brief The Item Bag opened.
            void onItemBagOpened() override;

            /// @brief The hooks the Item Bag window calls back with.
            /// @return The hooks.
            ItemBagWindow::Hooks itemBagHooks();

            /// @brief The hooks the storeroom window calls back with.
            /// @return The hooks.
            StoreroomWindow::Hooks storeroomHooks();

            /// @brief The Remote Control was reset (WorldSceneRemote.cpp).
            void onRemoteReset() override;

            /// @brief The hooks the Remote Control calls back with.
            /// @return The hooks.
            RemoteControl::Hooks remoteHooks();

            /// @brief Whether the battle has the screen, or the world has darkened to its still picture (the world's things are not drawn). Original: FScreenFrozen.
            /// @return True when the screen is frozen.
            bool screenFrozen() const;

            /// @brief The hooks the Remote Control window calls back with.
            /// @return The hooks.
            RemoteControlWindow::Hooks remoteWindowHooks();

            /// @brief The hooks the Remote Plus window calls back with.
            /// @return The hooks.
            RemotePlusWindow::Hooks remotePlusHooks();

            /// @brief Uses a Remote Control item.
            /// @param bagSlot Bag place.
            /// @return True when handled.
            bool useRemoteControl(int bagSlot);

            /// @brief The state of an exhibit cell.
            /// @param cell Cell number.
            /// @param itemId Item shown.
            void onExhibitState(int cell, int itemId) override;

            /// @brief The online draw's info.
            /// @param info Lottery info.
            void onLotteryInfo(const protocol::LotteryInfo& info) override;

            /// @brief The online draw's result.
            /// @param result Lottery result.
            void onLotteryResult(const protocol::LotteryResult& result) override;

            /// @brief DoActionTeleport's refusals.
            /// @return True when refused (told).
            bool activityTeleportRefused();

            /// @brief The hooks the activities window calls back with.
            /// @return The hooks.
            ActivitiesWindow::Hooks activitiesHooks();

            /// @brief The list of activities.
            /// @param list The activity list.
            void onActivityList(const protocol::ActivityList& list) override;

            /// @brief The states of the activities.
            /// @param states The activity states.
            void onActivityStates(const protocol::ActivityStates& states) override;

            /// @brief An activity notice.
            /// @param kind Notice kind.
            void onActivityNotice(int kind) override;

            /// @brief The map whose file and scene a map uses: the scene an instance's runtime map copies.
            /// @param mapId Map id.
            /// @return The map id whose file is used.
            int mapFileOf(int mapId) const;

            /// @brief Whether a map is a house map.
            /// @param mapId Map id.
            /// @return True for a house map.
            static bool isHouseMap(int mapId);

            /// @brief The picture of a floor room's background by its size.
            /// @param mapId Map id.
            /// @return The background picture (0: the map file's own).
            int houseBackground(int mapId) const;

            /// @brief The map file a room of a house loads. Original: GetHouseRoomMapId.
            /// @param room Room number.
            /// @return The map file id.
            static int houseMapFile(int room);

            /// @brief Shows the house's title for the location line and a short message. Original: ShowHouseSceneTitle.
            void showHouseTitle();

            /// @brief Handles 65/11..13: the pillars and a floor room's size.
            /// @param payload Message payload.
            void receiveHouseRoomMessage(const core::Bytes& payload);

            /// @brief The player stands at the door at once. Original: JumpToWaymark.
            /// @param door The door's position.
            void jumpToWaymark(core::Point door);

            /// @brief What the owner's furniture handling asks of the scene.
            /// @return The hooks.
            HouseEditor::Hooks houseEditorHooks();

            /// @brief The hooks the shelf price window calls back with (WorldSceneStalls.cpp).
            /// @return The hooks.
            StallPriceWindow::Hooks stallPriceHooks();

            /// @brief The hooks the egg sell window calls back with.
            /// @return The hooks.
            EggSellWindow::Hooks eggSellHooks();

            /// @brief The hooks the egg buy window calls back with.
            /// @return The hooks.
            EggBuyWindow::Hooks eggBuyHooks();

            /// @brief The hooks the display case window calls back with.
            /// @return The hooks.
            StallCaseWindow::Hooks stallCaseHooks();

            /// @brief The hooks the cupboard window calls back with.
            /// @return The hooks.
            CupboardWindow::Hooks cupboardHooks();

            /// @brief The hooks the fridge window calls back with.
            /// @return The hooks.
            IceBoxWindow::Hooks iceBoxHooks();

            /// @brief The hooks the locker window calls back with.
            /// @return The hooks.
            PledgeBoxWindow::Hooks pledgeBoxHooks();

            /// @brief The hooks the Music Center window calls back with.
            /// @return The hooks.
            MusicCenterWindow::Hooks musicCenterHooks();

            /// @brief The hooks the gas storage windows call back with.
            /// @return The hooks.
            GasStorageWindow::Hooks gasStorageHooks();

            /// @brief The hooks the Voyage compass window calls back with.
            /// @return The hooks.
            VoyageCompassWindow::Hooks voyageCompassHooks();

            /// @brief The hooks the Bookcase/Desk window calls back with.
            /// @return The hooks.
            BookShelfWindow::Hooks bookShelfHooks();

            /// @brief The hooks the Handcart window calls back with.
            /// @return The hooks.
            HomeAdornWindow::Hooks homeAdornHooks();

            /// @brief The hooks the house computer window calls back with.
            /// @return The hooks.
            ComputerWindow::Hooks computerHooks();

            /// @brief The hooks the picture frame window calls back with.
            /// @return The hooks.
            PortraitWindow::Hooks portraitHooks();

            /// @brief The hooks the doorplate window calls back with.
            /// @return The hooks.
            DoorplateWindow::Hooks doorplateHooks();

            /// @brief The hooks the bed window calls back with.
            /// @return The hooks.
            BedWindow::Hooks bedHooks();

            /// @brief The hooks the retained furniture window calls back with.
            /// @return The hooks.
            RetainFurWindow::Hooks retainFurHooks();

            /// @brief The hooks the bag requests call back with.
            /// @return The hooks.
            BagRequests::Hooks bagRequestHooks();

            /// @brief Asks a question answered with Confirm or Cancel, which goes by itself after 5 seconds. Original: ShowTalkOkCancelTimed on the main talk box.
            /// @param text The question.
            /// @param onOk Called when confirmed.
            void askTimed(const std::string& text, std::function<void()> onOk);

            /// @brief The hooks the furniture buy window calls back with.
            /// @return The hooks.
            FurnitureBuyWindow::Hooks furnitureBuyHooks();

            /// @brief The hooks the vending machine window calls back with.
            /// @return The hooks.
            VendingWindow::Hooks vendingHooks();

            /// @brief A bag item dropped on a shelf piece.
            /// @param bagSlot Bag place.
            /// @param piece Furniture piece.
            /// @return True when the piece is a shelf.
            bool dropOnShelf(int bagSlot, int piece);

            /// @brief A bag item dropped on the vending machine's form.
            /// @param bagSlot Bag place.
            void dropOnVending(int bagSlot);

            /// @brief A click on a shelf item (a buyer's: the buying form).
            /// @param number Shelf item number.
            void clickShelfItem(int number);

            /// @brief A click on an item of a display case: the owner's form or a buyer's.
            /// @param number Case item number.
            void clickCaseItem(int number);

            /// @brief The owner's right click: asks whether to stop selling an item.
            /// @param number Item number.
            /// @param vending True for a vending machine, false for a shelf.
            void askStopSelling(int number, bool vending);

            /// @brief A visitor's press on a piece of the house: a vending machine opens.
            /// @return True when a piece took the press.
            bool pressVisitorPiece();

            /// @brief A visitor's click on a piece of the house.
            /// @param slot Piece place.
            void clickVisitorPiece(int slot);

            /// @brief The Express button of a pet's page. Original: OpenPetExpress.
            /// @param pet The pet.
            void openPetExpress(const Pet& pet);

            /// @brief The names the express form lists from the team.
            /// @return The team members' names.
            std::vector<std::string> teamNames() const;

            /// @brief A click on a Piano (WorldScenePiano.cpp).
            /// @param furnIndex Furniture index.
            /// @param owner True when the house owner clicked.
            void usePiano(int furnIndex, bool owner);

            /// @brief A character uses a seat (WorldSceneSeat.cpp). Original: UseUtensil.
            /// @param character The character.
            /// @param furnIndex Furniture index.
            /// @param seat Seat number.
            /// @return True when the seat was taken.
            bool useUtensil(MapCharacter& character, int furnIndex, int seat);

            /// @brief Plays the sitting animation of a seated character.
            /// @param character The character.
            /// @param group Animation group.
            void playSitting(MapCharacter& character, int group);

            /// @brief A click on a seat.
            /// @param piece Furniture piece.
            void clickSeat(int piece);

            /// @brief Whether an item is a Treadmill or a Bicep Exerciser (WorldSceneTraining.cpp).
            /// @param itemId Item id.
            /// @return True for a training piece.
            bool isTrainingPiece(int itemId) const;

            /// @brief A character starts using a training piece.
            /// @param character The character.
            /// @param furnIndex Furniture index.
            /// @param piece The furniture record.
            /// @param even Whether the piece is on an even cell row.
            /// @param world The piece's world position.
            /// @return True when the training started.
            bool useTrainingPiece(
                MapCharacter& character,
                int furnIndex,
                const FurnitureRecord& piece,
                bool even,
                core::Point world
            );

            /// @brief A click on a training piece.
            /// @param piece Furniture piece.
            void clickTraining(int piece);

            /// @brief Ends a character's training on a piece.
            /// @param character The character.
            void endTraining(MapCharacter& character);

            /// @brief Updates the training bar and the pet on the Treadmill.
            /// @param nowMs Current time in milliseconds.
            void updateTrainingDisplay(std::uint64_t nowMs);

            /// @brief Draws the pet running on a Treadmill.
            /// @param canvas Target canvas.
            /// @param camera The camera.
            /// @param furnIndex Furniture index of the Treadmill.
            void paintTrainingPet(gfx::Canvas& canvas, const Camera& camera, int furnIndex);

            /// @brief A click on a Cracker or a Clock piece.
            /// @param piece Furniture piece.
            void clickCrackerOrClock(int piece);

            /// @brief A click on a Stairs piece.
            /// @param piece Furniture piece.
            void clickStairs(int piece);

            /// @brief Handles a storeroom message.
            /// @param payload Message payload.
            /// @return True when the message was a storeroom one.
            bool receiveStoreroomMessage(const core::Bytes& payload);

            /// @brief Asks about using the Magic Sandglass (WorldSceneSandglass.cpp).
            /// @param piece Furniture piece.
            void askSandglass(int piece);

            /// @brief Confirms using the Magic Sandglass.
            void confirmSandglass();

            /// @brief Handles a Magic Sandglass message.
            /// @param payload Message payload.
            void receiveSandglass(const core::Bytes& payload);

            /// @brief Shows the Magic Sandglass's effects.
            void showSandglassEffects();

            /// @brief Whether a piece of furniture is in use.
            /// @param furnIndex Furniture index.
            /// @return True when in use.
            bool pieceInUse(int furnIndex) const;

            /// @brief The own player stands up from a seat.
            void leaveSeat();

            /// @brief Closes the forms of a house.
            void closeHouseForms();

            /// @brief Shows what the item of a shelf under the mouse is.
            void showShelfInfo();

            /// @brief The hooks the Five in a Row hall calls back with (WorldSceneGobang.cpp).
            /// @return The hooks.
            GobangWindow::Hooks gobangHooks();

            /// @brief Opens the TV console that opens Five in a Row.
            /// @param piece Furniture piece.
            void openTvConsole(int piece);

            /// @brief The TV console's game choice.
            /// @param consolePiece The console piece.
            /// @param tvPiece The TV piece.
            void selectTvGame(int consolePiece, int tvPiece);

            /// @brief The hooks the manufacturing questions call back with (WorldSceneManufacture.cpp).
            /// @return The hooks.
            MakeQuestions::Hooks makeQuestionHooks();

            /// @brief The hooks the thing info window calls back with.
            /// @return The hooks.
            MakeThingInfoWindow::Hooks makeInfoHooks();

            /// @brief The hooks the make thing window calls back with.
            /// @return The hooks.
            MakeThingWindow::Hooks makeThingHooks();

            /// @brief Whether the player is in a floor room of a house.
            /// @return True in a floor room.
            bool inHouseFloorRoom() const;

            /// @brief Why manufacturing is refused now.
            /// @return The refusal text ("" when allowed).
            std::string makeRefusal() const;

            /// @brief A piece was clicked: opens the tree of what it makes.
            /// @param piece Furniture piece.
            void openMakeTree(int piece);

            /// @brief The player goes to work at the piece the tree was opened for.
            void workAtPiece();

            /// @brief Whether a Maid of any kind stands in the house. Original: HasMaidFurniture.
            /// @return True when there is one.
            bool hasMaidFurniture() const;

            /// @brief The partner of that id comes out. Original: HandleMakePacket 10 (ShowFollowPet); 3 to 9: it goes and Player.FMakePetSlot is cleared.
            /// @param petId The partner's id.
            void showMaker(int petId);

            /// @brief The end of a job when the computer's queue may go on.
            /// @param slot The job's place.
            void queueEnds(int slot);

            /// @brief The partner leaves after the job.
            void makerLeaves();

            /// @brief Moves the partner: drawn in the room of the work, facing the piece. Original: UpdateMove of the partner.
            void driveMaker();

            /// @brief Draws the bar of the job under the player's feet.
            /// @param canvas Target canvas.
            /// @param feet The player's feet position.
            void paintMakeBar(gfx::Canvas& canvas, core::Point feet);

            /// @brief Whether the bag has a place for each of the items (AddGridItem on a copy of the bag).
            /// @param itemIds The item ids.
            /// @return True when all fit.
            bool roomInBagFor(const std::vector<int>& itemIds) const;

            /// @brief Sets the hooks that draw the inside of a floor room.
            /// @param display The painter's display to fill.
            /// @param nowMs Current time in milliseconds.
            void setHouseDisplay(WorldPainter::Display& display, std::uint64_t nowMs);

            /// @brief Closes the forms of the public board along with it. Original: HideCtrl of the public board.
            void closeNoticeForms();

            /// @brief Opens the mail form for the author of a notice. Original: MailNoticeAuthor.
            /// @param guild True for the guild's board.
            /// @param authorId Author's id.
            /// @param name Author's name.
            void mailNoticeAuthor(bool guild, std::int32_t authorId, const std::string& name);

            /// @brief No guild any more. Original: ClearGuildData and what follows.
            void guildCleared();

            /// @brief The file the guild ensigns are kept in.
            /// @return The file path.
            std::filesystem::path ensignFile() const;

            /// @brief Shows the invite button for a member with the right. Original: LayoutFuncButtons.
            void layoutGuildInvite();

            /// @brief Starts the guild invite cursor mode. Original: EnterInviteCursorMode.
            void startGuildInvite();

            /// @brief Invites a player into the guild. Original: SendGuildInvite.
            /// @param playerId Player id.
            void inviteToGuild(std::int32_t playerId);

            /// @brief A castle notice.
            /// @param text The notice.
            /// @param inChat True when shown in the chat.
            void onCastleNotice(const std::string& text, bool inChat) override;

            /// @brief The guild that owns the castle (CastleManage.FOwnerGuildName).
            /// @param guild Guild name.
            void onCastleOwner(const std::string& guild) override;

            /// @brief A timed light effect starts.
            void onTimedLight() override;

            /// @brief A treasure NPC jumps.
            /// @param npcIndex NPC index.
            void onTreasureNpcJump(int npcIndex) override;

            /// @brief The refill form opens or closes.
            /// @param open True to open.
            void onReFillForm(bool open) override;

            /// @brief The guild lord changed.
            /// @param memberId New lord's id.
            void onGuildLordChanged(std::int32_t memberId) override;

            /// @brief The guild was renamed.
            /// @param name New name.
            void onGuildRenamed(const std::string& name) override;

            /// @brief A guild alliance message.
            /// @param payload Message payload.
            void onAllianceMessage(const core::Bytes& payload) override;

            /// @brief The announce form opens or closes.
            /// @param open True to open.
            /// @param text The announcement text.
            void onAnnounceForm(bool open, const std::string& text) override;

            /// @brief A leave word message.
            /// @param payload Message payload.
            void onLeaveWordMessage(const core::Bytes& payload) override;

            /// @brief A galaxy rank message.
            /// @param payload Message payload.
            void onGalaxyRankMessage(const core::Bytes& payload) override;

            /// @brief The NPC upgrade form opens.
            void onNpcUpgradeForm() override;

            /// @brief The guild rule help is shown.
            void onGuildRuleHelp() override;

            /// @brief A guild member's info.
            /// @param record The member record.
            void onGuildMemberInfo(const protocol::GuildMemberRecord& record) override;

            /// @brief A guild work message.
            /// @param payload Message payload.
            void onGuildWork(const core::Bytes& payload) override;

            /// @brief The compound facility changes.
            /// @param changes Pairs of facility and its new value.
            void onCompoundFacility(const std::vector<std::pair<int, int>>& changes) override;

            /// @brief Draws the jobs over the own player. Original: DrawLabourProgress, DrawCraftProgress, DrawFeatherMakingProgress.
            /// @param canvas Target canvas.
            /// @param feet The player's feet position.
            void paintGuildWork(gfx::Canvas& canvas, core::Point feet);

            /// @brief A property message.
            /// @param payload Message payload.
            void onPropertyMessage(const core::Bytes& payload) override;

            /// @brief A special floor room message.
            /// @param payload Message payload.
            void onSpecFloorMessage(const core::Bytes& payload) override;

            /// @brief The effect lights on a player.
            /// @param playerId Player id.
            /// @param kinds Effect light kinds.
            void onEffectLights(std::int32_t playerId, const std::vector<int>& kinds) override;

            /// @brief A player's role changed.
            /// @param playerId Player id.
            /// @param role Role id.
            void onRoleChange(std::int32_t playerId, int role) override;

            /// @brief The wait-for-event icon over a player.
            /// @param playerId Player id.
            /// @param mode Icon mode.
            void onWaitEventIcon(std::int32_t playerId, int mode) override;

            /// @brief The server asks whether to buy a level.
            /// @param pet True for a pet's level.
            void onBuyLevelAsked(bool pet) override;

            /// @brief The result of buying a level.
            /// @param code Result code.
            void onBuyLevelResult(int code) override;

            /// @brief Gold Eagle name prefixes of a castle.
            /// @param castle Castle number.
            /// @param replace True to replace the list, false to add.
            /// @param entries Pairs of slot and prefix.
            void onGoldEaglePrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) override;

            /// @brief A Gold Eagle prefix was removed.
            /// @param castle Castle number.
            /// @param slot Prefix slot.
            void onGoldEaglePrefixRemoved(int castle, int slot) override;

            /// @brief Defense NPC name prefixes of a castle.
            /// @param castle Castle number.
            /// @param replace True to replace the list, false to add.
            /// @param entries Pairs of slot and prefix.
            void onDefenseNpcPrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) override;

            /// @brief A defense NPC prefix was removed.
            /// @param castle Castle number.
            /// @param slot Prefix slot.
            void onDefenseNpcPrefixRemoved(int castle, int slot) override;

            /// @brief Stone Eagle name prefixes of a castle.
            /// @param castle Castle number.
            /// @param replace True to replace the list, false to add.
            /// @param entries Pairs of slot and prefix.
            void onStoneEaglePrefix(int castle, bool replace, const std::vector<std::pair<int, std::string>>& entries) override;

            /// @brief A Stone Eagle prefix was removed.
            /// @param castle Castle number.
            /// @param slot Prefix slot.
            void onStoneEaglePrefixRemoved(int castle, int slot) override;

            /// @brief A quest mark's count changed.
            /// @param markId Mark id.
            /// @param change Count change.
            void onMarkCount(int markId, int change) override;

            /// @brief A quest mark was deleted.
            /// @param markId Mark id.
            void onMarkDeleted(int markId) override;

            /// @brief A quest flag bit was set.
            /// @param flagBit Flag bit number.
            /// @param value Flag value.
            void onMarkFlag(int flagBit, int value) override;

            /// @brief A quest mark slot was set.
            /// @param slot Mark slot.
            /// @param markId Mark id.
            /// @param count Count.
            void onMarkSlot(int slot, int markId, int count) override;

            /// @brief A byte of the quest flags was set.
            /// @param byteIndex Byte index.
            /// @param bits The bits.
            void onMarkFlagByte(int byteIndex, int bits) override;

            /// @brief The quest list is full.
            void onQuestsFull() override;

            /// @brief The hotkey M (Ctrl+M): opens or closes the quest window, with a notebook in the bag.
            void toggleQuests();

            /// @brief Starts the F1-F8 bar's target cursor (FState.SlotKind / SlotId, cursor 7).
            /// @param kind Slot kind.
            /// @param id Slot id.
            void aimHotKey(int kind, int id);

            /// @brief Ends the hotkey target cursor.
            void endHotKeyAim();

            /// @brief Whether the own player's figure is under a screen point.
            /// @param screenPoint Screen point.
            /// @return True when it is.
            bool ownPlayerAt(core::Point screenPoint);

            /// @brief The frame of the own player's figure on the screen, seated on his vehicle when he rides one (what ownPlayerAt tests).
            /// @return The frame.
            core::Rect ownFigureArea();

            /// @brief The own fighting pet under a point.
            /// @param screenPoint Screen point.
            /// @return Its place in the team, or none.
            std::optional<int> ownPetAt(core::Point screenPoint);

            /// @brief Uses the aimed hotkey on the player or a pet.
            /// @param petPlace The pet's place, or none for the player.
            void useHotKeyOn(std::optional<int> petPlace);

            /// @brief A player's fight state changed (Player.FFightState).
            /// @param playerId Player id.
            /// @param state Fight state.
            void onPlayerFightState(std::int32_t playerId, int state) override;

            /// @brief A player left the battle.
            /// @param playerId Player id.
            void onPlayerLeftBattle(std::int32_t playerId) override;

            /// @brief A battle starts.
            /// @param start The battle start data.
            void onBattleStart(const protocol::BattleStart& start) override;

            /// @brief The battle's type.
            /// @param type Battle type.
            void onBattleType(int type) override;

            /// @brief The battle's round timer was reset.
            void onBattleTimeReset() override;

            /// @brief The battle's time limit was reached.
            void onBattleTimeLimit() override;

            /// @brief A new battle round begins.
            void onBattleRound() override;

            /// @brief The actions of a battle round.
            /// @param actions The actions.
            void onBattleActions(const std::vector<protocol::BattleAction>& actions) override;

            /// @brief An item round of the battle begins.
            void onBattleItemRound() override;

            /// @brief A fighter casts a skill.
            /// @param cast The cast data.
            void onFighterCast(const protocol::FighterCast& cast) override;

            /// @brief The count of marks on the fighters.
            /// @param count Mark count.
            void onFighterMarks(int count) override;

            /// @brief A fighter takes damage.
            /// @param kind Damage kind.
            /// @param row Fighter row.
            /// @param col Fighter column.
            /// @param damage Damage amount.
            void onFighterDamage(int kind, int row, int col, std::int32_t damage) override;

            /// @brief A fighter's attack area.
            /// @param row Fighter row.
            /// @param col Fighter column.
            /// @param area Area id.
            void onFighterAttackArea(int row, int col, int area) override;

            /// @brief A fighter's attribute changed.
            /// @param row Fighter row.
            /// @param col Fighter column.
            /// @param attribute Attribute id.
            /// @param value New value.
            void onFighterAttribute(int row, int col, int attribute, std::int32_t value) override;

            /// @brief A fighter leaves the battle.
            /// @param row Fighter row.
            /// @param col Fighter column.
            void onFighterLeaves(int row, int col) override;

            /// @brief A fighter's status effect changed.
            /// @param row Fighter row.
            /// @param col Fighter column.
            /// @param kind Status kind.
            /// @param skillId Skill that caused it.
            /// @param place Status place.
            void onFighterStatus(
                int row,
                int col,
                int kind,
                int skillId,
                int place
            ) override;

            /// @brief A fighter was knocked out.
            /// @param row Fighter row.
            /// @param col Fighter column.
            void onFighterKnockedOut(int row, int col) override;

            /// @brief A fighter has acted.
            /// @param row Fighter row.
            /// @param col Fighter column.
            void onFighterActed(int row, int col) override;

            /// @brief The stars the player owns.
            /// @param payload Message payload.
            void onStarsOwned(const core::Bytes& payload) override;

            /// @brief A star was acquired.
            /// @param star Star id.
            /// @param npcIndex NPC the star came from.
            void onStarAcquired(int star, int npcIndex) override;

            /// @brief Opens the star sky (the notebook's Character Introductions). Original: CH_NoteBook.NoteBookButtonClick 3.
            void openStarSky();

            /// @brief An item was thrown in battle.
            /// @param itemId Item id.
            /// @param row Thrower row.
            /// @param col Thrower column.
            /// @param targets Pairs of target row and column.
            void onItemThrown(int itemId, int row, int col, const std::vector<std::pair<int, int>>& targets) override;

            /// @brief A monster was met for the monster book.
            /// @param npcId Monster NPC id.
            void onMonsterMet(int npcId) override;

            /// @brief The monster book's met-bits.
            /// @param payload Message payload.
            void onMonsterBits(const core::Bytes& payload) override;

            /// @brief The screen goes dark and the battle takes it. Original: TFightField.SetupBattleUi.
            void showBattle();

            /// @brief The world lightens again and the field closes. Original: FighterLeaveBattle.
            void leaveBattle();

            /// @brief Advises the player to recover after a battle.
            void adviseRecovery();

            /// @brief Saves the exp of the player and the pets before a fight. Original: SaveExpBeforeFight.
            void saveExpBeforeFight();

            /// @brief The own fighter as CheckCanUseSkill reads it.
            /// @param fighter The fighter.
            /// @return The skill user.
            SkillUser skillUserOf(const Fighter& fighter) const;

            /// @brief Shows what the battle brought the player and the pets. Original: ShowFightExpResult.
            void showFightExpResult();

            /// @brief Shows floating text on a pet when it is the one out on the map. Original: TBasePlayer.ShowFloatText on a pet.
            /// @param place Pet place.
            /// @param text The text.
            void showPetFloatText(int place, const std::string& text);

            /// @brief A chat message.
            /// @param channel Chat channel.
            /// @param speakerId Speaker id.
            /// @param text The text.
            void onChat(int channel, std::int32_t speakerId, const std::string& text) override;

            /// @brief The own player's attributes.
            /// @param attributes The attribute block.
            void onOwnAttributes(const protocol::OwnAttributes& attributes) override;

            /// @brief One of the own attributes changed.
            /// @param attribute Attribute id.
            /// @param value New value.
            /// @param extra Extra value.
            void onAttributeChanged(int attribute, std::int32_t value, std::int32_t extra) override;

            /// @brief A skill's level changed.
            /// @param skillId Skill id.
            /// @param level New level.
            void skillChanged(int skillId, int level);

            /// @brief Whether a player id is the own player's.
            /// @param playerId Player id.
            /// @return True for the own player.
            bool isOwn(std::int32_t playerId) const;

            /// @brief A pet's skill level went up: "Learn skill" (TFollowNpc in SetHumanAttr 110).
            /// @param place Pet place.
            /// @param skillId Skill id.
            /// @param level New level.
            void petSkillChanged(int place, int skillId, int level);

            /// @brief Starts the team button's mouse mode (CY_Team.pas): asking and inviting.
            void startTeamMode();

            /// @brief Sets the mouse mode (MouseMode: 0 walking, 1 PK, 3 watching a battle, 4 joining one, 2 asking to join a team, 5 asking to be friends, 9 / 10 asking to trade items / pets, 16 inviting into a team, 18 inviting into a guild).
            /// @param mode The mode.
            void setMouseMode(int mode);

            /// @brief Starts a battle cursor mode. Original: StartPkCursor / StartJoinCombatCursor / StartWatchCombatCursor.
            /// @param mode The mouse mode (PK, join or watch).
            void startBattleMode(int mode);

            /// @brief The player clicked in PK mode. Original: RequestPkPlayer.
            /// @param playerId Player id.
            void askToPk(std::int32_t playerId);

            /// @brief The player clicked in join-fight mode. Original: RequestJoinPlayerFight.
            /// @param playerId Player id.
            void askToJoinFight(std::int32_t playerId);

            /// @brief The player clicked in watch-fight mode. Original: RequestWatchPlayerFight.
            /// @param playerId Player id.
            void askToWatchFight(std::int32_t playerId);

            /// @brief A map NPC was clicked in PK mode. Original: RequestFightNpc.
            /// @param npcIndex NPC index.
            void askToFightNpc(int npcIndex);

            /// @brief The one asked about a team's battle: its leader.
            /// @param playerId Player id.
            /// @return The player to ask.
            std::int32_t battleTarget(std::int32_t playerId) const;

            /// @brief The player drawn under a screen point.
            /// @param screenPoint Screen point.
            /// @return The player id, or none.
            std::optional<std::int32_t> playerAt(core::Point screenPoint);

            /// @brief Asks to join a player's team.
            /// @param playerId Player id.
            void askToJoin(std::int32_t playerId);

            /// @brief Invites a player into the own team.
            /// @param playerId Player id.
            void inviteToTeam(std::int32_t playerId);

            /// @brief Whether the own player may team up with a player (says why not).
            /// @param playerId Player id.
            /// @param joining True when asking to join, false when inviting.
            /// @return True when allowed.
            bool canTeamUpWith(std::int32_t playerId, bool joining);

            /// @brief Members of teams walk after their leader.
            /// @param nowMs Current time in milliseconds.
            void followLeaders(std::uint64_t nowMs);

            /// @brief A player's pet hides in a team and comes back after. Original: ResetHumanTeam / joining.
            /// @param playerId Player id.
            /// @param inTeam True when the player is in a team now.
            void teamChanged(std::int32_t playerId, bool inTeam);

            /// @brief The character of a player on the map.
            /// @param playerId Player id.
            /// @return The character, or none.
            std::optional<std::reference_wrapper<MapCharacter>> characterOf(std::int32_t playerId);

            /// @brief The name of a player.
            /// @param playerId Player id.
            /// @return The name.
            std::string playerName(std::int32_t playerId) const;

            /// @brief Marks the leaders of the teams on the map.
            void markTeamLeaders();

            /// @brief Why the own player may not hand the lead over now, if so.
            /// @param pending True when an offer is pending.
            /// @return The refusal, or none.
            std::optional<GroupWindow::LeaderRefusal> leaderRefusal(bool pending) const;

            /// @brief The talk box speaker for a player.
            /// @param playerId Player id.
            /// @return The speaker, or none.
            std::optional<TalkDialog::Speaker> speakerOf(std::int32_t playerId) const;

            /// @brief The map a movie plays on (m_movieMap): its size and ground objects.
            /// @param mapId Map id.
            /// @return The map ground, or none.
            std::optional<MovieMapGround> movieMapGround(int mapId);

            /// @brief Who a role of a movie is (a surface of the script).
            /// @param surface The script surface.
            /// @return The role's look, or none.
            std::optional<MovieRoleLook> movieRoleLook(int surface);

            /// @brief Who a speaker of a movie's talks is.
            /// @param surface The script surface.
            /// @return The speaker, or none.
            std::optional<TalkDialog::Speaker> movieSpeaker(int surface);

            /// @brief The player who holds a team seat of a movie surface.
            /// @param surface The script surface.
            /// @return The player id (0 when none).
            std::int32_t teamSeatHolder(int surface) const;

            /// @brief The own player as a talk speaker. Original: SetTalkSpeaker(Player, portrait, 1); in the clothes the role starts with the main box is wide (ShowShortMsg).
            /// @param portrait Whether a portrait is shown.
            /// @return The speaker.
            TalkDialog::Speaker ownSpeaker(bool portrait) const;

            /// @brief Shows a team notice about a player.
            /// @param playerId Player id.
            /// @param text The notice.
            /// @param durationMs How long it shows, in milliseconds.
            void teamNotice(std::int32_t playerId, const std::string& text, int durationMs);

            /// @brief A player as a team mate record.
            /// @param playerId Player id.
            /// @return The team mate, or none.
            std::optional<TeamMate> teamMate(std::int32_t playerId) const;

            /// @brief The own gold changed.
            /// @param amount Gold amount or change.
            /// @param absolute True when amount is the new total, false when a change.
            void onGoldChanged(std::int32_t amount, bool absolute) override;

            /// @brief The whole contents of the bag.
            /// @param records Bag item records.
            void onBagItems(const std::vector<protocol::BagItemRecord>& records) override;

            /// @brief An item was added to the bag.
            /// @param record The bag item record.
            void onBagItemAdded(const protocol::BagItemRecord& record) override;

            /// @brief Items were removed from the bag.
            /// @param itemId Item id.
            /// @param count Count removed.
            void onBagItemsRemoved(int itemId, int count) override;

            /// @brief A bag place's count decreased.
            /// @param slot Bag place.
            /// @param count Count removed.
            void onBagSlotDecreased(int slot, int count) override;

            /// @brief Items moved within the bag.
            /// @param fromSlot Source place.
            /// @param count Count moved.
            /// @param toSlot Target place.
            void onBagItemsMoved(int fromSlot, int count, int toSlot) override;

            /// @brief The worn equipment.
            /// @param items The worn items.
            void onEquipment(const std::vector<protocol::ItemInstance>& items) override;

            /// @brief A worn item was removed.
            /// @param slot Equipment place.
            /// @param destroyed True when destroyed.
            void onEquipmentRemoved(int slot, bool destroyed) override;

            /// @brief An item was taken off into the bag.
            /// @param equipSlot Equipment place.
            /// @param bagSlot Bag place it went to.
            void onUnequipped(int equipSlot, int bagSlot) override;

            /// @brief An item was put on from the bag.
            /// @param bagSlot Bag place it came from.
            /// @param freeSlot Place freed.
            void onEquippedFromBag(int bagSlot, int freeSlot) override;

            /// @brief A pet's worn item was removed.
            /// @param place Pet place.
            /// @param slot Equipment place.
            /// @param told Whether the player is told.
            void onPetEquipmentRemoved(int place, int slot, bool told) override;

            /// @brief A pet's item was taken off into the bag.
            /// @param place Pet place.
            /// @param equipSlot Equipment place.
            /// @param bagSlot Bag place it went to.
            void onPetUnequipped(int place, int equipSlot, int bagSlot) override;

            /// @brief A pet put on an item from the bag.
            /// @param place Pet place.
            /// @param bagSlot Bag place it came from.
            void onPetEquippedFromBag(int place, int bagSlot) override;

            /// @brief A pet's worn item is set.
            /// @param place Pet place.
            /// @param item The item.
            void onPetEquipmentSet(int place, const protocol::ItemInstance& item) override;

            /// @brief The wear of a worn item.
            /// @param place Pet place (0: the player).
            /// @param slot Equipment place.
            /// @param wear Wear.
            void onEquipmentWear(int place, int slot, int wear) override;

            /// @brief The amount left in a potion changed.
            /// @param bagSlot Bag place.
            /// @param amount Amount.
            /// @param add True when added, false when used.
            void onPotionUsedAmount(int bagSlot, int amount, bool add) override;

            /// @brief A bottle pool amount.
            /// @param kind Pool kind.
            /// @param amount Amount.
            void onBottlePool(int kind, int amount) override;

            /// @brief The result of a pet potential roll.
            /// @param petPlace Pet place.
            /// @param potential Potential value.
            void onPotentialResult(int petPlace, int potential) override;

            /// @brief A pet's potential was returned.
            /// @param petPlace Pet place.
            void onPotentialReturned(int petPlace) override;

            /// @brief An item was used.
            void onItemUsed() override;

            /// @brief The own pets.
            /// @param pets Pet records.
            void onPetList(const std::vector<protocol::PetRecord>& pets) override;

            /// @brief A pet joined the team.
            /// @param joined The joined pet data.
            void onPetJoined(const protocol::PetJoined& joined) override;

            /// @brief A pet left its owner.
            /// @param ownerId Owner id.
            /// @param place Pet place.
            void onPetLeft(std::int32_t ownerId, int place) override;

            /// @brief A pet was renamed.
            /// @param ownerId Owner id.
            /// @param place Pet place.
            /// @param name New name.
            void onPetRenamed(std::int32_t ownerId, int place, const std::string& name) override;

            /// @brief The pet out on the map changed.
            /// @param npcId Pet NPC id.
            /// @param silent True to change without announcing.
            void onActivePet(int npcId, bool silent) override;

            /// @brief A pet died or revived in a city battle.
            /// @param place Pet place.
            /// @param dead True when dead.
            void onCityBattleDeadPet(int place, bool dead) override;

            /// @brief A dreaming pill notice.
            /// @param kind Notice kind.
            void onDreamingPill(int kind) override;

            /// @brief The dark mode of the own player (Player.FDarkMode).
            /// @param dark True when dark.
            void onDarkMode(bool dark) override;

            /// @brief Item variants are reverted.
            void onRevertItemVariants() override;

            /// @brief A pet sublimation failed.
            /// @param code Failure code.
            void onPetSublimateFailed(int code) override;

            /// @brief The player is forced to leave the water.
            void onForceLeaveWater() override;

            /// @brief The NPCs are hidden or shown (FNpcsHidden).
            /// @param hidden True when hidden.
            void onNpcsHidden(bool hidden) override;

            /// @brief The map the player came from (FPrevMapId).
            /// @param mapId Previous map id.
            void onPreviousMap(int mapId) override;

            /// @brief One of a pet's attributes changed.
            /// @param place Pet place.
            /// @param attribute Attribute id.
            /// @param value New value.
            /// @param extra Extra value.
            void onPetAttributeChanged(int place, int attribute, std::int32_t value, std::int32_t extra) override;

            /// @brief An attribute of another player changed.
            /// @param playerId Player id.
            /// @param attribute Attribute id.
            /// @param value New value.
            /// @param extra Extra value.
            void onWorldPlayerAttribute(std::int32_t playerId, int attribute, std::int32_t value, std::int32_t extra) override;

            /// @brief The pets out on the map.
            /// @param pets Scene pet records.
            void onScenePets(const std::vector<protocol::ScenePet>& pets) override;

            /// @brief The players riding pets.
            /// @param pets The mounted pets data.
            void onPetsMounted(const protocol::PetsMounted& pets) override;

            /// @brief A player's pet rests.
            /// @param playerId Owner id.
            void onScenePetRests(std::int32_t playerId) override;

            /// @brief A player mounted a pet.
            /// @param playerId Player id.
            /// @param place Pet place.
            /// @param npcId Pet NPC id.
            void onPetMounted(std::int32_t playerId, int place, int npcId) override;

            /// @brief The pet rides on the map.
            /// @param rides The pet ride records.
            void onPetRides(const std::vector<protocol::PetRide>& rides) override;

            /// @brief A map pet was equipped with an item.
            /// @param playerId Owner id.
            /// @param itemId Item id.
            void onScenePetEquipped(std::int32_t playerId, int itemId) override;

            /// @brief A map pet's item was taken off.
            /// @param playerId Owner id.
            void onScenePetUnequipped(std::int32_t playerId) override;

            /// @brief A player's riding flag.
            /// @param playerId Player id.
            /// @param riding True when riding.
            void onPlayerRidingFlag(std::int32_t playerId, bool riding) override;

            /// @brief A player's Trojan war disguise flag.
            /// @param playerId Player id.
            /// @param disguised True when disguised.
            void onTrojanWarFlag(std::int32_t playerId, bool disguised) override;

            /// @brief A player got off a pet.
            /// @param playerId Player id.
            void onPetDismounted(std::int32_t playerId) override;

            /// @brief The pet of that kind carries the player the old way (FOnHorse, action 18). Original: MountPet.
            /// @param playerId Player id.
            /// @param npcId Pet NPC id.
            void mountHorse(std::int32_t playerId, int npcId);

            /// @brief The own player gets off the pet.
            void dismountOwnPet();

            /// @brief A player asks to join the own team.
            /// @param requesterId Requester id.
            void onTeamJoinRequest(std::int32_t requesterId) override;

            /// @brief The asked player is busy.
            void onTeamBusy() override;

            /// @brief The answer to a request to join a team.
            /// @param answer Answer code.
            /// @param leaderId The leader's id.
            void onTeamJoinAnswer(int answer, std::int32_t leaderId) override;

            /// @brief A player left a team.
            /// @param playerId Player id.
            void onTeamLeft(std::int32_t playerId) override;

            /// @brief A player joined a team.
            /// @param leaderId The leader's id.
            /// @param memberId The new member's id.
            void onTeamJoined(std::int32_t leaderId, std::int32_t memberId) override;

            /// @brief The teams on the map.
            /// @param teams Scene team records.
            void onSceneTeams(const std::vector<protocol::SceneTeam>& teams) override;

            /// @brief A team invitation arrived.
            /// @param inviterId Inviter id.
            void onTeamInvite(std::int32_t inviterId) override;

            /// @brief The answer to a team invitation.
            /// @param answer Answer code.
            /// @param inviteeId Invited player's id.
            void onTeamInviteAnswer(int answer, std::int32_t inviteeId) override;

            /// @brief Joining a team failed.
            /// @param code Error code.
            void onTeamJoinError(int code) override;

            /// @brief The team fan icon is shown or hidden over players.
            /// @param ids Player ids.
            /// @param show True to show.
            void onTeamFanIcon(const std::vector<std::int32_t>& ids, bool show) override;

            /// @brief The team shape change was refused.
            /// @param reason Reason code.
            void onTeamShapeRefusal(int reason) override;

            /// @brief The leader offers the lead.
            /// @param leaderId The leader's id.
            void onTeamLeaderOffer(std::int32_t leaderId) override;

            /// @brief The answer to a lead offer.
            /// @param answer Answer code.
            /// @param memberId The member's id.
            void onTeamLeaderAnswer(int answer, std::int32_t memberId) override;

            /// @brief The team's leader changed.
            /// @param newLeaderId New leader's id.
            /// @param oldLeaderId Old leader's id.
            void onTeamLeaderChanged(std::int32_t newLeaderId, std::int32_t oldLeaderId) override;

            /// @brief Starts the Make Friends mouse mode (AC_ICQ.pas).
            void startFriendMode();

            /// @brief Asks a player to be friends.
            /// @param playerId Player id.
            void askToBeFriends(std::int32_t playerId);

            /// @brief What the world knows of a player as a friend (RecvFriendAdded).
            /// @param playerId Player id.
            /// @return The friend record.
            protocol::FriendRecord friendRecordOf(std::int32_t playerId) const;

            /// @brief The state of a player in the world. Original: DrawFriendListPage without grouped ids.
            /// @param playerId Player id.
            /// @return The online state (3: offline).
            int worldOnlineState(std::int32_t playerId) const;

            /// @brief A player asks to be a friend.
            /// @param playerId Player id.
            void onFriendRequest(std::int32_t playerId) override;

            /// @brief The answer to a friend request.
            /// @param playerId Player id.
            /// @param answer Answer code.
            void onFriendAnswer(std::int32_t playerId, int answer) override;

            /// @brief A friend was removed.
            /// @param friendId Friend id.
            void onFriendRemoved(std::int32_t friendId) override;

            /// @brief The friend list.
            /// @param friends Friend records.
            void onFriendList(const std::vector<protocol::FriendRecord>& friends) override;

            /// @brief A player's guild name.
            /// @param playerId Player id.
            /// @param guild Guild name.
            void onGuildName(std::int32_t playerId, const std::string& guild) override;

            /// @brief A friend's info.
            /// @param record The friend record.
            void onFriendInfo(const protocol::FriendRecord& record) override;

            /// @brief The online states of the friends.
            /// @param states The friend states.
            void onFriendStates(const std::vector<protocol::FriendState>& states) override;

            /// @brief A friend list entry was added.
            /// @param record The friend record.
            void onFriendEntry(const protocol::FriendRecord& record) override;

            /// @brief The friend list is full.
            void onFriendListFull() override;

            /// @brief A mail arrived.
            /// @param senderId Sender id.
            /// @param sendTime Send time.
            /// @param text Mail text.
            void onMail(std::int32_t senderId, double sendTime, const std::string& text) override;

            /// @brief The mail box is full.
            void onMailboxFull() override;

            /// @brief The sent mail box is full.
            void onSentMailFull() override;

            /// @brief A furniture mail arrived.
            /// @param senderId Sender id.
            /// @param senderName Sender name.
            /// @param sendTime Send time.
            /// @param text Mail text.
            void onFurMail(std::int32_t senderId, const std::string& senderName, double sendTime, const std::string& text) override;

            /// @brief The result of sending a furniture mail.
            /// @param code Result code.
            void onFurMailResult(int code) override;

            /// @brief The furniture mail box opens.
            void onFurMailBox() override;

            /// @brief A store tank row.
            /// @param kind Tank kind.
            /// @param row Row.
            /// @param itemId Item id.
            /// @param count Count.
            void onStoreTank(int kind, int row, int itemId, int count) override;

            /// @brief The server's answer about sorting (23/250).
            /// @param op Operation.
            /// @param grid Grid the sort applies to.
            /// @param result Result code.
            void onServerSort(int op, int grid, int result) override;

            /// @brief Plays an emote tab emotion over the own head. Original: PlayEmotionButton.
            /// @param number Emotion number.
            void playEmotion(int number);

            /// @brief Another player's emotion (32/1).
            /// @param playerId Player id.
            /// @param emotionId Emotion id.
            void onEmotion(std::int32_t playerId, int emotionId) override;

            /// @brief The result of a compound.
            /// @param itemId Resulting item id.
            /// @param bagSlot Bag place.
            void onCompoundResult(int itemId, int bagSlot) override;

            /// @brief A trade was refused.
            /// @param reason Reason code.
            void onTradeRefused(int reason) override;

            /// @brief Whether the online time is enough to trade (Player.FEnoughOnlineTime, 25/44).
            /// @param enough True when enough.
            void onTradeOnlineTime(bool enough) override;

            /// @brief The player clicked in mouse mode 9 (RequestItemTrade): whether trading with them is allowed.
            /// @param playerId Player id.
            /// @return True when allowed.
            bool mayTradeWith(std::int32_t playerId);

            /// @brief Asks a player to trade items. Original: RequestItemTrade.
            /// @param playerId Player id.
            void askToTradeItems(std::int32_t playerId);

            /// @brief An item trade starts.
            /// @param partnerId The partner's id.
            void onItemTradeStart(std::int32_t partnerId) override;

            /// @brief Asks a player to trade large items (WorldSceneUtensils.cpp).
            /// @param playerId Player id.
            void askToTradeUtensils(std::int32_t playerId);

            /// @brief The hooks the large item trade window calls back with.
            /// @return The hooks.
            UtensilTradeWindow::Hooks utensilTradeHooks();

            /// @brief A large item trade starts.
            /// @param partnerId The partner's id.
            void onUtensilTradeStart(std::int32_t partnerId) override;

            /// @brief The partner's offer in a large item trade.
            /// @param offer The offer.
            void onUtensilTradeOffer(const protocol::TradeOffer& offer) override;

            /// @brief The result of a large item trade.
            /// @param code Result code.
            void onUtensilTradeResult(int code) override;

            /// @brief The result of an item trade.
            /// @param result Result code.
            void onItemTradeResult(int result) override;

            /// @brief The partner's offer in an item trade.
            /// @param offer The offer.
            void onItemTradeOffer(const protocol::TradeOffer& offer) override;

            /// @brief The player clicked in mouse mode 10: asks to trade pets. Original: RequestPetTrade.
            /// @param playerId Player id.
            void askToTradePets(std::int32_t playerId);

            /// @brief A pet trade starts.
            /// @param partnerId The partner's id.
            void onPetTradeStart(std::int32_t partnerId) override;

            /// @brief The result of a pet trade.
            /// @param result Result code.
            void onPetTradeResult(int result) override;

            /// @brief The partner's offer in a pet trade.
            /// @param offer The offer.
            void onPetTradeOffer(const protocol::PetTradeOffer& offer) override;

            /// @brief The bag cells' marks changed: every copy of the bag redraws them.
            void bagMarksChanged();

            /// @brief The own item stall opened. Original: OpenItemStall.
            /// @param stall The stall.
            void onOwnItemStall(const protocol::OwnStall& stall) override;

            /// @brief The item stalls on the map.
            /// @param stalls Stall sign records.
            void onItemStalls(const std::vector<protocol::StallSignRecord>& stalls) override;

            /// @brief A player's item stall closed. Original: CloseItemStall.
            /// @param playerId Player id.
            void onItemStallClosed(std::int32_t playerId) override;

            /// @brief A sale at a stall. Original: RecvStallSale.
            /// @param sale The sale.
            void onStallSale(const protocol::StallSale& sale) override;

            /// @brief The goods of a stall.
            /// @param goods The stall goods.
            void onStallGoods(const protocol::StallGoods& goods) override;

            /// @brief The own pet stall opened. Original: OpenPetStall.
            /// @param stall The stall.
            void onOwnPetStall(const protocol::OwnStall& stall) override;

            /// @brief The pet stalls on the map.
            /// @param stalls Stall sign records.
            void onPetStalls(const std::vector<protocol::StallSignRecord>& stalls) override;

            /// @brief A player's pet stall closed. Original: ClosePetStall.
            /// @param playerId Player id.
            void onPetStallClosed(std::int32_t playerId) override;

            /// @brief The goods of a pet stall.
            /// @param goods The pet stall goods.
            void onPetStallGoods(const protocol::PetStallGoods& goods) override;

            /// @brief A sale at a pet stall. Original: RecvPetStallSale.
            /// @param sale The sale.
            void onPetStallSale(const protocol::StallSale& sale) override;

            /// @brief The hooks the large item stall window calls back with (WorldSceneUtensils.cpp).
            /// @return The hooks.
            UtensilStallWindow::Hooks utensilStallHooks();

            /// @brief The own large item stall opened. Original: OpenUtensilStall.
            /// @param stall The stall.
            void onOwnUtensilStall(const protocol::OwnStall& stall) override;

            /// @brief The large item stalls on the map.
            /// @param stalls Stall sign records.
            void onUtensilStalls(const std::vector<protocol::StallSignRecord>& stalls) override;

            /// @brief A player's large item stall closed. Original: CloseUtensilStall.
            /// @param playerId Player id.
            void onUtensilStallClosed(std::int32_t playerId) override;

            /// @brief The goods of a large item stall.
            /// @param goods The stall goods.
            void onUtensilStallGoods(const protocol::UtensilStallGoods& goods) override;

            /// @brief A sale at a large item stall. Original: RecvUtensilStallSale.
            /// @param sale The sale.
            void onUtensilStallSale(const protocol::StallSale& sale) override;

            /// @brief The stall plate under a screen point (TVenderItem).
            /// @param screenPoint Screen point.
            /// @return The stall owner's id, or none.
            std::optional<std::int32_t> stallPlateAt(core::Point screenPoint);

            /// @brief A double click on a stall plate opens the stall (HandleDblClick).
            /// @param playerId Stall owner's id.
            void openStallOf(std::int32_t playerId);

            /// @brief Goes back to a revival spot. Original: SystemMenuButtonClick 4.
            void returnToSpot();

            /// @brief Chooses the return spot. Original: ChooseReturnSpot, ConfirmReturnSpot.
            /// @param choice The chosen spot.
            void chooseReturnSpot(int choice);

            /// @brief The return spot was recorded (FReturnSpotRecorded, 5/21).
            /// @param code Result code.
            void onReturnSpotRecorded(int code) override;

            /// @brief A Bookcase or Desk place changed.
            /// @param desk True for the Desk, false for the Bookcase.
            /// @param place Place.
            /// @param itemId Item id.
            void onBookShelfSlot(bool desk, int place, int itemId) override;

            /// @brief The result of a password change.
            /// @param result Result code.
            void onPasswordChanged(int result) override;

            /// @brief A request to a player was refused.
            /// @param reason Reason code.
            void onPlayerRefused(int reason) override;

            /// @brief The blacklist or the result of changing it.
            /// @param result Result code.
            /// @param entries The blacklist entries.
            void onBlacklist(int result, const std::vector<protocol::BlacklistEntry>& entries) override;

            /// @brief The player was blocked by another player.
            /// @param code Block code.
            void onBlockedBy(int code) override;

            /// @brief The player's chat and request switches.
            /// @param switches The switch values.
            /// @param channels The chat channels mask.
            // four switches
            void onSwitches(const std::array<int, 4>& switches, int channels) override;

            /// @brief SceneData.Dat's flags of the map the player is on.
            /// @return The flags.
            int sceneFlags() const;

            /// @brief Whether the cell under the feet is plain ("It's not flat" otherwise). Original: StallItemClick and the others.
            /// @details A click on a tent reads Player.FTileX / FTileY as they stand (m_ownTile) instead of the cell under the feet.
            /// @param readStoredTile True to read the stored tile instead of the cell under the feet.
            /// @return True when flat.
            bool standsOnFlatGround(bool readStoredTile = false) const;

            /// @brief The own character stops and plays an action. Original: PlayActionButton / PlayHumanAction.
            /// @param number Action number.
            void playAction(int number);

            /// @brief Stops the own character's action (32/3). Original: StopHumanAction.
            void stopOwnAction();

            /// @brief Another player plays an action.
            /// @param playerId Player id.
            /// @param slot Action slot.
            void onAction(std::int32_t playerId, int slot) override;

            /// @brief The heart radar: the nearest player on the map whose heart number matches.
            /// @param heartNumber The heart number to look for.
            /// @return The player's id, or none.
            std::optional<int> nearestHeart(int heartNumber) const;

            /// @brief A player's nickname.
            /// @param playerId Player id.
            /// @param nickname The nickname.
            void onNickname(std::int32_t playerId, const std::string& nickname) override;

            /// @brief A player's heart number.
            /// @param playerId Player id.
            /// @param heartNumber The heart number.
            void onHeartNumber(std::int32_t playerId, int heartNumber) override;

            /// @brief A message no handler took.
            /// @param id The message id.
            void onUnhandled(protocol::MessageId id) override;

            GameServices& m_services;
            ui::Screen m_screen;
            ui::Element& m_layout;
            ui::Element& m_backdrop;
            ui::DrawingArea& m_view;
            GroundItems m_ground;
            GroundFlights m_flights;
            SpecFloorThing m_specFloor;
            bool m_specFloorPressed = false;
            int m_hoveredGround = 0;
            int m_pressedGround = 0;
            std::optional<formats::DataArchive> m_mapArchive;
            std::optional<formats::DataArchive> m_objectArchive;
            std::optional<formats::SceneTable> m_scenes;
            std::optional<formats::EventArchive> m_events;
            std::optional<WorldMap> m_map;
            std::optional<WorldMap> m_movieMap;
            Camera m_camera;
            WorldPainter m_painter;
            SceneEffects m_effects;
            LightEffect m_screenLight;
            bool m_darkMode = false;
            LightEffect m_playerLight;
            int m_prevMapId = 0;
            bool m_npcsHidden = false;
            LightEffect m_vanish;
            core::Point m_vanishAt;
            bool m_christmasOn = false;
            NewYearEvent m_newYear;
            bool m_weddingFlag = false;
            MapCharacter m_player;
            int m_trainPct = 0;
            std::optional<TrainingPet> m_trainingPet;
            std::map<std::int32_t, MapCharacter> m_others;
            WorldPlayerRecords m_otherRecords;
            std::map<std::int32_t, MateVitals> m_mateVitals;
            std::vector<MapNpc> m_npcs;
            NpcFacings m_npcFacings;
            PlayerTrails m_trails;
            PetsOnMap m_mapPets{m_trails};
            Teams m_teams;
            FriendList m_friends;
            Guild m_guild;
            AllyList m_allies;
            EnsignBook m_ensigns;
            Marriage m_marriage;
            int m_mouseMode = 0;
            static constexpr int kThrowMouseMode = 20; // mouse mode 20: throwing an item
            bool m_fireworkOwn = false;
            std::map<std::int32_t, FireworkShow> m_fireworks;
            ThrowShow m_throwShow;
            int m_throwSlot = 0;
            int m_throwKind = 0;
            bool m_autoCollecting = false;
            int m_autoCollectNpc = 0;
            std::uint64_t m_autoCollectMs = 0;
            std::int32_t m_teamTarget = 0;
            std::vector<formats::SceneDoor> m_doors;
            PlayerStatus m_status;
            Inventory m_inventory;
            Inventory m_bank;
            std::int32_t m_bankGold = 0;
            BagMarks m_bagMarks;
            PetTeam m_pets;
            Battle m_battle;
            StarSky m_starSky;
            StarSkyWindow m_starSkyWindow;
            GobangWindow m_gobang;
            MiniMap m_miniMap;
            MainStatus m_mainStatus;
            BattleMainForm m_battleMain;
            BattleSkillWindow m_battleSkills;
            ChatWindow m_chat;
            HotKeyBar m_hotKeys;
            MainButtons m_mainButtons;
            FunctionButtons m_functionButtons;
            StatusEffects m_statusEffects;
            Sandglass m_sandglass;
            TimeEffectIcons m_timeEffects;
            InputBar m_inputBar;
            ItemWindow m_itemWindow;
            SkillWindow m_skillWindow;
            SettingsWindow m_systemWindow;
            GroupWindow m_groupWindow;
            NoticeBoard m_publicBoard;
            NoticeBoard m_guildBoard;
            NoticeReadForm m_noticeRead;
            NoticeSendForm m_noticeSend;
            NoticeBoardWindow m_noticeBoardWindow;
            Top100 m_top100;
            Top100Window m_top100Window;
            Instances m_instances;
            InstanceCreateForm m_instanceCreate;
            InstanceDetailForm m_instanceDetail;
            InstanceWindow m_instanceWindow;
            GuildWindow m_guildWindow;
            GuildRulesForm m_guildRules;
            GuildMyAuthorityForm m_guildMyAuthority;
            GuildAuthorityForm m_guildAuthority;
            GuildCreateForm m_guildCreate;
            SocialWindow m_socialWindow;
            MarryApplicationForm m_marryApplication;
            MarryDivorceForm m_marryDivorce;
            MarryCardWindow m_marryCard;
            CareerWindow m_career;
            BeforeCareerWindow m_beforeCareer;
            Camps m_camps;
            Vehicles m_vehicles;
            VehicleWrecks m_vehicleWrecks;
            VehicleRepairWindow m_vehicleRepair;
            int m_vehicleBagSlot = 0;
            int m_fishingRodSlot = 0;
            std::optional<core::Point> m_boardCell;
            bool m_walkSent = false;
            core::Point m_ownTile;
            CampStyleWindow m_campStyle;
            NpcTeamMenu m_npcTeam;
            LuckyWheelWindow m_luckyWheel;
            LuckyCardsWindow m_luckyCards;
            MemoryGameWindow m_memory;
            HitMouseWindow m_hitMouse;
            BoxingWindow m_boxing;
            JigsawWindow m_jigsaw;
            DollWindow m_dolls;
            LuckyWindow m_lucky;
            HunterWindow m_hunter;
            DigHoleWindow m_digHole;
            PumpkinWindow m_pumpkin;
            ScotdWindow m_scotd;
            SheepWindow m_sheep;
            ThrowWindow m_throw;
            MovieLayer m_movie;
            QuizWindow m_quiz;
            CompassWindow m_compass;
            FortuneEggWindow m_fortuneEgg;
            SlotMachineWindow m_slotMachine;
            GameStartWindow m_gameStart;
            CompoundWindow m_compoundWindow;
            MoneyMenu m_moneyMenu;
            ThrowMoneyBox m_throwMoney;
            ItemTradeWindow m_itemTrade;
            UtensilTradeWindow m_utensilTrade;
            ItemStallWindow m_itemStall;
            CountBox m_countBox;
            BagRequests m_bagRequests;
            StallGoodsWindow m_stallGoods;
            NpcStoreWindow m_npcStore;
            NpcSaleWindow m_npcSale;
            InnAnswerWindow m_innAnswer;
            BankItemWindow m_bankItems;
            BankMoneyWindow m_bankMoney;
            std::uint32_t m_atmGold = 0;
            AtmDepositWindow m_atmDeposit;
            AtmListWindow m_atmList;
            AtmTransferWindow m_atmTransfer;
            PointExchangeWindow m_pointExchange;
            WarehouseWindow m_warehouse;
            AdminItemsWindow m_adminItems;
            // number of cells of the sale store
            ItemStore m_saleStore{10};
            std::int32_t m_saleGold = 0;
            std::uint8_t m_saleRequestPending = 0;
            SaleNetWindow m_saleNet;
            SaleMenuWindow m_saleMenu;
            std::array<std::int32_t, GuildWeaponBankWindow::kKinds + 1> m_guildWeaponCounts{};
            GuildWeaponBankWindow m_guildWeaponBank;
            PetStorageWindow m_petStorage;
            PetStallWindow m_petStall;
            StallPetsWindow m_stallPets;
            UtensilStallWindow m_utensilStall;
            BuyUtensilWindow m_buyUtensil;
            ChangePasswordWindow m_changePassword;
            BlacklistWindow m_blacklist;
            QuestMarks m_marks;
            std::string m_castleOwner;
            DefenseNpcPrefixes m_defenseNpc;
            GoldEaglePrefixes m_goldEagle;
            StoneEaglePrefixes m_stoneEagle;
            int m_autoPillCount = 0;
            MallShop m_mallShop;
            MallWindow m_mall;
            LuckyBags m_luckyBags;
            ArticleInfoWindow m_articleInfo;
            MallCartWindow m_cart;
            std::optional<HotKeySlot> m_hotKeyAim;
            QuestWindow m_quests;
            MonsterBook m_monsterBook;
            MonsterBookWindow m_monsterBookWindow;
            NotebookWindow m_notebook;
            MsgSkinWindow m_msgSkin;
            SendMailWindow m_sendMail;
            MailBoxWindow m_mailBox;
            FurMailWindow m_furMail;
            PetTradeWindow m_petTrade;
            HouseInterior m_house;
            HousePainter m_housePainter;
            HouseWaymark m_waymark;
            WalkMark m_walkMark;
            HouseEditor m_houseEditor;
            BookShelves m_bookShelves;
            MakeJobs m_makeJobs;
            MakeMaterials m_makeMaterials;
            ComputerWindow m_computer;
            NpcMakeWindow m_npcMake;
            bool m_npcMaking = false;
            MakeThingInfoWindow m_makeInfo;
            MakeThingWindow m_makeThing;
            StallDropRules m_stallDrops;
            StallPriceWindow m_stallPrice;
            FurnitureBuyWindow m_furnitureBuy;
            VendingWindow m_vending;
            EggSellWindow m_eggSell;
            EggBuyWindow m_eggBuy;
            StallCaseWindow m_stallCase;
            CupboardWindow m_cupboard;
            HouseBoxes m_houseBoxes;
            IceBoxWindow m_iceBox;
            PledgeBoxWindow m_pledgeBox;
            MusicCenterWindow m_musicCenter;
            GasStorageWindow m_gasCan;
            GasStorageWindow m_smallGas;
            VoyageCompassWindow m_voyageCompass;
            GlobeWindow m_globe;
            BookShelfWindow m_bookShelf;
            BlueprintWindow m_blueprint;
            HomeAdornWindow m_homeAdorn;
            PortraitWindow m_portrait;
            DoorplateWindow m_doorplate;
            BedWindow m_bed;
            DyeWindow m_dye;
            DismantleWindow m_dismantle;
            ConfuseStoneWindow m_confuseStone;
            ReinforceWindow m_reinforce;
            MagicBoxWindow m_magicBox;
            PackingWindow m_packing;
            ReferWindow m_refer;
            RepairWindow m_repair;
            PuppetState m_puppetState;
            InsteadEcisWindow m_insteadEcis;
            MemCardWindow m_memCard;
            SewingKitWindow m_sewingKit;
            GiftExchangeWindow m_giftExchange;
            GpsWindow m_gps;
            PointStoreWindow m_pointStore;
            PicnicWindow m_picnic;
            ExpBoxWindow m_expBox;
            BlackBoardWindow m_blackBoard;
            IdAndPwWindow m_idAndPw;
            ReFillWindow m_reFill;
            AnnounceWindow m_announce;
            LeaveWordWindow m_leaveWord;
            GalaxyRankWindow m_galaxyRank;
            NpcUpgradeWindow m_npcUpgrade;
            GuildWork m_guildWork;
            PropertyWindow m_property;
            CafeIdWindow m_cafeId;
            bool m_picnicAim = false;
            GpsTeleportEffect m_gpsEffect;
            bool m_teamFormTeleport = false;
            ExpressWindow m_express;
            ForageWindow m_forage;
            FishTankWindow m_fishTank;
            FishBuyWindow m_fishBuy;
            FishAnnotateWindow m_fishAnnotate;
            PianoWindow m_piano;
            MaidMenu m_maid;
            bool m_pianoSeated = false;
            RetainFurWindow m_retainFur;
            PotentialWindow m_potentialWindow;
            PotentialReturnWindow m_potentialReturn;
            StoreTankWindow m_storeTank;
            CapsuleWindow m_capsule;
            ParkingWindow m_parking;
            ShowPlatformWindow m_showPlatform;
            CarCaseWindow m_carCase;
            TransportWindow m_transport;
            TransmitEffect m_transmit;
            WishEffects m_wishEffects;
            ItemInfoPopup m_itemInfo;
            PetInfoPopup m_petInfo;
            RightClickMenu m_rightClickMenu;
            TalkDialog m_talk;
            RoleIntroWindow m_roleIntro;
            EventRunner m_eventRunner;
            RebornWish m_rebornWish;
            bool m_fleeKillActive = false;
            InstanceGoalsPainter m_instanceGoals;
            PlayerMessages m_messages;
            PlayerMessages m_centreMessages;
            DisconnectForm m_disconnect;
            ActivityInfoWindow m_activityInfo;
            ActivitiesWindow m_activities;
            LotteryWindow m_lottery;
            MagicMapWindow m_magicMap;
            MagnetWindow m_magnet;
            BlessBalloonWindow m_blessBalloon;
            BlessMessageWindow m_blessMessage;
            LoveFlowerWindow m_loveFlower;
            LoveFlowerMessageWindow m_loveFlowerMessage;
            ItemBagWindow m_itemBag;
            StoreroomWindow m_storeroom;
            bool m_sortAsked = false;
            RemoteControl m_remote;
            RemotePlusWindow m_remotePlus;
            RemoteControlWindow m_remoteWindow;
            MakeQuestions m_makeQuestions;
            int m_makePiece = 0;
            int m_pressedShelfItem = 0;
            int m_pressedCaseItem = 0;
            bool m_shelfInfoShown = false;
            int m_pressedVisitorPiece = 0;
            bool m_enoughOnlineTime = false;

            bool m_entering = true;
            bool m_battleShown = false;
            bool m_battleClosing = false;
            int m_fightState = 0;
            std::array<std::int64_t, PetTeam::kPlaces + 1> m_expBefore{};
            std::array<int, PetTeam::kPlaces + 1> m_amityBefore{};
            bool m_inWorld = false;
            int m_houseRoom = 0;
            std::string m_houseTitle;
            int m_fadeStep = 0;
            int m_musicDelay = 0;
            bool m_returnSpotRecorded = false;
            int m_activeSport = 0;
            int m_gmQuestion = 0;
            bool m_isGm = false;
            bool m_moveLocked = false;
            bool m_inputLocked = false;
            int m_currentDoor = 0;
            std::uint64_t m_doorCheckMs = 0;
            std::int32_t m_innPrice = 0;

            /// @brief The phases of the inn sleep scene.
            enum class InnSleep { Awake, FallingAsleep, Asleep };
            InnSleep m_innSleep = InnSleep::Awake;
            std::uint64_t m_innWakeMs = 0;
            int m_nearNpc = 0;
            int m_chaseNpc = 0;
            std::uint64_t m_nowMs = 0;
            SeenTexts m_seenTexts;
            int m_sprayFrame = 0;
            std::uint64_t m_chatBlockedMs = 0;
            ChatSilence m_chatSilence;
            std::uint64_t m_enteredMs = 0;
            bool m_mouseHeld = false;
            core::Point m_mouse;
            std::uint64_t m_lastWalkMs = 0;
            std::uint64_t m_keyWalkMs = 0;
            std::optional<core::Point> m_lastWalkSent;
            std::mt19937 m_random{std::random_device{}()};
            PlayerEffectLights m_effectLights;
            TimedLight m_timedLight;
            TreasureNpcJump m_treasureJump;
            TreasureLights m_treasureLights;
            std::optional<SceneKind> m_next;
            int m_dimStep = 0;
            int m_dimTarget = 0;
            std::uint64_t m_dimStepMs = 0;
            int m_dimStepBefore = 0;
            std::optional<gfx::Texture> m_frozenWorld;
    };

}

#endif
