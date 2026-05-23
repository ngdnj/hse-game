#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstddef>
#include <vector>

namespace core {

// AABB intersection test
[[nodiscard]] constexpr bool intersects(
    const sf::Vector2f& aMin, const sf::Vector2f& aMax,
    const sf::Vector2f& bMin, const sf::Vector2f& bMax) noexcept {
    return aMin.x < bMax.x && aMax.x > bMin.x &&
           aMin.y < bMax.y && aMax.y > bMin.y;
}

// AABB as min-corner / max-corner
struct AABB {
    sf::Vector2f min;
    sf::Vector2f max;
};

[[nodiscard]] inline AABB makeAABB(sf::Vector2f pos, sf::Vector2f size) noexcept {
    return {pos, {pos.x + size.x, pos.y + size.y}};
}

[[nodiscard]] inline AABB fromFloatRect(const sf::FloatRect& r) noexcept {
    return {r.position, {r.position.x + r.size.x, r.position.y + r.size.y}};
}

// Resolve a movement vector against a list of static AABBs.
// Returns the resolved displacement that avoids all obstacles (slide-along-walls).
// Algorithm: try full movement, then X-only, then Y-only.
[[nodiscard]] inline sf::Vector2f resolveMovement(
    AABB entity, sf::Vector2f desired,
    const std::vector<AABB>& obstacles) noexcept {

    if (desired.x == 0.f && desired.y == 0.f) return {};

    const auto testMove = [&](AABB box, sf::Vector2f d) -> bool {
        for (const auto& obs : obstacles) {
            if (intersects(
                    {box.min.x + d.x, box.min.y + d.y},
                    {box.max.x + d.x, box.max.y + d.y},
                    obs.min, obs.max)) {
                return false;
            }
        }
        return true;
    };

    // Try full movement
    if (testMove(entity, desired)) {
        return desired;
    }

    // Try X-only (slide along Y)
    if (testMove(entity, {desired.x, 0.f})) {
        return {desired.x, 0.f};
    }

    // Try Y-only (slide along X)
    if (testMove(entity, {0.f, desired.y})) {
        return {0.f, desired.y};
    }

    // Completely blocked
    return {0.f, 0.f};
}

// Static obstacle definition with a unique ID and sf::Shape factory
struct ObstacleDesc {
    int id;
    sf::Vector2f position;
    sf::Vector2f size;
    sf::Color fillColor;
    sf::Color outlineColor;
    float outlineThickness;
};

// A simple static obstacle that lives in world space and can be rendered
class Obstacle {
public:
    explicit Obstacle(ObstacleDesc desc)
        : id_(desc.id)
        , shape_(std::make_unique<sf::RectangleShape>(desc.size))
    {
        shape_->setPosition(desc.position);
        shape_->setFillColor(desc.fillColor);
        shape_->setOutlineColor(desc.outlineColor);
        shape_->setOutlineThickness(desc.outlineThickness);
        shape_->setOrigin({desc.size.x * 0.5f, desc.size.y * 0.5f});
        // shift origin so position marks the center
        shape_->setPosition({
            desc.position.x + desc.size.x * 0.5f,
            desc.position.y + desc.size.y * 0.5f
        });
    }

    [[nodiscard]] AABB getAABB() const noexcept {
        const auto b = shape_->getLocalBounds();
        const sf::Vector2f pos = shape_->getPosition();
        return {{pos.x - b.size.x * 0.5f, pos.y - b.size.y * 0.5f},
                {pos.x + b.size.x * 0.5f, pos.y + b.size.y * 0.5f}};
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        target.draw(*shape_, states);
    }

    [[nodiscard]] int id() const noexcept { return id_; }

private:
    int id_;
    std::unique_ptr<sf::RectangleShape> shape_;
};

// Generate a list of AABBs from obstacles for collision queries
[[nodiscard]] inline std::vector<AABB> toAABBs(const std::vector<Obstacle*>& obs) noexcept {
    std::vector<AABB> result;
    result.reserve(obs.size());
    for (const auto* o : obs) {
        result.push_back(o->getAABB());
    }
    return result;
}

} // namespace core