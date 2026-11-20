#ifndef NORTH_ISLAND_GFX_SDL_DELETER_H
#define NORTH_ISLAND_GFX_SDL_DELETER_H

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace gfx {

    /// @brief How many textures were destroyed so far (what was kept of a texture from an earlier frame is good while this stays).
    /// @return The count.
    inline unsigned& texturesFreed() {
        static unsigned count = 0;
        return count;
    }

    /// @brief The textures destroyed since the list was last emptied: what was kept of one of them is good no more.
    struct FreedTextures {
        static constexpr std::size_t kMost = 1024; // more than this many are not listed: all count as destroyed then
        std::vector<SDL_Texture*> list;
        bool all = false;

        /// @brief Forgets them.
        void clear() {
            list.clear();
            all = false;
        }

        /// @brief Whether a texture was destroyed.
        /// @param texture The texture.
        /// @return True when it was (or too many were to tell).
        bool has(const SDL_Texture* texture) const { return all || std::find(list.begin(), list.end(), texture) != list.end(); }
    };

    /// @brief The list of the destroyed textures.
    /// @return The list.
    inline FreedTextures& freedTextures() {
        static FreedTextures freed;
        return freed;
    }

    void forgetSpriteShape(SDL_Texture* texture); // gfx/SpriteShapes.h

    /// @brief Deleter that releases SDL objects so they can live in std::unique_ptr.
    struct SdlDeleter {
        /// @brief Destroys a window.
        /// @param window The window.
        void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }

        /// @brief Destroys a renderer.
        /// @param renderer The renderer.
        void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }

        /// @brief Destroys a texture.
        /// @param texture The texture.
        void operator()(SDL_Texture* texture) const {
            ++texturesFreed();
            if (FreedTextures& freed = freedTextures(); freed.list.size() < FreedTextures::kMost) {
                freed.list.push_back(texture);
            } else {
                freed.all = true;
            }
            forgetSpriteShape(texture);
            SDL_DestroyTexture(texture);
        }

        /// @brief Destroys a surface.
        /// @param surface The surface.
        void operator()(SDL_Surface* surface) const { SDL_DestroySurface(surface); }

        /// @brief Destroys a cursor.
        /// @param cursor The cursor.
        void operator()(SDL_Cursor* cursor) const { SDL_DestroyCursor(cursor); }
    };

    template <typename T>
    using SdlHandle = std::unique_ptr<T, SdlDeleter>;

}

#endif
