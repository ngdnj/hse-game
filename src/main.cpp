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
constexpr float kCameraSmoothing = 0.12f; // 0..1, lower = smoother/slower follow

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

// Clamp a view center so the view rectangle stays within world bounds
void clampViewToBounds(sf::View& view, const sf::FloatRect& worldBounds) {
    const sf::Vector2f halfSize = view.getSize() * 0.5f;
    const sf::Vector2f minPos = worldBounds.position + halfSize;
    const sf::Vector2f maxPos{
        worldBounds.position.x + worldBounds.size.x - halfSize.x,
        worldBounds.position.y + worldBounds.size.y - halfSize.y
    };
    const sf::Vector2f currentCenter = view.getCenter();
    view.setCenter({
        std::clamp(currentCenter.x, minPos.x, maxPos.x),
        std::clamp(currentCenter.y, minPos.y, maxPos.y)
    });
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({1200u, 800u}), "HSE Game Prototype");
    window.setFramerateLimit(60);

    const sf::Vector2f worldSize{2000.f, 2000.f};
    const sf::FloatRect worldBounds{sf::Vector2f{0.f, 0.f}, worldSize};

    // Camera
    sf::View camera;
    camera.setSize({1200.f, 800.f});
    camera.setCenter({600.f, 400.f});

    // Resource manager for shapes
    core::ResourceManager res;
    res.initDefaults();

    // Create player (no textures -> uses colored shapes)
    Player player(sf::Vector2f{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f},
                 worldBounds, "", "");
    player.setSpeed(220.f);
    player.setAttackDamage(25);
    player.setAttackRange(55.f);

    // Inventory
    core::Inventory inventory(20);

    // Static obstacles (declaration before setup)
    std::vector<std::unique_ptr<core::Obstacle>> obstacles;
    std::vector<core::AABB> obstacleAABBs;

    // ---- Static obstacles ----
    const auto obstacleColor = sf::Color(80, 100, 80);
    const auto obstacleOutline = sf::Color(50, 60, 50);
    const auto addObs = [&](int id, float x, float y, float w, float h) {
        obstacles.push_back(std::make_unique<core::Obstacle>(core::ObstacleDesc{
            id, {x, y}, {w, h}, obstacleColor, obstacleOutline, 2.f}));
        obstacleAABBs.push_back(obstacles.back()->getAABB());
    };
    // Scattered rocks/walls
    addObs(1, 400.f, 300.f, 80.f, 40.f);
    addObs(2, 600.f, 500.f, 120.f, 40.f);
    addObs(3, 900.f, 200.f, 60.f, 80.f);
    addObs(4, 1400.f, 600.f, 100.f, 50.f);
    addObs(5, 300.f, 800.f, 70.f, 70.f);
    addObs(6, 1100.f, 1100.f, 90.f, 40.f);
    addObs(7, 1600.f, 400.f, 50.f, 100.f);
    addObs(8, 700.f, 1400.f, 80.f, 60.f);
    // Feed obstacles to entities
    player.setObstacles(&obstacleAABBs);

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
            enemies.back()->setObstacles(&obstacleAABBs);
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

        // Set camera view (world space)
        window.setView(camera);

        // Grid lines spanning the full world
        sf::VertexArray grid(sf::PrimitiveType::Lines);
        for (float x = 0.f; x <= 2000.f; x += 80.f) {
            grid.append(sf::Vertex{{x, 0.f}, sf::Color(60, 64, 72)});
            grid.append(sf::Vertex{{x, 2000.f}, sf::Color(60, 64, 72)});
        }
        for (float y = 0.f; y <= 2000.f; y += 80.f) {
            grid.append(sf::Vertex{{0.f, y}, sf::Color(60, 64, 72)});
            grid.append(sf::Vertex{{2000.f, y}, sf::Color(60, 64, 72)});
        }
        window.draw(grid);

        // World boundary outline
        sf::RectangleShape border{{2000.f, 2000.f}};
        border.setPosition({0.f, 0.f});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(100, 100, 100));
        border.setOutlineThickness(4.f);
        window.draw(border);

        // Static obstacles
        for (const auto& obs : obstacles) {
            obs->draw(window, sf::RenderStates::Default);
        }

        // Loot
        for (auto& l : loot) window.draw(*l);

        // Enemies
        for (auto& e : enemies) window.draw(*e);

        // Player
        window.draw(player);

        // UI (back to default view so it stays on screen)
        window.setView(window.getDefaultView());

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

        // Smooth camera follow
        const sf::Vector2f targetCenter = player.getPosition();
        const sf::Vector2f currentCenter = camera.getCenter();
        const sf::Vector2f newCenter{
            currentCenter.x + (targetCenter.x - currentCenter.x) * kCameraSmoothing,
            currentCenter.y + (targetCenter.y - currentCenter.y) * kCameraSmoothing
        };
        camera.setCenter(newCenter);
        clampViewToBounds(camera, worldBounds);
    }

    std::cout << "[Game] Shutdown. Kills: " << killCount << "\n";
    return 0;
}