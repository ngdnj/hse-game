#include "ui/HUD.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

namespace ui {

namespace {

constexpr float kBarWidth = 140.f;
constexpr float kBarHeight = 10.f;

struct Glyph5x7 {
    char ch;
    std::array<std::uint8_t, 7> rows;
};

const std::vector<Glyph5x7>& glyphs5x7() {
    static const std::vector<Glyph5x7> glyphs{
        Glyph5x7{'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
        Glyph5x7{'B', {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110}},
        Glyph5x7{'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
        Glyph5x7{'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},
        Glyph5x7{'E', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111}},
        Glyph5x7{'F', {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000}},
        Glyph5x7{'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
        Glyph5x7{'H', {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
        Glyph5x7{'I', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111}},
        Glyph5x7{'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
        Glyph5x7{'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
        Glyph5x7{'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
        Glyph5x7{'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
        Glyph5x7{'N', {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}},
        Glyph5x7{'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
        Glyph5x7{'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
        Glyph5x7{'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
        Glyph5x7{'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
        Glyph5x7{'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
        Glyph5x7{'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
        Glyph5x7{'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
        Glyph5x7{'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
        Glyph5x7{'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010}},
        Glyph5x7{'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
        Glyph5x7{'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
        Glyph5x7{'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
        Glyph5x7{'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
        Glyph5x7{'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
        Glyph5x7{'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
        Glyph5x7{'3', {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}},
        Glyph5x7{'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
        Glyph5x7{'5', {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}},
        Glyph5x7{'6', {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
        Glyph5x7{'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
        Glyph5x7{'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
        Glyph5x7{'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110}},
        Glyph5x7{':', {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000}},
        Glyph5x7{'/', {0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b00000, 0b00000}},
        Glyph5x7{'-', {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000}},
        Glyph5x7{'+', {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000}},
        Glyph5x7{'|', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
        Glyph5x7{'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
        Glyph5x7{')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}}
     };
     return glyphs;
 }

const Glyph5x7* findGlyph(char c) {
    for (const auto& g : glyphs5x7()) {
        if (g.ch == c) return &g;
    }
    return nullptr;
}

void drawBitmapText(sf::RenderTarget& target, sf::Vector2f pos,
                    const std::string& text, float scale, sf::Color color) {
    sf::RectangleShape px({scale, scale});
    px.setFillColor(color);
    float cursorX = pos.x;
    const float cursorY = pos.y;
    for (char raw : text) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        if (c == ' ') {
            cursorX += 4.f * scale;
            continue;
        }
        const auto* glyph = findGlyph(c);
        if (!glyph) {
            cursorX += 6.f * scale;
            continue;
        }
        for (std::size_t row = 0; row < glyph->rows.size(); ++row) {
            const std::uint8_t bits = glyph->rows[row];
            for (int col = 0; col < 5; ++col) {
                if ((bits >> (4 - col)) & 0x1) {
                    px.setPosition({cursorX + col * scale, cursorY + row * scale});
                    target.draw(px);
                }
            }
        }
        cursorX += 6.f * scale;
    }
}

} // namespace

HUD::HUD(const std::string& fontPath) {
    hasFont_ = font_.openFromFile(fontPath);
    if (!hasFont_) {
        // Fallbacks for non-macOS environments (the default path is macOS).
        static const std::array<const char*, 8> kFallbacks{
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",        // common on Linux
            "/usr/share/fonts/TTF/DejaVuSans.ttf",                    // some distros
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",        // alternative
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
            "/usr/share/fonts/truetype/ubuntu/Ubuntu-Regular.ttf"
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

    const sf::Vector2u winSz = target.getSize();
    const float barH = kBarHeight + 4.f;
    const float bottomMargin = 18.f;
    const float barGap = 8.f;
    const float barX = (static_cast<float>(winSz.x) - kBarWidth) * 0.5f;
    const float ammoY = static_cast<float>(winSz.y) - bottomMargin - barH;
    const float hpY = ammoY - barH - barGap;
    drawHealthBar(target, {barX, hpY}, state.playerHealth, state.playerMaxHealth);
    drawAmmoBar(target, {barX, ammoY}, state.ammoCurrent, state.ammoMax, state.ammoInfinite);

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
    if (!hasFont_) {
        const float scale = std::max(1.f, std::round(static_cast<float>(size) / 7.f));
        drawBitmapText(target, pos, str, scale, sf::Color::White);
        return;
    }
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

    drawText(target, {pos.x, pos.y - 14.f},
             "HP: " + std::to_string(hp) + "/" + std::to_string(maxHp), 12);
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
        drawText(target, {pos.x, pos.y - 14.f}, "AMMO: INF", 12);
    } else if (maxAmmo > 0) {
        drawText(target, {pos.x, pos.y - 14.f},
                 "AMMO: " + std::to_string(ammo) + "/" + std::to_string(maxAmmo), 12);
    } else {
        drawText(target, {pos.x, pos.y - 14.f}, "AMMO: --", 12);
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

    if (!hasFont_) {
        const float scale = 3.f;
        const float textW = static_cast<float>(label.size()) * 6.f * scale;
        const float textH = 7.f * scale;
        const sf::Vector2f pos{
            r.position.x + (r.size.x - textW) * 0.5f,
            r.position.y + (r.size.y - textH) * 0.5f
        };
        drawBitmapText(target, pos, label, scale, sf::Color::White);
        return;
    }

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

    drawButton(target, state.mainMenuBtnStart, "Return", state.mainMenuSelected == 0);
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
