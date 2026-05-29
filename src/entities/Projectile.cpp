#include "entities/Projectile.hpp"
#include <cmath>

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

void Projectile::setTint(const Color& core, const Color& glow, const Color& trail) noexcept {
    core_.setFillColor(core);
    glow_.setFillColor(glow);
    trailColor_ = trail;
}

FloatRect Projectile::getLocalBounds() const {
    return {{-kRadius, -kRadius}, {kRadius * 2.f, kRadius * 2.f}};
}

void Projectile::update(float dt) {
    if (consumed_) return;
    trail_.push_front(getPosition());
    if (trail_.size() > kTrailLength) trail_.pop_back();

    if (accelPerSec_ > 0.f) {
        const float speedSq = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
        if (speedSq > 0.0001f) {
            const float speed = std::sqrt(speedSq);
            const float newSpeed = speed + accelPerSec_ * dt;
            const float scale = newSpeed / speed;
            velocity_.x *= scale;
            velocity_.y *= scale;
        }
    }

    sf::Vector2f desired = velocity_ * dt;
    bool bounced = false;


    if (obstacles_ && !obstacles_->empty()) {
        const auto gb = getGlobalBounds();
        core::AABB entityAABB = core::fromFloatRect(gb);
        const sf::Vector2f resolved = core::resolveMovement(entityAABB, desired, *obstacles_);
        const bool blockedX = (desired.x != 0.f && resolved.x == 0.f);
        const bool blockedY = (desired.y != 0.f && resolved.y == 0.f);
        if (blockedX) {
            velocity_.x = -velocity_.x;
            bounced = true;
        }
        if (blockedY) {
            velocity_.y = -velocity_.y;
            bounced = true;
        }
        desired = resolved;
    }

    if (bounced) {
        ++bounceCount_;
        if (bounceCount_ > maxBounces_) {
            markConsumed();
            return;
        }
    }

    move(desired);
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
        ghost.setFillColor(Color(trailColor_.r, trailColor_.g, trailColor_.b,
                                 static_cast<std::uint8_t>(trailColor_.a * t)));
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