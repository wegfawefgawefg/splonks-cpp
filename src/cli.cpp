#include "cli.hpp"

#include "debug/playback.hpp"
#include "frame_data.hpp"
#include "quest.hpp"
#include "raw_frame_data.hpp"
#include "stage_gen/classic/stagegen.hpp"
#include "stage_gen/classic/tile_palette.hpp"
#include "stage_gen/room_template_loader.hpp"
#include "tile.hpp"
#include "tile_source_data.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
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
