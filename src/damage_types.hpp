#pragma once

namespace splonks {

enum class DamageType {
    Attack,
    IgnitingAttack,
    HeavyAttack,
    JumpOn,
    Burn,
    Explosion,
    Crush,
    Spikes,
    Fall,
};

enum class DamageVuln {
    Immune,
    BurningOnly,
    CrushingOnly,
    AttackingOnly,
    HeavyAttackOnly,
    ExplosionOnly,
    CrushingAndSpikes,
    CrushingSpikesAndExplosion,
    Vulnerable,
    AnthingExceptJumpOn,
};

} // namespace splonks
