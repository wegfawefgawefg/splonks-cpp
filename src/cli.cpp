#include "cli.hpp"

#include "frame_data.hpp"
#include "raw_frame_data.hpp"
#include "tile_source_data.hpp"

#include <iostream>
#include <string>

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

    std::cout << "tile images: " << tile_source_db.image_paths.size() << '\n';
    std::cout << "tile sources: " << tile_source_db.sources.size() << '\n';
    std::cout << "tile spans: " << tile_source_db.spans.size() << '\n';
    for (const std::string& image_path : tile_source_db.image_paths) {
        std::cout << "  " << image_path << '\n';
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

    return false;
}

} // namespace splonks
