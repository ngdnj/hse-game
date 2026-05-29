#include "entities/Enemy.hpp"
#include "core/Collision.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

using namespace sf;

namespace entities {

namespace {

constexpr float kMinSpeed = 0.001f;

Vector2f normalizeOrZero(const Vector2f& v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= kMinSpeed) return {0.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

Vector2f normalizeOrRight(const Vector2f& v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= kMinSpeed) return {1.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

} // namespace

Enemy::Enemy(const Vector2f& pos, [[maybe_unused]] const Vector2f& size,
             const FloatRect& worldBounds, core::ResourceManager* res)
    : worldBounds_(worldBounds), res_(res) {
    setPosition(pos);
    applyShapeFromDesc();
    (void)size;
}

void Enemy::applyShapeFromDesc() {
    if (res_) {
        auto shape = res_->makeShape("enemy");
        shape_ = std::move(shape);
    }
    if (!shape_) {
        shape_ = std::make_unique<RectangleShape>(Vector2f{36.f, 36.f});
        shape_->setFillColor(Color(220, 80, 80));
        shape_->setOutlineColor(Color::White);
        shape_->setOutlineThickness(1.f);
    }
    // Center origin
    const auto b = shape_->getLocalBounds();
    shape_->setOrigin({b.size.x * 0.5f, b.size.y * 0.5f});
}

FloatRect Enemy::getLocalBounds() const {
    // Shapes are drawn centered (shape origin is set to its center), so the
    // entity's local bounds must also be centered around (0,0). Otherwise
    // getGlobalBounds() becomes offset and collision AABBs are wrong.
    if (shape_) {
        const auto b = shape_->getLocalBounds();
        return {{-b.size.x * 0.5f, -b.size.y * 0.5f}, b.size};
    }
    return {{-18.f, -18.f}, {36.f, 36.f}};
}

void Enemy::update(float dt) {
    if (!isActive() || isDead()) return;

    flashTimer_ = std::max(0.f, flashTimer_ - dt);
    attackCooldownRemaining_ = std::max(0.f, attackCooldownRemaining_ - dt);

    // Apply and dampen knockback velocity.
    // While attacking (wind-up or pending strike), the enemy is locked in place.
    if (!attackWindingUp_ && !attackPending_) {
        if (knockbackVel_.x != 0.f || knockbackVel_.y != 0.f) {
            const Vector2f resolvedKb = resolveMove(knockbackVel_ * dt);
            if (resolvedKb.x != 0.f || resolvedKb.y != 0.f) {
                move(resolvedKb);
            }
            // Dampen: reduce by 80% per second
            const float decay = std::exp(-8.f * dt);
            knockbackVel_.x *= decay;
            knockbackVel_.y *= decay;
            if (std::abs(knockbackVel_.x) < 0.5f && std::abs(knockbackVel_.y) < 0.5f) {
                knockbackVel_ = {0.f, 0.f};
            }
        }
    } else {
        // Hard lock: ignore knockback during committed attack.
        knockbackVel_ = {0.f, 0.f};
    }

    // Melee attack wind-up/strike state machine.
    // While winding up, the enemy pauses chasing to make the telegraph readable.
    if (attackWindingUp_) {
        attackWindupRemaining_ -= dt;
        if (attackWindupRemaining_ <= 0.f) {
            attackWindingUp_ = false;
            attackPending_ = true;
            attackCooldownRemaining_ = kAttackCooldownSec_;
        }
    }

    // Simple chase: move toward player if position is known
    // (disabled while attack is committed).
    if (playerPos_ && !attackWindingUp_ && !attackPending_) {
        const Vector2f toPlayer = *playerPos_ - getPosition();
        const Vector2f dir = normalizeOrZero(toPlayer);
        const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        // Start an attack if close enough and not on cooldown.
        if (attackCooldownRemaining_ <= 0.f && dist <= kAttackRadius_) {
            attackWindingUp_ = true;
            attackWindupRemaining_ = kAttackWindupSec_;
            attackDir_ = normalizeOrRight(toPlayer);
        }

        if (!attackWindingUp_ && dist > 5.f) { // don't jitter when very close
            const Vector2f resolved = resolveMove(dir * chaseSpeed_ * dt);
            if (resolved.x != 0.f || resolved.y != 0.f) {
                move(resolved);
            }
        }
    }

    // Clamp to world
    const auto gb = getGlobalBounds();
    Vector2f pos = getPosition();
    if (gb.position.x < worldBounds_.position.x)
        pos.x += worldBounds_.position.x - gb.position.x;
    if (gb.position.y < worldBounds_.position.y)
        pos.y += worldBounds_.position.y - gb.position.y;
    const float boundRight = worldBounds_.position.x + worldBounds_.size.x;
    const float boundBottom = worldBounds_.position.y + worldBounds_.size.y;
    if (gb.position.x + gb.size.x > boundRight)
        pos.x -= (gb.position.x + gb.size.x) - boundRight;
    if (gb.position.y + gb.size.y > boundBottom)
        pos.y -= (gb.position.y + gb.size.y) - boundBottom;
    setPosition(pos);
}

void Enemy::takeDamage(int amount) {
    health_ = std::max(0, health_ - amount);
    flashTimer_ = kFlashDuration_;
    if (isDead()) markForRemoval();
}

sf::Vector2f Enemy::resolveMove(sf::Vector2f desired) noexcept {
    if (!obstacles_ || (desired.x == 0.f && desired.y == 0.f)) {
        return desired;
    }
    const auto gb = getGlobalBounds();
    core::AABB entityAABB = core::fromFloatRect(gb);
    return core::resolveMovement(entityAABB, desired, *obstacles_);
}

void Enemy::onDraw(sf::RenderTarget& target, sf::RenderStates states) const {
    // Telegraph melee strike radius during wind-up.
    if (attackWindingUp_) {
        const float t = std::clamp(1.f - (attackWindupRemaining_ / kAttackWindupSec_), 0.f, 1.f);
        const float baseAngle = std::atan2(attackDir_.y, attackDir_.x);

        // Draw a cone sector (like player's slash arc), in enemy-local space.
        constexpr int kSegments = 18;
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, kSegments + 2);
        const auto alphaFill = static_cast<std::uint8_t>(20.f + 70.f * t);
        const auto alphaEdge = static_cast<std::uint8_t>(90.f + 140.f * t);
        const sf::Color fill(255, 60, 60, alphaFill);
        const sf::Color edge(255, 140, 140, alphaEdge);

        fan[0].position = {0.f, 0.f};
        fan[0].color = fill;
        for (int i = 0; i <= kSegments; ++i) {
            const float frac = static_cast<float>(i) / static_cast<float>(kSegments);
            const float angle = baseAngle - kAttackArcHalfAngleRad_ +
                                frac * 2.f * kAttackArcHalfAngleRad_;
            fan[i + 1].position = sf::Vector2f{std::cos(angle), std::sin(angle)} * kAttackRadius_;
            fan[i + 1].color = edge;
        }

        // Draw in world space (ignore enemy's transform already baked in `states`)
        // is NOT desired here; we want it attached to the enemy, so keep `states`.
        target.draw(fan, states);
    }

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

    // Health bar above enemy
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
}

std::optional<Enemy::MeleeAttack> Enemy::consumePendingAttack() noexcept {
    if (!attackPending_) return std::nullopt;
    attackPending_ = false;
    return Enemy::MeleeAttack{.origin = getPosition(),
                              .direction = attackDir_,
                              .radius = kAttackRadius_,
                              .halfAngleRad = kAttackArcHalfAngleRad_,
                              .damage = kAttackDamage_};
}

// ---- Loot ----

Loot::Loot(const Vector2f& pos, std::string itemName, int value)
    : itemName_(std::move(itemName)), value_(value) {
    setPosition(pos);
    circle_.setRadius(10.f);
    if (itemName_ == "hp") {
        circle_.setFillColor(Color(80, 220, 120));
        circle_.setOutlineColor(Color::Black);
    } else {
        circle_.setFillColor(Color(120, 170, 240));
        circle_.setOutlineColor(Color(30, 40, 60));
    }
    circle_.setOutlineThickness(1.f);
    circle_.setOrigin({10.f, 10.f});
}

FloatRect Loot::getLocalBounds() const {
    return {{-10.f, -10.f}, {20.f, 20.f}};
}

void Loot::update(float dt) {
    if (attracted_) {
        pulse_ += dt * 6.f;
        if (pulse_ > 6.283f) pulse_ -= 6.283f;
    } else if (pulse_ > 0.f) {
        pulse_ = std::max(0.f, pulse_ - dt * 6.f);
    }
}

void Loot::onDraw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (attracted_) {
        const float pulse = 0.5f + 0.5f * std::sin(pulse_);
        const float glowR = 16.f + 3.f * pulse;
        const std::uint8_t alpha = static_cast<std::uint8_t>(60.f + 80.f * pulse);
        sf::CircleShape glow(glowR);
        glow.setOrigin({glowR, glowR});
        glow.setPosition(getPosition());
        glow.setFillColor(sf::Color(255, 220, 120, alpha));
        target.draw(glow, states);
    }
    target.draw(circle_, states);
}

} // namespace entities