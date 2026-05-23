#include "combat/ShotgunWeapon.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace combat {

namespace {

sf::Vector2f normalizeOrRight(sf::Vector2f v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= std::numeric_limits<float>::epsilon()) return {1.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

} // namespace

ShotgunWeapon::ShotgunWeapon(int damagePerPellet, int pelletCount,
                             float spreadHalfAngleRad, float projectileSpeed,
                             float cooldown, float knockbackMultiplier)
    : damagePerPellet_(damagePerPellet),
      pelletCount_(std::max(1, pelletCount)),
      spreadHalfAngleRad_(std::max(0.f, spreadHalfAngleRad)),
      projectileSpeed_(projectileSpeed),
      cooldown_(cooldown),
      knockbackMultiplier_(knockbackMultiplier) {}

void ShotgunWeapon::update(float dt) {
    cooldownRemaining_ = std::max(0.f, cooldownRemaining_ - dt);
}

bool ShotgunWeapon::canFire() const noexcept {
    return cooldownRemaining_ <= 0.f;
}

AttackResult ShotgunWeapon::fire(sf::Vector2f origin, sf::Vector2f dir) {
    AttackResult result;
    if (!canFire()) return result;
    cooldownRemaining_ = cooldown_;
    result.knockbackMultiplier = knockbackMultiplier_;

    const auto dirs = pelletDirections(dir, pelletCount_, spreadHalfAngleRad_);
    result.projectiles.reserve(dirs.size());
    for (const auto& d : dirs) {
        result.projectiles.push_back(std::make_unique<entities::Projectile>(
            origin, d, projectileSpeed_, damagePerPellet_));
    }
    return result;
}

std::vector<sf::Vector2f>
ShotgunWeapon::pelletDirections(sf::Vector2f baseDir, int pelletCount,
                                float spreadHalfAngleRad) {
    std::vector<sf::Vector2f> out;
    if (pelletCount < 1) return out;

    const sf::Vector2f base = normalizeOrRight(baseDir);
    const float baseAngle = std::atan2(base.y, base.x);
    out.reserve(static_cast<std::size_t>(pelletCount));

    if (pelletCount == 1) {
        out.push_back(base);
        return out;
    }

    const float startAngle = baseAngle - spreadHalfAngleRad;
    const float step = (2.f * spreadHalfAngleRad) /
                       static_cast<float>(pelletCount - 1);
    for (int i = 0; i < pelletCount; ++i) {
        const float a = startAngle + step * static_cast<float>(i);
        out.push_back({std::cos(a), std::sin(a)});
    }
    return out;
}

} // namespace combat
