#pragma once

#include "audio_asset_id.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace splonks {

struct RawAudioAssetFile;

struct AudioAsset {
    AudioAssetId id = kInvalidAudioAssetId;
    std::string name;
    std::string file;
    float default_volume = 1.0F;
    bool streamed = false;
};

struct AudioAssetDb {
    std::vector<AudioAsset> assets;
    std::unordered_map<AudioAssetId, std::size_t> index_by_id;
    std::unordered_map<std::string, std::size_t> index_by_name;

    static AudioAssetDb FromRaw(const RawAudioAssetFile& raw_file);

    const AudioAsset* Find(AudioAssetId id) const;
    const AudioAsset* Find(std::string_view name) const;
    std::optional<std::size_t> FindIndex(AudioAssetId id) const;
};

AudioAssetDb LoadAudioAssetDb(const std::string& yaml_path);

} // namespace splonks
