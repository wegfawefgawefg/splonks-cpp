#pragma once

#include <string>
#include <vector>

namespace splonks {

struct RawAudioAsset {
    std::string source_yaml_path;
    int source_line = 0;
    std::string name;
    std::string file;
    float default_volume = 1.0F;
    bool streamed = false;
};

struct RawAudioAssetFile {
    std::vector<RawAudioAsset> audio;
};

RawAudioAssetFile LoadRawAudioAssetFile(const std::string& yaml_path);

} // namespace splonks
