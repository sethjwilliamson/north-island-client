#include "client/subsystems/battle/BattleTween.h"

#include "core/Between.h"

#include <algorithm>

namespace client {

    namespace {

        /// @brief Calls a visitor for every effect layer of a fighter with its place in FighterPose::layers.
        /// @tparam Visit Callable taking (int index, EffectLayer& layer).
        /// @param fighter The fighter whose status, skill and hit layers are visited.
        /// @param visit The visitor.
        template <typename Visit>
        void forEachLayer(Fighter& fighter, Visit visit) {
            int index = 0;
            for (auto& set : fighter.statusLayers) {
                for (EffectLayer& layer : set) {
                    visit(index++, layer);
                }
            }
            for (EffectLayer& layer : fighter.skillLayers) {
                visit(index++, layer);
            }
            for (EffectLayer& layer : fighter.hitLayers) {
                visit(index++, layer);
            }
        }

    }

    double BattleTween::progress(std::uint64_t currentMs, std::uint64_t nowMs, std::uint64_t nextBeatMs) {
        if (nextBeatMs <= currentMs || nowMs >= nextBeatMs) {
            return 1.0;
        }
        if (nowMs <= currentMs) {
            return 0.0;
        }
        return static_cast<double>(nowMs - currentMs) / static_cast<double>(nextBeatMs - currentMs);
    }

    core::Point BattleTween::between(core::Point from, core::Point to, double amount) { return core::between(from, to, amount, kSnapPixels); }

    BattleTween::Snapshot BattleTween::take(const BattleCamera& camera, BattleField& field, std::uint64_t atMs) {
        Snapshot snapshot;
        snapshot.atMs = atMs;
        snapshot.camera = camera.position();
        snapshot.farX = camera.farX();
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = field.fighter(slot);
            if (!found) {
                continue;
            }
            Fighter& fighter = *found;
            FighterPose pose;
            pose.id = fighter.id;
            pose.position = fighter.position;
            pose.jumpHeight = fighter.jumpHeight;
            pose.flyRise = fighter.flyRise;
            pose.placed = fighter.placed;
            forEachLayer(fighter, [&](int index, EffectLayer& layer) {
                if (layer.placed()) {
                    pose.layers[static_cast<std::size_t>(index)] = layer.position();
                }
            });
            snapshot.fighters[static_cast<std::size_t>(slot)] = pose;
        }
        return snapshot;
    }

    void BattleTween::put(BattleCamera& camera, BattleField& field, const Snapshot& snapshot) {
        camera.showAt(snapshot.camera, snapshot.farX);
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = field.fighter(slot);
            const auto& pose = snapshot.fighters[static_cast<std::size_t>(slot)];
            if (!found || !pose || pose->id != found->get().id) {
                continue;
            }
            Fighter& fighter = *found;
            fighter.position = pose->position;
            fighter.jumpHeight = pose->jumpHeight;
            fighter.flyRise = pose->flyRise;
            forEachLayer(fighter, [&](int index, EffectLayer& layer) {
                if (const auto& at = pose->layers[static_cast<std::size_t>(index)]; at && layer.playing()) {
                    layer.setPosition(*at);
                }
            });
        }
    }

    void BattleTween::record(const BattleCamera& camera, BattleField& field, std::uint64_t atMs) {
        m_previous = std::move(m_current);
        m_current = take(camera, field, atMs);
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            if (const auto found = field.fighter(slot)) {
                found->get().placed = false;
            }
        }
    }

    void BattleTween::apply(BattleCamera& camera, BattleField& field, std::uint64_t nowMs, std::uint64_t nextBeatMs) {
        m_saved.reset();
        if (!m_previous || !m_current) {
            return;
        }
        const double amount = progress(m_current->atMs, nowMs, nextBeatMs);
        if (amount >= 1.0) {
            return;
        }
        m_saved = take(camera, field, m_current->atMs);
        Snapshot shown = *m_saved;
        shown.camera = between(m_previous->camera, m_current->camera, amount);
        shown.farX = between({m_previous->farX, 0}, {m_current->farX, 0}, amount).x;
        for (std::size_t slot = 1; slot < shown.fighters.size(); ++slot) {
            auto& pose = shown.fighters[slot];
            const auto& before = m_previous->fighters[slot];
            const auto& after = m_current->fighters[slot];
            if (!pose || !before || !after || before->id != pose->id || after->id != pose->id) {
                continue;
            }
            if (after->placed) {
                // snapped this tick: not blended from the tick before, but kept on its place on the ground while the camera is
                // drawn between the ticks (its screen place counts the tick's camera)
                pose->position = {after->position.x + m_current->camera.x - shown.camera.x, after->position.y + m_current->camera.y - shown.camera.y};
                continue;
            }
            pose->position = between(before->position, after->position, amount);
            pose->jumpHeight = between({before->jumpHeight, 0}, {after->jumpHeight, 0}, amount).x;
            pose->flyRise = between({before->flyRise, 0}, {after->flyRise, 0}, amount).x;
            for (std::size_t i = 0; i < pose->layers.size(); ++i) {
                if (pose->layers[i] && before->layers[i] && after->layers[i]) {
                    pose->layers[i] = between(*before->layers[i], *after->layers[i], amount);
                }
            }
        }
        put(camera, field, shown);
    }

    void BattleTween::restore(BattleCamera& camera, BattleField& field) {
        if (m_saved) {
            put(camera, field, *m_saved);
            m_saved.reset();
        }
    }

}
