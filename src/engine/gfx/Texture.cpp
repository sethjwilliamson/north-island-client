#include "gfx/Texture.h"

#include "core/Log.h"
#include "core/Speedups.h"
#include "gfx/RenderDevice.h"
#include "gfx/SpriteShapes.h"

namespace gfx {

    namespace {

        /// @brief Builds an SDL float rectangle from integers.
        /// @param x Left edge.
        /// @param y Top edge.
        /// @param width Width.
        /// @param height Height.
        /// @return The rectangle.
        SDL_FRect toFloat(int x, int y, int width, int height) {
            return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)};
        }

    }

    static_assert(sizeof(core::Color) == 4, "core::Color must be four bytes (r, g, b, a) to upload it");

    std::optional<Texture> Texture::create(const RenderDevice& device, const formats::Image& image, int detail) {
        if (image.isEmpty() || detail < 1 || image.width() < detail || image.height() < detail) {
            return std::nullopt;
        }
        // static (run-length encoded) textures with the software renderer, see loose()
        const bool encoded = !device.isAccelerated() && core::speedups().softwareEncodedTextures && detail == 1;
        const SDL_TextureAccess access = device.isAccelerated() || encoded ? SDL_TEXTUREACCESS_STATIC : SDL_TEXTUREACCESS_STREAMING;
        Texture texture;
        texture.m_texture.reset(SDL_CreateTexture(device.renderer(), SDL_PIXELFORMAT_RGBA32, access, image.width(), image.height()));
        if (!texture.m_texture) {
            core::logError("cannot create a ", image.width(), 'x', image.height(), " texture: ", SDL_GetError());
            return std::nullopt;
        }
        SDL_UpdateTexture(texture.m_texture.get(), nullptr, image.pixels().data(), image.width() * 4); // pitch: 4 bytes a pixel
        if (device.isAccelerated()) { // the lighting casts shadows by the sprites' shapes
            noteSpriteShape(texture.m_texture.get(), image);
        }
        SDL_SetTextureBlendMode(texture.m_texture.get(), SDL_BLENDMODE_BLEND);
        if (detail > 1) {
            SDL_SetTextureScaleMode(texture.m_texture.get(), SDL_SCALEMODE_LINEAR);
        } else if (device.pixelScale() == 1 || !device.scalesPixelArtQuickly() || !SDL_SetTextureScaleMode(texture.m_texture.get(), SDL_SCALEMODE_PIXELART)) {
            SDL_SetTextureScaleMode(texture.m_texture.get(), SDL_SCALEMODE_NEAREST);
        }
        texture.m_width = image.width() / detail;
        texture.m_height = image.height() / detail;
        texture.m_detail = detail;
        texture.m_encoded = encoded;
        return texture;
    }

    std::optional<Texture> Texture::createOpaque(const RenderDevice& device, int width, int height) {
        if (width <= 0 || height <= 0) {
            return std::nullopt;
        }
        Texture texture;
        // Software renderer: the screen's own pixel format, so a draw is a copy of rows (an RGBA texture would be converted
        // pixel by pixel on every draw). SDL names a format by its 32-bit value: XRGB8888 is the bytes b, g, r, x.
        SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32;
        if (!device.isAccelerated()) {
            const SDL_PixelFormat screen = SDL_GetWindowPixelFormat(device.window());
            if (screen == SDL_PIXELFORMAT_XRGB8888 || screen == SDL_PIXELFORMAT_ARGB8888) {
                format = screen;
                texture.m_swapRedBlue = true;
            }
        }
        const SDL_TextureAccess access = device.isAccelerated() ? SDL_TEXTUREACCESS_STATIC : SDL_TEXTUREACCESS_STREAMING;
        texture.m_texture.reset(SDL_CreateTexture(device.renderer(), format, access, width, height));
        if (!texture.m_texture) {
            core::logError("cannot create a ", width, 'x', height, " texture: ", SDL_GetError());
            return std::nullopt;
        }
        SDL_SetTextureScaleMode(texture.m_texture.get(), SDL_SCALEMODE_NEAREST);
        if (device.pixelScale() != 1 && device.scalesPixelArtQuickly()) {
            SDL_SetTextureScaleMode(texture.m_texture.get(), SDL_SCALEMODE_PIXELART);
        }
        texture.m_width = width;
        texture.m_height = height;
        texture.m_opaque = true;
        texture.applyBlend();
        return texture;
    }

    void Texture::setRows(int top, int rows, core::Color* pixels) const {
        if (!m_texture || top < 0 || rows <= 0 || top + rows > m_height) {
            return;
        }
        if (m_swapRedBlue) {
            auto* words = reinterpret_cast<std::uint32_t*>(pixels);
            const std::size_t count = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(rows);
            for (std::size_t i = 0; i < count; ++i) {
                const std::uint32_t pixel = words[i]; // bytes r, g, b, a: red in the low byte
                words[i] = (pixel & 0xFF00FF00u) | ((pixel >> 16) & 0xFFu) | ((pixel & 0xFFu) << 16);
            }
        }
        const SDL_Rect area{0, top, m_width, rows};
        SDL_UpdateTexture(m_texture.get(), &area, pixels, m_width * 4); // pitch: 4 bytes a pixel
    }

    void Texture::applyBlend() const {
        // an opaque texture drawn as it is covers what lies below either way; without blending the renderer only copies
        const bool copy = m_opaque && m_alphaMod == 255;
        SDL_SetTextureBlendMode(m_texture.get(), m_additive ? SDL_BLENDMODE_ADD : copy ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    }

    void Texture::draw(const RenderDevice& device, core::Point destination, const std::optional<core::Rect>& source) const {
        const core::Rect area = source ? source->intersected({0, 0, m_width, m_height}) : core::Rect{0, 0, m_width, m_height};
        if (area.isEmpty()) {
            return;
        }
        const int shiftX = source ? area.x - source->x : 0;
        const int shiftY = source ? area.y - source->y : 0;
        drawScaled(device, {destination.x + shiftX, destination.y + shiftY, area.width, area.height}, area);
    }

    void Texture::drawQuads(const RenderDevice& device, const std::vector<Quad>& quads) const {
        if (!m_texture || quads.empty()) {
            return;
        }
        float textureWidth = 0.0f;
        float textureHeight = 0.0f;
        SDL_GetTextureSize(m_texture.get(), &textureWidth, &textureHeight);
        thread_local std::vector<SDL_Vertex> vertices;
        thread_local std::vector<int> indices;
        vertices.clear();
        indices.clear();
        for (const Quad& quad : quads) {
            // as draw: clip the source to the texture and move the destination with it
            const core::Rect area = quad.source.intersected({0, 0, m_width, m_height});
            if (area.isEmpty()) {
                continue;
            }
            const float minX = static_cast<float>(quad.destination.x + area.x - quad.source.x);
            const float minY = static_cast<float>(quad.destination.y + area.y - quad.source.y);
            const float maxX = minX + static_cast<float>(area.width);
            const float maxY = minY + static_cast<float>(area.height);
            // as SDL_RenderTexture: texture coordinates are source pixels over the texture size
            const float fromX = static_cast<float>(area.x * m_detail);
            const float fromY = static_cast<float>(area.y * m_detail);
            const float minU = fromX / textureWidth;
            const float minV = fromY / textureHeight;
            const float maxU = (fromX + static_cast<float>(area.width * m_detail)) / textureWidth;
            const float maxV = (fromY + static_cast<float>(area.height * m_detail)) / textureHeight;
            // as SDL_SetTextureColorMod / SDL_SetTextureAlphaMod: each channel over 255
            const SDL_FColor color{
                static_cast<float>(quad.color.r) / 255.0f,
                static_cast<float>(quad.color.g) / 255.0f,
                static_cast<float>(quad.color.b) / 255.0f,
                static_cast<float>(quad.color.a) / 255.0f
            };
            if (auto* echo = device.spriteEcho(); echo != nullptr && !m_additive) {
                echo->push_back({loose(device), {fromX, fromY, static_cast<float>(area.width * m_detail), static_cast<float>(area.height * m_detail)}, {minX, minY, maxX - minX, maxY - minY}, false, false, quad.color.a});
            }
            const int first = static_cast<int>(vertices.size());
            vertices.push_back({{minX, minY}, color, {minU, minV}});
            vertices.push_back({{maxX, minY}, color, {maxU, minV}});
            vertices.push_back({{maxX, maxY}, color, {maxU, maxV}});
            vertices.push_back({{minX, maxY}, color, {minU, maxV}});
            for (const int corner : {0, 1, 2, 0, 2, 3}) { // SDL's own corner order for a rectangle
                indices.push_back(first + corner);
            }
        }
        if (!vertices.empty()) {
            SDL_RenderGeometry(device.renderer(), loose(device), vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
        }
    }

    void Texture::drawFlipped(
        const RenderDevice& device,
        core::Point destination,
        bool leftRight,
        bool topBottom,
        const std::optional<core::Rect>& source
    ) const {
        const core::Rect area = source ? source->intersected({0, 0, m_width, m_height}) : core::Rect{0, 0, m_width, m_height};
        if (!m_texture || area.isEmpty()) {
            return;
        }
        const SDL_FRect from = toFloat(area.x * m_detail, area.y * m_detail, area.width * m_detail, area.height * m_detail);
        const SDL_FRect to = toFloat(destination.x, destination.y, area.width, area.height);
        const auto flip = static_cast<SDL_FlipMode>((leftRight ? SDL_FLIP_HORIZONTAL : 0) | (topBottom ? SDL_FLIP_VERTICAL : 0));
        if (auto* echo = device.spriteEcho(); echo != nullptr && !m_additive) {
            echo->push_back({loose(device), from, to, leftRight, topBottom, 255});
        }
        SDL_RenderTextureRotated(device.renderer(), loose(device), &from, &to, 0.0, nullptr, flip);
    }

    void Texture::drawScaled(const RenderDevice& device, const core::Rect& destination, const std::optional<core::Rect>& source) const {
        if (!m_texture || destination.isEmpty()) {
            return;
        }
        const SDL_FRect to = toFloat(destination.x, destination.y, destination.width, destination.height);
        const bool unscaled = m_detail == 1 && (source ? source->width == destination.width && source->height == destination.height
                                                       : destination.width == m_width && destination.height == m_height);
        const bool plain = unscaled && !m_additive && m_alphaMod == 255 && m_colorMod.r == 255 && m_colorMod.g == 255 && m_colorMod.b == 255;
        // the encoded main texture takes the plain draws only; it never gets a colour, alpha or blend change
        SDL_Texture* texture = m_encoded && plain ? m_texture.get() : loose(device);
        if (auto* echo = device.spriteEcho(); echo != nullptr && !m_additive) {
            const SDL_FRect all = source ? toFloat(source->x * m_detail, source->y * m_detail, source->width * m_detail, source->height * m_detail)
                                         : toFloat(0, 0, m_width * m_detail, m_height * m_detail);
            echo->push_back({texture, all, to, false, false, 255});
        }
        if (source) {
            const SDL_FRect from = toFloat(source->x * m_detail, source->y * m_detail, source->width * m_detail, source->height * m_detail);
            SDL_RenderTexture(device.renderer(), texture, &from, &to);
        } else {
            SDL_RenderTexture(device.renderer(), texture, nullptr, &to);
        }
    }

    SDL_Texture* Texture::loose(const RenderDevice& device) const {
        if (!m_encoded) {
            return m_texture.get(); // the settings are put on it as they are set
        }
        if (!m_loose && !m_looseFailed) {
            // copied out of the main texture through a render target: SDL gives no way to read a static texture's pixels
            SDL_Renderer* renderer = device.renderer();
            float width = 0.0f;
            float height = 0.0f;
            SDL_GetTextureSize(m_texture.get(), &width, &height);
            m_loose.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, static_cast<int>(width), static_cast<int>(height)));
            SDL_Texture* target = SDL_GetRenderTarget(renderer);
            if (m_loose && SDL_SetRenderTarget(renderer, m_loose.get())) {
                SDL_SetTextureBlendMode(m_texture.get(), SDL_BLENDMODE_NONE); // every channel as it is, alpha too
                SDL_RenderTexture(renderer, m_texture.get(), nullptr, nullptr);
                SDL_SetRenderTarget(renderer, target);
                SDL_SetTextureBlendMode(m_texture.get(), SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(m_loose.get(), SDL_SCALEMODE_NEAREST);
                m_looseStale = true;
            } else {
                m_loose.reset();
                m_looseFailed = true;
            }
        }
        if (!m_loose) {
            return m_texture.get();
        }
        if (m_looseStale) {
            SDL_SetTextureColorMod(m_loose.get(), m_colorMod.r, m_colorMod.g, m_colorMod.b);
            SDL_SetTextureAlphaMod(m_loose.get(), m_alphaMod);
            SDL_SetTextureBlendMode(m_loose.get(), m_additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
            m_looseStale = false;
        }
        return m_loose.get();
    }

    void Texture::setColorMod(core::Color color) const {
        m_colorMod = color;
        m_looseStale = true;
        if (!m_encoded) {
            SDL_SetTextureColorMod(m_texture.get(), color.r, color.g, color.b);
        }
    }

    void Texture::setAlphaMod(std::uint8_t alpha) const {
        m_alphaMod = alpha;
        m_looseStale = true;
        if (!m_encoded) {
            SDL_SetTextureAlphaMod(m_texture.get(), alpha);
            if (m_opaque) {
                applyBlend();
            }
        }
    }

    void Texture::setAdditive(bool additive) const {
        m_additive = additive;
        m_looseStale = true;
        if (!m_encoded) {
            applyBlend();
        }
    }

}