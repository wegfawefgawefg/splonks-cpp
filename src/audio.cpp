#include "audio.hpp"

#include <filesystem>
#include <stdexcept>

namespace splonks {

namespace {

std::size_t SongIndex(Song song) {
    return static_cast<std::size_t>(song);
}

std::size_t SoundEffectIndex(SoundEffect sound_effect) {
    return static_cast<std::size_t>(sound_effect);
}

} // namespace

std::vector<Song> AllSongs() {
    return {Song::Title, Song::Playing};
}

std::vector<SoundEffect> AllSoundEffects() {
    return {
        SoundEffect::Jump,
        SoundEffect::Step1,
        SoundEffect::Step2,
        SoundEffect::ClimbMetal1,
        SoundEffect::ClimbMetal2,
        SoundEffect::BatFlap1,
        SoundEffect::BatFlap2,
        SoundEffect::BatSqueak,
        SoundEffect::Thud,
        SoundEffect::GameOver,
        SoundEffect::Jetpack1,
        SoundEffect::Jetpack2,
        SoundEffect::Equip,
        SoundEffect::Throw,
        SoundEffect::BombExplosion,
        SoundEffect::AnimalCrush1,
        SoundEffect::AnimalCrush2,
        SoundEffect::Gold,
        SoundEffect::GoldStack,
        SoundEffect::MoneySmashed,
        SoundEffect::PlayerOuch,
        SoundEffect::BlockDrag1,
        SoundEffect::BlockDrag2,
        SoundEffect::BlockLand,
        SoundEffect::DefaultLand,
        SoundEffect::RopeDeploy,
        SoundEffect::ClimbRope1,
        SoundEffect::ClimbRope2,
        SoundEffect::StageWin,
        SoundEffect::PotShatter,
        SoundEffect::BoxBreak,
        SoundEffect::BaseballBatSwing,
        SoundEffect::BaseballBatKillHit1,
        SoundEffect::BaseballBatKillHit2,
        SoundEffect::BaseballBatKillHit3,
        SoundEffect::BaseballBatMetalDink1,
        SoundEffect::BaseballBatBoxSmash,
        SoundEffect::UiCant,
        SoundEffect::UiConfirm,
        SoundEffect::UiCursorMove,
        SoundEffect::UiLeft,
        SoundEffect::UiRight,
        SoundEffect::UiSuperConfirm,
    };
}

std::vector<LoadedSong> LoadSongs() {
    std::vector<LoadedSong> songs;
    songs.reserve(kSongCount);

    const std::vector<std::string> file_names = {"title", "playing"};
    for (const std::string& name : file_names) {
        const std::string path = "assets/music/" + name + ".ogg";
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Error loading music: missing file " + path);
        }

        LoadedSong song;
        song.path = path;
        song.volume = 1.0F;
        song.playing = false;
        songs.push_back(song);
    }

    return songs;
}

std::vector<LoadedSound> LoadSounds() {
    std::vector<LoadedSound> sounds;
    sounds.reserve(kSoundEffectCount);

    for (const SoundEffect sound_effect : AllSoundEffects()) {
        const std::string file_name_prefix = GetSoundFileName(sound_effect);
        const std::string path = "assets/sounds/" + file_name_prefix + ".ogg";
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Error loading sound: missing file " + path);
        }

        LoadedSound sound;
        sound.path = path;
        sound.volume = 1.0F;
        sounds.push_back(sound);
    }

    return sounds;
}

Audio Audio::New(const std::vector<LoadedSong>& loaded_songs,
                 const std::vector<LoadedSound>& loaded_sounds) {
    Audio result;
    result.has_current_song = false;
    result.current_song = Song::Title;
    result.songs = loaded_songs;
    result.sounds = loaded_sounds;
    result.music_volume = 1.0F;
    result.sound_effects_volume = 1.0F;
    return result;
}

void Audio::PlaySong(Song song) {
    if (has_current_song && current_song != song) {
        StopSong(current_song);
    }

    has_current_song = true;
    current_song = song;

    LoadedSong& loaded_song = songs[SongIndex(song)];
    loaded_song.volume = music_volume;
    loaded_song.playing = true;
}

void Audio::StopCurrentSong() {
    if (has_current_song) {
        StopSong(current_song);
    }
}

void Audio::StopSong(Song song) {
    LoadedSong& loaded_song = songs[SongIndex(song)];
    loaded_song.playing = false;
}

void Audio::UpdateCurrentSongStreamData() {
    if (!has_current_song) {
        return;
    }

    // Placeholder hook. The Rust code updates streamed music here.
    LoadedSong& loaded_song = songs[SongIndex(current_song)];
    loaded_song.volume = music_volume;
}

void Audio::PlaySoundEffect(SoundEffect sound_effect) {
    LoadedSound& loaded_sound = sounds[SoundEffectIndex(sound_effect)];
    loaded_sound.volume = sound_effects_volume;
}

void Audio::SetCurrentSongVolume(float volume) {
    if (!has_current_song) {
        return;
    }

    LoadedSong& loaded_song = songs[SongIndex(current_song)];
    loaded_song.volume = volume;
}

void PlayMenuSoundCant(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiCant);
}

void PlayMenuSoundConfirm(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiConfirm);
}

void PlayMenuSoundCursorMove(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiCursorMove);
}

void PlayMenuSoundLeft(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiLeft);
}

void PlayMenuSoundRight(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiRight);
}

void PlayMenuSoundSuperConfirm(Audio& audio) {
    audio.PlaySoundEffect(SoundEffect::UiSuperConfirm);
}

const char* GetSoundFileName(SoundEffect sound_effect) {
    switch (sound_effect) {
    case SoundEffect::Jump:
        return "jump";
    case SoundEffect::Step1:
        return "step1";
    case SoundEffect::Step2:
        return "step2";
    case SoundEffect::ClimbMetal1:
        return "climb_metal1";
    case SoundEffect::ClimbMetal2:
        return "climb_metal2";
    case SoundEffect::BatFlap1:
        return "bat_flap1";
    case SoundEffect::BatFlap2:
        return "bat_flap2";
    case SoundEffect::BatSqueak:
        return "bat_squeak";
    case SoundEffect::Thud:
        return "thud";
    case SoundEffect::GameOver:
        return "game_over";
    case SoundEffect::Jetpack1:
        return "jetpack1";
    case SoundEffect::Jetpack2:
        return "jetpack2";
    case SoundEffect::Equip:
        return "equip";
    case SoundEffect::Throw:
        return "throw";
    case SoundEffect::BombExplosion:
        return "bomb_explosion";
    case SoundEffect::AnimalCrush1:
        return "animal_crush1";
    case SoundEffect::AnimalCrush2:
        return "animal_crush2";
    case SoundEffect::Gold:
        return "gold";
    case SoundEffect::GoldStack:
        return "gold_stack";
    case SoundEffect::MoneySmashed:
        return "money_smashed";
    case SoundEffect::PlayerOuch:
        return "player_ouch";
    case SoundEffect::BlockDrag1:
        return "block_drag1";
    case SoundEffect::BlockDrag2:
        return "block_drag2";
    case SoundEffect::BlockLand:
        return "block_land";
    case SoundEffect::DefaultLand:
        return "default_land";
    case SoundEffect::RopeDeploy:
        return "rope_deploy";
    case SoundEffect::ClimbRope1:
        return "climb_rope1";
    case SoundEffect::ClimbRope2:
        return "climb_rope2";
    case SoundEffect::StageWin:
        return "stage_win";
    case SoundEffect::PotShatter:
        return "pot_shatter";
    case SoundEffect::BoxBreak:
        return "box_break";
    case SoundEffect::BaseballBatSwing:
        return "baseball_bat_swing";
    case SoundEffect::BaseballBatKillHit1:
        return "baseball_bat_kill_hit1";
    case SoundEffect::BaseballBatKillHit2:
        return "baseball_bat_kill_hit2";
    case SoundEffect::BaseballBatKillHit3:
        return "baseball_bat_kill_hit3";
    case SoundEffect::BaseballBatMetalDink1:
        return "baseball_bat_metal_dink1";
    case SoundEffect::BaseballBatBoxSmash:
        return "baseball_bat_box_smash";
    case SoundEffect::UiCant:
        return "ui_cant";
    case SoundEffect::UiConfirm:
        return "ui_confirm";
    case SoundEffect::UiCursorMove:
        return "ui_cursor_move";
    case SoundEffect::UiLeft:
        return "ui_left";
    case SoundEffect::UiRight:
        return "ui_right";
    case SoundEffect::UiSuperConfirm:
        return "ui_super_confirm";
    }

    return "";
}

} // namespace splonks
