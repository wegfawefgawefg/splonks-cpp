#pragma once

#include "origin.hpp"
#include "math_types.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace splonks {

enum class Sprite {
    NoSprite,
    Reticle,
    Heart,
    Bomb,
    RopeIcon,
    GoldIcon,
    Rope,
    RopeWindingUp,
    BaseballBat,
    BatNeutral,
    BatHanging,
    BatDead,
    Block,
    BombTicking,
    Pot,
    Box,
    Jetpack,
    Gold,
    GoldStack,
    Rock,
    PlayerStanding,
    PlayerStandingHolding,
    PlayerWalking,
    PlayerWalkingHolding,
    PlayerFlying,
    PlayerDead,
    PlayerStunned,
    PlayerClimbing,
    PlayerHanging,
    PlayerFalling,
};

constexpr std::size_t kSpriteCount = 30;

const char* ToFilename(Sprite sprite);

struct Frame {
    UVec2 sample_position;
    float duration = 0.0F;
};

struct SpriteData {
    std::vector<Frame> frames;
    UVec2 size;
};

std::vector<Sprite> AllSprites();
std::vector<SpriteData> LoadSprites(const std::string& asset_folder);
SpriteData LoadSpriteData(const std::string& json_path);

struct SpriteAnimator {
    Sprite sprite = Sprite::NoSprite;
    std::size_t current_frame = 0;
    float current_time = 0.0F;
    float scale = 1.0F;
    float speed = 1.0F;
    bool animate = true;

    static SpriteAnimator New(Sprite sprite);

    void SetSprite(Sprite sprite_value);
    void SetSpeed(float speed_value);
    void ResetSpeed();
    Sprite GetSprite() const;
    void Step(const std::vector<SpriteData>& sprites, float dt);
    void RandomizeFrame(const std::vector<SpriteData>& sprites);
    Vec2 GetOrigin(const std::vector<SpriteData>& sprites, Origin origin) const;
};

} // namespace splonks
