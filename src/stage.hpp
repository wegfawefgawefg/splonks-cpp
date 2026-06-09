#pragma once

#include "audio_asset_id.hpp"
#include "ent/core_types.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "tile.hpp"
#include "utils.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>


namespace splonks {

constexpr float kDefaultStageGravity = 0.3F;

struct Audio;
struct Ent;
struct State;

struct EntSpawn {
    EntType type_ = EntType::None;
    FVec2 pos = FVec2::New(0.0F, 0.0F);
    std::optional<FVec2> size_override = std::nullopt;
    Side facing = Side::Left;
    std::optional<EntAiState> ai_state_override = std::nullopt;
    AFrameId anim_id = kInvalidAFrameId;
    std::optional<std::uint32_t> ent_a_spawn_index = std::nullopt;
    std::optional<std::uint32_t> ent_b_spawn_index = std::nullopt;
    std::optional<std::uint32_t> ent_c_spawn_index = std::nullopt;
    std::optional<std::uint32_t> ent_d_spawn_index = std::nullopt;
    std::optional<std::uint32_t> shop_owner_spawn_index = std::nullopt;
    bool buyable = false;
    std::uint32_t buy_price = 0;
    std::string exit_id;
};

struct StageExitRequirement {
    std::string flag;
    bool expected = true;
};

struct StageExitTarget {
    std::string target_stage_id;
    std::vector<StageExitRequirement> requirements;
};

using StageExitId = int;
constexpr StageExitId kInvalidStageExitId = -1;

struct StageTileTrigger;
enum class StageTileTriggerKind {
    Destroyed,
    Changed,
    Entered,
    Exited,
};
using StageTileTriggerHandler = void (*)(const StageTileTrigger&, const IVec2&, State&, Audio&);

struct StageTileTrigger {
    IVec2 tile_pos = IVec2::New(0, 0);
    StageTileTriggerKind kind = StageTileTriggerKind::Destroyed;
    std::optional<std::uint32_t> target_spawn_index = std::nullopt;
    std::optional<VID> target_vid = std::nullopt;
    std::uint32_t payload_id = 0;
    StageTileTriggerHandler on_triggered = nullptr;
    const char* debug_label = nullptr;
};

struct StageExit {
    std::string id;
    StageExitTarget target;
};

enum class BackgroundStampCondition {
    None,
    Wanted,
};

struct BackgroundStamp {
    AFrameId anim_id = kInvalidAFrameId;
    FVec2 pos = FVec2::New(0.0F, 0.0F);
    BackgroundStampCondition condition = BackgroundStampCondition::None;
};

struct StageLight {
    VID vid;
    IVec2 tile_pos = IVec2::New(0, 0);
    int radius = 0;
};

enum class EmbeddedTreasureVisibility : std::uint8_t {
    Hidden,
    Visible,
};

struct EmbeddedTreasureDrop {
    EntType type_ = EntType::None;
    int count = 0;
};

constexpr std::size_t kMaxEmbeddedTreasureDrops = 4;

struct EmbeddedTreasure {
    EmbeddedTreasureVisibility visibility = EmbeddedTreasureVisibility::Hidden;
    AFrameId overlay_frame = kInvalidAFrameId;
    AudioAssetId break_sound = kInvalidAudioAssetId;
    std::array<EmbeddedTreasureDrop, kMaxEmbeddedTreasureDrops> drops{};

    bool IsEmpty() const;
    bool IsVisible() const;
    std::optional<AFrameId> GetOverlayFrame() const;
};

struct StageGenAnnotation {
    FVec2 world_pos = FVec2::New(0.0F, 0.0F);
    std::string text;
};

enum class StageType : int {
    Blank,
    Test1,
};

enum class StageBorderSideKind : std::uint8_t {
    Left,
    Right,
    Top,
    Bottom,
};

struct StageBorderSide {
    Tile tile = Tile::Air;
};

struct StageBorder {
    StageBorderSide left;
    StageBorderSide right;
    StageBorderSide top;
    StageBorderSide bottom;
    bool wrap_x = false;
    bool wrap_y = false;
    std::optional<int> void_death_y = std::nullopt;
};

enum class TileShakeLayerMask : std::uint8_t {
    None = 0,
    Foreground = 1 << 0,
    Background = 1 << 1,
    Both = 3,
};

constexpr TileShakeLayerMask operator|(TileShakeLayerMask a, TileShakeLayerMask b) {
    return static_cast<TileShakeLayerMask>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

constexpr bool HasTileShakeLayerMask(TileShakeLayerMask mask, TileShakeLayerMask flag) {
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(flag)) != 0;
}

struct Stage {
    StageType stage_type = StageType::Blank;
    std::string quest_id;
    std::string quest_stage_id;
    std::string route_label;
    std::string stage_title;
    int quest_level_number = 0;
    std::optional<std::uint32_t> generation_seed = std::nullopt;
    std::vector<StageExit> exits;
    std::vector<std::vector<Tile>> tiles;
    std::vector<std::vector<TileRotation>> tile_rotations;
    std::vector<std::vector<Tile>> fluid_tiles;
    std::vector<std::vector<sim::Scalar>> fluid_amount;
    std::vector<std::vector<sim::Scalar>> fluid_display_amount;
    std::vector<std::vector<sim::FxVec2>> fluid_velocity;
    std::vector<std::vector<sim::FxVec2>> fluid_gravity;
    std::vector<std::vector<std::uint8_t>> fluid_gravity_strength;
    std::vector<std::vector<sim::FxVec2>> fluid_temp_gravity;
    std::vector<std::vector<sim::Scalar>> tile_shake;
    std::vector<std::vector<sim::Scalar>> backwall_tile_shake;
    std::vector<std::vector<Tile>> backwall_tiles;
    std::vector<Tile> backwall_fill_tiles;
    std::vector<std::vector<EmbeddedTreasure>> embedded_treasures;
    std::vector<std::vector<int>> rooms;
    std::vector<IVec2> path;
    std::vector<EntSpawn> ent_spawns;
    std::vector<StageTileTrigger> tile_triggers;
    std::vector<BackgroundStamp> background_stamps;
    std::vector<StageGenAnnotation> stagegen_annotations;
    std::vector<StageLight> lights;
    AFrameId block_anim_id = aframe_ids::CaveBlock;
    sim::Scalar gravity = ToFxScalar(kDefaultStageGravity);
    StageBorder border{};
    bool camera_clamp_enabled = true;
    FVec2 camera_clamp_margin = FVec2::New(0.0F, 0.0F);
    bool wrap_transform_active = false;
    std::uint32_t wrap_padding_tiles = 0;
    UVec2 wrap_core_origin_tiles = UVec2::New(0, 0);
    UVec2 wrap_core_size_tiles = UVec2::New(0, 0);
    std::uint32_t next_light_vid = 0;
    std::uint32_t tile_change_generation = 0;

    static const UVec2 kShape;
    static const UVec2 kRoomShape;
    static const UVec2 kRoomLayout;

    static Stage NewBlank();
    static Stage New(StageType stage_type, DetRng& det_rng);
    static StageBorder MakeUniformBorder(Tile tile);
    UVec2 GetStageDims() const;
    UVec2 GetRoomLayoutDims() const;
    UVec2 GetRegularRoomGridRoomDims() const;
    IVec2 GetRegularRoomGridTlWc(const IVec2& room) const;
    const Tile& GetTile(std::uint32_t x, std::uint32_t y) const;
    TileRotation GetTileRotation(std::uint32_t x, std::uint32_t y) const;
    Tile GetFluidTile(std::uint32_t x, std::uint32_t y) const;
    float GetFluidAmount(std::uint32_t x, std::uint32_t y) const;
    float GetTileShake(std::uint32_t x, std::uint32_t y) const;
    float GetForegroundTileShake(std::uint32_t x, std::uint32_t y) const;
    float GetBackgroundTileShake(std::uint32_t x, std::uint32_t y) const;
    const Tile& GetBackwallTile(std::uint32_t x, std::uint32_t y) const;
    EmbeddedTreasure GetEmbeddedTreasure(std::uint32_t x, std::uint32_t y) const;
    const Tile* GetTileAtWc(const IVec2& pos) const;
    std::vector<const Tile*> GetTilesInRectWc(const IVec2& tl, const IVec2& br) const;
    std::vector<const Tile*> GetTilesInRect(const IVec2& tl, const IVec2& br) const;
    void FillBackwall(const std::vector<Tile>& fill_tiles);
    void FillBackwall(const std::vector<Tile>& fill_tiles, DetRng& det_rng);
    void SyncTileShakeGrid();
    void SyncTileInstanceMetadataGrid();
    void SyncFluidTileGrid();
    void SyncFluidVelocityGrid();
    void SetFluidGravityOverride(const IVec2& pos, sim::FxVec2 gravity_value);
    void ClearFluidGravityOverride(const IVec2& pos);
    void AddFluidTempGravity(const IVec2& pos, sim::FxVec2 gravity_value);
    void ClearFluidTempGravity(const IVec2& pos);
    void SetTile(const IVec2& pos, Tile tile);
    void SetFluidTile(const IVec2& pos, Tile tile);
    void SetTileRotation(const IVec2& pos, TileRotation rotation);
    void AddTileShake(const IVec2& pos, float amount);
    void AddForegroundTileShake(const IVec2& pos, float amount);
    void AddBackgroundTileShake(const IVec2& pos, float amount);
    void AddTileShake(const IVec2& pos, float amount, TileShakeLayerMask layers);
    void AddTileShakeArea(const IVec2& pos, float magnitude, float dist);
    void AddForegroundTileShakeArea(const IVec2& pos, float magnitude, float dist);
    void AddBackgroundTileShakeArea(const IVec2& pos, float magnitude, float dist);
    void AddTileShakeArea(const IVec2& pos, float magnitude, float dist, TileShakeLayerMask layers);
    void AttenuateTileShake(float amount);
    void AttenuateForegroundTileShake(float amount);
    void AttenuateBackgroundTileShake(float amount);
    void AttenuateTileShake(float amount, TileShakeLayerMask layers);
    void SetBackwallTile(const IVec2& pos, Tile tile);
    void SetEmbeddedTreasure(const IVec2& pos, EntType type_);
    void SetEmbeddedTreasure(const IVec2& pos, const EmbeddedTreasure& embedded_treasure);
    EmbeddedTreasure TakeEmbeddedTreasure(const IVec2& pos);
    VID AddLight(const IVec2& tile_pos, int radius);
    VID AddLightWithVid(VID vid, const IVec2& tile_pos, int radius);
    bool RemoveLight(VID vid);
    const StageLight* GetLight(VID vid) const;
    void SetTilesInRectWc(sim::FxAABB area, Tile tile_type);
    void SetTilesInRect(IAABB tile_area, Tile tile_type);
    std::vector<IAABB> GetAabbsForAllCollidableTilesInRect(const IVec2& tl, const IVec2& br) const;
    UVec2 GetRandomRegularRoomGridCoord(DetRng& det_rng) const;
    std::optional<IVec2> GetRandomNoncollidablePositionInStage(DetRng& det_rng) const;
    std::optional<IVec2> GetRandomNoncollidablePositionInRandomRegularRoomGridCell(DetRng& det_rng) const;
    std::optional<IVec2> GetRandomNoncollidablePositionInRegularRoomGridCell(const UVec2& room,
                                                                              DetRng& det_rng) const;
    std::uint32_t GetWidth() const;
    std::uint32_t GetHeight() const;
    std::uint32_t GetTileWidth() const;
    std::uint32_t GetTileHeight() const;
    bool WrapsX() const;
    bool WrapsY() const;
    bool HasVoidDeathY() const;
    sim::Scalar GetVoidDeathY() const;
    const StageBorderSide& GetBorderSide(StageBorderSideKind side) const;
    Tile GetBorderTile(StageBorderSideKind side) const;
    bool IsBorderSideBlocking(StageBorderSideKind side) const;
    std::optional<StageBorderSideKind> GetOutOfBoundsSideForTileCoord(int tile_x, int tile_y) const;
    std::optional<StageBorderSideKind> GetOutOfBoundsSideForWorldPos(const IVec2& wc) const;
    Tile GetTileOrBorder(int tile_x, int tile_y) const;
    bool IsTileCoordInside(int tile_x, int tile_y) const;
    bool IsWorldPosInside(const IVec2& wc) const;
    StageExitId FindExitId(std::string_view id) const;
    const StageExit* GetExit(StageExitId id) const;
    IVec2 WrapTileCoord(const IVec2& tile_coord) const;
    IVec2 WrapWorldPos(const IVec2& wc) const;
    void NormalizeEntPositionForWrap(Ent& ent) const;
    std::pair<UVec2, UVec2> GetRegularRoomGridCorners(const UVec2& room) const;
    std::vector<const Tile*> GetTilesInRegularRoomGridCell(const UVec2& room) const;
    IVec2 GetStartingRoom() const;
    IVec2 GetTileCoordAtWc(const IVec2& wc) const;
    bool TileCoordAtWcExists(const IVec2& wc) const;
};

} // namespace splonks
