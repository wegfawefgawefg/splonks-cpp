#include "cli_state_smoke.hpp"

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "frame_data.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "network/input_lockstep.hpp"
#include "network/net_fuzzer.hpp"
#include "quest_stage_loader.hpp"
#include "raw_frame_data.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"
#include "step.hpp"
#include "tile_source_data.hpp"
#include "tools/tool_archetype.hpp"
#include "world_ops.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace splonks {

namespace {

constexpr const char* kAnnotationsYamlPath = "assets/graphics/annotations.yaml";

void InitCliSmokeRuntimeTables(Graphics& graphics) {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    graphics.frame_data_db = FrameDataDb::FromRaw(raw_file);
    graphics.tile_source_db = BuildTileSourceDb(graphics.frame_data_db);

    PopulateEntityArchetypesTable();
    SyncEntityArchetypeSizesFromFrameData(graphics);
    PopulateToolArchetypesTable();
}

bool CompareCanonicalFingerprints(
    const State& left,
    const State& right,
    const char* label
) {
    const CanonicalStateFingerprint left_fingerprint = ComputeCanonicalStateFingerprint(left);
    const CanonicalStateFingerprint right_fingerprint = ComputeCanonicalStateFingerprint(right);
    if (left_fingerprint.value == right_fingerprint.value) {
        std::cout << "state equality smoke " << label << " ok: "
                  << left_fingerprint.summary
                  << " hash=" << left_fingerprint.value << '\n';
        return true;
    }

    std::cerr << "state equality smoke failed at " << label << "\n"
              << "  left  " << left_fingerprint.summary << " hash="
              << left_fingerprint.value << "\n"
              << "  right " << right_fingerprint.summary << " hash="
              << right_fingerprint.value << "\n";
    return false;
}

std::string DescribeFirstStateDifference(const State& left, const State& right) {
    if (left.stage.GetStageDims() != right.stage.GetStageDims()) {
        std::ostringstream output;
        output << "stage dims differ: left=" << left.stage.GetTileWidth() << "x"
               << left.stage.GetTileHeight() << " right=" << right.stage.GetTileWidth()
               << "x" << right.stage.GetTileHeight();
        return output.str();
    }

    for (unsigned int y = 0; y < left.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < left.stage.GetTileWidth(); ++x) {
            if (left.stage.GetTile(x, y) != right.stage.GetTile(x, y)) {
                std::ostringstream output;
                output << "foreground tile differs at " << x << "," << y
                       << ": left=" << static_cast<int>(left.stage.GetTile(x, y))
                       << " right=" << static_cast<int>(right.stage.GetTile(x, y));
                return output.str();
            }
            if (left.stage.GetBackwallTile(x, y) != right.stage.GetBackwallTile(x, y)) {
                std::ostringstream output;
                output << "backwall tile differs at " << x << "," << y
                       << ": left=" << static_cast<int>(left.stage.GetBackwallTile(x, y))
                       << " right=" << static_cast<int>(right.stage.GetBackwallTile(x, y));
                return output.str();
            }
        }
    }

    if (left.entity_manager.entities.size() != right.entity_manager.entities.size()) {
        std::ostringstream output;
        output << "entity array size differs: left=" << left.entity_manager.entities.size()
               << " right=" << right.entity_manager.entities.size();
        return output.str();
    }

    for (std::size_t i = 0; i < left.entity_manager.entities.size(); ++i) {
        const Entity& a = left.entity_manager.entities[i];
        const Entity& b = right.entity_manager.entities[i];
        if (a.active != b.active ||
            a.type_ != b.type_ ||
            a.vid != b.vid) {
            std::ostringstream output;
            output << "entity " << i << " identity differs:"
                   << " frame " << left.frame << "/" << right.frame
                   << " stage_frame " << left.stage_frame << "/" << right.stage_frame
                   << " mode " << static_cast<int>(left.mode) << "/" << static_cast<int>(right.mode)
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/" << static_cast<int>(b.type_)
                   << " vid " << a.vid.id << ":" << a.vid.version
                   << "/" << b.vid.id << ":" << b.vid.version;
            std::size_t version_diff_count = 0;
            std::ostringstream version_examples;
            for (std::size_t j = 0; j < left.entity_manager.entities.size(); ++j) {
                const Entity& left_entity = left.entity_manager.entities[j];
                const Entity& right_entity = right.entity_manager.entities[j];
                if (left_entity.vid.version != right_entity.vid.version) {
                    if (version_diff_count < 8) {
                        version_examples << " [" << j
                                         << " t" << static_cast<int>(left_entity.type_)
                                         << "/" << static_cast<int>(right_entity.type_)
                                         << " v" << left_entity.vid.version
                                         << "/" << right_entity.vid.version
                                         << " a" << left_entity.active
                                         << "/" << right_entity.active << "]";
                    }
                    version_diff_count += 1;
                }
            }
            output << " version_diffs=" << version_diff_count
                   << " available_ids " << left.entity_manager.available_ids.size()
                   << "/" << right.entity_manager.available_ids.size()
                   << version_examples.str();
            return output.str();
        }
        if (!a.active) {
            continue;
        }
        if (a.pos != b.pos ||
            a.vel != b.vel ||
            a.acc != b.acc ||
            a.grounded != b.grounded ||
            a.holding != b.holding ||
            a.wanted != b.wanted ||
            a.render_enabled != b.render_enabled ||
            a.size != b.size ||
            a.rotation != b.rotation ||
            a.coyote_time != b.coyote_time ||
            a.stun_timer != b.stun_timer ||
            a.fall_timer != b.fall_timer ||
            a.facing != b.facing ||
            a.draw_layer != b.draw_layer ||
            a.condition != b.condition ||
            a.ai_state != b.ai_state ||
            a.damage_vulnerability != b.damage_vulnerability ||
            a.movement_flags != b.movement_flags ||
            a.health != b.health ||
            a.back_vid != b.back_vid ||
            a.holding_vid != b.holding_vid ||
            a.held_by_vid != b.held_by_vid ||
            a.entity_a != b.entity_a ||
            a.entity_b != b.entity_b ||
            a.entity_c != b.entity_c ||
            a.entity_d != b.entity_d ||
            a.stage_exit_id != b.stage_exit_id ||
            a.money != b.money ||
            a.counter_a != b.counter_a ||
            a.counter_b != b.counter_b ||
            a.counter_c != b.counter_c ||
            a.counter_d != b.counter_d ||
            a.light_strength != b.light_strength ||
            a.light_color.r != b.light_color.r ||
            a.light_color.g != b.light_color.g ||
            a.light_color.b != b.light_color.b ||
            a.light_radius != b.light_radius ||
            a.point_a != b.point_a ||
            a.point_b != b.point_b ||
            a.point_c != b.point_c ||
            a.point_d != b.point_d ||
            a.frame_data_animator.animation_id != b.frame_data_animator.animation_id ||
            a.frame_data_animator.current_frame != b.frame_data_animator.current_frame ||
            a.frame_data_animator.current_time != b.frame_data_animator.current_time ||
            a.frame_data_animator.speed != b.frame_data_animator.speed ||
            a.frame_data_animator.animate != b.frame_data_animator.animate ||
            a.frame_data_animator.loop != b.frame_data_animator.loop ||
            a.frame_data_animator.finished != b.frame_data_animator.finished) {
            std::ostringstream output;
            output << "entity " << i << " differs:"
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/" << static_cast<int>(b.type_)
                   << " vidver " << a.vid.version << "/" << b.vid.version
                   << " pos " << a.pos.x << "," << a.pos.y
                   << "/" << b.pos.x << "," << b.pos.y
                   << " vel " << a.vel.x << "," << a.vel.y
                   << "/" << b.vel.x << "," << b.vel.y
                   << " health " << a.health << "/" << b.health
                   << " condition " << static_cast<int>(a.condition)
                   << "/" << static_cast<int>(b.condition)
                   << " counters " << a.counter_a << "," << a.counter_b
                   << "," << a.counter_c << "," << a.counter_d
                   << "/" << b.counter_a << "," << b.counter_b
                   << "," << b.counter_c << "," << b.counter_d
                   << " anim " << a.frame_data_animator.animation_id
                   << "/" << b.frame_data_animator.animation_id;
            return output.str();
        }
    }

    if (left.drng.state != right.drng.state) {
        std::ostringstream output;
        output << "drng differs: left=" << left.drng.state << " right=" << right.drng.state;
        return output.str();
    }
    if (left.stage.tile_change_generation != right.stage.tile_change_generation) {
        std::ostringstream output;
        output << "tile_change_generation differs: left=" << left.stage.tile_change_generation
               << " right=" << right.stage.tile_change_generation;
        return output.str();
    }

    if (left.players.slots.size() != right.players.slots.size()) {
        std::ostringstream output;
        output << "player slot count differs: left=" << left.players.slots.size()
               << " right=" << right.players.slots.size();
        return output.str();
    }
    for (std::size_t i = 0; i < left.players.slots.size(); ++i) {
        const PlayerSlot& a = left.players.slots[i];
        const PlayerSlot& b = right.players.slots[i];
        if (a.player_id != b.player_id ||
            a.connected != b.connected ||
            a.entity_vid != b.entity_vid ||
            a.input_frame.left != b.input_frame.left ||
            a.input_frame.right != b.input_frame.right ||
            a.input_frame.up != b.input_frame.up ||
            a.input_frame.down != b.input_frame.down ||
            a.input_frame.jump != b.input_frame.jump ||
            a.input_frame.run != b.input_frame.run ||
            a.input_frame.use_button != b.input_frame.use_button ||
            a.input_frame.equip_button != b.input_frame.equip_button ||
            a.input_frame.pick_up_drop != b.input_frame.pick_up_drop ||
            a.input_frame.bomb != b.input_frame.bomb ||
            a.input_frame.rope != b.input_frame.rope ||
            a.input_frame.attack != b.input_frame.attack ||
            a.previous_input_frame.left != b.previous_input_frame.left ||
            a.previous_input_frame.right != b.previous_input_frame.right ||
            a.previous_input_frame.up != b.previous_input_frame.up ||
            a.previous_input_frame.down != b.previous_input_frame.down ||
            a.previous_input_frame.jump != b.previous_input_frame.jump ||
            a.previous_input_frame.run != b.previous_input_frame.run ||
            a.previous_input_frame.use_button != b.previous_input_frame.use_button ||
            a.previous_input_frame.equip_button != b.previous_input_frame.equip_button ||
            a.previous_input_frame.pick_up_drop != b.previous_input_frame.pick_up_drop ||
            a.previous_input_frame.bomb != b.previous_input_frame.bomb ||
            a.previous_input_frame.rope != b.previous_input_frame.rope ||
            a.previous_input_frame.attack != b.previous_input_frame.attack) {
            std::ostringstream output;
            output << "player slot " << i << " differs:"
                   << " player_id " << a.player_id << "/" << b.player_id
                   << " connected " << a.connected << "/" << b.connected
                   << " entity_vid "
                   << (a.entity_vid.has_value() ? static_cast<int>(a.entity_vid->id) : -1)
                   << "/"
                   << (b.entity_vid.has_value() ? static_cast<int>(b.entity_vid->id) : -1);
            return output.str();
        }
    }

    return "no simple lane difference found; fingerprint includes a field not covered by the smoke diff";
}

const Entity* FindFirstActiveEntity(const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active) {
            return &entity;
        }
    }
    return nullptr;
}

bool ApplyDeterministicWorldOpsSmokeMutations(State& state, const char*& failed_step) {
    const Entity* source = FindFirstActiveEntity(state);
    if (source == nullptr) {
        failed_step = "find source entity";
        return false;
    }

    if (!world_ops::SetForegroundTile(state, IVec2::New(3, 3), Tile::Rope)) {
        failed_step = "set foreground tile";
        return false;
    }

    if (!world_ops::PlaceRopeTile(state, *source, IVec2::New(4, 3))) {
        failed_step = "place rope tile";
        return false;
    }

    Entity* rock = world_ops::SpawnEntity(
        state,
        EntityType::Rock,
        [](Entity& entity) {
            entity.pos = Vec2::New(96.0F, 64.0F);
            entity.vel = Vec2::New(1.0F, -2.0F);
            entity.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (rock == nullptr) {
        failed_step = "spawn rock";
        return false;
    }
    world_ops::PatchEntityState(state, *rock, *rock);

    if (!world_ops::DeactivateEntity(state, rock->vid)) {
        failed_step = "deactivate rock";
        return false;
    }

    return true;
}

std::vector<PlayerInputFrame> BuildDeterministicReplayInputScript() {
    std::vector<PlayerInputFrame> frames(180, PlayerInputFrame::New());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        PlayerInputFrame& frame = frames[i];
        frame.mouse_pos = UVec2::New(320, 180);

        if (i < 45) {
            frame.right = true;
        } else if (i < 80) {
            frame.left = true;
        } else if (i < 130) {
            frame.right = true;
            frame.run = true;
        }

        if ((i >= 8 && i < 16) || (i >= 62 && i < 70) || (i >= 108 && i < 116)) {
            frame.jump = true;
        }
        if (i >= 132 && i < 150) {
            frame.down = true;
        }
    }
    return frames;
}

std::vector<std::array<PlayerInputFrame, 2>> BuildDeterministicMultiLocalReplayInputScript() {
    std::vector<std::array<PlayerInputFrame, 2>> frames(240);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        PlayerInputFrame p1 = PlayerInputFrame::New();
        PlayerInputFrame p2 = PlayerInputFrame::New();
        p1.mouse_pos = UVec2::New(320, 180);
        p2.mouse_pos = UVec2::New(320, 180);

        p1.right = i < 70 || (i >= 130 && i < 190);
        p1.left = i >= 80 && i < 118;
        p1.run = i >= 130;
        p1.jump = (i >= 10 && i < 16) || (i >= 92 && i < 98);

        p2.left = i < 55 || (i >= 150 && i < 205);
        p2.right = i >= 70 && i < 130;
        p2.run = i >= 150;
        p2.jump = (i >= 24 && i < 30) || (i >= 164 && i < 170);
        p2.down = i >= 210 && i < 225;

        frames[i] = {p1, p2};
    }
    return frames;
}

std::vector<PlayerInputFrame> BuildBroadDeterministicReplayInputScript() {
    std::vector<PlayerInputFrame> frames(1000, PlayerInputFrame::New());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        PlayerInputFrame& frame = frames[i];
        frame.mouse_pos = UVec2::New(340, 210);
        frame.right = (i >= 12 && i < 180) || (i >= 300 && i < 460) || (i >= 620 && i < 780);
        frame.left = (i >= 210 && i < 285) || (i >= 500 && i < 580);
        frame.run = (i >= 80 && i < 170) || (i >= 640 && i < 740);
        frame.jump = (i >= 32 && i < 40) || (i >= 140 && i < 148) ||
                     (i >= 360 && i < 368) || (i >= 700 && i < 708);
        frame.pick_up_drop = (i >= 4 && i < 8) || (i >= 56 && i < 60) || (i >= 430 && i < 434);
        frame.attack = (i >= 22 && i < 26) || (i >= 120 && i < 124) || (i >= 520 && i < 524);
        frame.bomb = i >= 190 && i < 194;
        frame.rope = i >= 250 && i < 254;
        frame.down = i >= 830 && i < 860;
    }
    return frames;
}

void ApplyPrimaryInputFrame(State& state, const PlayerInputFrame& input_frame) {
    state.playing_input_snapshot = ToPlayingInputSnapshot(input_frame);
}

Entity* FindPrimaryPlayerMut(State& state) {
    PlayerSlot* const primary = state.players.FindPrimaryLocal();
    if (primary == nullptr || !primary->entity_vid.has_value()) {
        return nullptr;
    }
    return state.entity_manager.GetEntityMut(*primary->entity_vid);
}

bool SetScenarioForegroundTile(State& state, const IVec2& tile_pos, Tile tile) {
    const IVec2 wrapped_tile_pos = state.stage.WrapTileCoord(tile_pos);
    if (!state.stage.IsTileCoordInside(wrapped_tile_pos.x, wrapped_tile_pos.y)) {
        return false;
    }
    if (state.stage.GetTile(
            static_cast<unsigned int>(wrapped_tile_pos.x),
            static_cast<unsigned int>(wrapped_tile_pos.y)
        ) == tile &&
        state.stage.GetTileRotation(
            static_cast<unsigned int>(wrapped_tile_pos.x),
            static_cast<unsigned int>(wrapped_tile_pos.y)
        ) == kTileRotation0) {
        return true;
    }
    return world_ops::SetForegroundTile(state, wrapped_tile_pos, tile);
}

bool PrepareBroadDeterministicReplayScenario(State& state, const char*& failed_step) {
    Entity* const player = FindPrimaryPlayerMut(state);
    if (player == nullptr) {
        failed_step = "find primary player";
        return false;
    }

    for (int y = 8; y <= 20; ++y) {
        for (int x = 3; x <= 30; ++x) {
            const Tile tile = y == 20 ? Tile::CaveBlock : Tile::Air;
            if (!SetScenarioForegroundTile(state, IVec2::New(x, y), tile)) {
                failed_step = "prepare arena tiles";
                return false;
            }
        }
    }
    for (int y = 16; y <= 19; ++y) {
        if (!SetScenarioForegroundTile(state, IVec2::New(13, y), Tile::Ladder)) {
            failed_step = "prepare ladder";
            return false;
        }
    }
    if (!SetScenarioForegroundTile(state, IVec2::New(18, 19), Tile::Spikes)) {
        failed_step = "prepare spikes";
        return false;
    }

    player->pos = Vec2::New(4.0F * static_cast<float>(kTileSize), 20.0F * static_cast<float>(kTileSize) - player->size.y);
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
    player->grounded = false;
    player->condition = EntityCondition::Normal;
    player->stun_timer = 0;
    FillToolSlot(state.entity_tools.EnsureToolSlot(player->vid, 0), ToolKind::ThrowBomb, 3, true);
    FillToolSlot(state.entity_tools.EnsureToolSlot(player->vid, 1), ToolKind::ThrowRope, 3, true);

    const auto spawn = [&](EntityType type, Vec2 pos) -> bool {
        Entity* const entity = world_ops::SpawnEntity(state, type, [&](Entity& spawned) {
            spawned.pos = pos;
            spawned.vel = Vec2::New(0.0F, 0.0F);
            spawned.acc = Vec2::New(0.0F, 0.0F);
        });
        return entity != nullptr;
    };
    if (!spawn(EntityType::Rock, Vec2::New(5.0F * static_cast<float>(kTileSize), 20.0F * static_cast<float>(kTileSize) - 8.0F)) ||
        !spawn(EntityType::Pot, Vec2::New(9.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize))) ||
        !spawn(EntityType::Box, Vec2::New(11.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize))) ||
        !spawn(EntityType::Snake, Vec2::New(16.0F * static_cast<float>(kTileSize), 19.0F * static_cast<float>(kTileSize)))) {
        failed_step = "spawn broad scenario entities";
        return false;
    }

    return true;
}

bool AddSecondLocalPlayerForDeterministicReplay(State& state, Graphics& graphics) {
    constexpr PlayerId kSecondPlayerId = 2;
    (void)state.players.EnsureLocalPlayer(kSecondPlayerId, "Player 2", false);

    Vec2 spawn_pos = Vec2::New(32.0F, 32.0F);
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal();
        primary != nullptr && primary->entity_vid.has_value()) {
        if (const Entity* const primary_entity = state.entity_manager.GetEntity(*primary->entity_vid)) {
            spawn_pos = primary_entity->pos + Vec2::New(16.0F, 0.0F);
        }
    }

    const std::optional<VID> second_player_vid =
        SpawnPlayerForPlayerId(state, kSecondPlayerId, spawn_pos);
    if (!second_player_vid.has_value()) {
        return false;
    }
    state.UpdateSidForEntity(second_player_vid->id, graphics);
    return true;
}

void ApplyMultiLocalInputFrame(
    State& state,
    const std::array<PlayerInputFrame, 2>& input_frame
) {
    ApplyPrimaryInputFrame(state, input_frame[0]);
    state.players.SetInputFrameForPlayer(2, input_frame[1]);
}

std::vector<std::array<PlayerInputFrame, 2>> BuildInputLockstepSmokeScript() {
    std::vector<std::array<PlayerInputFrame, 2>> frames(1200);
    const std::vector<PlayerInputFrame> broad = BuildBroadDeterministicReplayInputScript();
    for (std::size_t i = 0; i < frames.size(); ++i) {
        PlayerInputFrame p1 = i < broad.size() ? broad[i] : PlayerInputFrame::New();
        PlayerInputFrame p2 = PlayerInputFrame::New();
        p2.mouse_pos = UVec2::New(300, 190);

        p2.left = (i >= 16 && i < 96) || (i >= 240 && i < 300);
        p2.right = (i >= 112 && i < 212) || (i >= 310 && i < 350);
        p2.run = i >= 150 && i < 220;
        p2.jump = (i >= 40 && i < 48) || (i >= 168 && i < 176);
        p2.pick_up_drop = (i >= 72 && i < 76) || (i >= 132 && i < 136);
        p2.attack = i >= 190 && i < 194;
        p2.down = i >= 300 && i < 322;

        frames[i] = {p1, p2};
    }
    return frames;
}

struct FakeLockstepInFlightPacket {
    network::LockstepPeerId from_peer = 0;
    network::LockstepPeerId to_peer = 0;
    network::LockstepInputPacket packet;
    std::uint64_t due_tick = 0;
    std::uint64_t insertion_order = 0;
};

struct FakeLockstepNetwork {
    network::NetFuzzerConfig fuzzer;
    network::NetFuzzerStats stats;
    DeterministicRng rng = DeterministicRng::New(1U);
    std::vector<FakeLockstepInFlightPacket> in_flight;
    std::uint64_t next_insertion_order = 0;

    static FakeLockstepNetwork New(const network::NetFuzzerConfig& config, std::uint32_t seed) {
        FakeLockstepNetwork network;
        network.fuzzer = config;
        network.rng = DeterministicRng::New(seed);
        return network;
    }

    bool RollPercent(float percent) {
        return percent > 0.0F && rng.RandomFloat(0.0F, 100.0F) < percent;
    }

    std::uint64_t ComputeDelayTicks() {
        if (!fuzzer.enabled) {
            return 0;
        }

        float delay_ms = fuzzer.latency_ms;
        if (fuzzer.jitter_ms > 0.0F) {
            delay_ms += rng.RandomFloat(-fuzzer.jitter_ms, fuzzer.jitter_ms);
        }
        delay_ms = std::max(0.0F, delay_ms);

        constexpr float kTickMs = 1000.0F / static_cast<float>(kFramesPerSecond);
        std::uint64_t delay_ticks = static_cast<std::uint64_t>(std::ceil(delay_ms / kTickMs));
        if (fuzzer.reorder_window_packets > 0) {
            const int reorder_window = static_cast<int>(fuzzer.reorder_window_packets);
            delay_ticks += static_cast<std::uint64_t>(rng.RandomIntInclusive(0, reorder_window));
        }
        return delay_ticks;
    }

    void QueueOne(
        std::uint64_t now_tick,
        network::LockstepPeerId from_peer,
        network::LockstepPeerId to_peer,
        const network::LockstepInputPacket& packet
    ) {
        FakeLockstepInFlightPacket in_flight_packet;
        in_flight_packet.from_peer = from_peer;
        in_flight_packet.to_peer = to_peer;
        in_flight_packet.packet = packet;
        in_flight_packet.due_tick = now_tick + ComputeDelayTicks();
        in_flight_packet.insertion_order = next_insertion_order++;
        in_flight.push_back(in_flight_packet);
    }

    void Send(
        std::uint64_t now_tick,
        network::LockstepPeerId from_peer,
        network::LockstepPeerId to_peer,
        const network::LockstepInputPacket& packet
    ) {
        stats.packets_sent += 1;
        if (RollPercent(fuzzer.packet_loss_percent)) {
            stats.packets_dropped += 1;
            return;
        }

        QueueOne(now_tick, from_peer, to_peer, packet);
        if (RollPercent(fuzzer.duplicate_percent)) {
            stats.packets_duplicated += 1;
            QueueOne(now_tick, from_peer, to_peer, packet);
        }
    }

    std::vector<network::LockstepInputPacket> ReceiveForPeer(
        std::uint64_t now_tick,
        network::LockstepPeerId peer_id
    ) {
        std::vector<FakeLockstepInFlightPacket> delivered;
        for (const FakeLockstepInFlightPacket& packet : in_flight) {
            if (packet.to_peer == peer_id && packet.due_tick <= now_tick) {
                delivered.push_back(packet);
            }
        }
        in_flight.erase(
            std::remove_if(
                in_flight.begin(),
                in_flight.end(),
                [&](const FakeLockstepInFlightPacket& packet) {
                    return packet.to_peer == peer_id && packet.due_tick <= now_tick;
                }
            ),
            in_flight.end()
        );

        std::sort(
            delivered.begin(),
            delivered.end(),
            [](const FakeLockstepInFlightPacket& lhs, const FakeLockstepInFlightPacket& rhs) {
                if (lhs.due_tick != rhs.due_tick) {
                    return lhs.due_tick < rhs.due_tick;
                }
                return lhs.insertion_order < rhs.insertion_order;
            }
        );

        std::vector<network::LockstepInputPacket> packets;
        packets.reserve(delivered.size());
        for (const FakeLockstepInFlightPacket& packet : delivered) {
            stats.packets_received += 1;
            packets.push_back(packet.packet);
        }
        return packets;
    }
};

struct FakeLockstepPeer {
    network::LockstepPeerId peer_id = 0;
    std::vector<PlayerId> owned_players;
    State state = State::New();
    network::LockstepInputBuffer input_buffer;
    network::LockstepFrame next_frame_to_step = 0;
    std::uint32_t next_packet_sequence = 1;
    std::vector<std::uint64_t> frame_hashes;
};

PlayerInputFrame GetLockstepScriptInput(
    const std::vector<std::array<PlayerInputFrame, 2>>& script,
    PlayerId player_id,
    network::LockstepFrame frame
) {
    if (frame >= script.size()) {
        return PlayerInputFrame::New();
    }
    const std::size_t frame_index = static_cast<std::size_t>(frame);
    if (player_id == 1) {
        return script[frame_index][0];
    }
    if (player_id == 2) {
        return script[frame_index][1];
    }
    return PlayerInputFrame::New();
}

network::LockstepInputPacket BuildLockstepInputPacket(
    FakeLockstepPeer& peer,
    const std::vector<std::array<PlayerInputFrame, 2>>& script,
    network::LockstepFrame latest_frame
) {
    network::LockstepInputPacket packet;
    packet.session_id = 0x51A7E001U;
    packet.stage_instance_id = 1U;
    packet.sender_peer_id = peer.peer_id;
    packet.sequence = peer.next_packet_sequence++;

    for (network::LockstepFrame frame = 0; frame <= latest_frame; ++frame) {
        for (PlayerId player_id : peer.owned_players) {
            network::LockstepInputRecord record;
            record.player_id = player_id;
            record.frame = frame;
            record.sequence = packet.sequence;
            record.input = GetLockstepScriptInput(script, player_id, frame);
            peer.input_buffer.Store(record);
            packet.records.push_back(record);
        }
    }
    return packet;
}

bool ConfigureLockstepSmokeOwnership(State& state, const std::vector<PlayerId>& owned_players) {
    for (PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        const bool owned =
            std::find(owned_players.begin(), owned_players.end(), slot.player_id) != owned_players.end();
        slot.connection_kind = owned ? PlayerConnectionKind::Local : PlayerConnectionKind::Remote;
        slot.primary_local = false;
    }

    if (!owned_players.empty()) {
        if (PlayerSlot* const primary = state.players.Find(owned_players.front())) {
            primary->primary_local = true;
        }
    }
    return state.players.FindPrimaryLocal() != nullptr;
}

bool PrepareLockstepSmokeState(
    State& state,
    Graphics& graphics,
    const std::vector<PlayerId>& owned_players,
    const char*& failed_step
) {
    constexpr std::uint32_t seed = 12345;
    state.net_session.input_lockstep_enabled = true;
    if (!LoadQuestStage(state, "classic", "classic_mines_1", false, seed)) {
        failed_step = "load stage";
        return false;
    }
    if (!PrepareBroadDeterministicReplayScenario(state, failed_step)) {
        return false;
    }
    if (!AddSecondLocalPlayerForDeterministicReplay(state, graphics)) {
        failed_step = "spawn second player";
        return false;
    }
    if (!ConfigureLockstepSmokeOwnership(state, owned_players)) {
        failed_step = "configure lockstep ownership";
        return false;
    }
    return true;
}

void ApplyLockstepInputsToState(
    State& state,
    const std::vector<PlayerId>& player_ids,
    const std::vector<PlayerInputFrame>& input_frames
) {
    const PlayerSlot* const primary_slot = state.players.FindPrimaryLocal();
    const PlayerId primary_player_id =
        primary_slot != nullptr ? primary_slot->player_id : kInvalidPlayerId;

    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        if (player_ids[i] == primary_player_id) {
            ApplyPrimaryInputFrame(state, input_frames[i]);
        } else {
            state.players.SetInputFrameForPlayer(player_ids[i], input_frames[i]);
        }
    }
}

bool StepReadyLockstepFrames(
    FakeLockstepPeer& peer,
    const std::vector<PlayerId>& required_players,
    Audio& audio,
    Graphics& graphics,
    network::LockstepFrame total_frames,
    std::string& error
) {
    std::vector<PlayerInputFrame> frame_inputs;
    while (peer.next_frame_to_step < total_frames &&
           peer.input_buffer.BuildFrameInputs(
               required_players,
               peer.next_frame_to_step,
               frame_inputs
           )) {
        ApplyLockstepInputsToState(peer.state, required_players, frame_inputs);
        StepSingleTick(peer.state, audio, graphics);
        const CanonicalStateFingerprint fingerprint =
            ComputeGameplayDeterminismFingerprint(peer.state);
        peer.frame_hashes.push_back(fingerprint.value);
        peer.next_frame_to_step += 1;

        if (peer.next_frame_to_step > 4) {
            peer.input_buffer.ClearBefore(peer.next_frame_to_step - 4);
        }
    }

    if (peer.next_frame_to_step > peer.frame_hashes.size()) {
        error = "lockstep peer advanced without recording hash";
        return false;
    }
    return true;
}

} // namespace

bool CheckStateFingerprintSmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State state = State::New();
        if (!LoadQuestStage(state, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state fingerprint smoke failed: could not load test stage\n";
            return false;
        }

        CanonicalStateFingerprint first_fingerprint = ComputeCanonicalStateFingerprint(state);
        CanonicalStateFingerprint second_fingerprint = ComputeCanonicalStateFingerprint(state);
        if (first_fingerprint.value != second_fingerprint.value) {
            std::cerr << "state fingerprint smoke failed: fingerprint is unstable without mutation\n"
                      << "  first  " << first_fingerprint.summary << " hash="
                      << first_fingerprint.value << "\n"
                      << "  second " << second_fingerprint.summary << " hash="
                      << second_fingerprint.value << "\n";
            return false;
        }

        const IVec2 rope_tile_pos = IVec2::New(2, 2);
        const Entity* source = state.entity_manager.entities.empty()
            ? nullptr
            : &state.entity_manager.entities.front();
        if (source == nullptr) {
            std::cerr << "state fingerprint smoke failed: no source entity\n";
            return false;
        }

        (void)world_ops::PlaceRopeTile(state, *source, rope_tile_pos);

        first_fingerprint = ComputeCanonicalStateFingerprint(state);
        second_fingerprint = ComputeCanonicalStateFingerprint(state);
        if (first_fingerprint.value != second_fingerprint.value) {
            std::cerr << "state fingerprint smoke failed: fingerprint is unstable after world_ops mutation\n"
                      << "  first  " << first_fingerprint.summary << " hash="
                      << first_fingerprint.value << "\n"
                      << "  second " << second_fingerprint.summary << " hash="
                      << second_fingerprint.value << "\n";
            return false;
        }

        std::cout << "state fingerprint smoke ok: "
                  << first_fingerprint.summary
                  << " hash=" << first_fingerprint.value << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "state fingerprint smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckStateEqualitySmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State left = State::New();
        State right = State::New();
        if (!LoadQuestStage(left, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(right, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "state equality smoke failed: could not load test stages\n";
            return false;
        }

        if (!CompareCanonicalFingerprints(left, right, "after load")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(left, right) << '\n';
            return false;
        }

        const char* failed_step = nullptr;
        if (!ApplyDeterministicWorldOpsSmokeMutations(left, failed_step)) {
            std::cerr << "state equality smoke failed on left mutation: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!ApplyDeterministicWorldOpsSmokeMutations(right, failed_step)) {
            std::cerr << "state equality smoke failed on right mutation: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }

        if (!CompareCanonicalFingerprints(left, right, "after canonical world_ops mutations")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(left, right) << '\n';
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "state equality smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckDeterministicReplaySmoke() {
    try {
        Graphics graphics;
        InitCliSmokeRuntimeTables(graphics);
        Audio audio;

        constexpr std::uint32_t seed = 12345;
        State recorded = State::New();
        State replayed = State::New();
        if (!LoadQuestStage(recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "deterministic replay smoke failed: could not load test stages\n";
            return false;
        }

        if (!CompareCanonicalFingerprints(recorded, replayed, "deterministic replay initial")) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(recorded, replayed) << '\n';
            return false;
        }

        const std::vector<PlayerInputFrame> inputs = BuildDeterministicReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < inputs.size(); ++frame_index) {
            ApplyPrimaryInputFrame(recorded, inputs[frame_index]);
            ApplyPrimaryInputFrame(replayed, inputs[frame_index]);
            StepSingleTick(recorded, audio, graphics);
            StepSingleTick(replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "deterministic replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(recorded, replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint final_fingerprint =
            ComputeGameplayDeterminismFingerprint(recorded);
        std::cout << "deterministic replay smoke ok: frames=" << inputs.size()
                  << " " << final_fingerprint.summary
                  << " hash=" << final_fingerprint.value << '\n';

        State multi_recorded = State::New();
        State multi_replayed = State::New();
        if (!LoadQuestStage(multi_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(multi_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "deterministic multi-local replay smoke failed: could not load test stages\n";
            return false;
        }
        if (!AddSecondLocalPlayerForDeterministicReplay(multi_recorded, graphics) ||
            !AddSecondLocalPlayerForDeterministicReplay(multi_replayed, graphics)) {
            std::cerr << "deterministic multi-local replay smoke failed: could not spawn second player\n";
            return false;
        }
        if (!CompareCanonicalFingerprints(
                multi_recorded,
                multi_replayed,
                "deterministic multi-local replay initial"
            )) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(multi_recorded, multi_replayed) << '\n';
            return false;
        }

        const std::vector<std::array<PlayerInputFrame, 2>> multi_inputs =
            BuildDeterministicMultiLocalReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < multi_inputs.size(); ++frame_index) {
            ApplyMultiLocalInputFrame(multi_recorded, multi_inputs[frame_index]);
            ApplyMultiLocalInputFrame(multi_replayed, multi_inputs[frame_index]);
            StepSingleTick(multi_recorded, audio, graphics);
            StepSingleTick(multi_replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(multi_recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(multi_replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "deterministic multi-local replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(multi_recorded, multi_replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint multi_final_fingerprint =
            ComputeGameplayDeterminismFingerprint(multi_recorded);
        std::cout << "deterministic multi-local replay smoke ok: frames="
                  << multi_inputs.size() << " " << multi_final_fingerprint.summary
                  << " hash=" << multi_final_fingerprint.value << '\n';

        State broad_recorded = State::New();
        State broad_replayed = State::New();
        if (!LoadQuestStage(broad_recorded, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(broad_replayed, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "deterministic broad replay smoke failed: could not load test stages\n";
            return false;
        }
        const char* failed_step = nullptr;
        if (!PrepareBroadDeterministicReplayScenario(broad_recorded, failed_step) ||
            !PrepareBroadDeterministicReplayScenario(broad_replayed, failed_step)) {
            std::cerr << "deterministic broad replay smoke failed during setup: "
                      << (failed_step != nullptr ? failed_step : "unknown") << '\n';
            return false;
        }
        if (!CompareCanonicalFingerprints(
                broad_recorded,
                broad_replayed,
                "deterministic broad replay initial"
            )) {
            std::cerr << "  first simple diff: "
                      << DescribeFirstStateDifference(broad_recorded, broad_replayed) << '\n';
            return false;
        }

        const std::vector<PlayerInputFrame> broad_inputs =
            BuildBroadDeterministicReplayInputScript();
        for (std::size_t frame_index = 0; frame_index < broad_inputs.size(); ++frame_index) {
            ApplyPrimaryInputFrame(broad_recorded, broad_inputs[frame_index]);
            ApplyPrimaryInputFrame(broad_replayed, broad_inputs[frame_index]);
            StepSingleTick(broad_recorded, audio, graphics);
            StepSingleTick(broad_replayed, audio, graphics);

            const CanonicalStateFingerprint recorded_fingerprint =
                ComputeGameplayDeterminismFingerprint(broad_recorded);
            const CanonicalStateFingerprint replayed_fingerprint =
                ComputeGameplayDeterminismFingerprint(broad_replayed);
            if (recorded_fingerprint.value != replayed_fingerprint.value) {
                std::cerr << "deterministic broad replay smoke failed at frame "
                          << frame_index << "\n"
                          << "  recorded " << recorded_fingerprint.summary
                          << " hash=" << recorded_fingerprint.value << "\n"
                          << "  replayed " << replayed_fingerprint.summary
                          << " hash=" << replayed_fingerprint.value << "\n"
                          << "  first simple diff: "
                          << DescribeFirstStateDifference(broad_recorded, broad_replayed) << '\n';
                return false;
            }
        }

        const CanonicalStateFingerprint broad_final_fingerprint =
            ComputeGameplayDeterminismFingerprint(broad_recorded);
        std::cout << "deterministic broad replay smoke ok: frames="
                  << broad_inputs.size() << " " << broad_final_fingerprint.summary
                  << " hash=" << broad_final_fingerprint.value << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "deterministic replay smoke failed: " << e.what() << '\n';
        return false;
    }
}

bool CheckInputLockstepSmoke() {
    try {
        Graphics peer0_graphics;
        Graphics peer1_graphics;
        InitCliSmokeRuntimeTables(peer0_graphics);
        InitCliSmokeRuntimeTables(peer1_graphics);
        Audio peer0_audio;
        Audio peer1_audio;

        const std::vector<std::array<PlayerInputFrame, 2>> script =
            BuildInputLockstepSmokeScript();
        const std::vector<PlayerId> required_players = {1, 2};

        const auto run_case = [&](
            const char* label,
            const network::NetFuzzerConfig& fuzzer,
            std::uint32_t fuzzer_seed
        ) -> bool {
            const network::LockstepFrame total_frames =
                static_cast<network::LockstepFrame>(script.size());
            constexpr network::LockstepFrame kInputDelayFrames = 8;
            constexpr std::uint64_t kMaxWallTicks = 6000;

            FakeLockstepPeer peer0;
            peer0.peer_id = 0;
            peer0.owned_players = {1};
            FakeLockstepPeer peer1;
            peer1.peer_id = 1;
            peer1.owned_players = {2};

            const char* failed_step = nullptr;
            if (!PrepareLockstepSmokeState(peer0.state, peer0_graphics, peer0.owned_players, failed_step) ||
                !PrepareLockstepSmokeState(peer1.state, peer1_graphics, peer1.owned_players, failed_step)) {
                std::cerr << "input lockstep smoke " << label
                          << " failed during setup: "
                          << (failed_step != nullptr ? failed_step : "unknown") << '\n';
                return false;
            }

            if (!CompareCanonicalFingerprints(peer0.state, peer1.state, "input lockstep initial")) {
                std::cerr << "  first simple diff: "
                          << DescribeFirstStateDifference(peer0.state, peer1.state) << '\n';
                return false;
            }

            FakeLockstepNetwork network = FakeLockstepNetwork::New(fuzzer, fuzzer_seed);
            std::size_t compared_hashes = 0;
            std::string step_error;

            for (std::uint64_t wall_tick = 0; wall_tick < kMaxWallTicks; ++wall_tick) {
                const network::LockstepFrame latest_input_frame =
                    std::min<network::LockstepFrame>(
                        static_cast<network::LockstepFrame>(wall_tick) + kInputDelayFrames,
                        total_frames - 1
                    );

                const network::LockstepInputPacket p0_packet =
                    BuildLockstepInputPacket(peer0, script, latest_input_frame);
                const network::LockstepInputPacket p1_packet =
                    BuildLockstepInputPacket(peer1, script, latest_input_frame);
                network.Send(wall_tick, peer0.peer_id, peer1.peer_id, p0_packet);
                network.Send(wall_tick, peer1.peer_id, peer0.peer_id, p1_packet);

                for (const network::LockstepInputPacket& packet :
                     network.ReceiveForPeer(wall_tick, peer0.peer_id)) {
                    for (const network::LockstepInputRecord& record : packet.records) {
                        peer0.input_buffer.Store(record);
                    }
                }
                for (const network::LockstepInputPacket& packet :
                     network.ReceiveForPeer(wall_tick, peer1.peer_id)) {
                    for (const network::LockstepInputRecord& record : packet.records) {
                        peer1.input_buffer.Store(record);
                    }
                }

                if (!StepReadyLockstepFrames(
                        peer0,
                        required_players,
                        peer0_audio,
                        peer0_graphics,
                        total_frames,
                        step_error
                    ) ||
                    !StepReadyLockstepFrames(
                        peer1,
                        required_players,
                        peer1_audio,
                        peer1_graphics,
                        total_frames,
                        step_error
                    )) {
                    std::cerr << "input lockstep smoke " << label
                              << " failed while stepping: " << step_error << '\n';
                    return false;
                }

                const std::size_t comparable_hashes =
                    std::min(peer0.frame_hashes.size(), peer1.frame_hashes.size());
                while (compared_hashes < comparable_hashes) {
                    if (peer0.frame_hashes[compared_hashes] !=
                        peer1.frame_hashes[compared_hashes]) {
                        std::cerr << "input lockstep smoke " << label
                                  << " hash mismatch at frame " << compared_hashes
                                  << "\n  peer0 hash=" << peer0.frame_hashes[compared_hashes]
                                  << "\n  peer1 hash=" << peer1.frame_hashes[compared_hashes]
                                  << "\n  first simple diff: "
                                  << DescribeFirstStateDifference(peer0.state, peer1.state)
                                  << '\n';
                        return false;
                    }
                    compared_hashes += 1;
                }

                if (peer0.next_frame_to_step >= total_frames &&
                    peer1.next_frame_to_step >= total_frames) {
                    const CanonicalStateFingerprint final_fingerprint =
                        ComputeGameplayDeterminismFingerprint(peer0.state);
                    std::cout << "input lockstep smoke " << label
                              << " ok: frames=" << total_frames
                              << " wall_ticks=" << (wall_tick + 1)
                              << " sent=" << network.stats.packets_sent
                              << " recv=" << network.stats.packets_received
                              << " drop=" << network.stats.packets_dropped
                              << " dup=" << network.stats.packets_duplicated
                              << " " << final_fingerprint.summary
                              << " hash=" << final_fingerprint.value << '\n';
                    return true;
                }
            }

            std::cerr << "input lockstep smoke " << label
                      << " timed out:"
                      << " peer0_frame=" << peer0.next_frame_to_step
                      << " peer1_frame=" << peer1.next_frame_to_step
                      << " sent=" << network.stats.packets_sent
                      << " recv=" << network.stats.packets_received
                      << " drop=" << network.stats.packets_dropped
                      << " dup=" << network.stats.packets_duplicated << '\n';
            return false;
        };

        network::NetFuzzerConfig clean;
        clean.enabled = false;
        if (!run_case("clean", clean, 0x1001U)) {
            return false;
        }

        network::NetFuzzerConfig impaired = network::NetFuzzerConfig::TexasToCaliforniaPreset();
        impaired.duplicate_percent = 4.0F;
        impaired.reorder_window_packets = 4;
        if (!run_case("impaired", impaired, 0x1002U)) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "input lockstep smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks
