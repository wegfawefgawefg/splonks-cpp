#include "state.hpp"
#include "world_query.hpp"

#include "entities/common/common.hpp"
#include "stage_init.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

bool HasAnyAreaListenerCallback(const Entity& entity) {
    return entity.on_area_enter != nullptr || entity.on_area_exit != nullptr ||
           entity.on_area_tile_changed != nullptr;
}

} // namespace

void AddShake(
    State& state,
    const Vec2& world_pos,
    float foreground_tile_amount,
    float background_tile_amount,
    float entity_amount,
    float radius_tiles,
    std::optional<VID> exclude_entity_vid
) {
    if (foreground_tile_amount <= 0.0F && background_tile_amount <= 0.0F && entity_amount <= 0.0F) {
        return;
    }

    const IVec2 world_pixel = IVec2::New(
        static_cast<int>(std::floor(world_pos.x)),
        static_cast<int>(std::floor(world_pos.y))
    );
    const IVec2 tile_pos = state.stage.GetTileCoordAtWc(world_pixel);

    if (foreground_tile_amount > 0.0F) {
        state.stage.AddForegroundTileShakeArea(tile_pos, foreground_tile_amount, radius_tiles);
    }
    if (background_tile_amount > 0.0F) {
        state.stage.AddBackgroundTileShakeArea(tile_pos, background_tile_amount, radius_tiles);
    }
    if (entity_amount <= 0.0F) {
        return;
    }

    const float radius_world = radius_tiles * static_cast<float>(kTileSize);
    const AABB area = AABB::New(
        world_pos - Vec2::New(radius_world, radius_world),
        world_pos + Vec2::New(radius_world, radius_world)
    );
    for (const VID& vid : QueryEntitiesInAabb(state, area, exclude_entity_vid)) {
        Entity* const entity = state.entity_manager.GetEntityMut(vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }

        const Vec2 nearest_center = GetNearestWorldPoint(state.stage, world_pos, entity->GetCenter());
        const Vec2 delta = nearest_center - world_pos;
        const float distance = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
        if (radius_world > 0.0F) {
            if (distance > radius_world) {
                continue;
            }
            AddEntityShake(*entity, entity_amount * (1.0F - (distance / radius_world)));
            continue;
        }

        const AABB nearest_aabb = GetNearestWorldAabb(state.stage, world_pos, entity->GetAABB());
        if (world_pos.x >= nearest_aabb.tl.x && world_pos.x <= nearest_aabb.br.x &&
            world_pos.y >= nearest_aabb.tl.y && world_pos.y <= nearest_aabb.br.y) {
            AddEntityShake(*entity, entity_amount);
        }
    }
}

void AddShake(
    State& state,
    const Vec2& world_pos,
    float amount,
    float radius_tiles,
    std::optional<VID> exclude_entity_vid
) {
    AddShake(state, world_pos, amount, amount, amount, radius_tiles, exclude_entity_vid);
}

void AddShake(
    State& state,
    const Vec2& world_pos,
    float amount,
    float radius_tiles,
    ShakeMask mask,
    std::optional<VID> exclude_entity_vid
) {
    if (amount <= 0.0F || mask == ShakeMask::None) {
        return;
    }

    AddShake(
        state,
        world_pos,
        HasShakeMask(mask, ShakeMask::ForegroundTiles) ? amount : 0.0F,
        HasShakeMask(mask, ShakeMask::BackgroundTiles) ? amount : 0.0F,
        HasShakeMask(mask, ShakeMask::Entities) ? amount : 0.0F,
        radius_tiles,
        exclude_entity_vid
    );
}

State State::New() {
    State state;
    state.mode = Mode::Playing;
    state.settings = LoadSettings();
    state.menu_inputs = MenuInputs::New();
    state.menu_input_snapshot = MenuInputSnapshot::New();
    state.previous_menu_input_snapshot = MenuInputSnapshot::New();
    state.menu_input_debounce_timers = MenuInputDebounceTimers::New();
    state.playing_inputs = PlayingInputs::New();
    state.immediate_playing_inputs = PlayingInputs::New();
    state.playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_immediate_playing_input_snapshot = PlayingInputSnapshot::New();
    state.title_menu_selection = TitleMenuOption::Start;
    state.settings_menu_selection = SettingsMenuOption::Video;
    state.video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
    state.ui_settings_menu_selection = UiSettingsMenuOption::IconScale;
    state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::Effect;
    state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainLighting;
    state.video_settings_target_window_size_index.reset();
    state.video_settings_target_resolution_index.reset();
    state.video_settings_target_fullscreen.reset();
    state.choosing_control_binding = false;
    state.rebuild_render_texture = false;
    state.scene_frame = 0;
    state.time_since_last_update = 0.0F;
    state.now = 0.0;
    state.running = true;
    state.frame = 0;
    state.stage_frame = 0;
    state.menu_return_to = Mode::Title;
    state.game_over = false;
    state.pause = false;
    state.win = false;
    state.points = 0;
    state.deaths = 0;
    state.depth = 0;
    state.frame_pause = 0;
    state.interact_claimed_vids_this_frame.clear();
    state.entity_manager = EntityManager::New();
    state.particles = ParticleSystem{};
    state.sid = SID::New();
    state.area_listener_vids.clear();
    state.respawn_target = StageLoadTarget::ForStageType(StageType::SplkMines1);
    state.pending_stage_transition.reset();
    state.stage_lighting = StageLighting::New();
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
    state.mouse_trailer_vid.reset();
    state.contact = ContactBookkeeping{};
    state.entity_tools = EntityToolInventoryState{};
    state.world_prompts.clear();
    InitDebugLevel(state);
    return state;
}

void State::SetMode(Mode new_mode) {
    mode = new_mode;
    scene_frame = 0;
    world_prompts.clear();
    interact_claimed_vids_this_frame.clear();
}

void State::RebuildSid(const Graphics& graphics) {
    sid.Clear();
    area_listener_vids.clear();

    for (std::size_t entity_id = 0; entity_id < entity_manager.entities.size(); ++entity_id) {
        UpdateSidForEntity(entity_id, graphics);
    }
}

void State::UpdateSidForEntity(std::size_t entity_id, const Graphics& graphics) {
    if (entity_id >= entity_manager.entities.size()) {
        return;
    }

    const Entity& entity = entity_manager.entities[entity_id];
    UpdateAreaListenerCacheForEntity(entity_id);
    sid.Remove(entity.vid);
    if (!entity.active) {
        return;
    }

    const AABB broadphase_aabb = entities::common::GetEntityBroadphaseAabb(entity, graphics);
    sid.Upsert(entity.vid, broadphase_aabb);
}

void State::RebuildAreaListenerCache() {
    area_listener_vids.clear();
    for (std::size_t entity_id = 0; entity_id < entity_manager.entities.size(); ++entity_id) {
        UpdateAreaListenerCacheForEntity(entity_id);
    }
}

void State::UpdateAreaListenerCacheForEntity(std::size_t entity_id) {
    if (entity_id >= entity_manager.entities.size()) {
        return;
    }

    const Entity& entity = entity_manager.entities[entity_id];
    area_listener_vids.erase(
        std::remove_if(
            area_listener_vids.begin(),
            area_listener_vids.end(),
            [&](const VID& candidate) {
                return candidate.id == entity.vid.id;
            }
        ),
        area_listener_vids.end()
    );

    if (!entity.active || !HasAnyAreaListenerCallback(entity)) {
        return;
    }

    area_listener_vids.push_back(entity.vid);
}

void State::ClearWorldPrompts() {
    world_prompts.clear();
}

void State::AddWorldPrompt(const WorldPrompt& prompt) {
    world_prompts.push_back(prompt);
}

void State::ClearInteractClaims() {
    interact_claimed_vids_this_frame.clear();
}

void State::ClaimInteractForEntity(VID entity_vid) {
    if (std::find(interact_claimed_vids_this_frame.begin(),
                  interact_claimed_vids_this_frame.end(),
                  entity_vid) == interact_claimed_vids_this_frame.end()) {
        interact_claimed_vids_this_frame.push_back(entity_vid);
    }
}

bool State::IsInteractClaimedForEntity(VID entity_vid) const {
    return std::find(interact_claimed_vids_this_frame.begin(),
                     interact_claimed_vids_this_frame.end(),
                     entity_vid) != interact_claimed_vids_this_frame.end();
}

} // namespace splonks
