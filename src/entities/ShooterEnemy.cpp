#include "entities/ShooterEnemy.hpp"
#include "entities/Enemy.hpp" // for Projectile
#include "core/Collision.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace sf;

namespace entities {

ShooterEnemy::ShooterEnemy(const Vector2f& pos, const FloatRect& worldBounds,
                           core::ResourceManager* res)
    : worldBounds_(worldBounds), res_(res) {
    setPosition(pos);
    if (res_) {
        auto sh = res_->makeShape("shooter");
        if (sh) shape_ = std::move(sh);
    }
    if (!shape_) {
        shape_ = std::make_unique<RectangleShape>(Vector2f{32.f, 32.f});
        shape_->setFillColor(Color(150, 80, 200));
        shape_->setOutlineColor(Color::White);
        shape_->setOutlineThickness(1.f);
    }
    const auto b = shape_->getLocalBounds();
    shape_->setOrigin({b.size.x * 0.5f, b.size.y * 0.5f});
}

FloatRect ShooterEnemy::getLocalBounds() const {
    if (shape_) {
        const auto b = shape_->getLocalBounds();
        return {{-b.size.x * 0.5f, -b.size.y * 0.5f}, b.size};
    }
    return {{-16.f, -16.f}, {32.f, 32.f}};
}

sf::Vector2f ShooterEnemy::normalizeOrZero(sf::Vector2f v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= std::numeric_limits<float>::epsilon()) return {0.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

void ShooterEnemy::update(float dt) {
    if (!isActive() || isDead()) return;

    flashTimer_ = std::max(0.f, flashTimer_ - dt);

    // Tick knockback
    if (knockbackVel_.x != 0.f || knockbackVel_.y != 0.f) {
        const Vector2f resolvedKb = resolveMove(knockbackVel_ * dt);
        if (resolvedKb.x != 0.f || resolvedKb.y != 0.f) move(resolvedKb);
        const float decay = std::exp(-8.f * dt);
        knockbackVel_.x *= decay;
        knockbackVel_.y *= decay;
        if (std::abs(knockbackVel_.x) < 0.5f && std::abs(knockbackVel_.y) < 0.5f)
            knockbackVel_ = {0.f, 0.f};
    }

    // Update projectiles
    for (auto& p : projectiles_) p->update(dt);
    std::erase_if(projectiles_, [](const std::unique_ptr<Projectile>& p) {
        return p->consumed();
    });

    // Shooter AI
    if (playerPos_) {
        const Vector2f toPlayer = *playerPos_ - getPosition();
        const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        if (dist > preferredDistance_ + 30.f) {
            // Move closer
            const Vector2f dir = normalizeOrZero(toPlayer);
            const Vector2f resolved = resolveMove(dir * chaseSpeed_ * dt);
            if (resolved.x != 0.f || resolved.y != 0.f) move(resolved);
        } else if (dist < preferredDistance_ - 30.f) {
            // Back off
            const Vector2f dir = normalizeOrZero(toPlayer);
            const Vector2f resolved = resolveMove(dir * (-chaseSpeed_) * 0.6f * dt);
            if (resolved.x != 0.f || resolved.y != 0.f) move(resolved);
        }

        // Shooting
        shootCooldown_ = std::max(0.f, shootCooldown_ - dt);
        if (shootCooldown_ <= 0.f && dist < 600.f) {
            shootCooldown_ = shootInterval_;
            const Vector2f dir = normalizeOrZero(toPlayer);
            auto proj = std::make_unique<Projectile>(getPosition(), dir, 200.f, 15);
            projectiles_.push_back(std::move(proj));
        }
    }

    // World bounds clamp
    const auto gb = getGlobalBounds();
    Vector2f pos = getPosition();
    if (gb.position.x < worldBounds_.position.x) pos.x += worldBounds_.position.x - gb.position.x;
    if (gb.position.y < worldBounds_.position.y) pos.y += worldBounds_.position.y - gb.position.y;
    const float boundRight = worldBounds_.position.x + worldBounds_.size.x;
    const float boundBottom = worldBounds_.position.y + worldBounds_.size.y;
    if (gb.position.x + gb.size.x > boundRight) pos.x -= (gb.position.x + gb.size.x) - boundRight;
    if (gb.position.y + gb.size.y > boundBottom) pos.y -= (gb.position.y + gb.size.y) - boundBottom;
    setPosition(pos);
}

void ShooterEnemy::takeDamage(int amount) {
    health_ = std::max(0, health_ - amount);
    flashTimer_ = kFlashDuration_;
    if (isDead()) markForRemoval();
}

sf::Vector2f ShooterEnemy::resolveMove(sf::Vector2f desired) noexcept {
    if (!obstacles_ || (desired.x == 0.f && desired.y == 0.f)) return desired;
    const auto gb = getGlobalBounds();
    core::AABB entityAABB = core::fromFloatRect(gb);
    return core::resolveMovement(entityAABB, desired, *obstacles_);
}

std::vector<std::unique_ptr<Projectile>> ShooterEnemy::extractFiredProjectiles() {
    return std::move(projectiles_);
}

void ShooterEnemy::onDraw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (shape_) {
        if (flashTimer_ > 0.f) {
            const Color savedFill = shape_->getFillColor();
            shape_->setFillColor(Color(255, 240, 240));
            target.draw(*shape_, states);
            shape_->setFillColor(savedFill);
        } else {
            target.draw(*shape_, states);
        }
    }

    // Health bar
    const float barW = 40.f;
    const float barH = 4.f;
    const auto pos = getPosition();
    RectangleShape bg{{barW, barH}};
    bg.setFillColor(Color(80, 80, 80));
    bg.setOrigin({barW * 0.5f, barH * 0.5f});
    bg.setPosition({pos.x, pos.y - getLocalBounds().size.y * 0.5f - 8.f});
    target.draw(bg, states);

    if (health_ < maxHealth_) {
        const float ratio = static_cast<float>(health_) / maxHealth_;
        RectangleShape fg{{barW * ratio, barH}};
        fg.setFillColor(ratio > 0.5f ? Color(80, 220, 80) :
                       ratio > 0.25f ? Color(220, 220, 80) : Color(220, 80, 80));
        fg.setOrigin({barW * 0.5f, barH * 0.5f});
        fg.setPosition({pos.x, pos.y - getLocalBounds().size.y * 0.5f - 8.f});
        target.draw(fg, states);
    }

    // Projectiles
    for (const auto& p : projectiles_) target.draw(*p, states);
}

} // namespace entities