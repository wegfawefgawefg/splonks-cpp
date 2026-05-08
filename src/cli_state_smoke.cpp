#include "cli_state_smoke.hpp"

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "frame_data.hpp"
#include "graphics.hpp"
#include "quest_stage_loader.hpp"
#include "raw_frame_data.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"
#include "tile_source_data.hpp"
#include "tools/tool_archetype.hpp"
#include "world_ops.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

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

} // namespace splonks
