# Audio Asset Refactor Plan

## Goal

Replace the current split `Song` / `SoundEffect` ident model with one unified audio asset system that matches the shape of the sprite asset side more closely:

- stable hashed ids in code
- one asset database owning audio metadata
- runtime playback behavior chosen at play time

This removes the current duplicated sound-definition maintenance and gives audio the same general structure as frame data and sprite assets.

## Current Problems

The current audio code has a few structural issues:

- sound ident is split across `enum class SoundEffect`, `AllSoundEffects()`, `kSoundEffectCount`, and `GetSoundFileName()`
- music and sound effects use separate ident systems even though both are just audio assets
- runtime playback policy is mixed together with asset definition
- adding a new sound requires touching multiple places and keeping ordering consistent

The main correctness risk is order-coupled loading. If the enum order and the loaded-sound ordering ever drift, playback can silently point at the wrong asset.

## Target Shape

### 1. Unified audio ids

Add `audio_asset_id.hpp` with the same general pattern as `aframe_id.hpp`.

```cpp
using AudioAssetId = std::uint32_t;

constexpr AudioAssetId kInvalidAudioAssetId = 0;
constexpr AudioAssetId HashAudioAssetIdConstexpr(std::string_view text);
AudioAssetId HashAudioAssetId(const std::string& text);

namespace audio_asset_ids {
constexpr AudioAssetId Jump = HashAudioAssetIdConstexpr("jump");
constexpr AudioAssetId Title = HashAudioAssetIdConstexpr("title");
}
```

This gives gameplay code stable symbolic ids without tying it to filenames or enum ordering.

### 2. Unified audio asset database

Add an audio asset database with one row per asset.

```cpp
struct AudioAsset {
    AudioAssetId id = kInvalidAudioAssetId;
    std::string name;
    std::string file;
    float default_volume = 1.0F;
    bool streamed = false;
};

struct AudioAssetDb {
    std::vector<AudioAsset> assets;
    const AudioAsset* Find(AudioAssetId id) const;
    const AudioAsset* Find(std::string_view name) const;
};
```

The asset db owns audio metadata. The runtime audio system should query this db rather than maintaining its own duplicated name table.

### 3. Manifest-backed asset definitions

Audio asset definitions should live in data, not hardcoded switches.

Proposed file:

- `assets/audio/annotations.yaml`

Initial shape:

```yaml
audio:
  - name: title
    file: music/title.ogg
    streamed: true
    default_volume: 1.0

  - name: jump
    file: sounds/jump.ogg
    streamed: false
    default_volume: 1.0
```

Notes:

- `name` is the stable semantic name used for hashing
- `file` is the actual relative asset path
- `streamed` is an asset property because it affects loading/storage policy
- `default_volume` is a content-side default, not an instance behavior

### 4. Runtime playback params stay runtime

Playback behavior should not be baked into the asset.

These remain play-time choices:

- `positional`
- `world_pos`
- `loops`
- filter / reverb settings
- direct gain
- bus selection

That means the same asset can be:

- non-positional in one usage
- positional in another
- played on different buses depending on context

This is the correct split between asset metadata and runtime policy.

## Bus Decision

Audio buses are useful, but they should not be part of the asset definition by default.

Reason:

- the same asset may reasonably be played on different buses in different situations
- bus routing is a playback concern, not a content ident concern

If buses are needed, they should live in playback params or higher-level wrapper APIs.

## Streamed Decision

`streamed` stays asset-side.

Reason:

- it affects how the file is loaded and stored
- it is not just a playback styling choice
- changing it per play call would require awkward dual loading/caching rules

Rule:

- short frequently reused sounds: usually `streamed = false`
- long music / ambience: often `streamed = true`

## API Direction

Keep the public gameplay-facing helpers expressive, but make them forward into one unified underlying audio asset system.

Examples:

```cpp
PlayMusic(audio_asset_ids::Title);
PlayWorldSound(audio_asset_ids::Jump, params);
PlayUiSound(audio_asset_ids::UiConfirm, params);
```

Internally these should all resolve through the same `AudioAssetDb` and shared instance system.

The point is not to force all call sites to use one ambiguous `PlayAudio()` function. The point is to unify the data model underneath the helpers.

## Implementation Plan

### Phase 1: ident and asset db

- add `audio_asset_id.hpp`
- add `raw_audio_asset.hpp/.cpp` if needed for manifest loading
- add `audio_asset.hpp/.cpp`
- add `assets/audio/annotations.yaml`
- load the db at startup

### Phase 2: runtime loading refactor

- replace `SoundEffect` and `Song` loading paths with db-driven loading
- remove `AllSoundEffects()`
- remove `GetSoundFileName()`
- remove `kSoundEffectCount`
- stop depending on enum order for loaded sound indexing

### Phase 3: call site migration

- replace `SoundEffect::...` and `Song::...` call sites with `audio_asset_ids::...`
- keep wrapper APIs readable
- update debug UI to show asset names from the db rather than file-stem switches

### Phase 4: cleanup

- remove the old enum-based ident system entirely
- remove any dead compatibility helpers

## Non-Goals

This refactor should not try to solve everything at once.

Out of scope for the first pass:

- advanced mixer graph design
- per-asset bus routing policy
- editor tooling for audio assets
- localization / subtitle metadata
- automatic loudness normalization

Those can be added later once the core asset model is clean.

## Design Rules

- one source of truth for audio asset definitions
- ids are stable and semantic, not filename-driven
- runtime playback behavior is chosen at play time
- loading/storage policy stays with the asset
- helper APIs may differ, but the underlying asset system is unified

## Summary

The intended end state is:

- one unified audio asset id type
- one manifest-backed audio asset database
- no duplicated sound-definition switch/list/count maintenance
- runtime playback policy chosen by the caller
- songs and sound effects treated as the same kind of asset, with different usage patterns rather than different ident systems
