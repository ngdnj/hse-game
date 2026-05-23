#pragma once

#include "core/Collision.hpp"
#include "core/Entity.hpp"
#include "core/ResourceManager.hpp"
#include "entities/Projectile.hpp"
#include <SFML/Graphics.hpp>

namespace entities {

// ShooterEnemy: keeps distance from player, fires projectiles
class ShooterEnemy final : public Entity {
public:
    ShooterEnemy(const sf::Vector2f& pos, const sf::FloatRect& worldBounds,
                 core::ResourceManager* res);

    void update(float dt) override;
    [[nodiscard]] sf::FloatRect getLocalBounds() const override;

    void takeDamage(int amount);
    void applyKnockback(sf::Vector2f force) noexcept {
        knockbackVel_.x += force.x;
        knockbackVel_.y += force.y;
    }
    [[nodiscard]] int health() const noexcept { return health_; }
    [[nodiscard]] int maxHealth() const noexcept { return maxHealth_; }
    [[nodiscard]] bool isDead() const noexcept { return health_ <= 0; }

    void setPlayerPosition(const sf::Vector2f* pos) { playerPos_ = pos; }
    void setObstacles(const std::vector<core::AABB>* obstacles) noexcept { obstacles_ = obstacles; }
    sf::Vector2f resolveMove(sf::Vector2f desired) noexcept;

    // Projectile ownership
    void addProjectile(std::unique_ptr<Projectile> proj) { projectiles_.push_back(std::move(proj)); }
    [[nodiscard]] const std::vector<std::unique_ptr<Projectile>>& projectiles() const noexcept { return projectiles_; }

    // Called each frame to retrieve newly fired projectiles
    std::vector<std::unique_ptr<Projectile>> extractFiredProjectiles();

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    static sf::Vector2f normalizeOrZero(sf::Vector2f v);

    const sf::Vector2f* playerPos_{nullptr};
    sf::FloatRect worldBounds_;
    core::ResourceManager* res_{nullptr};

    std::unique_ptr<sf::Shape> shape_;
    std::vector<std::unique_ptr<Projectile>> projectiles_;

    int health_{80};
    int maxHealth_{80};

    // Damage flash
    float flashTimer_{0.f};
    static constexpr float kFlashDuration_{0.15f};

    float chaseSpeed_{70.f};
    const std::vector<core::AABB>* obstacles_{nullptr};
    sf::Vector2f knockbackVel_{0.f, 0.f};

    // Shooting state
    float shootCooldown_{0.f};
    float shootInterval_{2.5f}; // seconds between shots
    float preferredDistance_{250.f}; // ideal range from player
};

} // namespace entities