#include "client/rendering/world/SolidShadows.h"

#include "gfx/SpriteShapes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace client {

    namespace {

        constexpr float kCell = 1; // the shape is worked out in squares this many px wide
        constexpr int kMostCells = 320; // across or down: a bigger figure is not cast
        constexpr float kClimb = 1.0f; // the ground under a column rises at most this many px for every px across
        constexpr int kNear = 4; // a column's ground is evened out with this many columns to each side
        constexpr float kLyingHigh = 0.7f; // of what a lying thing's column has over its ground this much is height, the rest lies back
        constexpr float kRound = 0.5f; // a part is this much as deep as it is thick across
        constexpr float kAloft = 8; // a figure whose lowest pixel is this far over its feet or more is drawn in the air, px

        /// @brief The convex outline of some points, in order.
        /// @return How many points it has.
        int hull(std::array<SDL_FPoint, 8>& points, int count, std::array<SDL_FPoint, 17>& out) {
            std::sort(points.begin(), points.begin() + count, [](const SDL_FPoint& a, const SDL_FPoint& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
            const auto turn = [](const SDL_FPoint& o, const SDL_FPoint& a, const SDL_FPoint& b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
            int size = 0;
            for (int i = 0; i < count; ++i) {
                while (size >= 2 && turn(out[static_cast<std::size_t>(size - 2)], out[static_cast<std::size_t>(size - 1)], points[static_cast<std::size_t>(i)]) <= 0) {
                    --size;
                }
                out[static_cast<std::size_t>(size++)] = points[static_cast<std::size_t>(i)];
            }
            const int lower = size + 1;
            for (int i = count - 2; i >= 0; --i) {
                while (size >= lower && turn(out[static_cast<std::size_t>(size - 2)], out[static_cast<std::size_t>(size - 1)], points[static_cast<std::size_t>(i)]) <= 0) {
                    --size;
                }
                out[static_cast<std::size_t>(size++)] = points[static_cast<std::size_t>(i)];
            }
            return std::max(0, size - 1); // the last is the first again
        }

    }

    void castSolidShadow(const std::vector<gfx::LightMap::Sprite>& parts, core::Point feet, const SolidLight& light, float amount, std::vector<SDL_Vertex>& triangles) {
        if (parts.empty() || amount <= 0.004f) {
            return;
        }
        // the figure's shape, put together from its sprites, a byte a square
        float left = std::numeric_limits<float>::max();
        float top = left;
        float right = -left;
        float bottom = -left;
        for (const gfx::LightMap::Sprite& part : parts) {
            if (part.to.w > 0 && part.to.h > 0) {
                left = std::min(left, part.to.x);
                top = std::min(top, part.to.y);
                right = std::max(right, part.to.x + part.to.w);
                bottom = std::max(bottom, part.to.y + part.to.h);
            }
        }
        if (right <= left || bottom <= top) {
            return;
        }
        left = std::floor(left);
        top = std::floor(top);
        const int across = static_cast<int>(std::ceil((right - left) / kCell));
        const int down = static_cast<int>(std::ceil((bottom - top) / kCell));
        if (across <= 0 || down <= 0 || across > kMostCells || down > kMostCells) {
            return;
        }
        std::vector<std::uint8_t> shape(static_cast<std::size_t>(across * down), 0);
        float alpha = 0;
        for (const gfx::LightMap::Sprite& part : parts) {
            gfx::SpriteMask mask;
            if (part.to.w <= 0 || part.to.h <= 0 || part.from.w <= 0 || part.from.h <= 0 || part.alpha < 0.05f || !gfx::spriteMask(part.texture, mask)) {
                continue;
            }
            alpha = std::max(alpha, part.alpha);
            const int fromColumn = std::max(0, static_cast<int>((part.to.x - left) / kCell));
            const int toColumn = std::min(across, static_cast<int>(std::ceil((part.to.x + part.to.w - left) / kCell)));
            const int fromRow = std::max(0, static_cast<int>((part.to.y - top) / kCell));
            const int toRow = std::min(down, static_cast<int>(std::ceil((part.to.y + part.to.h - top) / kCell)));
            for (int row = fromRow; row < toRow; ++row) {
                float v = (top + (static_cast<float>(row) + 0.5f) * kCell - part.to.y) / part.to.h;
                if (v < 0 || v >= 1) {
                    continue;
                }
                if (part.flipY) {
                    v = 1 - v;
                }
                const int y = static_cast<int>(part.from.y + v * part.from.h);
                for (int column = fromColumn; column < toColumn; ++column) {
                    float u = (left + (static_cast<float>(column) + 0.5f) * kCell - part.to.x) / part.to.w;
                    if (u < 0 || u >= 1) {
                        continue;
                    }
                    if (part.flipX) {
                        u = 1 - u;
                    }
                    if (mask.seen(static_cast<int>(part.from.x + u * part.from.w), y)) {
                        shape[static_cast<std::size_t>(row * across + column)] = 1;
                    }
                }
            }
        }
        amount *= std::min(1.0f, alpha);
        if (amount <= 0.004f) {
            return;
        }
        const auto seen = [&](int column, int row) { return column >= 0 && row >= 0 && column < across && row < down && shape[static_cast<std::size_t>(row * across + column)] != 0; };
        const auto index = [](int value) { return static_cast<std::size_t>(value); };
        // where each column touches the ground: its lowest square, unless that lies far over its neighbours' ground (what is held
        // out hangs in the air; a lying body climbs gently from its tail to its head)
        std::vector<int> lowest(index(across), -1);
        std::vector<int> highest(index(across), -1);
        for (int column = 0; column < across; ++column) {
            for (int row = down - 1; row >= 0; --row) {
                if (seen(column, row)) {
                    lowest[index(column)] = row;
                    break;
                }
            }
            for (int row = 0; row < down; ++row) {
                if (seen(column, row)) {
                    highest[index(column)] = row;
                    break;
                }
            }
        }
        std::vector<float> grounded(index(across), -1);
        for (int column = 0; column < across; ++column) {
            if (lowest[index(column)] < 0) {
                continue;
            }
            float under = static_cast<float>(lowest[index(column)]);
            for (int other = 0; other < across; ++other) {
                if (lowest[index(other)] >= 0) {
                    under = std::max(under, static_cast<float>(lowest[index(other)]) - kClimb * static_cast<float>(std::abs(other - column)));
                }
            }
            grounded[index(column)] = under;
        }
        std::vector<float> ground(index(across), -1); // the screen's y where each column touches the ground
        std::vector<float> grounds;
        std::vector<float> highs;
        float seenTop = std::numeric_limits<float>::max();
        float seenBottom = -seenTop;
        int seenLeft = across;
        int seenRight = -1;
        for (int column = 0; column < across; ++column) {
            if (lowest[index(column)] < 0) {
                continue;
            }
            float sum = 0;
            int count = 0;
            for (int other = std::max(0, column - kNear); other <= std::min(across - 1, column + kNear); ++other) {
                if (grounded[index(other)] >= 0) {
                    sum += grounded[index(other)];
                    ++count;
                }
            }
            const float row = std::max(static_cast<float>(lowest[index(column)]), sum / static_cast<float>(count));
            ground[index(column)] = top + (row + 1) * kCell;
            const float columnTop = top + static_cast<float>(highest[index(column)]) * kCell;
            grounds.push_back(ground[index(column)]);
            highs.push_back(ground[index(column)] - columnTop);
            seenTop = std::min(seenTop, columnTop);
            seenBottom = std::max(seenBottom, top + static_cast<float>(lowest[index(column)] + 1) * kCell);
            seenLeft = std::min(seenLeft, column);
            seenRight = std::max(seenRight, column);
        }
        if (grounds.empty()) {
            return;
        }
        const float seenHigh = seenBottom - seenTop;
        // how much it lies: by how far up the figure the ground it touches reaches, or by how low it is for its width. One drawn in
        // the air (its lowest pixel well over its feet: a dragonfly, a bee) does not lie however wide it is: it hangs over its feet
        float lying = 0;
        const bool aloft = static_cast<float>(feet.y) - seenBottom >= kAloft;
        if (!aloft && grounds.size() >= 4 && seenHigh > 0) {
            std::sort(grounds.begin(), grounds.end());
            std::sort(highs.begin(), highs.end());
            const float spread = (grounds[grounds.size() * 4 / 5] - grounds[grounds.size() / 5]) / seenHigh;
            const float wide = static_cast<float>(seenRight - seenLeft + 1) * kCell;
            const float low = highs[highs.size() / 2] / wide;
            lying = std::max(std::clamp((spread - 0.25f) / (0.4f - 0.25f), 0.0f, 1.0f), std::clamp((0.6f - low) / (0.6f - 0.4f), 0.0f, 1.0f));
        }
        // an upright figure stands on its feet; a lying one on its own ground at every column
        const float highShare = 1 - (1 - kLyingHigh) * lying;
        for (float& at : ground) {
            if (at >= 0) {
                at = static_cast<float>(feet.y) + (at - static_cast<float>(feet.y)) * lying;
            }
        }
        // the solid: every square of the shape is a box over the ground, as high as it is drawn over its column's ground and as deep
        // as the shape is thick there, standing where its column touches the ground (a lying thing's parts further up lie further
        // back). The squares of a column that stand on the same ground and are as deep are one box. Each box is cast whole, so
        // what the figure holds out or has between its parts shows in the shadow as in the sprite
        const auto runFrom = [&](int column, int row, int stepX, int stepY) {
            int count = 0;
            while (seen(column + stepX * (count + 1), row + stepY * (count + 1))) {
                ++count;
            }
            return static_cast<float>(count) + 0.5f;
        };
        const float lampHigh = std::max(20.0f, light.lampHigh);
        const auto cast = [&](float x, float y, float high) {
            const SDL_FPoint sun{x + high * light.sunX, y + high * light.sunY};
            if (light.lampShare <= 0) {
                return sun;
            }
            const float over = std::min(high, lampHigh * 0.75f);
            const float stretch = lampHigh / (lampHigh - over);
            const float groundY = y / light.flat;
            const SDL_FPoint lamp{light.lampX + (x - light.lampX) * stretch, (light.lampY + (groundY - light.lampY) * stretch) * light.flat};
            return SDL_FPoint{sun.x + (lamp.x - sun.x) * light.lampShare, sun.y + (lamp.y - sun.y) * light.lampShare};
        };
        const float shade = 1 - std::min(1.0f, amount);
        const SDL_FColor color{shade, shade, shade, 1};
        std::array<SDL_FPoint, 8> corners{};
        std::array<SDL_FPoint, 17> outline{};
        const auto box = [&](float x, float middle, float deep, float low, float high) {
            int count = 0;
            for (const float h : {low, high}) {
                for (const float dx : {0.0f, kCell}) {
                    for (const float dy : {-deep, deep}) {
                        corners[index(count++)] = cast(x + dx, middle + dy, h);
                    }
                }
            }
            const int size = hull(corners, count, outline);
            for (int i = 1; i + 1 < size; ++i) {
                triangles.push_back({outline[0], color, {0, 0}});
                triangles.push_back({outline[index(i)], color, {0, 0}});
                triangles.push_back({outline[index(i + 1)], color, {0, 0}});
            }
        };
        for (int column = 0; column < across; ++column) {
            if (ground[index(column)] < 0) {
                continue;
            }
            const float under = ground[index(column)];
            const float x = left + static_cast<float>(column) * kCell;
            bool open = false; // a box is being made
            float boxMiddle = 0;
            float boxDeep = 0;
            float boxLow = 0;
            float boxHigh = 0;
            for (int row = down - 1; row >= -1; --row) { // from the ground up
                bool part = false;
                float middle = 0;
                float deep = 0;
                float low = 0;
                float high = 0;
                if (row >= 0 && seen(column, row)) {
                    const float cellTop = top + static_cast<float>(row) * kCell;
                    high = std::max(0.0f, under - cellTop) * highShare;
                    low = std::max(0.0f, under - cellTop - kCell) * highShare;
                    // where it stands, the screen's y, and as deep as it is thick: the shorter of its width and its height there,
                    // as for a round body
                    middle = std::round(under - std::max(0.0f, under - (cellTop + kCell / 2)) * (1 - highShare));
                    const float thick = std::min(std::sqrt(runFrom(column, row, -1, 0) * runFrom(column, row, 1, 0)), std::sqrt(runFrom(column, row, 0, -1) * runFrom(column, row, 0, 1)));
                    deep = std::round(thick * kCell * kRound * light.flat * 2) / 2;
                    part = high > 0.5f;
                }
                if (open && (!part || middle != boxMiddle || deep != boxDeep)) {
                    box(x, boxMiddle, std::max(0.5f, boxDeep), boxLow, boxHigh);
                    open = false;
                }
                if (part) {
                    if (!open) {
                        open = true;
                        boxMiddle = middle;
                        boxDeep = deep;
                        boxLow = low;
                    }
                    boxHigh = high;
                }
            }
        }
    }

}
