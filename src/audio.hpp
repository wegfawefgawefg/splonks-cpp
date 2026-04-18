#pragma once

#include "audio_filter.hpp"
#include "math_types.hpp"

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

enum class Song {
    Title,
    Playing,
};

enum class SoundEffect {
    Jump,
    Step1,
    Step2,
    ClimbMetal1,
    ClimbMetal2,
    BatFlap1,
    BatFlap2,
    BatSqueak,
    Thud,
    GameOver,
    Jetpack1,
    Jetpack2,
    Equip,
    Throw,
    PistolShoot,
    PistolUnholster,
    GunEmpty,
    BombExplosion,
    AnimalCrush1,
    AnimalCrush2,
    Gold,
    GoldStack,
    MoneySmashed,
    PlayerOuch,
    BlockDrag1,
    BlockDrag2,
    BlockLand,
    DefaultLand,
    RopeDeploy,
    ClimbRope1,
    ClimbRope2,
    StageWin,
    PotShatter,
    BoxBreak,
    BaseballBatSwing,
    BaseballBatKillHit1,
    BaseballBatKillHit2,
    BaseballBatKillHit3,
    BaseballBatMetalDink1,
    BaseballBatBoxSmash,
    CavemanNotice,
    CavemanHurt,
    DamselAmbientCry,
    DamselHurt,
    Smooch,
    ChestOpen,
    Unlock,
    LawsonEnter,
    CashRegister,
    ShopkeepAnger0,
    LightBreak,
    BoulderLatch,
    BoulderHitGround,
    BoulderTileCrash,
    BoulderRoll,
    UiCant,
    UiConfirm,
    UiCursorMove,
    UiLeft,
    UiRight,
    UiSuperConfirm,
};

constexpr std::size_t kSongCount = 2;
constexpr std::size_t kSoundEffectCount = 61;

struct LoadedSong {
    std::string path;
    float volume = 1.0F;
    bool playing = false;
    MIX_Audio* audio = nullptr;
};

struct LoadedSound {
    std::string path;
    float volume = 1.0F;
    MIX_Audio* audio = nullptr;
};

struct SoundEffectInstanceHandle {
    static constexpr std::uint32_t kInvalidSlotIndex =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t slot_index = kInvalidSlotIndex;
    std::uint32_t generation = 0;

    bool IsValid() const { return slot_index != kInvalidSlotIndex; }
};

struct SoundEffectPlaybackParams {
    float volume_scale = 1.0F;
    bool positional = false;
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    int loops = 0;
    bool low_pass_enabled = false;
    float low_pass_cutoff_hz = audio_filter::kMaxLowPassCutoffHz;
    float low_pass_wet = 1.0F;
};

namespace audio_detail {

constexpr std::size_t kSoundEffectTrackCount = 16;

struct SoundEffectTrackRuntime {
    MIX_Track* track = nullptr;
    std::uint32_t slot_index = 0;
    std::atomic<bool> active = false;
    std::atomic<std::uint32_t> generation = 0;
    std::atomic<float> stereo_target_left = 1.0F;
    std::atomic<float> stereo_target_right = 1.0F;
    std::atomic<bool> low_pass_enabled = false;
    audio_filter::StereoPanProcessor stereo_pan_processor{};
    std::atomic<float> low_pass_target_cutoff_hz =
        audio_filter::kMaxLowPassCutoffHz;
    std::atomic<float> low_pass_target_wet = 0.0F;
    audio_filter::LowPassProcessor low_pass_processor{};
};

} // namespace audio_detail

std::vector<Song> AllSongs();
std::vector<SoundEffect> AllSoundEffects();
std::vector<LoadedSong> LoadSongs();
std::vector<LoadedSound> LoadSounds();

struct Audio {
    bool initialized = false;
    bool has_current_song = false;
    Song current_song = Song::Title;
    std::vector<LoadedSong> songs;
    std::vector<LoadedSound> sounds;
    MIX_Mixer* mixer = nullptr;
    MIX_Track* song_track = nullptr;
    std::vector<MIX_Track*> sound_effect_tracks;
    std::vector<std::unique_ptr<audio_detail::SoundEffectTrackRuntime>>
        sound_effect_track_runtimes;
    std::size_t next_sound_effect_track = 0;
    float music_volume = 1.0F;
    float sound_effects_volume = 1.0F;
    float pan_half_width_px = 256.0F;
    Vec2 listener_world_pos = Vec2::New(0.0F, 0.0F);

    static Audio New(
        const std::vector<LoadedSong>& songs,
        const std::vector<LoadedSound>& sounds
    );
    void Shutdown();

    void PlaySong(Song song);
    void StopCurrentSong();
    void UpdateCurrentSongStreamData();
    void PlaySoundEffect(SoundEffect sound_effect, float volume_scale = 1.0F);
    SoundEffectInstanceHandle PlaySoundEffectInstance(
        SoundEffect sound_effect,
        const SoundEffectPlaybackParams& params
    );
    bool UpdateSoundEffectInstance(
        SoundEffectInstanceHandle handle,
        const SoundEffectPlaybackParams& params
    );
    bool StopSoundEffectInstance(SoundEffectInstanceHandle handle);
    bool IsSoundEffectInstancePlaying(SoundEffectInstanceHandle handle) const;
    void SetListenerWorldPos(const Vec2& world_pos);
    void SetPanHalfWidthPx(float half_width_px);
    void SetCurrentSongVolume(float volume);

  private:
    void StopSong(Song song);
    void CreateTracks();
    void InitializeSoundEffectTrack(MIX_Track* track);
    audio_detail::SoundEffectTrackRuntime* GetSoundEffectTrackRuntime(
        SoundEffectInstanceHandle handle
    );
    const audio_detail::SoundEffectTrackRuntime* GetSoundEffectTrackRuntime(
        SoundEffectInstanceHandle handle
    ) const;
    static void SDLCALL OnSoundEffectTrackStopped(void* userdata, MIX_Track* track);
    static void SDLCALL OnSoundEffectTrackCooked(
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
const char* GetSoundFileName(SoundEffect sound_effect);

} // namespace splonks
