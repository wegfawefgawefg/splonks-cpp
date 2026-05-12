#include "cli_state_smoke.hpp"

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "frame_data.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
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
            a.type_ != b.type_) {
            std::ostringstream output;
            output << "entity " << i << " identity differs:"
                   << " active " << a.active << "/" << b.active
                   << " type " << static_cast<int>(a.type_) << "/" << static_cast<int>(b.type_);
            return output.str();
        }
        if (!a.active) {
            continue;
        }
        if (a.pos != b.pos ||
            a.vel != b.vel ||
            a.acc != b.acc ||
            a.health != b.health ||
            a.frame_data_animator.animation_id != b.frame_data_animator.animation_id ||
            a.frame_data_animator.current_frame != b.frame_data_animator.current_frame ||
            a.frame_data_animator.current_time != b.frame_data_animator.current_time ||
            a.frame_data_animator.speed != b.frame_data_animator.speed) {
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
                   << " anim " << a.frame_data_animator.animation_id
                   << "/" << b.frame_data_animator.animation_id;
            return output.str();
        }
    }

    if (left.players.slots.size() != right.players.slots.size()) {
        std::ostringstream output;
        output << "player slot count differs: left=" << left.players.slots.size()
               << " right=" << right.players.slots.size();
        return output.str();
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

} // namespace splonks
