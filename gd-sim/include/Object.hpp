#pragma once
#include <util.hpp>
#include <unordered_map>
#include <optional>
#include <memory>
#include <concepts>
#include <type_traits>
#include <utility>

struct ObjectContainer;
struct Player;
struct TeleportPortal;
struct SpeedPortal;

struct Object : public Entity {
    /// NOT object id. A unique ID associated with each Object for direct comparisons
    int id = -1;

    /**
     * In GD, some objects have their collision checks later than others (blocks, hazards).
     * A higher priority number means collisions are processed later.
     */
    int prio = 0;

    Object() = default;
    Object(Vec2D size, std::unordered_map<int, std::string>&& fields);
    virtual ~Object() = default;

    /// Polymorphic copy used when a Level is copied by the pathfinder.
    virtual std::unique_ptr<Object> clone() const = 0;

    /// Determines if the object should be counted as colliding with the player.
    virtual bool touching(Player const&) const;

    /// Where all of the collision magic happens.
    virtual void collide(Player&) const;

    /// Type-safe hooks used by post-parse linking and path prediction without RTTI.
    virtual TeleportPortal* asTeleportPortal() { return nullptr; }
    virtual SpeedPortal const* asSpeedPortal() const { return nullptr; }

    /// Create an object from a given level string mapping.
    static std::optional<ObjectContainer> create(std::unordered_map<int, std::string>&& ob);
};

/**
 * Owns a concrete Object while exposing pointer-like access. The previous implementation
 * copied polymorphic objects into an unaligned char buffer with memcpy, which did not
 * establish a C++ object lifetime and was undefined behavior.
 */
struct ObjectContainer {
    std::unique_ptr<Object> object;

    ObjectContainer(ObjectContainer const& other)
        : object(other.object ? other.object->clone() : nullptr) {}
    ObjectContainer(ObjectContainer&&) noexcept = default;

    ObjectContainer& operator=(ObjectContainer const& other) {
        if (this != &other)
            object = other.object ? other.object->clone() : nullptr;
        return *this;
    }
    ObjectContainer& operator=(ObjectContainer&&) noexcept = default;

    template <class T>
        requires std::derived_from<std::remove_cvref_t<T>, Object>
    explicit ObjectContainer(T&& obj)
        : object(std::make_unique<std::remove_cvref_t<T>>(std::forward<T>(obj))) {}

    Object const* operator->() const { return object.get(); }
    Object* operator->() { return object.get(); }
};
