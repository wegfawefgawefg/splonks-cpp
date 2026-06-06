#pragma once

#include "audio_asset.hpp"
#include "audio_filter.hpp"
#include "math_types.hpp"
#include "vid.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;
struct SDL_AudioSpec;

namespace splonks {

struct LoadedAudioAsset {
    std::string path;
    float default_volume = 1.0F;
    bool streamed = false;
    MIX_Audio* audio = nullptr;
};

inline constexpr std::uint32_t kInvalidAudioInstanceId =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr VID kInvalidAudioInstanceVID{
    kInvalidAudioInstanceId,
    0,
};

inline bool IsValidAudioInstanceVID(const VID& vid) {
    return vid.id != kInvalidAudioInstanceId;
}

struct AudioPlaybackParams {
    float volume_scale = 1.0F;
    bool positional = false;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    int loops = 0;
    float direct_gain = 1.0F;
    bool low_pass_enabled = false;
    float low_pass_cutoff_hz = audio_filter::kMaxLowPassCutoffHz;
    float low_pass_wet = 1.0F;
    bool reverb_enabled = false;
    float reverb_wet = 0.0F;
    float reverb_feedback = 0.0F;
    float reverb_delay_ms = 80.0F;
    float reverb_low_pass_cutoff_hz = audio_filter::kMaxLowPassCutoffHz;
};

namespace audio_detail {

constexpr std::size_t kAudioInstanceTrackCount = 16;

struct AudioInstanceTrackRuntime {
    MIX_Track* track = nullptr;
    std::uint32_t slot_index = 0;
    std::atomic<bool> active = false;
    std::atomic<std::uint32_t> generation = 0;
    float asset_default_volume = 1.0F;
    std::atomic<float> stereo_target_left = 1.0F;
    std::atomic<float> stereo_target_right = 1.0F;
    audio_filter::StereoPanProcessor stereo_pan_processor{};
    std::atomic<float> direct_gain = 1.0F;
    audio_filter::GainProcessor gain_processor{};
    std::atomic<bool> low_pass_enabled = false;
    std::atomic<float> low_pass_target_cutoff_hz =
        audio_filter::kMaxLowPassCutoffHz;
    std::atomic<float> low_pass_target_wet = 0.0F;
    audio_filter::LowPassProcessor low_pass_processor{};
    std::atomic<bool> reverb_enabled = false;
    std::atomic<float> reverb_target_wet = 0.0F;
    std::atomic<float> reverb_target_feedback = 0.0F;
    std::atomic<float> reverb_target_delay_ms = 80.0F;
    std::atomic<float> reverb_target_cutoff_hz =
        audio_filter::kMaxLowPassCutoffHz;
    audio_filter::DelayReverbProcessor delay_reverb_processor{};
    std::vector<float> cooked_scratch{};
};

} // namespace audio_detail

struct Audio {
    bool initialized = false;
    bool has_current_music = false;
    AudioAssetId current_music_asset_id = kInvalidAudioAssetId;
    AudioAssetDb asset_db;
    std::vector<LoadedAudioAsset> loaded_assets;
    MIX_Mixer* mixer = nullptr;
    MIX_Track* music_track = nullptr;
    std::vector<MIX_Track*> audio_instance_tracks;
    std::vector<std::unique_ptr<audio_detail::AudioInstanceTrackRuntime>>
        audio_instance_track_runtimes;
    std::size_t next_audio_instance_track = 0;
    float music_volume = 1.0F;
    float sound_effects_volume = 1.0F;
    float pan_half_width_px = 256.0F;
    Vec2 listener_world_pos = Vec2::New(0.0F, 0.0F);

    static Audio New(const AudioAssetDb& asset_db);
    void Shutdown();

    const AudioAssetDb& GetAssetDb() const;
    const AudioAsset* FindAudioAsset(AudioAssetId asset_id) const;
    const char* GetAudioAssetNameCStr(AudioAssetId asset_id) const;

    void PlayMusic(AudioAssetId asset_id);
    void StopCurrentMusic();
    void UpdateCurrentMusicStreamData();
    void SetCurrentMusicVolume(float volume);

    void PlayAudioAsset(AudioAssetId asset_id, float volume_scale = 1.0F);
    VID PlayAudioAssetInstance(
        AudioAssetId asset_id,
        const AudioPlaybackParams& params
    );
    bool UpdateAudioInstance(
        VID handle,
        const AudioPlaybackParams& params
    );
    bool StopAudioInstance(VID handle);
    bool IsAudioInstancePlaying(VID handle) const;
    void SetListenerWorldPos(const Vec2& world_pos);
    void SetPanHalfWidthPx(float half_width_px);

  private:
    void StopMusic(AudioAssetId asset_id);
    void CreateTracks();
    void InitializeAudioInstanceTrack(MIX_Track* track);
    LoadedAudioAsset* FindLoadedAudioAsset(AudioAssetId asset_id);
    const LoadedAudioAsset* FindLoadedAudioAsset(AudioAssetId asset_id) const;
    audio_detail::AudioInstanceTrackRuntime* GetAudioInstanceTrackRuntime(
        VID handle
    );
    const audio_detail::AudioInstanceTrackRuntime* GetAudioInstanceTrackRuntime(
        VID handle
    ) const;
    static void SDLCALL OnAudioInstanceTrackStopped(void* userdata, MIX_Track* track);
    static void SDLCALL OnAudioInstanceTrackCooked(
        void* userdata,
        MIX_Track* track,
        const SDL_AudioSpec* spec,
        float* pcm,
        int samples
    );
};

void PlayMenuSoundCant(Audio& audio);
void PlayMenuSoundConfirm(Audio& audio);
void PlayMenuSoundCursorMove(Audio& audio);
void PlayMenuSoundLeft(Audio& audio);
void PlayMenuSoundRight(Audio& audio);
void PlayMenuSoundSuperConfirm(Audio& audio);

} // namespace splonks
