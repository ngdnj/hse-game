#include "input/PlayerInputController.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <cmath>

InputState PlayerInputController::poll() {
    InputState state = InputState::idle();

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) state.moveDirection.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) state.moveDirection.y += 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) state.moveDirection.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) state.moveDirection.x += 1.f;

    // Normalize diagonal movement
    const float lenSq = state.moveDirection.x * state.moveDirection.x
                       + state.moveDirection.y * state.moveDirection.y;
    if (lenSq > 1.f) {
        const float invLen = 1.f / std::sqrt(lenSq);
        state.moveDirection.x *= invLen;
        state.moveDirection.y *= invLen;
    }

    state.attack     = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    state.dash       = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
    state.swapWeapon = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);

    return state;
}