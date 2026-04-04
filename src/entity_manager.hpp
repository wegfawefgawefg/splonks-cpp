#pragma once

#include "entity.hpp"

#include <optional>
#include <vector>

namespace splonks {

struct EntityManager {
    std::vector<Entity> entities;
    std::vector<std::size_t> available_ids;

    static constexpr std::size_t kMaxNumEntities = 128;

    static EntityManager New();

    std::optional<VID> NewEntity();
    void SetInactive(std::size_t entity_id);
    void SetInactiveVid(const VID& vid);
    void SetEntityInactive(Entity& entity);
    VID GetVid(std::size_t id) const;
    const Entity& GetEntityById(std::size_t id) const;
    const Entity* GetEntity(const VID& vid) const;
    Entity* GetEntityMut(const VID& vid);
    std::vector<Entity>& GetEntities();
    std::size_t NumEntities() const;
    std::uint32_t NumActiveEntities() const;
    void ClearAllEntities();
    void ClearAllNonPlayerEntities();
};

} // namespace splonks
