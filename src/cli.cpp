#include "cli.hpp"

#include "cli_state_smoke.hpp"
#include "debug/playback.hpp"
#include "debug/debug_stage_builders.hpp"
#include "audio.hpp"
#include "ent/spec.hpp"
#include "aframe.hpp"
#include "graphics.hpp"
#include "gubsy_shell_smoke.hpp"
#include "network/input_lockstep.hpp"
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
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace splonks {

namespace {

constexpr const char* kAnnotationsYamlPath = "assets/graphics/annotations.yaml";
constexpr std::uint32_t kDesyncReplayMagic = 0x53445250U; // SDRP
constexpr std::uint32_t kDesyncReplayVersionMin = 1;
constexpr std::uint32_t kDesyncReplayVersionMax = 3;

struct DesyncReplayInputRecord {
    PlayerId player_id = kInvalidPlayerId;
    network::LockstepFrame frame = 0;
    std::uint32_t input_flags = 0;
    std::uint32_t mouse_x = 0;
    std::uint32_t mouse_y = 0;
};

struct DesyncReplayFile {
    network::StageInstanceId stage_instance_id = network::kInvalidStageInstanceId;
    std::uint32_t stage_seed = 0;
    network::LockstepFrame start_frame = 0;
    std::string quest_id;
    std::string quest_stage_id;
    PlayerId mismatch_peer_id = kInvalidPlayerId;
    network::LockstepFrame mismatch_frame = 0;
    std::uint64_t local_hash = 0;
    std::uint64_t remote_hash = 0;
    NetworkStateFingerprintComponents local_components;
    NetworkStateFingerprintComponents remote_components;
    std::vector<std::uint8_t> initial_snapshot_bytes;
    std::vector<DesyncReplayInputRecord> inputs;
    std::vector<NetworkEntFingerprint> captured_local_ent_hashes;
};

struct ReplayedDesyncFile {
    std::string path;
    DesyncReplayFile replay;
    Graphics graphics;
    State state;
    NetworkStateFingerprintComponents components;
    std::uint64_t hash = 0;
    std::vector<NetworkEntFingerprint> ent_hashes;
};

template <typename T>
bool ReadReplayPod(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return in.good();
}

bool ReadReplayString(std::istream& in, std::string& value) {
    std::uint32_t size = 0;
    if (!ReadReplayPod(in, size)) {
        return false;
    }
    value.resize(size);
    if (size > 0) {
        in.read(value.data(), static_cast<std::streamsize>(size));
    }
    return in.good();
}

bool LoadDesyncReplayFile(const std::string& path, DesyncReplayFile& replay, std::string* status) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        if (status != nullptr) {
            *status = "failed to open replay file";
        }
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!ReadReplayPod(in, magic) || !ReadReplayPod(in, version) ||
        magic != kDesyncReplayMagic || version < kDesyncReplayVersionMin ||
        version > kDesyncReplayVersionMax) {
        if (status != nullptr) {
            *status = "invalid desync replay header";
        }
        return false;
    }

    if (!ReadReplayPod(in, replay.stage_instance_id) ||
        !ReadReplayPod(in, replay.stage_seed) ||
        !ReadReplayPod(in, replay.start_frame) ||
        !ReadReplayString(in, replay.quest_id) ||
        !ReadReplayString(in, replay.quest_stage_id) ||
        !ReadReplayPod(in, replay.mismatch_peer_id) ||
        !ReadReplayPod(in, replay.mismatch_frame) ||
        !ReadReplayPod(in, replay.local_hash) ||
        !ReadReplayPod(in, replay.remote_hash) ||
        !ReadReplayPod(in, replay.local_components.root) ||
        !ReadReplayPod(in, replay.remote_components.root) ||
        !ReadReplayPod(in, replay.local_components.stage) ||
        !ReadReplayPod(in, replay.remote_components.stage) ||
        !ReadReplayPod(in, replay.local_components.players) ||
        !ReadReplayPod(in, replay.remote_components.players) ||
        !ReadReplayPod(in, replay.local_components.tools) ||
        !ReadReplayPod(in, replay.remote_components.tools) ||
        !ReadReplayPod(in, replay.local_components.ents) ||
        !ReadReplayPod(in, replay.remote_components.ents)) {
        if (status != nullptr) {
            *status = "failed to read replay metadata";
        }
        return false;
    }

    std::uint32_t snapshot_size = 0;
    if (!ReadReplayPod(in, snapshot_size)) {
        if (status != nullptr) {
            *status = "failed to read snapshot size";
        }
        return false;
    }
    replay.initial_snapshot_bytes.resize(snapshot_size);
    if (snapshot_size > 0) {
        in.read(
            reinterpret_cast<char*>(replay.initial_snapshot_bytes.data()),
            static_cast<std::streamsize>(snapshot_size)
        );
        if (!in.good()) {
            if (status != nullptr) {
                *status = "failed to read snapshot bytes";
            }
            return false;
        }
    }

    std::uint64_t input_count = 0;
    if (!ReadReplayPod(in, input_count)) {
        if (status != nullptr) {
            *status = "failed to read input count";
        }
        return false;
    }
    replay.inputs.resize(static_cast<std::size_t>(input_count));
    for (DesyncReplayInputRecord& input : replay.inputs) {
        if (!ReadReplayPod(in, input.player_id) ||
            !ReadReplayPod(in, input.frame) ||
            !ReadReplayPod(in, input.input_flags)) {
            if (status != nullptr) {
                *status = "failed to read input records";
            }
            return false;
        }
        if (version >= 2 &&
            (!ReadReplayPod(in, input.mouse_x) || !ReadReplayPod(in, input.mouse_y))) {
            if (status != nullptr) {
                *status = "failed to read input mouse records";
            }
            return false;
        }
    }
    if (version >= 3) {
        std::uint64_t ent_hash_count = 0;
        if (!ReadReplayPod(in, ent_hash_count)) {
            if (status != nullptr) {
                *status = "failed to read local entity hash count";
            }
            return false;
        }
        replay.captured_local_ent_hashes.resize(static_cast<std::size_t>(ent_hash_count));
        for (NetworkEntFingerprint& ent_hash : replay.captured_local_ent_hashes) {
            if (!ReadReplayPod(in, ent_hash.net_ent_id) ||
                !ReadReplayPod(in, ent_hash.type) ||
                !ReadReplayPod(in, ent_hash.hash)) {
                if (status != nullptr) {
                    *status = "failed to read local entity hashes";
                }
                return false;
            }
        }
    }
    return true;
}

Graphics MakeCliGraphics() {
    Graphics graphics;
    const RawAFrameFile raw_file = LoadRawAFrameFile(kAnnotationsYamlPath);
    graphics.aframe_db = AFrameDb::FromRaw(raw_file);
    graphics.tile_source_db = BuildTileSourceDb(graphics.aframe_db);
    graphics.tile_contact_db = BuildTileContactDb(graphics.tile_source_db);
    graphics.window_dims = UVec2::New(1920, 540);
    graphics.dims = UVec2::New(1920, 540);
    return graphics;
}

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
        Graphics graphics = MakeCliGraphics();

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

std::optional<network::LockstepFrame> ParseStopFrameArg(int argc, char** argv) {
    for (int i = 3; i + 1 < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "--stop-frame") {
            try {
                return static_cast<network::LockstepFrame>(std::stoull(argv[i + 1]));
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

void ApplyReplayInputsForFrame(
    State& state,
    const std::vector<DesyncReplayInputRecord>& inputs,
    std::size_t& input_index,
    network::LockstepFrame frame
) {
    const PlayerSlot* const primary_slot = state.players.FindPrimaryLocal();
    const PlayerId primary_player_id =
        primary_slot != nullptr ? primary_slot->player_id : kInvalidPlayerId;

    while (input_index < inputs.size() && inputs[input_index].frame < frame) {
        ++input_index;
    }
    while (input_index < inputs.size() && inputs[input_index].frame == frame) {
        const DesyncReplayInputRecord& input = inputs[input_index];
        const InputFrame input_frame = network::UnpackInputFrame(
            input.input_flags,
            UVec2::New(input.mouse_x, input.mouse_y)
        );
        if (input.player_id == primary_player_id) {
            state.playing_input_snapshot = ToPlayingInputSnapshot(input_frame);
        } else {
            state.players.SetInputFrameForPlayer(input.player_id, input_frame);
        }
        ++input_index;
    }
}

void PrintComponents(
    const char* label,
    const NetworkStateFingerprintComponents& components,
    std::uint64_t hash
) {
    std::cout << label
              << " hash=" << hash
              << " root=" << components.root
              << " stage=" << components.stage
              << " players=" << components.players
              << " tools=" << components.tools
              << " ents=" << components.ents
              << '\n';
}

std::string Vec2DebugString(Vec2 value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4) << "(" << value.x << "," << value.y << ")";
    return out.str();
}

std::string IVec2DebugString(IVec2 value) {
    return "(" + std::to_string(value.x) + "," + std::to_string(value.y) + ")";
}

std::string OptionalVidDebugString(const std::optional<VID>& value) {
    return value.has_value() ? std::to_string(value->id) : std::string("none");
}

const Ent* FindEntByNetId(const State& state, network::NetEntId net_ent_id) {
    if (net_ent_id == network::kInvalidNetEntId) {
        return nullptr;
    }
    const std::optional<VID> vid = state.net_session.FindLocalVid(net_ent_id);
    return vid.has_value() ? state.ents.GetEnt(*vid) : nullptr;
}

bool ReplayDesyncToState(
    const std::string& path,
    std::optional<network::LockstepFrame> stop_frame,
    ReplayedDesyncFile& result,
    std::string* status
) {
    result = ReplayedDesyncFile{};
    result.path = path;

    if (!LoadDesyncReplayFile(path, result.replay, status)) {
        return false;
    }

    SimSnapshot snapshot;
    if (!DeserializeSimSnapshotFromBytes(result.replay.initial_snapshot_bytes, snapshot)) {
        if (status != nullptr) {
            *status = "snapshot decode failed";
        }
        return false;
    }

    result.graphics = MakeCliGraphics();
    PopulateEntSpecsTable();
    SyncEntSpecSizesFromAFrame(result.graphics);
    PopulateToolSpecsTable();

    result.state = State::New();
    RestoreSimSnapshot(snapshot, result.state, result.graphics);
    result.state.net_session.role = network::NetRole::Offline;
    result.state.net_session.stage_instance_id = result.replay.stage_instance_id;
    result.state.net_session.quest_id = result.replay.quest_id;
    result.state.net_session.quest_stage_id = result.replay.quest_stage_id;
    result.state.net_session.stage_seed = result.replay.stage_seed;

    Audio audio;
    const network::LockstepFrame target_frame = stop_frame.value_or(result.replay.mismatch_frame);
    std::size_t input_index = 0;
    if (result.replay.start_frame <= target_frame) {
        for (network::LockstepFrame frame = result.replay.start_frame; frame <= target_frame; ++frame) {
            ApplyReplayInputsForFrame(result.state, result.replay.inputs, input_index, frame);
            StepSingleTickWithMode(
                result.state,
                audio,
                result.graphics,
                SimulationTickMode::ReplayNoNetwork
            );
            result.state.audio_emitters.ClearAll();
            if (frame == target_frame) {
                break;
            }
        }
    }

    result.components = ComputeNetworkStateFingerprintComponents(result.state);
    result.hash = CombineNetworkStateFingerprintComponents(result.components);
    result.ent_hashes = ComputeNetworkEntFingerprints(result.state);
    return true;
}

void PrintReplaySummary(
    const ReplayedDesyncFile& replayed,
    network::LockstepFrame target_frame
) {
    const DesyncReplayFile& replay = replayed.replay;
    std::cout << "desync replay: " << replayed.path << '\n'
              << "stage_instance=" << replay.stage_instance_id
              << " quest=" << replay.quest_id
              << " stage=" << replay.quest_stage_id
              << " seed=" << replay.stage_seed << '\n'
              << "start_frame=" << replay.start_frame
              << " target_frame=" << target_frame
              << " captured_mismatch_frame=" << replay.mismatch_frame
              << " inputs=" << replay.inputs.size() << '\n';
    PrintComponents("replayed", replayed.components, replayed.hash);
    PrintComponents("captured_local", replay.local_components, replay.local_hash);
    PrintComponents("captured_remote", replay.remote_components, replay.remote_hash);

    if (target_frame == replay.mismatch_frame) {
        std::cout << "replay_vs_captured_local="
                  << (replayed.hash == replay.local_hash ? "match" : "DIFFERENT")
                  << '\n';
    }
}

void PrintEntHashList(const char* label, const std::vector<NetworkEntFingerprint>& hashes) {
    std::cout << label << "=" << hashes.size() << '\n';
    const std::size_t limit = std::min<std::size_t>(hashes.size(), 64);
    for (std::size_t i = 0; i < limit; ++i) {
        const NetworkEntFingerprint& ent = hashes[i];
        std::cout << "  ent net=" << ent.net_ent_id
                  << " type=" << ent.type
                  << " hash=" << ent.hash << '\n';
    }
    if (hashes.size() > limit) {
        std::cout << "  ... " << (hashes.size() - limit) << " more\n";
    }
}

void PrintFieldDiff(const char* name, const std::string& left, const std::string& right) {
    if (left != right) {
        std::cout << "    " << name << ": " << left << " != " << right << '\n';
    }
}

void PrintFieldDiff(const char* name, bool left, bool right) {
    PrintFieldDiff(
        name,
        std::string(left ? "true" : "false"),
        std::string(right ? "true" : "false")
    );
}

template <typename T>
void PrintNumericFieldDiff(const char* name, T left, T right) {
    if (left != right) {
        std::cout << "    " << name << ": " << left << " != " << right << '\n';
    }
}

void PrintEntStateDiff(const Ent* left, const Ent* right) {
    if (left == nullptr || right == nullptr) {
        std::cout << "    presence: "
                  << (left != nullptr ? "present" : "missing") << " != "
                  << (right != nullptr ? "present" : "missing") << '\n';
        return;
    }

    PrintFieldDiff("active", left->active, right->active);
    PrintFieldDiff("grounded", left->grounded, right->grounded);
    PrintFieldDiff("holding", left->holding, right->holding);
    PrintFieldDiff("wanted", left->wanted, right->wanted);
    PrintNumericFieldDiff("type", static_cast<int>(left->type_), static_cast<int>(right->type_));
    PrintFieldDiff("pos", Vec2DebugString(left->pos), Vec2DebugString(right->pos));
    PrintFieldDiff("vel", Vec2DebugString(left->vel), Vec2DebugString(right->vel));
    PrintFieldDiff("acc", Vec2DebugString(left->acc), Vec2DebugString(right->acc));
    PrintFieldDiff("size", Vec2DebugString(left->size), Vec2DebugString(right->size));
    PrintNumericFieldDiff("rotation", left->rotation, right->rotation);
    PrintNumericFieldDiff("coyote_time", left->coyote_time, right->coyote_time);
    PrintNumericFieldDiff("stun_timer", left->stun_timer, right->stun_timer);
    PrintNumericFieldDiff("fall_timer", left->fall_timer, right->fall_timer);
    PrintNumericFieldDiff("facing", static_cast<int>(left->facing), static_cast<int>(right->facing));
    PrintNumericFieldDiff(
        "condition",
        static_cast<int>(left->condition),
        static_cast<int>(right->condition)
    );
    PrintNumericFieldDiff("ai_state", static_cast<int>(left->ai_state), static_cast<int>(right->ai_state));
    PrintNumericFieldDiff(
        "damage_vuln",
        static_cast<int>(left->damage_vuln),
        static_cast<int>(right->damage_vuln)
    );
    PrintNumericFieldDiff("movement_flags", left->movement_flags, right->movement_flags);
    PrintNumericFieldDiff("health", left->health, right->health);
    PrintFieldDiff("back_vid", OptionalVidDebugString(left->back_vid), OptionalVidDebugString(right->back_vid));
    PrintFieldDiff(
        "holding_vid",
        OptionalVidDebugString(left->holding_vid),
        OptionalVidDebugString(right->holding_vid)
    );
    PrintFieldDiff(
        "held_by_vid",
        OptionalVidDebugString(left->held_by_vid),
        OptionalVidDebugString(right->held_by_vid)
    );
    PrintFieldDiff("ent_a", OptionalVidDebugString(left->ent_a), OptionalVidDebugString(right->ent_a));
    PrintFieldDiff("ent_b", OptionalVidDebugString(left->ent_b), OptionalVidDebugString(right->ent_b));
    PrintFieldDiff("ent_c", OptionalVidDebugString(left->ent_c), OptionalVidDebugString(right->ent_c));
    PrintFieldDiff("ent_d", OptionalVidDebugString(left->ent_d), OptionalVidDebugString(right->ent_d));
    PrintNumericFieldDiff("stage_exit_id", left->stage_exit_id, right->stage_exit_id);
    PrintNumericFieldDiff("money", left->money, right->money);
    PrintNumericFieldDiff("counter_a", left->counter_a, right->counter_a);
    PrintNumericFieldDiff("counter_b", left->counter_b, right->counter_b);
    PrintNumericFieldDiff("counter_c", left->counter_c, right->counter_c);
    PrintNumericFieldDiff("counter_d", left->counter_d, right->counter_d);
    PrintNumericFieldDiff("light_strength", left->light_strength, right->light_strength);
    PrintNumericFieldDiff("light_radius", left->light_radius, right->light_radius);
    PrintFieldDiff("point_a", IVec2DebugString(left->point_a), IVec2DebugString(right->point_a));
    PrintFieldDiff("point_b", IVec2DebugString(left->point_b), IVec2DebugString(right->point_b));
    PrintNumericFieldDiff(
        "anim_id",
        static_cast<int>(left->aframe_animator.anim_id),
        static_cast<int>(right->aframe_animator.anim_id)
    );
    PrintNumericFieldDiff(
        "anim_frame",
        left->aframe_animator.current_frame,
        right->aframe_animator.current_frame
    );
    PrintNumericFieldDiff(
        "anim_time",
        left->aframe_animator.current_time,
        right->aframe_animator.current_time
    );
}

std::unordered_map<network::NetEntId, NetworkEntFingerprint> LinkedEntHashMap(
    const std::vector<NetworkEntFingerprint>& hashes
) {
    std::unordered_map<network::NetEntId, NetworkEntFingerprint> result;
    for (const NetworkEntFingerprint& hash : hashes) {
        if (hash.net_ent_id != network::kInvalidNetEntId) {
            result[hash.net_ent_id] = hash;
        }
    }
    return result;
}

bool CompareDesyncFiles(
    const std::string& left_path,
    const std::string& right_path,
    std::optional<network::LockstepFrame> stop_frame
) {
    try {
        ReplayedDesyncFile left;
        ReplayedDesyncFile right;
        std::string status;
        if (!ReplayDesyncToState(left_path, stop_frame, left, &status)) {
            std::cerr << "left desync replay failed: " << status << '\n';
            return false;
        }
        if (!ReplayDesyncToState(right_path, stop_frame, right, &status)) {
            std::cerr << "right desync replay failed: " << status << '\n';
            return false;
        }

        const network::LockstepFrame target_frame =
            stop_frame.value_or(std::min(left.replay.mismatch_frame, right.replay.mismatch_frame));
        std::cout << "compare desync replays at frame " << target_frame << '\n'
                  << "left=" << left_path << '\n'
                  << "right=" << right_path << '\n';
        PrintComponents("left_replayed", left.components, left.hash);
        PrintComponents("right_replayed", right.components, right.hash);

        if (left.hash == right.hash) {
            std::cout << "replayed_states=match\n";
            return true;
        }
        std::cout << "replayed_states=DIFFERENT\n";

        const auto left_hashes = LinkedEntHashMap(left.ent_hashes);
        const auto right_hashes = LinkedEntHashMap(right.ent_hashes);
        std::set<network::NetEntId> net_ids;
        for (const auto& [net_id, _] : left_hashes) {
            net_ids.insert(net_id);
        }
        for (const auto& [net_id, _] : right_hashes) {
            net_ids.insert(net_id);
        }

        std::size_t printed = 0;
        for (network::NetEntId net_id : net_ids) {
            const auto left_found = left_hashes.find(net_id);
            const auto right_found = right_hashes.find(net_id);
            const bool left_present = left_found != left_hashes.end();
            const bool right_present = right_found != right_hashes.end();
            const std::uint64_t left_hash = left_present ? left_found->second.hash : 0;
            const std::uint64_t right_hash = right_present ? right_found->second.hash : 0;
            if (left_present && right_present && left_hash == right_hash) {
                continue;
            }

            const Ent* left_ent = FindEntByNetId(left.state, net_id);
            const Ent* right_ent = FindEntByNetId(right.state, net_id);
            const EntType type = left_ent != nullptr
                ? left_ent->type_
                : (right_ent != nullptr ? right_ent->type_ : EntType::None);
            std::cout << "  differing ent net=" << net_id
                      << " type=" << GetEntTypeName(type)
                      << " left_hash=" << left_hash
                      << " right_hash=" << right_hash << '\n';
            PrintEntStateDiff(left_ent, right_ent);

            printed += 1;
            if (printed >= 16) {
                std::cout << "  ... more differing linked entities omitted\n";
                break;
            }
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "desync compare failed: " << e.what() << '\n';
        return false;
    }
}

bool ReplayDesyncFile(const std::string& path, std::optional<network::LockstepFrame> stop_frame) {
    try {
        ReplayedDesyncFile replayed;
        std::string status;
        if (!ReplayDesyncToState(path, stop_frame, replayed, &status)) {
            std::cerr << "desync replay load failed: " << status << '\n';
            return false;
        }
        const network::LockstepFrame target_frame =
            stop_frame.value_or(replayed.replay.mismatch_frame);
        PrintReplaySummary(replayed, target_frame);

        if (replayed.components.ents != replayed.replay.remote_components.ents ||
            replayed.components.ents != replayed.replay.local_components.ents) {
            PrintEntHashList("replayed_ent_hashes", replayed.ent_hashes);
        }
        if (!replayed.replay.captured_local_ent_hashes.empty()) {
            PrintEntHashList(
                "captured_local_ent_hashes",
                replayed.replay.captured_local_ent_hashes
            );
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "desync replay failed: " << e.what() << '\n';
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

    if (command == "--check-gameplay-snapshot-callback-rebind-smoke") {
        std::exit(CheckGameplaySnapshotCallbackRebindSmoke() ? 0 : 1);
    }

    if (command == "--check-det-replay-smoke") {
        std::exit(CheckDetReplaySmoke() ? 0 : 1);
    }

    if (command == "--check-network-fresh-reload-ownership-smoke") {
        std::exit(CheckNetworkFreshReloadOwnershipSmoke() ? 0 : 1);
    }

    if (command == "--check-input-lockstep-smoke") {
        std::exit(CheckInputLockstepSmoke() ? 0 : 1);
    }

    if (command == "--check-join-barrier-next-stage-restart-smoke") {
        std::exit(CheckJoinBarrierNextStageRestartSmoke() ? 0 : 1);
    }

    if (command == "--check-gubsy-shell-smoke") {
        std::exit(CheckGubsyShellSmoke() ? 0 : 1);
    }

    if (command == "--check-gubsy-shell-real-roomd-smoke") {
        std::exit(CheckGubsyShellRealRoomdSmoke() ? 0 : 1);
    }

    if (command == "--check-gubsy-shell-realnet-lan-host") {
        if (argc < 3) {
            std::cerr << "usage: --check-gubsy-shell-realnet-lan-host <room_server_url> [max_frames]\n";
            std::exit(2);
        }
        const int max_frames = argc >= 4 ? std::max(1, std::atoi(argv[3])) : 1800;
        std::exit(CheckGubsyShellRealnetLanHost(argv[2], max_frames) ? 0 : 1);
    }

    if (command == "--check-gubsy-shell-realnet-lan-client") {
        if (argc < 3) {
            std::cerr << "usage: --check-gubsy-shell-realnet-lan-client <room_server_url> [room_code] [max_frames]\n";
            std::exit(2);
        }
        const char* room_code = argc >= 4 ? argv[3] : "";
        const int max_frames = argc >= 5 ? std::max(1, std::atoi(argv[4])) : 1800;
        std::exit(CheckGubsyShellRealnetLanClient(argv[2], room_code, max_frames) ? 0 : 1);
    }

    if (command == "--dump-recording-text") {
        if (argc < 4) {
            std::cerr << "usage: --dump-recording-text <input.splrec> <output.txt>\n";
            return true;
        }
        return DumpRecordingAsText(argv[2], argv[3]);
    }

    if (command == "--replay-desync") {
        if (argc < 3) {
            std::cerr << "usage: --replay-desync <input.sdrp> [--stop-frame N]\n";
            return true;
        }
        std::exit(ReplayDesyncFile(argv[2], ParseStopFrameArg(argc, argv)) ? 0 : 1);
    }

    if (command == "--compare-desync") {
        if (argc < 4) {
            std::cerr << "usage: --compare-desync <left.sdrp> <right.sdrp> [--stop-frame N]\n";
            return true;
        }
        std::exit(CompareDesyncFiles(argv[2], argv[3], ParseStopFrameArg(argc, argv)) ? 0 : 1);
    }

    return false;
}

} // namespace splonks
