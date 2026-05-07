#include "cli.hpp"

#include "debug/playback.hpp"
#include "debug/debug_stage_builders.hpp"
#include "audio.hpp"
#include "entity/archetype.hpp"
#include "frame_data.hpp"
#include "graphics.hpp"
#include "network/net_event_apply.hpp"
#include "quest.hpp"
#include "quest_stage_loader.hpp"
#include "raw_frame_data.hpp"
#include "stage_init.hpp"
#include "stage_gen/classic/stagegen.hpp"
#include "stage_gen/classic/tile_palette.hpp"
#include "stage_gen/room_template_loader.hpp"
#include "stage_spawning.hpp"
#include "state_fingerprint.hpp"
#include "step.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_source_data.hpp"
#include "tools/tool_archetype.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace splonks {

namespace {

constexpr const char* kAnnotationsYamlPath = "assets/graphics/annotations.yaml";

void PrintFrameDataSummary() {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    const FrameDataDb frame_data_db = FrameDataDb::FromRaw(raw_file);

    std::cout << "raw frames: " << raw_file.sprites.size() << '\n';
    std::cout << "animations: " << frame_data_db.animations.size() << '\n';
    std::cout << "frames: " << frame_data_db.frames.size() << '\n';
    for (const FrameDataAnimation& animation : frame_data_db.animations) {
        std::cout << "  " << animation.name << " (" << animation.frame_indices.size() << " frames";
        if (animation.tile) {
            std::cout << ", tile";
        }
        std::cout << ")\n";
    }
}

void PrintTileSourceDataSummary() {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    const FrameDataDb frame_data_db = FrameDataDb::FromRaw(raw_file);
    const TileSourceDb tile_source_db = BuildTileSourceDb(frame_data_db);

    std::cout << "tile images: " << frame_data_db.image_paths.size() << '\n';
    std::cout << "tile sources: " << tile_source_db.sources.size() << '\n';
    std::cout << "tile spans: " << tile_source_db.tile_spans.size() << '\n';
    for (const std::string& image_path : frame_data_db.image_paths) {
        std::cout << "  " << image_path << '\n';
    }
}

bool DumpRecordingAsText(const std::string& input_path, const std::string& output_path) {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    const FrameDataDb frame_data_db = FrameDataDb::FromRaw(raw_file);
    std::string status;
    const bool ok = ConvertRecordingFileToText(input_path, output_path, frame_data_db, &status);
    std::cout << status << '\n';
    return ok;
}

void CheckClassicGlyphCoverage(const StageConfig& stage_config) {
    const GlyphMap glyph_map = LoadGlyphMap(GetClassicQuestRootPath(), stage_config.glyphs_path);
    std::set<std::string> checked_pool_paths;
    for (const auto& [pool_name, pool_path] : stage_config.room_pools) {
        if (!checked_pool_paths.insert(pool_path).second) {
            continue;
        }

        const std::filesystem::path absolute_pool_path =
            std::filesystem::path(GetClassicQuestRootPath()) / pool_path;
        const std::vector<stage_gen::RoomTemplate> rooms =
            stage_gen::LoadRoomTemplatePool(absolute_pool_path.string());
        if (rooms.empty()) {
            throw std::runtime_error(stage_config.id + " room pool configured but empty: " +
                                     pool_path);
        }

        for (const stage_gen::RoomTemplate& room : rooms) {
            std::unordered_set<char> seen;
            for (const char glyph : room.grid) {
                if (glyph == '\n' || glyph == '\r') {
                    continue;
                }
                if (!seen.insert(glyph).second) {
                    continue;
                }
                if (glyph_map.Find(glyph) == nullptr) {
                    throw std::runtime_error(stage_config.id + " room " + room.source_path +
                                             " uses unmapped glyph: " + std::string(1, glyph));
                }
            }
        }
    }
}

bool CheckClassicQuestStagegen() {
    try {
        const QuestDefinition quest = LoadQuestDefinition(std::string(GetClassicQuestRootPath()) + "/quest.yaml");
        for (const QuestStageDefinition& stage_def : quest.stages) {
            const StageConfig stage_config = LoadStageConfig(GetClassicQuestRootPath(), stage_def.stage_file);
            CheckClassicGlyphCoverage(stage_config);
            const Stage stage = stage_gen::classic::GenerateStage(quest, stage_def, stage_config);
            const auto count_spawns = [&](EntityType type_) {
                return static_cast<std::size_t>(
                    std::count_if(
                        stage.entity_spawns.begin(),
                        stage.entity_spawns.end(),
                        [type_](const StageEntitySpawn& spawn) {
                            return spawn.type_ == type_;
                        }
                    )
                );
            };
            const auto count_block_tiles = [&]() {
                std::size_t count = 0;
                for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
                    for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
                        if (stage_gen::classic::IsBlockTile(stage.GetTile(x, y))) {
                            ++count;
                        }
                    }
                }
                return count;
            };
            const auto count_tiles = [&](Tile tile) {
                std::size_t count = 0;
                for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
                    for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
                        if (stage.GetTile(x, y) == tile) {
                            ++count;
                        }
                    }
                }
                return count;
            };
            const auto normalized_exit_id = [](const StageEntitySpawn& spawn) -> std::string_view {
                return spawn.exit_id.empty() ? std::string_view("default")
                                             : std::string_view(spawn.exit_id);
            };
            const auto count_basic_exit_spawns = [&](std::string_view exit_id) {
                return static_cast<std::size_t>(
                    std::count_if(
                        stage.entity_spawns.begin(),
                        stage.entity_spawns.end(),
                        [&](const StageEntitySpawn& spawn) {
                            return spawn.type_ == EntityType::BasicExit &&
                                   normalized_exit_id(spawn) == exit_id;
                        }
                    )
                );
            };

            const std::size_t entrance_tiles = count_tiles(Tile::Entrance);
            const bool validate_default_entrance_exit = stage_config.id != "olmec_lair";
            if (validate_default_entrance_exit && entrance_tiles != 1) {
                throw std::runtime_error(stage_def.id + " expected exactly 1 entrance tile, found " +
                                         std::to_string(entrance_tiles));
            }
            const std::size_t default_exit_spawns = count_basic_exit_spawns("default");
            if (validate_default_entrance_exit && default_exit_spawns != 1) {
                throw std::runtime_error(stage_def.id +
                                         " expected exactly 1 default BasicExit spawn, found " +
                                         std::to_string(default_exit_spawns));
            }
            for (const StageEntitySpawn& spawn : stage.entity_spawns) {
                if (spawn.type_ != EntityType::BasicExit || stage.exits.empty()) {
                    continue;
                }
                const std::string_view exit_id = normalized_exit_id(spawn);
                if (stage.FindExitId(exit_id) == kInvalidStageExitId) {
                    throw std::runtime_error(stage_def.id +
                                             " BasicExit references undeclared exit id: " +
                                             std::string(exit_id));
                }
            }

            std::cout << stage_def.id << ": "
                      << stage.entity_spawns.size() << " spawns, "
                      << stage.stagegen_annotations.size() << " annotations, "
                      << entrance_tiles << " entrances, "
                      << default_exit_spawns << " default exits, "
                      << count_block_tiles() << " block tiles, "
                      << count_spawns(EntityType::Block) << " block spawns, "
                      << count_spawns(EntityType::ArrowTrap) << " arrow traps, "
                      << count_spawns(EntityType::SacAltar) / 2 << " sac altars\n";
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "classic quest stagegen check failed: " << e.what() << '\n';
        return false;
    }
}

bool SampleClassicMinesAltars(int runs) {
    try {
        const QuestDefinition quest =
            LoadQuestDefinition(std::string(GetClassicQuestRootPath()) + "/quest.yaml");
        struct StageSample {
            std::string id;
            int generated = 0;
            std::map<int, int> altar_counts;
        };
        std::vector<StageSample> samples;
        for (const QuestStageDefinition& stage_def : quest.stages) {
            if (stage_def.id == "classic_mines_1" || stage_def.id == "classic_mines_2" ||
                stage_def.id == "classic_mines_3" || stage_def.id == "classic_mines_4") {
                samples.push_back(StageSample{
                    .id = stage_def.id,
                    .generated = 0,
                    .altar_counts = {},
                });
            }
        }

        for (int run = 0; run < runs; ++run) {
            rng::SetSeed(static_cast<std::uint32_t>(run + 1));
            for (StageSample& sample : samples) {
                const QuestStageDefinition* const stage_def = quest.FindStage(sample.id);
                if (stage_def == nullptr) {
                    continue;
                }
                const StageConfig stage_config =
                    LoadStageConfig(GetClassicQuestRootPath(), stage_def->stage_file);
                const Stage stage = stage_gen::classic::GenerateStage(quest, *stage_def, stage_config);
                const auto sac_altar_halves = static_cast<int>(
                    std::count_if(
                        stage.entity_spawns.begin(),
                        stage.entity_spawns.end(),
                        [](const StageEntitySpawn& spawn) {
                            return spawn.type_ == EntityType::SacAltar;
                        }
                    )
                );
                sample.generated += 1;
                sample.altar_counts[sac_altar_halves / 2] += 1;
            }
        }

        for (const StageSample& sample : samples) {
            const int with_altar = sample.generated -
                                   (sample.altar_counts.contains(0)
                                        ? sample.altar_counts.at(0)
                                        : 0);
            const double rate =
                sample.generated == 0 ? 0.0 : static_cast<double>(with_altar) / sample.generated;
            std::cout << sample.id << ": " << with_altar << "/" << sample.generated
                      << " = " << rate * 100.0 << "% with altar; counts";
            for (const auto& [altar_count, count] : sample.altar_counts) {
                std::cout << " " << altar_count << ":" << count;
            }
            std::cout << '\n';
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "classic mines altar sample failed: " << e.what() << '\n';
        return false;
    }
}

void SetCliStageTile(Stage& stage, int x, int y, Tile tile) {
    stage.SetTile(IVec2::New(x, y), tile);
}

void FillCliStageRect(Stage& stage, int left_x, int top_y, int right_x, int bottom_y, Tile tile) {
    for (int y = top_y; y <= bottom_y; ++y) {
        for (int x = left_x; x <= right_x; ++x) {
            SetCliStageTile(stage, x, y, tile);
        }
    }
}

void BuildCliStageLadder(Stage& stage, int x, int top_y, int bottom_y) {
    SetCliStageTile(stage, x, top_y, Tile::LadderTop);
    for (int y = top_y + 1; y <= bottom_y; ++y) {
        SetCliStageTile(stage, x, y, Tile::Ladder);
    }
}

Stage MakeBigMonkeySampleStage() {
    constexpr int width = 40;
    constexpr int height = 32;
    constexpr Tile wall_tile = Tile::CaveDirt;

    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(height),
        std::vector<Tile>(static_cast<std::size_t>(width), Tile::Air)
    );
    stage.FillBackwall(std::vector<Tile>{
        Tile::CaveAir0,
        Tile::CaveAir1,
        Tile::CaveAir2,
    });
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(wall_tile);
    stage.camera_clamp_enabled = true;
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;

    for (int x = 0; x < width; ++x) {
        SetCliStageTile(stage, x, 0, wall_tile);
        SetCliStageTile(stage, x, height - 1, wall_tile);
    }
    for (int y = 0; y < height; ++y) {
        SetCliStageTile(stage, 0, y, wall_tile);
        SetCliStageTile(stage, width - 1, y, wall_tile);
    }

    for (int room_y = 0; room_y < 4; ++room_y) {
        const int base_y = room_y * 8;
        FillCliStageRect(stage, 6, base_y + 6, 10, base_y + 6, wall_tile);
        FillCliStageRect(stage, 13, base_y + 4, 17, base_y + 4, wall_tile);
        FillCliStageRect(stage, 22, base_y + 5, 28, base_y + 5, wall_tile);
        FillCliStageRect(stage, 31, base_y + 3, 36, base_y + 3, wall_tile);
    }

    BuildCliStageLadder(stage, 4, 2, height - 2);
    BuildCliStageLadder(stage, 15, 1, height - 2);
    BuildCliStageLadder(stage, 25, 2, height - 2);
    BuildCliStageLadder(stage, 35, 1, height - 2);
    for (int y = 1; y <= height - 2; ++y) {
        SetCliStageTile(stage, 20, y, Tile::Rope);
    }

    return stage;
}

void InitBigMonkeySampleStage(State& state) {
    InitCommonStageState(state);

    for (int room_y = 0; room_y < 4; ++room_y) {
        for (int room_x = 0; room_x < 4; ++room_x) {
            const float center_x = static_cast<float>(room_x * 10 + 5) * static_cast<float>(kTileSize);
            const float center_y = static_cast<float>(room_y * 8 + 4) * static_cast<float>(kTileSize);
            if (const std::optional<VID> monkey_vid =
                    SpawnStageEntityAtCenter(state, EntityType::Monkey, Vec2::New(center_x, center_y))) {
                if (Entity* const monkey = state.entity_manager.GetEntityMut(*monkey_vid)) {
                    monkey->facing = ((room_x + room_y) % 2 == 0) ? LeftOrRight::Left : LeftOrRight::Right;
                }
            }
        }
    }
}

bool SampleMonkeyTest(int frames, bool big_stage) {
    try {
        Graphics graphics;
        const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
        graphics.frame_data_db = FrameDataDb::FromRaw(raw_file);
        graphics.tile_source_db = BuildTileSourceDb(graphics.frame_data_db);

        PopulateEntityArchetypesTable();
        SyncEntityArchetypeSizesFromFrameData(graphics);
        PopulateToolArchetypesTable();

        rng::SetSeed(1);
        State state = State::New();
        if (big_stage) {
            state.stage = MakeBigMonkeySampleStage();
            InitBigMonkeySampleStage(state);
        } else {
            state.debug_level.kind = DebugLevelKind::MonkeyTest;
            InitDebugLevel(state, false);
        }

        Audio audio;
        for (int frame = 0; frame < frames; ++frame) {
            StepSingleTick(state, audio, graphics);
            state.audio_emitters.ClearAll();
        }

        int count = 0;
        float min_y = 0.0F;
        float max_y = 0.0F;
        float sum_y = 0.0F;
        std::array<int, 4> y_bins{};
        const int tile_height = std::max(1, static_cast<int>(state.stage.GetTileHeight()));
        for (const Entity& entity : state.entity_manager.entities) {
            if (!entity.active || entity.type_ != EntityType::Monkey) {
                continue;
            }

            const float center_y = entity.GetCenter().y;
            if (count == 0) {
                min_y = center_y;
                max_y = center_y;
            } else {
                min_y = std::min(min_y, center_y);
                max_y = std::max(max_y, center_y);
            }
            sum_y += center_y;
            count += 1;

            const int tile_y = std::clamp(
                static_cast<int>(center_y / static_cast<float>(kTileSize)),
                0,
                tile_height - 1
            );
            y_bins[static_cast<std::size_t>(std::min((tile_y * 4) / tile_height, 3))] += 1;
        }

        if (count == 0) {
            std::cerr << "monkey test failed: no active monkeys\n";
            return false;
        }

        std::cout << (big_stage ? "big monkey test" : "monkey test")
                  << " after " << frames << " frames: "
                  << count << " monkeys, y min/avg/max = "
                  << std::fixed << std::setprecision(1)
                  << min_y << "/"
                  << (sum_y / static_cast<float>(count)) << "/"
                  << max_y
                  << ", normalized y bins top-to-bottom = "
                  << y_bins[0] << "/"
                  << y_bins[1] << "/"
                  << y_bins[2] << "/"
                  << y_bins[3] << '\n';
        return true;
    } catch (const std::exception& e) {
        std::cerr << "monkey test sample failed: " << e.what() << '\n';
        return false;
    }
}

void InitCliRuntimeTables(Graphics& graphics) {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    graphics.frame_data_db = FrameDataDb::FromRaw(raw_file);
    graphics.tile_source_db = BuildTileSourceDb(graphics.frame_data_db);

    PopulateEntityArchetypesTable();
    SyncEntityArchetypeSizesFromFrameData(graphics);
    PopulateToolArchetypesTable();
}

bool CheckStateFingerprintSmoke() {
    try {
        Graphics graphics;
        InitCliRuntimeTables(graphics);

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

bool CheckStateEqualitySmoke() {
    try {
        Graphics graphics;
        InitCliRuntimeTables(graphics);

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

void ConfigureProtocolSmokeCoordinator(State& state) {
    state.net_session.role = network::NetRole::Coordinator;
    state.net_session.local_player_id = 1;
    state.net_session.coordinator_player_id = 1;
    state.net_session.stage_instance_id = 1;
    state.net_session.next_expected_coordinator_order = 1;
}

void ConfigureProtocolSmokePeer(State& state) {
    state.net_session.role = network::NetRole::Peer;
    state.net_session.local_player_id = 2;
    state.net_session.coordinator_player_id = 1;
    state.net_session.stage_instance_id = 1;
    state.net_session.next_expected_coordinator_order = 1;
}

bool ApplyCoordinatorEventsToPeer(State& coordinator, State& peer, const char* label) {
    peer.net_session.ordered_events = coordinator.net_session.ordered_events;
    const std::size_t applied = network::ApplyOrderedEvents(peer.net_session, peer);
    if (applied == 0 && !coordinator.net_session.ordered_events.empty()) {
        std::cerr << "network protocol smoke failed at " << label
                  << ": peer applied 0 of " << coordinator.net_session.ordered_events.size()
                  << " queued coordinator events\n";
        return false;
    }
    coordinator.net_session.ordered_events.clear();
    return true;
}

bool CompareProtocolSmokeStates(const State& coordinator, const State& peer, const char* label) {
    if (CompareCanonicalFingerprints(coordinator, peer, label)) {
        return true;
    }
    std::cerr << "  first simple diff: "
              << DescribeFirstStateDifference(coordinator, peer) << '\n';
    return false;
}

bool CheckNetworkProtocolApplySmoke() {
    try {
        Graphics graphics;
        InitCliRuntimeTables(graphics);

        constexpr std::uint32_t seed = 12345;
        State coordinator = State::New();
        State peer = State::New();
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network protocol smoke failed: could not load test stages\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!CompareProtocolSmokeStates(coordinator, peer, "protocol after load")) {
            return false;
        }

        const Entity* source = FindFirstActiveEntity(coordinator);
        if (source == nullptr) {
            std::cerr << "network protocol smoke failed: no source entity\n";
            return false;
        }

        if (!world_ops::SetForegroundTile(coordinator, IVec2::New(3, 3), Tile::Rope)) {
            std::cerr << "network protocol smoke failed: coordinator did not set tile\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "tile changed") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol tile changed")) {
            return false;
        }

        if (!world_ops::PlaceRopeTile(coordinator, *source, IVec2::New(4, 3))) {
            std::cerr << "network protocol smoke failed: coordinator did not place rope tile\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "rope tile placed") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol rope tile placed")) {
            return false;
        }

        Entity* rock = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [](Entity& entity) {
                entity.pos = Vec2::New(96.0F, 64.0F);
                entity.vel = Vec2::New(1.0F, -2.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (rock == nullptr) {
            std::cerr << "network protocol smoke failed: coordinator did not spawn rock\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity spawned") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity spawned")) {
            return false;
        }

        rock->vel = Vec2::New(2.0F, -1.0F);
        world_ops::PatchEntityState(coordinator, *rock, *rock);
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity state patched") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity state patched")) {
            return false;
        }

        if (!world_ops::DeactivateEntity(coordinator, rock->vid)) {
            std::cerr << "network protocol smoke failed: coordinator did not deactivate rock\n";
            return false;
        }
        if (!ApplyCoordinatorEventsToPeer(coordinator, peer, "entity deactivated") ||
            !CompareProtocolSmokeStates(coordinator, peer, "protocol entity deactivated")) {
            return false;
        }

        std::cout << "network protocol apply smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network protocol smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace

bool RunCliCommand(int argc, char** argv) {
    if (argc < 2) {
        return false;
    }

    const std::string command = argv[1];
    if (command == "--check-frame-data") {
        PrintFrameDataSummary();
        return true;
    }

    if (command == "--check-tile-source-data") {
        PrintTileSourceDataSummary();
        return true;
    }

    if (command == "--check-classic-quest-stagegen") {
        std::exit(CheckClassicQuestStagegen() ? 0 : 1);
    }

    if (command == "--sample-classic-mines-altars") {
        const int runs = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 1000;
        std::exit(SampleClassicMinesAltars(runs) ? 0 : 1);
    }

    if (command == "--sample-monkey-test") {
        const int frames = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 3600;
        std::exit(SampleMonkeyTest(frames, false) ? 0 : 1);
    }

    if (command == "--sample-monkey-test-big") {
        const int frames = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 3600;
        std::exit(SampleMonkeyTest(frames, true) ? 0 : 1);
    }

    if (command == "--check-state-fingerprint-smoke") {
        std::exit(CheckStateFingerprintSmoke() ? 0 : 1);
    }

    if (command == "--check-state-equality-smoke") {
        std::exit(CheckStateEqualitySmoke() ? 0 : 1);
    }

    if (command == "--check-network-protocol-smoke") {
        std::exit(CheckNetworkProtocolApplySmoke() ? 0 : 1);
    }

    if (command == "--dump-recording-text") {
        if (argc < 4) {
            std::cerr << "usage: --dump-recording-text <input.splrec> <output.txt>\n";
            return true;
        }
        return DumpRecordingAsText(argv[2], argv[3]);
    }

    return false;
}

} // namespace splonks
