#include "entities/Enemy.hpp"
#include "core/Collision.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

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
    if (shape_) return shape_->getLocalBounds();
    return {{0.f, 0.f}, {36.f, 36.f}};
}

void Enemy::update(float dt) {
    if (!isActive() || isDead()) return;

    // Simple chase: move toward player if position is known
    if (playerPos_) {
        const Vector2f toPlayer = *playerPos_ - getPosition();
        const Vector2f dir = normalizeOrZero(toPlayer);
        const float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

        if (dist > 5.f) { // don't jitter when very close
            move(dir * chaseSpeed_ * dt);
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
    if (isDead()) markForRemoval();
}

void Enemy::onDraw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (shape_) target.draw(*shape_, states);

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

// ---- Loot ----

Loot::Loot(const Vector2f& pos, std::string itemName, int value)
    : itemName_(std::move(itemName)), value_(value) {
    setPosition(pos);
    circle_.setRadius(10.f);
    circle_.setFillColor(Color(255, 215, 0));
    circle_.setOutlineColor(Color::White);
    circle_.setOutlineThickness(1.f);
    circle_.setOrigin({10.f, 10.f});
}

FloatRect Loot::getLocalBounds() const {
    return {{-10.f, -10.f}, {20.f, 20.f}};
}

void Loot::update(float dt) {
    (void)dt;
    // Loot just sits there; could add bobbing/pickup animation here
}

void Loot::onDraw(sf::RenderTarget& target, sf::RenderStates states) const {
    target.draw(circle_, states);
}

} // namespace entities