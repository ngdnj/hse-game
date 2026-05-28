#include "combat/SwordWeapon.hpp"

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

SwordWeapon::SwordWeapon(int damage, float range, float cooldown, float knockbackMultiplier)
    : damage_(damage)
    , range_(range)
    , cooldown_(cooldown)
    , knockbackMultiplier_(std::max(0.f, knockbackMultiplier))
{}

void SwordWeapon::update(float dt) {
    cooldownRemaining_ = std::max(0.f, cooldownRemaining_ - dt);
}

bool SwordWeapon::canFire() const noexcept {
    return cooldownRemaining_ <= 0.f;
}

AttackResult SwordWeapon::fire(sf::Vector2f origin, sf::Vector2f dir) {
    AttackResult result;
    if (!canFire()) return result;

    cooldownRemaining_ = cooldown_;
    result.hasMeleeHitbox = true;
    result.meleeHitbox = computeHitbox(origin, dir, range_);
    result.knockbackMultiplier = knockbackMultiplier_;
    return result;
}

sf::FloatRect SwordWeapon::computeHitbox(sf::Vector2f origin,
                                          sf::Vector2f dir,
                                          float range) {
    const sf::Vector2f d = normalizeOrRight(dir);
    constexpr float halfH = 30.f;
    const float halfW = range * 0.5f;
    const sf::Vector2f centerOffset = d * range * 0.5f;
    return sf::FloatRect{
        sf::Vector2f{origin.x + centerOffset.x - halfW,
                     origin.y + centerOffset.y - halfH},
        sf::Vector2f{range, halfH * 2.f}};
}

} // namespace combat
