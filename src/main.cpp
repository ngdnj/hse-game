#include <SFML/Graphics.hpp>
#include <array>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

#include "core/Collision.hpp"
#include "core/Inventory.hpp"
#include "core/ResourceManager.hpp"
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"

using namespace entities;

namespace {

constexpr float kFixedDt = 1.f / 60.f;  // 60 Hz physics
constexpr int kMaxFrameSkip = 5;        // prevent spiral of death
constexpr float kSpawnInterval = 3.f;   // seconds between enemy spawns
constexpr int kMaxEnemies = 8;

// Simple pseudo-random for spawn positions
std::mt19937& rng() {
    static std::mt19937 rng_{std::random_device{}()};
    return rng_;
}

sf::Vector2f randomEdgePosition(const sf::FloatRect& bounds) {
    std::uniform_real_distribution<float> distX(bounds.position.x,
                                                 bounds.position.x + bounds.size.x - 40.f);
    std::uniform_real_distribution<float> distY(bounds.position.y,
                                                 bounds.position.y + bounds.size.y - 40.f);
    std::uniform_int_distribution<int> side{0, 3};
    switch (side(rng())) {
        case 0: return {distX(rng()), bounds.position.y + 10.f};
        case 1: return {bounds.position.x + bounds.size.x - 40.f, distY(rng())};
        case 2: return {distX(rng()), bounds.position.y + bounds.size.y - 40.f};
        default: return {bounds.position.x + 10.f, distY(rng())};
    }
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({1200u, 800u}), "HSE Game Prototype");
    window.setFramerateLimit(60);

    const sf::Vector2f worldSize{1200.f, 800.f};
    const sf::FloatRect worldBounds{sf::Vector2f{0.f, 0.f}, worldSize};

    // Resource manager for shapes
    core::ResourceManager res;
    res.initDefaults();

    // Create player (no textures -> uses colored shapes)
    Player player(sf::Vector2f{40.f, 40.f}, sf::Vector2f{600.f, 400.f},
                 worldBounds, "", "");
    player.setSpeed(220.f);
    player.setAttackDamage(25);
    player.setAttackRange(55.f);

    // Inventory
    core::Inventory inventory(20);

    // Game state
    std::vector<std::unique_ptr<entities::Enemy>> enemies;
    std::vector<std::unique_ptr<entities::Loot>> loot;

    float spawnTimer = 0.f;
    int killCount = 0;

    // Fixed timestep accumulator
    float accumulator = 0.f;
    sf::Clock gameClock;

    // UI font (use default SFML sans-serif)
    sf::Font uiFont;
    bool hasFont = uiFont.openFromFile("/System/Library/Fonts/Helvetica.ttc");
    if (!hasFont) {
        std::cerr << "[Game] Could not load UI font, text will be invisible\n";
    }

    // Helper: draw text
    auto drawText = [&](sf::RenderTarget& target, const sf::Vector2f& pos,
                         const std::string& str, unsigned size = 16) {
        if (!hasFont) return;
        sf::Text text(uiFont, sf::String(str), size);
        text.setFillColor(sf::Color::White);
        text.setOutlineColor(sf::Color::Black);
        text.setOutlineThickness(1.f);
        text.setPosition(pos);
        target.draw(text);
    };

    while (window.isOpen()) {
        // ---- Input ----
        while (auto eventOpt = window.pollEvent()) {
            if (eventOpt->is<sf::Event::Closed>()) {
                window.close();
            }
            // E key: pickup loot
            if (auto* key = eventOpt->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::E) {
                    for (auto& l : loot) {
                        if (!l->consumed() && l->getGlobalBounds().findIntersection(player.getGlobalBounds()).has_value()) {
                            std::cout << "[Loot] Picked up: " << l->itemName()
                                      << " x" << l->value() << "\n";
                            inventory.addItem(l->itemName(), {.stackSize = l->value()});
                            l->consume();
                        }
                    }
                }
            }
        }

        // ---- Variable dt for rendering ----
        const float realDt = gameClock.restart().asSeconds();
        (void)realDt;

        // ---- Fixed update loop ----
        accumulator += realDt;
        int steps = 0;
        while (accumulator >= kFixedDt && steps < kMaxFrameSkip) {
            // Player
            player.update(kFixedDt);

            // Combat: attack triggers a hitbox; check against enemies
            if (player.isAttacking()) {
                const auto& hitbox = player.attackHitbox();
                for (auto& enemy : enemies) {
                    if (!enemy->isAlive()) continue;
                    if (hitbox.findIntersection(enemy->getGlobalBounds()).has_value()) {
                        enemy->takeDamage(player.attackDamage());
                        std::cout << "[Combat] Hit enemy! HP: "
                                  << enemy->health() << "/" << enemy->maxHealth() << "\n";
                        if (enemy->isDead()) {
                            std::cout << "[Combat] Enemy killed!\n";
                            ++killCount;
                            // Drop loot at enemy position
                            loot.push_back(std::make_unique<entities::Loot>(
                                enemy->getPosition(), "coin", 5));
                        }
                    }
                }
            }

            // Player position reference for enemy AI
            const sf::Vector2f playerPos = player.getPosition();

            // Enemies
            for (auto& enemy : enemies) {
                if (!enemy->isAlive()) continue;
                enemy->setPlayerPosition(&playerPos);
                enemy->update(kFixedDt);
            }

            // Loot (remove consumed)
            for (auto& l : loot) {
                if (!l->consumed()) l->update(kFixedDt);
            }

            // Clamp to bounds (enemies and loot already do this internally)
            (void)worldBounds;
            (void)kMaxFrameSkip;
            (void)steps;

            accumulator -= kFixedDt;
            ++steps;
        }

        // Spawn enemies periodically
        spawnTimer += realDt;
        if (spawnTimer >= kSpawnInterval && enemies.size() < kMaxEnemies) {
            spawnTimer = 0.f;
            auto pos = randomEdgePosition(worldBounds);
            enemies.push_back(std::make_unique<entities::Enemy>(
                pos, sf::Vector2f{36.f, 36.f}, worldBounds, &res));
            std::cout << "[Spawn] New enemy at (" << pos.x << "," << pos.y << ")\n";
        }

        // ---- Deferred removal (dead enemies and consumed loot) ----
        {
            auto deadEnemies = std::move(enemies);
            enemies.clear();
            for (auto& e : deadEnemies) {
                if (e->isAlive()) enemies.push_back(std::move(e));
            }
        }
        {
            auto activeLoot = std::move(loot);
            loot.clear();
            for (auto& l : activeLoot) {
                if (!l->consumed()) loot.push_back(std::move(l));
            }
        }

        // ---- Render ----
        window.clear(sf::Color(20, 24, 32));

        // Grid lines (optional visual)
        sf::VertexArray grid(sf::PrimitiveType::Lines);
        for (float x = 0; x < 1200.f; x += 80.f) {
            grid.append(sf::Vertex{{x, 0.f}, sf::Color(60, 64, 72)});
            grid.append(sf::Vertex{{x, 800.f}, sf::Color(60, 64, 72)});
        }
        for (float y = 0; y < 800.f; y += 80.f) {
            grid.append(sf::Vertex{{0.f, y}, sf::Color(60, 64, 72)});
            grid.append(sf::Vertex{{1200.f, y}, sf::Color(60, 64, 72)});
        }
        window.draw(grid);

        // Loot
        for (auto& l : loot) window.draw(*l);

        // Enemies
        for (auto& e : enemies) window.draw(*e);

        // Player
        window.draw(player);

        // UI
        drawText(window, {10.f, 10.f},
                 "WASD move | SPACE attack | E pick up loot", 16);
        drawText(window, {10.f, 30.f},
                 "Enemies: " + std::to_string(enemies.size()) +
                 " | Kills: " + std::to_string(killCount), 16);
        drawText(window, {10.f, 50.f},
                 "Loot in world: " + std::to_string(loot.size()), 16);
        drawText(window, {10.f, 70.f},
                 "Inventory: " + std::to_string(inventory.usedSlots()) +
                 "/" + std::to_string(inventory.capacity()), 16);

        // Print collected inventory
        int invY = 90;
        inventory.forEach([&](const std::string& name, const core::ItemData& data) {
            drawText(window, {10.f, static_cast<float>(invY)},
                     name + " x" + std::to_string(data.stackSize), 14);
            invY += 18;
        });

        window.display();
    }

    std::cout << "[Game] Shutdown. Kills: " << killCount << "\n";
    return 0;
}