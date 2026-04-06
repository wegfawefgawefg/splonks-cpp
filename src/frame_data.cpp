#include "frame_data.hpp"

#include <algorithm>
#include <stdexcept>

namespace splonks {

namespace {

FrameRect DefaultCboxFromSampleRect(const FrameRect& sample_rect) {
    return FrameRect{
        .x = 0,
        .y = 0,
        .w = sample_rect.w,
        .h = sample_rect.h,
    };
}

void ValidateRawFrameData(const RawFrameData& raw_frame_data) {
    if (raw_frame_data.path.empty()) {
        throw std::runtime_error("FrameData conversion error: frame path is empty");
    }
    if (raw_frame_data.name.empty()) {
        throw std::runtime_error("FrameData conversion error: frame name is empty");
    }
    if (raw_frame_data.frame < 0) {
        throw std::runtime_error(
            "FrameData conversion error: frame index is negative for " + raw_frame_data.name);
    }
    if (raw_frame_data.aabb.w <= 0 || raw_frame_data.aabb.h <= 0) {
        throw std::runtime_error(
            "FrameData conversion error: sample rect has non-positive size for " +
            raw_frame_data.name);
    }
}

FrameData ToFrameData(const RawFrameData& raw_frame_data) {
    ValidateRawFrameData(raw_frame_data);

    FrameData frame_data;
    frame_data.path = raw_frame_data.path;
    frame_data.sample_rect = raw_frame_data.aabb;
    frame_data.name = raw_frame_data.name;
    frame_data.frame = raw_frame_data.frame;
    frame_data.duration = raw_frame_data.duration > 0 ? raw_frame_data.duration : 1;
    frame_data.draw_offset = raw_frame_data.offset;
    frame_data.center = raw_frame_data.center;
    frame_data.tags = raw_frame_data.tags;
    frame_data.cbox =
        raw_frame_data.has_cbox ? raw_frame_data.cbox : DefaultCboxFromSampleRect(raw_frame_data.aabb);
    frame_data.tile = raw_frame_data.tile;
    return frame_data;
}

} // namespace

FrameDataDb FrameDataDb::FromRaw(const RawFrameDataFile& raw_file) {
    FrameDataDb database;
    database.frames.reserve(raw_file.sprites.size());

    for (const RawFrameData& raw_frame_data : raw_file.sprites) {
        database.frames.push_back(ToFrameData(raw_frame_data));
    }

    std::unordered_map<std::string, std::vector<std::size_t>> grouped_indices;
    grouped_indices.reserve(database.frames.size());

    for (std::size_t i = 0; i < database.frames.size(); ++i) {
        grouped_indices[database.frames[i].name].push_back(i);
    }

    database.animations.reserve(grouped_indices.size());
    for (auto& [name, frame_indices] : grouped_indices) {
        std::sort(
            frame_indices.begin(),
            frame_indices.end(),
            [&database](std::size_t left, std::size_t right) {
                return database.frames[left].frame < database.frames[right].frame;
            });

        for (std::size_t i = 1; i < frame_indices.size(); ++i) {
            const FrameData& previous = database.frames[frame_indices[i - 1]];
            const FrameData& current = database.frames[frame_indices[i]];
            if (previous.frame == current.frame) {
                throw std::runtime_error(
                    "FrameData conversion error: duplicate frame index " +
                    std::to_string(current.frame) + " for " + name);
            }
            if (previous.tile != current.tile) {
                throw std::runtime_error(
                    "FrameData conversion error: mixed tile flag values for " + name);
            }
        }

        FrameDataAnimation animation;
        animation.name = name;
        animation.tile = database.frames[frame_indices.front()].tile;
        animation.frame_indices = std::move(frame_indices);

        database.animation_indices_by_name[animation.name] = database.animations.size();
        database.animations.push_back(std::move(animation));
    }

    return database;
}

const FrameDataAnimation* FrameDataDb::FindAnimation(const std::string& name) const {
    const auto found = animation_indices_by_name.find(name);
    if (found == animation_indices_by_name.end()) {
        return nullptr;
    }
    return &animations[found->second];
}

const FrameData* FrameDataDb::FindFrame(const std::string& name, std::size_t ordered_frame_index) const {
    const FrameDataAnimation* const animation = FindAnimation(name);
    if (animation == nullptr || ordered_frame_index >= animation->frame_indices.size()) {
        return nullptr;
    }
    return &frames[animation->frame_indices[ordered_frame_index]];
}

} // namespace splonks
