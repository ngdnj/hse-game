#include "entities/Projectile.hpp"

using namespace sf;

namespace entities {

Projectile::Projectile(Vector2f pos, Vector2f direction, float speed, int damage)
    : damage_(damage), velocity_(direction * speed) {
    setPosition(pos);
    circle_.setRadius(6.f);
    circle_.setFillColor(Color(255, 120, 255));
    circle_.setOutlineColor(Color::White);
    circle_.setOutlineThickness(1.f);
    circle_.setOrigin({6.f, 6.f});
}

FloatRect Projectile::getLocalBounds() const {
    return {{-6.f, -6.f}, {12.f, 12.f}};
}

void Projectile::update(float dt) {
    if (consumed_) return;
    move(velocity_ * dt);
}

void Projectile::onDraw(RenderTarget& target, RenderStates states) const {
    target.draw(circle_, states);
}

} // namespace entities