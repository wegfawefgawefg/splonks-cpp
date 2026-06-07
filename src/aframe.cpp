#include "aframe.hpp"
#include "tile.hpp"

#include <algorithm>
#include <stdexcept>

namespace splonks {

namespace {

std::string FormatRawAFrameLocation(const RawAFrame& raw_aframe) {
    const int cell_x = raw_aframe.aabb.x / static_cast<int>(kTileSize);
    const int cell_y = raw_aframe.aabb.y / static_cast<int>(kTileSize);
    const int in_cell_x = raw_aframe.aabb.x % static_cast<int>(kTileSize);
    const int in_cell_y = raw_aframe.aabb.y % static_cast<int>(kTileSize);

    std::string details;
    if (!raw_aframe.path.empty()) {
        details += raw_aframe.path;
        details += " ";
    }
    details += "aabb(" + std::to_string(raw_aframe.aabb.x) + "," +
               std::to_string(raw_aframe.aabb.y) + "," +
               std::to_string(raw_aframe.aabb.w) + "," +
               std::to_string(raw_aframe.aabb.h) + ")";
    details += " cell(" + std::to_string(cell_x) + "," + std::to_string(cell_y) + ")";
    details += " offset(" + std::to_string(in_cell_x) + "," + std::to_string(in_cell_y) + ")";

    if (!raw_aframe.source_yaml_path.empty() && raw_aframe.source_line > 0) {
        return raw_aframe.source_yaml_path + ":" +
               std::to_string(raw_aframe.source_line) + " [" + details + "]";
    }
    if (raw_aframe.source_line > 0) {
        return "line " + std::to_string(raw_aframe.source_line) + " [" + details + "]";
    }
    return "unknown location [" + details + "]";
}

FrameRect DefaultBoxFromSampleRect(const FrameRect& sample_rect) {
    return FrameRect{
        .x = 0,
        .y = 0,
        .w = sample_rect.w,
        .h = sample_rect.h,
    };
}

void ValidateRawAFrame(const RawAFrame& raw_aframe) {
    if (raw_aframe.path.empty()) {
        throw std::runtime_error(
            "AFrame conversion error at " + FormatRawAFrameLocation(raw_aframe) +
            ": frame path is empty"
        );
    }
    if (raw_aframe.name.empty()) {
        throw std::runtime_error(
            "AFrame conversion error at " + FormatRawAFrameLocation(raw_aframe) +
            ": frame name is empty"
        );
    }
    if (raw_aframe.frame < 0) {
        throw std::runtime_error(
            "AFrame conversion error at " + FormatRawAFrameLocation(raw_aframe) +
            ": frame index is negative for " + raw_aframe.name
        );
    }
    if (raw_aframe.aabb.w <= 0 || raw_aframe.aabb.h <= 0) {
        throw std::runtime_error(
            "AFrame conversion error at " + FormatRawAFrameLocation(raw_aframe) +
            ": sample rect has non-positive size for " + raw_aframe.name
        );
    }
}

AFrame ToAFrame(const RawAFrame& raw_aframe) {
    ValidateRawAFrame(raw_aframe);

    AFrame aframe;
    aframe.anim_id = HashAFrameId(raw_aframe.name);
    aframe.path = raw_aframe.path;
    aframe.sample_rect = raw_aframe.aabb;
    aframe.name = raw_aframe.name;
    aframe.frame = raw_aframe.frame;
    aframe.duration = raw_aframe.duration > 0 ? raw_aframe.duration : 1;
    aframe.draw_offset = raw_aframe.offset;
    aframe.center = raw_aframe.center;
    aframe.emit_point = raw_aframe.emit_point;
    aframe.tags = raw_aframe.tags;
    aframe.pbox =
        raw_aframe.has_pbox ? raw_aframe.pbox : DefaultBoxFromSampleRect(raw_aframe.aabb);
    aframe.cbox = raw_aframe.has_cbox ? raw_aframe.cbox : aframe.pbox;
    aframe.tile = raw_aframe.tile;
    return aframe;
}

} // namespace

AFrameDb AFrameDb::FromRaw(const RawAFrameFile& raw_file) {
    AFrameDb database;
    database.frames.reserve(raw_file.sprites.size());
    std::unordered_map<std::string, std::uint32_t> image_id_by_path;
    image_id_by_path.reserve(raw_file.sprites.size());

    for (const RawAFrame& raw_aframe : raw_file.sprites) {
        AFrame aframe = ToAFrame(raw_aframe);
        const auto image_found = image_id_by_path.find(aframe.path);
        if (image_found != image_id_by_path.end()) {
            aframe.image_id = image_found->second;
        } else {
            aframe.image_id = static_cast<std::uint32_t>(database.image_paths.size());
            database.image_paths.push_back(aframe.path);
            image_id_by_path[aframe.path] = aframe.image_id;
        }
        database.frames.push_back(std::move(aframe));
    }

    std::unordered_map<std::string, std::vector<std::size_t>> grouped_indices;
    grouped_indices.reserve(database.frames.size());

    for (std::size_t i = 0; i < database.frames.size(); ++i) {
        grouped_indices[database.frames[i].name].push_back(i);
    }

    std::vector<std::string> anim_names;
    anim_names.reserve(grouped_indices.size());
    for (const auto& grouped : grouped_indices) {
        anim_names.push_back(grouped.first);
    }
    std::sort(anim_names.begin(), anim_names.end());

    database.anims.reserve(grouped_indices.size());
    for (const std::string& name : anim_names) {
        std::vector<std::size_t>& frame_indices = grouped_indices.at(name);
        std::sort(
            frame_indices.begin(),
            frame_indices.end(),
            [&database](std::size_t left, std::size_t right) {
                return database.frames[left].frame < database.frames[right].frame;
            });

        for (std::size_t i = 1; i < frame_indices.size(); ++i) {
            const AFrame& previous = database.frames[frame_indices[i - 1]];
            const AFrame& current = database.frames[frame_indices[i]];
            if (previous.frame == current.frame) {
                throw std::runtime_error(
                    "AFrame conversion error: duplicate frame index " +
                    std::to_string(current.frame) + " for " + name + " at " +
                    FormatRawAFrameLocation(raw_file.sprites[frame_indices[i - 1]]) + " and " +
                    FormatRawAFrameLocation(raw_file.sprites[frame_indices[i]])
                );
            }
            if (previous.tile != current.tile) {
                throw std::runtime_error(
                    "AFrame conversion error: mixed tile flag values for " + name + " at " +
                    FormatRawAFrameLocation(raw_file.sprites[frame_indices[i - 1]]) + " and " +
                    FormatRawAFrameLocation(raw_file.sprites[frame_indices[i]])
                );
            }
        }

        AFrameAnim anim;
        anim.anim_id = HashAFrameId(name);
        anim.name = name;
        anim.tile = database.frames[frame_indices.front()].tile;
        anim.frame_indices = std::move(frame_indices);

        database.anim_indices_by_name[anim.name] = database.anims.size();
        const auto anim_id_found = database.anim_indices_by_id.find(anim.anim_id);
        if (anim_id_found != database.anim_indices_by_id.end() &&
            database.anims[anim_id_found->second].name != anim.name) {
            throw std::runtime_error(
                "AFrame conversion error: anim id collision for " + name
            );
        }
        database.anim_indices_by_id[anim.anim_id] = database.anims.size();
        database.anims.push_back(std::move(anim));
    }

    return database;
}

const AFrameAnim* AFrameDb::FindAnim(const std::string& name) const {
    const auto found = anim_indices_by_name.find(name);
    if (found == anim_indices_by_name.end()) {
        return nullptr;
    }
    return &anims[found->second];
}

const AFrameAnim* AFrameDb::FindAnim(AFrameId anim_id) const {
    const auto found = anim_indices_by_id.find(anim_id);
    if (found == anim_indices_by_id.end()) {
        return nullptr;
    }
    return &anims[found->second];
}

const AFrame* AFrameDb::FindFrame(const std::string& name, std::size_t ordered_frame_index) const {
    const AFrameAnim* const anim = FindAnim(name);
    if (anim == nullptr || ordered_frame_index >= anim->frame_indices.size()) {
        return nullptr;
    }
    return &frames[anim->frame_indices[ordered_frame_index]];
}

const AFrame* AFrameDb::FindFrame(AFrameId anim_id, std::size_t ordered_frame_index) const {
    const AFrameAnim* const anim = FindAnim(anim_id);
    if (anim == nullptr || ordered_frame_index >= anim->frame_indices.size()) {
        return nullptr;
    }
    return &frames[anim->frame_indices[ordered_frame_index]];
}

} // namespace splonks
