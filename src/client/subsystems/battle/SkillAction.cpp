#include "client/subsystems/battle/SkillAction.h"

#include "client/subsystems/battle/Fighter.h"
#include "client/subsystems/battle/StatusRules.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace client {

    namespace {

        constexpr int kFlee = 60041; // skill 60041: Flee
        constexpr int kFailToFlee = 60046; // skill 60046: Fail to Flee
        constexpr int kCapture = 10008; // skill 10008: Capture
        constexpr int kGuardMotion = 60021; // motion / skill 60021: Defense (the guard pose of an avoided hit)
        constexpr int kBlockMotion = 60031; // motion 60031: the block pose (guard 2)
        constexpr int kKnockedMotion = 60011; // motion 60011: knocked away (hit kind 1)
        constexpr int kThrownMotion = 65090; // motion 65090: thrown (hit kind other than 1)
        constexpr int kKnockedHitMotion = 15996; // motion 15996: the hit reaction of a fighter with a knock-away pending
        constexpr int kThrownHitMotion = 15090; // motion 15090: the hit reaction of a fighter with a throw pending
        constexpr int kHp = 25; // attribute id 25: HP
        constexpr int kSp = 26; // attribute id 26: SP
        constexpr int kFleeType = 12; // skill type 12: flee
        constexpr int kReviveType = 8; // skill type 8: revive (may target fallen fighters)
        constexpr int kEnemyOffset = 570; // px the attacker stops short of an enemy on x
        constexpr double kTextSpeed = 0.24; // floating text speed, px per ms
        constexpr int kTextRise = 200; // px a floating text rises
        constexpr int kTextKind = 3; // floating text kind 3: a number with a fixed font size
        constexpr int kHpColour = 8; // number colour index 8: HP's yellow-red font
        constexpr int kSpColour = 9; // number colour index 9: SP's blue font
        constexpr double kReturnCameraSpeed = 0.6; // camera speed when returning to rest, px per ms

        /// @brief Delphi-style Random(range): a number from 0 to range - 1.
        /// @param generator Random engine.
        /// @param range Exclusive upper bound.
        /// @return The random number.
        int randomBelow(std::mt19937& generator, int range) { return static_cast<int>(generator() % static_cast<std::uint32_t>(range)); }

        /// @brief The idle stance action of a fighter by its side.
        /// @param fighter Fighter asked about.
        /// @return Action number of the stance.
        int stance(const Fighter& fighter) { return fighter.side() == 1 ? 16 : 17; } // idle stance action: 16 on side 1, 17 on the other

        /// @brief The action number of a two-way action for the fighter's side (the side keeps the facing).
        /// @details Original: SetHumanAction for the two-way actions.
        /// @param fighter Fighter asked about.
        /// @param action Action number for side 1.
        /// @return The action number to use.
        int facing(const Fighter& fighter, int action) { return action + (fighter.side() == 1 ? 0 : 1); }

        /// @brief Keeps a fallen fighter lying on its cell while a hit motion still runs for it.
        /// @details With several attackers on one target the target can fall before the later hits' motions run; those would throw
        /// the body along the hit path, walk it home and stand it up for a tick.
        /// @param hit Fighter that is hit.
        /// @param camera The battle camera's position (the cell is drawn moved by it while the camera is away from rest).
        /// @return True when the fighter is fallen and was laid down.
        bool layFallen(Fighter& hit, core::Point camera) {
            if (hit.hp > 0) {
                return false;
            }
            const core::Point cell = BattleField::cellPosition(hit.row, hit.col);
            hit.position = {cell.x + BattleCamera::kRest.x - camera.x, cell.y + BattleCamera::kRest.y - camera.y};
            hit.placed = true;
            hit.jumping = false;
            hit.jumpHeight = 0;
            hit.action = facing(hit, 26); // action 26: down (dead)
            hit.fall();
            hit.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
            return true;
        }

        /// @brief Adds or takes away an amount of the fighter's HP or SP, kept within 0 and its maximum.
        /// @details Original: AdjustFighterAttr; values 1 and 2 take away.
        /// @param fighter Fighter changed.
        /// @param attribute Attribute id (HP 25, SP 26).
        /// @param negative 1 or 2 take away, else add.
        /// @param amount Size of the change.
        void adjust(Fighter& fighter, int attribute, int negative, std::int32_t amount) {
            const std::int32_t change = negative == 1 || negative == 2 ? -amount : amount; // negative 1 and 2: the value takes away
            if (attribute == kHp) {
                fighter.hp = std::clamp(fighter.hp + change, 0, fighter.maxHp);
            } else if (attribute == kSp) {
                fighter.sp = std::clamp(fighter.sp + change, 0, fighter.maxSp);
            }
        }

        /// @brief The weapon sounds' row for an item type.
        /// @details Original: WeaponGroup.
        /// @param itemType Item type of the weapon.
        /// @return Weapon sound group 1..5.
        int weaponGroup(int itemType) {
            // the returned numbers are the weapon sound groups (rows of the motion's weapon sounds)
            switch (itemType) {
                case 1: // item type 1 (knives): group 1
                case 2: // item type 2 (swords): group 1
                case 3: // item type 3 (spears): group 1
                case 6: // item type 6 (claws): group 1
                case 8: // item type 8 (axes): group 1
                    return 1;
                case 4: // item type 4 (bows): group 4
                    return 4;
                case 5: // item type 5 (staves, wands): group 3
                case 10: // item type 10 (fans): group 3
                    return 3;
                case 11: // item type 11 (guns): group 5
                    return 5;
                default: // any other item type: group 2
                    return 2;
            }
        }

        /// @brief Reads a record's sound entry: u16 sound number, i32 when.
        /// @param bytes The 7 bytes of the sound entry.
        /// @return The sound number and its delay.
        std::pair<int, int> soundOf(const std::array<std::uint8_t, 7>& bytes) {
            // little-endian: u16 number in bytes 0-1 (shift 8), i32 delay in bytes 2-5 (shifts 8, 16, 24)
            const int number = bytes[0] | (bytes[1] << 8);
            const int delay = static_cast<int>(
                static_cast<std::uint32_t>(bytes[2]) | (static_cast<std::uint32_t>(bytes[3]) << 8) | (static_cast<std::uint32_t>(bytes[4]) << 16) |
                (static_cast<std::uint32_t>(bytes[5]) << 24)
            );
            return {number, delay};
        }

        /// @brief The body sounds' row of a fighter: a player's body type or an NPC kind's own.
        /// @param fighter Fighter asked about.
        /// @return The row, or nothing if the fighter has none.
        std::optional<int> bodyRow(const Fighter& fighter) {
            if (fighter.look) {
                return fighter.look->bodyType;
            }
            // NPC direction kinds 0 and 2 are the ones with a body sounds row
            if (fighter.npc && (fighter.npc->directionKind == 0 || fighter.npc->directionKind == 2)) {
                return fighter.npc->bodyType;
            }
            return std::nullopt;
        }

    }

    SkillAction::SkillAction(
        ActionContext& context,
        int attackerSlot,
        int targetSlot,
        int skillId
    )
        : m_attacker(attackerSlot),
          m_target(targetSlot),
          m_skillId(skillId) {
        m_motion = context.motions.find(skillId);
        m_skill = context.skills.find(skillId);
        const auto attackerFound = context.field.fighter(attackerSlot);
        const auto targetFound = context.field.fighter(targetSlot);
        if (!m_motion || !m_skill || !attackerFound || !targetFound || attackerFound->get().hp <= 0) {
            m_done = true;
            return;
        }
        if (const int hitId = m_motion->get().hitMotionId) {
            m_hitMotion = context.motions.find(hitId);
        }
        Fighter& a = *attackerFound;
        m_attackerSide = a.side();
        m_targetSide = targetFound->get().side();
        a.skillSounds.clear();
        a.hitSounds.clear();
        if (skillId == kFlee && m_skill->skillType == kFleeType && !a.isPlayer() && a.kind != 4) { // kind 4: pet
            context.notice(a.name + " escaped from the combat");
        }
        setUpCaster(context);
        markTargets(context);
        setUpHitMotions(context);
        a.casting = true;
        a.mover.restart(context.nowMs);
        constexpr int kFreeCategory = 8; // skill category 8: costs no SP
        if (m_skill->category != kFreeCategory) {
            adjust(a, kSp, 1, m_skill->spCost);
        }
        if (context.skillZoomOn && a.cameraFollow) {
            m_attackSpan = std::abs(a.position.x - targetFound->get().position.x);
        }
    }

    Fighter& SkillAction::attacker(ActionContext& context) const { return *context.field.fighter(m_attacker); }

    Fighter& SkillAction::target(ActionContext& context) const { return *context.field.fighter(m_target); }

    int SkillAction::drawPosition(const formats::MotionRecord& record) {
        for (std::size_t i = 0; i < record.drawOrder.size(); ++i) {
            if (record.drawOrder[i] == 1) {
                return static_cast<int>(i) + 1;
            }
        }
        return 1;
    }

    bool SkillAction::due(int point, int delay, std::uint64_t sinceMs, std::uint64_t nowMs) const {
        if (delay > 0 && delay < 101) {
            return point >= delay;
        }
        return static_cast<double>(nowMs - sinceMs) >= delay;
    }

    void SkillAction::setUpCaster(ActionContext& context) {
        Fighter& a = attacker(context);
        Fighter& t = target(context);
        if (a.cameraFollow) {
            t.effectMotionId = m_skillId;
        }
        core::Point move;
        const int type = m_skill->skillType;
        const int targetType = m_skill->targetType;
        if (type == kFleeType) {
            move = {};
            // skill target types 1, 3 and 5; skill types 1 and 2 run to the target
        } else if (targetType == 1 || targetType == 3 || targetType == 5) {
            if (type == 1 || type == 2) {
                move = {t.position.x < a.position.x ? t.position.x - a.position.x : std::abs(a.position.x - t.position.x), a.position.y - t.position.y};
            }
        } else if (t.side() == a.side()) {
            move = {std::abs(a.position.x - t.position.x), a.position.y - t.position.y};
        } else {
            move = {std::abs(a.position.x - t.position.x) - kEnemyOffset, a.position.y - t.position.y};
        }
        const formats::MotionRecord& record = *m_motion;
        loadCasterLayers(context, a, move);
        loadCasterSounds(context, a);
        a.skillMotion.placeAt(BattleField::cellPosition(a.row, a.col), context.camera.position());
        a.skillMotion.load(record, a.side(), move, true);
        if (a.cameraFollow) {
            context.camera.loadPath(record.camera, a.side());
        }
        m_casterDrawPosition = drawPosition(record);
        if (m_hitMotion) {
            m_hitDrawPosition = drawPosition(*m_hitMotion);
        }
        a.skillMotion.placeAt(a.position, context.camera.position());
        m_stage = 1;
        m_returnStage = 0;
        a.skillMotion.reset(context.nowMs);
        for (std::size_t i = 1; i <= record.layers.size() && i <= Fighter::kLayers; ++i) {
            a.skillLayers[i].reset(context.nowMs);
        }
        a.skillSounds.restart(context.nowMs);
        m_effectsActive = true;
        if (a.cameraFollow) {
            context.camera.resetPath(context.nowMs);
        }
        m_hitMs = context.nowMs;
        m_effectMs = context.nowMs;
        m_effectTicks.fill(context.nowMs);
    }

    void SkillAction::loadCasterLayers(ActionContext& context, Fighter& caster, core::Point move) {
        const formats::MotionRecord& record = *m_motion;
        const core::Point camera = context.camera.position();
        const int side = caster.side();
        for (std::size_t i = 0; i < record.layers.size() && i < Fighter::kLayers; ++i) {
            const formats::MotionLayer& layer = record.layers[i];
            std::vector<core::Point> points(1);
            for (const core::Point& point : layer.points) {
                if (side == 1) {
                    points.push_back({caster.position.x - point.x - move.x + camera.x, point.y + caster.position.y - move.y + camera.y});
                } else {
                    points.push_back({point.x + caster.position.x + move.x + camera.x, point.y + caster.position.y - move.y + camera.y});
                }
            }
            int direction = layer.direction;
            // directions 1 and 2 are the L / R pictures, swapped on side 1 (3 - direction)
            if (side == 1 && (direction == 1 || direction == 2)) {
                direction = 3 - direction;
            }
            caster.skillLayers[i + 1].load(layer, std::move(points), direction, camera);
            context.pictures.prefetch(caster.skillLayers[i + 1].effectPicture(), EffectLayer::kPictureTint); // decoded on a worker before the layer starts
            if (caster.skillLayers[i + 1].isCutIn()) {
                caster.cutIn = SkillCutIn{true, std::nullopt, 0, 0};
            }
        }
    }

    void SkillAction::loadCasterSounds(ActionContext& context, Fighter& caster) {
        const formats::MotionRecord& record = *m_motion;
        constexpr int kSoundNumbers = 10000; // sound numbers are taken modulo this; the sound file number is the remainder
        for (const auto& bytes : record.sounds) {
            const auto [number, delay] = soundOf(bytes);
            const std::string file = number % kSoundNumbers != 0 ? context.soundFile(number % kSoundNumbers) : std::string();
            if (!file.empty()) {
                caster.skillSounds.addSound(file, delay);
            }
        }
        const auto row = bodyRow(caster);
        if (!row || *row < 0 || *row >= static_cast<int>(record.bodySounds.size())) {
            return;
        }
        for (int i = 1; i <= 5; ++i) { // five body sound slots
            const int number = record.bodySounds[static_cast<std::size_t>(*row)][static_cast<std::size_t>(i)] % kSoundNumbers;
            const std::string file = number != 0 ? context.soundFile(number) : std::string();
            if (!file.empty()) {
                caster.skillSounds.setBodySound(i, file, record.bodySoundDelays[static_cast<std::size_t>(i)]);
            }
        }
    }

    void SkillAction::markTargets(ActionContext& context) {
        const bool revives = m_skill->skillType == kReviveType;
        const auto mark = [&](int row, int col) {
            const auto found = context.field.fighter(context.field.slotAt(row, col));
            if (found && (revives || found->get().hp > 0) && !found->get().waiting) {
                found->get().isHit = true;
            }
        };
        Fighter& t = target(context);
        constexpr int kAnyHp = 15244; // skill 15244: Rest, which may target a fighter at any HP
        // battle grid: rows 1..4 (1-2 one side, 3-4 the other; 1 and 4 the back rows, 2 and 3 the front rows) by columns 1..4
        switch (attacker(context).attackArea) {
            case 0: // area 0: the target only
                if ((revives || t.hp > 0 || m_skillId == kAnyHp) && !t.waiting) {
                    t.isHit = true;
                }
                break;
            // area 1: the target and the one behind or before it
            case 1:
                if (t.row == 1 || t.row == 4) {
                    if ((revives || t.hp > 0) && !t.waiting) {
                        t.isHit = true;
                        mark(t.row == 1 ? 2 : 3, t.col);
                    }
                } else {
                    mark(t.row, t.col);
                    mark(t.row == 2 ? 1 : 4, t.col);
                }
                break;
            // area 2: the target's column and its two neighbours in the row
            case 2:
            // area 3: area 2 and the one behind or before
            case 3:
                for (int col = t.col - 1; col <= t.col + 1; ++col) {
                    if (col >= 1 && col <= 4) {
                        mark(t.row, col);
                    }
                }
                if (attacker(context).attackArea == 3) {
                    mark(t.row == 1 || t.row == 3 ? t.row + 1 : t.row - 1, t.col);
                }
                break;
            // area 4: the whole row
            case 4:
                for (int col = 1; col <= 4; ++col) {
                    mark(t.row, col);
                }
                break;
            // area 5: a front row's target and the three behind
            case 5:
                mark(t.row, t.col);
                if (t.row == 2 || t.row == 3) {
                    for (int col = t.col - 1; col <= t.col + 1; ++col) {
                        if (col >= 1 && col <= 4) {
                            mark(t.row == 2 ? 1 : 4, col);
                        }
                    }
                }
                break;
            // area 6: three columns of both rows of the side
            case 6:
                for (int col = t.col - 1; col <= t.col + 1; ++col) {
                    if (col >= 1 && col <= 4) {
                        mark(t.row <= 2 ? 1 : 3, col);
                        mark(t.row <= 2 ? 2 : 4, col);
                    }
                }
                break;
            // area 7: the whole side
            case 7:
                for (int col = 1; col <= 4; ++col) {
                    mark(t.side() == 1 ? 3 : 1, col);
                    mark(t.side() == 1 ? 4 : 2, col);
                }
                break;
            default: // any other area: nobody extra
                break;
        }
    }

    void SkillAction::setUpHitMotions(ActionContext& context) {
        Fighter& a = attacker(context);
        const int recordHit = m_motion->get().hitMotionId;
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = context.field.fighter(slot);
            if (!found || !found->get().isHit || found->get().waiting) {
                continue;
            }
            Fighter& hit = *found;
            const bool rewritten = a.pendingHit60011 || a.pendingHit65090;
            if (!rewritten) {
                applyEffect(context, hit);
            }
            int motionId = 0;
            int statusMotion = 0;
            if (hit.statuses.main[2].effect != 0) { // main[2]: the second status slot
                if (const auto status = context.skills.find(hit.statuses.main[2].skill)) {
                    statusMotion = status->motionId;
                }
            }
            if (m_attackerSide == m_targetSide) {
                const int targetType = m_skill->targetType;
                // same side: only skill target types 1, 2, 3 and 5 (ally skills) reach the target
                if (targetType != 1 && targetType != 2 && targetType != 3 && targetType != 5) {
                    continue;
                }
                motionId = recordHit != 0 && context.motions.find(recordHit) ? recordHit : 0;
                hit.hitMotionId = motionId;
            } else {
                const int targetType = m_skill->targetType;
                // other side: only skill target types 0 and 2 (enemy skills) reach the target
                if (targetType != 0 && targetType != 2) {
                    continue;
                }
                if (hit.hitAvoided) {
                    motionId = kGuardMotion;
                    if (hit.statuses.main[2].effect != 0) { // main[2]: the second status slot
                        motionId = statusMotion;
                        hit.hitMotionId = statusMotion;
                    }
                } else {
                    // main[2]: the second status slot
                    motionId = hit.statuses.main[2].effect != 0 ? statusMotion : recordHit;
                    if (recordHit != 0 && hit.statuses.main[2].effect != 0 && a.pendingHit65090) {
                        motionId = kThrownHitMotion;
                        hit.hitEffectOn = true;
                    } else if (recordHit != 0 && hit.statuses.main[2].effect == 0 && a.pendingHit60011) {
                        motionId = kKnockedHitMotion;
                        hit.hitEffectOn = true;
                    }
                    hit.hitMotionId = motionId;
                    // kind 4: pet; NPC direction kinds 2, 3, 5 and 6 stand stiff when hit; skill type 11 is exempt
                    const bool stiff =
                        hit.npc && !hit.isPlayer() && hit.kind != 4 && hit.npc->uncatchable == 1 &&
                        (hit.npc->directionKind == 2 || hit.npc->directionKind == 3 || hit.npc->directionKind == 5 || hit.npc->directionKind == 6);
                    if (stiff && m_skill->skillType != 11) {
                        motionId = kKnockedMotion;
                        hit.hitMotionId = kKnockedMotion;
                    }
                }
            }
            if (a.cameraFollow) {
                hit.hitAttacker = m_attacker;
                prepareHitMotion(context, slot, motionId);
            }
            setReactFlag(context, slot);
        }
    }

    void SkillAction::applyEffect(ActionContext& context, Fighter& hit) {
        Fighter& a = attacker(context);
        const auto found =
            std::find_if(a.effects.rbegin(), a.effects.rend(), [&](const TargetEffect& effect) { return effect.row == hit.row && effect.col == hit.col; });
        if (found == a.effects.rend()) {
            return;
        }
        TargetEffect effect = *found;
        a.effects.erase(std::next(found).base());
        const bool wasAlive = hit.hp > 0;
        switch (effect.guard) {
            case 0: // guard 0: not guarded
                hit.hitEffectOn = true;
                break;
            case 1: // guard 1: the defence pose (motion 60021)
                hit.hitMotionId = kGuardMotion;
                hit.hitEffectOn = true;
                hit.hitAvoided = true;
                break;
            case 2: // guard 2: the guard pose (motion 60031)
                hit.hitMotionId = kBlockMotion;
                hit.hitEffectOn = true;
                hit.hitAvoided = true;
                break;
            default: // other guard values (4: the values at once) set no pose
                break;
        }
        ++effect.hitsDone;
        if (!effect.hit) {
            if (effect.skillId == kFailToFlee) {
                a.texts.add("Fail", kTextKind, hit.position, {hit.position.x, hit.position.y - kTextRise}, kTextSpeed, kHpColour, context.nowMs);
            }
            hit.missedBy[static_cast<std::size_t>(m_attacker)] = true;
            hit.hitAvoided = true;
            return;
        }
        if (const auto cure = context.skills.find(effect.skillId);
            cure && (cure->skillType == 5 || cure->skillType == 16)) { // skill types 5 and 16: curing skills
            cureStatus(hit.statuses, context.skills, effect.skillId);
            return;
        }
        std::int32_t hpTotal = 0;
        std::int32_t spTotal = 0;
        for (TargetEffect::Value& value : effect.values) {
            if (value.attribute == 0) {
                continue;
            }
            const std::int32_t first = effect.values.empty() ? 0 : effect.values.front().total;
            const StatusValueResult status = applyStatusValue(hit.statuses, context.skills, value.attribute, effect.skillId, first);
            if (status == StatusValueResult::Applied) {
                continue;
            }
            if (status == StatusValueResult::BecomesHp) {
                value.attribute = kHp;
            }
            value.hit = value.left;
            value.left = 0;
            if (a.cameraFollow) {
                hit.pending.push_back(value);
            }
            if (value.attribute == kHp) {
                hpTotal += value.total;
            } else if (value.attribute == kSp) {
                spTotal += value.total;
            }
        }
        constexpr int kAtOnce = 4; // guard value 4: the effect applies at once
        if (effect.guard != kAtOnce) {
            hit.numbers.split(splitMotion(context), hpTotal, spTotal);
        }
        std::int32_t newHp = hit.hp;
        for (const TargetEffect::Value& value : effect.values) {
            if (value.attribute == kHp) {
                newHp += value.negative == 1 || value.negative == 2 ? -value.hit : value.hit;
            }
        }
        if (newHp <= 0) {
            const bool ownPet = hit.kind == 4 && hit.ownerId == context.ownId; // kind 4: pet
            if (wasAlive && ownPet) {
                context.chatLine(hit.name + " is killed by " + a.name);
            }
            hit.killed = true;
            hit.dead = true;
            // action 26: down (dead)
            hit.action = facing(hit, 26);
            hit.fall();
        }
        if (effect.guard != kAtOnce) {
            return;
        }
        for (const TargetEffect::Value& value : hit.pending) {
            adjust(hit, value.attribute, value.negative, value.hit);
            if (value.attribute == kHp || value.attribute == kSp) {
                // 0.12: speed the number rises at; 9: SP colour index (kSpColour)
                hit.texts.add(
                    std::to_string(value.hit),
                    kTextKind,
                    hit.position,
                    {hit.position.x, hit.position.y - kTextRise},
                    0.12,
                    value.attribute == kHp ? kHpColour : 9, // 9: SP colour index (kSpColour)
                    context.nowMs
                );
            }
        }
    }

    const formats::MotionRecord& SkillAction::splitMotion(ActionContext& context) const {
        if (const int lead = leadSlot(context); lead != 0) {
            if (const auto record = context.motions.find(context.field.fighter(lead)->get().cmdSkillId)) {
                return record->get();
            }
        }
        return *m_motion;
    }

    int SkillAction::leadSlot(ActionContext& context) {
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            if (const auto found = context.field.fighter(slot); found && found->get().cameraFollow) {
                return slot;
            }
        }
        return 0;
    }

    void SkillAction::holdHitFighters(ActionContext& context) {
        const int lead = leadSlot(context);
        if (lead != 0 && context.actionStage) {
            if (const std::optional<int> stage = context.actionStage(lead); stage && *stage > 1) { // stage 1 is before the hit; later stages are past it
                return;
            }
        }
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = context.field.fighter(slot);
            if (found && found->get().isHit && found->get().hitEffectOn && slot != m_attacker && found->get().hp > 0) { // a fallen fighter keeps lying
                found->get().action = facing(found->get(), 30); // action 30: the hit pose
                found->get().holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
            }
        }
    }

    void SkillAction::setReactFlag(ActionContext& context, int slot) {
        const Fighter& hit = *context.field.fighter(slot);
        bool react = m_reacts[static_cast<std::size_t>(slot)];
        const auto hitSkill = context.skills.find(hit.hitMotionId);
        constexpr int kDefenceType = 10; // skill type 10: defence skills
        if (hit.statuses.main[2].effect != 0) {
            switch (hit.statuses.main[2].effect) { // main[2]: the second status slot
                case 101: // second-status effect 101: no reaction
                case 103: // second-status effect 103: no reaction
                    react = false;
                    break;
                case 104: // second-status effect 104: reacts
                    react = true;
                    break;
                default: // other second-status effects leave the flag as it was
                    break;
            }
        } else if (hitSkill && hitSkill->skillType == kDefenceType) {
            react = true;
        } else if (hit.hitMotionId == kBlockMotion) {
            react = false;
        } else {
            const int type = m_skill->skillType;
            react = type == 1 || type == 2 || type == 6 || type == 7 || type == 13 || type == 17; // skill types whose hits react
        }
        if (hit.missedBy[static_cast<std::size_t>(m_attacker)]) {
            react = false;
        }
        if (const auto reached = context.field.fighter(hit.hitAttacker); reached && (reached->get().pendingHit60011 || reached->get().pendingHit65090)) {
            react = false;
        }
        m_reacts[static_cast<std::size_t>(slot)] = react;
    }

    void SkillAction::prepareHitMotion(ActionContext& context, int slot, int motionId) {
        const auto record = context.motions.find(motionId);
        if (!record) {
            return;
        }
        loadMotion(context, *context.field.fighter(slot), *record, false);
    }

    void SkillAction::loadMotion(ActionContext& context, Fighter& hit, const formats::MotionRecord& motion, bool skillSide) {
        FighterMotion& into = skillSide ? hit.skillMotion : hit.hitMotion;
        auto& layers = skillSide ? hit.skillLayers : hit.hitLayers;
        BattleSounds& sounds = skillSide ? hit.skillSounds : hit.hitSounds;
        const core::Point camera = context.camera.position();
        into.placeAt(hit.position, camera);
        into.load(motion, hit.side(), {}, false);
        for (std::size_t i = 0; i < motion.layers.size() && i < Fighter::kLayers; ++i) {
            const formats::MotionLayer& layer = motion.layers[i];
            std::vector<core::Point> points(1);
            for (const core::Point& point : layer.points) {
                if (hit.side() == 1) {
                    points.push_back({hit.position.x - point.x + camera.x, point.y + hit.position.y + camera.y});
                } else {
                    points.push_back({point.x + hit.position.x + camera.x, point.y + hit.position.y + camera.y});
                }
            }
            int direction = layer.direction;
            // directions 1 and 2 are the L / R pictures, swapped on side 1 (3 - direction)
            if (hit.side() == 1 && (direction == 1 || direction == 2)) {
                direction = 3 - direction;
            }
            layers[i + 1].load(layer, std::move(points), direction, camera);
            context.pictures.prefetch(layers[i + 1].effectPicture(), EffectLayer::kPictureTint); // decoded on a worker before the layer starts
        }
        // layers this motion does not have are cleared: one left from an earlier action (a held crack, a status motion with fewer
        // layers than the skill's hit motion) would otherwise be drawn again by this action at the screen place it had then
        for (std::size_t i = motion.layers.size() + 1; i <= Fighter::kLayers; ++i) {
            layers[i] = EffectLayer{};
        }
        constexpr int kSoundNumbers = 10000; // sound numbers are taken modulo this; the sound file number is the remainder
        for (const auto& bytes : motion.sounds) {
            const auto [number, delay] = soundOf(bytes);
            const std::string file = number % kSoundNumbers != 0 ? context.soundFile(number % kSoundNumbers) : std::string();
            if (!file.empty()) {
                sounds.addSound(file, delay);
            }
        }
        const Fighter& a = attacker(context);
        int itemType = a.weaponType;
        if (a.npc && !a.isPlayer() && !(a.kind == 4 && a.npc->directionKind != 1)) { // kind 4: pet
            itemType = a.npc->weaponType;
        }
        const int group = weaponGroup(itemType);
        for (int i = 1; i <= 5; ++i) { // five weapon sound slots
            const int number = motion.weaponSounds[static_cast<std::size_t>(group)][static_cast<std::size_t>(i)] % kSoundNumbers;
            const std::string file = number != 0 ? context.soundFile(number) : std::string();
            if (!file.empty()) {
                sounds.setWeaponSound(i, file, motion.weaponSoundDelays[static_cast<std::size_t>(i)]);
            }
        }
        const auto row = bodyRow(hit);
        if (row && *row >= 0 && *row < static_cast<int>(motion.bodySounds.size())) {
            for (int i = 1; i <= 5; ++i) { // five body sound slots
                const int number = motion.bodySounds[static_cast<std::size_t>(*row)][static_cast<std::size_t>(i)] % kSoundNumbers;
                const std::string file = number != 0 ? context.soundFile(number) : std::string();
                if (!file.empty()) {
                    sounds.setBodySound(i, file, m_motion->get().bodySoundDelays[static_cast<std::size_t>(i)]);
                }
            }
        }
    }

    void SkillAction::updateCamera(ActionContext& context) {
        Fighter& a = attacker(context);
        if (m_hitApplied) {
            constexpr double kHitCameraSpeed = 0.15; // camera speed back to rest after a hit, px per ms
            context.camera.startMove(context.camera.position(), BattleCamera::kRest);
            context.camera.step(kHitCameraSpeed, context.nowMs);
            return;
        }
        switch (m_motion->get().cameraMode) {
            case 0: // camera mode 0: the camera follows the attacker
                if (m_returnStage == 0) {
                    a.skillMotion.setCameraFollow(true);
                }
                break;
            case 1: // camera mode 1: the jumping camera
                context.camera.jumpStep({}, a.skillMotion.pointIndex(), context.nowMs);
                break;
            case 2: // camera mode 2: the camera follows the record's path
                context.camera.followPath(a.skillMotion.pointIndex(), context.nowMs);
                break;
            default: // other camera modes: the camera stays
                break;
        }
    }

    void SkillAction::placeIdleFighters(ActionContext& context) {
        const core::Point camera = context.camera.position();
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = context.field.fighter(slot);
            if (!found || found->get().casting) {
                continue;
            }
            Fighter& fighter = *found;
            if (fighter.hp > 0) { // a fallen fighter keeps its down pose while the camera moves
                fighter.action = stance(fighter);
            }
            const core::Point cell = BattleField::cellPosition(fighter.row, fighter.col);
            fighter.position = {cell.x + BattleCamera::kRest.x - camera.x, cell.y + BattleCamera::kRest.y - camera.y};
        }
    }

    void SkillAction::stepCasterPoints(ActionContext& context, Fighter& caster) {
        FighterMotion& motion = caster.skillMotion;
        caster.jumping = false;
        motion.advancePoint(context.nowMs);
        if (std::abs(motion.screen().y - motion.home().y) > 0) {
            caster.jumping = true;
            caster.jumpHeight = std::abs(motion.screen().y - motion.home().y);
            caster.position = motion.home();
        } else {
            caster.position = motion.screen();
            caster.jumping = false;
            caster.jumpHeight = 0;
        }
        const bool over = motion.pathDone() && motion.pathEndReached() && motion.pointIndex() > motion.lastPointIndex();
        const int point = over ? motion.pointIndex() - 1 : motion.pointIndex();
        caster.action = motion.frameSlot(point);
        caster.holdFrame = motion.frameHold(point);
        caster.mover.setSpeed(motion.slope(point));
    }

    void SkillAction::updateCasterMotion(ActionContext& context) {
        Fighter& a = attacker(context);
        FighterMotion& motion = a.skillMotion;
        BattleCamera& camera = context.camera;
        constexpr int kCloseRange = 1; // skill range kind 1: the attacker runs up to its target
        if (context.skillZoomOn && a.cameraFollow && m_skill->rangeKind == kCloseRange && context.skillZoom) {
            context.skillZoom(m_attackSpan, motion.position().x, target(context).position.x + camera.position().x, motion.position().y);
        }
        for (int index = 1; index <= kScreenEffects; ++index) {
            if (m_effectStates[static_cast<std::size_t>(index)] == 3) { // effect state 3: ghosts (after images) on
                motion.leaveAfterImage(a, context.nowMs);
            }
        }
        if (m_returnStage == 1) {
            if (m_skill->skillType == kFleeType) {
                motion.setRunning(true);
                m_returnStage = 3;
            } else {
                if (camera.atRest()) {
                    motion.setCameraFollow(false);
                }
                motion.setSpeedMode(2); // speed mode 2: return home
                motion.returnHome(a, true, camera, context.nowMs);
                if (!a.cameraFollow && m_stage != 3) {
                    m_stage = 3;
                }
                if (motion.arrived()) {
                    a.action = stance(a);
                    a.jumping = false;
                    a.jumpHeight = 0;
                    if (!a.cameraFollow) {
                        motion.setSpeedMode(4); // speed mode 4: back home, the motion is over
                        motion.setRunning(true);
                        finishSkill(context);
                    }
                    a.position = {motion.start().x - camera.position().x, motion.start().y - camera.position().y};
                    a.placed = true; // home: drawn there at once, not slid in by the tween
                    if (a.cameraFollow) {
                        camera.restartTicks(context.nowMs);
                    }
                    m_returnStage = 2;
                    a.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
                }
            }
        } else if (m_returnStage == 2) {
            if (a.cameraFollow) {
                if (camera.atRest()) {
                    motion.setSpeedMode(4); // speed mode 4: back home, the motion is over
                    motion.setRunning(true);
                    m_returnStage = 3;
                } else {
                    camera.startMove(camera.position(), BattleCamera::kRest);
                    camera.step(kReturnCameraSpeed, context.nowMs);
                    placeIdleFighters(context);
                }
            }
        } else if (m_returnStage == 0) {
            context.sleep(motion.holdSleepMs());
            motion.advancePath(camera, a.cameraFollow, a.jumpHeight, context.nowMs);
            motion.calcDrawPosition();
            if (motion.speedMode() == 0) { // speed mode 0: run to the start point
                motion.runToStart(a, true);
            } else if (motion.speedMode() == 1) { // speed mode 1: step through the record's poses
                stepCasterPoints(context, a);
            }
            if (motion.pathEndReached() && motion.pathDone()) {
                m_returnStage = 1;
                motion.restartMove(context.nowMs);
                motion.clearArrived();
                a.jumping = false;
                a.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
                camera.setSkillCamera(true);
                camera.endPathAtRest();
                if (a.cameraFollow) {
                    applyCasts(context);
                }
            }
        }
        const formats::MotionRecord& record = a.specialHitStarted && m_knockedMotion ? m_knockedMotion->get() : m_motion->get();
        for (std::size_t i = 1; i <= record.layers.size() && i <= Fighter::kLayers; ++i) {
            a.skillLayers[i].update(context.pictures, motion.pointIndex(), camera.position(), context.nowMs);
        }
        a.skillSounds.schedule(motion.pointIndex(), context.nowMs);
    }

    void SkillAction::applyCasts(ActionContext& context) {
        constexpr int kCastRise = 150; // px a caster's HP / SP number rises
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = context.field.fighter(slot);
            if (!found || !found->get().cast) {
                continue;
            }
            Fighter& fighter = *found;
            const TargetEffect::Value value = *fighter.cast;
            adjust(fighter, value.attribute, value.negative, value.hit);
            if (value.attribute == kHp || value.attribute == kSp) {
                fighter.texts.add(
                    std::to_string(value.hit),
                    kTextKind,
                    fighter.position,
                    {fighter.position.x, fighter.position.y - kCastRise},
                    kTextSpeed,
                    value.attribute == kHp ? kHpColour : kSpColour,
                    context.nowMs
                );
            }
            fighter.cast.reset();
        }
    }

    bool SkillAction::inEffectWindow(ActionContext& context, int index, int frame) {
        const auto& effects = m_motion->get().screenEffects;
        if (index < 1 || index > static_cast<int>(effects.size())) {
            return false;
        }
        const formats::MotionScreenEffect& effect = effects[static_cast<std::size_t>(index - 1)];
        // 1..100 is a step number; larger values are milliseconds
        const auto inSteps = [](int value) { return value > 0 && value < 101; };
        const double since = static_cast<double>(context.nowMs - m_effectTicks[static_cast<std::size_t>(index)]);
        if (inSteps(effect.start)) {
            if (inSteps(effect.end)) {
                return effect.start <= frame && effect.end >= frame;
            }
            if (effect.start > frame) {
                return false;
            }
            if (!m_effectFlags[static_cast<std::size_t>(index)]) {
                m_effectTicks[static_cast<std::size_t>(index)] = context.nowMs;
                m_effectFlags[static_cast<std::size_t>(index)] = true;
            }
            return static_cast<double>(context.nowMs - m_effectTicks[static_cast<std::size_t>(index)]) <= effect.end;
        }
        if (inSteps(effect.end)) {
            return since >= effect.start && effect.end >= frame;
        }
        return since >= effect.start && since <= effect.end;
    }

    void SkillAction::startShake(ActionContext& context, int index) {
        m_effectStates[static_cast<std::size_t>(index)] = 1;
        // offset of up to 20 pixels each way (20 - Random(40) + 1)
        context.field.effects.shake = {20 - randomBelow(m_random, 40) + 1, 20 - randomBelow(m_random, 40) + 1};
    }

    void SkillAction::updateFade(ActionContext& context, int index) {
        ScreenEffects& screen = context.field.effects;
        if (m_effectStates[static_cast<std::size_t>(index)] != 2) {
            return;
        }
        // fade level 255 is full brightness; going down by 7..11 a step to a floor of 140, up by 5 a step
        if (m_fadeUp && screen.fadeLevel >= 255) {
            m_effectStates[static_cast<std::size_t>(index)] = 0;
            screen.fadeLevel = 255;
            m_fadeUp = false;
            m_fadeDown = false;
            screen.fading = false;
        }
        if (m_fadeDown) {
            screen.fadeLevel -= randomBelow(m_random, 5) + 7;
        } else if (m_fadeUp) {
            screen.fadeLevel = 255 - screen.fadeLevel < 5 ? 255 : std::min(screen.fadeLevel + 5, 255);
        }
        if (m_fadeDown && screen.fadeLevel < 140) {
            screen.fadeLevel = 140;
            m_fadeUp = true;
            m_fadeDown = false;
        }
    }

    void SkillAction::updateScreenEffects(ActionContext& context) {
        ScreenEffects& screen = context.field.effects;
        const int frame = attacker(context).skillMotion.pointIndex();
        const auto& effects = m_motion->get().screenEffects;
        for (int index = 1; index <= static_cast<int>(effects.size()); ++index) {
            const std::size_t at = static_cast<std::size_t>(index);
            switch (effects[at - 1].kind) {
                case 1: // screen effect 1: shake
                    if (inEffectWindow(context, index, frame)) {
                        screen.shaking = true;
                        startShake(context, index);
                    } else {
                        screen.shaking = false;
                        screen.shake = {};
                        m_effectStates[at] = 0;
                    }
                    break;
                case 2: // screen effect 2: fade
                    if (inEffectWindow(context, index, frame)) {
                        m_effectStates[at] = 2;
                        screen.fading = true;
                        m_fadeDown = true;
                        m_fadeUp = false;
                        updateFade(context, index);
                    } else if (m_effectStates[at] == 2) {
                        m_fadeDown = false;
                        m_fadeUp = true;
                        updateFade(context, index);
                    }
                    break;
                case 3: // screen effect 3: ghosts (after images)
                    if (inEffectWindow(context, index, frame)) {
                        m_effectStates[at] = 3;
                        if (!m_afterImages) {
                            attacker(context).skillMotion.clearAfterImages(context.nowMs);
                            m_afterImages = true;
                        }
                    } else if (m_effectStates[at] == 3) {
                        m_effectStates[at] = 0;
                        m_afterImages = false;
                    }
                    break;
                default: // other screen effect kinds do nothing
                    break;
            }
        }
    }

    void SkillAction::processHit(ActionContext& context) {
        Fighter& a = attacker(context);
        if (!due(a.skillMotion.pointIndex(), m_motion->get().hitTime, m_hitMs, context.nowMs)) {
            return;
        }
        if (a.cameraFollow) {
            for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
                const auto found = context.field.fighter(slot);
                if (!found || !found->get().isHit || found->get().waiting || m_stage != 1) {
                    continue;
                }
                Fighter& hit = *found;
                // a fighter this hit kills still flies back and comes home (original EndFighterHitMotion); only one that was down
                // before the hit is kept lying on its cell
                m_downBeforeHit[static_cast<std::size_t>(slot)] = hit.hp <= 0;
                hit.hitMotion.reset(context.nowMs);
                if (m_hitMotion) {
                    for (std::size_t i = 1; i <= m_hitMotion->get().layers.size() && i <= Fighter::kLayers; ++i) {
                        hit.hitLayers[i].reset(context.nowMs);
                    }
                }
                hit.hitSounds.restart(context.nowMs);
                const bool missed = hit.missedBy[static_cast<std::size_t>(m_attacker)];
                if (missed) {
                    a.texts.add("Miss", kTextKind, hit.position, {hit.position.x, hit.position.y - kTextRise}, kTextSpeed, kHpColour, context.nowMs);
                }
                const bool landed = (!hit.hitAvoided || hit.hitMotionId == kGuardMotion) && !missed;
                markStatusHit(hit.statuses, context.skills, m_skillId, landed, missed);
                if (a.pendingHit60011) {
                    a.pendingHit60011 = false;
                    startSpecialHit(context, kKnockedMotion);
                }
                if (a.pendingHit65090) {
                    a.pendingHit65090 = false;
                    startSpecialHit(context, kThrownMotion);
                }
            }
        }
        m_stage = 2;
    }

    void SkillAction::startSpecialHit(ActionContext& context, int motionId) {
        Fighter& a = attacker(context);
        a.specialHitStarted = true;
        a.skillSounds.clear();
        a.skillMotion.reset(context.nowMs);
        for (EffectLayer& layer : a.skillLayers) {
            layer = EffectLayer{};
        }
        m_knockedMotion = context.motions.find(kKnockedMotion);
        if (const auto record = context.motions.find(motionId)) {
            loadMotion(context, a, *record, true);
            a.skillMotion.reset(context.nowMs);
            for (std::size_t i = 1; i <= record->get().layers.size() && i <= Fighter::kLayers; ++i) {
                a.skillLayers[i].reset(context.nowMs);
            }
        }
        a.skillSounds.restart(context.nowMs);
        a.numbers.setHp(1, a.pendingHpDamage);
        a.numbers.showHp(1, a.position, context.camera.position().x, 0);
        adjust(a, kHp, 1, a.pendingHpDamage);
        a.pendingHpDamage = 0;
        m_hitApplied = true;
        context.camera.stopMoving();
    }

    void SkillAction::updateHitMotion(ActionContext& context, int slot) {
        Fighter& hit = *context.field.fighter(slot);
        if (!hit.hitEffectOn || !hit.isHit) {
            return;
        }
        if (hit.hitMotionId == 0) {
            if (slot != m_attacker) {
                hit.action = facing(hit, 30); // action 30: the hit pose
                hit.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
            }
            return;
        }
        FighterMotion& motion = hit.hitMotion;
        context.sleep(motion.holdSleepMs());
        motion.advancePath(context.camera, hit.cameraFollow, hit.jumpHeight, context.nowMs);
        motion.calcDrawPosition();
        if (motion.speedMode() == 0) { // speed mode 0: run to the start point
            motion.runToStart(hit, slot != m_attacker);
        } else if (motion.speedMode() == 1) { // speed mode 1: step through the record's poses
            motion.advancePoint(context.nowMs);
            if (slot == m_attacker) {
                return;
            }
            if (std::abs(motion.screen().y - motion.home().y) > 0) {
                hit.jumping = true;
                hit.jumpHeight = std::abs(motion.screen().y - motion.home().y);
                hit.position = motion.home();
            } else {
                hit.jumping = false;
                hit.jumpHeight = 0;
                hit.position = motion.screen();
            }
            // kind 4: pet; NPC direction kinds 2, 3, 5 and 6 stand stiff when hit; uncatchable == 1: cannot be captured
            const bool stiff = hit.npc && !hit.isPlayer() && hit.kind != 4 && hit.npc->uncatchable == 1 &&
                               (hit.npc->directionKind == 2 || hit.npc->directionKind == 3 || hit.npc->directionKind == 5 || hit.npc->directionKind == 6);
            if (stiff) {
                hit.action = facing(hit, 30); // action 30: the hit pose
                hit.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
            } else {
                hit.action = motion.frameSlot(motion.pointIndex());
                hit.holdFrame = motion.frameHold(motion.pointIndex());
                hit.mover.setSpeed(motion.slope(motion.pointIndex()));
            }
        }
    }

    void SkillAction::endHitMotion(ActionContext& context, int slot) {
        Fighter& hit = *context.field.fighter(slot);
        if (!hit.hitEffectOn || !hit.isHit) {
            return;
        }
        if (hit.hitMotionId == 0) {
            if (slot == m_attacker) {
                return;
            }
            hit.action = stance(hit);
            hit.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
            hit.isHit = false;
            hit.hitEffectOn = false;
            hit.hitMotion.setRunning(true);
            m_stage = 4;
            return;
        }
        FighterMotion& motion = hit.hitMotion;
        motion.setSpeedMode(2); // speed mode 2: return home
        motion.returnHome(hit, slot != m_attacker, context.camera, context.nowMs);
        if (!motion.arrived()) {
            return;
        }
        motion.setRunning(true);
        hit.isHit = false;
        hit.hitEffectOn = false;
        if (slot != m_attacker) {
            hit.action = stance(hit);
            // its cell as the drawn camera shows it now (motion.screen() counts the camera the motion stepped ahead to, a tick before
            // the drawn camera got there: the fighter popped off its cell for a tick at the end of each hit)
            const core::Point cell = BattleField::cellPosition(hit.row, hit.col);
            const core::Point camera = context.camera.position();
            hit.position = {cell.x + BattleCamera::kRest.x - camera.x, cell.y + BattleCamera::kRest.y - camera.y};
            hit.placed = true;
            hit.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
        }
        m_stage = 4;
    }

    void SkillAction::applyHitEffects(ActionContext& context, int slot) {
        Fighter& hit = *context.field.fighter(slot);
        const formats::MotionRecord& record = *m_motion;
        const int point = attacker(context).skillMotion.pointIndex();
        const int cameraX = context.camera.position().x;
        const int type = m_skill->skillType;
        const bool aid = type == 6 || type == 7 || type == 13; // skill types 6, 7 and 13: aid skills
        const auto apply = [&](int number) {
            hit.numbers.markShown(number);
            if (!hit.numbers.hpShown(number) || hit.numbers.spShown(number)) {
                return;
            }
            for (const TargetEffect::Value& value : hit.pending) {
                if (value.attribute == kHp) {
                    // number style: 1 for aid skills, 2 when negative is 2, else 0
                    hit.numbers.showHp(number, hit.position, cameraX, aid ? 1 : (value.negative == 2 ? 2 : 0));
                    adjust(hit, kHp, value.negative, hit.numbers.hpOf(number));
                } else if (value.attribute == kSp) {
                    hit.numbers.showSp(number, hit.position, cameraX, aid ? 1 : 0);
                    adjust(hit, kSp, value.negative, hit.numbers.spOf(number));
                }
            }
        };
        const int count = static_cast<int>(record.hits.size());
        switch (record.hitMode) {
            case 0: // hit mode 0: one number at the first hit's time
                if (count > 0 && due(point, record.hits[0].first, m_effectMs, context.nowMs)) {
                    apply(1);
                }
                break;
            case 1: // hit mode 1: all the numbers together at the first hit's time
                if (count > 0 && due(point, record.hits[0].first, m_effectMs, context.nowMs)) {
                    for (int number = 1; number <= std::min(count, HitNumbers::kHits); ++number) {
                        apply(number);
                    }
                }
                break;
            case 2: // hit mode 2: each number at its own hit's time
                for (int number = 1; number <= std::min(count, HitNumbers::kHits); ++number) {
                    if (due(point, record.hits[static_cast<std::size_t>(number - 1)].first, m_effectMs, context.nowMs)) {
                        apply(number);
                    }
                }
                break;
            default: // other hit modes show no numbers
                break;
        }
    }

    void SkillAction::updateStages(ActionContext& context) {
        Fighter& a = attacker(context);
        if (m_stage == 1) {
            processHit(context);
        }
        if (!a.cameraFollow && m_stage == 2) {
            holdHitFighters(context);
        }
        if (a.cameraFollow && m_stage == 2) {
            for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
                const auto found = context.field.fighter(slot);
                if (!found || !found->get().isHit || !found->get().hitEffectOn) {
                    continue;
                }
                updateHitMotion(context, slot);
                if (slot != m_attacker && m_downBeforeHit[static_cast<std::size_t>(slot)]) {
                    layFallen(*found, context.camera.position()); // the motion still steps (the stages wait for it), the body does not move
                }
                FighterMotion& motion = found->get().hitMotion;
                if (motion.pathEndReached() && motion.pathDone()) {
                    motion.restartMove(context.nowMs);
                    motion.clearArrived();
                    found->get().jumping = false;
                    m_stage = 3;
                }
            }
        }
        if (a.cameraFollow && m_stage == 3) {
            for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
                const auto found = context.field.fighter(slot);
                if (!found || !found->get().isHit || !found->get().hitEffectOn) {
                    continue;
                }
                if (m_skillId == kCapture) {
                    context.field.remove(slot);
                    continue;
                }
                endHitMotion(context, slot);
                Fighter& hit = *found;
                if (slot != m_attacker && m_downBeforeHit[static_cast<std::size_t>(slot)]) {
                    layFallen(hit, context.camera.position()); // no walk home and no stance for a fighter fallen before the hit
                }
                for (int number = 1; number <= HitNumbers::kHits; ++number) {
                    if (hit.numbers.spShown(number) || hit.numbers.hpOf(number) == 0) {
                        continue;
                    }
                    hit.numbers.showHp(number, hit.position, context.camera.position().x, 0);
                    if (!hit.pending.empty()) {
                        adjust(hit, hit.pending.front().attribute, hit.pending.front().negative, hit.numbers.hpOf(number));
                    }
                }
            }
        }
        if (a.cameraFollow && m_stage != 1) {
            for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
                const auto found = context.field.fighter(slot);
                if (!found || !found->get().isHit || !found->get().hitEffectOn) {
                    continue;
                }
                Fighter& hit = *found;
                if (m_hitMotion && hit.hitMotionId != 0) {
                    if (const auto own = context.motions.find(hit.hitMotionId)) {
                        for (std::size_t i = 1; i <= own->get().layers.size() && i <= Fighter::kLayers; ++i) {
                            hit.hitLayers[i].update(context.pictures, hit.hitMotion.pointIndex(), context.camera.position(), context.nowMs);
                        }
                    }
                }
                hit.hitSounds.schedule(hit.hitMotion.pointIndex(), context.nowMs);
                if (m_reacts[static_cast<std::size_t>(slot)]) {
                    applyHitEffects(context, slot);
                }
            }
        }
    }

    void SkillAction::checkFinished(ActionContext& context) {
        Fighter& a = attacker(context);
        if (!a.skillMotion.running()) {
            return;
        }
        if (m_motion->get().hitMotionId == 0 || !a.cameraFollow) {
            finishSkill(context);
            return;
        }
        bool finished = true;
        for (int slot = 1; slot <= BattleField::kSlots; ++slot) {
            const auto found = context.field.fighter(slot);
            if (found && found->get().isHit && found->get().hitEffectOn && !found->get().hitMotion.running()) {
                finished = false;
                break;
            }
        }
        if (m_skillId == kFlee || finished) {
            finishSkill(context);
        }
    }

    void SkillAction::finishSkill(ActionContext& context) {
        Fighter& a = attacker(context);
        a.skillMotion.setSpeedMode(0);
        m_effectsActive = false;
        m_returnStage = 0;
        m_stage = 0;
        a.casting = false;
        m_done = true;
    }

    void SkillAction::update(ActionContext& context) {
        if (!m_effectsActive || m_done) {
            return;
        }
        const auto attackerFound = context.field.fighter(m_attacker);
        const auto targetFound = context.field.fighter(m_target);
        if (!attackerFound || !targetFound) {
            m_done = true;
            return;
        }
        Fighter& a = *attackerFound;
        if (a.hp <= 0 && !a.specialHitStarted) {
            m_done = true;
            return;
        }
        a.casting = true;
        a.position = a.mover.step(a.position, BattleField::cellPosition(a.row, a.col), context.nowMs);
        if (a.cameraFollow) {
            updateCamera(context);
            placeIdleFighters(context);
        }
        updateCasterMotion(context);
        if (m_skill->skillType != kFleeType) {
            updateStages(context);
        }
        updateScreenEffects(context);
        checkFinished(context);
    }

    void SkillAction::finish(ActionContext& context) {
        for (const int slot : {m_attacker, m_target}) {
            const auto found = context.field.fighter(slot);
            if (!found) {
                continue;
            }
            Fighter& fighter = *found;
            if (slot == m_target && slot != m_attacker && fighter.isHit && fighter.hitEffectOn) {
                // another fighter's action leads and still moves this target along its hit motion: it puts the target back itself
                // (putting it on its cell here made it jump there for a tick each time one of several attackers finished)
                if (const int lead = leadSlot(context); lead != 0 && lead != m_attacker) {
                    continue;
                }
            }
            // the cell as the camera shows it now: another fighter's action may still hold the camera away from rest
            const core::Point cell = BattleField::cellPosition(fighter.row, fighter.col);
            const core::Point camera = context.camera.position();
            fighter.position = {cell.x + BattleCamera::kRest.x - camera.x, cell.y + BattleCamera::kRest.y - camera.y};
            fighter.placed = true;
            fighter.action = fighter.hp <= 0 ? facing(fighter, 26) : stance(fighter); // action 26: down (dead)
            if (fighter.hp <= 0) {
                fighter.fall();
            } else {
                fighter.stand();
            }
            fighter.holdFrame = 255; // 255: hold the pose (hold-frame sentinel)
        }
    }

    void SkillAction::drawCasterLayers(
        gfx::Canvas& canvas,
        Fighter& caster,
        bool front,
        core::Point shake,
        const std::function<void(Fighter&, const EffectLayer&)>& cutIn
    ) const {
        if (!m_effectsActive || !m_motion) {
            return;
        }
        const formats::MotionRecord& record = front && caster.specialHitStarted && m_knockedMotion ? m_knockedMotion->get() : m_motion->get();
        const int count = static_cast<int>(record.drawOrder.size());
        const int from = front ? m_casterDrawPosition + 1 : 1;
        const int to = front ? count : m_casterDrawPosition - 1;
        for (int j = from; j <= to; ++j) {
            const int layer = record.drawOrder[static_cast<std::size_t>(j - 1)] - 1;
            if (layer >= 1 && layer <= Fighter::kLayers) {
                const EffectLayer& drawn = caster.skillLayers[static_cast<std::size_t>(layer)];
                if (!drawn.isCutIn()) {
                    drawn.draw(canvas, shake);
                } else if (drawn.playing()) {
                    cutIn(caster, drawn);
                }
            }
        }
    }

    void SkillAction::drawHitLayers(gfx::Canvas& canvas, Fighter& hit, bool front, core::Point shake) const {
        if (!m_effectsActive || !m_hitMotion) {
            return;
        }
        const formats::MotionRecord& record = *m_hitMotion;
        const int count = static_cast<int>(record.drawOrder.size());
        const int from = front ? m_hitDrawPosition + 1 : 1;
        const int to = front ? count : m_hitDrawPosition - 1;
        for (int j = from; j <= to; ++j) {
            const int layer = record.drawOrder[static_cast<std::size_t>(j - 1)] - 1;
            if (layer >= 1 && layer <= Fighter::kLayers) {
                hit.hitLayers[static_cast<std::size_t>(layer)].draw(canvas, shake);
            }
        }
    }

}
