#include "gfx/LightMap.h"

#include "core/Log.h"
#include "gfx/RenderDevice.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gfx {

    namespace {

        constexpr int kCoarse = 4; // the area's pixels across one pixel of the light map
        constexpr int kSpotSize = 96; // width and height of the round light's texture, px
        constexpr int kPatternSize = 512; // width and height of the shade pattern, px
        constexpr int kCornerSize = 64; // width and height of the corner texture, px
        constexpr float kCastFinest = 3; // the most pixels of the cast shadows' layer across one of the area

        float smooth(float low, float high, float value) {
            const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
            return t * t * (3 - 2 * t);
        }

        /// @brief A number from 0 to 1 for a lattice point, the same each run.
        float lattice(int x, int y, int seed) {
            std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u + static_cast<std::uint32_t>(seed) * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
        }

        /// @brief Smooth noise that repeats every period cells.
        float noise(float x, float y, int period, int seed) {
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const float fx = smooth(0, 1, x - static_cast<float>(x0));
            const float fy = smooth(0, 1, y - static_cast<float>(y0));
            const auto at = [&](int cx, int cy) { return lattice(((cx % period) + period) % period, ((cy % period) + period) % period, seed); };
            const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * fx;
            const float bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * fx;
            return top + (bottom - top) * fy;
        }

        SDL_Texture* makeTexture(SDL_Renderer* renderer, int size, const std::vector<std::uint32_t>& pixels, SDL_BlendMode blend) {
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size, size);
            if (texture != nullptr) {
                SDL_UpdateTexture(texture, nullptr, pixels.data(), size * 4);
                SDL_SetTextureBlendMode(texture, blend);
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            }
            return texture;
        }

        std::uint8_t byteOf(float value) { return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); }

    }

    bool LightMap::prepare(const RenderDevice& device, core::Size area) {
        SDL_Renderer* renderer = device.renderer();
        const int width = (area.width + kCoarse - 1) / kCoarse;
        const int height = (area.height + kCoarse - 1) / kCoarse;
        if (!m_map || width > m_mapWidth || height > m_mapHeight) {
            m_mapWidth = std::max(width, m_mapWidth);
            m_mapHeight = std::max(height, m_mapHeight);
            m_map.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, m_mapWidth, m_mapHeight));
            if (!m_map) {
                return false;
            }
            SDL_SetTextureScaleMode(m_map.get(), SDL_SCALEMODE_LINEAR);
            // twice the map times the picture, so that light can brighten as well as darken
            const SDL_BlendMode twice = SDL_ComposeCustomBlendMode(
                SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_SRC_COLOR, SDL_BLENDOPERATION_ADD,
                SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD
            );
            m_doubles = SDL_SetTextureBlendMode(m_map.get(), twice);
            if (!m_doubles) {
                core::logInfo("lighting: this renderer cannot brighten (", SDL_GetError(), "), it only darkens");
                SDL_SetTextureBlendMode(m_map.get(), SDL_BLENDMODE_MOD);
            }
        }
        if (!m_spot) {
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kSpotSize) * kSpotSize);
            for (int y = 0; y < kSpotSize; ++y) {
                for (int x = 0; x < kSpotSize; ++x) {
                    const float dx = (static_cast<float>(x) + 0.5f) / (kSpotSize / 2.0f) - 1;
                    const float dy = (static_cast<float>(y) + 0.5f) / (kSpotSize / 2.0f) - 1;
                    const float inside = std::max(0.0f, 1 - (dx * dx + dy * dy));
                    pixels[static_cast<std::size_t>(y) * kSpotSize + x] = (static_cast<std::uint32_t>(byteOf(inside * inside)) << 24) | 0xFFFFFFu;
                }
            }
            m_spot.reset(makeTexture(renderer, kSpotSize, pixels, SDL_BLENDMODE_ADD));
            // the fall-off in the colour: what is there + what is there x the light, so a light brightens the picture's own colours
            for (std::uint32_t& pixel : pixels) {
                const std::uint32_t inside = pixel >> 24;
                pixel = 0xFF000000u | (inside << 16) | (inside << 8) | inside;
            }
            const SDL_BlendMode brighten = SDL_ComposeCustomBlendMode(
                SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD
            );
            m_spotLit.reset(makeTexture(renderer, kSpotSize, pixels, SDL_BLENDMODE_ADD));
            if (m_spotLit && !SDL_SetTextureBlendMode(m_spotLit.get(), brighten)) {
                m_spotLit.reset(); // the glow is added as the light's colour instead
            }
        }
        if (!m_pattern) {
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kPatternSize) * kPatternSize);
            for (int y = 0; y < kPatternSize; ++y) {
                for (int x = 0; x < kPatternSize; ++x) {
                    float sum = 0;
                    float weight = 0.5f;
                    for (int period = 4; period <= 16; period *= 2) { // three layers, each twice as fine and half as strong: no small shreds
                        sum += weight * noise(static_cast<float>(x) * static_cast<float>(period) / kPatternSize, static_cast<float>(y) * static_cast<float>(period) / kPatternSize, period, period);
                        weight /= 2;
                    }
                    pixels[static_cast<std::size_t>(y) * kPatternSize + x] = (static_cast<std::uint32_t>(byteOf(smooth(0.57f, 0.69f, sum / 0.875f))) << 24) | 0xFFFFFFu;
                }
            }
            m_pattern.reset(makeTexture(renderer, kPatternSize, pixels, SDL_BLENDMODE_BLEND));
            m_patternAlpha.resize(pixels.size());
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                m_patternAlpha[i] = static_cast<std::uint8_t>(pixels[i] >> 24);
            }
        }
        if (!m_blob) {
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kSpotSize) * kSpotSize);
            for (int y = 0; y < kSpotSize; ++y) {
                for (int x = 0; x < kSpotSize; ++x) {
                    const float dx = (static_cast<float>(x) + 0.5f) / (kSpotSize / 2.0f) - 1;
                    const float dy = (static_cast<float>(y) + 0.5f) / (kSpotSize / 2.0f) - 1;
                    pixels[static_cast<std::size_t>(y) * kSpotSize + x] = (static_cast<std::uint32_t>(byteOf(1 - smooth(0.72f, 1.0f, std::sqrt(dx * dx + dy * dy)))) << 24) | 0xFFFFFFu;
                }
            }
            m_blob.reset(makeTexture(renderer, kSpotSize, pixels, SDL_BLENDMODE_BLEND));
        }
        if (!m_corner) {
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kCornerSize) * kCornerSize);
            for (int y = 0; y < kCornerSize; ++y) {
                for (int x = 0; x < kCornerSize; ++x) {
                    const float dx = (static_cast<float>(x) + 0.5f) / (kCornerSize / 2.0f) - 1;
                    const float dy = (static_cast<float>(y) + 0.5f) / (kCornerSize / 2.0f) - 1;
                    pixels[static_cast<std::size_t>(y) * kCornerSize + x] = static_cast<std::uint32_t>(byteOf(smooth(0.45f, 1.35f, std::sqrt(dx * dx + dy * dy)))) << 24;
                }
            }
            m_corner.reset(makeTexture(renderer, kCornerSize, pixels, SDL_BLENDMODE_BLEND));
        }
        return m_spot && m_pattern && m_corner && m_blob;
    }

    void LightMap::setPaintedShade(const RenderDevice& device, int width, int height, const std::vector<std::uint8_t>& shade, int cell) {
        m_painted.reset();
        if (!device.isAccelerated() || width <= 0 || height <= 0 || shade.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            return;
        }
        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = (static_cast<std::uint32_t>(shade[i]) << 24) | 0xFFFFFFu;
        }
        m_painted.reset(SDL_CreateTexture(device.renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height));
        if (m_painted) {
            SDL_UpdateTexture(m_painted.get(), nullptr, pixels.data(), width * 4);
            SDL_SetTextureBlendMode(m_painted.get(), SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(m_painted.get(), SDL_SCALEMODE_LINEAR);
            m_paintedCell = std::max(1, cell);
        }
    }

    float LightMap::shadeAt(const Frame& frame, core::Point point) const {
        if (frame.shadeAmount <= 0 || frame.shadeScale <= 0 || m_patternAlpha.empty()) {
            return 0;
        }
        const auto wrapped = [](float value) {
            const int cell = static_cast<int>(std::floor(value)) % kPatternSize;
            return cell < 0 ? cell + kPatternSize : cell;
        };
        float lit = 1; // what the layers leave, one after the other
        for (int layer = 0; layer < 2; ++layer) {
            const float scale = layer == 0 ? frame.shadeScale : frame.shadeScale2;
            const core::Point at = layer == 0 ? frame.shadeAt : frame.shadeAt2;
            if (scale <= 0) {
                continue;
            }
            const int x = wrapped(static_cast<float>(point.x - at.x) / scale);
            const int y = wrapped(static_cast<float>(point.y - at.y) / scale);
            lit *= 1 - frame.shadeAmount * static_cast<float>(m_patternAlpha[static_cast<std::size_t>(y) * kPatternSize + x]) / 255.0f;
        }
        return 1 - lit;
    }

    void LightMap::drawShapes(SDL_Renderer* renderer, const Sprite* sprites, std::size_t count, bool flat, int layer, SDL_FPoint shift, bool fadesUp) {
        // what is there times (1 - the sprite's alpha): darker by the sprite's shape, whatever its colours
        static const SDL_BlendMode shape = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD
        );
        // the same in the layer's alpha alone (layer 1), and the other way round: the alpha whole by the sprite's shape (layer 2)
        static const SDL_BlendMode hole = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD
        );
        static const SDL_BlendMode patch = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD
        );
        const SDL_BlendMode mode = layer == 1 ? hole : layer == 2 ? patch : shape;
        for (std::size_t i = 0; i < count; ++i) {
            const Sprite& sprite = sprites[i];
            float width = 0;
            float height = 0;
            if (sprite.texture == nullptr || sprite.to.w <= 0 || sprite.to.h <= 0 || !SDL_GetTextureSize(sprite.texture, &width, &height) || width <= 0 || height <= 0) {
                continue;
            }
            const float amount = std::clamp(sprite.alpha * (flat ? sprite.amount : 1.0f), 0.0f, 1.0f);
            if (amount <= 0.004f) {
                continue;
            }
            float left = sprite.from.x / width;
            float right = (sprite.from.x + sprite.from.w) / width;
            float top = sprite.from.y / height;
            float bottom = (sprite.from.y + sprite.from.h) / height;
            if (sprite.flipX) {
                std::swap(left, right);
            }
            if (sprite.flipY) {
                std::swap(top, bottom);
            }
            SDL_Vertex corners[4];
            int corner = 0;
            for (const bool under : {false, true}) {
                for (const bool rightSide : {false, true}) {
                    const float x = rightSide ? sprite.to.x + sprite.to.w : sprite.to.x;
                    const float y = under ? sprite.to.y + sprite.to.h : sprite.to.y;
                    SDL_FPoint at{x + shift.x, y + shift.y};
                    if (flat) { // on the ground: over the feet falls along the lean, beside them along the across
                        const float over = std::max(0.0f, static_cast<float>(sprite.feet.y) - y); // what hangs under the feet lies at them
                        const float beside = x - static_cast<float>(sprite.feet.x);
                        at = {
                            static_cast<float>(sprite.feet.x) + sprite.startX + beside * sprite.acrossX + over * sprite.leanX,
                            static_cast<float>(sprite.feet.y) + sprite.startY + beside * sprite.acrossY + over * sprite.leanY
                        };
                    }
                    const SDL_FColor color{1, 1, 1, fadesUp && !under ? amount * std::clamp(sprite.amount, 0.0f, 1.0f) : amount};
                    corners[corner++] = {at, color, {rightSide ? right : left, under ? bottom : top}};
                }
            }
            const int order[6] = {0, 1, 3, 0, 3, 2};
            SDL_BlendMode before = SDL_BLENDMODE_BLEND;
            SDL_GetTextureBlendMode(sprite.texture, &before);
            if (SDL_SetTextureBlendMode(sprite.texture, mode)) {
                SDL_RenderGeometry(renderer, sprite.texture, corners, 4, order, 6);
            }
            SDL_SetTextureBlendMode(sprite.texture, before);
        }
    }

    void LightMap::shadeSprites(
        const RenderDevice& device,
        core::Size area,
        const std::vector<Sprite>& sprites,
        const std::function<float(const Sprite&, float)>& amountAt,
        const std::vector<Sprite>& covers,
        const std::vector<std::size_t>& coversFrom
    ) {
        if (m_failed || !device.isAccelerated() || sprites.empty() || area.width <= 0 || area.height <= 0) {
            return;
        }
        constexpr float kBand = 12; // a thing is cut into bands this high, each end with its own amount, px
        SDL_Renderer* renderer = device.renderer();
        // the things with any shade on them: each from its first sprite to before the next feet
        struct Thing {
            std::size_t first = 0;
            std::size_t count = 0;
            SDL_FRect box{};
        };
        std::vector<Thing> things;
        for (std::size_t i = 0; i < sprites.size();) {
            Thing thing{i, 0, sprites[i].to};
            float right = thing.box.x + thing.box.w;
            float bottom = thing.box.y + thing.box.h;
            while (i < sprites.size() && sprites[i].feet == sprites[thing.first].feet) {
                const SDL_FRect& to = sprites[i].to;
                thing.box.x = std::min(thing.box.x, to.x);
                thing.box.y = std::min(thing.box.y, to.y);
                right = std::max(right, to.x + to.w);
                bottom = std::max(bottom, to.y + to.h);
                ++i;
                ++thing.count;
            }
            thing.box.w = right - thing.box.x;
            thing.box.h = bottom - thing.box.y;
            bool any = false;
            for (float y = thing.box.y; y <= bottom && !any; y += kBand) {
                any = amountAt(sprites[thing.first], y) > 0.01f;
            }
            if (any && thing.box.w > 0 && thing.box.h > 0) {
                things.push_back(thing);
            }
        }
        if (things.empty()) {
            return;
        }
        if (!m_shade || area.width > m_shadeWidth || area.height > m_shadeHeight) {
            m_shadeWidth = std::max(area.width, m_shadeWidth);
            m_shadeHeight = std::max(area.height, m_shadeHeight);
            m_shade.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, m_shadeWidth, m_shadeHeight));
            if (!m_shade) {
                return;
            }
            SDL_SetTextureScaleMode(m_shade.get(), SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(m_shade.get(), SDL_BLENDMODE_MOD);
        }
        SDL_Texture* drawnInto = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, m_shade.get())) {
            return;
        }
        // the layer holds how lit each pixel is: white, and a thing's shape as lit as its shade leaves it. The shape is cut into
        // the layer's alpha first, and closed again by what was drawn before the thing: that is not darkened with it
        static const SDL_BlendMode alphaOnly = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD
        );
        static const SDL_BlendMode throughHole = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA, SDL_BLENDFACTOR_DST_ALPHA, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD
        );
        SDL_SetRenderScale(renderer, 1, 1);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        std::vector<SDL_Vertex> corners;
        std::vector<int> order;
        std::vector<Sprite> before;
        for (const Thing& thing : things) {
            SDL_SetRenderDrawBlendMode(renderer, alphaOnly);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &thing.box);
            drawShapes(renderer, &sprites[thing.first], thing.count, false, 1);
            if (coversFrom.size() == sprites.size()) {
                before.clear();
                for (std::size_t i = coversFrom[thing.first]; i < covers.size(); ++i) {
                    const SDL_FRect& to = covers[i].to;
                    if (to.x < thing.box.x + thing.box.w && to.x + to.w > thing.box.x && to.y < thing.box.y + thing.box.h && to.y + to.h > thing.box.y) {
                        before.push_back(covers[i]);
                    }
                }
                const SDL_Rect clip{static_cast<int>(std::floor(thing.box.x)), static_cast<int>(std::floor(thing.box.y)), static_cast<int>(std::ceil(thing.box.w)) + 1, static_cast<int>(std::ceil(thing.box.h)) + 1};
                SDL_SetRenderClipRect(renderer, &clip);
                drawShapes(renderer, before.data(), before.size(), false, 2);
                SDL_SetRenderClipRect(renderer, nullptr);
            }
            const int bands = std::max(1, static_cast<int>(std::ceil(thing.box.h / kBand)));
            corners.clear();
            order.clear();
            for (int row = 0; row <= bands; ++row) {
                const float y = thing.box.y + thing.box.h * static_cast<float>(row) / static_cast<float>(bands);
                const float lit = 1 - std::clamp(amountAt(sprites[thing.first], y), 0.0f, 1.0f);
                const SDL_FColor color{lit, lit, lit, 1};
                corners.push_back({{thing.box.x, y}, color, {0, 0}});
                corners.push_back({{thing.box.x + thing.box.w, y}, color, {0, 0}});
                if (row > 0) {
                    const int first = (row - 1) * 2;
                    for (const int corner : {0, 1, 3, 0, 3, 2}) {
                        order.push_back(first + corner);
                    }
                }
            }
            SDL_SetRenderDrawBlendMode(renderer, throughHole);
            SDL_RenderGeometry(renderer, nullptr, corners.data(), static_cast<int>(corners.size()), order.data(), static_cast<int>(order.size()));
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // as the device sets it: what is filled after blends by its alpha
        SDL_SetRenderTarget(renderer, drawnInto);
        const SDL_FRect whole{0, 0, static_cast<float>(area.width), static_cast<float>(area.height)};
        SDL_RenderTexture(renderer, m_shade.get(), &whole, &whole);
    }

    void LightMap::edgeShadows(const RenderDevice& device, core::Size area, const std::vector<Sprite>& sprites, float reach) {
        if (m_failed || !device.isAccelerated() || sprites.empty() || reach <= 0 || area.width <= 0 || area.height <= 0) {
            return;
        }
        SDL_Renderer* renderer = device.renderer();
        // drawn small and stretched smoothly over the area: the steps of the edge run into each other
        constexpr float fine = 0.25f;
        const int width = static_cast<int>(std::ceil(static_cast<float>(area.width) * fine));
        const int height = static_cast<int>(std::ceil(static_cast<float>(area.height) * fine));
        if (!m_cast || width > m_castWidth || height > m_castHeight) {
            m_castWidth = std::max(width, m_castWidth);
            m_castHeight = std::max(height, m_castHeight);
            m_cast.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, m_castWidth, m_castHeight));
            if (!m_cast) {
                return;
            }
            SDL_SetTextureScaleMode(m_cast.get(), SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(m_cast.get(), SDL_BLENDMODE_MOD);
        }
        SDL_Texture* drawnInto = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, m_cast.get())) {
            return;
        }
        // the layer holds how lit the ground is: white, and a little darker by every shape drawn a step aside. Where more of the
        // steps reach, right at a thing's outline, it is darkest; the ground is seen at a slant, so up and down the steps are shorter
        SDL_SetRenderScale(renderer, fine, fine);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        constexpr float kFlat = 0.6f;
        constexpr float kSteps[16][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f},
                                         {0.55f, 0}, {-0.55f, 0}, {0, 0.55f}, {0, -0.55f}, {0.2f, 0.2f}, {-0.2f, 0.2f}, {0.2f, -0.2f}, {-0.2f, -0.2f}};
        for (const auto& step : kSteps) {
            drawShapes(renderer, sprites.data(), sprites.size(), false, 0, {step[0] * reach, step[1] * reach * kFlat}, true);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // as the device sets it
        SDL_SetRenderTarget(renderer, drawnInto);
        const SDL_FRect part{0, 0, static_cast<float>(area.width) * fine, static_cast<float>(area.height) * fine};
        const SDL_FRect whole{0, 0, static_cast<float>(area.width), static_cast<float>(area.height)};
        SDL_RenderTexture(renderer, m_cast.get(), &part, &whole);
    }

    void LightMap::castShadows(
        const RenderDevice& device,
        core::Size area,
        const std::vector<Shadow>& shadows,
        const std::vector<Sprite>& sprites,
        const Tone& tone,
        core::Point paintedAt,
        bool leavePainted,
        const std::vector<SDL_Vertex>& solids
    ) {
        if (m_failed || !device.isAccelerated() || (shadows.empty() && sprites.empty() && solids.empty()) || area.width <= 0 || area.height <= 0) {
            return;
        }
        if (!m_blob && !prepare(device, {kCoarse, kCoarse})) {
            return;
        }
        SDL_Renderer* renderer = device.renderer();
        // as fine as the area is shown: a shadow's edge is as sharp as the figure's that casts it
        const float fine = std::clamp(device.drawScale(), 1.0f, kCastFinest);
        const int width = static_cast<int>(std::ceil(static_cast<float>(area.width) * fine));
        const int height = static_cast<int>(std::ceil(static_cast<float>(area.height) * fine));
        if (!m_cast || width > m_castWidth || height > m_castHeight) {
            m_castWidth = std::max(width, m_castWidth);
            m_castHeight = std::max(height, m_castHeight);
            m_cast.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, m_castWidth, m_castHeight));
            if (!m_cast) {
                return;
            }
            SDL_SetTextureScaleMode(m_cast.get(), SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(m_cast.get(), SDL_BLENDMODE_MOD);
        }
        SDL_Texture* drawnInto = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, m_cast.get())) {
            return;
        }
        // the layer holds how lit the ground is: white, black where a shadow falls. Shadows that overlap are black still, not
        // twice as dark; at the end black is raised to the shadow's tone
        SDL_SetRenderScale(renderer, fine, fine);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_SetTextureColorMod(m_blob.get(), 0, 0, 0);
        for (const Shadow& shadow : shadows) {
            const SDL_FRect to{static_cast<float>(shadow.middle.x - shadow.halfLength), static_cast<float>(shadow.middle.y - shadow.halfWidth), static_cast<float>(shadow.halfLength * 2), static_cast<float>(shadow.halfWidth * 2)};
            SDL_SetTextureAlphaMod(m_blob.get(), byteOf(shadow.amount));
            SDL_RenderTextureRotated(renderer, m_blob.get(), nullptr, &to, static_cast<double>(shadow.degrees), nullptr, SDL_FLIP_NONE);
        }
        drawShapes(renderer, sprites.data(), sprites.size(), true);
        if (!solids.empty()) {
            // the darker of what is there and the triangle: a thing's parts overlap without getting darker
            static const SDL_BlendMode darkest = SDL_ComposeCustomBlendMode(
                SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM, SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM
            );
            if (!SDL_SetRenderDrawBlendMode(renderer, darkest)) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // they cover what is there instead
            }
            SDL_RenderGeometry(renderer, nullptr, solids.data(), static_cast<int>(solids.size()), nullptr, 0);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        if (m_painted && leavePainted) { // white again where the picture is in painted shade
            float cellsAcross = 0;
            float cellsDown = 0;
            SDL_GetTextureSize(m_painted.get(), &cellsAcross, &cellsDown);
            SDL_SetTextureColorMod(m_painted.get(), 255, 255, 255);
            const SDL_FRect to{static_cast<float>(paintedAt.x), static_cast<float>(paintedAt.y), cellsAcross * static_cast<float>(m_paintedCell), cellsDown * static_cast<float>(m_paintedCell)};
            SDL_RenderTexture(renderer, m_painted.get(), nullptr, &to);
        }
        // what is there + the tone x (1 - what is there): black becomes the tone, white stays
        static const SDL_BlendMode raise = SDL_ComposeCustomBlendMode(
            SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD
        );
        if (!SDL_SetRenderDrawBlendMode(renderer, raise)) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        }
        SDL_SetRenderDrawColor(renderer, byteOf(tone.r), byteOf(tone.g), byteOf(tone.b), 255);
        SDL_RenderFillRect(renderer, nullptr);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // as the device sets it
        SDL_SetRenderTarget(renderer, drawnInto);
        const SDL_FRect part{0, 0, static_cast<float>(area.width) * fine, static_cast<float>(area.height) * fine};
        const SDL_FRect whole{0, 0, static_cast<float>(area.width), static_cast<float>(area.height)};
        SDL_RenderTexture(renderer, m_cast.get(), &part, &whole);
    }

    void LightMap::drawSpot(const RenderDevice& device, core::Point middle, int halfLength, int halfWidth, float degrees, const Tone& tone, float amount, bool adds) {
        if (m_failed || !device.isAccelerated() || amount <= 0 || halfLength <= 0 || halfWidth <= 0) {
            return;
        }
        if (!m_spot && !prepare(device, {kCoarse, kCoarse})) {
            return;
        }
        SDL_Renderer* renderer = device.renderer();
        const SDL_FRect to{static_cast<float>(middle.x - halfLength), static_cast<float>(middle.y - halfWidth), static_cast<float>(halfLength * 2), static_cast<float>(halfWidth * 2)};
        const float times = adds ? amount : 1.0f;
        SDL_SetTextureColorMod(m_spot.get(), byteOf(tone.r * times), byteOf(tone.g * times), byteOf(tone.b * times));
        if (!adds) {
            SDL_SetTextureBlendMode(m_spot.get(), SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(m_spot.get(), byteOf(amount));
        }
        SDL_RenderTextureRotated(renderer, m_spot.get(), nullptr, &to, static_cast<double>(degrees), nullptr, SDL_FLIP_NONE);
        if (!adds) {
            SDL_SetTextureBlendMode(m_spot.get(), SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(m_spot.get(), 255);
        }
    }

    bool LightMap::apply(const RenderDevice& device, core::Size area, const Frame& frame) {
        if (m_failed || !device.isAccelerated() || area.width <= 0 || area.height <= 0) {
            return false;
        }
        if (!prepare(device, area)) {
            core::logWarning("lighting: off, the renderer cannot make its textures: ", SDL_GetError());
            m_failed = true;
            return false;
        }
        SDL_Renderer* renderer = device.renderer();
        const float unit = m_doubles ? 0.5f : 1.0f; // the map's value for light 1
        const auto mod = [&](SDL_Texture* texture, const Tone& tone, float times) {
            SDL_SetTextureColorMod(texture, byteOf(tone.r * times), byteOf(tone.g * times), byteOf(tone.b * times));
        };
        SDL_Texture* drawnInto = SDL_GetRenderTarget(renderer);
        if (!SDL_SetRenderTarget(renderer, m_map.get())) {
            return false;
        }
        SDL_SetRenderScale(renderer, 1.0f / kCoarse, 1.0f / kCoarse); // the map is drawn in the area's pixels
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, byteOf(frame.ambient.r * unit), byteOf(frame.ambient.g * unit), byteOf(frame.ambient.b * unit), 255);
        SDL_RenderClear(renderer);
        const SDL_FRect whole{0, 0, static_cast<float>(area.width), static_cast<float>(area.height)};
        for (int layer = 0; layer < 2 && frame.shadeAmount > 0; ++layer) { // two layers of other sizes: together they repeat nowhere near
            const float scale = layer == 0 ? frame.shadeScale : frame.shadeScale2;
            const core::Point at = layer == 0 ? frame.shadeAt : frame.shadeAt2;
            if (scale <= 0) {
                continue;
            }
            const float tile = kPatternSize * scale;
            const float left = std::fmod(static_cast<float>(at.x), tile);
            const float top = std::fmod(static_cast<float>(at.y), tile);
            mod(m_pattern.get(), frame.shade, unit);
            SDL_SetTextureAlphaMod(m_pattern.get(), byteOf(frame.shadeAmount));
            for (float y = top > 0 ? top - tile : top; y < whole.h; y += tile) {
                for (float x = left > 0 ? left - tile : left; x < whole.w; x += tile) {
                    const SDL_FRect to{x, y, tile, tile};
                    SDL_RenderTexture(renderer, m_pattern.get(), nullptr, &to);
                }
            }
        }
        if (m_painted && frame.shadeAmount > 0) { // the ambient light again where the picture is in painted shade
            float cellsAcross = 0;
            float cellsDown = 0;
            SDL_GetTextureSize(m_painted.get(), &cellsAcross, &cellsDown);
            mod(m_painted.get(), frame.ambient, unit);
            const SDL_FRect to{static_cast<float>(frame.paintedAt.x), static_cast<float>(frame.paintedAt.y), cellsAcross * static_cast<float>(m_paintedCell), cellsDown * static_cast<float>(m_paintedCell)};
            SDL_RenderTexture(renderer, m_painted.get(), nullptr, &to);
        }
        for (const Light& light : frame.lights) {
            const SDL_FRect to{static_cast<float>(light.at.x - light.radius), static_cast<float>(light.at.y - light.radius), static_cast<float>(light.radius * 2), static_cast<float>(light.radius * 2)};
            mod(m_spot.get(), light.tone, unit);
            SDL_RenderTexture(renderer, m_spot.get(), nullptr, &to);
        }
        if (frame.corners > 0) {
            SDL_SetTextureAlphaMod(m_corner.get(), byteOf(frame.corners));
            SDL_RenderTexture(renderer, m_corner.get(), nullptr, &whole);
        }
        SDL_SetRenderTarget(renderer, drawnInto);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // as the device sets it
        const SDL_FRect part{0, 0, static_cast<float>(area.width) / kCoarse, static_cast<float>(area.height) / kCoarse};
        SDL_RenderTexture(renderer, m_map.get(), &part, &whole);
        if (frame.glow > 0) {
            // the part that brightens the picture by its own colour: added light makes it paler (its colours run to white), this
            // keeps them as they are, only brighter. It is stronger, since on the dark parts it adds little
            constexpr float kVividTimes = 2.5f;
            const float vivid = m_spotLit ? std::clamp(frame.vivid, 0.0f, 1.0f) : 0.0f;
            for (const Light& light : frame.lights) {
                if (!light.glows) {
                    continue;
                }
                const SDL_FRect to{static_cast<float>(light.at.x - light.radius), static_cast<float>(light.at.y - light.radius), static_cast<float>(light.radius * 2), static_cast<float>(light.radius * 2)};
                if (vivid < 1) {
                    mod(m_spot.get(), light.tone, frame.glow * (1 - vivid));
                    SDL_RenderTexture(renderer, m_spot.get(), nullptr, &to);
                }
                if (vivid > 0) {
                    mod(m_spotLit.get(), light.tone, frame.glow * vivid * kVividTimes);
                    SDL_RenderTexture(renderer, m_spotLit.get(), nullptr, &to);
                }
            }
        }
        return true;
    }

}
