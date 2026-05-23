#pragma once

#include "combat/Weapon.hpp"
#include <vector>

namespace combat {

/// Close-range shotgun: fires `pelletCount_` projectiles spread evenly across
/// a 2*spreadHalfAngle cone, each dealing `damage_` and inheriting
/// `knockbackMultiplier_` for the game loop to scale impact knockback.
class ShotgunWeapon final : public Weapon {
public:
    explicit ShotgunWeapon(int damagePerPellet = 8, int pelletCount = 5,
                           float spreadHalfAngleRad = 0.35f,
                           float projectileSpeed = 380.f,
                           float cooldown = 0.9f,
                           float knockbackMultiplier = 2.4f);

    void update(float dt) override;
    [[nodiscard]] bool canFire() const noexcept override;
    [[nodiscard]] AttackResult fire(sf::Vector2f origin,
                                    sf::Vector2f dir) override;

    [[nodiscard]] std::string_view name() const noexcept override { return "Shotgun"; }
    [[nodiscard]] int damage() const noexcept override { return damagePerPellet_; }
    void setDamage(int d) noexcept override { damagePerPellet_ = d; }
    [[nodiscard]] int pelletCount() const noexcept { return pelletCount_; }
    [[nodiscard]] float spreadHalfAngleRad() const noexcept { return spreadHalfAngleRad_; }
    [[nodiscard]] float cooldown() const noexcept { return cooldown_; }

    /// Pure trajectory math, no SFML state, no allocation in projectile form.
    /// Given a base direction and a number of pellets, returns the unit-vector
    /// firing direction of each pellet evenly distributed across the cone.
    /// pelletCount must be >= 1; pelletCount == 1 -> exactly `baseDir`.
    [[nodiscard]] static std::vector<sf::Vector2f>
    pelletDirections(sf::Vector2f baseDir, int pelletCount,
                     float spreadHalfAngleRad);

private:
    int damagePerPellet_;
    int pelletCount_;
    float spreadHalfAngleRad_;
    float projectileSpeed_;
    float cooldown_;
    float knockbackMultiplier_;
    float cooldownRemaining_{0.f};
};

} // namespace combat
