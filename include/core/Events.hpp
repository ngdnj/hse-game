#pragma once

#include <SFML/System/Vector2.hpp>
#include <cstddef>

namespace core {

struct EnemyKilledEvent {
    int killCount{0};
};

struct AreaDamageRequest {
    sf::Vector2f position;
    float radius{0.f};
    int bonusDamage{0};
    sf::Vector2f source;
};

} // namespace core