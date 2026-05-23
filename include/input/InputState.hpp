#pragma once
#include <SFML/System/Vector2.hpp>

struct InputState {
    sf::Vector2f moveDirection;  // normalized -1..1 on each axis
    bool attack     = false;
    bool dash       = false;
    bool swapWeapon = false;

    static InputState idle() {
        return {{0.f, 0.f}, false, false, false};
    }
};