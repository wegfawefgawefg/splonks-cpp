#pragma once

#include "aframe_id.hpp"
#include "math_types.hpp"
#include "raw_aframe.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace splonks {

struct AFrame {
    AFrameId anim_id = kInvalidAFrameId;
    std::uint32_t image_id = 0;
    std::string path;
    FrameRect sample_rect;
    std::string name;
    int frame = 0;
    int duration = 1;
    IVec2 draw_offset = IVec2::New(0, 0);
    IVec2 center = IVec2::New(0, 0);
    IVec2 emit_point = IVec2::New(0, 0);
    std::vector<std::string> tags;
    FrameRect pbox;
    FrameRect cbox;
    bool tile = false;
};

struct AFrameAnim {
    AFrameId anim_id = kInvalidAFrameId;
    std::string name;
    bool tile = false;
    std::vector<std::size_t> frame_indices;
};

struct AFrameDb {
    std::vector<std::string> image_paths;
    std::vector<AFrame> frames;
    std::vector<AFrameAnim> anims;
    std::unordered_map<std::string, std::size_t> anim_indices_by_name;
    std::unordered_map<AFrameId, std::size_t> anim_indices_by_id;

    static AFrameDb FromRaw(const RawAFrameFile& raw_file);
    const AFrameAnim* FindAnim(const std::string& name) const;
    const AFrameAnim* FindAnim(AFrameId anim_id) const;
    const AFrame* FindFrame(const std::string& name, std::size_t ordered_frame_index) const;
    const AFrame* FindFrame(AFrameId anim_id, std::size_t ordered_frame_index) const;
};

} // namespace splonks
