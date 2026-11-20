#ifndef NORTH_ISLAND_GFX_SPRITE_SHAPES_H
#define NORTH_ISLAND_GFX_SPRITE_SHAPES_H

#include "formats/Image.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace gfx {

    /// @brief Which pixels of a texture's picture are seen: a bit a pixel, row by row.
    struct SpriteMask {
        int width = 0;
        int height = 0;
        int words = 0; // 64-bit words a row
        const std::uint64_t* bits = nullptr;

        /// @brief Whether a pixel is seen (false outside the picture).
        bool seen(int x, int y) const {
            return x >= 0 && y >= 0 && x < width && y < height && ((bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(words) + static_cast<std::size_t>(x / 64)] >> (x % 64)) & 1) != 0;
        }
    };

    /// @brief Keeps which pixels of a texture's picture are seen (a bit a pixel), so the lighting can tell a sprite's shape.
    /// @param texture The texture made from the picture.
    /// @param image The picture, as it was uploaded.
    void noteSpriteShape(SDL_Texture* texture, const formats::Image& image);

    /// @brief Forgets what was kept of a texture (it is destroyed).
    /// @param texture The texture.
    void forgetSpriteShape(SDL_Texture* texture);

    /// @brief Which pixels of a texture are seen.
    /// @param texture The texture.
    /// @param mask Set to them; good until the texture is destroyed or another is kept.
    /// @return False when nothing was kept of the texture.
    bool spriteMask(const SDL_Texture* texture, SpriteMask& mask);

    /// @brief The part of a texture's rectangle whose pixels are seen.
    /// @param texture The texture.
    /// @param from The rectangle, in the texture's pixels.
    /// @param seen Set to the part seen, in the texture's pixels.
    /// @return False when nothing was kept of the texture or nothing of the rectangle is seen.
    bool spriteSeenPart(const SDL_Texture* texture, const SDL_FRect& from, SDL_FRect& seen);

    /// @brief The lowest seen row of each column of a texture's rectangle (where a thing lying on the ground touches it).
    /// @param texture The texture.
    /// @param from The rectangle, in the texture's pixels.
    /// @param strip How many pixels wide a column is (always the same: what was worked out is kept by rectangle alone).
    /// @param bottoms Set to a row per column (in the texture's pixels), -1 for a column with nothing seen.
    /// @param tops Set to the highest seen row of each column, -1 for a column with nothing seen.
    /// @return False when nothing was kept of the texture.
    bool spriteColumnBottoms(const SDL_Texture* texture, const SDL_FRect& from, int strip, std::vector<float>& bottoms, std::vector<float>& tops);

}

#endif
