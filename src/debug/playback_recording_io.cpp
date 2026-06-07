#include "debug/playback_internal.hpp"

#include "buying.hpp"
#include "ents/damsel.hpp"
#include "sim/fxp.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 101;

enum class BuyableCallbackKind : std::uint8_t {
    None = 0,
    TryBuyEntForMoney = 1,
    BuyDamsel = 2,
};

void WriteRawByte(std::ostream& out, std::uint8_t value) {
    const char byte = static_cast<char>(value);
    out.write(&byte, 1);
}

bool ReadRawByte(std::istream& in, std::uint8_t& value) {
    char byte = 0;
    in.read(&byte, 1);
    if (!in.good()) {
        return false;
    }
    value = static_cast<std::uint8_t>(byte);
    return in.good();
}

void WriteFloat(std::ostream& out, float value);
bool ReadFloat(std::istream& in, float& value);
void WriteSimScalar(std::ostream& out, sim::Scalar value);
bool ReadSimScalar(std::istream& in, sim::Scalar& value);
void WriteInt32(std::ostream& out, int value);
bool ReadInt32(std::istream& in, int& value);
void WriteSigned32(std::ostream& out, std::int32_t value);
bool ReadSigned32(std::istream& in, std::int32_t& value);
void WriteUint8(std::ostream& out, std::uint8_t value);
bool ReadUint8(std::istream& in, std::uint8_t& value);
void WriteUint16(std::ostream& out, std::uint16_t value);
bool ReadUint16(std::istream& in, std::uint16_t& value);
void WriteUint32(std::ostream& out, std::uint32_t value);
bool ReadUint32(std::istream& in, std::uint32_t& value);
void WriteUint64(std::ostream& out, std::uint64_t value);
bool ReadUint64(std::istream& in, std::uint64_t& value);

void WriteOptionalUint32Index(std::ostream& out, const std::optional<std::uint32_t>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (has_value) {
        WriteUint32(out, *value);
    }
}

bool ReadOptionalUint32Index(std::istream& in, std::optional<std::uint32_t>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    if (!ReadUint32(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteUint32Vector(std::ostream& out, const std::vector<std::uint32_t>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WriteUint32(out, count);
    for (const std::uint32_t value : values) {
        WriteUint32(out, value);
    }
}

bool ReadUint32Vector(std::istream& in, std::vector<std::uint32_t>& values) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    values.resize(count);
    for (std::uint32_t& value : values) {
        if (!ReadUint32(in, value)) {
            return false;
        }
    }
    return true;
}

void WriteVid(std::ostream& out, const VID& vid) {
    WriteUint32(out, vid.id);
    WriteUint32(out, vid.version);
}

bool ReadVid(std::istream& in, VID& vid) {
    return ReadUint32(in, vid.id) &&
           ReadUint32(in, vid.version);
}

void WriteOptionalVid(std::ostream& out, const std::optional<VID>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteVid(out, *value);
    }
}

bool ReadOptionalVid(std::istream& in, std::optional<VID>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    WriteUint32(out, count);
    for (const VID& value : values) {
        WriteVid(out, value);
    }
}

bool ReadVidVector(std::istream& in, std::vector<VID>& values) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteUint8(out, has_value);
    if (values.has_value()) {
        WriteVidVector(out, *values);
    }
}

bool ReadOptionalVidVector(std::istream& in, std::optional<std::vector<VID>>& values) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    WriteUint32(out, count);
    if (count > 0) {
        out.write(value.data(), static_cast<std::streamsize>(count));
    }
}

bool ReadString(std::istream& in, std::string& value) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteUint8(out, stored);
}

bool ReadBoolByte(std::istream& in, bool& value) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, has_value);
    if (has_value) {
        WriteBoolByte(out, *value);
    }
}

bool ReadOptionalBoolByte(std::istream& in, std::optional<bool>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    WriteUint8(out, stored);
}

bool ReadPlayerConnectionKind(std::istream& in, PlayerConnectionKind& kind) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadMode(std::istream& in, Mode& mode) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadSettingsMode(std::istream& in, SettingsMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadPostProcessEffect(std::istream& in, PostProcessEffect& effect) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadStageType(std::istream& in, StageType& stage_type) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadMultiplayerRespawnMode(std::istream& in, MultiplayerRespawnMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, stored);
}

bool ReadAttachMode(std::istream& in, AttachMode& mode) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteUint32(out, *value);
    }
}

bool ReadOptionalAFrameId(std::istream& in, std::optional<AFrameId>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    if (!ReadUint32(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

template <typename T>
void WriteEnumByte(std::ostream& out, T value) {
    const std::uint8_t stored = static_cast<std::uint8_t>(value);
    WriteUint8(out, stored);
}

template <typename T>
bool ReadEnumByte(std::istream& in, T& value, T max_value) {
    std::uint8_t stored = 0;
    if (!ReadUint8(in, stored)) {
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
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteEnumByte(out, *value);
    }
}

template <typename T>
bool ReadOptionalEnumByte(std::istream& in, std::optional<T>& value, T max_value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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

void WriteTitleMenuOption(std::ostream& out, TitleMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadTitleMenuOption(std::istream& in, TitleMenuOption& option) {
    return ReadEnumByte(in, option, TitleMenuOption::Quit);
}

void WriteSettingsMenuOption(std::ostream& out, SettingsMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadSettingsMenuOption(std::istream& in, SettingsMenuOption& option) {
    return ReadEnumByte(in, option, SettingsMenuOption::Back);
}

void WriteVideoSettingsMenuOption(std::ostream& out, VideoSettingsMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadVideoSettingsMenuOption(std::istream& in, VideoSettingsMenuOption& option) {
    return ReadEnumByte(in, option, VideoSettingsMenuOption::Back);
}

void WriteUiSettingsMenuOption(std::ostream& out, UiSettingsMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadUiSettingsMenuOption(std::istream& in, UiSettingsMenuOption& option) {
    return ReadEnumByte(in, option, UiSettingsMenuOption::Back);
}

void WritePostFxSettingsMenuOption(std::ostream& out, PostFxSettingsMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadPostFxSettingsMenuOption(std::istream& in, PostFxSettingsMenuOption& option) {
    return ReadEnumByte(in, option, PostFxSettingsMenuOption::Back);
}

void WriteLightingSettingsMenuOption(std::ostream& out, LightingSettingsMenuOption option) {
    WriteEnumByte(out, option);
}

bool ReadLightingSettingsMenuOption(std::istream& in, LightingSettingsMenuOption& option) {
    return ReadEnumByte(in, option, LightingSettingsMenuOption::Back);
}

void WriteStageRotationWrapPolicy(std::ostream& out, StageRotationWrapPolicy policy) {
    WriteEnumByte(out, policy);
}

bool ReadStageRotationWrapPolicy(std::istream& in, StageRotationWrapPolicy& policy) {
    return ReadEnumByte(in, policy, StageRotationWrapPolicy::SwapXYWrap);
}

void WriteDebugFluidBrushMode(std::ostream& out, DebugFluidBrushState::Mode mode) {
    WriteEnumByte(out, mode);
}

bool ReadDebugFluidBrushMode(std::istream& in, DebugFluidBrushState::Mode& mode) {
    return ReadEnumByte(in, mode, DebugFluidBrushState::Mode::GlobalGravityDirection);
}

void WriteQuestId(std::ostream& out, QuestId id) {
    WriteEnumByte(out, id);
}

bool ReadQuestId(std::istream& in, QuestId& id) {
    return ReadEnumByte(in, id, QuestId::Classic);
}

void WriteEntType(std::ostream& out, EntType type) {
    const std::uint16_t stored = static_cast<std::uint16_t>(type);
    WriteUint16(out, stored);
}

bool ReadEntType(std::istream& in, EntType& type) {
    std::uint16_t stored = 0;
    if (!ReadUint16(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint16_t>(EntType::DebugMovingLight)) {
        return false;
    }
    type = static_cast<EntType>(stored);
    return true;
}

void WriteVec2(std::ostream& out, const Vec2& value) {
    WriteFloat(out, value.x);
    WriteFloat(out, value.y);
}

bool ReadVec2(std::istream& in, Vec2& value) {
    return ReadFloat(in, value.x) &&
           ReadFloat(in, value.y);
}

void WriteIVec2(std::ostream& out, const IVec2& value) {
    WriteInt32(out, value.x);
    WriteInt32(out, value.y);
}

bool ReadIVec2(std::istream& in, IVec2& value) {
    return ReadInt32(in, value.x) &&
           ReadInt32(in, value.y);
}

void WriteUVec2(std::ostream& out, const UVec2& value) {
    const std::uint32_t x = static_cast<std::uint32_t>(value.x);
    const std::uint32_t y = static_cast<std::uint32_t>(value.y);
    WriteUint32(out, x);
    WriteUint32(out, y);
}

bool ReadUVec2(std::istream& in, UVec2& value) {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    if (!ReadUint32(in, x) || !ReadUint32(in, y)) {
        return false;
    }
    value.x = x;
    value.y = y;
    return true;
}

void WriteColor3(std::ostream& out, const Color3& color) {
    WriteFloat(out, color.r);
    WriteFloat(out, color.g);
    WriteFloat(out, color.b);
}

bool ReadColor3(std::istream& in, Color3& color) {
    return ReadFloat(in, color.r) &&
           ReadFloat(in, color.g) &&
           ReadFloat(in, color.b);
}

void WriteSimColor3(std::ostream& out, const sim::Color3& color) {
    WriteSimScalar(out, color.r);
    WriteSimScalar(out, color.g);
    WriteSimScalar(out, color.b);
}

bool ReadSimColor3(std::istream& in, sim::Color3& color) {
    return ReadSimScalar(in, color.r) &&
           ReadSimScalar(in, color.g) &&
           ReadSimScalar(in, color.b);
}

void WriteUVec2Vector(std::ostream& out, const std::vector<UVec2>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WriteUint32(out, count);
    for (const UVec2& value : values) {
        WriteUVec2(out, value);
    }
}

bool ReadUVec2Vector(std::istream& in, std::vector<UVec2>& values) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    values.resize(count);
    for (UVec2& value : values) {
        if (!ReadUVec2(in, value)) {
            return false;
        }
    }
    return true;
}

void WriteFloat(std::ostream& out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    static_assert(std::numeric_limits<float>::is_iec559);
    std::memcpy(&bits, &value, sizeof(bits));
    WriteUint32(out, bits);
}

bool ReadFloat(std::istream& in, float& value) {
    std::uint32_t bits = 0;
    if (!ReadUint32(in, bits)) {
        return false;
    }
    static_assert(sizeof(bits) == sizeof(value));
    static_assert(std::numeric_limits<float>::is_iec559);
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

void WriteSimScalar(std::ostream& out, sim::Scalar value) {
    WriteSigned32(out, value.raw_value());
}

bool ReadSimScalar(std::istream& in, sim::Scalar& value) {
    std::int32_t raw_value = 0;
    if (!ReadSigned32(in, raw_value)) {
        return false;
    }
    value = sim::Scalar::from_raw(raw_value);
    return true;
}

void WriteDouble(std::ostream& out, double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::memcpy(&bits, &value, sizeof(bits));
    WriteUint64(out, bits);
}

bool ReadDouble(std::istream& in, double& value) {
    std::uint64_t bits = 0;
    if (!ReadUint64(in, bits)) {
        return false;
    }
    static_assert(sizeof(bits) == sizeof(value));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

void WriteInt32(std::ostream& out, int value) {
    const std::int32_t stored = static_cast<std::int32_t>(value);
    WriteSigned32(out, stored);
}

bool ReadInt32(std::istream& in, int& value) {
    std::int32_t stored = 0;
    if (!ReadSigned32(in, stored)) {
        return false;
    }
    value = static_cast<int>(stored);
    return true;
}

void WriteOptionalInt32(std::ostream& out, const std::optional<int>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteInt32(out, *value);
    }
}

bool ReadOptionalInt32(std::istream& in, std::optional<int>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    int loaded = 0;
    if (!ReadInt32(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteSigned32(std::ostream& out, std::int32_t value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    WriteUint32(out, bits);
}

bool ReadSigned32(std::istream& in, std::int32_t& value) {
    std::uint32_t bits = 0;
    if (!ReadUint32(in, bits)) {
        return false;
    }
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

void WriteUint8(std::ostream& out, std::uint8_t value) {
    WriteRawByte(out, value);
}

bool ReadUint8(std::istream& in, std::uint8_t& value) {
    return ReadRawByte(in, value);
}

void WriteUint16(std::ostream& out, std::uint16_t value) {
    for (std::uint32_t shift = 0; shift < 16; shift += 8) {
        WriteRawByte(
            out,
            static_cast<std::uint8_t>(
                (value >> shift) & static_cast<std::uint16_t>(0xFFU)
            )
        );
    }
}

bool ReadUint16(std::istream& in, std::uint16_t& value) {
    value = 0;
    for (std::uint32_t shift = 0; shift < 16; shift += 8) {
        std::uint8_t byte = 0;
        if (!ReadRawByte(in, byte)) {
            return false;
        }
        value = static_cast<std::uint16_t>(
            value | static_cast<std::uint16_t>(static_cast<std::uint16_t>(byte) << shift)
        );
    }
    return true;
}

void WriteUint32(std::ostream& out, std::uint32_t value) {
    for (std::uint32_t shift = 0; shift < 32; shift += 8) {
        WriteRawByte(
            out,
            static_cast<std::uint8_t>(
                (value >> shift) & static_cast<std::uint32_t>(0xFFU)
            )
        );
    }
}

bool ReadUint32(std::istream& in, std::uint32_t& value) {
    value = 0;
    for (std::uint32_t shift = 0; shift < 32; shift += 8) {
        std::uint8_t byte = 0;
        if (!ReadRawByte(in, byte)) {
            return false;
        }
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return true;
}

void WriteUint64(std::ostream& out, std::uint64_t value) {
    for (std::uint32_t shift = 0; shift < 64; shift += 8) {
        WriteRawByte(
            out,
            static_cast<std::uint8_t>(
                (value >> shift) & static_cast<std::uint64_t>(0xFFU)
            )
        );
    }
}

bool ReadUint64(std::istream& in, std::uint64_t& value) {
    value = 0;
    for (std::uint32_t shift = 0; shift < 64; shift += 8) {
        std::uint8_t byte = 0;
        if (!ReadRawByte(in, byte)) {
            return false;
        }
        value |= static_cast<std::uint64_t>(byte) << shift;
    }
    return true;
}

void WriteOptionalUint32(std::ostream& out, const std::optional<std::uint32_t>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteUint32(out, *value);
    }
}

bool ReadOptionalUint32(std::istream& in, std::optional<std::uint32_t>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    if (!ReadUint32(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WritePlayerId(std::ostream& out, PlayerId player_id) {
    const std::uint32_t stored = static_cast<std::uint32_t>(player_id);
    WriteUint32(out, stored);
}

bool ReadPlayerId(std::istream& in, PlayerId& player_id) {
    std::uint32_t stored = 0;
    if (!ReadUint32(in, stored)) {
        return false;
    }
    player_id = static_cast<PlayerId>(stored);
    return true;
}

void WriteOptionalPlayerId(std::ostream& out, const std::optional<PlayerId>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WritePlayerId(out, *value);
    }
}

bool ReadOptionalPlayerId(std::istream& in, std::optional<PlayerId>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    PlayerId loaded = kInvalidPlayerId;
    if (!ReadPlayerId(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteAudioAssetId(std::ostream& out, AudioAssetId audio_asset_id) {
    const std::uint32_t stored = static_cast<std::uint32_t>(audio_asset_id);
    WriteUint32(out, stored);
}

bool ReadAudioAssetId(std::istream& in, AudioAssetId& audio_asset_id) {
    std::uint32_t stored = 0;
    if (!ReadUint32(in, stored)) {
        return false;
    }
    audio_asset_id = static_cast<AudioAssetId>(stored);
    return true;
}

void WriteOptionalAudioAssetId(std::ostream& out, const std::optional<AudioAssetId>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteAudioAssetId(out, *value);
    }
}

bool ReadOptionalAudioAssetId(std::istream& in, std::optional<AudioAssetId>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    if (has_value != 1) {
        return false;
    }
    AudioAssetId loaded = kInvalidAudioAssetId;
    if (!ReadAudioAssetId(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteDetRng(std::ostream& out, const DetRng& rng) {
    WriteUint64(out, static_cast<std::uint64_t>(rng.state));
}

bool ReadDetRng(std::istream& in, DetRng& rng) {
    std::uint64_t state = 0;
    if (!ReadUint64(in, state)) {
        return false;
    }
    rng.state = state;
    return true;
}

void WriteNetEntId(std::ostream& out, network::NetEntId net_id) {
    const std::uint64_t stored = static_cast<std::uint64_t>(net_id);
    WriteUint64(out, stored);
}

bool ReadNetEntId(std::istream& in, network::NetEntId& net_id) {
    std::uint64_t stored = 0;
    if (!ReadUint64(in, stored)) {
        return false;
    }
    net_id = static_cast<network::NetEntId>(stored);
    return true;
}

void WriteButtonState(std::ostream& out, const ButtonState& button) {
    WriteBoolByte(out, button.down);
    WriteBoolByte(out, button.pressed);
    WriteBoolByte(out, button.released);
}

bool ReadButtonState(std::istream& in, ButtonState& button) {
    return ReadBoolByte(in, button.down) &&
           ReadBoolByte(in, button.pressed) &&
           ReadBoolByte(in, button.released);
}

void WriteMenuInputs(std::ostream& out, const MenuInputs& inputs) {
    WriteButtonState(out, inputs.left);
    WriteButtonState(out, inputs.right);
    WriteButtonState(out, inputs.up);
    WriteButtonState(out, inputs.down);
    WriteButtonState(out, inputs.confirm);
    WriteButtonState(out, inputs.back);
    WriteButtonState(out, inputs.page_prev);
    WriteButtonState(out, inputs.page_next);
}

bool ReadMenuInputs(std::istream& in, MenuInputs& inputs) {
    return ReadButtonState(in, inputs.left) &&
           ReadButtonState(in, inputs.right) &&
           ReadButtonState(in, inputs.up) &&
           ReadButtonState(in, inputs.down) &&
           ReadButtonState(in, inputs.confirm) &&
           ReadButtonState(in, inputs.back) &&
           ReadButtonState(in, inputs.page_prev) &&
           ReadButtonState(in, inputs.page_next);
}

void WriteMenuInputSnapshot(std::ostream& out, const MenuInputSnapshot& inputs) {
    WriteBoolByte(out, inputs.left);
    WriteBoolByte(out, inputs.right);
    WriteBoolByte(out, inputs.up);
    WriteBoolByte(out, inputs.down);
    WriteBoolByte(out, inputs.confirm);
    WriteBoolByte(out, inputs.back);
    WriteBoolByte(out, inputs.page_prev);
    WriteBoolByte(out, inputs.page_next);
}

bool ReadMenuInputSnapshot(std::istream& in, MenuInputSnapshot& inputs) {
    return ReadBoolByte(in, inputs.left) &&
           ReadBoolByte(in, inputs.right) &&
           ReadBoolByte(in, inputs.up) &&
           ReadBoolByte(in, inputs.down) &&
           ReadBoolByte(in, inputs.confirm) &&
           ReadBoolByte(in, inputs.back) &&
           ReadBoolByte(in, inputs.page_prev) &&
           ReadBoolByte(in, inputs.page_next);
}

void WritePlayingInputs(std::ostream& out, const PlayingInputs& inputs) {
    WriteButtonState(out, inputs.left);
    WriteButtonState(out, inputs.right);
    WriteButtonState(out, inputs.up);
    WriteButtonState(out, inputs.down);
    WriteButtonState(out, inputs.jump);
    WriteButtonState(out, inputs.run);
    WriteButtonState(out, inputs.use_button);
    WriteButtonState(out, inputs.equip_button);
    WriteButtonState(out, inputs.pick_up_drop);
    WriteButtonState(out, inputs.stop);
    WriteButtonState(out, inputs.bomb);
    WriteButtonState(out, inputs.rope);
    WriteButtonState(out, inputs.attack);
    WriteButtonState(out, inputs.buy_button);
    WriteButtonState(out, inputs.emote_up);
    WriteButtonState(out, inputs.emote_down);
    WriteButtonState(out, inputs.quit);
    WriteButtonState(out, inputs.toggle_collision_boxes);
    WriteButtonState(out, inputs.regenerate_level);
    WriteUVec2(out, inputs.mouse_pos);
}

bool ReadPlayingInputs(std::istream& in, PlayingInputs& inputs) {
    return ReadButtonState(in, inputs.left) &&
           ReadButtonState(in, inputs.right) &&
           ReadButtonState(in, inputs.up) &&
           ReadButtonState(in, inputs.down) &&
           ReadButtonState(in, inputs.jump) &&
           ReadButtonState(in, inputs.run) &&
           ReadButtonState(in, inputs.use_button) &&
           ReadButtonState(in, inputs.equip_button) &&
           ReadButtonState(in, inputs.pick_up_drop) &&
           ReadButtonState(in, inputs.stop) &&
           ReadButtonState(in, inputs.bomb) &&
           ReadButtonState(in, inputs.rope) &&
           ReadButtonState(in, inputs.attack) &&
           ReadButtonState(in, inputs.buy_button) &&
           ReadButtonState(in, inputs.emote_up) &&
           ReadButtonState(in, inputs.emote_down) &&
           ReadButtonState(in, inputs.quit) &&
           ReadButtonState(in, inputs.toggle_collision_boxes) &&
           ReadButtonState(in, inputs.regenerate_level) &&
           ReadUVec2(in, inputs.mouse_pos);
}

void WriteInputFrame(std::ostream& out, const InputFrame& inputs) {
    WriteBoolByte(out, inputs.left);
    WriteBoolByte(out, inputs.right);
    WriteBoolByte(out, inputs.up);
    WriteBoolByte(out, inputs.down);
    WriteBoolByte(out, inputs.jump);
    WriteBoolByte(out, inputs.run);
    WriteBoolByte(out, inputs.use_button);
    WriteBoolByte(out, inputs.equip_button);
    WriteBoolByte(out, inputs.pick_up_drop);
    WriteBoolByte(out, inputs.stop);
    WriteBoolByte(out, inputs.bomb);
    WriteBoolByte(out, inputs.rope);
    WriteBoolByte(out, inputs.attack);
    WriteBoolByte(out, inputs.buy_button);
    WriteBoolByte(out, inputs.emote_up);
    WriteBoolByte(out, inputs.emote_down);
    WriteBoolByte(out, inputs.quit);
    WriteBoolByte(out, inputs.toggle_collision_boxes);
    WriteBoolByte(out, inputs.regenerate_level);
    WriteUVec2(out, inputs.mouse_pos);
}

bool ReadInputFrame(std::istream& in, InputFrame& inputs) {
    return ReadBoolByte(in, inputs.left) &&
           ReadBoolByte(in, inputs.right) &&
           ReadBoolByte(in, inputs.up) &&
           ReadBoolByte(in, inputs.down) &&
           ReadBoolByte(in, inputs.jump) &&
           ReadBoolByte(in, inputs.run) &&
           ReadBoolByte(in, inputs.use_button) &&
           ReadBoolByte(in, inputs.equip_button) &&
           ReadBoolByte(in, inputs.pick_up_drop) &&
           ReadBoolByte(in, inputs.stop) &&
           ReadBoolByte(in, inputs.bomb) &&
           ReadBoolByte(in, inputs.rope) &&
           ReadBoolByte(in, inputs.attack) &&
           ReadBoolByte(in, inputs.buy_button) &&
           ReadBoolByte(in, inputs.emote_up) &&
           ReadBoolByte(in, inputs.emote_down) &&
           ReadBoolByte(in, inputs.quit) &&
           ReadBoolByte(in, inputs.toggle_collision_boxes) &&
           ReadBoolByte(in, inputs.regenerate_level) &&
           ReadUVec2(in, inputs.mouse_pos);
}

void WritePlayingInputSnapshot(std::ostream& out, const PlayingInputSnapshot& inputs) {
    WriteBoolByte(out, inputs.left);
    WriteBoolByte(out, inputs.right);
    WriteBoolByte(out, inputs.up);
    WriteBoolByte(out, inputs.down);
    WriteBoolByte(out, inputs.jump);
    WriteBoolByte(out, inputs.run);
    WriteBoolByte(out, inputs.use_button);
    WriteBoolByte(out, inputs.equip_button);
    WriteBoolByte(out, inputs.pick_up_drop);
    WriteBoolByte(out, inputs.stop);
    WriteBoolByte(out, inputs.bomb);
    WriteBoolByte(out, inputs.rope);
    WriteBoolByte(out, inputs.attack);
    WriteBoolByte(out, inputs.buy_button);
    WriteBoolByte(out, inputs.emote_up);
    WriteBoolByte(out, inputs.emote_down);
    WriteBoolByte(out, inputs.quit);
    WriteBoolByte(out, inputs.toggle_collision_boxes);
    WriteBoolByte(out, inputs.regenerate_level);
    WriteUVec2(out, inputs.mouse_pos);
}

bool ReadPlayingInputSnapshot(std::istream& in, PlayingInputSnapshot& inputs) {
    return ReadBoolByte(in, inputs.left) &&
           ReadBoolByte(in, inputs.right) &&
           ReadBoolByte(in, inputs.up) &&
           ReadBoolByte(in, inputs.down) &&
           ReadBoolByte(in, inputs.jump) &&
           ReadBoolByte(in, inputs.run) &&
           ReadBoolByte(in, inputs.use_button) &&
           ReadBoolByte(in, inputs.equip_button) &&
           ReadBoolByte(in, inputs.pick_up_drop) &&
           ReadBoolByte(in, inputs.stop) &&
           ReadBoolByte(in, inputs.bomb) &&
           ReadBoolByte(in, inputs.rope) &&
           ReadBoolByte(in, inputs.attack) &&
           ReadBoolByte(in, inputs.buy_button) &&
           ReadBoolByte(in, inputs.emote_up) &&
           ReadBoolByte(in, inputs.emote_down) &&
           ReadBoolByte(in, inputs.quit) &&
           ReadBoolByte(in, inputs.toggle_collision_boxes) &&
           ReadBoolByte(in, inputs.regenerate_level) &&
           ReadUVec2(in, inputs.mouse_pos);
}

void WriteMenuInputDebounceTimers(std::ostream& out, const MenuInputDebounceTimers& timers) {
    WriteFloat(out, timers.left);
    WriteFloat(out, timers.right);
    WriteFloat(out, timers.up);
    WriteFloat(out, timers.down);
}

bool ReadMenuInputDebounceTimers(std::istream& in, MenuInputDebounceTimers& timers) {
    return ReadFloat(in, timers.left) &&
           ReadFloat(in, timers.right) &&
           ReadFloat(in, timers.up) &&
           ReadFloat(in, timers.down);
}

void WriteOptionalVec2(std::ostream& out, const std::optional<Vec2>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteVec2(out, *value);
    }
}

bool ReadOptionalVec2(std::istream& in, std::optional<Vec2>& value) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    WriteUint8(out, target.debug_variant);
    WriteFixedCharArray(out, target.quest_id.data(), target.quest_id.size());
    WriteFixedCharArray(out, target.quest_stage_id.data(), target.quest_stage_id.size());
}

bool ReadStageLoadTarget(std::istream& in, StageLoadTarget& target) {
    return ReadStageLoadTargetKind(in, target.kind) &&
           ReadStageType(in, target.stage_type) &&
           ReadDebugLevelKind(in, target.debug_level) &&
           ReadUint8(in, target.debug_variant) &&
           ReadFixedCharArray(in, target.quest_id.data(), target.quest_id.size()) &&
           ReadFixedCharArray(in, target.quest_stage_id.data(), target.quest_stage_id.size());
}

void WriteStageTransitionTarget(std::ostream& out, const StageTransitionTarget& target) {
    WriteStageLoadTarget(out, target.destination);
    WriteBoolByte(out, target.preserve_player_state);
    WriteOptionalUint32(out, target.seed);
}

bool ReadStageTransitionTarget(std::istream& in, StageTransitionTarget& target) {
    return ReadStageLoadTarget(in, target.destination) &&
           ReadBoolByte(in, target.preserve_player_state) &&
           ReadOptionalUint32(in, target.seed);
}

void WriteOptionalStageTransitionTarget(
    std::ostream& out,
    const std::optional<StageTransitionTarget>& value
) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WriteUint8(out, has_value);
    if (value.has_value()) {
        WriteStageTransitionTarget(out, *value);
    }
}

bool ReadOptionalStageTransitionTarget(
    std::istream& in,
    std::optional<StageTransitionTarget>& value
) {
    std::uint8_t has_value = 0;
    if (!ReadUint8(in, has_value)) {
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
    if (!ReadUint8(in, stored)) {
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
    WriteUint32(out, buyable.display_quantity);
    WriteOptionalAFrameId(out, buyable.display_icon_anim_id);
    WriteOptionalVid(out, buyable.shop_owner_vid);
    const std::optional<BuyableCallbackKind> callback_kind = GetBuyableCallbackKind(buyable.on_try_buy);
    const std::uint8_t callback = callback_kind.has_value()
        ? static_cast<std::uint8_t>(*callback_kind)
        : 0xFFU;
    WriteUint8(out, callback);
}

bool ReadBuyable(std::istream& in, Buyable& buyable) {
    bool active = false;
    std::uint32_t display_quantity = 0;
    std::optional<AFrameId> display_icon_anim_id;
    std::optional<VID> shop_owner_vid;
    EntOnTryBuy on_try_buy = nullptr;
    if (!ReadBoolByte(in, active) ||
        !ReadUint32(in, display_quantity) ||
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
    WriteUint32(out, use_state.frames);
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
        !ReadUint32(in, frames) ||
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
    WriteUint8(out, id);
    WriteSigned32(out, effect.count);
    WriteFloat(out, effect.value);
    WriteUint32(out, effect.frames_remaining);
}

bool ReadEffectInstance(std::istream& in, EffectInstance& effect) {
    std::uint8_t id = 0;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
    if (!ReadUint8(in, id) ||
        !ReadSigned32(in, count) ||
        !ReadFloat(in, value) ||
        !ReadUint32(in, frames_remaining)) {
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
    WriteUint8(out, count);
    for (std::uint8_t i = 0; i < count; ++i) {
        WriteEffectInstance(out, effects->effects[i]);
    }
}

bool ReadEntEffects(std::istream& in, BoxedEntEffects& effects_box) {
    std::uint8_t count = 0;
    if (!ReadUint8(in, count)) {
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
    WriteUint32(out, animator.anim_id);
    WriteUint32(out, animator.current_frame);
    WriteFloat(out, animator.current_time);
    WriteFloat(out, animator.scale);
    WriteFloat(out, animator.speed);
    WriteBoolByte(out, animator.animate);
    WriteBoolByte(out, animator.loop);
    WriteBoolByte(out, animator.finished);
    const std::uint8_t playback_mode = static_cast<std::uint8_t>(animator.playback_mode);
    WriteUint8(out, playback_mode);
    WriteUint32(out, animator.play_count);
    WriteUint32(out, animator.plays_completed);
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
    if (!ReadUint32(in, animator.anim_id) ||
        !ReadUint32(in, animator.current_frame) ||
        !ReadFloat(in, animator.current_time) ||
        !ReadFloat(in, animator.scale) ||
        !ReadFloat(in, animator.speed) ||
        !ReadBoolByte(in, animate) ||
        !ReadBoolByte(in, loop) ||
        !ReadBoolByte(in, finished) ||
        !ReadUint8(in, playback_mode) ||
        !ReadUint32(in, animator.play_count) ||
        !ReadUint32(in, animator.plays_completed) ||
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
    WriteSimScalar(out, ent.shake);
    WriteSimScalar(out, ent.rotation);
    WriteSimScalar(out, ent.alpha);
    WriteUint32(out, ent.coyote_time);
    WriteUint32(out, ent.stun_timer);
    WriteBoolByte(out, ent.stun_recovers_on_ground);
    WriteBoolByte(out, ent.stun_recovers_while_held);
    WriteBoolByte(out, ent.can_be_picked_up);
    WriteBoolByte(out, ent.affected_by_cobweb);
    WriteBoolByte(out, ent.can_only_be_picked_up_if_dead_or_stunned);
    WriteBoolByte(out, ent.impassable);
    WriteBoolByte(out, ent.can_be_hung_on);
    WriteUint32(out, ent.fall_timer);
    WriteVec2(out, ent.pos);
    WriteVec2(out, ent.vel);
    WriteVec2(out, ent.acc);
    WriteFloat(out, ent.max_speed);
    WriteUint32(out, ent.jump_hold_gravity_frames_remaining);
    WriteFloat(out, ent.throw_velocity_scale);
    WriteFloat(out, ent.buoyancy);
    WriteEntEffects(out, ent.effects);
    WriteVec2(out, ent.size);
    WriteSimScalar(out, ent.self_light);
    WriteSimScalar(out, ent.light_strength);
    WriteSimColor3(out, ent.light_color);
    WriteInt32(out, ent.light_radius);
    WriteSimScalar(out, ent.dist_traveled_this_frame);
    WriteEnumByte(out, ent.facing);
    WriteBoolByte(out, ent.vertical_flip);
    WriteEnumByte(out, ent.draw_layer);
    WriteBoolByte(out, ent.render_enabled);
    WriteAFrameAnimator(out, ent.aframe_animator);
    WriteUint32(out, ent.jump_delay_frame_count);
    WriteBoolByte(out, ent.jumped_this_frame);
    WriteUint32(out, ent.climb_detach_cooldown);
    WriteOptionalEnumByte(out, ent.hang_side);
    WriteBoolByte(out, ent.can_hang_ledge);
    WriteBoolByte(out, ent.can_hang_wall);
    WriteUint32(out, ent.hang_count);
    WriteBoolByte(out, ent.holding);
    WriteOptionalEnumByte(out, ent.pickup_effect);
    WriteUint32(out, ent.money);
    WriteBuyable(out, ent.buyable);
    WriteOptionalUint32(out, ent.stage_spawn_index);
    WriteOptionalVid(out, ent.back_vid);
    WriteAttachMode(out, ent.attach_mode);
    WriteUseState(out, ent.use_state);
    WriteSimScalar(out, ent.travel_sound_countdown);
    WriteEnumByte(out, ent.travel_sound);
    WriteEnumByte(out, ent.condition);
    WriteEnumByte(out, ent.last_condition);
    WriteEnumByte(out, ent.ai_state);
    WriteEnumByte(out, ent.last_ai_state);
    WriteUint32(out, ent.movement_flags);
    WriteUint32(out, ent.health);
    WriteBoolByte(out, ent.hurt_on_contact);
    WriteBoolByte(out, ent.vanish_on_death);
    WriteBoolByte(out, ent.affected_by_ground_friction);
    WriteSimScalar(out, ent.support_ground_friction);
    WriteSimScalar(out, ent.push_acc);
    WriteOptionalAFrameId(out, ent.damage_anim);
    WriteOptionalAudioAssetId(out, ent.damage_sound);
    WriteOptionalAudioAssetId(out, ent.collide_sound);
    WriteOptionalAudioAssetId(out, ent.death_sound);
    WriteOptionalStageTransitionTarget(out, ent.transition_target);
    WriteInt32(out, ent.stage_exit_id);
    WriteFloat(out, ent.attack_weight);
    WriteFloat(out, ent.weight);
    WriteUint32(out, ent.bomb_throw_delay_countdown);
    WriteUint32(out, ent.rope_throw_delay_countdown);
    WriteUint32(out, ent.attack_delay_countdown);
    WriteUint32(out, ent.equip_delay_countdown);
    WriteOptionalVid(out, ent.thrown_by);
    WriteUint32(out, ent.thrown_immunity_timer);
    WriteEnumByte(out, ent.proj_contact_damage_type);
    WriteUint32(out, ent.proj_contact_damage_amount);
    WriteBoolByte(out, ent.can_apply_proj_contact);
    WriteUint32(out, ent.proj_contact_timer);
    WriteBoolByte(out, ent.collided);
    WriteBoolByte(out, ent.collided_last_frame);
    WriteUint32(out, ent.contact_sound_cooldown);
    WriteEnumByte(out, ent.damage_vuln);
    WriteBoolByte(out, ent.can_be_stunned);
    WriteIVec2(out, ent.point_a);
    WriteIVec2(out, ent.point_b);
    WriteIVec2(out, ent.point_c);
    WriteIVec2(out, ent.point_d);
    WriteEnumByte(out, ent.point_label_a);
    WriteEnumByte(out, ent.point_label_b);
    WriteEnumByte(out, ent.point_label_c);
    WriteEnumByte(out, ent.point_label_d);
    WriteOptionalVid(out, ent.holding_vid);
    WriteOptionalVid(out, ent.held_by_vid);
    WriteUint32(out, ent.holding_timer);
    WriteOptionalVid(out, ent.ent_a);
    WriteOptionalVid(out, ent.ent_b);
    WriteOptionalVid(out, ent.ent_c);
    WriteOptionalVid(out, ent.ent_d);
    WriteOptionalVidVector(out, ent.child_vids);
    WriteOptionalVidVector(out, ent.inside_vids);
    WriteEnumByte(out, ent.ent_label_a);
    WriteEnumByte(out, ent.alignment);
    WriteFloat(out, ent.counter_a);
    WriteFloat(out, ent.counter_b);
    WriteFloat(out, ent.counter_c);
    WriteFloat(out, ent.counter_d);
    WriteFloat(out, ent.threshold_a);
    WriteFloat(out, ent.threshold_b);
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
           ReadSimScalar(in, ent.shake) &&
           ReadSimScalar(in, ent.rotation) &&
           ReadSimScalar(in, ent.alpha) &&
           ReadUint32(in, ent.coyote_time) &&
           ReadUint32(in, ent.stun_timer) &&
           ReadBoolByte(in, ent.stun_recovers_on_ground) &&
           ReadBoolByte(in, ent.stun_recovers_while_held) &&
           ReadBoolByte(in, ent.can_be_picked_up) &&
           ReadBoolByte(in, ent.affected_by_cobweb) &&
           ReadBoolByte(in, ent.can_only_be_picked_up_if_dead_or_stunned) &&
           ReadBoolByte(in, ent.impassable) &&
           ReadBoolByte(in, ent.can_be_hung_on) &&
           ReadUint32(in, ent.fall_timer) &&
           ReadVec2(in, ent.pos) &&
           ReadVec2(in, ent.vel) &&
           ReadVec2(in, ent.acc) &&
           ReadFloat(in, ent.max_speed) &&
           ReadUint32(in, ent.jump_hold_gravity_frames_remaining) &&
           ReadFloat(in, ent.throw_velocity_scale) &&
           ReadFloat(in, ent.buoyancy) &&
           ReadEntEffects(in, ent.effects) &&
           ReadVec2(in, ent.size) &&
           ReadSimScalar(in, ent.self_light) &&
           ReadSimScalar(in, ent.light_strength) &&
           ReadSimColor3(in, ent.light_color) &&
           ReadInt32(in, ent.light_radius) &&
           ReadSimScalar(in, ent.dist_traveled_this_frame) &&
           ReadEnumByte(in, ent.facing, Side::Right) &&
           ReadBoolByte(in, ent.vertical_flip) &&
           ReadEnumByte(in, ent.draw_layer, DrawLayer::Foreground) &&
           ReadBoolByte(in, ent.render_enabled) &&
           ReadAFrameAnimator(in, ent.aframe_animator) &&
           ReadUint32(in, ent.jump_delay_frame_count) &&
           ReadBoolByte(in, ent.jumped_this_frame) &&
           ReadUint32(in, ent.climb_detach_cooldown) &&
           ReadOptionalEnumByte(in, ent.hang_side, Side::Right) &&
           ReadBoolByte(in, ent.can_hang_ledge) &&
           ReadBoolByte(in, ent.can_hang_wall) &&
           ReadUint32(in, ent.hang_count) &&
           ReadBoolByte(in, ent.holding) &&
           ReadOptionalEnumByte(in, ent.pickup_effect, EffectId::InWater) &&
           ReadUint32(in, ent.money) &&
           ReadBuyable(in, ent.buyable) &&
           ReadOptionalUint32(in, ent.stage_spawn_index) &&
           ReadOptionalVid(in, ent.back_vid) &&
           ReadAttachMode(in, ent.attach_mode) &&
           ReadUseState(in, ent.use_state) &&
           ReadSimScalar(in, ent.travel_sound_countdown) &&
           ReadEnumByte(in, ent.travel_sound, TravelSound::Two) &&
           ReadEnumByte(in, ent.condition, EntCondition::Stunned) &&
           ReadEnumByte(in, ent.last_condition, EntCondition::Stunned) &&
           ReadEnumByte(in, ent.ai_state, EntAiState::Returning) &&
           ReadEnumByte(in, ent.last_ai_state, EntAiState::Returning) &&
           ReadUint32(in, ent.movement_flags) &&
           ReadUint32(in, ent.health) &&
           ReadBoolByte(in, ent.hurt_on_contact) &&
           ReadBoolByte(in, ent.vanish_on_death) &&
           ReadBoolByte(in, ent.affected_by_ground_friction) &&
           ReadSimScalar(in, ent.support_ground_friction) &&
           ReadSimScalar(in, ent.push_acc) &&
           ReadOptionalAFrameId(in, ent.damage_anim) &&
           ReadOptionalAudioAssetId(in, ent.damage_sound) &&
           ReadOptionalAudioAssetId(in, ent.collide_sound) &&
           ReadOptionalAudioAssetId(in, ent.death_sound) &&
           ReadOptionalStageTransitionTarget(in, ent.transition_target) &&
           ReadInt32(in, ent.stage_exit_id) &&
           ReadFloat(in, ent.attack_weight) &&
           ReadFloat(in, ent.weight) &&
           ReadUint32(in, ent.bomb_throw_delay_countdown) &&
           ReadUint32(in, ent.rope_throw_delay_countdown) &&
           ReadUint32(in, ent.attack_delay_countdown) &&
           ReadUint32(in, ent.equip_delay_countdown) &&
           ReadOptionalVid(in, ent.thrown_by) &&
           ReadUint32(in, ent.thrown_immunity_timer) &&
           ReadEnumByte(in, ent.proj_contact_damage_type, DamageType::Fall) &&
           ReadUint32(in, ent.proj_contact_damage_amount) &&
           ReadBoolByte(in, ent.can_apply_proj_contact) &&
           ReadUint32(in, ent.proj_contact_timer) &&
           ReadBoolByte(in, ent.collided) &&
           ReadBoolByte(in, ent.collided_last_frame) &&
           ReadUint32(in, ent.contact_sound_cooldown) &&
           ReadEnumByte(in, ent.damage_vuln, DamageVuln::AnthingExceptJumpOn) &&
           ReadBoolByte(in, ent.can_be_stunned) &&
           ReadIVec2(in, ent.point_a) &&
           ReadIVec2(in, ent.point_b) &&
           ReadIVec2(in, ent.point_c) &&
           ReadIVec2(in, ent.point_d) &&
           ReadEnumByte(in, ent.point_label_a, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_b, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_c, PointLabel::Avoid) &&
           ReadEnumByte(in, ent.point_label_d, PointLabel::Avoid) &&
           ReadOptionalVid(in, ent.holding_vid) &&
           ReadOptionalVid(in, ent.held_by_vid) &&
           ReadUint32(in, ent.holding_timer) &&
           ReadOptionalVid(in, ent.ent_a) &&
           ReadOptionalVid(in, ent.ent_b) &&
           ReadOptionalVid(in, ent.ent_c) &&
           ReadOptionalVid(in, ent.ent_d) &&
           ReadOptionalVidVector(in, ent.child_vids) &&
           ReadOptionalVidVector(in, ent.inside_vids) &&
           ReadEnumByte(in, ent.ent_label_a, EntLabel::AttachedToThis) &&
           ReadEnumByte(in, ent.alignment, Alignment::Enemy) &&
           ReadFloat(in, ent.counter_a) &&
           ReadFloat(in, ent.counter_b) &&
           ReadFloat(in, ent.counter_c) &&
           ReadFloat(in, ent.counter_d) &&
           ReadFloat(in, ent.threshold_a) &&
           ReadFloat(in, ent.threshold_b);
}

void WriteVideoSettings(std::ostream& out, const VideoSettings& settings) {
    WriteUVec2(out, settings.resolution);
    WriteBoolByte(out, settings.fullscreen);
    WriteBoolByte(out, settings.vsync);
    WriteUVec2Vector(out, settings.resolution_options);
}

bool ReadVideoSettings(std::istream& in, VideoSettings& settings) {
    return ReadUVec2(in, settings.resolution) &&
           ReadBoolByte(in, settings.fullscreen) &&
           ReadBoolByte(in, settings.vsync) &&
           ReadUVec2Vector(in, settings.resolution_options);
}

void WriteAudioSettings(std::ostream& out, const AudioSettings& settings) {
    WriteFloat(out, settings.music_volume);
    WriteFloat(out, settings.sfx_volume);
    WriteFloat(out, settings.pan_half_width_px);
    WriteBoolByte(out, settings.acoustics_enabled);
    WriteFloat(out, settings.acoustics_occlusion_listener_epsilon_px);
    WriteBoolByte(out, settings.acoustics_reverb_enabled);
    WriteFloat(out, settings.acoustics_listener_room_weight);
    WriteFloat(out, settings.acoustics_direct_min_cutoff_hz);
    WriteFloat(out, settings.acoustics_direct_max_cutoff_hz);
    WriteFloat(out, settings.acoustics_occluded_cutoff_hz);
    WriteFloat(out, settings.acoustics_occluded_direct_gain);
    WriteFloat(out, settings.acoustics_reverb_send);
    WriteFloat(out, settings.acoustics_reverb_delay_ms);
    WriteFloat(out, settings.acoustics_reverb_feedback);
    WriteFloat(out, settings.acoustics_reverb_min_cutoff_hz);
    WriteFloat(out, settings.acoustics_reverb_max_cutoff_hz);
}

bool ReadAudioSettings(std::istream& in, AudioSettings& settings) {
    return ReadFloat(in, settings.music_volume) &&
           ReadFloat(in, settings.sfx_volume) &&
           ReadFloat(in, settings.pan_half_width_px) &&
           ReadBoolByte(in, settings.acoustics_enabled) &&
           ReadFloat(in, settings.acoustics_occlusion_listener_epsilon_px) &&
           ReadBoolByte(in, settings.acoustics_reverb_enabled) &&
           ReadFloat(in, settings.acoustics_listener_room_weight) &&
           ReadFloat(in, settings.acoustics_direct_min_cutoff_hz) &&
           ReadFloat(in, settings.acoustics_direct_max_cutoff_hz) &&
           ReadFloat(in, settings.acoustics_occluded_cutoff_hz) &&
           ReadFloat(in, settings.acoustics_occluded_direct_gain) &&
           ReadFloat(in, settings.acoustics_reverb_send) &&
           ReadFloat(in, settings.acoustics_reverb_delay_ms) &&
           ReadFloat(in, settings.acoustics_reverb_feedback) &&
           ReadFloat(in, settings.acoustics_reverb_min_cutoff_hz) &&
           ReadFloat(in, settings.acoustics_reverb_max_cutoff_hz);
}

void WriteControlsSettings(std::ostream& out, const ControlsSettings& settings) {
    WriteUint32(out, settings.jump);
    WriteUint32(out, settings.shoot);
}

bool ReadControlsSettings(std::istream& in, ControlsSettings& settings) {
    return ReadUint32(in, settings.jump) &&
           ReadUint32(in, settings.shoot);
}

void WriteUiSettings(std::ostream& out, const UiSettings& settings) {
    WriteFloat(out, settings.icon_scale);
    WriteFloat(out, settings.status_icon_scale);
    WriteFloat(out, settings.tool_slot_scale);
    WriteFloat(out, settings.tool_icon_scale);
}

bool ReadUiSettings(std::istream& in, UiSettings& settings) {
    return ReadFloat(in, settings.icon_scale) &&
           ReadFloat(in, settings.status_icon_scale) &&
           ReadFloat(in, settings.tool_slot_scale) &&
           ReadFloat(in, settings.tool_icon_scale);
}

void WritePostProcessSettings(std::ostream& out, const PostProcessSettings& settings) {
    WritePostProcessEffect(out, settings.effect);
    WriteBoolByte(out, settings.terrain_lighting);
    WriteBoolByte(out, settings.terrain_seam_ao);
    WriteBoolByte(out, settings.terrain_exposure_lighting);
    WriteBoolByte(out, settings.backwall_lighting);
    WriteFloat(out, settings.player_lamp_strength);
    WriteFloat(out, settings.embedded_treasure_brightness);
    WriteFloat(out, settings.openness_ambient_strength);
    WriteFloat(out, settings.openness_ambient_gamma);
    WriteBoolByte(out, settings.lighting_temporal_smoothing);
    WriteFloat(out, settings.lighting_temporal_smoothing_response);
    WriteFloat(out, settings.terrain_seam_ao_amount);
    WriteFloat(out, settings.terrain_seam_ao_size);
    WriteFloat(out, settings.terrain_exposure_amount);
    WriteBoolByte(out, settings.terrain_exposure_remap_enabled);
    WriteFloat(out, settings.terrain_exposure_input_min);
    WriteFloat(out, settings.terrain_exposure_input_max);
    WriteFloat(out, settings.terrain_exposure_gamma);
    WriteBoolByte(out, settings.terrain_exposure_output_levels_enabled);
    WriteFloat(out, settings.terrain_exposure_min_brightness);
    WriteFloat(out, settings.terrain_exposure_max_brightness);
    WriteFloat(out, settings.terrain_exposure_diagonal_weight);
    WriteFloat(out, settings.terrain_exposure_smoothing);
    WriteFloat(out, settings.backwall_brightness);
    WriteBoolByte(out, settings.backwall_remap_enabled);
    WriteFloat(out, settings.backwall_input_min);
    WriteFloat(out, settings.backwall_input_max);
    WriteFloat(out, settings.backwall_gamma);
    WriteBoolByte(out, settings.backwall_output_levels_enabled);
    WriteFloat(out, settings.backwall_min_brightness);
    WriteFloat(out, settings.backwall_max_brightness);
    WriteFloat(out, settings.backwall_smoothing);
    WriteFloat(out, settings.crt_scanline_amount);
    WriteFloat(out, settings.crt_scanline_edge_start);
    WriteFloat(out, settings.crt_scanline_edge_falloff);
    WriteFloat(out, settings.crt_scanline_edge_strength);
    WriteFloat(out, settings.crt_zoom);
    WriteFloat(out, settings.crt_warp_amount);
    WriteFloat(out, settings.crt_vignette_amount);
    WriteFloat(out, settings.crt_vignette_intensity);
    WriteFloat(out, settings.crt_grille_amount);
    WriteFloat(out, settings.crt_brightness_boost);
}

bool ReadPostProcessSettings(std::istream& in, PostProcessSettings& settings) {
    return ReadPostProcessEffect(in, settings.effect) &&
           ReadBoolByte(in, settings.terrain_lighting) &&
           ReadBoolByte(in, settings.terrain_seam_ao) &&
           ReadBoolByte(in, settings.terrain_exposure_lighting) &&
           ReadBoolByte(in, settings.backwall_lighting) &&
           ReadFloat(in, settings.player_lamp_strength) &&
           ReadFloat(in, settings.embedded_treasure_brightness) &&
           ReadFloat(in, settings.openness_ambient_strength) &&
           ReadFloat(in, settings.openness_ambient_gamma) &&
           ReadBoolByte(in, settings.lighting_temporal_smoothing) &&
           ReadFloat(in, settings.lighting_temporal_smoothing_response) &&
           ReadFloat(in, settings.terrain_seam_ao_amount) &&
           ReadFloat(in, settings.terrain_seam_ao_size) &&
           ReadFloat(in, settings.terrain_exposure_amount) &&
           ReadBoolByte(in, settings.terrain_exposure_remap_enabled) &&
           ReadFloat(in, settings.terrain_exposure_input_min) &&
           ReadFloat(in, settings.terrain_exposure_input_max) &&
           ReadFloat(in, settings.terrain_exposure_gamma) &&
           ReadBoolByte(in, settings.terrain_exposure_output_levels_enabled) &&
           ReadFloat(in, settings.terrain_exposure_min_brightness) &&
           ReadFloat(in, settings.terrain_exposure_max_brightness) &&
           ReadFloat(in, settings.terrain_exposure_diagonal_weight) &&
           ReadFloat(in, settings.terrain_exposure_smoothing) &&
           ReadFloat(in, settings.backwall_brightness) &&
           ReadBoolByte(in, settings.backwall_remap_enabled) &&
           ReadFloat(in, settings.backwall_input_min) &&
           ReadFloat(in, settings.backwall_input_max) &&
           ReadFloat(in, settings.backwall_gamma) &&
           ReadBoolByte(in, settings.backwall_output_levels_enabled) &&
           ReadFloat(in, settings.backwall_min_brightness) &&
           ReadFloat(in, settings.backwall_max_brightness) &&
           ReadFloat(in, settings.backwall_smoothing) &&
           ReadFloat(in, settings.crt_scanline_amount) &&
           ReadFloat(in, settings.crt_scanline_edge_start) &&
           ReadFloat(in, settings.crt_scanline_edge_falloff) &&
           ReadFloat(in, settings.crt_scanline_edge_strength) &&
           ReadFloat(in, settings.crt_zoom) &&
           ReadFloat(in, settings.crt_warp_amount) &&
           ReadFloat(in, settings.crt_vignette_amount) &&
           ReadFloat(in, settings.crt_vignette_intensity) &&
           ReadFloat(in, settings.crt_grille_amount) &&
           ReadFloat(in, settings.crt_brightness_boost);
}

void WriteFluidSettings(std::ostream& out, const FluidSettings& settings) {
    WriteBoolByte(out, settings.simulation_enabled);
    WriteInt32(out, settings.simulation_interval_frames);
    WriteFloat(out, settings.transfer_per_step);
    WriteFloat(out, settings.gravity_x);
    WriteFloat(out, settings.gravity_y);
    WriteFloat(out, settings.pressure_strength);
    WriteFloat(out, settings.velocity_damping);
    WriteFloat(out, settings.temp_gravity_decay);
    WriteBoolByte(out, settings.temporal_smoothing_enabled);
    WriteFloat(out, settings.temporal_smoothing_response);
    WriteFloat(out, settings.render_cutoff_amount);
    WriteFloat(out, settings.water_alpha);
    WriteBoolByte(out, settings.lighting_enabled);
    WriteFloat(out, settings.lighting_strength);
}

bool ReadFluidSettings(std::istream& in, FluidSettings& settings) {
    return ReadBoolByte(in, settings.simulation_enabled) &&
           ReadInt32(in, settings.simulation_interval_frames) &&
           ReadFloat(in, settings.transfer_per_step) &&
           ReadFloat(in, settings.gravity_x) &&
           ReadFloat(in, settings.gravity_y) &&
           ReadFloat(in, settings.pressure_strength) &&
           ReadFloat(in, settings.velocity_damping) &&
           ReadFloat(in, settings.temp_gravity_decay) &&
           ReadBoolByte(in, settings.temporal_smoothing_enabled) &&
           ReadFloat(in, settings.temporal_smoothing_response) &&
           ReadFloat(in, settings.render_cutoff_amount) &&
           ReadFloat(in, settings.water_alpha) &&
           ReadBoolByte(in, settings.lighting_enabled) &&
           ReadFloat(in, settings.lighting_strength);
}

void WriteWaterEffectSettings(std::ostream& out, const WaterEffectSettings& settings) {
    WriteFloat(out, settings.gravity_scale);
    WriteFloat(out, settings.velocity_damping_x);
    WriteFloat(out, settings.velocity_damping_y);
    WriteFloat(out, settings.move_speed_scale);
    WriteFloat(out, settings.max_fall_speed);
    WriteFloat(out, settings.buoyancy_strength);
    WriteFloat(out, settings.fall_timer_rate);
    WriteFloat(out, settings.stomp_damage_scale);
    WriteFloat(out, settings.swim_impulse);
}

bool ReadWaterEffectSettings(std::istream& in, WaterEffectSettings& settings) {
    return ReadFloat(in, settings.gravity_scale) &&
           ReadFloat(in, settings.velocity_damping_x) &&
           ReadFloat(in, settings.velocity_damping_y) &&
           ReadFloat(in, settings.move_speed_scale) &&
           ReadFloat(in, settings.max_fall_speed) &&
           ReadFloat(in, settings.buoyancy_strength) &&
           ReadFloat(in, settings.fall_timer_rate) &&
           ReadFloat(in, settings.stomp_damage_scale) &&
           ReadFloat(in, settings.swim_impulse);
}

void WriteDebugUiSettings(std::ostream& out, const DebugUiSettings& settings) {
    WriteBoolByte(out, settings.menu_visible);
    WriteBoolByte(out, settings.playback_visible);
    WriteBoolByte(out, settings.level_visible);
    WriteBoolByte(out, settings.ents_visible);
    WriteBoolByte(out, settings.ent_annotations_visible);
    WriteBoolByte(out, settings.shake_brush_visible);
    WriteBoolByte(out, settings.audio_brush_visible);
    WriteBoolByte(out, settings.fluid_brush_visible);
    WriteBoolByte(out, settings.fluid_brush_enabled);
    WriteBoolByte(out, settings.fluid_brush_replace_solid_tiles);
    WriteInt32(out, settings.fluid_brush_mode);
    WriteInt32(out, settings.fluid_brush_radius_tiles);
    WriteFloat(out, settings.fluid_brush_paint_gravity_x);
    WriteFloat(out, settings.fluid_brush_paint_gravity_y);
    WriteBoolByte(out, settings.fluid_brush_show_flow_indicators);
    WriteBoolByte(out, settings.audio_settings_visible);
    WriteBoolByte(out, settings.ui_settings_visible);
    WriteBoolByte(out, settings.post_fx_settings_visible);
    WriteBoolByte(out, settings.lighting_settings_visible);
    WriteBoolByte(out, settings.graphics_settings_visible);
    WriteBoolByte(out, settings.camera_settings_visible);
    WriteBoolByte(out, settings.performance_settings_visible);
    WriteBoolByte(out, settings.player_tuning_visible);
    WriteUint32(out, settings.ent_swap_type);
    WriteUint32(out, settings.default_spawn_type);
    WriteBoolByte(out, settings.default_spawn_enabled);
    WriteBoolByte(out, settings.ent_swap_fresh);
    WriteBoolByte(out, settings.ent_swap_keep_passives);
    WriteBoolByte(out, settings.ent_swap_keep_money);
    WriteBoolByte(out, settings.ent_swap_keep_health);
    WriteBoolByte(out, settings.ent_swap_keep_tools);
}

bool ReadDebugUiSettings(std::istream& in, DebugUiSettings& settings) {
    return ReadBoolByte(in, settings.menu_visible) &&
           ReadBoolByte(in, settings.playback_visible) &&
           ReadBoolByte(in, settings.level_visible) &&
           ReadBoolByte(in, settings.ents_visible) &&
           ReadBoolByte(in, settings.ent_annotations_visible) &&
           ReadBoolByte(in, settings.shake_brush_visible) &&
           ReadBoolByte(in, settings.audio_brush_visible) &&
           ReadBoolByte(in, settings.fluid_brush_visible) &&
           ReadBoolByte(in, settings.fluid_brush_enabled) &&
           ReadBoolByte(in, settings.fluid_brush_replace_solid_tiles) &&
           ReadInt32(in, settings.fluid_brush_mode) &&
           ReadInt32(in, settings.fluid_brush_radius_tiles) &&
           ReadFloat(in, settings.fluid_brush_paint_gravity_x) &&
           ReadFloat(in, settings.fluid_brush_paint_gravity_y) &&
           ReadBoolByte(in, settings.fluid_brush_show_flow_indicators) &&
           ReadBoolByte(in, settings.audio_settings_visible) &&
           ReadBoolByte(in, settings.ui_settings_visible) &&
           ReadBoolByte(in, settings.post_fx_settings_visible) &&
           ReadBoolByte(in, settings.lighting_settings_visible) &&
           ReadBoolByte(in, settings.graphics_settings_visible) &&
           ReadBoolByte(in, settings.camera_settings_visible) &&
           ReadBoolByte(in, settings.performance_settings_visible) &&
           ReadBoolByte(in, settings.player_tuning_visible) &&
           ReadUint32(in, settings.ent_swap_type) &&
           ReadUint32(in, settings.default_spawn_type) &&
           ReadBoolByte(in, settings.default_spawn_enabled) &&
           ReadBoolByte(in, settings.ent_swap_fresh) &&
           ReadBoolByte(in, settings.ent_swap_keep_passives) &&
           ReadBoolByte(in, settings.ent_swap_keep_money) &&
           ReadBoolByte(in, settings.ent_swap_keep_health) &&
           ReadBoolByte(in, settings.ent_swap_keep_tools);
}

void WritePlayerTuningState(std::ostream& out, const PlayerTuningState& tuning) {
    WriteFloat(out, tuning.gravity_scale);
    WriteFloat(out, tuning.max_fall_speed);
    WriteFloat(out, tuning.jump_impulse);
    WriteFloat(out, tuning.spring_shoes_jump_impulse_bonus);
    WriteInt32(out, tuning.jump_hold_frames);
    WriteInt32(out, tuning.coyote_frames);
    WriteInt32(out, tuning.jump_delay_frames);
    WriteInt32(out, tuning.fall_damage_light_frames);
    WriteInt32(out, tuning.fall_damage_medium_frames);
    WriteInt32(out, tuning.fall_damage_heavy_frames);
    WriteFloat(out, tuning.walk_speed);
    WriteFloat(out, tuning.run_speed);
    WriteFloat(out, tuning.move_acc);
    WriteFloat(out, tuning.run_acc);
    WriteFloat(out, tuning.ground_friction_scale);
    WriteFloat(out, tuning.air_friction);
    WriteFloat(out, tuning.climb_speed);
    WriteFloat(out, tuning.climb_depart_horizontal_speed);
    WriteFloat(out, tuning.climb_probe_bias_pixels);
    WriteFloat(out, tuning.climb_probe_x_scale);
    WriteInt32(out, tuning.climb_required_probe_hits);
    WriteInt32(out, tuning.climb_detach_cooldown);
    WriteInt32(out, tuning.hang_drop_cooldown);
    WriteInt32(out, tuning.glove_hang_drop_cooldown);
    WriteInt32(out, tuning.hang_wall_release_cooldown);
    WriteBoolByte(out, tuning.auto_ledge_grab);
}

bool ReadPlayerTuningState(std::istream& in, PlayerTuningState& tuning) {
    return ReadFloat(in, tuning.gravity_scale) &&
           ReadFloat(in, tuning.max_fall_speed) &&
           ReadFloat(in, tuning.jump_impulse) &&
           ReadFloat(in, tuning.spring_shoes_jump_impulse_bonus) &&
           ReadInt32(in, tuning.jump_hold_frames) &&
           ReadInt32(in, tuning.coyote_frames) &&
           ReadInt32(in, tuning.jump_delay_frames) &&
           ReadInt32(in, tuning.fall_damage_light_frames) &&
           ReadInt32(in, tuning.fall_damage_medium_frames) &&
           ReadInt32(in, tuning.fall_damage_heavy_frames) &&
           ReadFloat(in, tuning.walk_speed) &&
           ReadFloat(in, tuning.run_speed) &&
           ReadFloat(in, tuning.move_acc) &&
           ReadFloat(in, tuning.run_acc) &&
           ReadFloat(in, tuning.ground_friction_scale) &&
           ReadFloat(in, tuning.air_friction) &&
           ReadFloat(in, tuning.climb_speed) &&
           ReadFloat(in, tuning.climb_depart_horizontal_speed) &&
           ReadFloat(in, tuning.climb_probe_bias_pixels) &&
           ReadFloat(in, tuning.climb_probe_x_scale) &&
           ReadInt32(in, tuning.climb_required_probe_hits) &&
           ReadInt32(in, tuning.climb_detach_cooldown) &&
           ReadInt32(in, tuning.hang_drop_cooldown) &&
           ReadInt32(in, tuning.glove_hang_drop_cooldown) &&
           ReadInt32(in, tuning.hang_wall_release_cooldown) &&
           ReadBoolByte(in, tuning.auto_ledge_grab);
}

void WriteSettings(std::ostream& out, const Settings& settings) {
    WriteSettingsMode(out, settings.mode);
    WriteVideoSettings(out, settings.video);
    WriteAudioSettings(out, settings.audio);
    WriteControlsSettings(out, settings.controls);
    WriteUiSettings(out, settings.ui);
    WritePostProcessSettings(out, settings.post_process);
    WriteFluidSettings(out, settings.fluid);
    WriteWaterEffectSettings(out, settings.water_effect);
    WriteDebugUiSettings(out, settings.debug_ui);
    WritePlayerTuningState(out, settings.player_tuning);
}

bool ReadSettings(std::istream& in, Settings& settings) {
    return ReadSettingsMode(in, settings.mode) &&
           ReadVideoSettings(in, settings.video) &&
           ReadAudioSettings(in, settings.audio) &&
           ReadControlsSettings(in, settings.controls) &&
           ReadUiSettings(in, settings.ui) &&
           ReadPostProcessSettings(in, settings.post_process) &&
           ReadFluidSettings(in, settings.fluid) &&
           ReadWaterEffectSettings(in, settings.water_effect) &&
           ReadDebugUiSettings(in, settings.debug_ui) &&
           ReadPlayerTuningState(in, settings.player_tuning);
}

void WriteTile(std::ostream& out, Tile tile);
bool ReadTile(std::istream& in, Tile& tile);

void WriteDebugOverlayState(std::ostream& out, const DebugOverlayState& overlay) {
    WriteBoolByte(out, overlay.show_ent_collision_boxes);
    WriteBoolByte(out, overlay.show_ent_ids);
    WriteBoolByte(out, overlay.show_ent_types);
    WriteBoolByte(out, overlay.show_ent_render_centers);
    WriteBoolByte(out, overlay.show_void_death_line);
    WriteBoolByte(out, overlay.show_chunk_boundaries);
    WriteBoolByte(out, overlay.show_chunk_coords);
    WriteBoolByte(out, overlay.show_tile_indexes);
    WriteBoolByte(out, overlay.show_tile_types);
    WriteBoolByte(out, overlay.show_tile_openness);
    WriteBoolByte(out, overlay.show_fluid_amounts);
    WriteBoolByte(out, overlay.show_fluid_gravity);
    WriteBoolByte(out, overlay.show_lights);
    WriteBoolByte(out, overlay.show_area_boundaries);
    WriteBoolByte(out, overlay.show_area_ids);
    WriteBoolByte(out, overlay.show_area_types);
    WriteBoolByte(out, overlay.show_audio_emitters);
    WriteBoolByte(out, overlay.show_audio_occlusion_paths);
    WriteBoolByte(out, overlay.show_debug_annotations);
    WriteBoolByte(out, overlay.show_stagegen_annotations);
}

bool ReadDebugOverlayState(std::istream& in, DebugOverlayState& overlay) {
    return ReadBoolByte(in, overlay.show_ent_collision_boxes) &&
           ReadBoolByte(in, overlay.show_ent_ids) &&
           ReadBoolByte(in, overlay.show_ent_types) &&
           ReadBoolByte(in, overlay.show_ent_render_centers) &&
           ReadBoolByte(in, overlay.show_void_death_line) &&
           ReadBoolByte(in, overlay.show_chunk_boundaries) &&
           ReadBoolByte(in, overlay.show_chunk_coords) &&
           ReadBoolByte(in, overlay.show_tile_indexes) &&
           ReadBoolByte(in, overlay.show_tile_types) &&
           ReadBoolByte(in, overlay.show_tile_openness) &&
           ReadBoolByte(in, overlay.show_fluid_amounts) &&
           ReadBoolByte(in, overlay.show_fluid_gravity) &&
           ReadBoolByte(in, overlay.show_lights) &&
           ReadBoolByte(in, overlay.show_area_boundaries) &&
           ReadBoolByte(in, overlay.show_area_ids) &&
           ReadBoolByte(in, overlay.show_area_types) &&
           ReadBoolByte(in, overlay.show_audio_emitters) &&
           ReadBoolByte(in, overlay.show_audio_occlusion_paths) &&
           ReadBoolByte(in, overlay.show_debug_annotations) &&
           ReadBoolByte(in, overlay.show_stagegen_annotations);
}

void WriteDebugShakeBrushState(std::ostream& out, const DebugShakeBrushState& brush) {
    WriteBoolByte(out, brush.enabled);
    WriteBoolByte(out, brush.affect_foreground_tiles);
    WriteBoolByte(out, brush.affect_background_tiles);
    WriteBoolByte(out, brush.affect_ents);
    WriteFloat(out, brush.foreground_tile_amount);
    WriteFloat(out, brush.background_tile_amount);
    WriteFloat(out, brush.ent_amount);
    WriteFloat(out, brush.radius_tiles);
}

bool ReadDebugShakeBrushState(std::istream& in, DebugShakeBrushState& brush) {
    return ReadBoolByte(in, brush.enabled) &&
           ReadBoolByte(in, brush.affect_foreground_tiles) &&
           ReadBoolByte(in, brush.affect_background_tiles) &&
           ReadBoolByte(in, brush.affect_ents) &&
           ReadFloat(in, brush.foreground_tile_amount) &&
           ReadFloat(in, brush.background_tile_amount) &&
           ReadFloat(in, brush.ent_amount) &&
           ReadFloat(in, brush.radius_tiles);
}

void WriteDebugAudioBrushState(std::ostream& out, const DebugAudioBrushState& brush) {
    WriteBoolByte(out, brush.enabled);
    WriteBoolByte(out, brush.show_openness_rays);
    WriteBoolByte(out, brush.show_occlusion_ray);
    WriteAudioAssetId(out, brush.audio_asset_id);
    WriteFloat(out, brush.volume_scale);
    WriteBoolByte(out, brush.source_active);
    WriteVec2(out, brush.source_world_pos);
}

bool ReadDebugAudioBrushState(std::istream& in, DebugAudioBrushState& brush) {
    return ReadBoolByte(in, brush.enabled) &&
           ReadBoolByte(in, brush.show_openness_rays) &&
           ReadBoolByte(in, brush.show_occlusion_ray) &&
           ReadAudioAssetId(in, brush.audio_asset_id) &&
           ReadFloat(in, brush.volume_scale) &&
           ReadBoolByte(in, brush.source_active) &&
           ReadVec2(in, brush.source_world_pos);
}

void WriteDebugFluidBrushState(std::ostream& out, const DebugFluidBrushState& brush) {
    WriteBoolByte(out, brush.enabled);
    WriteBoolByte(out, brush.replace_solid_tiles);
    WriteDebugFluidBrushMode(out, brush.mode);
    WriteInt32(out, brush.radius_tiles);
    WriteFloat(out, brush.paint_gravity_x);
    WriteFloat(out, brush.paint_gravity_y);
    WriteBoolByte(out, brush.show_flow_indicators);
}

bool ReadDebugFluidBrushState(std::istream& in, DebugFluidBrushState& brush) {
    return ReadBoolByte(in, brush.enabled) &&
           ReadBoolByte(in, brush.replace_solid_tiles) &&
           ReadDebugFluidBrushMode(in, brush.mode) &&
           ReadInt32(in, brush.radius_tiles) &&
           ReadFloat(in, brush.paint_gravity_x) &&
           ReadFloat(in, brush.paint_gravity_y) &&
           ReadBoolByte(in, brush.show_flow_indicators);
}

void WriteStageRotationState(std::ostream& out, const StageRotationState& rotation) {
    WriteBoolByte(out, rotation.active);
    WriteInt32(out, rotation.elapsed_frames);
    WriteInt32(out, rotation.duration_frames);
    WriteInt32(out, rotation.quarter_turns);
    WriteVec2(out, rotation.pivot);
    WriteStageRotationWrapPolicy(out, rotation.wrap_policy);
}

bool ReadStageRotationState(std::istream& in, StageRotationState& rotation) {
    return ReadBoolByte(in, rotation.active) &&
           ReadInt32(in, rotation.elapsed_frames) &&
           ReadInt32(in, rotation.duration_frames) &&
           ReadInt32(in, rotation.quarter_turns) &&
           ReadVec2(in, rotation.pivot) &&
           ReadStageRotationWrapPolicy(in, rotation.wrap_policy);
}

void WriteClassicQuestState(std::ostream& out, const ClassicQuestState& quest) {
    WriteBoolByte(out, quest.made_black_market);
    WriteBoolByte(out, quest.made_udjat_eye);
    WriteBoolByte(out, quest.has_udjat_eye);
    WriteBoolByte(out, quest.made_moai);
    WriteBoolByte(out, quest.has_hedjet);
    WriteBoolByte(out, quest.has_sceptre);
    WriteBoolByte(out, quest.has_book_of_dead);
}

bool ReadClassicQuestState(std::istream& in, ClassicQuestState& quest) {
    return ReadBoolByte(in, quest.made_black_market) &&
           ReadBoolByte(in, quest.made_udjat_eye) &&
           ReadBoolByte(in, quest.has_udjat_eye) &&
           ReadBoolByte(in, quest.made_moai) &&
           ReadBoolByte(in, quest.has_hedjet) &&
           ReadBoolByte(in, quest.has_sceptre) &&
           ReadBoolByte(in, quest.has_book_of_dead);
}

void WriteQuestState(std::ostream& out, const QuestState& quest) {
    WriteQuestId(out, quest.quest_id);
    WriteClassicQuestState(out, quest.classic);
}

bool ReadQuestState(std::istream& in, QuestState& quest) {
    return ReadQuestId(in, quest.quest_id) &&
           ReadClassicQuestState(in, quest.classic);
}

void WriteDebugLevelConfig(std::ostream& out, const DebugLevelConfig& debug_level) {
    WriteDebugLevelKind(out, debug_level.kind);
    WriteInt32(out, debug_level.hang_test.drop_tiles);
    WriteTile(out, debug_level.border_test.left_tile);
    WriteTile(out, debug_level.border_test.right_tile);
    WriteTile(out, debug_level.border_test.top_tile);
    WriteTile(out, debug_level.border_test.bottom_tile);
    WriteBoolByte(out, debug_level.border_test.wrap_x);
    WriteBoolByte(out, debug_level.border_test.wrap_y);
    WriteInt32(out, debug_level.border_test.wrap_padding_tiles);
    WriteBoolByte(out, debug_level.border_test.camera_clamp_enabled);
    WriteOptionalInt32(out, debug_level.border_test.void_death_y);
    WriteInt32(out, static_cast<int>(debug_level.maze_door_test.room));
    WriteInt32(out, debug_level.crusher_trap_test.stress_squisher_count);
    WriteInt32(out, debug_level.crusher_trap_test.squisher_sensor_tiles);
    WriteInt32(out, debug_level.lighting_stress_test.moving_light_count);
}

bool ReadDebugLevelConfig(std::istream& in, DebugLevelConfig& debug_level) {
    int maze_room = 0;
    if (!ReadDebugLevelKind(in, debug_level.kind) ||
        !ReadInt32(in, debug_level.hang_test.drop_tiles) ||
        !ReadTile(in, debug_level.border_test.left_tile) ||
        !ReadTile(in, debug_level.border_test.right_tile) ||
        !ReadTile(in, debug_level.border_test.top_tile) ||
        !ReadTile(in, debug_level.border_test.bottom_tile) ||
        !ReadBoolByte(in, debug_level.border_test.wrap_x) ||
        !ReadBoolByte(in, debug_level.border_test.wrap_y) ||
        !ReadInt32(in, debug_level.border_test.wrap_padding_tiles) ||
        !ReadBoolByte(in, debug_level.border_test.camera_clamp_enabled) ||
        !ReadOptionalInt32(in, debug_level.border_test.void_death_y) ||
        !ReadInt32(in, maze_room) ||
        !ReadInt32(in, debug_level.crusher_trap_test.stress_squisher_count) ||
        !ReadInt32(in, debug_level.crusher_trap_test.squisher_sensor_tiles) ||
        !ReadInt32(in, debug_level.lighting_stress_test.moving_light_count)) {
        return false;
    }
    if (maze_room < 0 || maze_room > static_cast<int>(MazeDoorTestRoom::RoomC)) {
        return false;
    }
    debug_level.maze_door_test.room = static_cast<MazeDoorTestRoom>(maze_room);
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
    WriteUint32(out, count);
    for (const StageExitRequirement& requirement : target.requirements) {
        WriteStageExitRequirement(out, requirement);
    }
}

bool ReadStageExitTarget(std::istream& in, StageExitTarget& target) {
    if (!ReadString(in, target.target_stage_id)) {
        return false;
    }
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteUint32(out, spawn.anim_id);
    WriteOptionalUint32(out, spawn.ent_a_spawn_index);
    WriteOptionalUint32(out, spawn.ent_b_spawn_index);
    WriteOptionalUint32(out, spawn.ent_c_spawn_index);
    WriteOptionalUint32(out, spawn.ent_d_spawn_index);
    WriteOptionalUint32(out, spawn.shop_owner_spawn_index);
    WriteBoolByte(out, spawn.buyable);
    WriteUint32(out, spawn.buy_price);
    WriteString(out, spawn.exit_id);
}

bool ReadEntSpawn(std::istream& in, EntSpawn& spawn) {
    return ReadEntType(in, spawn.type_) &&
           ReadVec2(in, spawn.pos) &&
           ReadOptionalVec2(in, spawn.size_override) &&
           ReadEnumByte(in, spawn.facing, Side::Right) &&
           ReadOptionalEnumByte(in, spawn.ai_state_override, EntAiState::Returning) &&
           ReadUint32(in, spawn.anim_id) &&
           ReadOptionalUint32(in, spawn.ent_a_spawn_index) &&
           ReadOptionalUint32(in, spawn.ent_b_spawn_index) &&
           ReadOptionalUint32(in, spawn.ent_c_spawn_index) &&
           ReadOptionalUint32(in, spawn.ent_d_spawn_index) &&
           ReadOptionalUint32(in, spawn.shop_owner_spawn_index) &&
           ReadBoolByte(in, spawn.buyable) &&
           ReadUint32(in, spawn.buy_price) &&
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
    WriteInt32(out, light.radius);
}

bool ReadStageLight(std::istream& in, StageLight& light) {
    return ReadVid(in, light.vid) &&
           ReadIVec2(in, light.tile_pos) &&
           ReadInt32(in, light.radius);
}

void WriteTile(std::ostream& out, Tile tile) {
    const std::uint16_t stored = static_cast<std::uint16_t>(tile);
    WriteUint16(out, stored);
}

bool ReadTile(std::istream& in, Tile& tile) {
    std::uint16_t stored = 0;
    if (!ReadUint16(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint16_t>(Tile::Exit)) {
        return false;
    }
    tile = static_cast<Tile>(stored);
    return true;
}

void WriteTileRotation(std::ostream& out, TileRotation rotation) {
    WriteUint8(out, rotation);
}

bool ReadTileRotation(std::istream& in, TileRotation& rotation) {
    if (!ReadUint8(in, rotation)) {
        return false;
    }
    return (rotation & ~kTileRotationMask) == 0;
}

template <typename T, typename WriteOne>
void WriteGridExplicit(std::ostream& out, const std::vector<std::vector<T>>& grid, WriteOne write_one) {
    const std::uint32_t rows = static_cast<std::uint32_t>(grid.size());
    WriteUint32(out, rows);
    for (const std::vector<T>& row : grid) {
        const std::uint32_t count = static_cast<std::uint32_t>(row.size());
        WriteUint32(out, count);
        for (const T& value : row) {
            write_one(out, value);
        }
    }
}

template <typename T, typename ReadOne>
bool ReadGridExplicit(std::istream& in, std::vector<std::vector<T>>& grid, ReadOne read_one) {
    std::uint32_t rows = 0;
    if (!ReadUint32(in, rows)) {
        return false;
    }
    grid.resize(rows);
    for (std::vector<T>& row : grid) {
        std::uint32_t count = 0;
        if (!ReadUint32(in, count)) {
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
    WriteUint32(out, count);
    for (Tile tile : tiles) {
        WriteTile(out, tile);
    }
}

bool ReadTileVector(std::istream& in, std::vector<Tile>& tiles) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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

void WriteIVec2Vector(std::ostream& out, const std::vector<IVec2>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WriteUint32(out, count);
    for (const IVec2& value : values) {
        WriteIVec2(out, value);
    }
}

bool ReadIVec2Vector(std::istream& in, std::vector<IVec2>& values) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    values.resize(count);
    for (IVec2& value : values) {
        if (!ReadIVec2(in, value)) {
            return false;
        }
    }
    return true;
}

void WriteEmbeddedTreasureDrop(std::ostream& out, const EmbeddedTreasureDrop& drop) {
    WriteEntType(out, drop.type_);
    WriteInt32(out, drop.count);
}

bool ReadEmbeddedTreasureDrop(std::istream& in, EmbeddedTreasureDrop& drop) {
    return ReadEntType(in, drop.type_) &&
           ReadInt32(in, drop.count);
}

void WriteEmbeddedTreasure(std::ostream& out, const EmbeddedTreasure& treasure) {
    WriteEnumByte(out, treasure.visibility);
    WriteUint32(out, treasure.overlay_frame);
    WriteAudioAssetId(out, treasure.break_sound);
    for (const EmbeddedTreasureDrop& drop : treasure.drops) {
        WriteEmbeddedTreasureDrop(out, drop);
    }
}

bool ReadEmbeddedTreasure(std::istream& in, EmbeddedTreasure& treasure) {
    if (!ReadEnumByte(in, treasure.visibility, EmbeddedTreasureVisibility::Visible) ||
        !ReadUint32(in, treasure.overlay_frame) ||
        !ReadAudioAssetId(in, treasure.break_sound)) {
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
    WriteUint32(out, stamp.anim_id);
    WriteVec2(out, stamp.pos);
    WriteEnumByte(out, stamp.condition);
}

bool ReadBackgroundStamp(std::istream& in, BackgroundStamp& stamp) {
    return ReadUint32(in, stamp.anim_id) &&
           ReadVec2(in, stamp.pos) &&
           ReadEnumByte(in, stamp.condition, BackgroundStampCondition::Wanted);
}

void WriteBackgroundStamps(std::ostream& out, const std::vector<BackgroundStamp>& stamps) {
    const std::uint32_t count = static_cast<std::uint32_t>(stamps.size());
    WriteUint32(out, count);
    for (const BackgroundStamp& stamp : stamps) {
        WriteBackgroundStamp(out, stamp);
    }
}

bool ReadBackgroundStamps(std::istream& in, std::vector<BackgroundStamp>& stamps) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteUint32(out, count);
    for (const StageLight& light : lights) {
        WriteStageLight(out, light);
    }
}

bool ReadStageLights(std::istream& in, std::vector<StageLight>& lights) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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

void WriteStage(std::ostream& out, const Stage& stage) {
    WriteStageType(out, stage.stage_type);
    WriteString(out, stage.quest_id);
    WriteString(out, stage.quest_stage_id);
    WriteString(out, stage.route_label);
    WriteString(out, stage.stage_title);
    WriteInt32(out, stage.quest_level_number);
    WriteOptionalUint32(out, stage.generation_seed);
    const std::uint32_t exit_count = static_cast<std::uint32_t>(stage.exits.size());
    WriteUint32(out, exit_count);
    for (const StageExit& exit : stage.exits) {
        WriteStageExit(out, exit);
    }
    WriteFloat(out, stage.gravity);
    WriteTile(out, stage.border.left.tile);
    WriteTile(out, stage.border.right.tile);
    WriteTile(out, stage.border.top.tile);
    WriteTile(out, stage.border.bottom.tile);
    WriteBoolByte(out, stage.border.wrap_x);
    WriteBoolByte(out, stage.border.wrap_y);
    WriteOptionalInt32(out, stage.border.void_death_y);
    WriteBoolByte(out, stage.camera_clamp_enabled);
    WriteVec2(out, stage.camera_clamp_margin);
    WriteBoolByte(out, stage.wrap_transform_active);
    WriteUint32(out, stage.wrap_padding_tiles);
    WriteUVec2(out, stage.wrap_core_origin_tiles);
    WriteUVec2(out, stage.wrap_core_size_tiles);
    WriteGridExplicit(out, stage.tiles, WriteTile);
    WriteGridExplicit(out, stage.tile_rotations, WriteTileRotation);
    WriteGridExplicit(out, stage.fluid_tiles, WriteTile);
    WriteGridExplicit(out, stage.fluid_amount, WriteFloat);
    WriteGridExplicit(out, stage.fluid_display_amount, WriteFloat);
    WriteGridExplicit(out, stage.fluid_velocity, WriteVec2);
    WriteGridExplicit(out, stage.fluid_gravity, WriteVec2);
    WriteGridExplicit(out, stage.fluid_gravity_strength, WriteFloat);
    WriteGridExplicit(out, stage.fluid_temp_gravity, WriteVec2);
    WriteGridExplicit(out, stage.tile_shake, WriteFloat);
    WriteGridExplicit(out, stage.backwall_tile_shake, WriteFloat);
    WriteGridExplicit(out, stage.backwall_tiles, WriteTile);
    WriteTileVector(out, stage.backwall_fill_tiles);
    WriteGridExplicit(out, stage.embedded_treasures, WriteEmbeddedTreasure);
    WriteGridExplicit(out, stage.rooms, WriteInt32);
    WriteIVec2Vector(out, stage.path);
    const std::uint32_t spawn_count = static_cast<std::uint32_t>(stage.ent_spawns.size());
    WriteUint32(out, spawn_count);
    for (const EntSpawn& spawn : stage.ent_spawns) {
        WriteEntSpawn(out, spawn);
    }
    WriteBackgroundStamps(out, stage.background_stamps);
    const std::uint32_t annotation_count =
        static_cast<std::uint32_t>(stage.stagegen_annotations.size());
    WriteUint32(out, annotation_count);
    for (const StageGenAnnotation& annotation : stage.stagegen_annotations) {
        WriteStageGenAnnotation(out, annotation);
    }
    WriteStageLights(out, stage.lights);
    WriteUint32(out, stage.block_anim_id);
    WriteUint32(out, stage.next_light_vid);
    WriteUint32(out, stage.tile_change_generation);
}

bool ReadStage(std::istream& in, Stage& stage) {
    if (!ReadStageType(in, stage.stage_type) ||
        !ReadString(in, stage.quest_id) ||
        !ReadString(in, stage.quest_stage_id) ||
        !ReadString(in, stage.route_label) ||
        !ReadString(in, stage.stage_title) ||
        !ReadInt32(in, stage.quest_level_number) ||
        !ReadOptionalUint32(in, stage.generation_seed)) {
        return false;
    }

    std::uint32_t exit_count = 0;
    if (!ReadUint32(in, exit_count)) {
        return false;
    }
    stage.exits.resize(exit_count);
    for (StageExit& exit : stage.exits) {
        if (!ReadStageExit(in, exit)) {
            return false;
        }
    }

    if (!ReadFloat(in, stage.gravity) ||
        !ReadTile(in, stage.border.left.tile) ||
        !ReadTile(in, stage.border.right.tile) ||
        !ReadTile(in, stage.border.top.tile) ||
        !ReadTile(in, stage.border.bottom.tile) ||
        !ReadBoolByte(in, stage.border.wrap_x) ||
        !ReadBoolByte(in, stage.border.wrap_y) ||
        !ReadOptionalInt32(in, stage.border.void_death_y) ||
        !ReadBoolByte(in, stage.camera_clamp_enabled) ||
        !ReadVec2(in, stage.camera_clamp_margin) ||
        !ReadBoolByte(in, stage.wrap_transform_active) ||
        !ReadUint32(in, stage.wrap_padding_tiles) ||
        !ReadUVec2(in, stage.wrap_core_origin_tiles) ||
        !ReadUVec2(in, stage.wrap_core_size_tiles)) {
        return false;
    }

    if (!ReadGridExplicit(in, stage.tiles, ReadTile) ||
        !ReadGridExplicit(in, stage.tile_rotations, ReadTileRotation) ||
        !ReadGridExplicit(in, stage.fluid_tiles, ReadTile) ||
        !ReadGridExplicit(in, stage.fluid_amount, ReadFloat) ||
        !ReadGridExplicit(in, stage.fluid_display_amount, ReadFloat) ||
        !ReadGridExplicit(in, stage.fluid_velocity, ReadVec2) ||
        !ReadGridExplicit(in, stage.fluid_gravity, ReadVec2) ||
        !ReadGridExplicit(in, stage.fluid_gravity_strength, ReadFloat) ||
        !ReadGridExplicit(in, stage.fluid_temp_gravity, ReadVec2) ||
        !ReadGridExplicit(in, stage.tile_shake, ReadFloat) ||
        !ReadGridExplicit(in, stage.backwall_tile_shake, ReadFloat) ||
        !ReadGridExplicit(in, stage.backwall_tiles, ReadTile) ||
        !ReadTileVector(in, stage.backwall_fill_tiles) ||
        !ReadGridExplicit(in, stage.embedded_treasures, ReadEmbeddedTreasure) ||
        !ReadGridExplicit(in, stage.rooms, ReadInt32) ||
        !ReadIVec2Vector(in, stage.path)) {
        return false;
    }

    std::uint32_t spawn_count = 0;
    if (!ReadUint32(in, spawn_count)) {
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
    if (!ReadUint32(in, annotation_count)) {
        return false;
    }
    stage.stagegen_annotations.resize(annotation_count);
    for (StageGenAnnotation& annotation : stage.stagegen_annotations) {
        if (!ReadStageGenAnnotation(in, annotation)) {
            return false;
        }
    }

    return ReadStageLights(in, stage.lights) &&
           ReadUint32(in, stage.block_anim_id) &&
           ReadUint32(in, stage.next_light_vid) &&
           ReadUint32(in, stage.tile_change_generation);
}

void WriteEntPool(std::ostream& out, const EntPool& ents) {
    const std::uint32_t ent_count = static_cast<std::uint32_t>(ents.ents.size());
    WriteUint32(out, ent_count);
    for (const Ent& ent : ents.ents) {
        WriteEnt(out, ent);
    }
    WriteUint32Vector(out, ents.available_ids);
}

bool ReadEntPool(std::istream& in, EntPool& ents) {
    std::uint32_t ent_count = 0;
    if (!ReadUint32(in, ent_count)) {
        return false;
    }

    ents.ents.resize(ent_count);
    for (std::uint32_t i = 0; i < ent_count; ++i) {
        if (!ReadEnt(in, ents.ents[i])) {
            return false;
        }
    }

    return ReadUint32Vector(in, ents.available_ids);
}

void WritePlayerRegistry(std::ostream& out, const PlayerRegistry& players) {
    const std::uint32_t count = static_cast<std::uint32_t>(players.slots.size());
    WriteUint32(out, count);
    for (const PlayerSlot& slot : players.slots) {
        WritePlayerId(out, slot.player_id);
        WriteOptionalVid(out, slot.ent_vid);
        WritePlayerConnectionKind(out, slot.connection_kind);
        WriteBoolByte(out, slot.connected);
        WriteBoolByte(out, slot.primary_local);
        WriteString(out, slot.display_name);
        WriteInputFrame(out, slot.input_frame);
        WriteInputFrame(out, slot.previous_input_frame);
        WritePlayingInputs(out, slot.inputs);
        WritePlayingInputs(out, slot.immediate_inputs);
    }
}

bool ReadPlayerRegistry(std::istream& in, PlayerRegistry& players) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    players.slots.resize(count);
    for (PlayerSlot& slot : players.slots) {
        if (!ReadPlayerId(in, slot.player_id) ||
            !ReadOptionalVid(in, slot.ent_vid) ||
            !ReadPlayerConnectionKind(in, slot.connection_kind) ||
            !ReadBoolByte(in, slot.connected) ||
            !ReadBoolByte(in, slot.primary_local) ||
            !ReadString(in, slot.display_name) ||
            !ReadInputFrame(in, slot.input_frame) ||
            !ReadInputFrame(in, slot.previous_input_frame) ||
            !ReadPlayingInputs(in, slot.inputs) ||
            !ReadPlayingInputs(in, slot.immediate_inputs)) {
            return false;
        }
    }
    return true;
}

void WriteContactBookkeeping(std::ostream& out, const ContactBookkeeping& contact) {
    const auto write_contact_cooldowns = [&](const std::vector<ContactCooldownEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WriteUint32(out, count);
        for (const ContactCooldownEntry& entry : entries) {
            WriteVid(out, entry.source_vid);
            WriteVid(out, entry.target_vid);
            WriteUint32(out, entry.expires_on_stage_frame);
        }
    };
    const auto write_interaction_cooldowns =
        [&](const std::vector<InteractionCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WriteUint32(out, count);
            for (const InteractionCooldownEntry& entry : entries) {
                WriteVid(out, entry.source_vid);
                WriteVid(out, entry.target_vid);
                WriteUint8(out, static_cast<std::uint8_t>(entry.kind));
                WriteUint32(out, entry.expires_on_stage_frame);
            }
        };
    const auto write_ent_dispatches = [&](const std::vector<EntContactDispatchEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WriteUint32(out, count);
        for (const EntContactDispatchEntry& entry : entries) {
            WriteVid(out, entry.first_vid);
            WriteVid(out, entry.second_vid);
        }
    };
    const auto write_proj_body_cooldowns =
        [&](const std::vector<ProjBodyImpactCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WriteUint32(out, count);
            for (const ProjBodyImpactCooldownEntry& entry : entries) {
                WriteVid(out, entry.first_vid);
                WriteVid(out, entry.second_vid);
                WriteUint32(out, entry.expires_on_stage_frame);
            }
        };

    write_contact_cooldowns(contact.contact_cooldowns);
    write_interaction_cooldowns(contact.interaction_cooldowns);
    write_ent_dispatches(contact.ent_contact_dispatches_this_tick);
    write_proj_body_cooldowns(contact.proj_body_impact_cooldowns);
}

bool ReadContactBookkeeping(std::istream& in, ContactBookkeeping& contact) {
    const auto read_count = [&](std::uint32_t& count) {
        return ReadUint32(in, count);
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
                !ReadUint32(in, entry.expires_on_stage_frame)) {
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
                !ReadUint8(in, kind) ||
                !ReadUint32(in, entry.expires_on_stage_frame)) {
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
                !ReadUint32(in, entry.expires_on_stage_frame)) {
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
    WriteUint8(out, kind);
    WriteUint16(out, slot.count);
    WriteUint16(out, slot.cooldown);
    WriteBoolByte(out, slot.active);
}

bool ReadToolSlot(std::istream& in, ToolSlot& slot) {
    std::uint8_t kind = 0;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    bool active = false;
    if (!ReadUint8(in, kind) ||
        !ReadUint16(in, count) ||
        !ReadUint16(in, cooldown) ||
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
    WriteUint32(out, count);
    for (const EntToolState& state : states) {
        WriteEntToolState(out, state);
    }
}

bool ReadEntToolStates(std::istream& in, std::vector<EntToolState>& states) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteMenuInputs(out, snapshot.menu_inputs);
    WriteMenuInputSnapshot(out, snapshot.menu_input_snapshot);
    WriteMenuInputSnapshot(out, snapshot.previous_menu_input_snapshot);
    WriteMenuInputDebounceTimers(out, snapshot.menu_input_debounce_timers);
    WritePlayingInputs(out, snapshot.playing_inputs);
    WritePlayingInputs(out, snapshot.immediate_playing_inputs);
    WritePlayingInputSnapshot(out, snapshot.playing_input_snapshot);
    WritePlayingInputSnapshot(out, snapshot.previous_playing_input_snapshot);
    WritePlayingInputSnapshot(out, snapshot.previous_immediate_playing_input_snapshot);
    WriteTitleMenuOption(out, snapshot.title_menu_selection);
    WriteSettingsMenuOption(out, snapshot.settings_menu_selection);
    WriteVideoSettingsMenuOption(out, snapshot.video_settings_menu_selection);
    WriteUiSettingsMenuOption(out, snapshot.ui_settings_menu_selection);
    WritePostFxSettingsMenuOption(out, snapshot.post_fx_settings_menu_selection);
    WriteLightingSettingsMenuOption(out, snapshot.lighting_settings_menu_selection);
    WriteOptionalUint32Index(out, snapshot.video_settings_target_window_size_index);
    WriteOptionalUint32Index(out, snapshot.video_settings_target_resolution_index);
    WriteOptionalBoolByte(out, snapshot.video_settings_target_fullscreen);
    WriteBoolByte(out, snapshot.rebuild_render_texture);
    WriteBoolByte(out, snapshot.choosing_control_binding);
    WriteDebugOverlayState(out, snapshot.debug_overlay);
    WriteDebugShakeBrushState(out, snapshot.debug_shake_brush);
    WriteDebugAudioBrushState(out, snapshot.debug_audio_brush);
    WriteDebugFluidBrushState(out, snapshot.debug_fluid_brush);
    WriteStageRotationState(out, snapshot.stage_rotation);
    WritePlayerTuningState(out, snapshot.player_tuning);
    WriteDouble(out, snapshot.now);
    WriteFloat(out, snapshot.time_since_last_update);
    WriteUint32(out, snapshot.scene_frame);
    WriteUint32(out, snapshot.frame);
    WriteUint32(out, snapshot.stage_frame);
    WriteDetRng(out, snapshot.drng);
    WriteDetRng(out, snapshot.stagegen_drng);
    WriteMode(out, snapshot.menu_return_to);
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
    WriteStageLoadTarget(out, snapshot.respawn_target);
    WriteOptionalStageTransitionTarget(out, snapshot.pending_stage_transition);
    WriteMultiplayerRespawnMode(out, snapshot.multiplayer_respawn_mode);
    WriteUint32(out, snapshot.points);
    WriteUint32(out, snapshot.deaths);
    WriteUint32(out, snapshot.depth);
    WriteSigned32(out, snapshot.sac_altar_favor);
    WriteUint32(out, snapshot.sac_altar_reward_tier);
    WriteQuestState(out, snapshot.quest_state);
    WritePlayerRegistry(out, snapshot.players);
    WriteUint32(out, snapshot.frame_pause);
    WriteDebugLevelConfig(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteStage(out, snapshot.stage);
    WriteOptionalVid(out, snapshot.controlled_ent_vid);
    WriteOptionalPlayerId(out, snapshot.spectator_target_player_id);
    WriteOptionalVid(out, snapshot.mouse_trailer_vid);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
    WriteVec2(out, snapshot.play_cam_pos);
}

bool ReadSnapshot(std::istream& in, GameplaySnapshot& snapshot) {
    return ReadMode(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadMenuInputs(in, snapshot.menu_inputs) &&
           ReadMenuInputSnapshot(in, snapshot.menu_input_snapshot) &&
           ReadMenuInputSnapshot(in, snapshot.previous_menu_input_snapshot) &&
           ReadMenuInputDebounceTimers(in, snapshot.menu_input_debounce_timers) &&
           ReadPlayingInputs(in, snapshot.playing_inputs) &&
           ReadPlayingInputs(in, snapshot.immediate_playing_inputs) &&
           ReadPlayingInputSnapshot(in, snapshot.playing_input_snapshot) &&
           ReadPlayingInputSnapshot(in, snapshot.previous_playing_input_snapshot) &&
           ReadPlayingInputSnapshot(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadTitleMenuOption(in, snapshot.title_menu_selection) &&
           ReadSettingsMenuOption(in, snapshot.settings_menu_selection) &&
           ReadVideoSettingsMenuOption(in, snapshot.video_settings_menu_selection) &&
           ReadUiSettingsMenuOption(in, snapshot.ui_settings_menu_selection) &&
           ReadPostFxSettingsMenuOption(in, snapshot.post_fx_settings_menu_selection) &&
           ReadLightingSettingsMenuOption(in, snapshot.lighting_settings_menu_selection) &&
           ReadOptionalUint32Index(in, snapshot.video_settings_target_window_size_index) &&
           ReadOptionalUint32Index(in, snapshot.video_settings_target_resolution_index) &&
           ReadOptionalBoolByte(in, snapshot.video_settings_target_fullscreen) &&
           ReadBoolByte(in, snapshot.rebuild_render_texture) &&
           ReadBoolByte(in, snapshot.choosing_control_binding) &&
           ReadDebugOverlayState(in, snapshot.debug_overlay) &&
           ReadDebugShakeBrushState(in, snapshot.debug_shake_brush) &&
           ReadDebugAudioBrushState(in, snapshot.debug_audio_brush) &&
           ReadDebugFluidBrushState(in, snapshot.debug_fluid_brush) &&
           ReadStageRotationState(in, snapshot.stage_rotation) &&
           ReadPlayerTuningState(in, snapshot.player_tuning) &&
           ReadDouble(in, snapshot.now) &&
           ReadFloat(in, snapshot.time_since_last_update) &&
           ReadUint32(in, snapshot.scene_frame) &&
           ReadUint32(in, snapshot.frame) &&
           ReadUint32(in, snapshot.stage_frame) &&
           ReadDetRng(in, snapshot.drng) &&
           ReadDetRng(in, snapshot.stagegen_drng) &&
           ReadMode(in, snapshot.menu_return_to) &&
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
           ReadStageLoadTarget(in, snapshot.respawn_target) &&
           ReadOptionalStageTransitionTarget(in, snapshot.pending_stage_transition) &&
           ReadMultiplayerRespawnMode(in, snapshot.multiplayer_respawn_mode) &&
           ReadUint32(in, snapshot.points) &&
           ReadUint32(in, snapshot.deaths) &&
           ReadUint32(in, snapshot.depth) &&
           ReadSigned32(in, snapshot.sac_altar_favor) &&
           ReadUint32(in, snapshot.sac_altar_reward_tier) &&
           ReadQuestState(in, snapshot.quest_state) &&
           ReadPlayerRegistry(in, snapshot.players) &&
           ReadUint32(in, snapshot.frame_pause) &&
           ReadDebugLevelConfig(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadStage(in, snapshot.stage) &&
           ReadOptionalVid(in, snapshot.controlled_ent_vid) &&
           ReadOptionalPlayerId(in, snapshot.spectator_target_player_id) &&
           ReadOptionalVid(in, snapshot.mouse_trailer_vid) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
           ReadVec2(in, snapshot.play_cam_pos);
}

void WriteSimPlayerSlotSnapshot(std::ostream& out, const SimPlayerSlotSnapshot& slot) {
    WritePlayerId(out, slot.player_id);
    WriteOptionalVid(out, slot.ent_vid);
    WriteBoolByte(out, slot.connected);
    WriteString(out, slot.display_name);
    WriteInputFrame(out, slot.input_frame);
    WriteInputFrame(out, slot.previous_input_frame);
    WritePlayingInputs(out, slot.inputs);
    WritePlayingInputs(out, slot.immediate_inputs);
}

bool ReadSimPlayerSlotSnapshot(std::istream& in, SimPlayerSlotSnapshot& slot) {
    return ReadPlayerId(in, slot.player_id) &&
           ReadOptionalVid(in, slot.ent_vid) &&
           ReadBoolByte(in, slot.connected) &&
           ReadString(in, slot.display_name) &&
           ReadInputFrame(in, slot.input_frame) &&
           ReadInputFrame(in, slot.previous_input_frame) &&
           ReadPlayingInputs(in, slot.inputs) &&
           ReadPlayingInputs(in, slot.immediate_inputs);
}

void WriteSimPlayerSlots(std::ostream& out, const std::vector<SimPlayerSlotSnapshot>& slots) {
    const std::uint32_t count = static_cast<std::uint32_t>(slots.size());
    WriteUint32(out, count);
    for (const SimPlayerSlotSnapshot& slot : slots) {
        WriteSimPlayerSlotSnapshot(out, slot);
    }
}

bool ReadSimPlayerSlots(std::istream& in, std::vector<SimPlayerSlotSnapshot>& slots) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
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
    WriteUint32(out, count);
    for (const SimNetEntLinkSnapshot& link : links) {
        WriteNetEntId(out, link.net_id);
        WriteVid(out, link.local_vid);
        WriteBoolByte(out, link.has_input_owner);
        WritePlayerId(out, link.input_owner_player_id);
    }
}

bool ReadSimNetEntLinks(
    std::istream& in,
    std::vector<SimNetEntLinkSnapshot>& links
) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    links.resize(count);
    for (SimNetEntLinkSnapshot& link : links) {
        if (!ReadNetEntId(in, link.net_id) ||
            !ReadVid(in, link.local_vid) ||
            !ReadBoolByte(in, link.has_input_owner) ||
            !ReadPlayerId(in, link.input_owner_player_id)) {
            return false;
        }
    }
    return true;
}

void WriteSimNetEntIdAliases(
    std::ostream& out,
    const std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    const std::uint32_t count = static_cast<std::uint32_t>(aliases.size());
    WriteUint32(out, count);
    for (const SimNetEntIdAliasSnapshot& alias : aliases) {
        WriteNetEntId(out, alias.from_id);
        WriteNetEntId(out, alias.to_id);
    }
}

bool ReadSimNetEntIdAliases(
    std::istream& in,
    std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    std::uint32_t count = 0;
    if (!ReadUint32(in, count)) {
        return false;
    }
    aliases.resize(count);
    for (SimNetEntIdAliasSnapshot& alias : aliases) {
        if (!ReadNetEntId(in, alias.from_id) ||
            !ReadNetEntId(in, alias.to_id)) {
            return false;
        }
    }
    return true;
}

void WriteSimSnapshot(std::ostream& out, const SimSnapshot& snapshot) {
    WriteMode(out, snapshot.mode);
    WriteSettings(out, snapshot.settings);
    WritePlayingInputs(out, snapshot.playing_inputs);
    WritePlayingInputs(out, snapshot.immediate_playing_inputs);
    WritePlayingInputSnapshot(out, snapshot.playing_input_snapshot);
    WritePlayingInputSnapshot(out, snapshot.previous_playing_input_snapshot);
    WritePlayingInputSnapshot(out, snapshot.previous_immediate_playing_input_snapshot);
    WriteStageRotationState(out, snapshot.stage_rotation);
    WritePlayerTuningState(out, snapshot.player_tuning);
    WriteBoolByte(out, snapshot.running);
    WriteDouble(out, snapshot.now);
    WriteFloat(out, snapshot.time_since_last_update);
    WriteUint32(out, snapshot.scene_frame);
    WriteUint32(out, snapshot.frame);
    WriteUint32(out, snapshot.stage_frame);
    WriteDetRng(out, snapshot.drng);
    WriteDetRng(out, snapshot.stagegen_drng);
    WriteMode(out, snapshot.menu_return_to);
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
    WriteStageLoadTarget(out, snapshot.respawn_target);
    WriteOptionalStageTransitionTarget(out, snapshot.pending_stage_transition);
    WriteMultiplayerRespawnMode(out, snapshot.multiplayer_respawn_mode);
    WriteUint32(out, snapshot.points);
    WriteUint32(out, snapshot.deaths);
    WriteUint32(out, snapshot.depth);
    WriteSigned32(out, snapshot.sac_altar_favor);
    WriteUint32(out, snapshot.sac_altar_reward_tier);
    WriteVidVector(out, snapshot.interact_claimed_vids_this_frame);
    WriteQuestState(out, snapshot.quest_state);
    WriteSimPlayerSlots(out, snapshot.players);
    WriteUint32(out, snapshot.frame_pause);
    WriteDebugLevelConfig(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteVidVector(out, snapshot.area_listener_vids);
    WriteStage(out, snapshot.stage);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
    WriteNetEntId(out, snapshot.net_next_local_ent_id);
    WriteSimNetEntLinks(out, snapshot.net_ent_links);
    WriteSimNetEntIdAliases(out, snapshot.net_ent_id_aliases);
}

bool ReadSimSnapshot(std::istream& in, SimSnapshot& snapshot) {
    return ReadMode(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPlayingInputs(in, snapshot.playing_inputs) &&
           ReadPlayingInputs(in, snapshot.immediate_playing_inputs) &&
           ReadPlayingInputSnapshot(in, snapshot.playing_input_snapshot) &&
           ReadPlayingInputSnapshot(in, snapshot.previous_playing_input_snapshot) &&
           ReadPlayingInputSnapshot(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadStageRotationState(in, snapshot.stage_rotation) &&
           ReadPlayerTuningState(in, snapshot.player_tuning) &&
           ReadBoolByte(in, snapshot.running) &&
           ReadDouble(in, snapshot.now) &&
           ReadFloat(in, snapshot.time_since_last_update) &&
           ReadUint32(in, snapshot.scene_frame) &&
           ReadUint32(in, snapshot.frame) &&
           ReadUint32(in, snapshot.stage_frame) &&
           ReadDetRng(in, snapshot.drng) &&
           ReadDetRng(in, snapshot.stagegen_drng) &&
           ReadMode(in, snapshot.menu_return_to) &&
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
           ReadStageLoadTarget(in, snapshot.respawn_target) &&
           ReadOptionalStageTransitionTarget(in, snapshot.pending_stage_transition) &&
           ReadMultiplayerRespawnMode(in, snapshot.multiplayer_respawn_mode) &&
           ReadUint32(in, snapshot.points) &&
           ReadUint32(in, snapshot.deaths) &&
           ReadUint32(in, snapshot.depth) &&
           ReadSigned32(in, snapshot.sac_altar_favor) &&
           ReadUint32(in, snapshot.sac_altar_reward_tier) &&
           ReadVidVector(in, snapshot.interact_claimed_vids_this_frame) &&
           ReadQuestState(in, snapshot.quest_state) &&
           ReadSimPlayerSlots(in, snapshot.players) &&
           ReadUint32(in, snapshot.frame_pause) &&
           ReadDebugLevelConfig(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadVidVector(in, snapshot.area_listener_vids) &&
           ReadStage(in, snapshot.stage) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
           ReadNetEntId(in, snapshot.net_next_local_ent_id) &&
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

    WriteUint32(out, kRecordingMagic);
    WriteUint32(out, kRecordingVersion);
    const std::uint32_t count = static_cast<std::uint32_t>(debug.recorded_snapshots.size());
    WriteUint32(out, count);
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
    if (!ReadUint32(in, magic) || !ReadUint32(in, version) || !ReadUint32(in, count)) {
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
