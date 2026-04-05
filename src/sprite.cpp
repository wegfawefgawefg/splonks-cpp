#include "sprite.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

namespace splonks {

namespace {

constexpr unsigned int kDefaultFallbackSpriteSize = 8;
constexpr float kDefaultFallbackFrameDuration = 1.0F;

int RandomIntExclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(generator);
}

std::size_t SpriteIndex(Sprite sprite) {
    return static_cast<std::size_t>(sprite);
}

SpriteData DefaultFallbackSpriteData() {
    return SpriteData{
        std::vector<Frame>{Frame{UVec2::New(0, 0), kDefaultFallbackFrameDuration}},
        UVec2::New(kDefaultFallbackSpriteSize, kDefaultFallbackSpriteSize),
    };
}

void PrintAssetWarning(const std::string& message, const std::string& path) {
    std::cerr << "\033[33mwarning:\033[0m " << message << '\n';
    std::cerr << "    " << path << '\n';
}

} // namespace

const char* ToFilename(Sprite sprite) {
    switch (sprite) {
    case Sprite::NoSprite:
        return "no_sprite";
    case Sprite::Reticle:
        return "reticle";
    case Sprite::Heart:
        return "heart";
    case Sprite::Bomb:
        return "bomb_icon";
    case Sprite::RopeIcon:
        return "rope_icon";
    case Sprite::GoldIcon:
        return "gold_icon";
    case Sprite::Rope:
        return "rope_bundle";
    case Sprite::RopeWindingUp:
        return "rope_unfolding";
    case Sprite::BaseballBat:
        return "baseball_bat";
    case Sprite::BatNeutral:
        return "bat_neutral";
    case Sprite::BatHanging:
        return "bat_hanging";
    case Sprite::BatDead:
        return "bat_dead";
    case Sprite::Block:
        return "block";
    case Sprite::BombTicking:
        return "bomb_ticking";
    case Sprite::Pot:
        return "pot";
    case Sprite::Box:
        return "box";
    case Sprite::Jetpack:
        return "jetpack";
    case Sprite::Gold:
        return "gold";
    case Sprite::GoldStack:
        return "gold_stack";
    case Sprite::Rock:
        return "rock";
    case Sprite::PlayerStanding:
        return "man_neutral";
    case Sprite::PlayerStandingHolding:
        return "man_neutral_holding";
    case Sprite::PlayerWalking:
        return "man_walking";
    case Sprite::PlayerWalkingHolding:
        return "man_walking_holding";
    case Sprite::PlayerFlying:
        return "man_neutral";
    case Sprite::PlayerDead:
        return "man_dead";
    case Sprite::PlayerStunned:
        return "man_stunned";
    case Sprite::PlayerClimbing:
        return "man_neutral";
    case Sprite::PlayerHanging:
        return "man_neutral";
    case Sprite::PlayerFalling:
        return "man_neutral";
    }

    return "no_sprite";
}

std::vector<Sprite> AllSprites() {
    return {
        Sprite::NoSprite,
        Sprite::Reticle,
        Sprite::Heart,
        Sprite::Bomb,
        Sprite::RopeIcon,
        Sprite::GoldIcon,
        Sprite::Rope,
        Sprite::RopeWindingUp,
        Sprite::BaseballBat,
        Sprite::BatNeutral,
        Sprite::BatHanging,
        Sprite::BatDead,
        Sprite::Block,
        Sprite::BombTicking,
        Sprite::Pot,
        Sprite::Box,
        Sprite::Jetpack,
        Sprite::Gold,
        Sprite::GoldStack,
        Sprite::Rock,
        Sprite::PlayerStanding,
        Sprite::PlayerStandingHolding,
        Sprite::PlayerWalking,
        Sprite::PlayerWalkingHolding,
        Sprite::PlayerFlying,
        Sprite::PlayerDead,
        Sprite::PlayerStunned,
        Sprite::PlayerClimbing,
        Sprite::PlayerHanging,
        Sprite::PlayerFalling,
    };
}

std::vector<SpriteData> LoadSprites(const std::string& asset_folder) {
    const std::filesystem::path fallback_json_path =
        std::filesystem::path(asset_folder) / (std::string(ToFilename(Sprite::NoSprite)) + ".json");
    SpriteData fallback_sprite_data = DefaultFallbackSpriteData();
    if (std::filesystem::exists(fallback_json_path)) {
        try {
            fallback_sprite_data = LoadSpriteData(fallback_json_path.string());
        } catch (...) {
        }
    }

    std::vector<SpriteData> sprites;
    sprites.reserve(kSpriteCount);
    for (const Sprite sprite : AllSprites()) {
        const char* filename = ToFilename(sprite);
        const std::filesystem::path json_path =
            std::filesystem::path(asset_folder) / (std::string(filename) + ".json");
        if (std::filesystem::exists(json_path)) {
            try {
                sprites.push_back(LoadSpriteData(json_path.string()));
            } catch (const std::exception& exception) {
                PrintAssetWarning(
                    "Failed to parse sprite metadata for " + std::string(filename) +
                        ". Using fallback metadata.",
                    json_path.string()
                );
                std::cerr << "    reason: " << exception.what() << '\n';
                sprites.push_back(fallback_sprite_data);
            }
        } else {
            PrintAssetWarning(
                "Missing sprite metadata for " + std::string(filename) +
                    ". Using fallback metadata.",
                json_path.string()
            );
            sprites.push_back(fallback_sprite_data);
        }
    }
    return sprites;
}

SpriteData LoadSpriteData(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + json_path);
    }

    nlohmann::json json;
    file >> json;

    const nlohmann::json& frames = json.at("frames");
    SpriteData sprite_data;
    sprite_data.size = UVec2::New(0, 0);

    for (auto it = frames.begin(); it != frames.end(); ++it) {
        const nlohmann::json& frame_data = it.value();
        const nlohmann::json& frame = frame_data.at("frame");

        const unsigned int x = frame.at("x").get<unsigned int>();
        const unsigned int y = frame.at("y").get<unsigned int>();
        const float duration = frame_data.at("duration").get<float>();

        Frame sprite_frame;
        sprite_frame.sample_position = UVec2::New(x, y);
        sprite_frame.duration = duration;
        sprite_data.frames.push_back(sprite_frame);

        if (sprite_data.size == UVec2::New(0, 0)) {
            const unsigned int w = frame.at("w").get<unsigned int>();
            const unsigned int h = frame.at("h").get<unsigned int>();
            sprite_data.size = UVec2::New(w, h);
        }
    }

    return sprite_data;
}

SpriteAnimator SpriteAnimator::New(Sprite sprite_value) {
    SpriteAnimator result;
    result.sprite = sprite_value;
    result.current_frame = 0;
    result.current_time = 0.0F;
    result.scale = 1.0F;
    result.speed = 1.0F;
    result.animate = true;
    return result;
}

void SpriteAnimator::SetSprite(Sprite sprite_value) {
    if (sprite != sprite_value) {
        current_frame = 0;
        current_time = 0.0F;
    }
    sprite = sprite_value;
}

void SpriteAnimator::SetSpeed(float speed_value) {
    speed = std::clamp(speed_value, 0.01F, 10.0F);
}

void SpriteAnimator::ResetSpeed() {
    speed = 1.0F;
}

Sprite SpriteAnimator::GetSprite() const {
    return sprite;
}

void SpriteAnimator::Step(const std::vector<SpriteData>& sprites, float dt) {
    if (!animate) {
        return;
    }

    const SpriteData& sprite_data = sprites[SpriteIndex(sprite)];
    const Frame& frame = sprite_data.frames[current_frame];
    current_time += dt * speed;
    if (current_time >= frame.duration) {
        current_time = 0.0F;
        current_frame = (current_frame + 1) % sprite_data.frames.size();
    }
}

void SpriteAnimator::RandomizeFrame(const std::vector<SpriteData>& sprites) {
    const SpriteData& sprite_data = sprites[SpriteIndex(sprite)];
    current_frame = static_cast<std::size_t>(
        RandomIntExclusive(0, static_cast<int>(sprite_data.frames.size())));
}

Vec2 SpriteAnimator::GetOrigin(const std::vector<SpriteData>& sprites, Origin origin) const {
    const SpriteData& sprite_data = sprites[SpriteIndex(sprite)];
    const Vec2 size =
        Vec2::New(static_cast<float>(sprite_data.size.x), static_cast<float>(sprite_data.size.y));

    switch (origin) {
    case Origin::Center:
        return Vec2::New(size.x / 2.0F, size.y / 2.0F);
    case Origin::TopLeft:
        return Vec2::New(0.0F, 0.0F);
    case Origin::Foot:
        return Vec2::New(size.x / 2.0F, size.y);
    }

    return Vec2::New(0.0F, 0.0F);
}

} // namespace splonks
