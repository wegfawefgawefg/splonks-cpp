#include "audio.hpp"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace splonks {

namespace {

[[noreturn]] void ThrowAudioError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

std::string BuildAudioAssetPath(const std::string& file) {
    if (file.rfind("assets/", 0) == 0) {
        return file;
    }
    return "assets/" + file;
}

void DestroyLoadedAudio(LoadedAudioAsset& asset) {
    if (asset.audio != nullptr) {
        MIX_DestroyAudio(asset.audio);
        asset.audio = nullptr;
    }
}

void LoadAudioObjects(Audio& audio) {
    for (LoadedAudioAsset& asset : audio.loaded_assets) {
        asset.audio = MIX_LoadAudio(audio.mixer, asset.path.c_str(), !asset.streamed);
        if (asset.audio == nullptr) {
            ThrowAudioError(("MIX_LoadAudio failed for " + asset.path).c_str());
        }
    }
}

SDL_PropertiesID MakeLoopingProperties() {
    SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties != 0) {
        SDL_SetNumberProperty(properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    }
    return properties;
}

} // namespace

void Audio::CreateTracks() {
    music_track = MIX_CreateTrack(mixer);
    if (music_track == nullptr) {
        ThrowAudioError("MIX_CreateTrack for music track failed");
    }

    audio_instance_tracks.reserve(audio_detail::kAudioInstanceTrackCount);
    audio_instance_track_runtimes.reserve(audio_detail::kAudioInstanceTrackCount);
    for (std::size_t i = 0; i < audio_detail::kAudioInstanceTrackCount; ++i) {
        MIX_Track* track = MIX_CreateTrack(mixer);
        if (track == nullptr) {
            ThrowAudioError("MIX_CreateTrack for audio instance track failed");
        }
        InitializeAudioInstanceTrack(track);
    }
}

Audio Audio::New(const AudioAssetDb& audio_asset_db) {
    Audio result;
    result.asset_db = audio_asset_db;
    result.loaded_assets.reserve(result.asset_db.assets.size());
    for (const AudioAsset& asset : result.asset_db.assets) {
        LoadedAudioAsset loaded_asset;
        loaded_asset.path = BuildAudioAssetPath(asset.file);
        loaded_asset.default_volume = asset.default_volume;
        loaded_asset.streamed = asset.streamed;
        result.loaded_assets.push_back(std::move(loaded_asset));
    }
    result.music_volume = 1.0F;
    result.sound_effects_volume = 1.0F;
    result.pan_half_width_px = 256.0F;

    if (!MIX_Init()) {
        ThrowAudioError("MIX_Init failed");
    }

    result.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (result.mixer == nullptr) {
        MIX_Quit();
        ThrowAudioError("MIX_CreateMixerDevice failed");
    }

    try {
        result.CreateTracks();
        LoadAudioObjects(result);
    } catch (...) {
        result.Shutdown();
        throw;
    }

    result.initialized = true;
    return result;
}

void Audio::Shutdown() {
    if (!initialized && mixer == nullptr) {
        return;
    }

    if (music_track != nullptr) {
        MIX_StopTrack(music_track, 0);
    }
    for (MIX_Track* track : audio_instance_tracks) {
        if (track != nullptr) {
            MIX_StopTrack(track, 0);
        }
    }

    for (LoadedAudioAsset& asset : loaded_assets) {
        DestroyLoadedAudio(asset);
    }

    if (mixer != nullptr) {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }

    music_track = nullptr;
    audio_instance_tracks.clear();
    audio_instance_track_runtimes.clear();
    has_current_music = false;
    current_music_asset_id = kInvalidAudioAssetId;
    initialized = false;
    MIX_Quit();
}

const AudioAssetDb& Audio::GetAssetDb() const {
    return asset_db;
}

const AudioAsset* Audio::FindAudioAsset(AudioAssetId asset_id) const {
    return asset_db.Find(asset_id);
}

const char* Audio::GetAudioAssetNameCStr(AudioAssetId asset_id) const {
    const AudioAsset* const asset = FindAudioAsset(asset_id);
    return asset != nullptr ? asset->name.c_str() : "<missing audio asset>";
}

LoadedAudioAsset* Audio::FindLoadedAudioAsset(AudioAssetId asset_id) {
    const std::optional<std::size_t> index = asset_db.FindIndex(asset_id);
    if (!index.has_value() || *index >= loaded_assets.size()) {
        return nullptr;
    }
    return &loaded_assets[*index];
}

const LoadedAudioAsset* Audio::FindLoadedAudioAsset(AudioAssetId asset_id) const {
    const std::optional<std::size_t> index = asset_db.FindIndex(asset_id);
    if (!index.has_value() || *index >= loaded_assets.size()) {
        return nullptr;
    }
    return &loaded_assets[*index];
}

void Audio::PlayMusic(AudioAssetId asset_id) {
    if (!initialized || music_track == nullptr) {
        return;
    }

    if (has_current_music && current_music_asset_id != asset_id) {
        StopMusic(current_music_asset_id);
    }

    LoadedAudioAsset* const loaded_asset = FindLoadedAudioAsset(asset_id);
    if (loaded_asset == nullptr || loaded_asset->audio == nullptr) {
        return;
    }

    has_current_music = true;
    current_music_asset_id = asset_id;

    if (!MIX_SetTrackAudio(music_track, loaded_asset->audio)) {
        return;
    }
    MIX_SetTrackGain(music_track, music_volume * loaded_asset->default_volume);

    SDL_PropertiesID properties = MakeLoopingProperties();
    MIX_PlayTrack(music_track, properties);
    if (properties != 0) {
        SDL_DestroyProperties(properties);
    }
}

void Audio::StopCurrentMusic() {
    if (has_current_music) {
        StopMusic(current_music_asset_id);
    }
}

void Audio::StopMusic(AudioAssetId asset_id) {
    (void)asset_id;
    if (!initialized || music_track == nullptr) {
        return;
    }

    MIX_StopTrack(music_track, 0);
    has_current_music = false;
    current_music_asset_id = kInvalidAudioAssetId;
}

void Audio::UpdateCurrentMusicStreamData() {
    if (!initialized || !has_current_music || music_track == nullptr) {
        return;
    }

    const LoadedAudioAsset* const loaded_asset = FindLoadedAudioAsset(current_music_asset_id);
    if (loaded_asset == nullptr) {
        return;
    }
    MIX_SetTrackGain(music_track, music_volume * loaded_asset->default_volume);
}

void Audio::SetCurrentMusicVolume(float volume) {
    music_volume = volume;
    if (!initialized || !has_current_music || music_track == nullptr) {
        return;
    }

    const LoadedAudioAsset* const loaded_asset = FindLoadedAudioAsset(current_music_asset_id);
    if (loaded_asset == nullptr) {
        return;
    }
    MIX_SetTrackGain(music_track, volume * loaded_asset->default_volume);
}

void PlayMenuSoundCant(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiCant);
}

void PlayMenuSoundConfirm(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiConfirm);
}

void PlayMenuSoundCursorMove(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiCursorMove);
}

void PlayMenuSoundLeft(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiLeft);
}

void PlayMenuSoundRight(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiRight);
}

void PlayMenuSoundSuperConfirm(Audio& audio) {
    audio.PlayAudioAsset(audio_asset_ids::UiSuperConfirm);
}

} // namespace splonks
