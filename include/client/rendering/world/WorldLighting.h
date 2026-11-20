#ifndef NORTH_ISLAND_CLIENT_RENDERING_WORLD_WORLD_LIGHTING_H
#define NORTH_ISLAND_CLIENT_RENDERING_WORLD_WORLD_LIGHTING_H

#include "client/models/world/Camera.h"
#include "client/models/world/MapCharacter.h"
#include "client/models/world/MapNpc.h"
#include "client/models/world/MapPet.h"
#include "client/models/world/WorldMap.h"
#include "client/rendering/characters/FigureShadow.h"
#include "client/services/settings/ExtraSettings.h"
#include "gfx/Canvas.h"
#include "gfx/LightMap.h"
#include "gfx/PictureLibrary.h"
#include "gfx/RenderDevice.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace client {

    /// @brief The experimental lighting of the world map: an ambient colour, drifting shade (clouds, leaves), lights at the
    /// players, the doors and the glowing objects, and darker corners, by the kind of scene and the map.
    /// @details The looks are in lighting.json in the user folder, written with the built-in ones when it is not there and read
    /// again whenever it changes: "enabled", "types" (a look per scene type, "default" under them all) and "maps" (what differs
    /// for a map id).
    class WorldLighting {
        public:
            /// @brief Sets the file the looks are read from.
            /// @param file Path of lighting.json.
            explicit WorldLighting(std::filesystem::path file) : m_file(std::move(file)) {}

            /// @brief Draws what lies on the ground under the standing things: the shadows they cast and the glints on water.
            /// @param canvas The canvas the ground was drawn on.
            /// @param pictures Picture library (the size of an object).
            /// @param map The map.
            /// @param camera Camera of the view.
            /// @param sceneType The scene's type.
            /// @param players The players on the map, the own one first.
            /// @param npcs The NPCs on the map.
            /// @param pets The pets on the map.
            /// @param nowMs Current time in milliseconds.
            void paintGround(
                gfx::Canvas& canvas,
                gfx::PictureLibrary& pictures,
                WorldMap& map,
                const Camera& camera,
                int sceneType,
                const std::vector<std::reference_wrapper<MapCharacter>>& players,
                const std::vector<MapNpc>& npcs,
                const std::vector<MapPet>& pets,
                std::uint64_t nowMs
            );

            /// @brief Lights what has been drawn of the map so far.
            /// @param canvas The canvas the map was drawn on.
            /// @param pictures Picture library (the size of a glowing object).
            /// @param map The map.
            /// @param camera Camera of the view.
            /// @param sceneType The scene's type.
            /// @param players The players on the map, the own one first.
            /// @param npcs The NPCs on the map (those that burn, as the look's 'npcLights' says, give light).
            /// @param nowMs Current time in milliseconds.
            void apply(
                gfx::Canvas& canvas,
                gfx::PictureLibrary& pictures,
                WorldMap& map,
                const Camera& camera,
                int sceneType,
                const std::vector<std::reference_wrapper<MapCharacter>>& players,
                const std::vector<MapNpc>& npcs,
                std::uint64_t nowMs
            );

            /// @brief Takes the player's switches (the Video tab's Lighting group): what is off is left out of every look.
            /// @param options The switches.
            void setOptions(const ExtraSettings::Lighting& options) {
                if (!(options == m_options)) {
                    m_options = options;
                    m_optionsChanged = true;
                }
            }

            /// @brief Forgets the figures noted in the frame before: what is drawn next is another scene (a movie, or the map
            /// after it).
            void forgetFigures() { m_lastValid = false; }

            /// @brief Whether the map's look casts shadows of its own under the figures (their dithered ones are then left out).
            /// @return True when it does.
            bool castsShadows() const { return m_enabled && m_accelerated && m_look.shadows > 0; }

            /// @brief Whether anything is lit at all.
            /// @return True when the lighting is on and the renderer can do it.
            bool isOn() const { return m_enabled && m_accelerated; }

            /// @brief Casts the fighters' shadows on a battle's ground, before they are drawn.
            /// @param canvas The canvas.
            /// @param nowMs Current time in milliseconds.
            void battleGround(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Lights a battle, after its fighters are drawn.
            /// @param canvas The canvas.
            /// @param nowMs Current time in milliseconds.
            void battleApply(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Whether the figures drawn on the map are to be noted (FigureEcho) for their shadows and their shade.
            /// @return True when they are.
            bool notesFigures() const { return m_enabled && m_accelerated && ((m_look.shadows > 0 && m_look.shapedShadows) || m_look.figureShade > 0); }

        private:
            /// @brief A kind of light source in a look.
            struct Source {
                gfx::LightMap::Tone tone;
                int radius = 0;
                float strength = 0;
                float flicker = -1; // how far its size wavers, 0 to 1; under 0: as the look's 'flicker' says
            };

            /// @brief How a map is lit.
            struct Look {
                gfx::LightMap::Tone ambient;
                float corners = 0;
                float effectLights = 0; // how much the battle effects that shine (fire, lightning) light the field, 0 for not at all
                float occlusion = 0; // how dark the ground gets right where a figure or an object stands on it, 0 to 1
                gfx::LightMap::Tone shade;
                float shadeAmount = 0;
                float shadeScale = 6;
                float shadeSpeedX = 0; // map pixels a second
                float shadeSpeedY = 0;
                Source player;
                Source others;
                Source door;
                Source object;
                float flicker = 0; // how far a light's size wavers, 0 to 1
                float glow = 0;
                float vivid = 0; // how much of the glow brightens the picture's own colours instead of adding the light's, 0 to 1
                std::map<int, gfx::LightMap::Tone> npcLights; // the NPCs that burn (by Npc.dat id): lit as the objects, in their colour
                float shadows = 0; // how much of the shadow a figure casts shows, 0 to 1
                float shadowLength = 80; // a figure's, px
                float shadowWidth = 22; // a figure's, px
                float shadowX = 0.91f; // where shadows fall when no light is near, on the screen
                float shadowY = -0.41f;
                gfx::LightMap::Tone shadowTone{0.52f, 0.545f, 0.59f}; // the light left in a full shadow
                float objectShadows = 0; // the same for the map's objects
                bool shapedShadows = true; // a figure's shadow has its shape (else it is an ellipse)
                float figureShade = 1; // how much of the painted shade a figure standing in it takes on, 0 to 1
                bool paintedShade = true; // shade and shadows are left out where the background is painted in shade
                float sparkle = 0; // glints on water, 0 to 1
                float motes = 0; // how many specks drift in the air
                float moteStrength = 0;
                gfx::LightMap::Tone moteTone;
                float shafts = 0; // shafts of light, 0 to 1
                gfx::LightMap::Tone shaftTone;
                float shaftAngle = -1; // degrees the shafts lean from upright, away from the light; under 0: as the shadows' direction and length say
                float shadowStart = 4; // a shadow starts this far behind the feet, px
                float shadowLamps = 0; // how much the lights near by turn the shadows away from them, 0 to 1 (the own player's always does)
                float shadowSun = 1; // how much of a shadow falls along the look's direction away from every light, 0 (inside) to 1
                float bakedLights = 0; // how strong the lights painted into the background shine, 0 for none
                float bakedRadius = 1; // their size, times what was found
                float bakedRays = 0; // how strong the rays from painted windows are, 0 for none
                float rayLength = 150; // px
            };

            /// @brief Works out where a map's background is painted in shade and hands it to the light map.
            /// @param device The device.
            /// @param pictures Picture library.
            /// @param map The map.
            void findPaintedShade(const gfx::RenderDevice& device, gfx::PictureLibrary& pictures, const WorldMap& map);

            /// @brief The drifting shade of the frame at a time, as the light map takes it.
            /// @param frame Receives the shade's fields.
            /// @param canvas The canvas.
            /// @param camera Camera of the view.
            /// @param nowMs Current time in milliseconds.
            void setShade(gfx::LightMap::Frame& frame, const gfx::Canvas& canvas, const Camera& camera, std::uint64_t nowMs) const;

            /// @brief Draws the specks and the shafts of light over the lit map.
            /// @param canvas The canvas.
            /// @param camera Camera of the view.
            /// @param nowMs Current time in milliseconds.
            void paintAir(gfx::Canvas& canvas, const Camera& camera, std::uint64_t nowMs);

            /// @brief Reads the file again when it changed (looked at twice a second) and works out the look of the map.
            /// @param mapId The map id.
            /// @param sceneType The scene's type.
            /// @param nowMs Current time in milliseconds.
            void refresh(int mapId, const std::string& kind, std::uint64_t nowMs);

            /// @brief The key of a map's look among the 'types': its scene type, but "room" for a painted map of the dungeon type (the
            /// lobbies, halls and jails that are no caves).
            /// @param map The map.
            /// @param sceneType Its scene type.
            /// @return The key.
            static std::string kindOf(const WorldMap& map, int sceneType);

            /// @brief A light painted into a map's background.
            struct Baked {
                core::Point at; // on the map
                int radius = 0;
                gfx::LightMap::Tone tone;
                float strength = 1;
                float casts = 1; // how much of a shadow falls away from it, 0 (none: no light is seen there) to 1
                bool wavers = false; // a fire or a lamp
                bool window = false; // daylight comes in: a ray falls from it
                float rayX = 0; // where the ray falls
                float rayY = 1;
            };

            /// @brief Looks for the lights painted into a map's background.
            /// @param pictures Picture library.
            /// @param map The map.
            void findBakedLights(gfx::PictureLibrary& pictures, const WorldMap& map);

            std::filesystem::path m_file;
            std::string m_text; // the looks as read (the built-in ones when the file cannot be read)
            std::filesystem::file_time_type m_fileTime{};
            std::uint64_t m_checkedMs = 0;
            bool m_read = false;
            bool m_enabled = false;
            int m_paintedMap = -1; // the map whose painted shade the light map holds (-1: none looked for yet)
            bool m_paintedWanted = false;
            std::vector<std::uint8_t> m_painted; // the painted shade of the map, a byte a cell (empty: none)
            int m_paintedWidth = 0;
            int m_paintedHeight = 0;
            int m_paintedCell = 1;
            /// @brief A part of a shadow cast in this frame by a figure or a thing, kept so that what stands in it is darkened too.
            struct CastPart {
                gfx::LightMap::Sprite sprite; // as it was laid on the ground
                core::Point owner; // the feet of what cast it: it is not darkened by its own shadow
                SDL_FRect bounds{}; // where on the ground it lies
            };

            /// @brief Where the shadow of a point a pixel high falls, for a figure or a thing standing at a point of the screen.
            struct Lean {
                core::Point feet;
                float x = 0;
                float y = 0;
            };

            /// @brief How much of the shadows cast by the other figures and things falls on a point of what stands at a place.
            /// @param feet Where it stands, on the screen.
            /// @param high How high the point is over the feet, px.
            /// @param leanX Where the shadow of a point a pixel high falls there, from under it.
            /// @param leanY The same, up and down.
            /// @return 0 (none) to 1.
            float castShadeAt(core::Point feet, float high, float leanX, float leanY) const;

            /// @brief Where the shadow of a point a pixel high falls for what stands at a point of the screen in this frame.
            /// @param feet Where it stands.
            /// @param x Set to it, across.
            /// @param y Set to it, up and down.
            void leanAt(core::Point feet, float& x, float& y) const;

            std::vector<CastPart> m_castParts; // the shadows cast in this frame
            std::vector<Lean> m_leans; // how the shadows fall for each figure and thing of this frame
            FigureEcho m_lastFigures; // the figures of the frame before: their shadows are cast before they are drawn again
            core::Point m_lastMapAt; // where the map's top left was then
            bool m_lastValid = false;

            /// @brief How much a point of the map lies in the shade painted into the background.
            /// @param point The point, in the background's pixels.
            /// @return 0 (lit) to 1.
            float paintedShadeAt(core::Point point) const;
            bool m_accelerated = false; // the renderer the map was last drawn with is an accelerated one
            int m_lookMap = -1;
            std::string m_lookKind;
            std::vector<Baked> m_baked; // the lights painted into the map
            int m_bakedMap = -1; // the map they are of (-1: none looked for yet)
            bool m_bakedWanted = false;
            int m_lastScene = -1; // the map the figures of the frame before stood on (-2: the battle)
            Look m_look;
            ExtraSettings::Lighting m_options; // the player's switches
            bool m_optionsChanged = false; // the look is to be worked out anew for them
            gfx::LightMap m_lightMap;
    };

}

#endif
