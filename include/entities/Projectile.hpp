#pragma once

#include "core/Entity.hpp"
#include "core/Collision.hpp"
#include <SFML/Graphics.hpp>
#include <deque>

namespace entities {

class Projectile final : public Entity {
public:
    Projectile(sf::Vector2f pos, sf::Vector2f direction, float speed, int damage);

    void update(float dt) override;
    [[nodiscard]] sf::FloatRect getLocalBounds() const override;
    [[nodiscard]] int damage() const noexcept { return damage_; }
    void markConsumed() noexcept {
        consumed_ = true;
        // Entity::draw() skips dead entities; this prevents a hit projectile
        // from being rendered for the rest of the frame.
        markForRemoval();
    }
    [[nodiscard]] bool consumed() const noexcept { return consumed_; }

    void setObstacles(const std::vector<core::AABB>* obstacles) noexcept { obstacles_ = obstacles; }
    void setWorldBounds(const sf::FloatRect& bounds) noexcept {
        worldBounds_ = bounds;
        hasWorldBounds_ = true;
    }
    void setMaxBounces(int bounces) noexcept { maxBounces_ = bounces < 0 ? 0 : bounces; }
    void setAcceleration(float accelPerSec) noexcept { accelPerSec_ = accelPerSec; }
    void setHealing(int amount) noexcept {
        healingAmount_ = amount < 0 ? 0 : amount;
        isHealing_ = healingAmount_ > 0;
    }
    [[nodiscard]] bool isHealing() const noexcept { return isHealing_; }
    [[nodiscard]] int healingAmount() const noexcept { return healingAmount_; }
    void setTint(const sf::Color& core, const sf::Color& glow, const sf::Color& trail) noexcept;

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    static constexpr float kRadius{8.f};
    static constexpr std::size_t kTrailLength{8};

    int damage_{10};
    bool consumed_{false};
    bool isHealing_{false};
    int healingAmount_{0};
    sf::Vector2f velocity_{0.f, 0.f};
    float accelPerSec_{0.f};
    sf::CircleShape core_;
    sf::CircleShape glow_;
    sf::Color trailColor_{255, 180, 40, 180};
    std::deque<sf::Vector2f> trail_;
    const std::vector<core::AABB>* obstacles_{nullptr};
    sf::FloatRect worldBounds_{};
    bool hasWorldBounds_{false};
    int bounceCount_{0};
    int maxBounces_{2};
};

} // namespace entities