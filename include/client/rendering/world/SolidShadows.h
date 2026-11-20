#ifndef NORTH_ISLAND_CLIENT_RENDERING_WORLD_SOLID_SHADOWS_H
#define NORTH_ISLAND_CLIENT_RENDERING_WORLD_SOLID_SHADOWS_H

#include "core/Point.h"
#include "gfx/LightMap.h"

#include <SDL3/SDL.h>

#include <vector>

namespace client {

    /// @brief Where the light that casts a figure's shadow comes from.
    struct SolidLight {
        float flat = 0.6f; // the ground is seen at a slant: a way up the screen is this much of the same way across
        float lampShare = 0; // how much of the shadow a light near by casts, 0 (the sun alone) to 1
        float lampX = 0; // that light, on the ground: the screen's x, and its y divided by 'flat'
        float lampY = 0;
        float lampHigh = 100; // how high it hangs, px
        float sunX = 0; // where the sun's shadow of a point a pixel high falls, from under it, on the screen
        float sunY = 0;
    };

    /// @brief Casts the shadow of a figure as if it were a solid thing: its sprites, as they were drawn, are put together into one
    /// shape, which is given a height over the ground (by where each column of it touches the ground) and a depth (as deep as it
    /// is thick there), and every part of that is cast onto the ground from the light.
    /// @param parts The figure's sprites, as drawn.
    /// @param feet Where the figure stands.
    /// @param light The light.
    /// @param amount How dark the shadow is, 0 to 1.
    /// @param triangles The shadow is added as triangles (three vertices each), their colour 1 - amount.
    void castSolidShadow(const std::vector<gfx::LightMap::Sprite>& parts, core::Point feet, const SolidLight& light, float amount, std::vector<SDL_Vertex>& triangles);

}

#endif
