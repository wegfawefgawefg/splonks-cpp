#pragma once

#include "ent.hpp"

#include <optional>
#include <vector>

namespace splonks {

struct EntPool {
    std::vector<Ent> ents;
    std::vector<std::size_t> available_ids;

    static constexpr std::size_t kMaxNumEnts = 1024;

    static EntPool New();

    std::optional<VID> NewEnt();
    void SetInactive(std::size_t ent_id);
    void SetInactiveVid(const VID& vid);
    void SetEntInactive(Ent& ent);
    VID GetVid(std::size_t id) const;
    const Ent& GetEntById(std::size_t id) const;
    const Ent* GetEnt(const VID& vid) const;
    Ent* GetEntMut(const VID& vid);
    std::vector<Ent>& GetEnts();
    std::size_t NumEnts() const;
    std::uint32_t NumActiveEnts() const;
    void ClearAllEnts();
    void ClearAllNonPlayerEnts();
};

} // namespace splonks
