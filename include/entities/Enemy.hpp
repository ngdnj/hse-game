#pragma once

#include "core/Collision.hpp"
#include "core/Entity.hpp"
#include "core/ResourceManager.hpp"
#include <SFML/Graphics.hpp>
#include <optional>

namespace entities {

class Enemy final : public Entity {
public:
    Enemy(const sf::Vector2f& pos, const sf::Vector2f& size,
          const sf::FloatRect& worldBounds, core::ResourceManager* res);

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
    void setChaseSpeed(float speed) noexcept { chaseSpeed_ = speed; }
    void setObstacles(const std::vector<core::AABB>* obstacles) noexcept { obstacles_ = obstacles; }
    sf::Vector2f resolveMove(sf::Vector2f desired) noexcept;

    struct MeleeAttack {
        sf::Vector2f origin;
        sf::Vector2f direction; // unit vector (facing for the strike)
        float radius{0.f};
        float halfAngleRad{0.f};
        int damage{0};
    };

    /// If an attack finished its wind-up this frame, returns the strike data
    /// once and clears the pending flag.
    [[nodiscard]] std::optional<MeleeAttack> consumePendingAttack() noexcept;

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    void applyShapeFromDesc();

    const sf::Vector2f* playerPos_{nullptr};
    sf::FloatRect worldBounds_;
    core::ResourceManager* res_{nullptr};

    // Shape rendering
    std::unique_ptr<sf::Shape> shape_;

    // Combat
    int health_{60};
    int maxHealth_{60};

    // Damage flash
    float flashTimer_{0.f};
    static constexpr float kFlashDuration_{0.15f};

    // AI
    float chaseSpeed_{100.f};
    const std::vector<core::AABB>* obstacles_{nullptr};
    sf::Vector2f knockbackVel_{0.f, 0.f};

    // Melee attack (telegraphed)
    static constexpr float kAttackRadius_{70.f};
    static constexpr float kAttackArcHalfAngleRad_{3.1415926f / 3.f}; // 60°
    static constexpr float kAttackWindupSec_{0.75f};
    static constexpr float kAttackCooldownSec_{1.2f};
    static constexpr int kAttackDamage_{12};

    float attackWindupRemaining_{0.f};
    float attackCooldownRemaining_{0.f};
    bool attackWindingUp_{false};
    bool attackPending_{false};
    sf::Vector2f attackDir_{1.f, 0.f};
};

class Loot final : public Entity {
public:
    Loot(const sf::Vector2f& pos, std::string itemName, int value = 1);

    void update(float dt) override;
    [[nodiscard]] sf::FloatRect getLocalBounds() const override;

    [[nodiscard]] const std::string& itemName() const noexcept { return itemName_; }
    [[nodiscard]] int value() const noexcept { return value_; }
    [[nodiscard]] bool consumed() const noexcept { return consumed_; }

    void consume() noexcept { consumed_ = true; }
    void setAttracted(bool attracted) noexcept { attracted_ = attracted; }

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    std::string itemName_;
    int value_{1};
    bool consumed_{false};
    bool attracted_{false};
    float pulse_{0.f};

    // Visual
    sf::CircleShape circle_;
};

} // namespace entities