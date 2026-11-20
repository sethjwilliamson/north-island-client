#include "gfx/SpriteShapes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gfx {

    namespace {

        constexpr std::uint8_t kSeen = 32; // a pixel this opaque or more is seen

        struct Shape {
            int width = 0;
            int height = 0;
            int words = 0; // 64-bit words a row
            std::vector<std::uint64_t> bits;
            std::unordered_map<std::uint64_t, SDL_FRect> parts; // what spriteSeenPart worked out, by rectangle
            std::unordered_map<std::uint64_t, std::pair<std::vector<float>, std::vector<float>>> columns; // what spriteColumnBottoms worked out, by rectangle
        };

        std::unordered_map<const SDL_Texture*, Shape>& shapes() {
            static std::unordered_map<const SDL_Texture*, Shape> all;
            return all;
        }

    }

    void noteSpriteShape(SDL_Texture* texture, const formats::Image& image) {
        if (texture == nullptr || image.isEmpty()) {
            return;
        }
        Shape shape;
        shape.width = image.width();
        shape.height = image.height();
        shape.words = (shape.width + 63) / 64;
        shape.bits.assign(static_cast<std::size_t>(shape.words) * static_cast<std::size_t>(shape.height), 0);
        for (int y = 0; y < shape.height; ++y) {
            std::uint64_t* row = &shape.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(shape.words)];
            for (int x = 0; x < shape.width; ++x) {
                if (image.at(x, y).a >= kSeen) {
                    row[x / 64] |= std::uint64_t{1} << (x % 64);
                }
            }
        }
        shapes()[texture] = std::move(shape);
    }

    void forgetSpriteShape(SDL_Texture* texture) { shapes().erase(texture); }

    bool spriteMask(const SDL_Texture* texture, SpriteMask& mask) {
        const auto found = shapes().find(texture);
        if (found == shapes().end() || found->second.bits.empty()) {
            return false;
        }
        mask = {found->second.width, found->second.height, found->second.words, found->second.bits.data()};
        return true;
    }

    bool spriteSeenPart(const SDL_Texture* texture, const SDL_FRect& from, SDL_FRect& seen) {
        const auto found = shapes().find(texture);
        if (found == shapes().end()) {
            return false;
        }
        Shape& shape = found->second;
        const int left = std::clamp(static_cast<int>(std::floor(from.x)), 0, shape.width);
        const int top = std::clamp(static_cast<int>(std::floor(from.y)), 0, shape.height);
        const int right = std::clamp(static_cast<int>(std::ceil(from.x + from.w)), left, shape.width);
        const int bottom = std::clamp(static_cast<int>(std::ceil(from.y + from.h)), top, shape.height);
        const std::uint64_t key = (static_cast<std::uint64_t>(left) << 48) | (static_cast<std::uint64_t>(top) << 32) | (static_cast<std::uint64_t>(right) << 16) | static_cast<std::uint64_t>(bottom);
        if (const auto part = shape.parts.find(key); part != shape.parts.end()) {
            seen = part->second;
            return seen.w > 0;
        }
        int minX = right;
        int maxX = left - 1;
        int minY = bottom;
        int maxY = top - 1;
        for (int y = top; y < bottom; ++y) {
            const std::uint64_t* row = &shape.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(shape.words)];
            for (int x = left; x < right;) {
                const std::uint64_t word = row[x / 64] >> (x % 64);
                if (word == 0) {
                    x = (x / 64 + 1) * 64;
                    continue;
                }
                if (word & 1) {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = y;
                }
                ++x;
            }
        }
        seen = maxX >= minX ? SDL_FRect{static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(maxX - minX + 1), static_cast<float>(maxY - minY + 1)} : SDL_FRect{0, 0, 0, 0};
        shape.parts.emplace(key, seen);
        return seen.w > 0;
    }

    bool spriteColumnBottoms(const SDL_Texture* texture, const SDL_FRect& from, int strip, std::vector<float>& bottoms, std::vector<float>& tops) {
        bottoms.clear();
        tops.clear();
        const auto found = shapes().find(texture);
        if (found == shapes().end() || strip < 1) {
            return false;
        }
        Shape& shape = found->second;
        const int left = std::clamp(static_cast<int>(std::floor(from.x)), 0, shape.width);
        const int top = std::clamp(static_cast<int>(std::floor(from.y)), 0, shape.height);
        const int right = std::clamp(static_cast<int>(std::ceil(from.x + from.w)), left, shape.width);
        const int bottom = std::clamp(static_cast<int>(std::ceil(from.y + from.h)), top, shape.height);
        const std::uint64_t key = (static_cast<std::uint64_t>(left) << 48) | (static_cast<std::uint64_t>(top) << 32) | (static_cast<std::uint64_t>(right) << 16) | static_cast<std::uint64_t>(bottom);
        if (const auto known = shape.columns.find(key); known != shape.columns.end()) {
            bottoms = known->second.first;
            tops = known->second.second;
            return true;
        }
        for (int x = left; x < right; x += strip) {
            float lowest = -1;
            for (int y = bottom - 1; y >= top && lowest < 0; --y) {
                const std::uint64_t* row = &shape.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(shape.words)];
                for (int column = x; column < std::min(right, x + strip); ++column) {
                    if ((row[column / 64] >> (column % 64)) & 1) {
                        lowest = static_cast<float>(y);
                        break;
                    }
                }
            }
            bottoms.push_back(lowest);
            float highest = -1;
            for (int y = top; y < bottom && lowest >= 0 && highest < 0; ++y) {
                const std::uint64_t* row = &shape.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(shape.words)];
                for (int column = x; column < std::min(right, x + strip); ++column) {
                    if ((row[column / 64] >> (column % 64)) & 1) {
                        highest = static_cast<float>(y);
                        break;
                    }
                }
            }
            tops.push_back(highest);
        }
        shape.columns.emplace(key, std::make_pair(bottoms, tops));
        return true;
    }

}
