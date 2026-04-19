#include "audio_asset.hpp"

#include "raw_audio_asset.hpp"

#include <stdexcept>

namespace splonks {

namespace {

AudioAsset ToAudioAsset(const RawAudioAsset& raw_asset) {
    AudioAsset asset;
    asset.id = HashAudioAssetId(raw_asset.name);
    asset.name = raw_asset.name;
    asset.file = raw_asset.file;
    asset.default_volume = raw_asset.default_volume;
    asset.streamed = raw_asset.streamed;
    return asset;
}

void ValidateRawAudioAsset(const RawAudioAsset& raw_asset) {
    if (raw_asset.name.empty()) {
        throw std::runtime_error(
            raw_asset.source_yaml_path + ":" + std::to_string(raw_asset.source_line) +
            ": audio asset is missing name"
        );
    }
    if (raw_asset.file.empty()) {
        throw std::runtime_error(
            raw_asset.source_yaml_path + ":" + std::to_string(raw_asset.source_line) +
            ": audio asset is missing file"
        );
    }
    if (raw_asset.default_volume < 0.0F) {
        throw std::runtime_error(
            raw_asset.source_yaml_path + ":" + std::to_string(raw_asset.source_line) +
            ": audio asset has negative default_volume"
        );
    }
}

} // namespace

AudioAssetDb AudioAssetDb::FromRaw(const RawAudioAssetFile& raw_file) {
    AudioAssetDb db;
    db.assets.reserve(raw_file.audio.size());

    for (const RawAudioAsset& raw_asset : raw_file.audio) {
        ValidateRawAudioAsset(raw_asset);
        AudioAsset asset = ToAudioAsset(raw_asset);
        if (db.index_by_name.contains(asset.name)) {
            throw std::runtime_error(
                raw_asset.source_yaml_path + ":" + std::to_string(raw_asset.source_line) +
                ": duplicate audio asset name: " + asset.name
            );
        }
        if (db.index_by_id.contains(asset.id)) {
            throw std::runtime_error(
                raw_asset.source_yaml_path + ":" + std::to_string(raw_asset.source_line) +
                ": audio asset id collision for name: " + asset.name
            );
        }
        const std::size_t index = db.assets.size();
        db.index_by_id.emplace(asset.id, index);
        db.index_by_name.emplace(asset.name, index);
        db.assets.push_back(std::move(asset));
    }

    return db;
}

const AudioAsset* AudioAssetDb::Find(AudioAssetId id) const {
    const auto found = index_by_id.find(id);
    if (found == index_by_id.end()) {
        return nullptr;
    }
    return &assets[found->second];
}

const AudioAsset* AudioAssetDb::Find(std::string_view name) const {
    const auto found = index_by_name.find(std::string(name));
    if (found == index_by_name.end()) {
        return nullptr;
    }
    return &assets[found->second];
}

std::optional<std::size_t> AudioAssetDb::FindIndex(AudioAssetId id) const {
    const auto found = index_by_id.find(id);
    if (found == index_by_id.end()) {
        return std::nullopt;
    }
    return found->second;
}

AudioAssetDb LoadAudioAssetDb(const std::string& yaml_path) {
    return AudioAssetDb::FromRaw(LoadRawAudioAssetFile(yaml_path));
}

} // namespace splonks
