#include "cli.hpp"

#include "cli_state_smoke.hpp"
#include "debug/playback.hpp"
#include "debug/debug_stage_builders.hpp"
#include "audio.hpp"
#include "ent/spec.hpp"
#include "aframe.hpp"
#include "graphics.hpp"
#include "quest.hpp"
#include "quest_stage_loader.hpp"
#include "raw_aframe.hpp"
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
#include "tools/tool_spec.hpp"
#include "utils.hpp"
#include "world_ops.hpp"

#include <SDL3/SDL_log.h>

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

void PrintAFrameSummary() {
    const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
    const AFrameDb aframe_db = AFrameDb::FromRaw(raw_file);

    std::cout << "raw frames: " << raw_file.sprites.size() << '\n';
    std::cout << "anims: " << aframe_db.anims.size() << '\n';
    std::cout << "frames: " << aframe_db.frames.size() << '\n';
    for (const AFrameAnim& anim : aframe_db.anims) {
        std::cout << "  " << anim.name << " (" << anim.frame_indices.size() << " frames";
        if (anim.tile) {
            std::cout << ", tile";
        }
        std::cout << ")\n";
    }
}

void PrintTileSourceDataSummary() {
    const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
    const AFrameDb aframe_db = AFrameDb::FromRaw(raw_file);
    const TileSourceDb tile_source_db = BuildTileSourceDb(aframe_db);

    std::cout << "tile images: " << aframe_db.image_paths.size() << '\n';
    std::cout << "tile sources: " << tile_source_db.sources.size() << '\n';
    std::cout << "tile spans: " << tile_source_db.tile_spans.size() << '\n';
    for (const std::string& image_path : aframe_db.image_paths) {
        std::cout << "  " << image_path << '\n';
    }
}

bool DumpRecordingAsText(const std::string& input_path, const std::string& output_path) {
    const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
    const AFrameDb aframe_db = AFrameDb::FromRaw(raw_file);
    std::string status;
    const bool ok = ConvertRecordingFileToText(input_path, output_path, aframe_db, &status);
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
            DetRng stagegen_rng = DetRng::New(1U);
            const Stage stage =
                stage_gen::classic::GenerateStage(quest, stage_def, stage_config, nullptr, stagegen_rng);
            const auto count_spawns = [&](EntType type_) {
                return static_cast<std::size_t>(
                    std::count_if(
                        stage.ent_spawns.begin(),
                        stage.ent_spawns.end(),
                        [type_](const EntSpawn& spawn) {
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
            const auto normalized_exit_id = [](const EntSpawn& spawn) -> std::string_view {
                return spawn.exit_id.empty() ? std::string_view("default")
                                             : std::string_view(spawn.exit_id);
            };
            const auto count_basic_exit_spawns = [&](std::string_view exit_id) {
                return static_cast<std::size_t>(
                    std::count_if(
                        stage.ent_spawns.begin(),
                        stage.ent_spawns.end(),
                        [&](const EntSpawn& spawn) {
                            return spawn.type_ == EntType::BasicExit &&
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
            for (const EntSpawn& spawn : stage.ent_spawns) {
                if (spawn.type_ != EntType::BasicExit || stage.exits.empty()) {
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
                      << stage.ent_spawns.size() << " spawns, "
                      << stage.stagegen_annotations.size() << " annotations, "
                      << entrance_tiles << " entrances, "
                      << default_exit_spawns << " default exits, "
                      << count_block_tiles() << " block tiles, "
                      << count_spawns(EntType::Block) << " block spawns, "
                      << count_spawns(EntType::ArrowTrap) << " arrow traps, "
                      << count_spawns(EntType::SacAltar) / 2 << " sac altars\n";
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
            for (StageSample& sample : samples) {
                const QuestStageDefinition* const stage_def = quest.FindStage(sample.id);
                if (stage_def == nullptr) {
                    continue;
                }
                const StageConfig stage_config =
                    LoadStageConfig(GetClassicQuestRootPath(), stage_def->stage_file);
                DetRng stagegen_rng = DetRng::New(static_cast<std::uint32_t>(run + 1));
                const Stage stage = stage_gen::classic::GenerateStage(
                    quest, *stage_def, stage_config, nullptr, stagegen_rng);
                const auto sac_altar_halves = static_cast<int>(
                    std::count_if(
                        stage.ent_spawns.begin(),
                        stage.ent_spawns.end(),
                        [](const EntSpawn& spawn) {
                            return spawn.type_ == EntType::SacAltar;
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
                    SpawnStageEntAtCenter(state, EntType::Monkey, Vec2::New(center_x, center_y))) {
                if (Ent* const monkey = state.ents.GetEntMut(*monkey_vid)) {
                    monkey->facing = ((room_x + room_y) % 2 == 0) ? Side::Left : Side::Right;
                }
            }
        }
    }
}

bool SampleMonkeyTest(int frames, bool big_stage) {
    try {
        Graphics graphics;
        const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
        graphics.aframe_db = AFrameDb::FromRaw(raw_file);
        graphics.tile_source_db = BuildTileSourceDb(graphics.aframe_db);

        PopulateEntSpecsTable();
        SyncEntSpecSizesFromAFrame(graphics);
        PopulateToolSpecsTable();

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
        for (const Ent& ent : state.ents.ents) {
            if (!ent.active || ent.type_ != EntType::Monkey) {
                continue;
            }

            const float center_y = ent.GetCenter().y;
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

} // namespace

bool RunCliCommand(int argc, char** argv) {
    if (argc < 2) {
        return false;
    }

    const std::string command = argv[1];
    if (command == "--check-frame-data") {
        PrintAFrameSummary();
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
        const bool ok = CheckStateFingerprintSmoke();
        SDL_Log("%s", ok ? "state fingerprint smoke ok" : "state fingerprint smoke failed");
        std::exit(ok ? 0 : 1);
    }

    if (command == "--check-state-equality-smoke") {
        std::exit(CheckStateEqualitySmoke() ? 0 : 1);
    }

    if (command == "--check-det-replay-smoke") {
        std::exit(CheckDetReplaySmoke() ? 0 : 1);
    }

    if (command == "--check-input-lockstep-smoke") {
        std::exit(CheckInputLockstepSmoke() ? 0 : 1);
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
