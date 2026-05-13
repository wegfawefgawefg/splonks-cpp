#include "ent/manager.hpp"

#include <cstdio>

namespace splonks {

EntPool EntPool::New() {
    EntPool manager;
    manager.ents.reserve(kMaxNumEnts);
    manager.available_ids.reserve(kMaxNumEnts);

    for (std::size_t i = 0; i < kMaxNumEnts; ++i) {
        Ent new_ent = Ent::New();
        new_ent.vid.id = i;
        manager.ents.push_back(new_ent);
        manager.available_ids.insert(manager.available_ids.begin(), i);
    }

    return manager;
}

std::optional<VID> EntPool::NewEnt() {
    if (!available_ids.empty()) {
        const std::size_t id = available_ids.back();
        available_ids.pop_back();
        ents[id].active = true;
        ents[id].vid.version += 1;
        return ents[id].vid;
    }

    std::printf("Ent budget bounce!\n");
    return std::nullopt;
}

void EntPool::SetInactive(std::size_t ent_id) {
    if (ent_id >= ents.size() || !ents[ent_id].active) {
        return;
    }
    ents[ent_id].active = false;
    available_ids.insert(available_ids.begin(), ent_id);
}

void EntPool::SetInactiveVid(const VID& vid) {
    const Ent& ent = ents[vid.id];
    if (vid.version == ent.vid.version && ent.active) {
        SetInactive(vid.id);
    }
}

void EntPool::SetEntInactive(Ent& ent) {
    ent.active = false;
    available_ids.insert(available_ids.begin(), ent.vid.id);
}

VID EntPool::GetVid(std::size_t id) const {
    return ents[id].vid;
}

const Ent& EntPool::GetEntById(std::size_t id) const {
    return ents[id];
}

const Ent* EntPool::GetEnt(const VID& vid) const {
    const Ent& ent = ents[vid.id];
    if (vid.version == ent.vid.version && ent.active) {
        return &ent;
    }
    return nullptr;
}

Ent* EntPool::GetEntMut(const VID& vid) {
    Ent& ent = ents[vid.id];
    if (ent.active && vid.version == ent.vid.version) {
        return &ent;
    }
    return nullptr;
}

std::vector<Ent>& EntPool::GetEnts() {
    return ents;
}

std::size_t EntPool::NumEnts() const {
    return ents.size();
}

std::uint32_t EntPool::NumActiveEnts() const {
    std::uint32_t count = 0;
    for (const Ent& ent : ents) {
        if (ent.active) {
            count += 1;
        }
    }
    return count;
}

void EntPool::ClearAllEnts() {
    available_ids.clear();
    for (std::size_t i = 0; i < kMaxNumEnts; ++i) {
        available_ids.insert(available_ids.begin(), i);
        ents[i].active = false;
        ents[i].type_ = EntType::None;
    }
}

void EntPool::ClearAllNonPlayerEnts() {
    available_ids.clear();
    for (std::size_t i = 0; i < kMaxNumEnts; ++i) {
        if (!IsPlayerLikeEntType(ents[i].type_)) {
            available_ids.insert(available_ids.begin(), i);
            ents[i].active = false;
            ents[i].type_ = EntType::None;
        }
    }
}

} // namespace splonks
