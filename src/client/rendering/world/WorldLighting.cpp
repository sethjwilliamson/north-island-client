#include "client/rendering/world/WorldLighting.h"

#include "client/rendering/world/SolidShadows.h"
#include "core/Log.h"
#include "gfx/SpriteShapes.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace client {

    namespace {

        using Json = nlohmann::json;

        // the looks the file is first written with; scene types: 1 open land, 2 village, 3 indoors, 4 cave, 6 woods, 10 shore
        constexpr const char* kBuiltIn = R"json({
  "enabled": true,
  "version": 26,
  "help": "Light 1 leaves the picture as it is, 2 doubles it, 0 is black. 'types' holds a look per kind of map over 'default': a scene type's number (1 open land, 2 village, 3 indoors, 4 cave, 5 ship, 6 woods, 8 sea, 9 volcano, 10 shore, 11 water cave, 12 desert, 14 seabed), 'room' for the painted maps of type 4 (lobbies, halls, jails), 'battle' for battles; 'maps' holds what differs for one map id. Saved changes show at once. Only the door and object lights (and the NPCs that burn) waver ('flicker', or a light's own 'flicker' in 'doorLight' or 'objectLight') and glow onto the picture ('glow'); 'vivid' is how much of that glow brightens the picture's own colours (0 adds the light's colour, which makes the colours paler). 'npcLights' are the NPCs that burn, by Npc.dat id, each with the colour of its light; they are lit as the objects are ('objectLight's radius and strength). A file with a lower 'version' than the game's is written anew. 'shadows' are cast by figures (and by the map's objects with 'objects'), along 'direction' as much as 'sun' says (0 inside, where only the lights cast shadows; never where the own player has a light), and away from the lights near by: the own player's always, and as much as 'lamps' says (0 to 1) the other players', the doors', the objects' and the painted ones (on the screen; the backgrounds' painted shadows fall to the upper right), in the 'tone' of the painted ones; with 'skipPainted' they and the drifting shade are left out where the background is painted in shade already; with 'shaped' a figure's shadow has the figure's shape (else it is an ellipse of 'length' and 'width'), and 'figures' is how much darker a figure gets where it stands in painted shade (0 for not at all); 'baked' finds the lights painted into a background: 'lights' is how strong each shines again, 'rays' how strong a ray falls from each window; a shadow starts 'start' pixels behind the feet; 'occlusion' darkens the ground along the outline of every figure and thing that stands on it, most at its foot (0 for not at all), whatever the light; 'effectLights' is how much a battle's effects that shine (fire, lightning, flashes) light the field around them, in their own colour (0 for not at all); 'water' glints, 'motes' drift in the air, 'shafts' are beams of light that come down from where the shadows fall away from: they lean as the shadows' 'direction' and 'length' say, or 'angle' degrees from upright when that is 0 or more.",
  "types": {
    "default": {
      "ambient": [1.0, 1.0, 1.0],
      "corners": 0.08,
      "occlusion": 0.05,
      "shade": { "tone": [0.52, 0.545, 0.59], "amount": 0.0, "scale": 4.0, "speed": [10, 4] },
      "playerLight": { "tone": [1.0, 0.86, 0.62], "radius": 200, "strength": 0.0 },
      "othersLight": { "tone": [1.0, 0.86, 0.62], "radius": 150, "strength": 0.0 },
      "doorLight": { "tone": [1.0, 0.92, 0.7], "radius": 110, "strength": 0.25 },
      "objectLight": { "tone": [1.0, 0.76, 0.45], "radius": 120, "strength": 0.35 },
      "flicker": 0.05,
      "glow": 0.18,
      "vivid": 1.0,
      "npcLights": { "18043": [1.0, 0.72, 0.4], "19086": [1.0, 0.8, 0.35], "19087": [0.5, 1.0, 0.45], "19088": [0.45, 0.65, 1.0], "19142": [1.0, 0.72, 0.4], "20159": [1.0, 0.72, 0.4] },
      "shadows": { "strength": 0.95, "length": 80, "width": 22, "direction": [0.91, -0.41], "tone": [0.52, 0.545, 0.59], "objects": 0.0, "skipPainted": true, "shaped": true, "figures": 1.0, "start": 4, "lamps": 0.0, "sun": 1.0 },
      "baked": { "lights": 0.0, "radius": 1.0, "rays": 0.0, "rayLength": 150 },
      "water": { "sparkle": 0.85 },
      "motes": { "amount": 0, "strength": 0.4, "tone": [1.0, 0.95, 0.8] },
      "shafts": { "strength": 0.0, "tone": [1.0, 0.94, 0.72], "angle": -1 }
    },
    "1": { "ambient": [1.06, 1.05, 1.02], "shade": { "amount": 0.9 }, "shafts": { "strength": 0.08 } },
    "10": { "ambient": [1.06, 1.05, 1.02], "shade": { "amount": 0.9 }, "shafts": { "strength": 0.08 } },
    "2": { "ambient": [1.06, 1.05, 1.02], "shade": { "amount": 0.9 }, "objectLight": { "strength": 0.5 }, "motes": { "amount": 14, "strength": 0.3 }, "shafts": { "strength": 0.1 } },
    "3": { "ambient": [0.97, 0.95, 0.92], "corners": 0.2, "objectLight": { "radius": 150, "strength": 0.6 }, "baked": { "lights": 0.17, "rays": 0.0 }, "shadows": { "strength": 0.85, "length": 60, "lamps": 1.0, "sun": 0.0, "skipPainted": false }, "motes": { "amount": 14, "strength": 0.3 } },
    "room": { "ambient": [0.98, 0.97, 0.96], "corners": 0.18, "objectLight": { "radius": 150, "strength": 0.6 }, "baked": { "lights": 0.17, "rays": 0.0 }, "shadows": { "strength": 0.85, "length": 60, "lamps": 1.0, "sun": 0.0, "skipPainted": false } },
    "5": { "ambient": [1.03, 1.02, 1.0], "corners": 0.1, "baked": { "lights": 0.1 }, "shadows": { "lamps": 1.0, "sun": 0.0 } },
    "8": { "ambient": [1.06, 1.05, 1.02], "shade": { "amount": 0.9 }, "shafts": { "strength": 0.08 } },
    "9": { "ambient": [0.95, 0.84, 0.76], "corners": 0.3, "playerLight": { "radius": 220, "strength": 0.15 }, "glow": 0.2, "shadows": { "strength": 0.5, "length": 50, "tone": [0.5, 0.42, 0.4], "lamps": 1.0 }, "motes": { "amount": 40, "strength": 0.5, "tone": [1.0, 0.6, 0.25] } },
    "11": { "ambient": [0.8, 0.88, 1.0], "corners": 0.3, "playerLight": { "radius": 240, "strength": 0.2 }, "shadows": { "strength": 0.6, "length": 55, "tone": [0.42, 0.46, 0.55], "lamps": 1.0 }, "motes": { "amount": 22, "strength": 0.3, "tone": [0.8, 0.95, 1.0] } },
    "12": { "ambient": [1.08, 1.06, 1.0], "shade": { "amount": 0.6 }, "shafts": { "strength": 0.08 } },
    "14": { "ambient": [0.9, 0.98, 1.08], "corners": 0.25, "shadows": { "strength": 0.5, "tone": [0.5, 0.58, 0.68] }, "baked": { "lights": 0.2 }, "motes": { "amount": 40, "strength": 0.4, "tone": [0.8, 0.95, 1.0] }, "shafts": { "strength": 0.2, "tone": [0.7, 0.92, 1.0], "angle": 14 } },
    "battle": { "ambient": [1.03, 1.02, 1.0], "corners": 0.2, "effectLights": 1.0, "shadows": { "strength": 0.85, "length": 70, "start": 1, "skipPainted": false, "figures": 0.0 } },
    "4": {
      "ambient": [0.4, 0.44, 0.6],
      "corners": 0.45,
      "playerLight": { "radius": 250, "strength": 0.38 },
      "othersLight": { "strength": 0.0 },
      "doorLight": { "tone": [1.0, 0.84, 0.6], "radius": 220, "strength": 0.8, "flicker": 0.0 },
      "objectLight": { "radius": 170, "strength": 0.9 },
      "glow": 0.3,
      "shadows": { "strength": 0.8, "length": 60, "tone": [0.4, 0.4, 0.46], "lamps": 1.0 },
      "motes": { "amount": 26, "strength": 0.3 }
    },
    "6": {
      "ambient": [1.08, 1.08, 1.0],
      "corners": 0.15,
      "shade": { "tone": [0.5, 0.56, 0.52], "amount": 0.95, "scale": 2.4, "speed": [3, 1] },
      "shadows": { "objects": 0.0 },
      "motes": { "amount": 36, "strength": 0.6 },
      "shafts": { "strength": 0.18 }
    }
  },
  "maps": {
    "11016": { "ambient": [1.0, 1.0, 1.0] }
  }
})json";

        void readNumber(const Json& object, const char* key, float& value) {
            if (const auto found = object.find(key); found != object.end() && found->is_number()) {
                value = found->get<float>();
            }
        }

        void readTone(const Json& object, const char* key, gfx::LightMap::Tone& tone) {
            const auto found = object.find(key);
            if (found != object.end() && found->is_array() && found->size() == 3 && (*found)[0].is_number() && (*found)[1].is_number() && (*found)[2].is_number()) {
                tone = {(*found)[0].get<float>(), (*found)[1].get<float>(), (*found)[2].get<float>()};
            }
        }

        template <typename Source>
        void readSource(const Json& look, const char* key, Source& source) {
            const auto found = look.find(key);
            if (found == look.end() || !found->is_object()) {
                return;
            }
            readTone(*found, "tone", source.tone);
            float radius = static_cast<float>(source.radius);
            readNumber(*found, "radius", radius);
            source.radius = static_cast<int>(radius);
            readNumber(*found, "strength", source.strength);
            readNumber(*found, "flicker", source.flicker);
        }

        /// @brief A number that is the same each time for a pair of numbers, and looks like chance.
        std::uint32_t hashOf(int a, int b) {
            std::uint32_t h = static_cast<std::uint32_t>(a) * 374761393u + static_cast<std::uint32_t>(b) * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return h ^ (h >> 16);
        }

        /// @brief A slow waver from -1 to 1 that differs from light to light.
        float waver(std::uint64_t nowMs, int index) {
            const double t = static_cast<double>(nowMs % 600000) / 1000.0 + index * 1.7;
            return static_cast<float>(0.6 * std::sin(t * 7.3) + 0.4 * std::sin(t * 12.1 + 1.3));
        }

    }

    void WorldLighting::refresh(int mapId, const std::string& kind, std::uint64_t nowMs) {
        constexpr std::uint64_t kCheckMs = 500; // how often the file is looked at
        bool changed = false;
        if (!m_read || nowMs > m_checkedMs + kCheckMs) {
            m_checkedMs = nowMs;
            std::error_code error;
            if (!m_read) { // once: the built-in looks where there is no file, or one of an older build
                std::ifstream old(m_file, std::ios::binary);
                const Json had = old ? Json::parse(old, nullptr, false) : Json();
                if (!had.is_object() || had.value("version", 0) < Json::parse(kBuiltIn).value("version", 0)) {
                    old.close();
                    std::ofstream(m_file, std::ios::binary | std::ios::trunc) << kBuiltIn;
                }
            }
            const auto time = std::filesystem::last_write_time(m_file, error);
            if (!m_read || (!error && time != m_fileTime)) {
                m_fileTime = time;
                std::ifstream in(m_file, std::ios::binary);
                std::stringstream text;
                text << in.rdbuf();
                m_text = Json::accept(text.str()) ? text.str() : std::string(kBuiltIn);
                core::logInfo("lighting: looks read from ", m_file.string());
                m_read = true;
                changed = true;
            }
        }
        if (!changed && !m_optionsChanged && mapId == m_lookMap && kind == m_lookKind) {
            return;
        }
        m_optionsChanged = false;
        m_lookMap = mapId;
        m_lookKind = kind;
        const Json root = Json::parse(m_text, nullptr, false);
        m_enabled = m_options.enabled && root.is_object() && root.value("enabled", false);
        Json look = Json::object();
        if (root.is_object()) {
            const auto merge = [&](const char* group, const std::string& key) {
                const auto found = root.find(group);
                if (found != root.end() && found->is_object() && found->contains(key) && (*found)[key].is_object()) {
                    look.merge_patch((*found)[key]);
                }
            };
            merge("types", "default");
            merge("types", kind);
            merge("maps", std::to_string(mapId));
        }
        m_look = Look{};
        readTone(look, "ambient", m_look.ambient);
        readNumber(look, "corners", m_look.corners);
        readNumber(look, "occlusion", m_look.occlusion);
        readNumber(look, "effectLights", m_look.effectLights);
        if (const auto shade = look.find("shade"); shade != look.end() && shade->is_object()) {
            readTone(*shade, "tone", m_look.shade);
            readNumber(*shade, "amount", m_look.shadeAmount);
            readNumber(*shade, "scale", m_look.shadeScale);
            const auto speed = shade->find("speed");
            if (speed != shade->end() && speed->is_array() && speed->size() == 2 && (*speed)[0].is_number() && (*speed)[1].is_number()) {
                m_look.shadeSpeedX = (*speed)[0].get<float>();
                m_look.shadeSpeedY = (*speed)[1].get<float>();
            }
        }
        readSource(look, "playerLight", m_look.player);
        readSource(look, "othersLight", m_look.others);
        readSource(look, "doorLight", m_look.door);
        readSource(look, "objectLight", m_look.object);
        readNumber(look, "flicker", m_look.flicker);
        readNumber(look, "glow", m_look.glow);
        readNumber(look, "vivid", m_look.vivid);
        if (const auto burning = look.find("npcLights"); burning != look.end() && burning->is_object()) {
            for (const auto& [key, value] : burning->items()) {
                gfx::LightMap::Tone tone;
                const Json holder{{"tone", value}};
                readTone(holder, "tone", tone);
                if (const int id = std::atoi(key.c_str()); id > 0 && value.is_array()) {
                    m_look.npcLights[id] = tone;
                }
            }
        }
        if (const auto shadows = look.find("shadows"); shadows != look.end() && shadows->is_object()) {
            readNumber(*shadows, "strength", m_look.shadows);
            readNumber(*shadows, "length", m_look.shadowLength);
            readNumber(*shadows, "start", m_look.shadowStart);
            readNumber(*shadows, "lamps", m_look.shadowLamps);
            readNumber(*shadows, "sun", m_look.shadowSun);
            readNumber(*shadows, "width", m_look.shadowWidth);
            readNumber(*shadows, "figures", m_look.figureShade);
            if (const auto shaped = shadows->find("shaped"); shaped != shadows->end() && shaped->is_boolean()) {
                m_look.shapedShadows = shaped->get<bool>();
            }
            readTone(*shadows, "tone", m_look.shadowTone);
            if (const auto painted = shadows->find("skipPainted"); painted != shadows->end() && painted->is_boolean()) {
                m_look.paintedShade = painted->get<bool>();
            }
            readNumber(*shadows, "objects", m_look.objectShadows);
            const auto direction = shadows->find("direction");
            if (direction != shadows->end() && direction->is_array() && direction->size() == 2 && (*direction)[0].is_number() && (*direction)[1].is_number()) {
                m_look.shadowX = (*direction)[0].get<float>();
                m_look.shadowY = (*direction)[1].get<float>();
            }
        }
        if (const auto water = look.find("water"); water != look.end() && water->is_object()) {
            readNumber(*water, "sparkle", m_look.sparkle);
        }
        if (const auto motes = look.find("motes"); motes != look.end() && motes->is_object()) {
            readNumber(*motes, "amount", m_look.motes);
            readNumber(*motes, "strength", m_look.moteStrength);
            readTone(*motes, "tone", m_look.moteTone);
        }
        if (const auto baked = look.find("baked"); baked != look.end() && baked->is_object()) {
            readNumber(*baked, "lights", m_look.bakedLights);
            readNumber(*baked, "radius", m_look.bakedRadius);
            readNumber(*baked, "rays", m_look.bakedRays);
            readNumber(*baked, "rayLength", m_look.rayLength);
        }
        if (const auto shafts = look.find("shafts"); shafts != look.end() && shafts->is_object()) {
            readNumber(*shafts, "strength", m_look.shafts);
            readTone(*shafts, "tone", m_look.shaftTone);
            readNumber(*shafts, "angle", m_look.shaftAngle);
        }
        // what the player has switched off is left out of the look
        if (!m_options.shadows) {
            m_look.shadows = 0;
            m_look.objectShadows = 0;
        }
        if (!m_options.clouds) {
            m_look.shadeAmount = 0;
        }
        if (!m_options.occlusion) {
            m_look.occlusion = 0;
        }
        if (!m_options.air) {
            m_look.sparkle = 0;
            m_look.motes = 0;
            m_look.shafts = 0;
        }
    }

    void WorldLighting::paintGround(
        gfx::Canvas& canvas,
        gfx::PictureLibrary& pictures,
        WorldMap& map,
        const Camera& camera,
        int sceneType,
        const std::vector<std::reference_wrapper<MapCharacter>>& players,
        const std::vector<MapNpc>& npcs,
        const std::vector<MapPet>& pets,
        std::uint64_t nowMs
    ) {
        refresh(map.id(), kindOf(map, sceneType), nowMs);
        m_accelerated = canvas.device().isAccelerated();
        m_castParts.clear();
        m_leans.clear();
        if (!m_enabled || !m_accelerated) {
            return;
        }
        const core::Rect window = canvas.window();
        const core::Point origin = canvas.origin();
        const gfx::RenderDevice& device = canvas.device();
        constexpr int kMargin = 80; // how far outside the view a thing still casts a shadow into it, px
        const auto inView = [&](core::Point at) { return at.x > -kMargin && at.y > -kMargin && at.x < window.width + kMargin && at.y < window.height + kMargin; };

        if (m_look.sparkle > 0) {
            constexpr int kCell = formats::MapFile::kCellSize;
            constexpr int kWater = formats::MapFile::kDeepWater | formats::MapFile::kShallowWater;
            const core::Point topLeft = camera.origin();
            const double seconds = static_cast<double>(nowMs % 3600000) / 1000.0;
            for (int cellY = topLeft.y / kCell; cellY <= (topLeft.y + window.height) / kCell; ++cellY) {
                for (int cellX = topLeft.x / kCell; cellX <= (topLeft.x + window.width) / kCell; ++cellX) {
                    const std::uint32_t chance = hashOf(cellX, cellY);
                    if (chance % 3 != 0 || (map.file().attribute(cellX, cellY) & kWater) == 0) { // a cell in three glints
                        continue;
                    }
                    const float wave = static_cast<float>(std::sin(seconds * 1.7 + static_cast<double>(chance % 628) / 100.0));
                    const float bright = wave > 0 ? wave * wave * wave * wave : 0;
                    if (bright < 0.05f) {
                        continue;
                    }
                    const core::Point at = camera.toScreen({cellX * kCell + static_cast<int>((chance >> 8) % kCell), cellY * kCell + static_cast<int>((chance >> 16) % kCell)}) + origin;
                    m_lightMap.drawSpot(device, at, 7, 2, 0, {0.9f, 0.97f, 1.0f}, m_look.sparkle * bright, true);
                }
            }
        }

        const bool wanted = m_look.paintedShade && map.file().tileMode() == 0 && map.background().isValid();
        if (m_paintedMap != map.id() || m_paintedWanted != wanted) {
            m_paintedMap = map.id();
            m_paintedWanted = wanted;
            if (wanted) {
                findPaintedShade(device, pictures, map);
            } else {
                m_lightMap.clearPaintedShade();
                m_painted.clear();
            }
        }
        const bool bakedWanted = (m_look.bakedLights > 0 || m_look.bakedRays > 0) && map.file().tileMode() == 0 && map.background().isValid();
        if (m_bakedMap != map.id() || m_bakedWanted != bakedWanted) {
            m_bakedMap = map.id();
            m_bakedWanted = bakedWanted;
            m_baked.clear();
            if (bakedWanted) {
                findBakedLights(pictures, map);
            }
        }
        if (m_look.shadows <= 0 && m_look.objectShadows <= 0 && m_look.occlusion <= 0) {
            return;
        }
        gfx::LightMap::Frame shade;
        setShade(shade, canvas, camera, nowMs);
        std::vector<gfx::LightMap::Shadow> shadows;
        constexpr float kFlat = 0.6f; // the ground is seen from above at a slant: what lies on it is this much shorter up and down
        const bool ownLight = m_look.player.strength > 0 && !players.empty();
        // the lights a shadow falls away from, on the screen: the own player's, and as the look's 'lamps' says the others
        struct Lamp {
            core::Point at;
            float reach = 0; // no shadow of it falls farther away, px
            float pull = 0; // how much of a shadow it makes, 0 to 1
            bool own = false; // the own player's: what carries it casts no shadow
        };
        std::vector<Lamp> lamps;
        constexpr float kReach = 1.3f; // a light casts shadows this many times as far as it shines
        const auto lampAt = [&](core::Point mapPoint, float radius, float pull, bool own) {
            if (pull > 0 && radius > 0) {
                lamps.push_back({camera.toScreen(mapPoint) + origin, radius * kReach, std::min(1.0f, pull), own});
            }
        };
        if (ownLight) {
            lampAt(players.front().get().position, static_cast<float>(m_look.player.radius), 1, true);
        }
        if (m_look.shadowLamps > 0) {
            if (m_look.others.strength > 0) {
                for (std::size_t i = 1; i < players.size(); ++i) {
                    lampAt(players[i].get().position, static_cast<float>(m_look.others.radius), m_look.shadowLamps, false);
                }
            }
            if (m_look.door.strength > 0) {
                for (const DoorLight& light : map.doorLights()) {
                    lampAt(light.position, static_cast<float>(m_look.door.radius), m_look.shadowLamps, false);
                }
            }
            if (m_look.object.strength > 0) {
                for (const GroundObject& object : map.objects()) {
                    if (object.light && object.picture.isValid()) {
                        lampAt({object.position.x + pictures.picture(object.picture).width() / 2, object.footY()}, static_cast<float>(m_look.object.radius), m_look.shadowLamps, false);
                    }
                }
            }
            if (m_look.object.strength > 0 && !m_look.npcLights.empty()) {
                for (const MapNpc& npc : npcs) {
                    if (npc.visible && m_look.npcLights.count(npc.info.npcId) != 0) {
                        lampAt(npc.body.position, static_cast<float>(m_look.object.radius), m_look.shadowLamps, false);
                    }
                }
            }
            if (m_look.bakedLights > 0) {
                for (const Baked& light : m_baked) {
                    lampAt(light.at, static_cast<float>(light.radius) * m_look.bakedRadius, m_look.shadowLamps * light.strength * light.casts, false);
                }
            }
        }
        // where the shadow of a thing standing at a point of the screen falls, and how much of it shows
        struct Fall {
            float dx = 0;
            float dy = 0;
            float strength = 0; // 0: none
            float lampShare = 0; // how much of it the lights near by make, 0 (the look's direction alone) to 1
            float length = 1; // of the look's length
            float lampPart = 0; // of the way the shadow falls, how much the lights near by make (the rest the sun)
            float lampX = 0; // where the lights near by are as one light, on the ground: the screen's x, its y divided by kFlat
            float lampY = 0;
            float lampHigh = 100; // and how high that light hangs to cast a shadow as long as the look's, px
        };
        const auto fallAt = [&](core::Point at, bool carriesOwn) {
            constexpr float kOwnNear = 12; // nearer than this to the own player's light a thing carries it, px
            float lampX = 0;
            float lampY = 0;
            float pulls = 0;
            float most = 0;
            float mostOf = 0; // the pull of the light that pulls most here, as it is at the light itself
            float fars = 0;
            float towardX = 0; // from the lights, on the ground
            float towardY = 0;
            for (const Lamp& lamp : lamps) {
                if (lamp.own && carriesOwn) {
                    continue;
                }
                const float x = static_cast<float>(at.x - lamp.at.x);
                const float y = static_cast<float>(at.y - lamp.at.y) / kFlat;
                const float far = std::sqrt(x * x + y * y);
                if (far < (lamp.own ? kOwnNear : 1.0f) || far > lamp.reach) {
                    continue;
                }
                const float pull = lamp.pull * (1 - far / lamp.reach);
                // the stronger lights near by decide where the shadow falls, the strongest most of all
                const float weight = pull * pull * pull;
                lampX += weight * x / far;
                lampY += weight * y / far * kFlat * kFlat;
                towardX += weight * x / far;
                towardY += weight * y / far;
                pulls += weight;
                fars += weight * far;
                if (pull > most) {
                    most = pull;
                    mostOf = lamp.pull;
                }
            }
            Fall fall;
            const float lampSize = std::sqrt(lampX * lampX + lampY * lampY);
            if (pulls > 0 && lampSize > 0) {
                // a weak light casts a weak shadow, and between lights that pull apart the shadow fades instead of swinging round
                const float agreed = std::clamp((lampSize / pulls - 0.1f) / 0.3f, 0.0f, 1.0f);
                fall.lampShare = std::min(std::sqrt(mostOf), most * 2.5f) * agreed;
            }
            // under a cloud nothing casts a shadow along the look's direction; where the own player has a light there is no other
            const float sun = ownLight ? 0 : std::clamp(m_look.shadowSun, 0.0f, 1.0f) * (1 - m_lightMap.shadeAt(shade, at)) * (1 - fall.lampShare);
            const float sunSize = std::sqrt(m_look.shadowX * m_look.shadowX + m_look.shadowY * m_look.shadowY);
            if (sunSize > 0) {
                fall.dx = m_look.shadowX / sunSize * sun;
                fall.dy = m_look.shadowY / sunSize * sun;
            }
            if (fall.lampShare > 0) {
                fall.dx += lampX / lampSize * fall.lampShare;
                fall.dy += lampY / lampSize * fall.lampShare;
            }
            const float size = std::sqrt(fall.dx * fall.dx + fall.dy * fall.dy);
            if (size <= 0.001f) {
                return Fall{};
            }
            fall.dx /= size;
            fall.dy /= size;
            fall.strength = std::min(1.0f, size);
            // a light hangs over the ground: right under it a thing's shadow is short, and it grows with the way from the light
            constexpr float kLampHigh = 110; // from this far a light's shadow has its whole length, px
            const float lampLong = fall.lampShare > 0 ? std::clamp(fars / pulls / kLampHigh, 0.15f, 1.0f) : 1.0f;
            fall.length = (sun + fall.lampShare * lampLong) / (sun + fall.lampShare);
            fall.lampPart = fall.lampShare / (sun + fall.lampShare);
            if (const float toward = std::sqrt(towardX * towardX + towardY * towardY); fall.lampShare > 0 && toward > 0) {
                const float far = fars / pulls;
                fall.lampX = static_cast<float>(at.x) - towardX / toward * far;
                fall.lampY = static_cast<float>(at.y) / kFlat - towardY / toward * far;
                // a point h high casts its shadow far x h / (high - h) away: near the feet that is as long as the look says
                fall.lampHigh = far / std::max(0.05f, m_look.shadowLength / 88.0f * lampLong);
            }
            return fall;
        };
        // a shadow's ellipse from the foot of what casts it
        const auto cast = [&](core::Point foot, float strength, float length, float width, bool own) {
            const core::Point at = camera.toScreen(foot) + origin;
            if (strength <= 0 || !inView(at)) {
                return;
            }
            const Fall fall = fallAt(at, own);
            strength *= fall.strength;
            if (strength <= 0.02f) {
                return;
            }
            length *= fall.length;
            const float near = -m_look.shadowStart; // the shadow starts a little behind the foot
            const core::Point middle{at.x + static_cast<int>(fall.dx * (length / 2 - near)), at.y + static_cast<int>(fall.dy * (length / 2 - near))};
            shadows.push_back({middle, std::max(2, static_cast<int>(length / 2)), std::max(2, static_cast<int>(width / 2)), std::atan2(fall.dy, fall.dx) * 57.29578f, std::min(1.0f, strength)});
        };
        // the figures as they were drawn a frame ago, laid flat: their textures must be there still
        std::vector<gfx::LightMap::Sprite> sprites;
        std::vector<SDL_Vertex> solids; // the figures' shadows cast as solid things (the player's choice)
        // a part of a shadow, kept for what stands in it: where it lies on the ground, as drawShapes lays it there
        const auto keep = [&](const gfx::LightMap::Sprite& part, core::Point owner) {
            if (part.texture == nullptr || part.to.w <= 0 || part.to.h <= 0 || part.amount * part.alpha <= 0.02f) {
                return;
            }
            float left = 1e9f;
            float top = 1e9f;
            float right = -1e9f;
            float bottom = -1e9f;
            for (const float x : {part.to.x, part.to.x + part.to.w}) {
                for (const float y : {part.to.y, part.to.y + part.to.h}) {
                    const float over = std::max(0.0f, static_cast<float>(part.feet.y) - y);
                    const float beside = x - static_cast<float>(part.feet.x);
                    const float groundX = static_cast<float>(part.feet.x) + part.startX + beside * part.acrossX + over * part.leanX;
                    const float groundY = static_cast<float>(part.feet.y) + part.startY + beside * part.acrossY + over * part.leanY;
                    left = std::min(left, groundX);
                    top = std::min(top, groundY);
                    right = std::max(right, groundX);
                    bottom = std::max(bottom, groundY);
                }
            }
            m_castParts.push_back({part, owner, {left, top, right - left, bottom - top}});
        };
        const bool shaped = m_look.shapedShadows && m_look.shadows > 0 && m_lastValid && m_lastScene == map.id() && !gfx::freedTextures().all;
        if (shaped) {
            constexpr float kTall = 88; // a figure this tall casts a shadow of the look's length, px
            const core::Point mapAt = camera.toScreen({0, 0}) + origin;
            const core::Point moved{mapAt.x - m_lastMapAt.x, mapAt.y - m_lastMapAt.y};
            const float tall = m_look.shadowLength / kTall;
            for (const FigureEcho::Figure& figure : m_lastFigures.figures) {
                const core::Point at{figure.feet.x + moved.x, figure.feet.y + moved.y};
                if (!inView(at)) {
                    continue;
                }
                const Fall fall = fallAt(at, false);
                m_leans.push_back({at, fall.dx * tall * fall.length * fall.strength, fall.dy * tall * fall.length * fall.strength});
                if (m_look.shadows * fall.strength <= 0.02f) {
                    continue;
                }
                if (m_options.shadows3d) { // as a solid thing (the player's choice)
                    std::vector<gfx::LightMap::Sprite> parts;
                    for (std::size_t i = figure.first; i < figure.first + figure.count && i < m_lastFigures.sprites.size(); ++i) {
                        const gfx::SpriteEcho& drawn = m_lastFigures.sprites[i];
                        if (!gfx::freedTextures().has(drawn.texture)) {
                            parts.push_back({drawn.texture, drawn.from, {drawn.to.x + static_cast<float>(moved.x), drawn.to.y + static_cast<float>(moved.y), drawn.to.w, drawn.to.h}, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f});
                        }
                    }
                    SolidLight light;
                    light.flat = kFlat;
                    light.lampShare = fall.lampPart;
                    light.lampX = fall.lampX;
                    light.lampY = fall.lampY;
                    light.lampHigh = fall.lampHigh;
                    if (const float sunSize = std::sqrt(m_look.shadowX * m_look.shadowX + m_look.shadowY * m_look.shadowY); sunSize > 0) {
                        light.sunX = m_look.shadowX / sunSize * tall;
                        light.sunY = m_look.shadowY / sunSize * tall;
                    }
                    castSolidShadow(parts, at, light, std::min(1.0f, m_look.shadows * fall.strength), solids);
                    // what stands in it is darkened as by the flat shadow of the figure standing upright, near enough
                    float acrossX = -fall.dy;
                    float acrossY = fall.dx * 0.85f;
                    if (acrossX < 0) {
                        acrossX = -acrossX;
                        acrossY = -acrossY;
                    }
                    acrossX = 1 + (acrossX - 1) * fall.lampShare;
                    acrossY *= fall.lampShare;
                    for (gfx::LightMap::Sprite part : parts) {
                        part.feet = at;
                        part.amount = std::min(1.0f, m_look.shadows * fall.strength);
                        part.leanX = fall.dx * tall * fall.length;
                        part.leanY = fall.dy * tall * fall.length;
                        part.acrossX = acrossX;
                        part.acrossY = acrossY;
                        part.startX = fall.dx * m_look.shadowStart;
                        part.startY = fall.dy * m_look.shadowStart;
                        keep(part, at);
                    }
                    continue;
                }
                // Each column of the figure's sprites, a few pixels wide, is looked at for where it would touch the ground
                constexpr int kStrip = 2; // a column is this many of the texture's pixels wide
                constexpr float kClimb = 1.0f; // the ground under a column rises at most this many pixels for every pixel across
                constexpr int kNear = 3; // a column's ground is evened out with this many columns to each side
                struct Column {
                    std::size_t sprite = 0;
                    float fromX = 0; // its part of the sprite's rectangle, in the texture's pixels
                    float wide = 0;
                    float fromHigh = 0;
                    SDL_FRect slice{}; // where it was drawn, cut off under its lowest pixel
                    float ground = 0; // the screen's y where it touches the ground
                    float high = 0; // how far its highest pixel is over that, px
                };
                std::vector<Column> columns;
                std::vector<bool> cut(figure.count, false); // a sprite that is cast as columns (else as a whole)
                float seenTop = 1e9f;
                float seenBottom = -1e9f;
                float seenLeft = 1e9f;
                float seenRight = -1e9f;
                std::vector<float> bottoms;
                std::vector<float> tops;
                for (std::size_t i = figure.first; i < figure.first + figure.count && i < m_lastFigures.sprites.size(); ++i) {
                    const gfx::SpriteEcho& drawn = m_lastFigures.sprites[i];
                    if (drawn.flipY || drawn.from.w <= 0 || drawn.from.h <= 0 || gfx::freedTextures().has(drawn.texture) || !gfx::spriteColumnBottoms(drawn.texture, drawn.from, kStrip, bottoms, tops)) {
                        continue;
                    }
                    const SDL_FRect to{drawn.to.x + static_cast<float>(moved.x), drawn.to.y + static_cast<float>(moved.y), drawn.to.w, drawn.to.h};
                    const float scaleX = to.w / drawn.from.w;
                    const float scaleY = to.h / drawn.from.h;
                    // the ground under a column: where its lowest pixel is, unless that lies far over its neighbours' ground.
                    std::vector<float> grounded(bottoms.size(), -1);
                    for (std::size_t column = 0; column < bottoms.size(); ++column) {
                        if (bottoms[column] < 0) {
                            continue;
                        }
                        float lowest = bottoms[column];
                        for (std::size_t other = 0; other < bottoms.size(); ++other) {
                            if (bottoms[other] >= 0) {
                                const float apart = static_cast<float>(other > column ? other - column : column - other) * static_cast<float>(kStrip);
                                lowest = std::max(lowest, bottoms[other] - kClimb * apart);
                            }
                        }
                        grounded[column] = lowest;
                    }
                    // evened out with its neighbours: where it stepped from column to column the shadows' far ends stood apart like the
                    // teeth of a comb
                    for (std::size_t column = 0; column < bottoms.size(); ++column) {
                        if (bottoms[column] < 0) {
                            continue;
                        }
                        float sum = 0;
                        int count = 0;
                        for (std::size_t other = column >= kNear ? column - kNear : 0; other < std::min(bottoms.size(), column + kNear + 1); ++other) {
                            if (grounded[other] >= 0) {
                                sum += grounded[other];
                                ++count;
                            }
                        }
                        const float fromX = drawn.from.x + static_cast<float>(column * kStrip);
                        const float wide = std::min(static_cast<float>(kStrip), drawn.from.x + drawn.from.w - fromX);
                        const float left = drawn.flipX ? drawn.from.x + drawn.from.w - (fromX + wide) : fromX - drawn.from.x;
                        const float fromHigh = bottoms[column] + 1 - drawn.from.y;
                        Column piece;
                        piece.sprite = i;
                        piece.fromX = fromX;
                        piece.wide = wide;
                        piece.fromHigh = fromHigh;
                        piece.slice = {to.x + left * scaleX, to.y, wide * scaleX, fromHigh * scaleY};
                        piece.ground = to.y + (std::max(bottoms[column], sum / static_cast<float>(count)) + 1 - drawn.from.y) * scaleY;
                        piece.high = piece.ground - (to.y + (tops[column] - drawn.from.y) * scaleY);
                        columns.push_back(piece);
                        seenBottom = std::max(seenBottom, piece.slice.y + piece.slice.h);
                        seenLeft = std::min(seenLeft, piece.slice.x);
                        seenRight = std::max(seenRight, piece.slice.x + piece.slice.w);
                    }
                    cut[i - figure.first] = true;
                    SDL_FRect seen{};
                    if (gfx::spriteSeenPart(drawn.texture, drawn.from, seen)) {
                        seenTop = std::min(seenTop, to.y + (seen.y - drawn.from.y) * scaleY);
                    }
                }
                // how much it lies: by how far up the figure the ground it touches reaches (the middle of the columns' grounds, not the
                // few highest or lowest), or by how low its columns are over their ground for its width
                constexpr float kSpreadUpright = 0.25f; // of its height, or less: it stands
                constexpr float kSpreadLying = 0.4f; // or more: it lies
                constexpr float kLowUpright = 0.6f; // its columns' middle height as much as this of its width, or more: it stands
                constexpr float kLowLying = 0.4f; // or this much or less: it lies
                float lying = 0;
                const float seenHigh = seenBottom - seenTop;
                // a figure drawn from several sprites is a person (body, head, hair, what is held): it stands. Its head's columns end
                // at the neck, which read as ground far up the figure
                constexpr float kAloft = 8; // a figure whose lowest pixel is this far over its feet or more is drawn in the air, px
                if (figure.count == 1 && columns.size() >= 4 && seenHigh > 0 && static_cast<float>(at.y) - seenBottom < kAloft) {
                    std::vector<float> grounds;
                    grounds.reserve(columns.size());
                    for (const Column& piece : columns) {
                        grounds.push_back(piece.ground);
                    }
                    std::sort(grounds.begin(), grounds.end());
                    const float spread = (grounds[grounds.size() * 4 / 5] - grounds[grounds.size() / 5]) / seenHigh;
                    std::vector<float> highs;
                    highs.reserve(columns.size());
                    for (const Column& piece : columns) {
                        highs.push_back(piece.high);
                    }
                    std::nth_element(highs.begin(), highs.begin() + static_cast<std::ptrdiff_t>(highs.size() / 2), highs.end());
                    const float low = highs[highs.size() / 2] / std::max(1.0f, seenRight - seenLeft);
                    lying = std::max(std::clamp((spread - kSpreadUpright) / (kSpreadLying - kSpreadUpright), 0.0f, 1.0f),
                                     std::clamp((kLowUpright - low) / (kLowUpright - kLowLying), 0.0f, 1.0f));
                }
                // under the sun the shadow leans and its width stays as it is; away from a light near by an upright figure's lies as
                // the figure turned over, and one that lies on the ground keeps its shape as seen: every column of it stands on its own
                // ground and only what is over that falls away from the light, so its shadow starts all along its body, the tail too
                constexpr float kWide = 0.85f; // a shadow lying sideways keeps this much of the figure's width: flatter it is a line
                constexpr float kLyingHigh = 0.7f; // of what a lying thing's column has over its ground this much is height
                const float turn = fall.lampShare * (1 - lying);
                const float grounds = lying * fall.lampShare; // how much each column stands on its own ground, 0 to 1
                const float high = 1 - (1 - kLyingHigh) * grounds;
                float acrossX = -fall.dy;
                float acrossY = fall.dx * kWide;
                if (acrossX < 0) {
                    acrossX = -acrossX;
                    acrossY = -acrossY;
                }
                acrossX = 1 + (acrossX - 1) * turn;
                acrossY *= turn;
                const float leanX = fall.dx * tall * fall.length * high;
                const float leanY = fall.dy * tall * fall.length * high;
                const float amount = std::min(1.0f, m_look.shadows * fall.strength);
                const bool asColumns = grounds >= 0.5f; // mostly upright: cast as a whole (thin columns of it broke up into dots)
                for (std::size_t i = figure.first; i < figure.first + figure.count && i < m_lastFigures.sprites.size(); ++i) {
                    const gfx::SpriteEcho& drawn = m_lastFigures.sprites[i];
                    if ((asColumns && cut[i - figure.first]) || gfx::freedTextures().has(drawn.texture)) {
                        continue;
                    }
                    const SDL_FRect to{drawn.to.x + static_cast<float>(moved.x), drawn.to.y + static_cast<float>(moved.y), drawn.to.w, drawn.to.h};
                    sprites.push_back({drawn.texture, drawn.from, to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, at, amount, leanX, leanY, acrossX, acrossY, fall.dx * m_look.shadowStart, fall.dy * m_look.shadowStart});
                    keep(sprites.back(), at);
                }
                if (asColumns) {
                    // a column is a slice of a body that is deep too: its width lies a little up or down the ground as well (whichever
                    // lies further from the way the shadow falls), or lit from the side a slice casts a line too thin to be drawn whole
                    constexpr float kDeep = 3.0f; // px up or down the screen for every pixel across
                    constexpr float kSeam = 0.5f; // the columns overlap a little, so no line shows between them
                    const float upward = acrossY - kDeep * grounds;
                    const float downward = acrossY + kDeep * grounds;
                    const float deepY = std::abs(acrossX * leanY - upward * leanX) >= std::abs(acrossX * leanY - downward * leanX) ? upward : downward;
                    for (const Column& piece : columns) {
                        const gfx::SpriteEcho& drawn = m_lastFigures.sprites[piece.sprite];
                        const SDL_FRect slice{piece.slice.x - kSeam, piece.slice.y, piece.slice.w + 2 * kSeam, piece.slice.h};
                        // it stands on its own ground at its own middle; where that middle falls is what the figure as a whole (turned
                        // over, as much as it is not lying) gives it
                        const core::Point middle{static_cast<int>(std::lround(slice.x + slice.w / 2)), static_cast<int>(std::lround(static_cast<float>(at.y) + (piece.ground - static_cast<float>(at.y)) * grounds))};
                        const float beside = static_cast<float>(middle.x - at.x);
                        sprites.push_back({drawn.texture, {piece.fromX, drawn.from.y, piece.wide, piece.fromHigh}, slice, drawn.flipX, false, static_cast<float>(drawn.alpha) / 255.0f, middle, amount, leanX, leanY, acrossX, deepY,
                                           fall.dx * m_look.shadowStart + beside * (acrossX - 1), fall.dy * m_look.shadowStart + beside * acrossY});
                        keep(sprites.back(), at);
                    }
                }
            }
        } else {
            bool own = true;
            for (const MapCharacter& player : players) {
                cast(player.position, m_look.shadows, m_look.shadowLength, m_look.shadowWidth, own);
                own = false;
            }
            for (const MapNpc& npc : npcs) {
                if (npc.visible) {
                    cast(npc.body.position, m_look.shadows, m_look.shadowLength, m_look.shadowWidth, false);
                }
            }
            for (const MapPet& pet : pets) {
                if (pet.visible) {
                    cast(pet.body.position, m_look.shadows, m_look.shadowLength * 0.6f, m_look.shadowWidth * 0.8f, false);
                }
            }
        }
        const bool thingsShaped = m_look.shadows > 0 && m_lastValid && m_lastScene == map.id() && !gfx::freedTextures().all;
        if (thingsShaped) {
            // the things whose painted shadow is left out (trees) cast one as the figures do, as they were drawn a frame ago
            constexpr float kTall = 88; // as for the figures
            constexpr int kBeside = 600; // a thing this far outside the view may still cast into it, px
            const core::Point mapAt = camera.toScreen({0, 0}) + origin;
            const core::Point moved{mapAt.x - m_lastMapAt.x, mapAt.y - m_lastMapAt.y};
            const float tall = m_look.shadowLength / kTall;
            for (const FigureEcho::Caster& caster : m_lastFigures.casters) {
                const core::Point at{caster.feet.x + moved.x, caster.feet.y + moved.y};
                if (at.x < -kBeside || at.y < -kBeside || at.x > window.width + kBeside || at.y > window.height + kBeside) {
                    continue;
                }
                const Fall fall = fallAt(at, false);
                m_leans.push_back({at, fall.dx * tall * fall.length * fall.strength, fall.dy * tall * fall.length * fall.strength});
                if (m_look.shadows * fall.strength <= 0.02f) {
                    continue;
                }
                constexpr float kWide = 0.85f;
                float acrossX = -fall.dy;
                float acrossY = fall.dx * kWide;
                if (acrossX < 0) {
                    acrossX = -acrossX;
                    acrossY = -acrossY;
                }
                acrossX = 1 + (acrossX - 1) * fall.lampShare;
                acrossY *= fall.lampShare;
                for (std::size_t i = caster.first; i < caster.end && i < m_lastFigures.covers.size(); ++i) {
                    const gfx::SpriteEcho& drawn = m_lastFigures.covers[i];
                    if (gfx::freedTextures().has(drawn.texture)) {
                        continue;
                    }
                    SDL_FRect to{drawn.to.x + static_cast<float>(moved.x), drawn.to.y + static_cast<float>(moved.y), drawn.to.w, drawn.to.h};
                    SDL_FRect from = drawn.from;
                    // what reaches forward of where it stands (roots) lies on the ground: only what stands over it casts
                    if (const float over = static_cast<float>(at.y) - to.y; !drawn.flipY && to.h > 0 && over < to.h) {
                        if (over <= 0) {
                            continue;
                        }
                        from.h *= over / to.h;
                        to.h = over;
                    }
                    sprites.push_back({drawn.texture, from, to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, at, std::min(1.0f, m_look.shadows * fall.strength), fall.dx * tall * fall.length, fall.dy * tall * fall.length, acrossX, acrossY, 0.0f, 0.0f});
                    keep(sprites.back(), at); // kept for the figures and things standing in it
                }
            }
        }
        if (m_look.objectShadows > 0) {
            for (const GroundObject& object : map.objects()) {
                if (!object.light && object.picture.isValid()) {
                    const gfx::Picture& picture = pictures.picture(object.picture);
                    const float tall = static_cast<float>(picture.height() / std::max(1, object.info.frames));
                    const float wide = static_cast<float>(picture.width()); // loading another picture may move this one
                    const core::Point foot{object.position.x + picture.width() / 2, object.footY()};
                    if (thingsShaped && inView(camera.toScreen(foot) + origin) && map.plain(pictures, object.picture) != object.picture) {
                        continue; // it casts a shadow of its own shape already
                    }
                    cast(foot, m_look.objectShadows, tall * 0.9f, wide * 0.45f, false);
                }
            }
        }
        m_lightMap.castShadows(device, {window.width, window.height}, shadows, sprites, m_look.shadowTone, camera.toScreen(map.file().backgroundPosition()) + origin, true, solids);
        if (m_look.occlusion > 0 && m_lastValid && m_lastScene == map.id() && !gfx::freedTextures().all) {
            // little of the light all around reaches the ground right beside a thing that stands on it: a dark edge along the
            // outline of every thing and figure as it was drawn a frame ago, darkest at its foot, whatever the lights
            constexpr float kEdge = 16; // how far the edge lies around a thing, px
            constexpr float kStep = 0.34f; // how dark one of the edge's steps is at 'occlusion' 1
            constexpr float kFigureTop = 0.1f; // what is left of the edge at a figure's head
            // of anything else only the foot has an edge: a tall thing is often a part of a building painted into the background,
            // drawn over it to stand before the figures, and its upper outline lies on that building, not on the ground
            constexpr float kFoot = 40; // the height of a thing's foot, px
            constexpr float kFootLeast = 0.25f; // and the least part of the thing it is
            const float dark = std::min(1.0f, m_look.occlusion) * kStep;
            const core::Point mapAt = camera.toScreen({0, 0}) + origin;
            const core::Point moved{mapAt.x - m_lastMapAt.x, mapAt.y - m_lastMapAt.y};
            std::vector<gfx::LightMap::Sprite> edges;
            const auto edge = [&](const gfx::SpriteEcho& drawn, bool figure) {
                if (gfx::freedTextures().has(drawn.texture)) {
                    return;
                }
                SDL_FRect to{drawn.to.x + static_cast<float>(moved.x), drawn.to.y + static_cast<float>(moved.y), drawn.to.w, drawn.to.h};
                SDL_FRect from = drawn.from;
                if (!figure && !drawn.flipY && to.h > 0) {
                    const float foot = std::clamp(kFoot / to.h, kFootLeast, 1.0f);
                    to.y += to.h * (1 - foot);
                    to.h *= foot;
                    from.y += from.h * (1 - foot);
                    from.h *= foot;
                }
                const bool seen = to.x < static_cast<float>(window.width) + kEdge && to.y < static_cast<float>(window.height) + kEdge && to.x + to.w > -kEdge && to.y + to.h > -kEdge;
                const bool whole = to.w >= static_cast<float>(window.width) && to.h >= static_cast<float>(window.height); // no thing: a layer over all
                if (seen && !whole) {
                    edges.push_back({drawn.texture, from, to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f * dark, {}, figure ? kFigureTop : 0.0f});
                }
            };
            for (const auto& [first, end] : m_lastFigures.things) { // not the pieces a larger picture is cut into: their edges are no outline
                for (std::size_t i = first; i < end && i < m_lastFigures.covers.size(); ++i) {
                    edge(m_lastFigures.covers[i], false);
                }
            }
            for (const gfx::SpriteEcho& drawn : m_lastFigures.sprites) {
                edge(drawn, true);
            }
            m_lightMap.edgeShadows(device, {window.width, window.height}, edges, kEdge);
        }
    }

    void WorldLighting::setShade(gfx::LightMap::Frame& frame, const gfx::Canvas& canvas, const Camera& camera, std::uint64_t nowMs) const {
        const core::Point origin = canvas.origin();
        frame.shade = {m_look.shade.r * m_look.ambient.r, m_look.shade.g * m_look.ambient.g, m_look.shade.b * m_look.ambient.b};
        frame.shadeAmount = m_look.shadeAmount;
        frame.shadeScale = m_look.shadeScale;
        const double seconds = static_cast<double>(nowMs) / 1000.0;
        const core::Point mapAt = camera.toScreen({0, 0});
        frame.shadeAt = {
            mapAt.x + origin.x + static_cast<int>(std::fmod(seconds * m_look.shadeSpeedX, 65536.0)),
            mapAt.y + origin.y + static_cast<int>(std::fmod(seconds * m_look.shadeSpeedY, 65536.0))
        };
        // the second layer is larger by a ratio no fraction comes near, and drifts a little slower
        frame.shadeScale2 = m_look.shadeScale * 1.618f;
        frame.shadeAt2 = {
            mapAt.x + origin.x + 1337 + static_cast<int>(std::fmod(seconds * m_look.shadeSpeedX * 0.6, 65536.0)),
            mapAt.y + origin.y + 911 + static_cast<int>(std::fmod(seconds * m_look.shadeSpeedY * 0.6, 65536.0))
        };
    }

    void WorldLighting::leanAt(core::Point feet, float& x, float& y) const {
        // the lean worked out for what stood there when the shadows were cast, or for the nearest thing near by
        constexpr int kNear = 160; // px: a lean found this near is near enough
        int nearest = kNear * kNear + 1;
        const Lean* found = nullptr;
        for (const Lean& lean : m_leans) {
            const int dx = lean.feet.x - feet.x;
            const int dy = lean.feet.y - feet.y;
            if (const int far = dx * dx + dy * dy; far < nearest) {
                nearest = far;
                found = &lean;
            }
        }
        if (found != nullptr) {
            x = found->x;
            y = found->y;
            return;
        }
        // none near: along the look's direction, where there is a sun
        const float size = std::sqrt(m_look.shadowX * m_look.shadowX + m_look.shadowY * m_look.shadowY);
        const float sun = m_look.player.strength > 0 ? 0.0f : std::clamp(m_look.shadowSun, 0.0f, 1.0f);
        const float tall = m_look.shadowLength / 88.0f;
        x = size > 0 ? m_look.shadowX / size * tall * sun : 0;
        y = size > 0 ? m_look.shadowY / size * tall * sun : 0;
    }

    float WorldLighting::castShadeAt(core::Point feet, float high, float leanX, float leanY) const {
        if (leanX * leanX + leanY * leanY < 0.0001f || gfx::freedTextures().all) {
            return 0;
        }
        constexpr float kSelf = 10; // px: a shadow cast from this near the feet is the thing's own
        constexpr float kSpread = 3; // px: a few points around the one looked at, so that the shadow's edge is soft on a thing
        constexpr float kAbove = 2; // px: what casts the shadow must be at least this much higher than the point it falls on
        const float groundX = static_cast<float>(feet.x) + high * leanX;
        const float groundY = static_cast<float>(feet.y) + high * leanY;
        const float points[5][2] = {{0, 0}, {-kSpread, 0}, {kSpread, 0}, {0, -kSpread}, {0, kSpread}};
        float sum = 0;
        for (const auto& point : points) {
            const float x = groundX + point[0];
            const float y = groundY + point[1];
            float most = 0;
            for (const CastPart& part : m_castParts) {
                const gfx::LightMap::Sprite& sprite = part.sprite;
                const float amount = std::min(1.0f, sprite.amount * sprite.alpha);
                if (amount <= most || x < part.bounds.x || y < part.bounds.y || x > part.bounds.x + part.bounds.w || y > part.bounds.y + part.bounds.h) {
                    continue;
                }
                if (std::abs(static_cast<float>(part.owner.x - feet.x)) < kSelf && std::abs(static_cast<float>(part.owner.y - feet.y)) < kSelf) {
                    continue;
                }
                // back from the ground to the point of what casts it whose shadow falls there
                const float turn = sprite.acrossX * sprite.leanY - sprite.leanX * sprite.acrossY;
                if (std::abs(turn) < 0.001f) {
                    continue;
                }
                const float dx = x - static_cast<float>(sprite.feet.x) - sprite.startX;
                const float dy = y - static_cast<float>(sprite.feet.y) - sprite.startY;
                const float beside = (dx * sprite.leanY - sprite.leanX * dy) / turn;
                const float over = (sprite.acrossX * dy - sprite.acrossY * dx) / turn;
                if (over < high + kAbove) { // it stands between the light and what casts it: it is not in the shadow there
                    continue;
                }
                const float atX = static_cast<float>(sprite.feet.x) + beside;
                const float atY = static_cast<float>(sprite.feet.y) - over;
                if (atX < sprite.to.x || atY < sprite.to.y || atX >= sprite.to.x + sprite.to.w || atY >= sprite.to.y + sprite.to.h || gfx::freedTextures().has(sprite.texture)) {
                    continue;
                }
                gfx::SpriteMask mask;
                if (!gfx::spriteMask(sprite.texture, mask)) {
                    continue;
                }
                float u = (atX - sprite.to.x) / sprite.to.w * sprite.from.w;
                float v = (atY - sprite.to.y) / sprite.to.h * sprite.from.h;
                if (sprite.flipX) {
                    u = sprite.from.w - u;
                }
                if (sprite.flipY) {
                    v = sprite.from.h - v;
                }
                if (mask.seen(static_cast<int>(sprite.from.x + u), static_cast<int>(sprite.from.y + v))) {
                    most = amount;
                }
            }
            sum += most;
        }
        return sum / 5;
    }

    float WorldLighting::paintedShadeAt(core::Point point) const {
        if (m_painted.empty() || m_paintedCell <= 0) {
            return 0;
        }
        // between the middles of the four cells around the point
        const float x = static_cast<float>(point.x) / static_cast<float>(m_paintedCell) - 0.5f;
        const float y = static_cast<float>(point.y) / static_cast<float>(m_paintedCell) - 0.5f;
        const int left = static_cast<int>(std::floor(x));
        const int top = static_cast<int>(std::floor(y));
        const float partX = x - static_cast<float>(left);
        const float partY = y - static_cast<float>(top);
        const auto cell = [&](int cx, int cy) {
            if (cx < 0 || cy < 0 || cx >= m_paintedWidth || cy >= m_paintedHeight) {
                return 0.0f;
            }
            return static_cast<float>(m_painted[static_cast<std::size_t>(cy) * m_paintedWidth + cx]) / 255.0f;
        };
        const float over = cell(left, top) * (1 - partX) + cell(left + 1, top) * partX;
        const float under = cell(left, top + 1) * (1 - partX) + cell(left + 1, top + 1) * partX;
        return over * (1 - partY) + under * partY;
    }

    void WorldLighting::findPaintedShade(const gfx::RenderDevice& device, gfx::PictureLibrary& pictures, const WorldMap& map) {
        // a cell of the background is in painted shade when ground of its own colour without the shadow's tint lies near it: a shadow
        // dims the ground by its tone, while other ground that is only darker (grass beside sand) has another colour altogether
        constexpr int kCell = 8; // background pixels across a cell
        constexpr int kNear = 16; // cells around a cell in which the lit ground is looked for
        constexpr int kStep = 2; // every this many of them
        constexpr float kAlike = 0.12f; // a colour this close (of the lit one, by its farthest channel) is the same ground
        constexpr float kUnlike = 0.24f; // and this far is other ground
        constexpr float kDarkest = 40; // channels under this are compared as if they were this
        m_lightMap.clearPaintedShade();
        m_painted.clear();
        const auto background = pictures.decode(std::to_string(map.file().backgroundId()), map.file().tint());
        if (!background || background->isEmpty()) {
            return;
        }
        const int width = background->width() / kCell;
        const int height = background->height() / kCell;
        if (width <= 0 || height <= 0) {
            return;
        }
        const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::vector<float> red(count);
        std::vector<float> green(count);
        std::vector<float> blue(count);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int sums[3] = {0, 0, 0};
                for (int j = 0; j < kCell; j += 2) { // every other pixel is enough for an average
                    for (int i = 0; i < kCell; i += 2) {
                        const core::Color& pixel = background->at(x * kCell + i, y * kCell + j);
                        sums[0] += pixel.r;
                        sums[1] += pixel.g;
                        sums[2] += pixel.b;
                    }
                }
                const std::size_t cell = static_cast<std::size_t>(y) * width + x;
                constexpr float kPixels = (kCell / 2) * (kCell / 2);
                red[cell] = static_cast<float>(sums[0]) / kPixels;
                green[cell] = static_cast<float>(sums[1]) / kPixels;
                blue[cell] = static_cast<float>(sums[2]) / kPixels;
            }
        }
        const gfx::LightMap::Tone tone{std::max(0.05f, m_look.shadowTone.r), std::max(0.05f, m_look.shadowTone.g), std::max(0.05f, m_look.shadowTone.b)};
        std::vector<float> found(count);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t cell = static_cast<std::size_t>(y) * width + x;
                // the cell's ground as it would be in the sun
                const float litRed = red[cell] / tone.r;
                const float litGreen = green[cell] / tone.g;
                const float litBlue = blue[cell] / tone.b;
                if (std::max({litRed, litGreen, litBlue}) > 285) { // brighter than anything painted: it is in the sun itself
                    continue;
                }
                float nearest = kUnlike;
                for (int cy = std::max(0, y - kNear); cy <= std::min(height - 1, y + kNear) && nearest > kAlike; cy += kStep) {
                    for (int cx = std::max(0, x - kNear); cx <= std::min(width - 1, x + kNear); cx += kStep) {
                        const std::size_t other = static_cast<std::size_t>(cy) * width + cx;
                        const float apart = std::max({
                            std::abs(litRed - red[other]) / std::max(kDarkest, red[other]),
                            std::abs(litGreen - green[other]) / std::max(kDarkest, green[other]),
                            std::abs(litBlue - blue[other]) / std::max(kDarkest, blue[other])
                        });
                        nearest = std::min(nearest, apart);
                    }
                }
                found[cell] = std::clamp((kUnlike - nearest) / (kUnlike - kAlike), 0.0f, 1.0f);
            }
        }
        std::vector<std::uint8_t> shade(count);
        for (int y = 0; y < height; ++y) { // smoothed over the cells around each
            for (int x = 0; x < width; ++x) {
                float sum = 0;
                int cells = 0;
                for (int cy = std::max(0, y - 1); cy <= std::min(height - 1, y + 1); ++cy) {
                    for (int cx = std::max(0, x - 1); cx <= std::min(width - 1, x + 1); ++cx) {
                        sum += found[static_cast<std::size_t>(cy) * width + cx];
                        ++cells;
                    }
                }
                shade[static_cast<std::size_t>(y) * width + x] = static_cast<std::uint8_t>(sum / static_cast<float>(cells) * 255.0f);
            }
        }
        m_lightMap.setPaintedShade(device, width, height, shade, kCell);
        m_painted = shade;
        m_paintedWidth = width;
        m_paintedHeight = height;
        m_paintedCell = kCell;
        core::logInfo("lighting: painted shade of map ", map.id(), " found, ", width, " x ", height, " cells");
    }

    std::string WorldLighting::kindOf(const WorldMap& map, int sceneType) {
        constexpr int kDungeon = 4; // scene type 4: the caves, and whatever else is closed in
        if (sceneType == kDungeon && map.file().tileMode() == 0) {
            return "room"; // caves are laid of tiles; a painted map of the type is a lobby, a hall, a jail
        }
        return std::to_string(sceneType);
    }

    void WorldLighting::findBakedLights(gfx::PictureLibrary& pictures, const WorldMap& map) {
        // a light painted into a background shows as a patch much brighter than the picture around it: a lamp, a fire, a window,
        // the pool of light one throws on a floor. Each becomes a light of its colour; the pale ones that are no fire are windows
        constexpr int kCell = 8; // background pixels across a cell
        constexpr int kNear = 14; // cells around a cell that are "the picture around it"
        constexpr float kVoid = 14; // darker than this is the black beside a room, no part of the picture
        constexpr float kBright = 125; // a light is at least this bright
        constexpr float kOver = 1.4f; // and this many times as bright as the picture around it
        constexpr std::size_t kMost = 40; // lights kept, the strongest
        const auto background = pictures.decode(std::to_string(map.file().backgroundId()), map.file().tint());
        if (!background || background->isEmpty()) {
            return;
        }
        const int width = background->width() / kCell;
        const int height = background->height() / kCell;
        if (width <= 0 || height <= 0) {
            return;
        }
        const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::vector<float> red(count);
        std::vector<float> green(count);
        std::vector<float> blue(count);
        std::vector<float> bright(count);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int sums[3] = {0, 0, 0};
                for (int j = 0; j < kCell; j += 2) {
                    for (int i = 0; i < kCell; i += 2) {
                        const core::Color& pixel = background->at(x * kCell + i, y * kCell + j);
                        sums[0] += pixel.r;
                        sums[1] += pixel.g;
                        sums[2] += pixel.b;
                    }
                }
                const std::size_t cell = static_cast<std::size_t>(y) * width + x;
                constexpr float kPixels = (kCell / 2) * (kCell / 2);
                red[cell] = static_cast<float>(sums[0]) / kPixels;
                green[cell] = static_cast<float>(sums[1]) / kPixels;
                blue[cell] = static_cast<float>(sums[2]) / kPixels;
                bright[cell] = 0.3f * red[cell] + 0.59f * green[cell] + 0.11f * blue[cell];
            }
        }
        // sums over every rectangle from the top left, for the brightness around a cell (the void left out)
        const int across = width + 1;
        std::vector<double> sumOf(static_cast<std::size_t>(across) * (height + 1), 0.0);
        std::vector<int> cellsOf(sumOf.size(), 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t cell = static_cast<std::size_t>(y) * width + x;
                const bool solid = bright[cell] >= kVoid;
                const std::size_t at = static_cast<std::size_t>(y + 1) * across + x + 1;
                sumOf[at] = (solid ? bright[cell] : 0.0) + sumOf[at - 1] + sumOf[at - across] - sumOf[at - across - 1];
                cellsOf[at] = (solid ? 1 : 0) + cellsOf[at - 1] + cellsOf[at - across] - cellsOf[at - across - 1];
            }
        }
        const auto around = [&](int x, int y, int reach) {
            const int left = std::max(0, x - reach);
            const int top = std::max(0, y - reach);
            const int right = std::min(width, x + reach + 1);
            const int bottom = std::min(height, y + reach + 1);
            const auto corner = [&](int cx, int cy) { return static_cast<std::size_t>(cy) * across + cx; };
            const int cells = cellsOf[corner(right, bottom)] - cellsOf[corner(left, bottom)] - cellsOf[corner(right, top)] + cellsOf[corner(left, top)];
            const double sum = sumOf[corner(right, bottom)] - sumOf[corner(left, bottom)] - sumOf[corner(right, top)] + sumOf[corner(left, top)];
            return cells > 0 ? static_cast<float>(sum / cells) : 0.0f;
        };
        std::vector<std::uint8_t> lit(count, 0);
        std::vector<float> over(count, 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t cell = static_cast<std::size_t>(y) * width + x;
                const float picture = around(x, y, kNear);
                if (bright[cell] >= kBright && picture > 0 && bright[cell] >= picture * kOver) {
                    lit[cell] = 1;
                    over[cell] = bright[cell] / picture;
                }
            }
        }
        struct Found {
            Baked light;
            float weight = 0;
        };
        std::vector<Found> found;
        std::vector<int> open;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (lit[static_cast<std::size_t>(y) * width + x] != 1) {
                    continue;
                }
                // the patch this cell is of
                int cells = 0;
                double sumX = 0;
                double sumY = 0;
                double sums[3] = {0, 0, 0};
                float most = 0;
                float brightness = 0;
                open.assign(1, y * width + x);
                lit[static_cast<std::size_t>(y) * width + x] = 2;
                while (!open.empty()) {
                    const int cell = open.back();
                    open.pop_back();
                    const int cx = cell % width;
                    const int cy = cell / width;
                    ++cells;
                    sumX += cx;
                    sumY += cy;
                    sums[0] += red[cell];
                    sums[1] += green[cell];
                    sums[2] += blue[cell];
                    brightness += bright[cell];
                    most = std::max(most, over[cell]);
                    const int beside[4][2] = {{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
                    for (const auto& next : beside) {
                        if (next[0] < 0 || next[1] < 0 || next[0] >= width || next[1] >= height) {
                            continue;
                        }
                        const std::size_t other = static_cast<std::size_t>(next[1]) * width + next[0];
                        if (lit[other] == 1) {
                            lit[other] = 2;
                            open.push_back(static_cast<int>(other));
                        }
                    }
                }
                if (cells < 3 || cells > 1200) { // a speck, or half the picture
                    continue;
                }
                const float middleX = static_cast<float>(sumX / cells);
                const float middleY = static_cast<float>(sumY / cells);
                const float r = static_cast<float>(sums[0] / cells);
                const float g = static_cast<float>(sums[1] / cells);
                const float b = static_cast<float>(sums[2] / cells);
                const float top = std::max({r, g, b, 1.0f});
                Found one;
                one.light.at = {map.file().backgroundPosition().x + static_cast<int>((middleX + 0.5f) * kCell), map.file().backgroundPosition().y + static_cast<int>((middleY + 0.5f) * kCell)};
                one.light.radius = static_cast<int>(std::clamp(std::sqrt(static_cast<float>(cells)) * kCell * 1.5f + 48, 56.0f, 200.0f));
                one.light.tone = {0.4f + 0.6f * r / top, 0.4f + 0.6f * g / top, 0.4f + 0.6f * b / top}; // its colour, paler
                one.light.strength = std::clamp((most - 1.2f) * 1.2f, 0.35f, 1.0f) * std::clamp(150.0f / static_cast<float>(cells), 0.35f, 1.0f); // a wide patch is a lit floor, no lamp
                // only a light that stands out of the picture as one is seen to cast shadows: a floor a little brighter than the
                // room around it shows no light to fall from
                one.light.casts = std::clamp((most - 1.8f) / 0.25f, 0.0f, 1.0f);
                one.light.wavers = r > b * 1.3f && r >= g; // red and yellow: a fire, a lamp
                // daylight is pale, and a window is small
                one.light.window = !one.light.wavers && b >= r * 0.8f && g >= r * 0.9f && brightness / static_cast<float>(cells) >= 135 && cells <= 120;
                if (one.light.window) { // the ray falls to the side on which the picture under the window is brighter
                    constexpr int kUnder = 7; // cells under and beside the window that are looked at
                    const int cx = static_cast<int>(middleX);
                    const int cy = static_cast<int>(middleY);
                    const float leftSide = around(std::max(0, cx - kUnder), std::min(height - 1, cy + kUnder), 3);
                    const float rightSide = around(std::min(width - 1, cx + kUnder), std::min(height - 1, cy + kUnder), 3);
                    one.light.rayX = rightSide >= leftSide ? 0.6f : -0.6f;
                    one.light.rayY = 0.8f;
                }
                one.weight = static_cast<float>(cells) * (most - 1);
                found.push_back(one);
            }
        }
        std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.weight > b.weight; });
        if (found.size() > kMost) {
            found.resize(kMost);
        }
        int windows = 0;
        for (const Found& one : found) {
            m_baked.push_back(one.light);
            windows += one.light.window ? 1 : 0;
        }
        core::logInfo("lighting: ", m_baked.size(), " painted lights found on map ", map.id(), ", ", windows, " of them windows");
    }

    void WorldLighting::battleGround(gfx::Canvas& canvas, std::uint64_t nowMs) {
        constexpr int kBattle = -2; // no map's id: the battle's own look and figures
        refresh(kBattle, "battle", nowMs);
        m_accelerated = canvas.device().isAccelerated();
        if (!m_enabled || !m_accelerated || m_look.shadows <= 0 || !m_look.shapedShadows) {
            return;
        }
        if (!m_lastValid || m_lastScene != kBattle || gfx::freedTextures().all) {
            return;
        }
        constexpr float kTall = 88; // as on the map
        const core::Rect window = canvas.window();
        const float tall = m_look.shadowLength / kTall;
        const float size = std::sqrt(m_look.shadowX * m_look.shadowX + m_look.shadowY * m_look.shadowY);
        if (size <= 0) {
            return;
        }
        const float dx = m_look.shadowX / size;
        const float dy = m_look.shadowY / size;
        std::vector<gfx::LightMap::Sprite> sprites;
        std::vector<gfx::LightMap::Sprite> edges; // the ground is darker along the outline of a fighter who stands on it
        std::vector<SDL_Vertex> solids; // the fighters' shadows cast as solid things (the player's choice)
        std::vector<gfx::LightMap::Sprite> parts;
        SolidLight sun;
        sun.sunX = dx * tall;
        sun.sunY = dy * tall;
        constexpr float kHigh = 90; // a fighter this high over the ground (a jump) casts no shadow any more, px
        for (const FigureEcho::Figure& figure : m_lastFigures.figures) {
            // the shadow stays on the ground under a fighter in the air, and fades the higher he is
            const float near = std::clamp(1 - static_cast<float>(figure.lift) / kHigh, 0.0f, 1.0f);
            if (near <= 0) {
                continue;
            }
            parts.clear();
            for (std::size_t i = figure.first; i < figure.first + figure.count && i < m_lastFigures.sprites.size(); ++i) {
                const gfx::SpriteEcho& drawn = m_lastFigures.sprites[i];
                if (gfx::freedTextures().has(drawn.texture)) {
                    continue;
                }
                if (m_options.shadows3d) {
                    parts.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f});
                } else {
                    sprites.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, figure.feet, std::min(1.0f, m_look.shadows) * near, dx * tall, dy * tall, 1, 0, dx * m_look.shadowStart, dy * m_look.shadowStart});
                }
                if (m_look.occlusion > 0 && figure.lift <= 2) {
                    edges.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f * std::min(1.0f, m_look.occlusion) * 0.34f, {}, 0.1f});
                }
            }
            if (!parts.empty()) {
                castSolidShadow(parts, figure.feet, sun, std::min(1.0f, m_look.shadows) * near, solids);
            }
        }
        m_lightMap.castShadows(canvas.device(), {window.width, window.height}, {}, sprites, m_look.shadowTone, {0, 0}, false, solids);
        m_lightMap.edgeShadows(canvas.device(), {window.width, window.height}, edges, 16);
    }

    void WorldLighting::battleApply(gfx::Canvas& canvas, std::uint64_t nowMs) {
        constexpr int kBattle = -2;
        refresh(kBattle, "battle", nowMs);
        if (!m_enabled || !m_accelerated) {
            return;
        }
        const core::Rect window = canvas.window();
        FigureEcho& figures = figureEcho();
        m_lastValid = figures.active;
        if (m_lastValid) {
            m_lastFigures.sprites = figures.sprites;
            m_lastFigures.figures = figures.figures;
            gfx::freedTextures().clear(); // from here on the list tells which of these figures' textures are gone
            m_lastScene = kBattle;
        }
        gfx::LightMap::Frame frame;
        frame.ambient = m_look.ambient;
        frame.corners = m_look.corners;
        if (m_look.effectLights > 0) {
            // the effects drawn in this frame that shine light the field around them
            constexpr std::size_t kMost = 16;
            std::vector<EffectGlow>& glows = effectGlows();
            if (glows.size() > kMost) { // the strongest
                std::partial_sort(glows.begin(), glows.begin() + static_cast<std::ptrdiff_t>(kMost), glows.end(), [](const EffectGlow& a, const EffectGlow& b) {
                    return a.r + a.g + a.b > b.r + b.g + b.b;
                });
                glows.resize(kMost);
            }
            for (const EffectGlow& glow : glows) {
                frame.lights.push_back({glow.at, glow.radius, {glow.r * m_look.effectLights, glow.g * m_look.effectLights, glow.b * m_look.effectLights}, false});
            }
        }
        effectGlows().clear();
        m_lightMap.apply(canvas.device(), {window.width, window.height}, frame);
    }

    void WorldLighting::paintAir(gfx::Canvas& canvas, const Camera& camera, std::uint64_t nowMs) {
        const core::Rect window = canvas.window();
        const gfx::RenderDevice& device = canvas.device();
        const core::Point view = camera.origin();
        const double seconds = static_cast<double>(nowMs % 3600000) / 1000.0;
        if (m_look.shafts > 0) {
            constexpr int kApart = 420; // a shaft about every this many pixels
            constexpr float kNear = 1.1f; // the shafts pass by this much faster than the ground: they stand nearer
            // a shaft comes down along the light: from over a thing to where its shadow falls, which lies as far beside it as
            // the shadow is long and as far under its top as it is tall, less what the shadow reaches up the screen
            constexpr float kTall = 88; // as for the shadows
            float rayX = 0;
            float rayY = 1;
            if (m_look.shaftAngle >= 0) {
                rayX = std::sin(m_look.shaftAngle * 0.01745329f);
                rayY = std::cos(m_look.shaftAngle * 0.01745329f);
            } else if (const float size = std::sqrt(m_look.shadowX * m_look.shadowX + m_look.shadowY * m_look.shadowY); size > 0) {
                const float tall = m_look.shadowLength / kTall;
                rayX = m_look.shadowX / size * tall;
                rayY = std::max(0.25f, 1 + m_look.shadowY / size * tall);
            }
            const float degrees = std::atan2(rayY, rayX) * 57.29578f;
            // it stands on the map: going up or down the map it passes by sideways as it leans
            const int passed = static_cast<int>((static_cast<float>(view.x) - static_cast<float>(view.y) * rayX / rayY) * kNear);
            for (int slot = passed / kApart - 1; slot <= (passed + window.width) / kApart + 1; ++slot) {
                const std::uint32_t chance = hashOf(slot, 7);
                const int x = slot * kApart + static_cast<int>(chance % 220) - passed;
                const float swell = 0.75f + 0.25f * static_cast<float>(std::sin(seconds * 0.13 + static_cast<double>(chance % 628) / 100.0));
                m_lightMap.drawSpot(device, {x, window.height * 2 / 5}, window.height, 40 + static_cast<int>((chance >> 9) % 45), degrees, m_look.shaftTone, m_look.shafts * swell, true);
            }
        }
        if (m_look.bakedRays > 0) {
            const core::Point origin = canvas.origin();
            int index = 0;
            for (const Baked& light : m_baked) {
                ++index;
                if (!light.window) {
                    continue;
                }
                const core::Point at = camera.toScreen(light.at) + origin;
                const int length = static_cast<int>(m_look.rayLength);
                if (at.x < -length || at.y < -length || at.x > window.width + length || at.y > window.height + length) {
                    continue;
                }
                const float swell = 0.8f + 0.2f * static_cast<float>(std::sin(seconds * 0.4 + index * 1.9));
                const core::Point middle{at.x + static_cast<int>(light.rayX * static_cast<float>(length) * 0.45f), at.y + static_cast<int>(light.rayY * static_cast<float>(length) * 0.45f)};
                m_lightMap.drawSpot(device, middle, length / 2, std::max(10, light.radius / 4), std::atan2(light.rayY, light.rayX) * 57.29578f, light.tone, m_look.bakedRays * swell, true);
            }
        }
        const int motes = std::min(200, static_cast<int>(m_look.motes));
        if (motes > 0 && m_look.moteStrength > 0 && window.width > 0 && window.height > 0) {
            constexpr float kNear = 1.25f; // the specks float nearer than the ground
            for (int i = 0; i < motes; ++i) {
                const std::uint32_t chance = hashOf(i, 31);
                const double phase = static_cast<double>(chance % 628) / 100.0;
                const double x = static_cast<double>(chance % 4096) + seconds * 5 + 18 * std::sin(seconds * 0.31 + phase) - static_cast<double>(view.x) * kNear;
                const double y = static_cast<double>((chance >> 12) % 4096) - seconds * 3.5 + 10 * std::cos(seconds * 0.23 + phase) - static_cast<double>(view.y) * kNear;
                const core::Point at{static_cast<int>(x - std::floor(x / window.width) * window.width), static_cast<int>(y - std::floor(y / window.height) * window.height)};
                const float lit = 0.55f + 0.45f * static_cast<float>(std::sin(seconds * 0.6 + phase * 3));
                const int half = 2 + static_cast<int>((chance >> 24) % 2);
                m_lightMap.drawSpot(device, at, half, half, 0, m_look.moteTone, m_look.moteStrength * lit, true);
            }
        }
    }

    void WorldLighting::apply(
        gfx::Canvas& canvas,
        gfx::PictureLibrary& pictures,
        WorldMap& map,
        const Camera& camera,
        int sceneType,
        const std::vector<std::reference_wrapper<MapCharacter>>& players,
        const std::vector<MapNpc>& npcs,
        std::uint64_t nowMs
    ) {
        refresh(map.id(), kindOf(map, sceneType), nowMs);
        if (!m_enabled) {
            return;
        }
        constexpr int kChest = 30; // a figure's light is this far over its feet, px
        const core::Rect window = canvas.window();
        const core::Point origin = canvas.origin();
        gfx::LightMap::Frame frame;
        frame.ambient = m_look.ambient;
        frame.corners = m_look.corners;
        frame.glow = m_look.glow;
        frame.vivid = m_look.vivid;
        setShade(frame, canvas, camera, nowMs);
        frame.paintedAt = camera.toScreen(map.file().backgroundPosition()) + origin;
        int index = 0;
        const auto add = [&](const Source& source, core::Point mapPoint, bool steady) {
            ++index;
            if (source.strength <= 0 || source.radius <= 0) {
                return;
            }
            const core::Point at = camera.toScreen(mapPoint) + origin;
            const int radius = steady ? source.radius : static_cast<int>(static_cast<float>(source.radius) * (1 + (source.flicker >= 0 ? source.flicker : m_look.flicker) * waver(nowMs, index)));
            if (at.x + radius < 0 || at.y + radius < 0 || at.x - radius > window.width || at.y - radius > window.height) {
                return;
            }
            frame.lights.push_back({at, radius, {source.tone.r * source.strength, source.tone.g * source.strength, source.tone.b * source.strength}, !steady});
        };
        bool own = true;
        for (const MapCharacter& player : players) {
            add(own ? m_look.player : m_look.others, {player.position.x, player.position.y - kChest}, true);
            own = false;
        }
        for (const DoorLight& light : map.doorLights()) {
            add(m_look.door, light.position, false);
        }
        // the glowing objects and the NPCs that burn
        std::vector<std::pair<core::Point, gfx::LightMap::Tone>> burning;
        for (const GroundObject& object : map.objects()) {
            if (object.light && object.picture.isValid()) {
                const gfx::Picture& picture = pictures.picture(object.picture);
                burning.emplace_back(core::Point{object.position.x + picture.width() / 2, object.position.y + picture.height() / std::max(1, object.info.frames) / 2}, m_look.object.tone);
            }
        }
        if (!m_look.npcLights.empty()) {
            constexpr int kFlame = 18; // the light of a burning NPC is this far over its feet, px
            for (const MapNpc& npc : npcs) {
                if (const auto found = m_look.npcLights.find(npc.info.npcId); npc.visible && found != m_look.npcLights.end()) {
                    burning.emplace_back(core::Point{npc.body.position.x, npc.body.position.y - kFlame}, found->second);
                }
            }
        }
        // where several of them stand close their lights add up to far too much (a room of fires is white): each is as much weaker
        // as the others light its middle, so that every middle is about as bright as one light alone
        // as the others light it a little beyond its own reach too: between two lights both shine (9 fires a pillar apart, as in a
        // backroom puzzle, are then no brighter anywhere than about 0.7 of one fire alone)
        constexpr float kCrowd = 1.5f;
        const float reach = kCrowd * static_cast<float>(std::max(1, m_look.object.radius));
        for (const auto& [at, tone] : burning) {
            float others = 0;
            for (const auto& [other, otherTone] : burning) {
                const float dx = static_cast<float>(other.x - at.x) / reach;
                const float dy = static_cast<float>(other.y - at.y) / reach;
                const float inside = std::max(0.0f, 1 - (dx * dx + dy * dy)); // as the round light falls off
                others += &other == &at ? 0.0f : inside * inside;
            }
            add({tone, m_look.object.radius, m_look.object.strength / (1 + others), m_look.object.flicker}, at, false);
        }
        if (m_look.bakedLights > 0) {
            for (const Baked& light : m_baked) {
                ++index;
                const core::Point at = camera.toScreen(light.at) + origin;
                const float size = static_cast<float>(light.radius) * m_look.bakedRadius * (light.wavers ? 1 + m_look.flicker * waver(nowMs, index) : 1.0f);
                const int radius = static_cast<int>(size);
                if (radius <= 0 || at.x + radius < 0 || at.y + radius < 0 || at.x - radius > window.width || at.y - radius > window.height) {
                    continue;
                }
                const float strength = m_look.bakedLights * light.strength;
                frame.lights.push_back({at, radius, {light.tone.r * strength, light.tone.g * strength, light.tone.b * strength}, true});
            }
        }
        FigureEcho& figures = figureEcho();
        const bool painted = m_look.player.strength <= 0 && !m_painted.empty(); // where the own player has a light no painted shade counts
        if (m_accelerated && m_look.figureShade > 0 && (painted || !m_castParts.empty()) && (!figures.figures.empty() || !figures.things.empty())) {
            // a figure (or a thing) in a shadow gets darker as the ground there is. A point of it over the feet is in the shadow when
            // the ground is where its own shadow would fall: its head can be in the sun yet. The shadows cast in this frame darken
            // what stands in them, the painted shade of the background only the figures (the things are painted with their own)
            constexpr float kTall = 88; // as for the shadows
            const float tall = m_look.shadowLength / kTall;
            const float dark = m_look.figureShade * (1 - (m_look.shadowTone.r + m_look.shadowTone.g + m_look.shadowTone.b) / 3);
            std::vector<gfx::LightMap::Sprite> sprites;
            // what was drawn after a figure stands before it: the other things, and the figures after it
            std::vector<gfx::LightMap::Sprite> covers;
            std::vector<std::size_t> coversFrom;
            const auto cover = [&](const gfx::SpriteEcho& drawn) { covers.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, {}}); };
            // the things that may stand in a cast shadow, by their first cover: their last, and where they stand
            std::map<std::size_t, std::pair<std::size_t, core::Point>> things;
            if (!m_castParts.empty()) {
                for (const auto& [first, end] : figures.things) {
                    if (end <= first || end > figures.covers.size()) {
                        continue;
                    }
                    float left = 1e9f;
                    float top = 1e9f;
                    float right = -1e9f;
                    float bottom = -1e9f;
                    for (std::size_t i = first; i < end; ++i) {
                        const SDL_FRect& to = figures.covers[i].to;
                        left = std::min(left, to.x);
                        top = std::min(top, to.y);
                        right = std::max(right, to.x + to.w);
                        bottom = std::max(bottom, to.y + to.h);
                    }
                    if (right - left >= static_cast<float>(window.width) && bottom - top >= static_cast<float>(window.height)) {
                        continue; // no thing: a layer over all
                    }
                    if (right < 0 || bottom < 0 || left > static_cast<float>(window.width) || top > static_cast<float>(window.height)) {
                        continue;
                    }
                    core::Point feet{static_cast<int>((left + right) / 2), static_cast<int>(bottom)};
                    for (const FigureEcho::Caster& caster : figures.casters) {
                        if (caster.first == first) {
                            feet = caster.feet; // where the lighting casts its shadow from: it is not darkened by that
                        }
                    }
                    things[first] = {end, feet};
                }
            }
            constexpr float kFigure = 1; // what is noted in a sprite's 'amount' here: a figure (else a thing)
            std::size_t noted = 0;
            std::size_t next = 0; // the next figure
            while (noted < figures.covers.size() || next < figures.figures.size()) {
                if (next < figures.figures.size() && (figures.figures[next].coversFrom <= noted || noted >= figures.covers.size())) {
                    const FigureEcho::Figure& figure = figures.figures[next++];
                    for (std::size_t i = figure.first; i < figure.first + figure.count && i < figures.sprites.size(); ++i) {
                        const gfx::SpriteEcho& drawn = figures.sprites[i];
                        sprites.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, figure.feet, kFigure});
                        cover(drawn);
                    }
                    coversFrom.resize(sprites.size(), covers.size());
                    continue;
                }
                if (const auto thing = things.find(noted); thing != things.end()) {
                    const auto& [end, feet] = thing->second;
                    for (; noted < end; ++noted) {
                        const gfx::SpriteEcho& drawn = figures.covers[noted];
                        sprites.push_back({drawn.texture, drawn.from, drawn.to, drawn.flipX, drawn.flipY, static_cast<float>(drawn.alpha) / 255.0f, feet, 0});
                        cover(drawn);
                    }
                    coversFrom.resize(sprites.size(), covers.size());
                    continue;
                }
                cover(figures.covers[noted++]);
            }
            m_lightMap.shadeSprites(canvas.device(), {window.width, window.height}, sprites, [&](const gfx::LightMap::Sprite& sprite, float y) {
                const float over = std::max(0.0f, static_cast<float>(sprite.feet.y) - y);
                float shaded = 0;
                if (!m_castParts.empty()) {
                    float leanX = 0;
                    float leanY = 0;
                    leanAt(sprite.feet, leanX, leanY);
                    shaded = castShadeAt(sprite.feet, over, leanX, leanY);
                }
                if (painted && sprite.amount >= kFigure) {
                    const core::Point ground{sprite.feet.x + static_cast<int>(over * m_look.shadowX * tall), sprite.feet.y + static_cast<int>(over * m_look.shadowY * tall)};
                    const float inPaint = paintedShadeAt({ground.x - frame.paintedAt.x, ground.y - frame.paintedAt.y});
                    const float edge = std::clamp((inPaint - 0.3f) / 0.4f, 0.0f, 1.0f); // a little shade near by does not count
                    shaded = std::max(shaded, edge * edge * (3 - 2 * edge) * (1 - m_lightMap.shadeAt(frame, ground))); // under a cloud it is dark already
                }
                return dark * shaded;
            }, covers, coversFrom);
        }
        // kept for the shadows of the next frame
        m_lastValid = figures.active && m_accelerated;
        if (m_lastValid) {
            m_lastFigures.sprites = figures.sprites;
            m_lastFigures.covers = figures.covers;
            m_lastFigures.things = figures.things;
            m_lastFigures.casters = figures.casters;
            m_lastFigures.figures = figures.figures;
            m_lastMapAt = camera.toScreen({0, 0}) + origin;
            gfx::freedTextures().clear(); // from here on the list tells which of these figures' textures are gone
            m_lastScene = map.id();
        }
        m_lightMap.apply(canvas.device(), {window.width, window.height}, frame);
        paintAir(canvas, camera, nowMs);
    }

}
