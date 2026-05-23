#pragma once

#include "entities/Projectile.hpp"
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <memory>
#include <string_view>
#include <vector>

namespace combat {

// What a single firing of a Weapon produces. The game loop reads this and
// translates it into gameplay effects (damage to enemies hit by hitbox,
// projectiles added to the world, knockback applied, etc.).
struct AttackResult {
    sf::FloatRect meleeHitbox{};
    bool hasMeleeHitbox{false};
    std::vector<std::unique_ptr<entities::Projectile>> projectiles;
    float knockbackMultiplier{1.f};
};

/// Polymorphic weapon — Sword (melee) and Shotgun (ranged) derive from this.
/// Kept SFML-light in its interface so subclasses can be unit-tested without
/// a render window.
class Weapon {
public:
    virtual ~Weapon() = default;

    /// Tick cooldown timers.
    virtual void update(float dt) = 0;

    /// Can the weapon fire right now (cooldown elapsed)?
    [[nodiscard]] virtual bool canFire() const noexcept = 0;

    /// Trigger an attack. `origin` is the attacker's world position and
    /// `dir` is the facing direction (unit vector). Returns the description
    /// of what happened.
    [[nodiscard]] virtual AttackResult fire(sf::Vector2f origin,
                                            sf::Vector2f dir) = 0;

    /// Display name (e.g. "Sword", "Shotgun").
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Damage per hit / per pellet.
    [[nodiscard]] virtual int damage() const noexcept = 0;

    /// Set damage. Default no-op for weapons whose damage is immutable.
    virtual void setDamage(int) noexcept {}
};

} // namespace combat
