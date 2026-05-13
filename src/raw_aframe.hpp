#pragma once

#include "math_types.hpp"

#include <string>
#include <vector>

namespace splonks {

struct FrameRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct RawAFrame {
    std::string source_yaml_path;
    int source_line = 0;
    std::string path;
    FrameRect aabb;
    std::string name;
    int frame = 0;
    int duration = 0;
    IVec2 offset = IVec2::New(0, 0);
    IVec2 center = IVec2::New(0, 0);
    IVec2 emit_point = IVec2::New(0, 0);
    std::vector<std::string> tags;
    FrameRect pbox;
    bool has_pbox = false;
    FrameRect cbox;
    bool has_cbox = false;
    bool tile = false;
};

struct RawAFrameFile {
    std::vector<RawAFrame> sprites;
};

RawAFrameFile LoadRawAFrameFile(const std::string& yaml_path);

} // namespace splonks
