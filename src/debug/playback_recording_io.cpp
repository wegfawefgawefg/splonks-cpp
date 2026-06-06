#include "debug/playback_internal.hpp"

#include "buying.hpp"
#include "ents/damsel.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 88;

enum class BuyableCallbackKind : std::uint8_t {
    None = 0,
    TryBuyEntForMoney = 1,
    BuyDamsel = 2,
};

template <typename T>
void WritePod(std::ostream& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    out.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
}

template <typename T>
bool ReadPod(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return in.good();
}

template <typename T>
void WriteVectorPod(std::ostream& out, const std::vector<T>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    if (!values.empty()) {
        out.write(
            reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(sizeof(T) * values.size())
        );
    }
}

template <typename T>
bool ReadVectorPod(std::istream& in, std::vector<T>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    if (count > 0) {
        in.read(
            reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(sizeof(T) * values.size())
        );
    }
    return in.good();
}

template <typename T>
void WriteOptionalPod(std::ostream& out, const std::optional<T>& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WritePod(out, *value);
    }
}

template <typename T>
bool ReadOptionalPod(std::istream& in, std::optional<T>& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    T loaded{};
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteOptionalSizeIndex(std::ostream& out, const std::optional<std::size_t>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        const std::uint32_t stored = static_cast<std::uint32_t>(*value);
        WritePod(out, stored);
    }
}

bool ReadOptionalSizeIndex(std::istream& in, std::optional<std::size_t>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    std::uint32_t loaded = 0;
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = static_cast<std::size_t>(loaded);
    return true;
}

void WriteSizeIndex(std::ostream& out, const std::size_t value) {
    const std::uint32_t stored = static_cast<std::uint32_t>(value);
    WritePod(out, stored);
}

bool ReadSizeIndex(std::istream& in, std::size_t& value) {
    std::uint32_t loaded = 0;
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = static_cast<std::size_t>(loaded);
    return true;
}

void WriteSizeIndexVector(std::ostream& out, const std::vector<std::size_t>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    for (const std::size_t value : values) {
        const std::uint32_t stored = static_cast<std::uint32_t>(value);
        WritePod(out, stored);
    }
}

bool ReadSizeIndexVector(std::istream& in, std::vector<std::size_t>& values) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::uint32_t loaded = 0;
        if (!ReadPod(in, loaded)) {
            return false;
        }
        values[i] = static_cast<std::size_t>(loaded);
    }
    return true;
}

void WriteVid(std::ostream& out, const VID& vid) {
    WritePod(out, vid.id);
    WritePod(out, vid.version);
}

bool ReadVid(std::istream& in, VID& vid) {
    return ReadPod(in, vid.id) &&
           ReadPod(in, vid.version);
}

void WriteOptionalVid(std::ostream& out, const std::optional<VID>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WriteVid(out, *value);
    }
}

bool ReadOptionalVid(std::istream& in, std::optional<VID>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    VID loaded{};
    if (!ReadVid(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteVidVector(std::ostream& out, const std::vector<VID>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    for (const VID& value : values) {
        WriteVid(out, value);
    }
}

bool ReadVidVector(std::istream& in, std::vector<VID>& values) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    for (VID& value : values) {
        if (!ReadVid(in, value)) {
            return false;
        }
    }
    return true;
}

void WriteOptionalVidVector(std::ostream& out, const std::optional<std::vector<VID>>& values) {
    const std::uint8_t has_value = values.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (values.has_value()) {
        WriteVidVector(out, *values);
    }
}

bool ReadOptionalVidVector(std::istream& in, std::optional<std::vector<VID>>& values) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        values.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    values.emplace();
    return ReadVidVector(in, *values);
}

void WriteString(std::ostream& out, const std::string& value) {
    const std::uint32_t count = static_cast<std::uint32_t>(value.size());
    WritePod(out, count);
    if (count > 0) {
        out.write(value.data(), static_cast<std::streamsize>(count));
    }
}

bool ReadString(std::istream& in, std::string& value) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    value.resize(count);
    if (count > 0) {
        in.read(value.data(), static_cast<std::streamsize>(count));
    }
    return in.good();
}

void WriteBoolByte(std::ostream& out, bool value) {
    const std::uint8_t stored = value ? 1U : 0U;
    WritePod(out, stored);
}

bool ReadBoolByte(std::istream& in, bool& value) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > 1) {
        return false;
    }
    value = stored != 0;
    return true;
}

void WriteOptionalBoolByte(std::ostream& out, const std::optional<bool>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WriteBoolByte(out, *value);
    }
}

bool ReadOptionalBoolByte(std::istream& in, std::optional<bool>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    bool loaded = false;
    if (!ReadBoolByte(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WritePlayerConnectionKind(std::ostream& out, PlayerConnectionKind kind) {
    const std::uint8_t stored = static_cast<std::uint8_t>(kind);
    WritePod(out, stored);
}

bool ReadPlayerConnectionKind(std::istream& in, PlayerConnectionKind& kind) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(PlayerConnectionKind::Remote)) {
        return false;
    }
    kind = static_cast<PlayerConnectionKind>(stored);
    return true;
}

void WriteMode(std::ostream& out, Mode mode) {
    const std::uint8_t stored = static_cast<std::uint8_t>(mode);
    WritePod(out, stored);
}

bool ReadMode(std::istream& in, Mode& mode) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(Mode::Win)) {
        return false;
    }
    mode = static_cast<Mode>(stored);
    return true;
}

void WriteSettingsMode(std::ostream& out, SettingsMode mode) {
    const std::uint8_t stored = static_cast<std::uint8_t>(mode);
    WritePod(out, stored);
}

bool ReadSettingsMode(std::istream& in, SettingsMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(SettingsMode::Controls)) {
        return false;
    }
    mode = static_cast<SettingsMode>(stored);
    return true;
}

void WritePostProcessEffect(std::ostream& out, PostProcessEffect effect) {
    const std::uint8_t stored = static_cast<std::uint8_t>(effect);
    WritePod(out, stored);
}

bool ReadPostProcessEffect(std::istream& in, PostProcessEffect& effect) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(PostProcessEffect::Crt)) {
        return false;
    }
    effect = static_cast<PostProcessEffect>(stored);
    return true;
}

void WriteStageType(std::ostream& out, StageType stage_type) {
    const std::uint8_t stored = static_cast<std::uint8_t>(stage_type);
    WritePod(out, stored);
}

bool ReadStageType(std::istream& in, StageType& stage_type) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(StageType::Test1)) {
        return false;
    }
    stage_type = static_cast<StageType>(stored);
    return true;
}

void WriteMultiplayerRespawnMode(std::ostream& out, MultiplayerRespawnMode mode) {
    const std::uint8_t stored = static_cast<std::uint8_t>(mode);
    WritePod(out, stored);
}

bool ReadMultiplayerRespawnMode(std::istream& in, MultiplayerRespawnMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(MultiplayerRespawnMode::RespawnAtEntrance)) {
        return false;
    }
    mode = static_cast<MultiplayerRespawnMode>(stored);
    return true;
}

void WriteAttachMode(std::ostream& out, AttachMode mode) {
    const std::uint8_t stored = static_cast<std::uint8_t>(mode);
    WritePod(out, stored);
}

bool ReadAttachMode(std::istream& in, AttachMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(AttachMode::Back)) {
        return false;
    }
    mode = static_cast<AttachMode>(stored);
    return true;
}

void WriteOptionalAFrameId(std::ostream& out, const std::optional<AFrameId>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WritePod(out, *value);
    }
}

bool ReadOptionalAFrameId(std::istream& in, std::optional<AFrameId>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    AFrameId loaded = 0;
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

template <typename T>
void WriteEnumByte(std::ostream& out, T value) {
    const std::uint8_t stored = static_cast<std::uint8_t>(value);
    WritePod(out, stored);
}

template <typename T>
bool ReadEnumByte(std::istream& in, T& value, T max_value) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(max_value)) {
        return false;
    }
    value = static_cast<T>(stored);
    return true;
}

template <typename T>
void WriteOptionalEnumByte(std::ostream& out, const std::optional<T>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WriteEnumByte(out, *value);
    }
}

template <typename T>
bool ReadOptionalEnumByte(std::istream& in, std::optional<T>& value, T max_value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    T loaded{};
    if (!ReadEnumByte(in, loaded, max_value)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteEntType(std::ostream& out, EntType type) {
    const std::uint16_t stored = static_cast<std::uint16_t>(type);
    WritePod(out, stored);
}

bool ReadEntType(std::istream& in, EntType& type) {
    std::uint16_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint16_t>(EntType::DebugMovingLight)) {
        return false;
    }
    type = static_cast<EntType>(stored);
    return true;
}

void WriteVec2(std::ostream& out, const Vec2& value) {
    WritePod(out, value.x);
    WritePod(out, value.y);
}

bool ReadVec2(std::istream& in, Vec2& value) {
    return ReadPod(in, value.x) &&
           ReadPod(in, value.y);
}

void WriteIVec2(std::ostream& out, const IVec2& value) {
    WritePod(out, value.x);
    WritePod(out, value.y);
}

bool ReadIVec2(std::istream& in, IVec2& value) {
    return ReadPod(in, value.x) &&
           ReadPod(in, value.y);
}

void WriteOptionalVec2(std::ostream& out, const std::optional<Vec2>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WriteVec2(out, *value);
    }
}

bool ReadOptionalVec2(std::istream& in, std::optional<Vec2>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    Vec2 loaded{};
    if (!ReadVec2(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteStageLoadTargetKind(std::ostream& out, StageLoadTargetKind kind) {
    WriteEnumByte(out, kind);
}

bool ReadStageLoadTargetKind(std::istream& in, StageLoadTargetKind& kind) {
    return ReadEnumByte(in, kind, StageLoadTargetKind::QuestStage);
}

void WriteDebugLevelKind(std::ostream& out, DebugLevelKind kind) {
    WriteEnumByte(out, kind);
}

bool ReadDebugLevelKind(std::istream& in, DebugLevelKind& kind) {
    return ReadEnumByte(in, kind, DebugLevelKind::LightingStressTest);
}

void WriteFixedCharArray(std::ostream& out, const char* data, std::size_t size) {
    out.write(data, static_cast<std::streamsize>(size));
}

bool ReadFixedCharArray(std::istream& in, char* data, std::size_t size) {
    in.read(data, static_cast<std::streamsize>(size));
    return in.good();
}

void WriteStageLoadTarget(std::ostream& out, const StageLoadTarget& target) {
    WriteStageLoadTargetKind(out, target.kind);
    WriteStageType(out, target.stage_type);
    WriteDebugLevelKind(out, target.debug_level);
    WritePod(out, target.debug_variant);
    WriteFixedCharArray(out, target.quest_id.data(), target.quest_id.size());
    WriteFixedCharArray(out, target.quest_stage_id.data(), target.quest_stage_id.size());
}

bool ReadStageLoadTarget(std::istream& in, StageLoadTarget& target) {
    return ReadStageLoadTargetKind(in, target.kind) &&
           ReadStageType(in, target.stage_type) &&
           ReadDebugLevelKind(in, target.debug_level) &&
           ReadPod(in, target.debug_variant) &&
           ReadFixedCharArray(in, target.quest_id.data(), target.quest_id.size()) &&
           ReadFixedCharArray(in, target.quest_stage_id.data(), target.quest_stage_id.size());
}

void WriteStageTransitionTarget(std::ostream& out, const StageTransitionTarget& target) {
    WriteStageLoadTarget(out, target.destination);
    WriteBoolByte(out, target.preserve_player_state);
    WriteOptionalPod(out, target.seed);
}

bool ReadStageTransitionTarget(std::istream& in, StageTransitionTarget& target) {
    return ReadStageLoadTarget(in, target.destination) &&
           ReadBoolByte(in, target.preserve_player_state) &&
           ReadOptionalPod(in, target.seed);
}

void WriteOptionalStageTransitionTarget(
    std::ostream& out,
    const std::optional<StageTransitionTarget>& value
) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WriteStageTransitionTarget(out, *value);
    }
}

bool ReadOptionalStageTransitionTarget(
    std::istream& in,
    std::optional<StageTransitionTarget>& value
) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    StageTransitionTarget loaded{};
    if (!ReadStageTransitionTarget(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

std::optional<BuyableCallbackKind> GetBuyableCallbackKind(EntOnTryBuy callback) {
    if (callback == nullptr) {
        return BuyableCallbackKind::None;
    }
    if (callback == TryBuyEntForMoney) {
        return BuyableCallbackKind::TryBuyEntForMoney;
    }
    if (callback == ents::damsel::BuyDamsel) {
        return BuyableCallbackKind::BuyDamsel;
    }
    return std::nullopt;
}

bool ReadBuyableCallback(std::istream& in, EntOnTryBuy& callback) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    switch (static_cast<BuyableCallbackKind>(stored)) {
    case BuyableCallbackKind::None:
        callback = nullptr;
        return true;
    case BuyableCallbackKind::TryBuyEntForMoney:
        callback = TryBuyEntForMoney;
        return true;
    case BuyableCallbackKind::BuyDamsel:
        callback = ents::damsel::BuyDamsel;
        return true;
    }
    return false;
}

void WriteBuyable(std::ostream& out, const Buyable& buyable) {
    WriteBoolByte(out, buyable.active);
    WritePod(out, buyable.display_quantity);
    WriteOptionalAFrameId(out, buyable.display_icon_anim_id);
    WriteOptionalVid(out, buyable.shop_owner_vid);
    const std::optional<BuyableCallbackKind> callback_kind = GetBuyableCallbackKind(buyable.on_try_buy);
    const std::uint8_t callback = callback_kind.has_value()
        ? static_cast<std::uint8_t>(*callback_kind)
        : 0xFFU;
    WritePod(out, callback);
}

bool ReadBuyable(std::istream& in, Buyable& buyable) {
    bool active = false;
    std::uint32_t display_quantity = 0;
    std::optional<AFrameId> display_icon_anim_id;
    std::optional<VID> shop_owner_vid;
    EntOnTryBuy on_try_buy = nullptr;
    if (!ReadBoolByte(in, active) ||
        !ReadPod(in, display_quantity) ||
        !ReadOptionalAFrameId(in, display_icon_anim_id) ||
        !ReadOptionalVid(in, shop_owner_vid) ||
        !ReadBuyableCallback(in, on_try_buy)) {
        return false;
    }
    buyable.active = active;
    buyable.display_quantity = display_quantity;
    buyable.display_icon_anim_id = display_icon_anim_id;
    buyable.shop_owner_vid = shop_owner_vid;
    buyable.on_try_buy = on_try_buy;
    return true;
}

void WriteUseState(std::ostream& out, const UseState& use_state) {
    WriteBoolByte(out, use_state.down);
    WriteBoolByte(out, use_state.pressed);
    WriteBoolByte(out, use_state.released);
    WritePod(out, use_state.frames);
    WriteOptionalVid(out, use_state.user_vid);
    WriteAttachMode(out, use_state.source);
}

bool ReadUseState(std::istream& in, UseState& use_state) {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint32_t frames = 0;
    std::optional<VID> user_vid;
    AttachMode source = AttachMode::None;
    if (!ReadBoolByte(in, down) ||
        !ReadBoolByte(in, pressed) ||
        !ReadBoolByte(in, released) ||
        !ReadPod(in, frames) ||
        !ReadOptionalVid(in, user_vid) ||
        !ReadAttachMode(in, source)) {
        return false;
    }
    use_state.down = down;
    use_state.pressed = pressed;
    use_state.released = released;
    use_state.frames = frames;
    use_state.user_vid = user_vid;
    use_state.source = source;
    return true;
}

void WriteEffectInstance(std::ostream& out, const EffectInstance& effect) {
    const std::uint8_t id = static_cast<std::uint8_t>(effect.id);
    WritePod(out, id);
    WritePod(out, effect.count);
    WritePod(out, effect.value);
    WritePod(out, effect.frames_remaining);
}

bool ReadEffectInstance(std::istream& in, EffectInstance& effect) {
    std::uint8_t id = 0;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
    if (!ReadPod(in, id) ||
        !ReadPod(in, count) ||
        !ReadPod(in, value) ||
        !ReadPod(in, frames_remaining)) {
        return false;
    }
    if (id >= static_cast<std::uint8_t>(EffectId::Count)) {
        return false;
    }
    effect.id = static_cast<EffectId>(id);
    effect.count = count;
    effect.value = value;
    effect.frames_remaining = frames_remaining;
    return true;
}

void WriteEntEffects(std::ostream& out, const BoxedEntEffects& effects_box) {
    const EntEffects* const effects = effects_box.get();
    const std::uint8_t count = effects != nullptr ? effects->count : 0;
    WritePod(out, count);
    for (std::uint8_t i = 0; i < count; ++i) {
        WriteEffectInstance(out, effects->effects[i]);
    }
}

bool ReadEntEffects(std::istream& in, BoxedEntEffects& effects_box) {
    std::uint8_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    if (count > kMaxEntEffects) {
        return false;
    }
    if (count == 0) {
        effects_box.reset();
        return true;
    }
    EntEffects& effects = effects_box.emplace();
    effects.count = count;
    for (std::uint8_t i = 0; i < count; ++i) {
        if (!ReadEffectInstance(in, effects.effects[i])) {
            return false;
        }
    }
    return true;
}

void WriteAFrameAnimator(std::ostream& out, const AFrameAnimator& animator) {
    WritePod(out, animator.anim_id);
    WriteSizeIndex(out, animator.current_frame);
    WritePod(out, animator.current_time);
    WritePod(out, animator.scale);
    WritePod(out, animator.speed);
    WriteBoolByte(out, animator.animate);
    WriteBoolByte(out, animator.loop);
    WriteBoolByte(out, animator.finished);
    const std::uint8_t playback_mode = static_cast<std::uint8_t>(animator.playback_mode);
    WritePod(out, playback_mode);
    WritePod(out, animator.play_count);
    WritePod(out, animator.plays_completed);
    WriteBoolByte(out, animator.playback_dirty);
    WriteBoolByte(out, animator.ping_pong_forward);
}

bool ReadAFrameAnimator(std::istream& in, AFrameAnimator& animator) {
    std::uint8_t playback_mode = 0;
    bool animate = false;
    bool loop = false;
    bool finished = false;
    bool playback_dirty = false;
    bool ping_pong_forward = false;
    if (!ReadPod(in, animator.anim_id) ||
        !ReadSizeIndex(in, animator.current_frame) ||
        !ReadPod(in, animator.current_time) ||
        !ReadPod(in, animator.scale) ||
        !ReadPod(in, animator.speed) ||
        !ReadBoolByte(in, animate) ||
        !ReadBoolByte(in, loop) ||
        !ReadBoolByte(in, finished) ||
        !ReadPod(in, playback_mode) ||
        !ReadPod(in, animator.play_count) ||
        !ReadPod(in, animator.plays_completed) ||
        !ReadBoolByte(in, playback_dirty) ||
        !ReadBoolByte(in, ping_pong_forward)) {
        return false;
    }
    if (playback_mode > static_cast<std::uint8_t>(AnimPlaybackMode::PingPong)) {
        return false;
    }
    animator.animate = animate;
    animator.loop = loop;
    animator.finished = finished;
    animator.playback_mode = static_cast<AnimPlaybackMode>(playback_mode);
    animator.playback_dirty = playback_dirty;
    animator.ping_pong_forward = ping_pong_forward;
    return true;
}

template <typename T>
void WriteOptionalVectorPod(std::ostream& out, const std::optional<std::vector<T>>& values) {
    const std::uint8_t has_value = values.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WriteVectorPod(out, *values);
    }
}

template <typename T>
bool ReadOptionalVectorPod(std::istream& in, std::optional<std::vector<T>>& values) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        values.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    values.emplace();
    return ReadVectorPod(in, *values);
}

void WriteEnt(std::ostream& out, const Ent& ent) {
    WriteBoolByte(out, ent.active);
    WriteBoolByte(out, ent.marked_for_destruction);
    WriteEntType(out, ent.type_);
    WriteVid(out, ent.vid);
    WriteBoolByte(out, ent.has_physics);
    WriteBoolByte(out, ent.can_collide);
    WriteBoolByte(out, ent.can_be_hit);
    WriteBoolByte(out, ent.can_receive_proj_contact);
    WriteBoolByte(out, ent.stone);
    WriteBoolByte(out, ent.wanted);
    WriteBoolByte(out, ent.crusher_pusher);
    WriteBoolByte(out, ent.pushable);
    WriteBoolByte(out, ent.can_stomp);
    WriteBoolByte(out, ent.can_be_stomped);
    WriteBoolByte(out, ent.can_collect_pickups);
    WriteBoolByte(out, ent.can_go_on_back);
    WriteBoolByte(out, ent.grounded);
    WritePod(out, ent.shake);
    WritePod(out, ent.rotation);
    WritePod(out, ent.alpha);
    WritePod(out, ent.coyote_time);
    WritePod(out, ent.stun_timer);
    WriteBoolByte(out, ent.stun_recovers_on_ground);
    WriteBoolByte(out, ent.stun_recovers_while_held);
    WriteBoolByte(out, ent.can_be_picked_up);
    WriteBoolByte(out, ent.affected_by_cobweb);
    WriteBoolByte(out, ent.can_only_be_picked_up_if_dead_or_stunned);
    WriteBoolByte(out, ent.impassable);
    WriteBoolByte(out, ent.can_be_hung_on);
    WritePod(out, ent.fall_timer);
    WritePod(out, ent.pos);
    WritePod(out, ent.vel);
    WritePod(out, ent.acc);
    WritePod(out, ent.max_speed);
    WritePod(out, ent.jump_hold_gravity_frames_remaining);
    WritePod(out, ent.throw_velocity_scale);
    WritePod(out, ent.buoyancy);
    WriteEntEffects(out, ent.effects);
    WritePod(out, ent.size);
    WritePod(out, ent.self_light);
    WritePod(out, ent.light_strength);
    WritePod(out, ent.light_color);
    WritePod(out, ent.light_radius);
    WritePod(out, ent.dist_traveled_this_frame);
    WriteEnumByte(out, ent.facing);
    WriteBoolByte(out, ent.vertical_flip);
    WriteEnumByte(out, ent.draw_layer);
    WriteBoolByte(out, ent.render_enabled);
    WriteAFrameAnimator(out, ent.aframe_animator);
    WritePod(out, ent.jump_delay_frame_count);
    WriteBoolByte(out, ent.jumped_this_frame);
    WritePod(out, ent.climb_detach_cooldown);
    WriteOptionalEnumByte(out, ent.hang_side);
    WriteBoolByte(out, ent.can_hang_ledge);
    WriteBoolByte(out, ent.can_hang_wall);
    WritePod(out, ent.hang_count);
    WriteBoolByte(out, ent.holding);
    WriteOptionalEnumByte(out, ent.pickup_effect);
    WritePod(out, ent.money);
    WriteBuyable(out, ent.buyable);
    WriteOptionalSizeIndex(out, ent.stage_spawn_index);
    WriteOptionalVid(out, ent.back_vid);
    WriteAttachMode(out, ent.attach_mode);
    WriteUseState(out, ent.use_state);
    WritePod(out, ent.travel_sound_countdown);
    WriteEnumByte(out, ent.travel_sound);
    WriteEnumByte(out, ent.condition);
    WriteEnumByte(out, ent.last_condition);
    WriteEnumByte(out, ent.ai_state);
    WriteEnumByte(out, ent.last_ai_state);
    WritePod(out, ent.movement_flags);
    WritePod(out, ent.health);
    WriteBoolByte(out, ent.hurt_on_contact);
    WriteBoolByte(out, ent.vanish_on_death);
    WriteBoolByte(out, ent.affected_by_ground_friction);
    WritePod(out, ent.support_ground_friction);
    WritePod(out, ent.push_acc);
    WriteOptionalAFrameId(out, ent.damage_anim);
    WriteOptionalPod(out, ent.damage_sound);
    WriteOptionalPod(out, ent.collide_sound);
    WriteOptionalPod(out, ent.death_sound);
    WriteOptionalPod(out, ent.transition_target);
    WritePod(out, ent.stage_exit_id);
    WritePod(out, ent.attack_weight);
    WritePod(out, ent.weight);
    WritePod(out, ent.bomb_throw_delay_countdown);
    WritePod(out, ent.rope_throw_delay_countdown);
    WritePod(out, ent.attack_delay_countdown);
    WritePod(out, ent.equip_delay_countdown);
    WriteOptionalVid(out, ent.thrown_by);
    WritePod(out, ent.thrown_immunity_timer);
    WriteEnumByte(out, ent.proj_contact_damage_type);
    WritePod(out, ent.proj_contact_damage_amount);
    WriteBoolByte(out, ent.can_apply_proj_contact);
    WritePod(out, ent.proj_contact_timer);
    WriteBoolByte(out, ent.collided);
    WriteBoolByte(out, ent.collided_last_frame);
    WritePod(out, ent.contact_sound_cooldown);
    WriteEnumByte(out, ent.damage_vuln);
    WriteBoolByte(out, ent.can_be_stunned);
    WritePod(out, ent.point_a);
    WritePod(out, ent.point_b);
    WritePod(out, ent.point_c);
    WritePod(out, ent.point_d);
    WriteEnumByte(out, ent.point_label_a);
    WriteEnumByte(out, ent.point_label_b);
    WriteEnumByte(out, ent.point_label_c);
    WriteEnumByte(out, ent.point_label_d);
    WriteOptionalVid(out, ent.holding_vid);
    WriteOptionalVid(out, ent.held_by_vid);
    WritePod(out, ent.holding_timer);
    WriteOptionalVid(out, ent.ent_a);
    WriteOptionalVid(out, ent.ent_b);
    WriteOptionalVid(out, ent.ent_c);
    WriteOptionalVid(out, ent.ent_d);
    WriteOptionalVidVector(out, ent.child_vids);
    WriteOptionalVidVector(out, ent.inside_vids);
    WriteEnumByte(out, ent.ent_label_a);
    WriteEnumByte(out, ent.alignment);
    WritePod(out, ent.counter_a);
    WritePod(out, ent.counter_b);
    WritePod(out, ent.counter_c);
    WritePod(out, ent.counter_d);
    WritePod(out, ent.threshold_a);
    WritePod(out, ent.threshold_b);
}

bool ReadEnt(std::istream& in, Ent& ent) {
    return ReadBoolByte(in, ent.active) &&
           ReadBoolByte(in, ent.marked_for_destruction) &&
           ReadEntType(in, ent.type_) &&
           ReadVid(in, ent.vid) &&
           ReadBoolByte(in, ent.has_physics) &&
           ReadBoolByte(in, ent.can_collide) &&
           ReadBoolByte(in, ent.can_be_hit) &&
           ReadBoolByte(in, ent.can_receive_proj_contact) &&
           ReadBoolByte(in, ent.stone) &&
           ReadBoolByte(in, ent.wanted) &&
           ReadBoolByte(in, ent.crusher_pusher) &&
           ReadBoolByte(in, ent.pushable) &&
           ReadBoolByte(in, ent.can_stomp) &&
           ReadBoolByte(in, ent.can_be_stomped) &&
           ReadBoolByte(in, ent.can_collect_pickups) &&
           ReadBoolByte(in, ent.can_go_on_back) &&
           ReadBoolByte(in, ent.grounded) &&
           ReadPod(in, ent.shake) &&
           ReadPod(in, ent.rotation) &&
           ReadPod(in, ent.alpha) &&
           ReadPod(in, ent.coyote_time) &&
           ReadPod(in, ent.stun_timer) &&
           ReadBoolByte(in, ent.stun_recovers_on_ground) &&
           ReadBoolByte(in, ent.stun_recovers_while_held) &&
           ReadBoolByte(in, ent.can_be_picked_up) &&
           ReadBoolByte(in, ent.affected_by_cobweb) &&
           ReadBoolByte(in, ent.can_only_be_picked_up_if_dead_or_stunned) &&
           ReadBoolByte(in, ent.impassable) &&
           ReadBoolByte(in, ent.can_be_hung_on) &&
           ReadPod(in, ent.fall_timer) &&
           ReadPod(in, ent.pos) &&
           ReadPod(in, ent.vel) &&
           ReadPod(in, ent.acc) &&
           ReadPod(in, ent.max_speed) &&
           ReadPod(in, ent.jump_hold_gravity_frames_remaining) &&
           ReadPod(in, ent.throw_velocity_scale) &&
           ReadPod(in, ent.buoyancy) &&
           ReadEntEffects(in, ent.effects) &&
           ReadPod(in, ent.size) &&
           ReadPod(in, ent.self_light) &&
           ReadPod(in, ent.light_strength) &&
           ReadPod(in, ent.light_color) &&
           ReadPod(in, ent.light_radius) &&
           ReadPod(in, ent.dist_traveled_this_frame) &&
           ReadEnumByte(in, ent.facing, Side::Right) &&
           ReadBoolByte(in, ent.vertical_flip) &&
           ReadEnumByte(in, ent.draw_layer, DrawLayer::Foreground) &&
           ReadBoolByte(in, ent.render_enabled) &&
           ReadAFrameAnimator(in, ent.aframe_animator) &&
           ReadPod(in, ent.jump_delay_frame_count) &&
           ReadBoolByte(in, ent.jumped_this_frame) &&
           ReadPod(in, ent.climb_detach_cooldown) &&
           ReadOptionalEnumByte(in, ent.hang_side, Side::Right) &&
           ReadBoolByte(in, ent.can_hang_ledge) &&
           ReadBoolByte(in, ent.can_hang_wall) &&
           ReadPod(in, ent.hang_count) &&
           ReadBoolByte(in, ent.holding) &&
           ReadOptionalEnumByte(in, ent.pickup_effect, EffectId::InWater) &&
           ReadPod(in, ent.money) &&
           ReadBuyable(in, ent.buyable) &&
           ReadOptionalSizeIndex(in, ent.stage_spawn_index) &&
           ReadOptionalVid(in, ent.back_vid) &&
           ReadAttachMode(in, ent.attach_mode) &&
           ReadUseState(in, ent.use_state) &&
           ReadPod(in, ent.travel_sound_countdown) &&
           ReadEnumByte(in, ent.travel_sound, TravelSound::Two) &&
           ReadEnumByte(in, ent.condition, EntCondition::Stunned) &&
           ReadEnumByte(in, ent.last_condition, EntCondition::Stunned) &&
           ReadEnumByte(in, ent.ai_state, EntAiState::Returning) &&
           ReadEnumByte(in, ent.last_ai_state, EntAiState::Returning) &&
           ReadPod(in, ent.movement_flags) &&
           ReadPod(in, ent.health) &&
           ReadBoolByte(in, ent.hurt_on_contact) &&
           ReadBoolByte(in, ent.vanish_on_death) &&
           ReadBoolByte(in, ent.affected_by_ground_friction) &&
           ReadPod(in, ent.support_ground_friction) &&
           ReadPod(in, ent.push_acc) &&
           ReadOptionalAFrameId(in, ent.damage_anim) &&
           ReadOptionalPod(in, ent.damage_sound) &&
           ReadOptionalPod(in, ent.collide_sound) &&
           ReadOptionalPod(in, ent.death_sound) &&
           ReadOptionalPod(in, ent.transition_target) &&
           ReadPod(in, ent.stage_exit_id) &&
           ReadPod(in, ent.attack_weight) &&
           ReadPod(in, ent.weight) &&
           ReadPod(in, ent.bomb_throw_delay_countdown) &&
           ReadPod(in, ent.rope_throw_delay_countdown) &&
           ReadPod(in, ent.attack_delay_countdown) &&
           ReadPod(in, ent.equip_delay_countdown) &&
           ReadOptionalVid(in, ent.thrown_by) &&
           ReadPod(in, ent.thrown_immunity_timer) &&
           ReadEnumByte(in, ent.proj_contact_damage_type, DamageType::Fall) &&
           ReadPod(in, ent.proj_contact_damage_amount) &&
           ReadBoolByte(in, ent.can_apply_proj_contact) &&
           ReadPod(in, ent.proj_contact_timer) &&
           ReadBoolByte(in, ent.collided) &&
           ReadBoolByte(in, ent.collided_last_frame) &&
           ReadPod(in, ent.contact_sound_cooldown) &&
           ReadEnumByte(in, ent.damage_vuln, DamageVuln::AnthingExceptJumpOn) &&
           ReadBoolByte(in, ent.can_be_stunned) &&
           ReadPod(in, ent.point_a) &&
           ReadPod(in, ent.point_b) &&
           ReadPod(in, ent.point_c) &&
           ReadPod(in, ent.point_d) &&
           ReadEnumByte(in, ent.point_label_a, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_b, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_c, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_d, PointLabel::Avoid) &&
           ReadOptionalVid(in, ent.holding_vid) &&
           ReadOptionalVid(in, ent.held_by_vid) &&
           ReadPod(in, ent.holding_timer) &&
           ReadOptionalVid(in, ent.ent_a) &&
           ReadOptionalVid(in, ent.ent_b) &&
           ReadOptionalVid(in, ent.ent_c) &&
           ReadOptionalVid(in, ent.ent_d) &&
           ReadOptionalVidVector(in, ent.child_vids) &&
           ReadOptionalVidVector(in, ent.inside_vids) &&
           ReadEnumByte(in, ent.ent_label_a, EntLabel::AttachedToThis) &&
           ReadEnumByte(in, ent.alignment, Alignment::Enemy) &&
           ReadPod(in, ent.counter_a) &&
           ReadPod(in, ent.counter_b) &&
           ReadPod(in, ent.counter_c) &&
           ReadPod(in, ent.counter_d) &&
           ReadPod(in, ent.threshold_a) &&
           ReadPod(in, ent.threshold_b);
}

void WriteSettings(std::ostream& out, const Settings& settings) {
    WriteSettingsMode(out, settings.mode);
    WritePod(out, settings.video.resolution);
    WriteBoolByte(out, settings.video.fullscreen);
    WriteBoolByte(out, settings.video.vsync);
    WriteVectorPod(out, settings.video.resolution_options);
    WritePod(out, settings.audio.music_volume);
    WritePod(out, settings.audio.sfx_volume);
    WritePod(out, settings.audio.pan_half_width_px);
    WritePod(out, settings.controls.jump);
    WritePod(out, settings.controls.shoot);
    WritePod(out, settings.ui.icon_scale);
    WritePod(out, settings.ui.status_icon_scale);
    WritePod(out, settings.ui.tool_slot_scale);
    WritePod(out, settings.ui.tool_icon_scale);
    WritePostProcessEffect(out, settings.post_process.effect);
    WriteBoolByte(out, settings.post_process.terrain_lighting);
    WriteBoolByte(out, settings.post_process.terrain_seam_ao);
    WriteBoolByte(out, settings.post_process.terrain_exposure_lighting);
    WriteBoolByte(out, settings.post_process.backwall_lighting);
    WritePod(out, settings.post_process.terrain_seam_ao_amount);
    WritePod(out, settings.post_process.terrain_seam_ao_size);
    WritePod(out, settings.post_process.terrain_exposure_amount);
    WritePod(out, settings.post_process.terrain_exposure_min_brightness);
    WritePod(out, settings.post_process.terrain_exposure_max_brightness);
    WritePod(out, settings.post_process.terrain_exposure_diagonal_weight);
    WritePod(out, settings.post_process.terrain_exposure_smoothing);
    WritePod(out, settings.post_process.backwall_brightness);
    WritePod(out, settings.post_process.backwall_min_brightness);
    WritePod(out, settings.post_process.backwall_max_brightness);
    WritePod(out, settings.post_process.backwall_smoothing);
    WritePod(out, settings.post_process.crt_scanline_amount);
    WritePod(out, settings.post_process.crt_scanline_edge_start);
    WritePod(out, settings.post_process.crt_scanline_edge_falloff);
    WritePod(out, settings.post_process.crt_scanline_edge_strength);
    WritePod(out, settings.post_process.crt_zoom);
    WritePod(out, settings.post_process.crt_warp_amount);
    WritePod(out, settings.post_process.crt_vignette_amount);
    WritePod(out, settings.post_process.crt_vignette_intensity);
    WritePod(out, settings.post_process.crt_grille_amount);
    WritePod(out, settings.post_process.crt_brightness_boost);
    WritePod(out, settings.player_tuning);
}

bool ReadSettings(std::istream& in, Settings& settings) {
    if (!ReadSettingsMode(in, settings.mode) ||
        !ReadPod(in, settings.video.resolution) ||
        !ReadBoolByte(in, settings.video.fullscreen) ||
        !ReadBoolByte(in, settings.video.vsync) ||
        !ReadVectorPod(in, settings.video.resolution_options) ||
        !ReadPod(in, settings.audio.music_volume) ||
        !ReadPod(in, settings.audio.sfx_volume) ||
        !ReadPod(in, settings.audio.pan_half_width_px) ||
        !ReadPod(in, settings.controls.jump) ||
        !ReadPod(in, settings.controls.shoot) ||
        !ReadPod(in, settings.ui.icon_scale) ||
        !ReadPod(in, settings.ui.status_icon_scale) ||
        !ReadPod(in, settings.ui.tool_slot_scale) ||
        !ReadPod(in, settings.ui.tool_icon_scale) ||
        !ReadPostProcessEffect(in, settings.post_process.effect) ||
        !ReadBoolByte(in, settings.post_process.terrain_lighting) ||
        !ReadBoolByte(in, settings.post_process.terrain_seam_ao) ||
        !ReadBoolByte(in, settings.post_process.terrain_exposure_lighting) ||
        !ReadBoolByte(in, settings.post_process.backwall_lighting) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao_amount) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao_size) ||
        !ReadPod(in, settings.post_process.terrain_exposure_amount) ||
        !ReadPod(in, settings.post_process.terrain_exposure_min_brightness) ||
        !ReadPod(in, settings.post_process.terrain_exposure_max_brightness) ||
        !ReadPod(in, settings.post_process.terrain_exposure_diagonal_weight) ||
        !ReadPod(in, settings.post_process.terrain_exposure_smoothing) ||
        !ReadPod(in, settings.post_process.backwall_brightness) ||
        !ReadPod(in, settings.post_process.backwall_min_brightness) ||
        !ReadPod(in, settings.post_process.backwall_max_brightness) ||
        !ReadPod(in, settings.post_process.backwall_smoothing) ||
        !ReadPod(in, settings.post_process.crt_scanline_amount) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_start) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_falloff) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_strength) ||
        !ReadPod(in, settings.post_process.crt_zoom) ||
        !ReadPod(in, settings.post_process.crt_warp_amount) ||
        !ReadPod(in, settings.post_process.crt_vignette_amount) ||
        !ReadPod(in, settings.post_process.crt_vignette_intensity) ||
        !ReadPod(in, settings.post_process.crt_grille_amount) ||
        !ReadPod(in, settings.post_process.crt_brightness_boost) ||
        !ReadPod(in, settings.player_tuning)) {
        return false;
    }
    return true;
}

void WriteStageExitRequirement(std::ostream& out, const StageExitRequirement& requirement) {
    WriteString(out, requirement.flag);
    WriteBoolByte(out, requirement.expected);
}

bool ReadStageExitRequirement(std::istream& in, StageExitRequirement& requirement) {
    return ReadString(in, requirement.flag) &&
           ReadBoolByte(in, requirement.expected);
}

void WriteStageExitTarget(std::ostream& out, const StageExitTarget& target) {
    WriteString(out, target.target_stage_id);
    const std::uint32_t count = static_cast<std::uint32_t>(target.requirements.size());
    WritePod(out, count);
    for (const StageExitRequirement& requirement : target.requirements) {
        WriteStageExitRequirement(out, requirement);
    }
}

bool ReadStageExitTarget(std::istream& in, StageExitTarget& target) {
    if (!ReadString(in, target.target_stage_id)) {
        return false;
    }
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    target.requirements.resize(count);
    for (StageExitRequirement& requirement : target.requirements) {
        if (!ReadStageExitRequirement(in, requirement)) {
            return false;
        }
    }
    return true;
}

void WriteStageExit(std::ostream& out, const StageExit& exit) {
    WriteString(out, exit.id);
    WriteStageExitTarget(out, exit.target);
}

bool ReadStageExit(std::istream& in, StageExit& exit) {
    return ReadString(in, exit.id) &&
           ReadStageExitTarget(in, exit.target);
}

void WriteEntSpawn(std::ostream& out, const EntSpawn& spawn) {
    WriteEntType(out, spawn.type_);
    WriteVec2(out, spawn.pos);
    WriteOptionalVec2(out, spawn.size_override);
    WriteEnumByte(out, spawn.facing);
    WriteOptionalEnumByte(out, spawn.ai_state_override);
    WritePod(out, spawn.anim_id);
    WriteOptionalSizeIndex(out, spawn.ent_a_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_b_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_c_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_d_spawn_index);
    WriteOptionalSizeIndex(out, spawn.shop_owner_spawn_index);
    WriteBoolByte(out, spawn.buyable);
    WritePod(out, spawn.buy_price);
    WriteString(out, spawn.exit_id);
}

bool ReadEntSpawn(std::istream& in, EntSpawn& spawn) {
    return ReadEntType(in, spawn.type_) &&
           ReadVec2(in, spawn.pos) &&
           ReadOptionalVec2(in, spawn.size_override) &&
           ReadEnumByte(in, spawn.facing, Side::Right) &&
           ReadOptionalEnumByte(in, spawn.ai_state_override, EntAiState::Returning) &&
           ReadPod(in, spawn.anim_id) &&
           ReadOptionalSizeIndex(in, spawn.ent_a_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_b_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_c_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_d_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.shop_owner_spawn_index) &&
           ReadBoolByte(in, spawn.buyable) &&
           ReadPod(in, spawn.buy_price) &&
           ReadString(in, spawn.exit_id);
}

void WriteStageGenAnnotation(std::ostream& out, const StageGenAnnotation& annotation) {
    WriteVec2(out, annotation.world_pos);
    WriteString(out, annotation.text);
}

bool ReadStageGenAnnotation(std::istream& in, StageGenAnnotation& annotation) {
    return ReadVec2(in, annotation.world_pos) &&
           ReadString(in, annotation.text);
}

void WriteStageLight(std::ostream& out, const StageLight& light) {
    WriteVid(out, light.vid);
    WriteIVec2(out, light.tile_pos);
    WritePod(out, light.radius);
}

bool ReadStageLight(std::istream& in, StageLight& light) {
    return ReadVid(in, light.vid) &&
           ReadIVec2(in, light.tile_pos) &&
           ReadPod(in, light.radius);
}

void WriteTile(std::ostream& out, Tile tile) {
    const std::uint16_t stored = static_cast<std::uint16_t>(tile);
    WritePod(out, stored);
}

bool ReadTile(std::istream& in, Tile& tile) {
    std::uint16_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint16_t>(Tile::Exit)) {
        return false;
    }
    tile = static_cast<Tile>(stored);
    return true;
}

void WriteTileRotation(std::ostream& out, TileRotation rotation) {
    WritePod(out, rotation);
}

bool ReadTileRotation(std::istream& in, TileRotation& rotation) {
    if (!ReadPod(in, rotation)) {
        return false;
    }
    return (rotation & ~kTileRotationMask) == 0;
}

template <typename T, typename WriteOne>
void WriteGridExplicit(std::ostream& out, const std::vector<std::vector<T>>& grid, WriteOne write_one) {
    const std::uint32_t rows = static_cast<std::uint32_t>(grid.size());
    WritePod(out, rows);
    for (const std::vector<T>& row : grid) {
        const std::uint32_t count = static_cast<std::uint32_t>(row.size());
        WritePod(out, count);
        for (const T& value : row) {
            write_one(out, value);
        }
    }
}

template <typename T, typename ReadOne>
bool ReadGridExplicit(std::istream& in, std::vector<std::vector<T>>& grid, ReadOne read_one) {
    std::uint32_t rows = 0;
    if (!ReadPod(in, rows)) {
        return false;
    }
    grid.resize(rows);
    for (std::vector<T>& row : grid) {
        std::uint32_t count = 0;
        if (!ReadPod(in, count)) {
            return false;
        }
        row.resize(count);
        for (T& value : row) {
            if (!read_one(in, value)) {
                return false;
            }
        }
    }
    return true;
}

void WriteTileVector(std::ostream& out, const std::vector<Tile>& tiles) {
    const std::uint32_t count = static_cast<std::uint32_t>(tiles.size());
    WritePod(out, count);
    for (Tile tile : tiles) {
        WriteTile(out, tile);
    }
}

bool ReadTileVector(std::istream& in, std::vector<Tile>& tiles) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    tiles.resize(count);
    for (Tile& tile : tiles) {
        if (!ReadTile(in, tile)) {
            return false;
        }
    }
    return true;
}

void WriteEmbeddedTreasureDrop(std::ostream& out, const EmbeddedTreasureDrop& drop) {
    WriteEntType(out, drop.type_);
    WritePod(out, drop.count);
}

bool ReadEmbeddedTreasureDrop(std::istream& in, EmbeddedTreasureDrop& drop) {
    return ReadEntType(in, drop.type_) &&
           ReadPod(in, drop.count);
}

void WriteEmbeddedTreasure(std::ostream& out, const EmbeddedTreasure& treasure) {
    WriteEnumByte(out, treasure.visibility);
    WritePod(out, treasure.overlay_frame);
    WritePod(out, treasure.break_sound);
    for (const EmbeddedTreasureDrop& drop : treasure.drops) {
        WriteEmbeddedTreasureDrop(out, drop);
    }
}

bool ReadEmbeddedTreasure(std::istream& in, EmbeddedTreasure& treasure) {
    if (!ReadEnumByte(in, treasure.visibility, EmbeddedTreasureVisibility::Visible) ||
        !ReadPod(in, treasure.overlay_frame) ||
        !ReadPod(in, treasure.break_sound)) {
        return false;
    }
    for (EmbeddedTreasureDrop& drop : treasure.drops) {
        if (!ReadEmbeddedTreasureDrop(in, drop)) {
            return false;
        }
    }
    return true;
}

void WriteBackgroundStamp(std::ostream& out, const BackgroundStamp& stamp) {
    WritePod(out, stamp.anim_id);
    WriteVec2(out, stamp.pos);
    WriteEnumByte(out, stamp.condition);
}

bool ReadBackgroundStamp(std::istream& in, BackgroundStamp& stamp) {
    return ReadPod(in, stamp.anim_id) &&
           ReadVec2(in, stamp.pos) &&
           ReadEnumByte(in, stamp.condition, BackgroundStampCondition::Wanted);
}

void WriteBackgroundStamps(std::ostream& out, const std::vector<BackgroundStamp>& stamps) {
    const std::uint32_t count = static_cast<std::uint32_t>(stamps.size());
    WritePod(out, count);
    for (const BackgroundStamp& stamp : stamps) {
        WriteBackgroundStamp(out, stamp);
    }
}

bool ReadBackgroundStamps(std::istream& in, std::vector<BackgroundStamp>& stamps) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    stamps.resize(count);
    for (BackgroundStamp& stamp : stamps) {
        if (!ReadBackgroundStamp(in, stamp)) {
            return false;
        }
    }
    return true;
}

void WriteStageLights(std::ostream& out, const std::vector<StageLight>& lights) {
    const std::uint32_t count = static_cast<std::uint32_t>(lights.size());
    WritePod(out, count);
    for (const StageLight& light : lights) {
        WriteStageLight(out, light);
    }
}

bool ReadStageLights(std::istream& in, std::vector<StageLight>& lights) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    lights.resize(count);
    for (StageLight& light : lights) {
        if (!ReadStageLight(in, light)) {
            return false;
        }
    }
    return true;
}

template <typename T>
void WriteGridPod(std::ostream& out, const std::vector<std::vector<T>>& grid) {
    const std::uint32_t rows = static_cast<std::uint32_t>(grid.size());
    WritePod(out, rows);
    for (const std::vector<T>& row : grid) {
        WriteVectorPod(out, row);
    }
}

template <typename T>
bool ReadGridPod(std::istream& in, std::vector<std::vector<T>>& grid) {
    std::uint32_t rows = 0;
    if (!ReadPod(in, rows)) {
        return false;
    }
    grid.resize(rows);
    for (std::uint32_t i = 0; i < rows; ++i) {
        if (!ReadVectorPod(in, grid[i])) {
            return false;
        }
    }
    return true;
}

void WriteStage(std::ostream& out, const Stage& stage) {
    WriteStageType(out, stage.stage_type);
    WriteString(out, stage.quest_id);
    WriteString(out, stage.quest_stage_id);
    WriteString(out, stage.route_label);
    WriteString(out, stage.stage_title);
    WritePod(out, stage.quest_level_number);
    WriteOptionalPod(out, stage.generation_seed);
    const std::uint32_t exit_count = static_cast<std::uint32_t>(stage.exits.size());
    WritePod(out, exit_count);
    for (const StageExit& exit : stage.exits) {
        WriteStageExit(out, exit);
    }
    WritePod(out, stage.gravity);
    WriteTile(out, stage.border.left.tile);
    WriteTile(out, stage.border.right.tile);
    WriteTile(out, stage.border.top.tile);
    WriteTile(out, stage.border.bottom.tile);
    WriteBoolByte(out, stage.border.wrap_x);
    WriteBoolByte(out, stage.border.wrap_y);
    WriteOptionalPod(out, stage.border.void_death_y);
    WriteBoolByte(out, stage.camera_clamp_enabled);
    WritePod(out, stage.camera_clamp_margin);
    WriteBoolByte(out, stage.wrap_transform_active);
    WritePod(out, stage.wrap_padding_tiles);
    WritePod(out, stage.wrap_core_origin_tiles);
    WritePod(out, stage.wrap_core_size_tiles);
    WriteGridExplicit(out, stage.tiles, WriteTile);
    WriteGridExplicit(out, stage.tile_rotations, WriteTileRotation);
    WriteGridExplicit(out, stage.fluid_tiles, WriteTile);
    WriteGridPod(out, stage.fluid_amount);
    WriteGridPod(out, stage.fluid_display_amount);
    WriteGridPod(out, stage.fluid_velocity);
    WriteGridPod(out, stage.fluid_gravity);
    WriteGridPod(out, stage.fluid_gravity_strength);
    WriteGridPod(out, stage.fluid_temp_gravity);
    WriteGridPod(out, stage.tile_shake);
    WriteGridPod(out, stage.backwall_tile_shake);
    WriteGridExplicit(out, stage.backwall_tiles, WriteTile);
    WriteTileVector(out, stage.backwall_fill_tiles);
    WriteGridExplicit(out, stage.embedded_treasures, WriteEmbeddedTreasure);
    WriteGridPod(out, stage.rooms);
    WriteVectorPod(out, stage.path);
    const std::uint32_t spawn_count = static_cast<std::uint32_t>(stage.ent_spawns.size());
    WritePod(out, spawn_count);
    for (const EntSpawn& spawn : stage.ent_spawns) {
        WriteEntSpawn(out, spawn);
    }
    WriteBackgroundStamps(out, stage.background_stamps);
    const std::uint32_t annotation_count =
        static_cast<std::uint32_t>(stage.stagegen_annotations.size());
    WritePod(out, annotation_count);
    for (const StageGenAnnotation& annotation : stage.stagegen_annotations) {
        WriteStageGenAnnotation(out, annotation);
    }
    WriteStageLights(out, stage.lights);
    WritePod(out, stage.block_anim_id);
    WriteSizeIndex(out, stage.next_light_vid);
    WritePod(out, stage.tile_change_generation);
}

bool ReadStage(std::istream& in, Stage& stage) {
    if (!ReadStageType(in, stage.stage_type) ||
        !ReadString(in, stage.quest_id) ||
        !ReadString(in, stage.quest_stage_id) ||
        !ReadString(in, stage.route_label) ||
        !ReadString(in, stage.stage_title) ||
        !ReadPod(in, stage.quest_level_number) ||
        !ReadOptionalPod(in, stage.generation_seed)) {
        return false;
    }

    std::uint32_t exit_count = 0;
    if (!ReadPod(in, exit_count)) {
        return false;
    }
    stage.exits.resize(exit_count);
    for (StageExit& exit : stage.exits) {
        if (!ReadStageExit(in, exit)) {
            return false;
        }
    }

    if (!ReadPod(in, stage.gravity) ||
        !ReadTile(in, stage.border.left.tile) ||
        !ReadTile(in, stage.border.right.tile) ||
        !ReadTile(in, stage.border.top.tile) ||
        !ReadTile(in, stage.border.bottom.tile) ||
        !ReadBoolByte(in, stage.border.wrap_x) ||
        !ReadBoolByte(in, stage.border.wrap_y) ||
        !ReadOptionalPod(in, stage.border.void_death_y) ||
        !ReadBoolByte(in, stage.camera_clamp_enabled) ||
        !ReadPod(in, stage.camera_clamp_margin) ||
        !ReadBoolByte(in, stage.wrap_transform_active) ||
        !ReadPod(in, stage.wrap_padding_tiles) ||
        !ReadPod(in, stage.wrap_core_origin_tiles) ||
        !ReadPod(in, stage.wrap_core_size_tiles)) {
        return false;
    }

    if (!ReadGridExplicit(in, stage.tiles, ReadTile) ||
        !ReadGridExplicit(in, stage.tile_rotations, ReadTileRotation) ||
        !ReadGridExplicit(in, stage.fluid_tiles, ReadTile) ||
        !ReadGridPod(in, stage.fluid_amount) ||
        !ReadGridPod(in, stage.fluid_display_amount) ||
        !ReadGridPod(in, stage.fluid_velocity) ||
        !ReadGridPod(in, stage.fluid_gravity) ||
        !ReadGridPod(in, stage.fluid_gravity_strength) ||
        !ReadGridPod(in, stage.fluid_temp_gravity) ||
        !ReadGridPod(in, stage.tile_shake) ||
        !ReadGridPod(in, stage.backwall_tile_shake) ||
        !ReadGridExplicit(in, stage.backwall_tiles, ReadTile) ||
        !ReadTileVector(in, stage.backwall_fill_tiles) ||
        !ReadGridExplicit(in, stage.embedded_treasures, ReadEmbeddedTreasure) ||
        !ReadGridPod(in, stage.rooms) ||
        !ReadVectorPod(in, stage.path)) {
        return false;
    }

    std::uint32_t spawn_count = 0;
    if (!ReadPod(in, spawn_count)) {
        return false;
    }
    stage.ent_spawns.resize(spawn_count);
    for (EntSpawn& spawn : stage.ent_spawns) {
        if (!ReadEntSpawn(in, spawn)) {
            return false;
        }
    }

    if (!ReadBackgroundStamps(in, stage.background_stamps)) {
        return false;
    }
    std::uint32_t annotation_count = 0;
    if (!ReadPod(in, annotation_count)) {
        return false;
    }
    stage.stagegen_annotations.resize(annotation_count);
    for (StageGenAnnotation& annotation : stage.stagegen_annotations) {
        if (!ReadStageGenAnnotation(in, annotation)) {
            return false;
        }
    }

    return ReadStageLights(in, stage.lights) &&
           ReadPod(in, stage.block_anim_id) &&
           ReadSizeIndex(in, stage.next_light_vid) &&
           ReadPod(in, stage.tile_change_generation);
}

void WriteEntPool(std::ostream& out, const EntPool& ents) {
    const std::uint32_t ent_count = static_cast<std::uint32_t>(ents.ents.size());
    WritePod(out, ent_count);
    for (const Ent& ent : ents.ents) {
        WriteEnt(out, ent);
    }
    WriteSizeIndexVector(out, ents.available_ids);
}

bool ReadEntPool(std::istream& in, EntPool& ents) {
    std::uint32_t ent_count = 0;
    if (!ReadPod(in, ent_count)) {
        return false;
    }

    ents.ents.resize(ent_count);
    for (std::uint32_t i = 0; i < ent_count; ++i) {
        if (!ReadEnt(in, ents.ents[i])) {
            return false;
        }
    }

    return ReadSizeIndexVector(in, ents.available_ids);
}

void WritePlayerRegistry(std::ostream& out, const PlayerRegistry& players) {
    const std::uint32_t count = static_cast<std::uint32_t>(players.slots.size());
    WritePod(out, count);
    for (const PlayerSlot& slot : players.slots) {
        WritePod(out, slot.player_id);
        WriteOptionalVid(out, slot.ent_vid);
        WritePlayerConnectionKind(out, slot.connection_kind);
        WriteBoolByte(out, slot.connected);
        WriteBoolByte(out, slot.primary_local);
        WriteString(out, slot.display_name);
        WritePod(out, slot.input_frame);
        WritePod(out, slot.previous_input_frame);
        WritePod(out, slot.inputs);
        WritePod(out, slot.immediate_inputs);
    }
}

bool ReadPlayerRegistry(std::istream& in, PlayerRegistry& players) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    players.slots.resize(count);
    for (PlayerSlot& slot : players.slots) {
        if (!ReadPod(in, slot.player_id) ||
            !ReadOptionalVid(in, slot.ent_vid) ||
            !ReadPlayerConnectionKind(in, slot.connection_kind) ||
            !ReadBoolByte(in, slot.connected) ||
            !ReadBoolByte(in, slot.primary_local) ||
            !ReadString(in, slot.display_name) ||
            !ReadPod(in, slot.input_frame) ||
            !ReadPod(in, slot.previous_input_frame) ||
            !ReadPod(in, slot.inputs) ||
            !ReadPod(in, slot.immediate_inputs)) {
            return false;
        }
    }
    return true;
}

void WriteContactBookkeeping(std::ostream& out, const ContactBookkeeping& contact) {
    const auto write_contact_cooldowns = [&](const std::vector<ContactCooldownEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WritePod(out, count);
        for (const ContactCooldownEntry& entry : entries) {
            WriteVid(out, entry.source_vid);
            WriteVid(out, entry.target_vid);
            WritePod(out, entry.expires_on_stage_frame);
        }
    };
    const auto write_interaction_cooldowns =
        [&](const std::vector<InteractionCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WritePod(out, count);
            for (const InteractionCooldownEntry& entry : entries) {
                WriteVid(out, entry.source_vid);
                WriteVid(out, entry.target_vid);
                WritePod(out, static_cast<std::uint8_t>(entry.kind));
                WritePod(out, entry.expires_on_stage_frame);
            }
        };
    const auto write_ent_dispatches = [&](const std::vector<EntContactDispatchEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WritePod(out, count);
        for (const EntContactDispatchEntry& entry : entries) {
            WriteVid(out, entry.first_vid);
            WriteVid(out, entry.second_vid);
        }
    };
    const auto write_proj_body_cooldowns =
        [&](const std::vector<ProjBodyImpactCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WritePod(out, count);
            for (const ProjBodyImpactCooldownEntry& entry : entries) {
                WriteVid(out, entry.first_vid);
                WriteVid(out, entry.second_vid);
                WritePod(out, entry.expires_on_stage_frame);
            }
        };

    write_contact_cooldowns(contact.contact_cooldowns);
    write_interaction_cooldowns(contact.interaction_cooldowns);
    write_ent_dispatches(contact.ent_contact_dispatches_this_tick);
    write_proj_body_cooldowns(contact.proj_body_impact_cooldowns);
}

bool ReadContactBookkeeping(std::istream& in, ContactBookkeeping& contact) {
    const auto read_count = [&](std::uint32_t& count) {
        return ReadPod(in, count);
    };
    const auto read_contact_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.contact_cooldowns.resize(count);
        for (ContactCooldownEntry& entry : contact.contact_cooldowns) {
            if (!ReadVid(in, entry.source_vid) ||
                !ReadVid(in, entry.target_vid) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
        }
        return true;
    };
    const auto read_interaction_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.interaction_cooldowns.resize(count);
        for (InteractionCooldownEntry& entry : contact.interaction_cooldowns) {
            std::uint8_t kind = 0;
            if (!ReadVid(in, entry.source_vid) ||
                !ReadVid(in, entry.target_vid) ||
                !ReadPod(in, kind) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
            entry.kind = static_cast<InteractionCooldownKind>(kind);
        }
        return true;
    };
    const auto read_ent_dispatches = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.ent_contact_dispatches_this_tick.resize(count);
        for (EntContactDispatchEntry& entry : contact.ent_contact_dispatches_this_tick) {
            if (!ReadVid(in, entry.first_vid) ||
                !ReadVid(in, entry.second_vid)) {
                return false;
            }
        }
        return true;
    };
    const auto read_proj_body_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.proj_body_impact_cooldowns.resize(count);
        for (ProjBodyImpactCooldownEntry& entry : contact.proj_body_impact_cooldowns) {
            if (!ReadVid(in, entry.first_vid) ||
                !ReadVid(in, entry.second_vid) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
        }
        return true;
    };

    return read_contact_cooldowns() &&
           read_interaction_cooldowns() &&
           read_ent_dispatches() &&
           read_proj_body_cooldowns();
}

void WriteToolSlot(std::ostream& out, const ToolSlot& slot) {
    const std::uint8_t kind = static_cast<std::uint8_t>(slot.kind);
    WritePod(out, kind);
    WritePod(out, slot.count);
    WritePod(out, slot.cooldown);
    WriteBoolByte(out, slot.active);
}

bool ReadToolSlot(std::istream& in, ToolSlot& slot) {
    std::uint8_t kind = 0;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    bool active = false;
    if (!ReadPod(in, kind) ||
        !ReadPod(in, count) ||
        !ReadPod(in, cooldown) ||
        !ReadBoolByte(in, active)) {
        return false;
    }
    if (kind >= static_cast<std::uint8_t>(ToolKind::ThrowStickyBomb) + 1U) {
        return false;
    }
    slot.kind = static_cast<ToolKind>(kind);
    slot.count = count;
    slot.cooldown = cooldown;
    slot.active = active;
    return true;
}

void WriteEntToolState(std::ostream& out, const EntToolState& state) {
    WriteVid(out, state.owner_vid);
    for (const ToolSlot& slot : state.slots) {
        WriteToolSlot(out, slot);
    }
}

bool ReadEntToolState(std::istream& in, EntToolState& state) {
    if (!ReadVid(in, state.owner_vid)) {
        return false;
    }
    for (ToolSlot& slot : state.slots) {
        if (!ReadToolSlot(in, slot)) {
            return false;
        }
    }
    return true;
}

void WriteEntToolStates(std::ostream& out, const std::vector<EntToolState>& states) {
    const std::uint32_t count = static_cast<std::uint32_t>(states.size());
    WritePod(out, count);
    for (const EntToolState& state : states) {
        WriteEntToolState(out, state);
    }
}

bool ReadEntToolStates(std::istream& in, std::vector<EntToolState>& states) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    states.resize(count);
    for (EntToolState& state : states) {
        if (!ReadEntToolState(in, state)) {
            return false;
        }
    }
    return true;
}

void WriteSnapshot(std::ostream& out, const GameplaySnapshot& snapshot) {
    WriteMode(out, snapshot.mode);
    WriteSettings(out, snapshot.settings);
    WritePod(out, snapshot.menu_inputs);
    WritePod(out, snapshot.menu_input_snapshot);
    WritePod(out, snapshot.previous_menu_input_snapshot);
    WritePod(out, snapshot.menu_input_debounce_timers);
    WritePod(out, snapshot.playing_inputs);
    WritePod(out, snapshot.immediate_playing_inputs);
    WritePod(out, snapshot.playing_input_snapshot);
    WritePod(out, snapshot.previous_playing_input_snapshot);
    WritePod(out, snapshot.previous_immediate_playing_input_snapshot);
    WritePod(out, snapshot.title_menu_selection);
    WritePod(out, snapshot.settings_menu_selection);
    WritePod(out, snapshot.video_settings_menu_selection);
    WritePod(out, snapshot.ui_settings_menu_selection);
    WritePod(out, snapshot.post_fx_settings_menu_selection);
    WritePod(out, snapshot.lighting_settings_menu_selection);
    WriteOptionalSizeIndex(out, snapshot.video_settings_target_window_size_index);
    WriteOptionalSizeIndex(out, snapshot.video_settings_target_resolution_index);
    WriteOptionalBoolByte(out, snapshot.video_settings_target_fullscreen);
    WriteBoolByte(out, snapshot.rebuild_render_texture);
    WriteBoolByte(out, snapshot.choosing_control_binding);
    WritePod(out, snapshot.debug_overlay);
    WritePod(out, snapshot.debug_shake_brush);
    WritePod(out, snapshot.debug_audio_brush);
    WritePod(out, snapshot.debug_fluid_brush);
    WritePod(out, snapshot.stage_rotation);
    WritePod(out, snapshot.player_tuning);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.drng);
    WritePod(out, snapshot.stagegen_drng);
    WriteMode(out, snapshot.menu_return_to);
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalStageTransitionTarget(out, snapshot.pending_stage_transition);
    WriteMultiplayerRespawnMode(out, snapshot.multiplayer_respawn_mode);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.depth);
    WritePod(out, snapshot.sac_altar_favor);
    WritePod(out, snapshot.sac_altar_reward_tier);
    WritePod(out, snapshot.quest_state);
    WritePlayerRegistry(out, snapshot.players);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteStage(out, snapshot.stage);
    WriteOptionalVid(out, snapshot.controlled_ent_vid);
    WriteOptionalPod(out, snapshot.spectator_target_player_id);
    WriteOptionalVid(out, snapshot.mouse_trailer_vid);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
    WritePod(out, snapshot.play_cam_pos);
}

bool ReadSnapshot(std::istream& in, GameplaySnapshot& snapshot) {
    return ReadMode(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPod(in, snapshot.menu_inputs) &&
           ReadPod(in, snapshot.menu_input_snapshot) &&
           ReadPod(in, snapshot.previous_menu_input_snapshot) &&
           ReadPod(in, snapshot.menu_input_debounce_timers) &&
           ReadPod(in, snapshot.playing_inputs) &&
           ReadPod(in, snapshot.immediate_playing_inputs) &&
           ReadPod(in, snapshot.playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadPod(in, snapshot.title_menu_selection) &&
           ReadPod(in, snapshot.settings_menu_selection) &&
           ReadPod(in, snapshot.video_settings_menu_selection) &&
           ReadPod(in, snapshot.ui_settings_menu_selection) &&
           ReadPod(in, snapshot.post_fx_settings_menu_selection) &&
           ReadPod(in, snapshot.lighting_settings_menu_selection) &&
           ReadOptionalSizeIndex(in, snapshot.video_settings_target_window_size_index) &&
           ReadOptionalSizeIndex(in, snapshot.video_settings_target_resolution_index) &&
           ReadOptionalBoolByte(in, snapshot.video_settings_target_fullscreen) &&
           ReadBoolByte(in, snapshot.rebuild_render_texture) &&
           ReadBoolByte(in, snapshot.choosing_control_binding) &&
           ReadPod(in, snapshot.debug_overlay) &&
           ReadPod(in, snapshot.debug_shake_brush) &&
           ReadPod(in, snapshot.debug_audio_brush) &&
           ReadPod(in, snapshot.debug_fluid_brush) &&
           ReadPod(in, snapshot.stage_rotation) &&
           ReadPod(in, snapshot.player_tuning) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.drng) &&
           ReadPod(in, snapshot.stagegen_drng) &&
           ReadMode(in, snapshot.menu_return_to) &&
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalStageTransitionTarget(in, snapshot.pending_stage_transition) &&
           ReadMultiplayerRespawnMode(in, snapshot.multiplayer_respawn_mode) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.depth) &&
           ReadPod(in, snapshot.sac_altar_favor) &&
           ReadPod(in, snapshot.sac_altar_reward_tier) &&
           ReadPod(in, snapshot.quest_state) &&
           ReadPlayerRegistry(in, snapshot.players) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadStage(in, snapshot.stage) &&
           ReadOptionalVid(in, snapshot.controlled_ent_vid) &&
           ReadOptionalPod(in, snapshot.spectator_target_player_id) &&
           ReadOptionalVid(in, snapshot.mouse_trailer_vid) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
           ReadPod(in, snapshot.play_cam_pos);
}

void WriteSimPlayerSlotSnapshot(std::ostream& out, const SimPlayerSlotSnapshot& slot) {
    WritePod(out, slot.player_id);
    WriteOptionalVid(out, slot.ent_vid);
    WriteBoolByte(out, slot.connected);
    WriteString(out, slot.display_name);
    WritePod(out, slot.input_frame);
    WritePod(out, slot.previous_input_frame);
    WritePod(out, slot.inputs);
    WritePod(out, slot.immediate_inputs);
}

bool ReadSimPlayerSlotSnapshot(std::istream& in, SimPlayerSlotSnapshot& slot) {
    return ReadPod(in, slot.player_id) &&
           ReadOptionalVid(in, slot.ent_vid) &&
           ReadBoolByte(in, slot.connected) &&
           ReadString(in, slot.display_name) &&
           ReadPod(in, slot.input_frame) &&
           ReadPod(in, slot.previous_input_frame) &&
           ReadPod(in, slot.inputs) &&
           ReadPod(in, slot.immediate_inputs);
}

void WriteSimPlayerSlots(std::ostream& out, const std::vector<SimPlayerSlotSnapshot>& slots) {
    const std::uint32_t count = static_cast<std::uint32_t>(slots.size());
    WritePod(out, count);
    for (const SimPlayerSlotSnapshot& slot : slots) {
        WriteSimPlayerSlotSnapshot(out, slot);
    }
}

bool ReadSimPlayerSlots(std::istream& in, std::vector<SimPlayerSlotSnapshot>& slots) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    slots.resize(count);
    for (SimPlayerSlotSnapshot& slot : slots) {
        if (!ReadSimPlayerSlotSnapshot(in, slot)) {
            return false;
        }
    }
    return true;
}

void WriteSimNetEntLinks(
    std::ostream& out,
    const std::vector<SimNetEntLinkSnapshot>& links
) {
    const std::uint32_t count = static_cast<std::uint32_t>(links.size());
    WritePod(out, count);
    for (const SimNetEntLinkSnapshot& link : links) {
        WritePod(out, link.net_id);
        WriteVid(out, link.local_vid);
        WriteBoolByte(out, link.has_input_owner);
        WritePod(out, link.input_owner_player_id);
    }
}

bool ReadSimNetEntLinks(
    std::istream& in,
    std::vector<SimNetEntLinkSnapshot>& links
) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    links.resize(count);
    for (SimNetEntLinkSnapshot& link : links) {
        if (!ReadPod(in, link.net_id) ||
            !ReadVid(in, link.local_vid) ||
            !ReadBoolByte(in, link.has_input_owner) ||
            !ReadPod(in, link.input_owner_player_id)) {
            return false;
        }
    }
    return true;
}

void WriteSimNetEntIdAliases(
    std::ostream& out,
    const std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    WriteVectorPod(out, aliases);
}

bool ReadSimNetEntIdAliases(
    std::istream& in,
    std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    return ReadVectorPod(in, aliases);
}

void WriteSimSnapshot(std::ostream& out, const SimSnapshot& snapshot) {
    WriteMode(out, snapshot.mode);
    WriteSettings(out, snapshot.settings);
    WritePod(out, snapshot.playing_inputs);
    WritePod(out, snapshot.immediate_playing_inputs);
    WritePod(out, snapshot.playing_input_snapshot);
    WritePod(out, snapshot.previous_playing_input_snapshot);
    WritePod(out, snapshot.previous_immediate_playing_input_snapshot);
    WritePod(out, snapshot.stage_rotation);
    WritePod(out, snapshot.player_tuning);
    WriteBoolByte(out, snapshot.running);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.drng);
    WritePod(out, snapshot.stagegen_drng);
    WriteMode(out, snapshot.menu_return_to);
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalStageTransitionTarget(out, snapshot.pending_stage_transition);
    WriteMultiplayerRespawnMode(out, snapshot.multiplayer_respawn_mode);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.depth);
    WritePod(out, snapshot.sac_altar_favor);
    WritePod(out, snapshot.sac_altar_reward_tier);
    WriteVidVector(out, snapshot.interact_claimed_vids_this_frame);
    WritePod(out, snapshot.quest_state);
    WriteSimPlayerSlots(out, snapshot.players);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteVidVector(out, snapshot.area_listener_vids);
    WriteStage(out, snapshot.stage);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
    WritePod(out, snapshot.net_next_local_ent_id);
    WriteSimNetEntLinks(out, snapshot.net_ent_links);
    WriteSimNetEntIdAliases(out, snapshot.net_ent_id_aliases);
}

bool ReadSimSnapshot(std::istream& in, SimSnapshot& snapshot) {
    return ReadMode(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPod(in, snapshot.playing_inputs) &&
           ReadPod(in, snapshot.immediate_playing_inputs) &&
           ReadPod(in, snapshot.playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadPod(in, snapshot.stage_rotation) &&
           ReadPod(in, snapshot.player_tuning) &&
           ReadBoolByte(in, snapshot.running) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.drng) &&
           ReadPod(in, snapshot.stagegen_drng) &&
           ReadMode(in, snapshot.menu_return_to) &&
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalStageTransitionTarget(in, snapshot.pending_stage_transition) &&
           ReadMultiplayerRespawnMode(in, snapshot.multiplayer_respawn_mode) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.depth) &&
           ReadPod(in, snapshot.sac_altar_favor) &&
           ReadPod(in, snapshot.sac_altar_reward_tier) &&
           ReadVidVector(in, snapshot.interact_claimed_vids_this_frame) &&
           ReadPod(in, snapshot.quest_state) &&
           ReadSimPlayerSlots(in, snapshot.players) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadVidVector(in, snapshot.area_listener_vids) &&
           ReadStage(in, snapshot.stage) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
           ReadPod(in, snapshot.net_next_local_ent_id) &&
           ReadSimNetEntLinks(in, snapshot.net_ent_links) &&
           ReadSimNetEntIdAliases(in, snapshot.net_ent_id_aliases);
}

} // namespace

std::vector<std::uint8_t> SerializeGameplaySnapshotToBytes(const GameplaySnapshot& snapshot) {
    std::ostringstream out(std::ios::out | std::ios::binary);
    WriteSnapshot(out, snapshot);
    const std::string text = out.str();
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool DeserializeGameplaySnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    GameplaySnapshot& snapshot
) {
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(text, std::ios::in | std::ios::binary);
    return ReadSnapshot(in, snapshot);
}

std::vector<std::uint8_t> SerializeSimSnapshotToBytes(const SimSnapshot& snapshot) {
    std::ostringstream out(std::ios::out | std::ios::binary);
    WriteSimSnapshot(out, snapshot);
    const std::string text = out.str();
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool DeserializeSimSnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    SimSnapshot& snapshot
) {
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(text, std::ios::in | std::ios::binary);
    return ReadSimSnapshot(in, snapshot);
}

bool SaveRecordingToFile(const DebugPlayback& debug, std::string* status_out) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ofstream out(debug.file_path.data(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for writing.";
        }
        return false;
    }

    WritePod(out, kRecordingMagic);
    WritePod(out, kRecordingVersion);
    const std::uint32_t count = static_cast<std::uint32_t>(debug.recorded_snapshots.size());
    WritePod(out, count);
    for (const GameplaySnapshot& snapshot : debug.recorded_snapshots) {
        WriteSnapshot(out, snapshot);
    }

    if (!out.good()) {
        if (status_out != nullptr) {
            *status_out = "Write failed.";
        }
        return false;
    }

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Saved %u snapshots.", count);
        *status_out = buffer;
    }
    return true;
}

bool LoadRecordingFromFile(DebugPlayback& debug, std::string* status_out) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ifstream in(debug.file_path.data(), std::ios::binary);
    if (!in.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for reading.";
        }
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    if (!ReadPod(in, magic) || !ReadPod(in, version) || !ReadPod(in, count)) {
        if (status_out != nullptr) {
            *status_out = "Failed to read recording header.";
        }
        return false;
    }
    if (magic != kRecordingMagic) {
        if (status_out != nullptr) {
            *status_out = "Recording file magic mismatch.";
        }
        return false;
    }
    if (version != kRecordingVersion) {
        if (status_out != nullptr) {
            *status_out = "Recording file version mismatch.";
        }
        return false;
    }

    std::deque<GameplaySnapshot> loaded_snapshots;
    for (std::uint32_t i = 0; i < count; ++i) {
        GameplaySnapshot snapshot;
        if (!ReadSnapshot(in, snapshot)) {
            if (status_out != nullptr) {
                *status_out = "Failed while reading snapshot data.";
            }
            return false;
        }
        loaded_snapshots.push_back(std::move(snapshot));
    }

    debug.recorded_snapshots = std::move(loaded_snapshots);
    debug.playback_index =
        debug.recorded_snapshots.empty() ? 0 : debug.recorded_snapshots.size() - 1;

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Loaded %u snapshots.", count);
        *status_out = buffer;
    }
    return true;
}

} // namespace splonks::debug_playback_internal

namespace splonks {

std::vector<std::uint8_t> SerializeGameplaySnapshotToBytes(const GameplaySnapshot& snapshot) {
    return debug_playback_internal::SerializeGameplaySnapshotToBytes(snapshot);
}

bool DeserializeGameplaySnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    GameplaySnapshot& snapshot
) {
    return debug_playback_internal::DeserializeGameplaySnapshotFromBytes(bytes, snapshot);
}

std::vector<std::uint8_t> SerializeSimSnapshotToBytes(const SimSnapshot& snapshot) {
    return debug_playback_internal::SerializeSimSnapshotToBytes(snapshot);
}

bool DeserializeSimSnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    SimSnapshot& snapshot
) {
    return debug_playback_internal::DeserializeSimSnapshotFromBytes(bytes, snapshot);
}

} // namespace splonks
