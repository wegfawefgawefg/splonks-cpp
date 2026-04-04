#include "entity_manager.hpp"

#include <cstdio>

namespace splonks {

EntityManager EntityManager::New() {
    EntityManager manager;
    manager.entities.reserve(kMaxNumEntities);
    manager.available_ids.reserve(kMaxNumEntities);

    for (std::size_t i = 0; i < kMaxNumEntities; ++i) {
        Entity new_entity = Entity::New();
        new_entity.vid.id = i;
        manager.entities.push_back(new_entity);
        manager.available_ids.insert(manager.available_ids.begin(), i);
    }

    return manager;
}

std::optional<VID> EntityManager::NewEntity() {
    if (!available_ids.empty()) {
        const std::size_t id = available_ids.back();
        available_ids.pop_back();
        entities[id].active = true;
        entities[id].vid.version += 1;
        return entities[id].vid;
    }

    std::printf("Entity budget bounce!\n");
    return std::nullopt;
}

void EntityManager::SetInactive(std::size_t entity_id) {
    entities[entity_id].active = false;
    available_ids.insert(available_ids.begin(), entity_id);
}

void EntityManager::SetInactiveVid(const VID& vid) {
    const Entity& entity = entities[vid.id];
    if (vid.version == entity.vid.version && entity.active) {
        SetInactive(vid.id);
    }
}

void EntityManager::SetEntityInactive(Entity& entity) {
    entity.active = false;
    available_ids.insert(available_ids.begin(), entity.vid.id);
}

VID EntityManager::GetVid(std::size_t id) const {
    return entities[id].vid;
}

const Entity& EntityManager::GetEntityById(std::size_t id) const {
    return entities[id];
}

const Entity* EntityManager::GetEntity(const VID& vid) const {
    const Entity& entity = entities[vid.id];
    if (vid.version == entity.vid.version && entity.active) {
        return &entity;
    }
    return nullptr;
}

Entity* EntityManager::GetEntityMut(const VID& vid) {
    Entity& entity = entities[vid.id];
    if (entity.active && vid.version == entity.vid.version) {
        return &entity;
    }
    return nullptr;
}

std::vector<Entity>& EntityManager::GetEntities() {
    return entities;
}

std::size_t EntityManager::NumEntities() const {
    return entities.size();
}

std::uint32_t EntityManager::NumActiveEntities() const {
    std::uint32_t count = 0;
    for (const Entity& entity : entities) {
        if (entity.active) {
            count += 1;
        }
    }
    return count;
}

void EntityManager::ClearAllEntities() {
    available_ids.clear();
    for (std::size_t i = 0; i < kMaxNumEntities; ++i) {
        available_ids.insert(available_ids.begin(), i);
        entities[i].active = false;
        entities[i].type_ = EntityType::None;
    }
}

void EntityManager::ClearAllNonPlayerEntities() {
    available_ids.clear();
    for (std::size_t i = 0; i < kMaxNumEntities; ++i) {
        if (entities[i].type_ != EntityType::Player) {
            available_ids.insert(available_ids.begin(), i);
            entities[i].active = false;
            entities[i].type_ = EntityType::None;
        }
    }
}

} // namespace splonks
