#pragma once

#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstddef>

namespace core {

// AABB intersection test
[[nodiscard]] constexpr bool intersects(
    const sf::Vector2f& aMin, const sf::Vector2f& aMax,
    const sf::Vector2f& bMin, const sf::Vector2f& bMax) noexcept {
    return aMin.x < bMax.x && aMax.x > bMin.x &&
           aMin.y < bMax.y && aMax.y > bMin.y;
}

// AABB overlap area (for resolution)
[[nodiscard]] constexpr float overlapX(
    const sf::Vector2f& aMin, const sf::Vector2f& aMax,
    const sf::Vector2f& bMin, const sf::Vector2f& bMax) noexcept {
    return std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x);
}

[[nodiscard]] constexpr float overlapY(
    const sf::Vector2f& aMin, const sf::Vector2f& aMax,
    const sf::Vector2f& bMin, const sf::Vector2f& bMax) noexcept {
    return std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y);
}

} // namespace core