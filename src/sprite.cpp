#include "sprite.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

namespace splonks {

namespace {

int RandomIntExclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum - 1);
    return distribution(generator);
}

std::size_t SpriteIndex(Sprite sprite) {
    return static_cast<std::size_t>(sprite);
}

} // namespace

const char* ToFilename(Sprite sprite) {
    switch (sprite) {
    case Sprite::NoSprite:
        return "no_sprite";
    case Sprite::Reticle:
        return "reticle";
    case Sprite::Heart:
        return "heart_icon";
    case Sprite::Bomb:
        return "bomb_icon";
    case Sprite::RopeIcon:
        return "rope_icon";
    case Sprite::GoldIcon:
        return "gold_icon";
    case Sprite::Rope:
        return "rope";
    case Sprite::RopeWindingUp:
        return "rope_winding_up";
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
        return "player_standing";
    case Sprite::PlayerStandingHolding:
        return "player_standing_holding";
    case Sprite::PlayerWalking:
        return "player_walking";
    case Sprite::PlayerWalkingHolding:
        return "player_walking_holding";
    case Sprite::PlayerFlying:
        return "player_flying";
    case Sprite::PlayerDead:
        return "player_dead";
    case Sprite::PlayerStunned:
        return "player_stunned";
    case Sprite::PlayerClimbing:
        return "player_climbing";
    case Sprite::PlayerHanging:
        return "player_hanging";
    case Sprite::PlayerFalling:
        return "player_falling";
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
    std::vector<std::string> missing_files;
    for (const Sprite sprite : AllSprites()) {
        const char* filename = ToFilename(sprite);
        const std::filesystem::path png_path =
            std::filesystem::path(asset_folder) / (std::string(filename) + ".png");
        const std::filesystem::path json_path =
            std::filesystem::path(asset_folder) / (std::string(filename) + ".json");
        if (!std::filesystem::exists(png_path)) {
            missing_files.push_back(png_path.string());
        }
        if (!std::filesystem::exists(json_path)) {
            missing_files.push_back(json_path.string());
        }
    }

    if (!missing_files.empty()) {
        throw std::runtime_error("Missing sprite files");
    }

    std::vector<SpriteData> sprites;
    sprites.reserve(kSpriteCount);
    for (const Sprite sprite : AllSprites()) {
        const char* filename = ToFilename(sprite);
        const std::filesystem::path json_path =
            std::filesystem::path(asset_folder) / (std::string(filename) + ".json");
        sprites.push_back(LoadSpriteData(json_path.string()));
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
