#ifndef NORTH_ISLAND_GFX_LIGHT_MAP_H
#define NORTH_ISLAND_GFX_LIGHT_MAP_H

#include "core/Point.h"
#include "core/Size.h"
#include "gfx/SdlDeleter.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace gfx {

    class RenderDevice;

    /// @brief Lights a drawn picture: a coarse map of light (an ambient colour, drifting shade, round lights, darker corners) is
    /// multiplied over what has been drawn so far.
    /// @details Experimental. A light value of 1 leaves the picture as it is, 2 doubles it, 0 is black. Needs an accelerated
    /// renderer; where the renderer cannot double (no custom blend mode) values above 1 show as 1.
    class LightMap {
        public:
            /// @brief A colour of light, each part from 0 (none) to 2 (twice as bright as the picture).
            struct Tone {
                float r = 1;
                float g = 1;
                float b = 1;
            };

            /// @brief A round light that fades to nothing at its radius.
            struct Light {
                core::Point at; // its middle, in the area's pixels
                int radius = 0;
                Tone tone; // what its middle adds to the light there
                bool glows = true; // also added onto the picture itself by the frame's glow
            };

            /// @brief What one frame is lit by.
            struct Frame {
                Tone ambient;
                Tone shade; // the light where the drifting shade is full
                float shadeAmount = 0; // 0 (no shade) to 1
                float shadeScale = 1; // the area's pixels a pixel of the shade's pattern covers
                core::Point shadeAt; // where the pattern's top left lies, in the area's pixels
                float shadeScale2 = 0; // the same for a second layer of the pattern, so that the shade does not repeat in view (0: none)
                core::Point shadeAt2;
                std::vector<Light> lights;
                float glow = 0; // how much of each light is also added onto the picture itself, 0 to 1
                float vivid = 0; // how much of the glow brightens the picture's own colours (instead of adding the light's), 0 to 1
                float corners = 0; // how dark the corners get, 0 to 1
                core::Point paintedAt; // where the painted-shade mask's top left lies, in the area's pixels
            };

            /// @brief A shadow cast on the ground: an ellipse.
            struct Shadow {
                core::Point middle; // in the area's pixels
                int halfLength = 0;
                int halfWidth = 0;
                float degrees = 0; // its length turned clockwise from pointing right
                float amount = 1; // 0 to 1
            };

            /// @brief A part of a texture that was drawn for something standing on the ground.
            struct Sprite {
                SDL_Texture* texture = nullptr;
                SDL_FRect from{}; // in the texture's pixels
                SDL_FRect to{}; // where it stands, in the area's pixels
                bool flipX = false;
                bool flipY = false;
                float alpha = 1;
                core::Point feet; // where what it belongs to stands
                float amount = 1; // of its shadow, 0 to 1
                float leanX = 0; // where the shadow of a point a pixel over the feet falls, from the feet
                float leanY = 0;
                float acrossX = 1; // and that of a point a pixel right of them
                float acrossY = 0;
                float startX = 0; // the shadow starts this far from the feet
                float startY = 0;
            };

            /// @brief Darkens what stands at the top left of what the renderer draws into now, by the shape of its sprites. Sprites
            /// that follow each other with the same feet are one thing, darkened once where they overlap.
            /// @param device The device.
            /// @param area Size of the area in logical pixels.
            /// @param sprites The sprites.
            /// @param amountAt How much darker (0 to 1) a thing gets at a height (a y of the area), by its first sprite.
            /// @param covers What was drawn besides the things, in the order it was drawn: where it stands before a thing, the thing
            /// is not darkened (it is not seen there).
            /// @param coversFrom For every sprite, the first of the covers drawn after it (as many as there are sprites, or none).
            void shadeSprites(
                const RenderDevice& device,
                core::Size area,
                const std::vector<Sprite>& sprites,
                const std::function<float(const Sprite&, float)>& amountAt,
                const std::vector<Sprite>& covers = {},
                const std::vector<std::size_t>& coversFrom = {}
            );

            /// @brief Darkens the ground at the top left of what the renderer draws into now along the outlines of things that
            /// stand on it: each thing's shape, a little wider than the thing, which is drawn over it afterwards.
            /// @param device The device.
            /// @param area Size of the area in logical pixels.
            /// @param sprites The things: 'alpha' is how dark the edge is at the foot (0 to 1), 'amount' how much of that is left
            /// at the top.
            /// @param reach How far the edge lies around a thing, px.
            void edgeShadows(const RenderDevice& device, core::Size area, const std::vector<Sprite>& sprites, float reach);

            /// @brief Sets where the picture under the light is in shade already (shadows painted into a map's background): shade and
            /// cast shadows are left out there, so that they do not add up with it.
            /// @param device The device.
            /// @param width Cells across.
            /// @param height Cells down.
            /// @param shade A byte a cell, row by row: 255 in full painted shade.
            /// @param cell The area's pixels across a cell.
            void setPaintedShade(const RenderDevice& device, int width, int height, const std::vector<std::uint8_t>& shade, int cell);

            /// @brief Forgets the painted shade (a map without any).
            void clearPaintedShade() { m_painted.reset(); }

            /// @brief Darkens the area at the top left of what the renderer draws into now by cast shadows. Shadows that overlap do not
            /// add up, and none falls where the painted shade is.
            /// @param device The device.
            /// @param area Size of the area in logical pixels.
            /// @param shadows The shadows.
            /// @param sprites What casts a shadow of its own shape.
            /// @param tone The light left in a full shadow.
            /// @param paintedAt Where the painted-shade mask's top left lies, in the area's pixels.
            /// @param leavePainted False to cast them over the painted shade too (a picture the mask is not of).
            /// @param solids Shadows already laid on the ground, as triangles (three vertices each) in the area's pixels: their colour
            /// is how lit the ground is under them (0 black); where they overlap the darkest counts.
            void castShadows(
                const RenderDevice& device,
                core::Size area,
                const std::vector<Shadow>& shadows,
                const std::vector<Sprite>& sprites,
                const Tone& tone,
                core::Point paintedAt,
                bool leavePainted = true,
                const std::vector<SDL_Vertex>& solids = {}
            );

            /// @brief How much of a frame's drifting shade lies over a point.
            /// @param frame The frame.
            /// @param point The point, in the area's pixels.
            /// @return 0 (none) to the frame's shade amount.
            float shadeAt(const Frame& frame, core::Point point) const;

            /// @brief Lights the area of this size at the top left of what the renderer draws into now.
            /// @param device The device.
            /// @param area Size of the area in logical pixels.
            /// @param frame The light.
            /// @return False when the renderer cannot do it (nothing drawn).
            bool apply(const RenderDevice& device, core::Size area, const Frame& frame);

            /// @brief Draws a soft ellipse onto what the renderer draws into now: dark (a shadow) or added light (a glint, a beam).
            /// @param device The device.
            /// @param middle Its middle, in what is drawn into.
            /// @param halfLength Half its length, px.
            /// @param halfWidth Half its width, px.
            /// @param degrees How far its length is turned clockwise from pointing right.
            /// @param tone Its colour (added light: 1 adds full white).
            /// @param amount How much of it shows, 0 to 1.
            /// @param adds True for added light, false to cover what is there with the colour.
            void drawSpot(const RenderDevice& device, core::Point middle, int halfLength, int halfWidth, float degrees, const Tone& tone, float amount, bool adds);

        private:
            /// @brief Makes the textures the first time and when the area grew.
            /// @param device The device.
            /// @param area Size of the area in logical pixels.
            /// @return False when a texture could not be made.
            bool prepare(const RenderDevice& device, core::Size area);

            SdlHandle<SDL_Texture> m_map; // the light, a pixel for kCoarse x kCoarse of the area
            SdlHandle<SDL_Texture> m_spot; // a round light: white, alpha falling off to the edge
            SdlHandle<SDL_Texture> m_spotLit; // the same with the fall-off in its colour (alpha whole), to brighten by the picture's own colour
            SdlHandle<SDL_Texture> m_pattern; // the shade: white, alpha the pattern, repeats at its edges
            SdlHandle<SDL_Texture> m_corner; // black, alpha rising to the corners
            /// @brief Draws sprites so that what is under their shape gets darker.
            /// @param renderer The renderer.
            /// @param sprites The first of the sprites.
            /// @param count How many.
            /// @param flat True to draw each flat on the ground, as its shadow, by its amount; false as it stands, black.
            void drawShapes(SDL_Renderer* renderer, const Sprite* sprites, std::size_t count, bool flat, int layer = 0, SDL_FPoint shift = {0, 0}, bool fadesUp = false);

            SdlHandle<SDL_Texture> m_shade; // how lit the things darkened in a frame are, a pixel for a pixel of the area
            int m_shadeWidth = 0;
            int m_shadeHeight = 0;

            SdlHandle<SDL_Texture> m_blob; // a shadow: white, alpha whole inside and falling off near the edge
            SdlHandle<SDL_Texture> m_cast; // the cast shadows of a frame, as fine as the area is shown
            SdlHandle<SDL_Texture> m_painted; // white, alpha the painted shade, a pixel a cell
            int m_paintedCell = 1;
            int m_castWidth = 0;
            int m_castHeight = 0;
            std::vector<std::uint8_t> m_patternAlpha; // the shade pattern's alpha, for shadeAt
            int m_mapWidth = 0;
            int m_mapHeight = 0;
            bool m_doubles = false; // the map is multiplied twice over (128 leaves the picture as it is)
            bool m_failed = false;
    };

}

#endif
