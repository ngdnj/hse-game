#pragma once

#include "core/Entity.hpp"
#include <SFML/Graphics.hpp>

namespace entities {

class Projectile final : public Entity {
public:
    Projectile(sf::Vector2f pos, sf::Vector2f direction, float speed, int damage);

    void update(float dt) override;
    [[nodiscard]] sf::FloatRect getLocalBounds() const override;
    [[nodiscard]] int damage() const noexcept { return damage_; }
    void markConsumed() noexcept { consumed_ = true; }
    [[nodiscard]] bool consumed() const noexcept { return consumed_; }

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    int damage_{10};
    bool consumed_{false};
    sf::Vector2f velocity_{0.f, 0.f};
    sf::CircleShape circle_;
};

} // namespace entities