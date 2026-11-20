#include "client/rendering/world/WorldPainter.h"

#include "client/rendering/world/CampSprite.h"
#include "client/rendering/world/NpcHoverLight.h"
#include "client/rendering/painters/EmblemPainter.h"
#include "gfx/RenderDevice.h"
#include "gfx/TextRenderer.h"
#include "client/rendering/hud/GameNumbers.h"
#include "client/rules/PvpRankText.h"
#include "client/rules/PvpServerNames.h"
#include "client/rendering/characters/FigureShadow.h"
#include "client/data/FishingRodOffsets.h"
#include "client/data/RideOffsets.h"
#include "client/models/house/HouseGrid.h"
#include "client/data/VehicleKinds.h"
#include "client/rules/VehicleSeats.h"
#include "client/rules/CharacterActions.h"

#include "gfx/TextStyle.h"

#include <unordered_map>
#include <algorithm>
#include <array>
#include <functional>
#include <set>
#include <variant>

namespace client {

    namespace {

        constexpr int kStandingKind = 3; // ground object kind 3: drawn in depth order with the characters (lower kinds under, higher kinds over)
        constexpr int kFlyingClass = 3; // vehicle class 3: air / space vehicles (Hot-air Balloon, Airship, Mighty Plane, UFO, Space Craft)
        constexpr int kSceneObjectNpc = 10; // NPC type 10: scene object (decoration drawn with the standing list)
        constexpr int kGoldEagleNpc = 20248; // NPC 20248: Gold Eagle (Trojan war map)
        constexpr int kStoneEagleNpc = 20247; // NPC 20247: Stone Eagle (Trojan war map)
        constexpr int kFirstDefenseNpc = 20158; // NPC 20158: Tsiji, first of the Trojan war defense NPCs
        constexpr int kLastDefenseNpc = 20160; // NPC 20160: Barricade, last of the defense NPCs (20159 is Torch)
        constexpr int kOpaque = 255; // alpha 255: fully opaque
        constexpr int kMarkedLight = 30; // light strength used for a highlighted object
        constexpr int kPlayerHeight = 90; // assumed player sprite height, px (name, icon and emotion anchor above the feet)
        constexpr int kNameGap = 10; // gap between a sprite top and its name, px
        constexpr int kShortNpcHeight = 50; // NPC sprites lower than this (px) use the larger name gap
        constexpr int kShortNpcNameGap = 25; // name gap above a short NPC, px
        constexpr int kCharWidth = 8; // width of one name character, px
        const core::Color kNpcNameColor{255, 255, 255, 255}; // white, opaque (r, g, b, a)
        const core::Color kStallNameColor = core::Color::fromRgb565(31); // RGB565 0x001F: blue
        const core::Color kNicknameColor = core::Color::fromRgb565(57886); // RGB565 0xE21E: light purple
        constexpr int kNicknameAbove = 17; // nickname sits 17 px above the name line, px
        constexpr int kTitleAbove = 35; // title sits 35 px above the name line, px
        constexpr int kBadgeAbove = 45; // guild badge sits 45 px above the name line, px
        constexpr int kBadgeLeft = 25; // guild badge sits 25 px left of the name edge, px
        const core::Color kTitleColor = core::Color::fromRgb565(64512); // RGB565 0xFC00: orange
        constexpr int kPvpRankAbove = 32; // PvP rank line px above the name line (no title)
        constexpr int kPvpRankAboveTitle = 52; // PvP rank line px above the name line (when a title is shown)
        const core::Color kPvpRankColor = core::Color::fromRgb565(65535); // RGB565 0xFFFF: white
        constexpr int kPlateLeft = 60; // stall name plate sits 60 px left of the feet, px
        constexpr int kCampMargin = 100; // a camp this far (px) outside the view is skipped
        constexpr int kCampSignDrop = 77; // px a camp owner name is lowered when only the sign is shown (no tent)
        constexpr int kCampTitleAbove = 16; // camp owner title sits 16 px above the camp name, px
        const core::Color kCampNameColor = core::Color::fromRgb565(2016); // RGB565 0x07E0: green
        const core::Color kCampTitleColor = core::Color::fromRgb565(2047); // RGB565 0x07FF: cyan
        constexpr int kPlateAbove = 30; // stall name plate sits 30 px above the player height, px
        constexpr int kStallNameAbove = 28; // stall name text sits 28 px above the player height, px
        constexpr int kStallNameHeight = 12; // stall name text box height, px
        const char* const kPlatePicture = "panel_VenderName_1";

        /// @brief Tells whether a box at a screen point touches the window the camera shows (the game's 800 x 600 screen, or a bigger window round it).
        /// @param camera Camera whose view is tested.
        /// @param at Top-left corner of the box.
        /// @param width Width of the box.
        /// @param height Height of the box.
        /// @return True when any of the box is in view.
        bool onScreen(const Camera& camera, core::Point at, int width, int height) {
            const core::Rect& view = camera.view();
            return at.x + width >= view.left() && at.y + height >= view.top() && at.x <= view.right() && at.y <= view.bottom();
        }

        /// @brief Tells whether a character standing at these feet could show in the view.
        /// @param camera Camera whose view is tested.
        /// @param feet Screen position of the feet.
        /// @return True when the 300 x 300 box around the character touches the view.
        bool nearScreen(const Camera& camera, core::Point feet) {
            return onScreen(camera, {feet.x - 150, feet.y - 250}, 300, 300);
        } // box around a character: 150 left, 250 up, 300 x 300

        /// @brief Tells whether a player is in a fight (FFightState neither 0 nor 4, which is watching).
        /// @param player The player.
        /// @return True when fighting.
        bool fighting(const MapCharacter& player) {
            constexpr int kWatching = 4;
            return player.fightState != 0 && player.fightState != kWatching;
        }

    }

    namespace {

        /// @brief Whether a picture shows a thing that stands on the ground by itself: its outline ends inside the picture. A piece
        /// a larger picture is cut into (a part of a building, of a tree's crown) is opaque up to its lower or side border.
        /// @param pictures Picture library.
        /// @param id The picture.
        /// @param frames How many frames it holds, one under the other.
        /// @return True when it stands by itself.
        bool standsAlone(gfx::PictureLibrary& pictures, gfx::PictureId id, int frames) {
            static std::unordered_map<int, bool> known;
            if (const auto found = known.find(id.value); found != known.end()) {
                return found->second;
            }
            const gfx::Picture& picture = pictures.picture(id);
            const int width = picture.width();
            const int height = picture.height() / std::max(1, frames);
            bool alone = false;
            if (width > 2 && height > 2) {
                int under = 0;
                for (int x = 0; x < width; ++x) {
                    under += picture.isOpaque(x, height - 1) ? 1 : 0;
                }
                int beside = 0;
                for (int y = height / 2; y < height; ++y) {
                    beside += (picture.isOpaque(0, y) ? 1 : 0) + (picture.isOpaque(width - 1, y) ? 1 : 0);
                }
                constexpr float kCut = 0.15f; // more of a border than this opaque: the picture is cut there
                alone = static_cast<float>(under) <= kCut * static_cast<float>(width) && static_cast<float>(beside) <= kCut * static_cast<float>(height);
            }
            known.emplace(id.value, alone);
            return alone;
        }

        /// @brief Where a thing stands on the ground, in its picture: the middle of its trunk on the row it is sorted by, or of
        /// its lowest part when that row is not in the lower half of it.
        /// @param pictures Picture library.
        /// @param id The picture, without a painted shadow.
        /// @param frames How many frames it holds, one under the other.
        /// @param depth The row it is sorted with the figures by.
        /// @return The place in the first frame.
        core::Point footOf(gfx::PictureLibrary& pictures, gfx::PictureId id, int frames, int depth) {
            static std::unordered_map<int, core::Point> known;
            if (const auto found = known.find(id.value); found != known.end()) {
                return found->second;
            }
            const gfx::Picture& picture = pictures.picture(id);
            const int width = picture.width();
            const int height = picture.height() / std::max(1, frames);
            constexpr int kBand = 14; // the lowest rows that are its foot
            constexpr int kInset = 5; // the foot lies this far over its lowest row
            core::Point foot{width / 2, height};
            int lowest = -1;
            int left = width;
            int right = -1;
            for (int y = height - 1; y >= 0 && (lowest < 0 || y > lowest - kBand); --y) {
                for (int x = 0; x < width; ++x) {
                    if (picture.isOpaque(x, y)) {
                        lowest = std::max(lowest, y);
                        left = std::min(left, x);
                        right = std::max(right, x);
                    }
                }
            }
            if (lowest >= 0) {
                foot = {(left + right) / 2, lowest - kInset};
            }
            if (depth > height / 2 && depth < lowest - kInset) {
                // roots reach forward of where it stands: the widest piece on that row is the trunk
                int best = 0;
                int run = 0;
                for (int x = 0; x <= width; ++x) {
                    if (x < width && picture.isOpaque(x, depth)) {
                        ++run;
                        continue;
                    }
                    if (run > best) {
                        best = run;
                        foot = {x - run / 2, depth};
                    }
                    run = 0;
                }
            }
            known.emplace(id.value, foot);
            return foot;
        }

    }

    void WorldPainter::paint(
        gfx::Canvas& canvas,
        WorldMap& map,
        const Camera& camera,
        const std::vector<std::reference_wrapper<MapCharacter>>& players,
        std::vector<MapNpc>& npcs,
        std::vector<MapPet>& pets,
        const std::map<int, Camp>& camps,
        std::map<int, Vehicle>& vehicles,
        const GroundItems& ground,
        int hoveredGround,
        bool showPeople,
        const Display& display,
        std::uint64_t nowMs
    ) {
        m_sceneType = display.sceneType;
        m_people.clear();
        for (MapCharacter& player : players) {
            m_people.emplace(player.id, std::ref(player));
        }
        std::set<std::int32_t> petsAway;
        for (const MapCharacter& player : players) {
            if (player.stall || fighting(player) || player.inShallowWater() || player.vehicle || (player.trojanWar && display.trojanWarMap) || player.bathing ||
                (display.petsHidden && display.petsHidden(player))) {
                petsAway.insert(player.id);
            }
        }
        const auto withLeader = [&](const MapCharacter& player) { return fighting(player) && display.teamMembers.count(player.id) != 0; };
        const auto petShown = [&](const MapPet& pet) {
            const bool own = pet.ownerId == display.ownId;
            constexpr int kDeepWater = 2; // map cell attribute 2: deep water
            if (pet.maker) {
                return pet.visible && !pet.body.inShallowWater() && pet.body.cellAttr != kDeepWater;
            }
            return pet.visible && (own || display.otherPets) && petsAway.count(pet.ownerId) == 0 && !pet.body.inShallowWater() &&
                   pet.body.cellAttr != kDeepWater;
        };
        // in a house only the pets of the players in the shown room are drawn
        const auto sameRoom = [&](std::int32_t ownerId) {
            for (const MapCharacter& player : players) {
                if (player.id == ownerId) {
                    return player.houseRoom == display.houseRoom || ownerId == display.ownId;
                }
            }
            return false;
        };
        const auto petNameShown = [&](const MapPet& pet) {
            return !pet.walker && petShown(pet) && (!display.houseCells || sameRoom(pet.ownerId)) &&
                   (pet.ownerId == display.ownId ? display.ownPetNames : display.otherPetNames);
        };
        {
            // how far the map moved on the screen since the last frame of this map (the software renderer can move the shown picture)
            const core::Point groundAt = camera.toScreen(map.file().backgroundPosition());
            if (m_lastMap == &map) {
                canvas.device().sceneMovedBy({groundAt.x - m_lastGroundAt.x, groundAt.y - m_lastGroundAt.y});
            }
            m_lastMap = &map;
            m_lastGroundAt = groundAt;
        }
        drawBackground(canvas, map, camera);
        // the lighting casts the shadows: those painted on the map's pictures are left out
        WorldMap* const plain = figureShadowsHidden() ? &map : nullptr;
        drawTiles(canvas, map, camera, plain);
        if (display.houseFloor) {
            display.houseFloor(canvas);
        }
        for (GroundObject& object : map.objects()) {
            if (object.info.kind < kStandingKind) {
                drawObject(canvas, object, camera, nowMs, plain);
            }
        }
        if (display.underStanding) {
            display.underStanding(canvas);
        }

        if (display.houseCells) {
            std::vector<HouseEntry> entries;
            if (showPeople) {
                const auto cellOf = [](core::Point feet) {
                    const HouseGrid::Cell cell = HouseGrid::screenToCell(feet);
                    return core::Point{cell.x, cell.y};
                };
                const auto playerCell = [&](const MapCharacter& player) {
                    const std::optional<core::Point> bath = display.bathCell ? display.bathCell(player) : std::nullopt;
                    return bath ? *bath : cellOf(player.position);
                };
                for (MapCharacter& player : players) {
                    if (player.id == display.ownId || (player.houseRoom == display.houseRoom && !withLeader(player))) {
                        entries.push_back({playerCell(player), [&, this] {
                                               if (!(player.id == display.ownId && display.hideOwnPlayer)) {
                                                   if (display.bathExtras && player.bathing) {
                                                       display.bathExtras(canvas, player, false);
                                                   }
                                                   drawPlayer(canvas, player, camera, nowMs);
                                                   if (display.bathExtras && player.bathing) {
                                                       display.bathExtras(canvas, player, true);
                                                   }
                                               }
                                               if (player.id == display.ownId && display.afterOwnPlayer) {
                                                   display.afterOwnPlayer(canvas, camera.toScreen(player.position));
                                               }
                                           }});
                    }
                }
                for (const bool own : {true, false}) {
                    for (MapPet& pet : pets) {
                        if (petShown(pet) && (pet.ownerId == display.ownId) == own && sameRoom(pet.ownerId)) {
                            entries.push_back({cellOf(pet.body.position), [&, this] { drawPet(canvas, pet, camera, display, nowMs); }});
                        }
                    }
                }
                std::vector<std::pair<int, std::reference_wrapper<MapNpc>>> sceneNpcs;
                for (MapNpc& npc : npcs) {
                    if (npc.visible && npc.info.type == kSceneObjectNpc) {
                        sceneNpcs.emplace_back(npc.body.position.y, std::ref(npc));
                    }
                }
                std::stable_sort(sceneNpcs.begin(), sceneNpcs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                for (auto& [y, npc] : sceneNpcs) {
                    drawNpc(canvas, npc, camera, display, nowMs);
                }
            }
            display.houseCells(canvas, entries);
        } else {
            using Standing = std::variant<
                std::reference_wrapper<GroundObject>,
                std::reference_wrapper<MapCharacter>,
                std::reference_wrapper<MapNpc>,
                std::reference_wrapper<const GroundTile>,
                std::reference_wrapper<MapPet>,
                std::reference_wrapper<const Camp>,
                GroundEntry,
                std::reference_wrapper<Vehicle>>;
            std::vector<std::pair<int, Standing>> sceneObjects;
            std::vector<std::pair<int, Standing>> standing;
            if (showPeople) {
                for (MapCharacter& player : players) {
                    if (!withLeader(player) && !isPassenger(player) && player.walkClass() != kFlyingClass) {
                        standing.emplace_back(player.position.y, std::ref(player));
                    }
                }
                for (MapNpc& npc : npcs) {
                    if (npc.visible) {
                        auto& list = npc.info.type == kSceneObjectNpc ? sceneObjects : standing;
                        list.emplace_back(npc.drawOrderY(), std::ref(npc));
                    }
                }
                for (MapPet& pet : pets) {
                    if (petShown(pet)) {
                        standing.emplace_back(pet.body.position.y, std::ref(pet));
                    }
                }
                for (const auto& [slot, camp] : camps) {
                    standing.emplace_back(camp.position.y, std::cref(camp));
                }
                for (auto& [slot, vehicle] : vehicles) {
                    standing.emplace_back(vehicle.position.y, std::ref(vehicle));
                }
                for (const auto& [index, item] : ground.things()) {
                    standing.emplace_back(item.position.y + item.extra, GroundEntry{index, std::cref(item)});
                }
            }
            // the standing objects and tiles go after the moving things above, as one list stable-sorted by y would have them
            std::vector<std::pair<int, Standing>> fixed;
            for (GroundObject& object : map.objects()) {
                if (object.info.kind == kStandingKind) {
                    fixed.emplace_back(object.footY(), std::ref(object));
                }
            }
            for (const GroundTile& tile : map.tiles()) {
                if (tile.object.isValid()) {
                    fixed.emplace_back(tile.sortY, std::cref(tile));
                }
            }
            const auto byY = [](const auto& a, const auto& b) { return a.first < b.first; };
            std::stable_sort(sceneObjects.begin(), sceneObjects.end(), byY);
            std::stable_sort(standing.begin(), standing.end(), byY);
            // a stable sort's order is the one where (y, list position) rises strictly: keep last frame's while it still does
            const auto rises = [&fixed](std::size_t a, std::size_t b) { return fixed[a].first < fixed[b].first || (fixed[a].first == fixed[b].first && a < b); };
            bool ordered = m_fixedOrder.size() == fixed.size();
            for (std::size_t k = 1; ordered && k < m_fixedOrder.size(); ++k) {
                ordered = rises(m_fixedOrder[k - 1], m_fixedOrder[k]);
            }
            if (!ordered) {
                m_fixedOrder.resize(fixed.size());
                for (std::size_t k = 0; k < fixed.size(); ++k) {
                    m_fixedOrder[k] = k;
                }
                std::sort(m_fixedOrder.begin(), m_fixedOrder.end(), rises);
            }
            const std::size_t moving = standing.size();
            standing.reserve(moving + fixed.size());
            for (const std::size_t k : m_fixedOrder) {
                standing.push_back(std::move(fixed[k]));
            }
            // both halves are sorted; on equal y the moving things stay first, as they were earlier in the one list
            std::inplace_merge(standing.begin(), standing.begin() + static_cast<std::ptrdiff_t>(moving), standing.end(), byY);
            using StandingList = std::vector<std::pair<int, Standing>>;
            for (StandingList& list : std::array<std::reference_wrapper<StandingList>, 2>{sceneObjects, standing}) {
                for (auto& [y, thing] : list) {
                    if (std::holds_alternative<std::reference_wrapper<GroundObject>>(thing)) {
                        GroundObject& object = std::get<std::reference_wrapper<GroundObject>>(thing);
                        drawStandingObject(canvas, object, camera, nowMs, plain);
                    } else if (std::holds_alternative<std::reference_wrapper<MapCharacter>>(thing)) {
                        MapCharacter& player = std::get<std::reference_wrapper<MapCharacter>>(thing);
                        if (!(player.id == display.ownId && display.hideOwnPlayer)) {
                            drawPlayer(canvas, player, camera, nowMs);
                            if (display.afterPlayer) {
                                display.afterPlayer(canvas, player.id);
                            }
                        }
                        if (player.id == display.ownId && display.afterOwnPlayer) {
                            display.afterOwnPlayer(canvas, camera.toScreen(player.position));
                        }
                    } else if (std::holds_alternative<std::reference_wrapper<const GroundTile>>(thing)) {
                        const GroundTile& tile = std::get<std::reference_wrapper<const GroundTile>>(thing);
                        const std::size_t noted = beginFigureThing();
                        drawPicture(canvas, plain != nullptr && tile.object.isValid() ? plain->plain(m_pictures, tile.object) : tile.object, camera.toScreen(tile.objectPosition));
                        if (tile.object.isValid() && standsAlone(m_pictures, tile.object, 1)) {
                            endFigureThing(noted);
                        }
                    } else if (std::holds_alternative<std::reference_wrapper<MapPet>>(thing)) {
                        drawPet(canvas, std::get<std::reference_wrapper<MapPet>>(thing), camera, display, nowMs);
                    } else if (std::holds_alternative<std::reference_wrapper<const Camp>>(thing)) {
                        drawCamp(canvas, std::get<std::reference_wrapper<const Camp>>(thing), camera, display);
                    } else if (std::holds_alternative<GroundEntry>(thing)) {
                        const GroundEntry& entry = std::get<GroundEntry>(thing);
                        ground.draw(canvas, entry.item.get(), camera, entry.index == hoveredGround, nowMs);
                    } else if (std::holds_alternative<std::reference_wrapper<Vehicle>>(thing)) {
                        drawParkedVehicle(canvas, std::get<std::reference_wrapper<Vehicle>>(thing), camera, nowMs);
                    } else {
                        drawNpc(canvas, std::get<std::reference_wrapper<MapNpc>>(thing), camera, display, nowMs);
                    }
                }
            }

            if (display.overStanding) {
                display.overStanding(canvas);
            }
            for (GroundObject& object : map.objects()) {
                if (object.info.kind > kStandingKind) {
                    drawObject(canvas, object, camera, nowMs, plain);
                }
            }
            if (showPeople) {
                for (MapCharacter& player : players) {
                    if (!withLeader(player) && !isPassenger(player) && player.walkClass() == kFlyingClass) {
                        if (!(player.id == display.ownId && display.hideOwnPlayer)) {
                            drawPlayer(canvas, player, camera, nowMs);
                        }
                        if (player.id == display.ownId && display.afterOwnPlayer) {
                            display.afterOwnPlayer(canvas, camera.toScreen(player.position));
                        }
                    }
                }
            }
        }
        for (DoorLight& light : map.doorLights()) {
            drawDoorLight(canvas, light, camera, nowMs);
        }
        if (display.beforeLabels) {
            display.beforeLabels(canvas);
        }
        if (!showPeople || !display.humInfo) {
            return;
        }
        int goldEagleNo = 0;
        int defenseNo = 0;
        int stoneEagleNo = 0;
        if (display.padMark && display.padHand != nullptr && display.padHand->isValid()) {
            const core::Point spot = shownAt(display, *display.padMark);
            canvas.drawTexture(*display.padHand, {spot.x - display.padHandTip.x, spot.y - display.padHandTip.y});
        }
        for (const MapNpc& npc : npcs) {
            if (npc.visible && npc.index == display.padNpc && display.padHand != nullptr && display.padHand->isValid()) {
                // the game pad's hand, its fingertip just above the name's place
                constexpr int kAboveName = 2; // px between the fingertip and the name's top
                constexpr int kNameHeight = 16; // px a name takes above which the hand stands
                const core::Point spot = shownAt(display, nameSpot(npc.info, npc.body, camera));
                const int tipY = npc.showsName() ? spot.y - kAboveName : spot.y + kNameHeight;
                canvas.drawTexture(*display.padHand, {spot.x - display.padHandTip.x, tipY - display.padHandTip.y});
            }
            if (npc.visible && npc.showsName()) {
                // showState 1: the NPC is shown (numbered labels only apply to shown NPCs)
                const bool defense = display.trojanWarMap && npc.showState == 1 && npc.info.npcId >= kFirstDefenseNpc && npc.info.npcId <= kLastDefenseNpc &&
                                     display.defenseNpcPrefix;
                const bool goldEagle = display.trojanWarMap && npc.showState == 1 && npc.info.npcId == kGoldEagleNpc && display.goldEaglePrefix;
                const bool stoneEagle = display.trojanWarMap && npc.showState == 1 && npc.info.npcId == kStoneEagleNpc && display.stoneEaglePrefix;
                std::string prefix;
                if (defense) {
                    prefix = display.defenseNpcPrefix(++defenseNo);
                } else if (goldEagle) {
                    prefix = display.goldEaglePrefix(++goldEagleNo);
                } else if (stoneEagle) {
                    prefix = display.stoneEaglePrefix(++stoneEagleNo);
                }
                drawName(canvas, prefix + npc.info.name, kNpcNameColor, shownAt(display, nameSpot(npc.info, npc.body, camera)));
            }
        }
        for (const MapPet& pet : pets) {
            if (petNameShown(pet)) {
                drawName(
                    canvas,
                    pet.name,
                    pet.nameColor(),
                    shownAt(display, nameSpot(pet.info, pet.body, camera, pet.maker && !pet.body.walking ? workingSlot(pet) : 0))
                );
            }
        }
        for (const auto& [slot, camp] : camps) {
            drawCampOwnerName(canvas, camp, camera, display);
        }
        for (MapCharacter& player : players) {
            const core::Point feet = camera.toScreen(player.position);
            const bool own = player.id == display.ownId;
            constexpr int kWaiting = 5;
            if (player.fightState == kWaiting || withLeader(player)) {
                continue;
            }
            const int rideHeight = player.riding && !player.inShallowWater() ? player.riding->height : 0;
            const core::Point spot = player.vehicle ? seatPoint(player, camera, player.vehicle->seat == 1 ? 1 : 2) : feet;
            const int mapTop = player.shape ? nameSpot(*player.shape, player, camera).y : spot.y - kPlayerHeight - kNameGap - rideHeight;
            const core::Point nameAt = shownAt(display, {spot.x, mapTop});
            const int top = nameAt.y;
            if (own || display.otherNames) {
                drawName(canvas, pvpServerPlayerName(player.name, 0, display.onPvpServer), player.nameColor, {nameAt.x, top});
            }
            if (own ? display.ownNickname : display.otherNicknames) {
                drawName(canvas, player.nickname, kNicknameColor, {nameAt.x, top - kNicknameAbove});
            }
            if (!player.title.empty() && (own ? display.ownTitle : display.otherTitles)) {
                drawName(canvas, player.title, kTitleColor, {nameAt.x, top - kTitleAbove});
            }
            if (player.fightState == 0 || player.fightState == 4) {
                for (const std::string& line : pvpRankLines(display.mapId, display.trojanWarMap, player.arenaVictory, player.pvpWins, player.pvpRank)) {
                    drawName(canvas, line, kPvpRankColor, {nameAt.x, top - (player.title.empty() ? kPvpRankAbove : kPvpRankAboveTitle)});
                }
            }
            if (player.fightState == 0 || player.fightState == 4) {
                if (const std::string line = survivalLine(display.mapId, own, player.survivalVictory); !line.empty()) {
                    drawName(canvas, line, kPvpRankColor, {nameAt.x, top - kPvpRankAboveTitle});
                }
            }
            if (display.badgeOf && player.guildId != 0 && (own ? display.ownTitle : display.otherTitles)) {
                if (const auto badge = display.badgeOf(player.guildId)) {
                    const int half = (gfx::TextRenderer::textWidth(player.title, gfx::TextEffect::Plain) + 1) / 2;
                    EmblemPainter::paint(canvas, badge->get(), {nameAt.x - half - kBadgeLeft, top - kBadgeAbove});
                }
            }
            if (player.stall && nearScreen(camera, feet)) {
                const core::Point head = shownAt(display, {feet.x, feet.y - kPlayerHeight});
                drawStallPlate(canvas, *player.stall, {head.x, head.y + kPlayerHeight});
            }
            const core::Size body = m_characters.figureFrameSize(player.look, player.actionSlot());
            const core::Point head = shownAt(display, {feet.x, feet.y - rideHeight - body.height});
            player.chat.draw(canvas, m_pictures, {head.x, head.y + body.height}, body, nowMs);
        }
    }

    core::Rect WorldPainter::stallPlateArea(core::Point feet) const {
        const auto plate = m_pictures.find(kPlatePicture);
        if (!plate) {
            return {};
        }
        const gfx::Picture& picture = m_pictures.picture(*plate);
        return {feet.x - kPlateLeft, feet.y - kPlayerHeight - kPlateAbove, picture.width(), picture.height()};
    }

    void WorldPainter::drawStallPlate(gfx::Canvas& canvas, const StallSign& stall, core::Point feet) {
        if (const auto plate = m_pictures.find(kPlatePicture)) {
            canvas.drawPicture(*plate, {feet.x - kPlateLeft, feet.y - kPlayerHeight - kPlateAbove});
        }
        const int width = static_cast<int>(stall.name.size()) * kCharWidth;
        canvas.drawText(
            {feet.x - width / 2, feet.y - kPlayerHeight - kStallNameAbove},
            stall.name,
            {kStallNameColor, gfx::TextEffect::Plain, std::nullopt},
            width,
            kStallNameHeight
        );
    }

    void WorldPainter::drawStallPicture(gfx::Canvas& canvas, const StallSign& stall, core::Point feet) {
        if (const auto picture = m_pictures.find(stall.pictureName())) {
            const gfx::Picture& data = m_pictures.picture(*picture);
            canvas.drawPicture(*picture, {feet.x - data.width() / 2, feet.y - data.height() / 2});
        }
    }

    void WorldPainter::drawDoorLight(gfx::Canvas& canvas, DoorLight& light, const Camera& camera, std::uint64_t nowMs) {
        constexpr std::uint64_t kFrameMs = 300; // time per door light frame, ms
        constexpr int kStrength = 24; // glow strength of the door light
        const core::Point at = camera.toScreen(light.position);
        const core::Rect& view = camera.view();
        if (at.x >= view.left() && at.y >= view.top() && at.x <= view.right() && at.y <= view.bottom()) {
            const gfx::Picture& picture = m_pictures.picture(light.picture);
            const int height = picture.height() / light.frames;
            canvas.drawPictureLight(
                light.picture,
                {at.x - picture.width() / 2, at.y - height + height / 4},
                kStrength, // glow sunk a quarter of its height
                core::Rect{0, light.frame * height, picture.width(), height}
            );
        }
        if (nowMs > light.frameMs + kFrameMs) {
            light.frame = (light.frame + 1) % light.frames;
            light.frameMs = nowMs;
        }
    }

    void WorldPainter::drawPicture(gfx::Canvas& canvas, gfx::PictureId picture, core::Point at) {
        if (!picture.isValid()) {
            return;
        }
        const gfx::Picture& data = m_pictures.picture(picture);
        const core::Rect window = canvas.window();
        if (at.x + data.width() >= window.left() && at.y + data.height() >= window.top() && at.x <= window.right() && at.y <= window.bottom()) {
            canvas.drawPicture(picture, at);
        }
    }

    void WorldPainter::drawBackground(gfx::Canvas& canvas, WorldMap& map, const Camera& camera) {
        if (!map.background().isValid()) {
            return;
        }
        if (m_sceneType != 9 && m_sceneType != 11) {
            canvas.drawPicture(map.background(), camera.toScreen(map.file().backgroundPosition()));
            return;
        }
        const gfx::Picture& data = m_pictures.picture(map.background());
        const core::Rect& view = camera.view();
        const core::Point centre = camera.backgroundCentre();
        const core::Point scroll = camera.backgroundScroll();
        const core::Point start{std::max(0, centre.x - data.width() / 2), std::max(0, centre.y - data.height() / 2)};
        // clamped so the picture always covers the screen (the original clamps to 800 x 600; a bigger window uses its own size)
        const int x = std::min(0, std::max(start.x - scroll.x, view.width - data.width()));
        const int y = std::min(0, std::max(start.y - scroll.y, view.height - data.height()));
        canvas.drawPicture(map.background(), {x + view.x, y + view.y});
    }

    void WorldPainter::drawStandingObject(gfx::Canvas& canvas, GroundObject& object, const Camera& camera, std::uint64_t nowMs, WorldMap* plain) {
        const std::size_t noted = beginFigureThing(); // the lighting darkens the ground at its edge
        drawObject(canvas, object, camera, nowMs, plain);
        if (object.picture.isValid() && standsAlone(m_pictures, object.picture, std::max(1, object.info.frames))) {
            endFigureThing(noted);
        }
        if (plain != nullptr && object.picture.isValid() && !object.light) {
            // its painted shadow is left out: the lighting casts one for it
            if (const gfx::PictureId shown = plain->plain(m_pictures, object.picture); shown != object.picture) {
                const int frames = std::max(1, object.info.frames);
                const core::Point at = camera.toScreen(object.position) + canvas.origin();
                const int frameTop = object.frame % frames * (m_pictures.picture(shown).height() / frames);
                noteFigureCaster(noted, at + footOf(m_pictures, shown, frames, object.info.depth), shown, at, frameTop);
            }
        }
    }

    void WorldPainter::paintMovieGround(gfx::Canvas& canvas, WorldMap& map, const Camera& camera) {
        if (map.background().isValid()) {
            canvas.drawPicture(map.background(), camera.toScreen(map.file().backgroundPosition()));
        }
        WorldMap* const plain = figureShadowsHidden() ? &map : nullptr; // as on the map: the lighting casts the shadows
        drawTiles(canvas, map, camera, plain);
        for (const GroundTile& tile : map.tiles()) {
            if (tile.object.isValid()) {
                drawPicture(canvas, plain != nullptr ? plain->plain(m_pictures, tile.object) : tile.object, camera.toScreen(tile.objectPosition));
            }
        }
    }

    void WorldPainter::paintMovieObject(gfx::Canvas& canvas, WorldMap& map, GroundObject& object, const Camera& camera, std::uint64_t nowMs) {
        drawStandingObject(canvas, object, camera, nowMs, figureShadowsHidden() ? &map : nullptr);
    }

    void WorldPainter::drawTiles(gfx::Canvas& canvas, const WorldMap& map, const Camera& camera, WorldMap* plain) {
        for (const core::Point position : map.largeTilePositions()) {
            drawPicture(canvas, map.largeTile(), camera.toScreen(position));
        }
        for (const GroundTile& tile : map.tiles()) {
            const auto shown = [&](gfx::PictureId picture) { return plain != nullptr && picture.isValid() ? plain->plain(m_pictures, picture) : picture; };
            drawPicture(canvas, shown(tile.ground), camera.toScreen(tile.groundPosition));
            drawPicture(canvas, shown(tile.objectBack), camera.toScreen(tile.objectBackPosition));
        }
    }

    void WorldPainter::drawObject(gfx::Canvas& canvas, GroundObject& object, const Camera& camera, std::uint64_t nowMs, WorldMap* plain) {
        const formats::GroundObjectInfo& info = object.info;
        if (info.frames > 1 && nowMs > object.frameMs + static_cast<std::uint64_t>(info.frameMs)) {
            object.frame = (object.frame + 1) % info.frames;
            object.frameMs = nowMs;
        }
        if (!object.picture.isValid()) {
            return;
        }
        const gfx::Picture& picture = m_pictures.picture(object.picture);
        const int frameHeight = picture.height() / info.frames;
        const core::Point at = camera.toScreen(object.position);
        if (!onScreen(camera, at, picture.width(), frameHeight)) {
            return;
        }
        const core::Rect source{0, object.frame * frameHeight, picture.width(), frameHeight};
        if (object.light) {
            canvas.drawPictureLight(object.picture, at, kMarkedLight, source);
        } else if (info.alpha == kOpaque) {
            canvas.drawPicture(plain != nullptr ? plain->plain(m_pictures, object.picture) : object.picture, at, source);
        } else {
            canvas.drawPictureLight(plain != nullptr ? plain->plain(m_pictures, object.picture) : object.picture, at, kOpaque - info.alpha, source);
        }
    }

    void WorldPainter::drawPlayer(gfx::Canvas& canvas, MapCharacter& player, const Camera& camera, std::uint64_t nowMs) {
        drawPlayerFigure(canvas, player, camera, nowMs);
        const core::Point feet = camera.toScreen(player.position);
        constexpr int kIconLeft = 10; // fan icon / team banner sit 10 px left of the feet, px
        constexpr int kIconAbove = 17; // fan icon / team banner sit 17 px above the player height, px
        constexpr int kWater = 2; // map cell attribute 2: deep water (no fan icon there)
        constexpr int kBannerFrames = 6; // frames stacked in the Banner picture
        constexpr std::uint64_t kBannerFrameMs = 200; // time per banner frame, ms
        if (!nearScreen(camera, feet) || fighting(player)) {
            return;
        }
        if (player.fanIcon && player.cellAttr != kWater) {
            if (const auto fan = m_pictures.find("icon_Fan")) {
                canvas.drawPicture(*fan, {feet.x - kIconLeft, feet.y - kPlayerHeight - kIconAbove});
            }
        }
        if (player.teamLeader) {
            if (const auto banner = m_pictures.find("Banner")) {
                const gfx::Picture& picture = m_pictures.picture(*banner);
                const int height = picture.height() / kBannerFrames;
                const int frame = static_cast<int>(nowMs / kBannerFrameMs % kBannerFrames);
                canvas.drawPicture(*banner, {feet.x - kIconLeft, feet.y - kPlayerHeight - kIconAbove}, core::Rect{0, frame * height, picture.width(), height});
            }
        }
        drawFloatText(canvas, player.floatText, feet, kPlayerHeight, false, nowMs);
        if (player.waitEventSinceMs) {
            constexpr int kFrames = 3; // frames stacked in the Waiteven picture
            constexpr int kFrameHeight = 32; // height of one Waiteven frame, px
            constexpr int kWidth = 51; // width of the Waiteven picture, px
            constexpr std::uint64_t kFrameMs = 500; // time per frame, ms
            if (const auto icon = m_pictures.find("Waiteven")) {
                const int frame = static_cast<int>((nowMs - std::min(nowMs, *player.waitEventSinceMs)) / kFrameMs % kFrames);
                canvas.drawPicture(
                    *icon,
                    {feet.x + 15, feet.y - 100},
                    core::Rect{0, frame * kFrameHeight, kWidth, kFrameHeight}
                ); // wait icon: 15 right, 100 above the feet
            }
        }
    }

    void WorldPainter::drawPlayerFigure(gfx::Canvas& canvas, MapCharacter& player, const Camera& camera, std::uint64_t nowMs) {
        const core::Point feet = camera.toScreen(player.position);
        if (!nearScreen(camera, feet)) {
            return;
        }
        constexpr int kBattleSign = 9000; // sign sprite 9000: battle sign drawn instead of a fighting player
        if (fighting(player)) {
            const int count = m_characters.signFrameCount(kBattleSign, 0);
            m_characters.drawSignSprite(canvas, kBattleSign, feet, 0, player.animation.frame(nowMs, count));
            return;
        }
        constexpr int kHitSprite = 9081; // sprite 9081: hit effect animation
        constexpr int kHitDrop = 64; // px the hit effect is drawn below the feet
        if (player.hit && player.hit->kind == 1 && !fighting(player)) {
            m_characters.drawHitSprite(canvas, kHitSprite, {feet.x, feet.y + kHitDrop}, player.hit->frame);
            return;
        }
        if (player.stall && player.stall->behindFigure()) {
            drawStallPicture(canvas, *player.stall, feet);
        }
        if (player.vehicle) {
            drawVehicleRider(canvas, player, feet, camera, nowMs);
            return;
        }
        if (player.ridingFlag) {
            constexpr int kRidingSprite = 9017; // sign sprite 9017: the riding-flag figure
            constexpr int kStanding = 8; // first standing slot; slot = 8 + facing direction (0..7)
            const int slot = kStanding + player.direction;
            drawFigureShadow(canvas, feet, 0);
            m_characters.drawSignSprite(canvas, kRidingSprite, feet, slot, player.animation.frame(nowMs, m_characters.signFrameCount(kRidingSprite, slot)));
            player.emotion.draw(canvas, feet, kPlayerHeight, nowMs);
            return;
        }
        if (player.trojanWar) {
            constexpr int kFaceRise = 60; // px the life face is drawn above the feet
            if (!player.trojanTarget) {
                m_characters.drawLifeFace(canvas, player.look, {feet.x, feet.y - kFaceRise});
            }
            return;
        }
        const bool bathingSwim = player.bathing && player.action != 0;
        if (player.inShallowWater() || bathingSwim) {
            constexpr int kSwimming = 66; // first swimming slot; slot = 66 + facing direction (0..7)
            const int slot = bathingSwim ? player.actionSlot() : kSwimming + player.direction;
            const int count = m_characters.figureFrameCount(player.look, slot);
            m_characters.drawSwimmer(canvas, player.look, feet, slot, player.animation.frame(nowMs, count, player.walking));
            player.emotion.draw(canvas, feet, kPlayerHeight, nowMs);
            return;
        }
        if (player.riding) {
            drawRider(canvas, player, feet, nowMs);
            return;
        }
        if (player.shape) {
            const formats::NpcInfo& info = *player.shape;
            drawFigureShadow(canvas, feet, info.shadow);
            const int shapeSlot = m_characters.npcSlot(info.spriteId, info.directionKind, player.actionSlot());
            const int shapeFrame = player.animation.frame(nowMs, m_characters.npcFrameCount(info.spriteId, shapeSlot), player.walking);
            m_characters.drawNpc(canvas, info, feet, shapeSlot, shapeFrame);
            player.emotion.draw(canvas, feet, kPlayerHeight, nowMs);
            if (player.stall && !player.stall->behindFigure()) {
                drawStallPicture(canvas, *player.stall, feet);
            }
            return;
        }
        const core::Point figureAt = player.usedUtensil != 0 ? core::Point{feet.x + player.seatShift.x, feet.y + player.seatShift.y} : feet;
        drawFigureShadow(canvas, figureAt, 0);
        if (player.horse) {
            constexpr int kStanding = 8;
            const formats::NpcInfo& horse = *player.horse;
            const int horseSlot = m_characters.npcSlot(horse.spriteId, horse.directionKind, (player.walking ? 0 : kStanding) + player.direction);
            m_characters
                .drawNpc(canvas, horse, feet, horseSlot, player.animation.frame(nowMs, m_characters.npcFrameCount(horse.spriteId, horseSlot), player.walking));
        }
        const int slot = player.actionSlot();
        const int count = m_characters.figureFrameCount(player.look, slot);
        const int frame = player.holdFirstFrame ? 0
                          : player.action != 0  ? player.animation.frame(nowMs, count, actionFrameMs(player.action, count), actionHoldsLastFrame(player.action))
                                                : player.animation.frame(nowMs, count, player.walking);
        drawFishingRod(canvas, player, feet, slot, frame, false);
        if (player.hit && player.hit->kind == 2) {
            CharacterLook tinted = player.look;
            tinted.pattern = HitReaction::clearedPattern();
            m_characters.drawFigure(canvas, tinted, figureAt, slot, frame);
        } else {
            m_characters.drawFigure(canvas, player.look, figureAt, slot, frame);
        }
        drawFishingRod(canvas, player, feet, slot, frame, true);
        if (player.usedUtensil != 0 && player.seatOverlay.isValid()) {
            constexpr core::Point kOverlayFrom{150, 196}; // seat overlay picture origin: 150 px left, 196 px above the feet
            canvas.drawPicture(player.seatOverlay, {feet.x - kOverlayFrom.x, feet.y - kOverlayFrom.y});
        }
        player.emotion.draw(canvas, feet, kPlayerHeight, nowMs);
        if (player.stall && !player.stall->behindFigure()) {
            drawStallPicture(canvas, *player.stall, feet);
        }
    }

    void WorldPainter::drawFishingRod(
        gfx::Canvas& canvas,
        const MapCharacter& player,
        core::Point feet,
        int slot,
        int frame,
        bool front
    ) {
        constexpr int kFirstSlotOver = 49; // action slots 49..51: the rod is drawn in front of the figure
        constexpr int kLastSlotOver = 51; // last of those slots
        if (player.fishing && player.rodKind >= 1 && (slot >= kFirstSlotOver && slot <= kLastSlotOver) == front) {
            drawRodSprite(canvas, player, feet, slot, frame);
        }
    }

    void WorldPainter::drawRodSprite(
        gfx::Canvas& canvas,
        const MapCharacter& player,
        core::Point feet,
        int slot,
        int frame
    ) {
        constexpr int kFirstRodSprite = 9001; // sign sprite 9001: first fishing rod sprite; + body type + 4 per rod pair
        const int sprite = kFirstRodSprite + player.look.bodyType + (player.rodKind - 1) / 2 * 4;
        const core::Point shift = FishingRodOffsets::offset(player.look.bodyType, slot);
        m_characters.drawSignSprite(canvas, sprite, {feet.x + shift.x, feet.y + shift.y}, slot, frame);
    }

    void WorldPainter::drawVehicleRod(
        gfx::Canvas& canvas,
        MapCharacter& driver,
        core::Point feet,
        int pose,
        std::uint64_t nowMs
    ) {
        // rod shifts by the way the rider faces + 1 (index 0 unused): Vehicle10RiderOfs second row and Vehicle12RiderOfs, x and y
        static constexpr int kShift10X[9] = {0, 0, -52, -80, -68, 0, 68, 94, 75};
        static constexpr int kShift10Y[9] = {0, -40, -8, -2, 8, 42, 29, 2, -30};
        static constexpr int kShift12X[9] = {0, 0, -52, -80, -68, 0, 68, 94, 51};
        static constexpr int kShift12Y[9] = {0, -40, -18, 2, 33, 47, 33, 2, -28};
        constexpr int kFirstPose = 45; // first ride pose slot; index = pose - 45 (1..8 = facing directions)
        const int index = pose - kFirstPose;
        if (!driver.fishing || driver.rodKind < 1 || index < 1 || index > 8) { // 8 facing directions
            return;
        }
        const bool ten = driver.vehicle->kind == 10; // vehicle kind 10
        const core::Point seat = VehicleSeats::offset(driver.look.bodyType, pose, 1, driver.vehicle->kind);
        const core::Point at{feet.x + (ten ? kShift10X : kShift12X)[index] + seat.x, feet.y + (ten ? kShift10Y : kShift12Y)[index] + seat.y};
        constexpr int kFirstRodSprite = 9001; // sign sprite 9001: first fishing rod sprite
        const int sprite = kFirstRodSprite + driver.look.bodyType + (driver.rodKind - 1) / 2 * 4;
        const int count = m_characters.signFrameCount(sprite, pose);
        drawRodSprite(canvas, driver, at, pose, driver.animation.frame(nowMs, count, driver.walking));
    }

    void WorldPainter::drawRider(gfx::Canvas& canvas, MapCharacter& player, core::Point feet, std::uint64_t nowMs) {
        constexpr int kStanding = 8; // first standing slot; slot = 8 + facing direction (0..7)
        constexpr int kRiding = 18; // first rider slot for a riding pose; + facing direction
        constexpr int kSitting = 46; // first rider slot for a sitting pose; + facing direction
        const RiddenPet& ridden = *player.riding;
        const formats::NpcInfo& pet = ridden.info;
        const int skill = pet.spriteId;
        const int riderSlot = (RideOffsets::sits(skill) ? kSitting : kRiding) + player.direction;
        const int frame = player.animation.frame(nowMs, m_characters.figureFrameCount(player.look, riderSlot), player.walking);
        const int petSlot = m_characters.npcSlot(pet.spriteId, pet.directionKind, (player.walking ? 0 : kStanding) + player.direction);
        const int petFrame = m_characters.npcFrameCount(pet.spriteId, petSlot) > 0 ? frame % m_characters.npcFrameCount(pet.spriteId, petSlot) : 0;
        drawFigureShadow(canvas, feet, pet.shadow);
        // the pet and its rider are one figure, standing where the pet does: the rider's shadow goes on from the pet's
        const FigureEchoScope echo(canvas, feet);
        m_characters.drawNpc(canvas, pet, feet, petSlot, petFrame);
        const int bodyType = player.look.bodyType;
        const core::Point seat = RideOffsets::rider(bodyType, riderSlot, skill);
        const core::Point own = petSlot < kStanding ? RideOffsets::pet(bodyType, riderSlot, skill) : core::Point{};
        const int rise = m_characters.rideRise(pet, petSlot, RideOffsets::isListed(skill) ? 0 : petFrame);
        m_characters.drawFigure(canvas, player.look, {feet.x + seat.x + own.x, feet.y + rise + seat.y + own.y}, riderSlot, frame);
        player.emotion.draw(canvas, {feet.x, feet.y - ridden.height}, kPlayerHeight, nowMs);
    }

    void WorldPainter::drawParkedVehicle(gfx::Canvas& canvas, Vehicle& vehicle, const Camera& camera, std::uint64_t nowMs) {
        constexpr int kStandingSouth = 11; // slot 11: standing, facing south (8 + direction 3)
        constexpr int kWalkingSouth = 3; // slot 3: walking, facing south
        constexpr int kKindWithHighShadow = 17; // vehicle kind 17: Space Craft (shadow raised, uses the walking pose)
        constexpr int kHighShadowRise = 42; // px the Space Craft shadow is raised
        if (vehicle.flight.isActive()) {
            return;
        }
        const core::Point feet = camera.toScreen(vehicle.position);
        if (!nearScreen(camera, feet)) {
            return;
        }
        if (VehicleKinds::shadowKind(vehicle.kind) == 0) {
            drawFigureShadow(canvas, feet, 0);
        } else {
            drawFigureShadow(canvas, {feet.x, feet.y - (vehicle.kind == kKindWithHighShadow ? kHighShadowRise : 0)}, 1);
        }
        const int slot = vehicle.kind == kKindWithHighShadow ? kWalkingSouth : kStandingSouth;
        vehicle.shownFrame = vehicle.animation.frame(nowMs, m_characters.vehicleFrameCount(vehicle.lookId, slot));
        m_characters.drawVehicle(canvas, vehicle.lookId, {feet.x, feet.y + VehicleKinds::spriteDrop(vehicle.kind)}, slot, vehicle.shownFrame);
    }

    namespace {
        /// @brief Pose slot a rider is in: the kind's first ride slot plus the way he faces.
        /// @details Original: ApplyVehicleRidePose.
        /// @param rider A player on a vehicle.
        /// @return The pose slot.
        int ridePose(const MapCharacter& rider) { return VehicleKinds::ridePoseSlot(rider.vehicle->kind) + rider.direction; }
    }

    bool WorldPainter::isPassenger(const MapCharacter& player) const {
        // passenger seats are 2..4 (seat 1 is the driver)
        return player.vehicle && player.vehicle->seat >= 2 && player.vehicle->seat <= 4 && m_people.count(player.vehicle->ownerId) != 0;
    }

    core::Point WorldPainter::seatPoint(const MapCharacter& player, const Camera& camera, int seat) const {
        const auto found = player.vehicle->seat == 1 ? m_people.end() : m_people.find(player.vehicle->ownerId);
        const MapCharacter& base = found != m_people.end() ? found->second.get() : player;
        const core::Point feet = camera.toScreen(base.position);
        const core::Point shift = VehicleSeats::offset(player.look.bodyType, ridePose(base), seat, player.vehicle->kind);
        return {feet.x + shift.x, feet.y + shift.y};
    }

    void WorldPainter::drawVehicleRider(
        gfx::Canvas& canvas,
        MapCharacter& driver,
        core::Point feet,
        const Camera& camera,
        std::uint64_t nowMs
    ) {
        constexpr int kStandingOffset = 8; // standing slots start at 8 (slot = 8 + direction)
        constexpr int kFerryScene = 14; // scene type 14: the map where a Submarine is drawn as a raft
        constexpr int kFerryRaft = 6027; // sprite 6027: the raft drawn for kind 16 in that scene
        constexpr int kKindWithFixedStand = 3; // vehicle kind 3: Turtle Car (fixed frame while standing)
        constexpr int kKindFlying = 17; // vehicle kind 17: Space Craft (fixed frame, slot = direction, while standing)
        constexpr int kKindFerry = 16; // vehicle kind 16: Submarine
        constexpr int kBobbedFrameA = 0; // vehicle frames 0 and 3 are the frames where riders bob
        constexpr int kBobbedFrameB = 3; // second bobbing frame
        constexpr int kBob = 2; // px a rider is lowered on a bobbing frame
        RiddenVehicle& vehicle = *driver.vehicle;
        const int kind = vehicle.kind;
        const bool standing = !driver.walking;
        const int pose = ridePose(driver);
        int slot = driver.direction + (standing ? kStandingOffset : 0);
        int sprite = vehicle.lookId;
        bool fixedFrame = false;
        if (kind == kKindWithFixedStand && standing) {
            fixedFrame = true;
        } else if (kind == kKindFlying && standing) {
            fixedFrame = true;
            slot = driver.direction;
        } else if (kind == kKindFerry && m_sceneType == kFerryScene) {
            sprite = kFerryRaft;
        }
        constexpr int kKindWithRod = 12; // vehicle kind 12: Yacht (can fish; rod drawn behind on ride pose 46)
        constexpr int kKindWithRodAlso = 10; // vehicle kind 10: Steam Ship (can fish)
        if (kind == kKindWithRod && pose == 46) { // ride pose 46: facing the way the rod goes behind
            drawVehicleRod(canvas, driver, feet, pose, nowMs);
        }
        if (!VehicleKinds::hidesShadow(kind)) {
            drawFigureShadow(canvas, feet, VehicleKinds::shadowKind(kind) == 0 ? 0 : 1);
        }
        const core::Point vehicleAt{feet.x, feet.y + VehicleKinds::spriteDrop(kind)};
        const int count = m_characters.vehicleFrameCount(sprite, slot);
        const int frame = fixedFrame || count <= 0 ? 0 : vehicle.animation.frame(nowMs, count);
        m_characters.drawVehicle(canvas, sprite, vehicleAt, slot, frame);
        const auto person = [&](MapCharacter& who, int seat) {
            const core::Point shift = VehicleSeats::offset(who.look.bodyType, pose, seat, kind);
            // vehicle kinds 2, 4 and 5 bob their riders on the frames A and B
            const bool bobs = (kind == 2 || kind == 4 || kind == 5) && (frame == kBobbedFrameA || frame == kBobbedFrameB);
            const int riderCount = m_characters.figureFrameCount(who.look, pose);
            const int riderFrame = who.animation.frame(nowMs, riderCount, driver.walking);
            m_characters.drawFigure(canvas, who.look, {feet.x + shift.x, feet.y + shift.y - (bobs ? kBob : 0)}, pose, riderFrame);
        };
        if (VehicleKinds::showsRider(vehicle.itemId)) {
            if (vehicle.passengers.empty()) {
                person(driver, 1);
            } else {
                for (int step = 1; step <= VehicleKinds::seatCount(kind); ++step) {
                    const int order = VehicleSeats::drawOrder(kind, pose, step);
                    if (order == 0) {
                        person(driver, 1);
                    } else if (static_cast<std::size_t>(order) <= vehicle.passengers.size()) {
                        const auto found = m_people.find(vehicle.passengers[static_cast<std::size_t>(order) - 1]);
                        if (found != m_people.end() && found->second.get().vehicle) {
                            person(found->second.get(), order + 1);
                        }
                    }
                }
            }
        }
        const bool frontHidden =
            kind == 1 && (pose == 19 || pose == 20 || pose == 24 || pose == 25); // kind 1 (bike) poses seen from the side where the front layer is not drawn
        if (VehicleKinds::hasFrontLayer(kind) && !frontHidden) {
            m_characters.drawVehicle(canvas, vehicle.lookId + 1, vehicleAt, slot, fixedFrame ? 0 : frame);
        }
        if (kind == kKindWithRodAlso || (kind == kKindWithRod && pose != 46)) { // ride pose 46: the rod was drawn behind already
            drawVehicleRod(canvas, driver, feet, pose, nowMs);
        }
        driver.emotion.draw(canvas, seatPoint(driver, camera, 1), kPlayerHeight, nowMs);
        for (const std::int32_t id : vehicle.passengers) {
            const auto found = m_people.find(id);
            if (found != m_people.end() && found->second.get().vehicle) {
                found->second.get().emotion.draw(canvas, seatPoint(found->second.get(), camera, 2), kPlayerHeight, nowMs);
            }
        }
    }

    void WorldPainter::drawCamp(gfx::Canvas& canvas, const Camp& camp, const Camera& camera, const Display& display) {
        const core::Point feet = camera.toScreen(camp.position);
        if ((camp.position.x == 0 && camp.position.y == 0) || (feet.x == 0 && feet.y == 0) || feet.x < camera.view().left() - kCampMargin ||
            feet.y < camera.view().top() - kCampMargin || feet.x > camera.view().right() + kCampMargin || feet.y > camera.view().bottom() + kCampMargin) {
            return;
        }
        const bool own = camp.own;
        const bool ownLook = display.trojanWarMap ? own : (display.showTents || own);
        if (const auto sprite = campSprite(camp, ownLook, m_items)) {
            m_characters.drawCampSprite(canvas, sprite->bank, sprite->id, feet);
        }
    }

    void WorldPainter::drawCampOwnerName(gfx::Canvas& canvas, const Camp& camp, const Camera& camera, const Display& display) {
        const core::Point feet = camera.toScreen(camp.position);
        if ((camp.position.x == 0 && camp.position.y == 0) || (feet.x == 0 && feet.y == 0) || feet.x < camera.view().left() - kCampMargin ||
            feet.y < camera.view().top() - kCampMargin || feet.x > camera.view().right() + kCampMargin || feet.y > camera.view().bottom() + kCampMargin ||
            camp.ownerId == 0) {
            return;
        }
        const bool own = camp.own;
        int y = 0;
        if (!camp.ownerName.empty()) {
            y = feet.y - camp.frameHeight;
            if (!display.showTents && !own) {
                y += kCampSignDrop;
            }
        }
        const core::Point nameAt = shownAt(display, {feet.x, y});
        if (!camp.ownerName.empty()) {
            drawName(canvas, camp.ownerName + "'s home ", kCampNameColor, nameAt);
        }
        if (!camp.ownerTitle.empty()) {
            drawName(canvas, camp.ownerTitle, kCampTitleColor, {nameAt.x, nameAt.y - kCampTitleAbove});
        }
    }

    core::Point WorldPainter::nameSpot(const formats::NpcInfo& info, const MapCharacter& body, const Camera& camera, int slot) {
        const core::Point feet = camera.toScreen(body.position);
        const int height = m_characters.npcHeight(info.spriteId, info.spriteSize, slot, 0) * (info.heightMode == 1 ? 2 : 1);
        const int gap = height < kShortNpcHeight ? kShortNpcNameGap : kNameGap;
        return {feet.x, feet.y - height - gap};
    }

    int WorldPainter::workingSlot(const MapPet& pet) const {
        constexpr int kWorkingSlot = 47; // first working-pose slot of a pet maker; + direction pair
        return m_characters
            .npcSlot(pet.info.spriteId, pet.info.directionKind, kWorkingSlot + pet.body.direction % 8 / 2 * 2); // 8 directions, one still frame per pair
    }

    void WorldPainter::drawPet(
        gfx::Canvas& canvas,
        MapPet& pet,
        const Camera& camera,
        const Display& display,
        std::uint64_t nowMs
    ) {
        const core::Point feet = camera.toScreen(pet.body.position);
        if (!nearScreen(camera, feet)) {
            return;
        }
        drawFloatText(
            canvas,
            pet.body.floatText,
            feet,
            m_characters
                .npcHeight(pet.info.spriteId, pet.info.spriteSize, m_characters.npcSlot(pet.info.spriteId, pet.info.directionKind, pet.body.actionSlot()), 0),
            true,
            nowMs
        );
        drawFigureShadow(canvas, feet, pet.info.shadow);
        const bool working = pet.maker && !pet.body.walking;
        const int slot = working ? workingSlot(pet) : m_characters.npcSlot(pet.info.spriteId, pet.info.directionKind, pet.body.actionSlot());
        const int frame = working ? 0 : pet.body.animation.frame(nowMs, m_characters.npcFrameCount(pet.info.spriteId, slot), pet.body.walking);
        if (pet.weapon.lookId > 0) {
            m_characters.drawArmedNpc(canvas, pet.info, pet.weapon.lookId, pet.weapon.colorCode, feet, slot, frame);
        } else {
            m_characters.drawNpc(canvas, pet.info, feet, slot, frame);
        }
        if (pet.maker && display.afterMakerPet) {
            display.afterMakerPet(canvas, feet);
        }
    }

    void WorldPainter::drawFloatText(
        gfx::Canvas& canvas,
        FloatText& text,
        core::Point feet,
        int spriteHeight,
        bool pet,
        std::uint64_t nowMs
    ) {
        constexpr int kPinkFont = 5; // game letter font 5: pink digits / text
        if (const auto shown = text.update(nowMs)) {
            drawGameLetters(
                canvas,
                {feet.x, feet.y - spriteHeight - shown->rise},
                shown->text,
                kPinkFont,
                pet ? 0.3F : 0.4F,
                pet ? 16 : 24,
                0,
                true
            ); // letter scale (pet 0.3, others 0.4) and letter size (pet 16, others 24)
        }
    }

    void WorldPainter::drawNpc(
        gfx::Canvas& canvas,
        MapNpc& npc,
        const Camera& camera,
        const Display& display,
        std::uint64_t nowMs
    ) {
        const core::Point feet = camera.toScreen(npc.body.position);
        if (!nearScreen(camera, feet)) {
            return;
        }
        drawFigureShadow(canvas, feet, npc.info.shadow);
        const int spriteId = npc.info.spriteId;
        const int slot = m_characters.npcSlot(spriteId, npc.info.directionKind, npc.body.actionSlot());
        const int count = m_characters.npcFrameCount(spriteId, slot);
        int animated = 0;
        if (npc.body.action != 0 && npc.actionHoldFrame != 255) { // 255: no frame held
            animated = count > 0 ? std::min(npc.actionHoldFrame, count - 1) : 0;
        } else if (npc.body.action != 0) {
            animated = npc.body.animation.frame(nowMs, count, actionFrameMs(npc.body.action, count), actionHoldsLastFrame(npc.body.action));
        } else {
            animated = npc.body.animation.frame(nowMs, count, npc.body.walking);
        }
        const int frame = npc.isSceneObject() && npc.holdFrame != 255 ? npc.holdFrame : animated; // 255: no frame held
        const core::Point spot{feet.x, feet.y + npc.spriteOffsetY};
        SpriteHighlight light;
        const bool tall = npc.info.heightMode == 1; // Npc.dat height mode 1: drawn twice the size, as in battle and movies
        if (tall) {
            light.scale = 20; // 20 / 10: twice the size
            light.scaleDivisor = 10;
        }
        if (display.mouse && NpcHoverLight::applies(npc.info.npcId) && m_characters.npcContains(npc.info, spot, slot, frame, *display.mouse, light)) {
            light.light = NpcHoverLight::kStrength;
        }
        m_characters.drawNpc(canvas, npc.info, spot, slot, frame, light);
        m_characters.drawNpcLook(canvas, npc.info, spot, slot, frame, light);
        npc.body.emotion.draw(canvas, feet, m_characters.npcFrameSize(spriteId, slot).height * (tall ? 2 : 1), nowMs);
    }

    void WorldPainter::drawName(gfx::Canvas& canvas, const std::string& name, core::Color color, core::Point at) {
        const int width = static_cast<int>(name.size()) * kCharWidth;
        canvas.drawText({at.x - width / 2, at.y}, name, {color, gfx::TextEffect::Shadow, std::nullopt});
    }

}
