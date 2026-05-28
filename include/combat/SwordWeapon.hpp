#pragma once

#include "combat/Weapon.hpp"

namespace combat {

/// Melee weapon that creates a rectangular hitbox in front of the attacker.
/// Replaces the old Player-baked attack logic.
class SwordWeapon final : public Weapon {
public:
    explicit SwordWeapon(int damage = 25, float range = 55.f,
                         float cooldown = 0.4f,
                         float knockbackMultiplier = 1.6f);

    void update(float dt) override;
    [[nodiscard]] bool canFire() const noexcept override;
    [[nodiscard]] AttackResult fire(sf::Vector2f origin,
                                    sf::Vector2f dir) override;

    [[nodiscard]] std::string_view name() const noexcept override { return "Sword"; }
    [[nodiscard]] int damage() const noexcept override { return damage_; }
    void setDamage(int d) noexcept override { damage_ = d; }
    void setRange(float r) noexcept { range_ = r; }
    void setCooldown(float c) noexcept { cooldown_ = c; }
    [[nodiscard]] float range() const noexcept { return range_; }
    [[nodiscard]] float cooldown() const noexcept { return cooldown_; }
    [[nodiscard]] float knockbackMultiplier() const noexcept { return knockbackMultiplier_; }

    /// Compute the melee hitbox without firing. Useful for tests and the
    /// renderer that wants the same geometry as the actual swing.
    [[nodiscard]] static sf::FloatRect computeHitbox(sf::Vector2f origin,
                                                      sf::Vector2f dir,
                                                      float range);

private:
    int damage_;
    float range_;
    float cooldown_;
    float knockbackMultiplier_{1.6f};
    float cooldownRemaining_{0.f};
};

} // namespace combat
