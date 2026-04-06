#pragma once

#include "math_types.hpp"
#include "raw_frame_data.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace splonks {

struct FrameData {
    std::string path;
    FrameRect sample_rect;
    std::string name;
    int frame = 0;
    int duration = 1;
    IVec2 draw_offset = IVec2::New(0, 0);
    IVec2 center = IVec2::New(0, 0);
    std::vector<std::string> tags;
    FrameRect cbox;
    bool tile = false;
};

struct FrameDataAnimation {
    std::string name;
    bool tile = false;
    std::vector<std::size_t> frame_indices;
};

struct FrameDataDb {
    std::vector<FrameData> frames;
    std::vector<FrameDataAnimation> animations;
    std::unordered_map<std::string, std::size_t> animation_indices_by_name;

    static FrameDataDb FromRaw(const RawFrameDataFile& raw_file);
    const FrameDataAnimation* FindAnimation(const std::string& name) const;
    const FrameData* FindFrame(const std::string& name, std::size_t ordered_frame_index) const;
};

} // namespace splonks
