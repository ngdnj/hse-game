#pragma once

#include "core/Inventory.hpp"
#include "core/Shop.hpp"
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

    int playerHealth{100};
    int playerMaxHealth{100};

    int ammoCurrent{0};
    int ammoMax{0};
    bool ammoInfinite{false};

    float dashCooldownRemaining{0.f};
    float dashCooldownTotal{1.5f};

    const core::Inventory* inventory{nullptr}; // not owned

    bool shopOpen{false};
    const core::Shop* shop{nullptr}; // not owned

    bool isGameOver{false};

    // Game over menu (computed in main loop in screen space)
    sf::FloatRect gameOverBtnRestart{};
    sf::FloatRect gameOverBtnMenu{};
    sf::FloatRect gameOverBtnExit{};
    int gameOverSelected{0}; // 0 restart, 1 menu, 2 exit

    // Placeholder main menu
    bool isMainMenu{false};
    sf::FloatRect mainMenuBtnStart{};
    sf::FloatRect mainMenuBtnExit{};
    int mainMenuSelected{0}; // 0 start, 1 exit

    // Round complete overlay (wave cleared)
    bool isRoundComplete{false};
    sf::FloatRect roundCompleteBtnNext{};
    sf::FloatRect roundCompleteBtnMenu{};
    int roundCompleteSelected{0}; // 0 next, 1 menu

    std::string weaponName{"Sword"};
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
    void drawAmmoBar(sf::RenderTarget& target, sf::Vector2f pos,
                     int ammo, int maxAmmo, bool infinite) const;
    void drawHealthBar(sf::RenderTarget& target, sf::Vector2f pos,
                       int hp, int maxHp) const;
    void drawShopOverlay(sf::RenderTarget& target, const HudState& state) const;
    void drawGameOver(sf::RenderTarget& target, const HudState& state) const;
    void drawMainMenu(sf::RenderTarget& target, const HudState& state) const;
    void drawRoundComplete(sf::RenderTarget& target, const HudState& state) const;
    void drawButton(sf::RenderTarget& target, const sf::FloatRect& r,
                    const std::string& label, bool selected) const;

    sf::Font font_;
    bool hasFont_{false};
};

} // namespace ui
