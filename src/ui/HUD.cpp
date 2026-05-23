#include "ui/HUD.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace ui {

namespace {

constexpr float kBarWidth = 140.f;
constexpr float kBarHeight = 10.f;

} // namespace

HUD::HUD(const std::string& fontPath) {
    hasFont_ = font_.openFromFile(fontPath);
    if (!hasFont_) {
        std::cerr << "[HUD] Could not load font: " << fontPath << "\n";
    }
}

void HUD::draw(sf::RenderTarget& target, const HudState& state) const {
    drawText(target, {10.f, 10.f},
             "WASD move | SPACE attack | LShift dash | E pick up loot", 16);

    drawText(target, {10.f, 30.f},
             "Wave: " + std::to_string(state.waveNumber) +
             " | Enemies: " + std::to_string(state.enemiesAlive) +
             " | Kills: " + std::to_string(state.killCount), 16);

    if (state.waveActive) {
        drawText(target, {10.f, 50.f},
                 "Enemies left: " + std::to_string(state.enemiesRemainingInWave), 16);
    } else {
        drawText(target, {10.f, 50.f},
                 "Next wave in: " + std::to_string(state.waveBreakRemainingSec) + "s", 16);
    }

    const std::size_t invUsed = state.inventory ? state.inventory->usedSlots() : 0;
    const std::size_t invCap = state.inventory ? state.inventory->capacity() : 0;
    drawText(target, {10.f, 70.f},
             "Loot: " + std::to_string(state.lootOnGround) +
             " | Inv: " + std::to_string(invUsed) + "/" + std::to_string(invCap), 16);

    int invY = 90;
    if (state.inventory) {
        state.inventory->forEach([&](const std::string& name, const core::ItemData& data) {
            drawText(target, {10.f, static_cast<float>(invY)},
                     name + " x" + std::to_string(data.stackSize), 14);
            invY += 18;
        });
    }

    drawDashCooldown(target, {10.f, static_cast<float>(invY + 6)},
                     state.dashCooldownRemaining, state.dashCooldownTotal);
}

void HUD::drawText(sf::RenderTarget& target, sf::Vector2f pos,
                   const std::string& str, unsigned size) const {
    if (!hasFont_) return;
    sf::Text text(font_, sf::String(str), size);
    text.setFillColor(sf::Color::White);
    text.setOutlineColor(sf::Color::Black);
    text.setOutlineThickness(1.f);
    text.setPosition(pos);
    target.draw(text);
}

void HUD::drawDashCooldown(sf::RenderTarget& target, sf::Vector2f pos,
                            float remaining, float total) const {
    const float ready = total > 0.f
        ? std::clamp(1.f - (remaining / total), 0.f, 1.f)
        : 1.f;

    sf::RectangleShape bg({kBarWidth, kBarHeight});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(40, 40, 40));
    bg.setOutlineColor(sf::Color(120, 120, 120));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    sf::RectangleShape fill({kBarWidth * ready, kBarHeight});
    fill.setPosition(pos);
    fill.setFillColor(ready >= 1.f ? sf::Color(100, 220, 255) : sf::Color(80, 140, 200));
    target.draw(fill);

    drawText(target, {pos.x + kBarWidth + 8.f, pos.y - 4.f},
             ready >= 1.f ? "DASH READY" : "Dash", 12);
}

} // namespace ui
