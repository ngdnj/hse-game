#pragma once

#include "core/Inventory.hpp"
#include <SFML/Graphics.hpp>
#include <string>

namespace ui {

// Snapshot of values the HUD needs to render in a given frame.
// Keeps HUD decoupled from Player / WaveManager / Inventory internals.
struct HudState {
    int waveNumber{0};
    bool waveActive{false};
    int waveBreakRemainingSec{0};
    int enemiesAlive{0};
    int enemiesRemainingInWave{0};
    int killCount{0};
    int lootOnGround{0};

    float dashCooldownRemaining{0.f};
    float dashCooldownTotal{1.5f};

    const core::Inventory* inventory{nullptr}; // not owned
};

class HUD {
public:
    // fontPath may be empty; if loading fails the HUD draws nothing textual,
    // but bars/backgrounds still render.
    explicit HUD(const std::string& fontPath = "/System/Library/Fonts/Helvetica.ttc");

    void draw(sf::RenderTarget& target, const HudState& state) const;

    [[nodiscard]] bool hasFont() const noexcept { return hasFont_; }

private:
    void drawText(sf::RenderTarget& target, sf::Vector2f pos,
                  const std::string& str, unsigned size = 16) const;
    void drawDashCooldown(sf::RenderTarget& target, sf::Vector2f pos,
                          float remaining, float total) const;

    sf::Font font_;
    bool hasFont_{false};
};

} // namespace ui
