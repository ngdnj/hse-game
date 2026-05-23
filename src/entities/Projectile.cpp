#include "entities/Projectile.hpp"

using namespace sf;

namespace entities {

Projectile::Projectile(Vector2f pos, Vector2f direction, float speed, int damage)
    : damage_(damage), velocity_(direction * speed) {
    setPosition(pos);

    // Bright yellow core with a thick black-rimmed white outline for max
    // contrast against the dark background.
    core_.setRadius(kRadius);
    core_.setFillColor(Color(255, 240, 60));
    core_.setOutlineColor(Color::Black);
    core_.setOutlineThickness(2.f);
    core_.setOrigin({kRadius, kRadius});

    // Translucent halo behind the core to imply glow.
    const float glowR = kRadius * 2.f;
    glow_.setRadius(glowR);
    glow_.setFillColor(Color(255, 200, 80, 90));
    glow_.setOrigin({glowR, glowR});
}

FloatRect Projectile::getLocalBounds() const {
    return {{-kRadius, -kRadius}, {kRadius * 2.f, kRadius * 2.f}};
}

void Projectile::update(float dt) {
    if (consumed_) return;
    trail_.push_front(getPosition());
    if (trail_.size() > kTrailLength) trail_.pop_back();
    move(velocity_ * dt);
}

void Projectile::onDraw(RenderTarget& target, RenderStates states) const {
    // Trail and halo positions are already in world space, so draw them with
    // default states (otherwise they'd be double-transformed by the Entity's
    // own matrix already baked into `states`).
    RenderStates worldStates;

    // Trail: oldest position smallest and most transparent.
    for (std::size_t i = 0; i < trail_.size(); ++i) {
        const float t = 1.f - static_cast<float>(i) / static_cast<float>(kTrailLength);
        CircleShape ghost(kRadius * (0.4f + 0.5f * t));
        ghost.setOrigin({ghost.getRadius(), ghost.getRadius()});
        ghost.setPosition(trail_[i]);
        ghost.setFillColor(Color(255, 180, 40, static_cast<std::uint8_t>(180.f * t)));
        target.draw(ghost, worldStates);
    }

    // Glow halo behind the core.
    CircleShape halo = glow_;
    halo.setPosition(getPosition());
    target.draw(halo, worldStates);

    // Solid core, transformed by the Entity.
    target.draw(core_, states);
}

} // namespace entities