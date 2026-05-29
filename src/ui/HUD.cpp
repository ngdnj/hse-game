#include "ui/HUD.hpp"

#include <array>
#include <algorithm>
#include <cmath>
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
        // Fallbacks for non-macOS environments (the default path is macOS).
        static const std::array<const char*, 3> kFallbacks{
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // common on Linux
            "/usr/share/fonts/TTF/DejaVuSans.ttf",             // some distros
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf"  // alternative
        };
        for (const auto* p : kFallbacks) {
            if (font_.openFromFile(p)) {
                hasFont_ = true;
                std::cerr << "[HUD] Loaded fallback font: " << p << "\n";
                break;
            }
        }
    }
    if (!hasFont_) {
        std::cerr << "[HUD] Could not load font (no text will be drawn). Tried: "
                  << fontPath << "\n";
    }
}

void HUD::draw(sf::RenderTarget& target, const HudState& state) const {
    drawText(target, {10.f, 10.f},
             "WASD move | SPACE attack | Q swap weapon | LShift dash | E pick up loot", 16);

    drawText(target, {10.f, 30.f},
             "Wave: " + std::to_string(state.waveNumber) +
             " | Enemies: " + std::to_string(state.enemiesAlive) +
             " | Kills: " + std::to_string(state.killCount) +
             " | Weapon: " + state.weaponName, 16);

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

    drawHealthBar(target, {10.f, 92.f}, state.playerHealth, state.playerMaxHealth);
    drawAmmoBar(target, {10.f, 108.f}, state.ammoCurrent, state.ammoMax, state.ammoInfinite);

    int invY = 132;
    if (state.inventory) {
        state.inventory->forEach([&](const std::string& name, const core::ItemData& data) {
            drawText(target, {10.f, static_cast<float>(invY)},
                     name + " x" + std::to_string(data.stackSize), 14);
            invY += 18;
        });
    }

    const std::string dashLabel = state.dashCooldownRemaining <= 0.f
        ? "Dash: ready"
        : "Dash: " + std::to_string(static_cast<int>(std::ceil(state.dashCooldownRemaining))) + "s";
    drawText(target, {10.f, static_cast<float>(invY + 6)}, dashLabel, 12);

    if (state.shopOpen) {
        drawShopOverlay(target, state);
    }

    if (state.isGameOver) {
        drawGameOver(target, state);
    }

    if (state.isMainMenu) {
        drawMainMenu(target, state);
    }

    if (state.isRoundComplete) {
        drawRoundComplete(target, state);
    }
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

void HUD::drawHealthBar(sf::RenderTarget& target, sf::Vector2f pos,
                         int hp, int maxHp) const {
    const float ratio = maxHp > 0
        ? std::clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.f, 1.f)
        : 0.f;

    sf::RectangleShape bg({kBarWidth, kBarHeight + 4.f});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(40, 40, 40));
    bg.setOutlineColor(sf::Color(120, 120, 120));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    sf::RectangleShape fill({kBarWidth * ratio, kBarHeight + 4.f});
    fill.setPosition(pos);
    const sf::Color color = ratio > 0.5f ? sf::Color(80, 220, 80)
                          : ratio > 0.25f ? sf::Color(230, 200, 60)
                                          : sf::Color(220, 60, 60);
    fill.setFillColor(color);
    target.draw(fill);

    drawText(target, {pos.x + kBarWidth + 8.f, pos.y - 2.f},
             "HP " + std::to_string(hp) + "/" + std::to_string(maxHp), 12);
}

void HUD::drawAmmoBar(sf::RenderTarget& target, sf::Vector2f pos,
                      int ammo, int maxAmmo, bool infinite) const {
    const float ratio = (maxAmmo > 0)
        ? std::clamp(static_cast<float>(ammo) / static_cast<float>(maxAmmo), 0.f, 1.f)
        : (infinite ? 1.f : 0.f);

    sf::RectangleShape bg({kBarWidth, kBarHeight + 4.f});
    bg.setPosition(pos);
    bg.setFillColor(sf::Color(40, 40, 40));
    bg.setOutlineColor(sf::Color(120, 120, 120));
    bg.setOutlineThickness(1.f);
    target.draw(bg);

    sf::RectangleShape fill({kBarWidth * ratio, kBarHeight + 4.f});
    fill.setPosition(pos);
    fill.setFillColor(sf::Color(220, 170, 80));
    target.draw(fill);

    if (infinite) {
        drawText(target, {pos.x + kBarWidth + 8.f, pos.y - 2.f}, "Ammo INF", 12);
    } else if (maxAmmo > 0) {
        drawText(target, {pos.x + kBarWidth + 8.f, pos.y - 2.f},
                 "Ammo " + std::to_string(ammo) + "/" + std::to_string(maxAmmo), 12);
    } else {
        drawText(target, {pos.x + kBarWidth + 8.f, pos.y - 2.f}, "Ammo --", 12);
    }
}

void HUD::drawShopOverlay(sf::RenderTarget& target, const HudState& state) const {
    if (!state.shop) return;

    const sf::Vector2f panelSize{460.f, 280.f};
    const sf::Vector2u winSz = target.getSize();
    const sf::Vector2f panelPos{
        (static_cast<float>(winSz.x) - panelSize.x) * 0.5f,
        (static_cast<float>(winSz.y) - panelSize.y) * 0.5f};

    sf::RectangleShape dim({static_cast<float>(winSz.x), static_cast<float>(winSz.y)});
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    target.draw(dim);

    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(sf::Color(28, 32, 44));
    panel.setOutlineColor(sf::Color(180, 180, 200));
    panel.setOutlineThickness(2.f);
    target.draw(panel);

    drawText(target, {panelPos.x + 16.f, panelPos.y + 12.f},
             "-- SHOP --   Wave " + std::to_string(state.waveNumber) + " cleared", 20);

    const int coins = state.inventory ? state.inventory->count(core::Shop::kCurrency) : 0;
    drawText(target, {panelPos.x + 16.f, panelPos.y + 44.f},
             "Coins: " + std::to_string(coins), 16);

    const float rowY0 = panelPos.y + 80.f;
    const float rowH = 50.f;
    for (std::size_t i = 0; i < state.shop->size(); ++i) {
        const auto& u = state.shop->at(i);
        const float y = rowY0 + rowH * static_cast<float>(i);
        const std::string costStr = u.maxedOut()
            ? "MAX"
            : std::to_string(u.currentCost()) + " coins";
        drawText(target, {panelPos.x + 16.f, y},
                 std::to_string(i + 1) + ") " + u.name +
                 " (lvl " + std::to_string(u.level) + ")  -  " + costStr, 16);
        drawText(target, {panelPos.x + 32.f, y + 20.f}, u.description, 12);
    }

    drawText(target, {panelPos.x + 16.f, panelPos.y + panelSize.y - 28.f},
             "Press 1/2/3 to buy   ENTER to continue", 14);
}

void HUD::drawGameOver(sf::RenderTarget& target, const HudState& state) const {
    const sf::Vector2u winSz = target.getSize();
    const auto winW = static_cast<float>(winSz.x);
    const auto winH = static_cast<float>(winSz.y);

    sf::RectangleShape dim({winW, winH});
    dim.setFillColor(sf::Color(0, 0, 0, 200));
    target.draw(dim);

    const float panelW = std::clamp(winW * 0.6f, 300.f, 520.f);
    const float panelH = std::clamp(winH * 0.45f, 220.f, 320.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{
        (winW - panelSize.x) * 0.5f,
        (winH - panelSize.y) * 0.5f};

    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(sf::Color(40, 14, 14));
    panel.setOutlineColor(sf::Color(220, 60, 60));
    panel.setOutlineThickness(3.f);
    target.draw(panel);

    if (hasFont_) {
        const unsigned titleSize = static_cast<unsigned>(std::clamp(panelH * 0.22f, 28.f, 48.f));
        sf::Text title(font_, sf::String("GAME OVER"), titleSize);
        title.setFillColor(sf::Color(240, 80, 80));
        title.setOutlineColor(sf::Color::Black);
        title.setOutlineThickness(2.f);
        const auto titleBounds = title.getLocalBounds();
        title.setPosition({
            panelPos.x + (panelSize.x - titleBounds.size.x) * 0.5f - titleBounds.position.x,
            panelPos.y + panelSize.y * 0.08f
        });
        target.draw(title);
    }

    drawText(target, {panelPos.x + 20.f, panelPos.y + panelSize.y * 0.36f},
             "Choose an option:", 16);

    drawButton(target, state.gameOverBtnRestart, "Restart", state.gameOverSelected == 0);
    drawButton(target, state.gameOverBtnMenu, "Menu", state.gameOverSelected == 1);
    drawButton(target, state.gameOverBtnExit, "Exit", state.gameOverSelected == 2);

    drawText(target, {panelPos.x + 20.f, panelPos.y + panelSize.y - 28.f},
             "Use Up/Down + Enter, or click", 14);
}

void HUD::drawButton(sf::RenderTarget& target, const sf::FloatRect& r,
                     const std::string& label, bool selected) const {
    sf::RectangleShape btn({r.size.x, r.size.y});
    btn.setPosition(r.position);
    btn.setFillColor(selected ? sf::Color(90, 120, 200) : sf::Color(60, 70, 90));
    btn.setOutlineColor(selected ? sf::Color(220, 220, 255) : sf::Color(160, 160, 180));
    btn.setOutlineThickness(selected ? 3.f : 2.f);
    target.draw(btn);

    if (!hasFont_) return;
    sf::Text t(font_, sf::String(label), 20);
    t.setFillColor(sf::Color::White);
    t.setOutlineColor(sf::Color::Black);
    t.setOutlineThickness(1.f);
    const auto b = t.getLocalBounds();
    t.setPosition({
        r.position.x + (r.size.x - b.size.x) * 0.5f - b.position.x,
        r.position.y + (r.size.y - b.size.y) * 0.5f - b.position.y - 2.f
    });
    target.draw(t);
}

void HUD::drawMainMenu(sf::RenderTarget& target, const HudState& state) const {
    const sf::Vector2u winSz = target.getSize();
    const auto winW = static_cast<float>(winSz.x);
    const auto winH = static_cast<float>(winSz.y);

    sf::RectangleShape dim({winW, winH});
    dim.setFillColor(sf::Color(0, 0, 0, 190));
    target.draw(dim);

    const float panelW = std::clamp(winW * 0.65f, 340.f, 560.f);
    const float panelH = std::clamp(winH * 0.5f, 240.f, 360.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{(winW - panelSize.x) * 0.5f, (winH - panelSize.y) * 0.5f};
    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(sf::Color(24, 28, 38));
    panel.setOutlineColor(sf::Color(180, 180, 200));
    panel.setOutlineThickness(3.f);
    target.draw(panel);

    drawText(target, {panelPos.x + 20.f, panelPos.y + panelSize.y * 0.08f}, "MAIN MENU (TODO)", 28);
    drawText(target, {panelPos.x + 20.f, panelPos.y + panelSize.y * 0.22f}, "This is a placeholder screen.", 16);

    drawButton(target, state.mainMenuBtnStart, "Start", state.mainMenuSelected == 0);
    drawButton(target, state.mainMenuBtnExit, "Exit", state.mainMenuSelected == 1);
}

void HUD::drawRoundComplete(sf::RenderTarget& target, const HudState& state) const {
    const sf::Vector2u winSz = target.getSize();
    const auto winW = static_cast<float>(winSz.x);
    const auto winH = static_cast<float>(winSz.y);

    sf::RectangleShape dim({winW, winH});
    dim.setFillColor(sf::Color(0, 0, 0, 170));
    target.draw(dim);

    const float panelW = std::clamp(winW * 0.6f, 340.f, 560.f);
    const float panelH = std::clamp(winH * 0.45f, 220.f, 320.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{(winW - panelSize.x) * 0.5f, (winH - panelSize.y) * 0.5f};
    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelPos);
    panel.setFillColor(sf::Color(20, 26, 40));
    panel.setOutlineColor(sf::Color(180, 180, 220));
    panel.setOutlineThickness(3.f);
    target.draw(panel);

    drawText(target, {panelPos.x + 18.f, panelPos.y + panelSize.y * 0.1f}, "ROUND CLEARED", 28);
    drawText(target, {panelPos.x + 18.f, panelPos.y + panelSize.y * 0.25f}, "Choose what to do next:", 16);

    drawButton(target, state.roundCompleteBtnNext, "Next round", state.roundCompleteSelected == 0);
    drawButton(target, state.roundCompleteBtnMenu, "Menu", state.roundCompleteSelected == 1);

    drawText(target, {panelPos.x + 18.f, panelPos.y + panelSize.y - 28.f},
             "Use Up/Down + Enter, or click", 14);
}

} // namespace ui
