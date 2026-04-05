#pragma once

#include <string>
#include <vector>

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

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
    UiCant,
    UiConfirm,
    UiCursorMove,
    UiLeft,
    UiRight,
    UiSuperConfirm,
};

constexpr std::size_t kSongCount = 2;
constexpr std::size_t kSoundEffectCount = 43;

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
    std::size_t next_sound_effect_track = 0;
    float music_volume = 1.0F;
    float sound_effects_volume = 1.0F;

    static Audio New(const std::vector<LoadedSong>& songs, const std::vector<LoadedSound>& sounds);
    void Shutdown();

    void PlaySong(Song song);
    void StopCurrentSong();
    void UpdateCurrentSongStreamData();
    void PlaySoundEffect(SoundEffect sound_effect);
    void SetCurrentSongVolume(float volume);

  private:
    void StopSong(Song song);
};

void PlayMenuSoundCant(Audio& audio);
void PlayMenuSoundConfirm(Audio& audio);
void PlayMenuSoundCursorMove(Audio& audio);
void PlayMenuSoundLeft(Audio& audio);
void PlayMenuSoundRight(Audio& audio);
void PlayMenuSoundSuperConfirm(Audio& audio);
const char* GetSoundFileName(SoundEffect sound_effect);

} // namespace splonks
