#include <SFML/Graphics.hpp>
#include <array>
#include <iostream>
#include <optional>
#include <random>
#include <vector>
#include <cstdint>

#include "core/Collision.hpp"
#include "core/GameState.hpp"
#include "core/Inventory.hpp"
#include "core/ResourceManager.hpp"
#include "core/Shop.hpp"
#include "core/WaveManager.hpp"
#include "combat/Artifacts.hpp"
#include "combat/ShotgunWeapon.hpp"
#include "combat/SwordWeapon.hpp"
#include "core/EventBus.hpp"
#include "core/Shop.hpp"
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"
#include "entities/ShooterEnemy.hpp"
#include "ui/HUD.hpp"

using namespace entities;

namespace {

constexpr float kFixedDt = 1.f / 60.f;  // 60 Hz physics
constexpr int kMaxFrameSkip = 5;        // prevent spiral of death
constexpr float kCameraSmoothing = 0.12f; // 0..1, lower = smoother/slower follow
constexpr float kLootPickupRadius = 18.f;
constexpr float kLootAttractRadius = 140.f;
constexpr float kLootAttractSpeed = 240.f;
constexpr int kDefaultAmmoMax = 9;
constexpr float kChaserSpeedBase = 100.f;
constexpr float kShooterSpeedBase = 70.f;
constexpr float kEnemySpeedVariance = 0.12f;
constexpr float kEnemySeparationStrength = 0.65f;
constexpr float kEnemySeparationMaxPush = 40.f;

// Simple pseudo-random for spawn positions
std::mt19937& rng() {
    static std::mt19937 rng_{std::random_device{}()};
    return rng_;
}

float variedSpeed(float base) {
    std::uniform_real_distribution<float> dist{-kEnemySpeedVariance, kEnemySpeedVariance};
    const float scaled = base * (1.f + dist(rng()));
    return std::max(10.f, scaled);
}

// Loot drops
constexpr float kHealthDropChance = 0.25f; // 25% chance per kill
constexpr int kChaserHealthDrop = 10;
constexpr int kShooterHealthDrop = 15;
constexpr int kChaserAmmoDrop = 2;
constexpr int kShooterAmmoDrop = 3;

bool rollChance(float p) {
    std::uniform_real_distribution<float> dist{0.f, 1.f};
    return dist(rng()) < std::clamp(p, 0.f, 1.f);
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

    // If the view is larger than the world, lock to the world center to avoid
    // invalid clamp ranges (hi < lo).
    if (maxPos.x < minPos.x || maxPos.y < minPos.y) {
        const sf::Vector2f worldCenter{
            worldBounds.position.x + worldBounds.size.x * 0.5f,
            worldBounds.position.y + worldBounds.size.y * 0.5f
        };
        view.setCenter(worldCenter);
        return;
    }

    const sf::Vector2f currentCenter = view.getCenter();
    view.setCenter({
        std::clamp(currentCenter.x, minPos.x, maxPos.x),
        std::clamp(currentCenter.y, minPos.y, maxPos.y)
    });
}

struct MenuButtons3 {
    sf::FloatRect b0;
    sf::FloatRect b1;
    sf::FloatRect b2;
};

struct MenuButtons2 {
    sf::FloatRect b0;
    sf::FloatRect b1;
};

MenuButtons3 layoutGameOverButtons(const sf::Vector2u& winSz) {
    const float winW = static_cast<float>(winSz.x);
    const float winH = static_cast<float>(winSz.y);
    const float panelW = std::clamp(winW * 0.6f, 300.f, 520.f);
    const float panelH = std::clamp(winH * 0.45f, 220.f, 320.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{(winW - panelSize.x) * 0.5f, (winH - panelSize.y) * 0.5f};

    const float btnW = panelSize.x * 0.6f;
    const float btnH = std::clamp(panelSize.y * 0.14f, 36.f, 52.f);
    const float x = panelPos.x + (panelSize.x - btnW) * 0.5f;
    const float y0 = panelPos.y + panelSize.y * 0.45f;
    const float gap = std::clamp(panelSize.y * 0.06f, 8.f, 14.f);

    return {
        .b0 = {{x, y0}, {btnW, btnH}},
        .b1 = {{x, y0 + (btnH + gap) * 1.f}, {btnW, btnH}},
        .b2 = {{x, y0 + (btnH + gap) * 2.f}, {btnW, btnH}},
    };
}

MenuButtons2 layoutMainMenuButtons(const sf::Vector2u& winSz) {
    const float winW = static_cast<float>(winSz.x);
    const float winH = static_cast<float>(winSz.y);
    const float panelW = std::clamp(winW * 0.65f, 340.f, 560.f);
    const float panelH = std::clamp(winH * 0.5f, 240.f, 360.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{(winW - panelSize.x) * 0.5f, (winH - panelSize.y) * 0.5f};

    const float btnW = panelSize.x * 0.6f;
    const float btnH = std::clamp(panelSize.y * 0.14f, 36.f, 52.f);
    const float x = panelPos.x + (panelSize.x - btnW) * 0.5f;
    const float y0 = panelPos.y + panelSize.y * 0.5f;
    const float gap = std::clamp(panelSize.y * 0.06f, 8.f, 14.f);

    return {
        .b0 = {{x, y0}, {btnW, btnH}},
        .b1 = {{x, y0 + (btnH + gap)}, {btnW, btnH}},
    };
}

MenuButtons2 layoutRoundCompleteButtons(const sf::Vector2u& winSz) {
    const float winW = static_cast<float>(winSz.x);
    const float winH = static_cast<float>(winSz.y);
    const float panelW = std::clamp(winW * 0.6f, 340.f, 560.f);
    const float panelH = std::clamp(winH * 0.45f, 220.f, 320.f);
    const sf::Vector2f panelSize{panelW, panelH};
    const sf::Vector2f panelPos{(winW - panelSize.x) * 0.5f, (winH - panelSize.y) * 0.5f};

    const float btnW = panelSize.x * 0.62f;
    const float btnH = std::clamp(panelSize.y * 0.14f, 36.f, 52.f);
    const float x = panelPos.x + (panelSize.x - btnW) * 0.5f;
    const float y0 = panelPos.y + panelSize.y * 0.45f;
    const float gap = std::clamp(panelSize.y * 0.06f, 8.f, 14.f);

    return {
        .b0 = {{x, y0}, {btnW, btnH}},
        .b1 = {{x, y0 + (btnH + gap)}, {btnW, btnH}},
    };
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({1200u, 800u}), "HSE Game Prototype");
    window.setFramerateLimit(60);

    const sf::Vector2f worldSize{2000.f, 2000.f};
    const sf::FloatRect worldBounds{sf::Vector2f{0.f, 0.f}, worldSize};
    const sf::Vector2f playerSpawn{1000.f, 1000.f};

    // Camera
    sf::View camera;
    camera.setSize({1200.f, 800.f});
    camera.setCenter({600.f, 400.f});

    // UI view (screen-space, updated on resize)
    sf::View uiView(sf::FloatRect({0.f, 0.f},
                                 {static_cast<float>(window.getSize().x),
                                  static_cast<float>(window.getSize().y)}));

    // Resource manager for shapes
    core::ResourceManager res;
    res.initDefaults();

    {
        auto chaserDesc = res.getShape("enemy");
        chaserDesc.texturePath = "assets/enemy/chaser.png";
        chaserDesc.fillColor = sf::Color::White;
        res.registerShape("enemy", chaserDesc);

        auto shooterDesc = res.getShape("shooter");
        shooterDesc.texturePath = "assets/enemy/shooter.png";
        shooterDesc.fillColor = sf::Color::White;
        res.registerShape("shooter", shooterDesc);
        
        auto playerDesc = res.getShape("player");
        playerDesc.texturePath = "assets/player/idle.png";
        playerDesc.fillColor = sf::Color::White;
        res.registerShape("player", playerDesc);
    }

    core::WaveManager waveMgr(res);
    waveMgr.startNextWave();

    // Shop (opens between waves)
    core::Shop shop;

    // Screen shake state: magnitude (px) decays toward 0 each frame; offset
    // is applied to the camera center at render time, then reset for next frame.
    float shakeMagnitude = 0.f;
    constexpr float kShakeDecayPerSec = 9.f;
    constexpr float kShakeMaxMagnitude = 18.f;
    std::uniform_real_distribution<float> shakeDist{-1.f, 1.f};
    auto triggerShake = [&](float mag) {
        shakeMagnitude = std::min(kShakeMaxMagnitude, std::max(shakeMagnitude, mag));
    };

    // Event bus for artifact effects
    core::EventBus eventBus;
    combat::VampiricFang vampiricFang;
    combat::ExplosiveShells explosiveShells;
    eventBus.subscribe<core::EnemyKilledEvent>([&](const core::EnemyKilledEvent& e) {
        vampiricFang.onEnemyKilled(e);
        const int heals = vampiricFang.pendingHeals();
        if (heals > 0) {
            player.heal(heals);
            vampiricFang.consumeHeals();
            std::cout << "[Artifact] Vampiric Fang healed " << heals << " HP\n";
        }
    });
    eventBus.subscribe<core::AreaDamageRequest>([&](const core::AreaDamageRequest& req) {
        explosiveShells.onAreaDamageRequest(req);
        for (const auto& expl : explosiveShells.pendingExplosions()) {
            for (auto& enemy : enemies) {
                if (!enemy->isAlive()) continue;
                const sf::Vector2f ec{enemy->getPosition()};
                const sf::Vector2f delta{ec.x - expl.position.x, ec.y - expl.position.y};
                const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (dist <= expl.radius) {
                    enemy->takeDamage(expl.bonusDamage);
                    std::cout << "[Artifact] Explosive shells dealt " << expl.bonusDamage
                              << " bonus damage!\n";
                    triggerShake(6.f);
                }
            }
            for (auto& shooter : shooters) {
                if (!shooter->isAlive()) continue;
                const sf::Vector2f sc{shooter->getPosition()};
                const sf::Vector2f delta2{sc.x - expl.position.x, sc.y - expl.position.y};
                const float dist2 = std::sqrt(delta2.x * delta2.x + delta2.y * delta2.y);
                if (dist2 <= expl.radius) {
                    shooter->takeDamage(expl.bonusDamage);
                    triggerShake(6.f);
                }
            }
        }
        explosiveShells.clearExplosions();
    });

    // Top-level state machine
    core::GameState gameState = core::GameState::Playing;

    int gameOverSelection = 0; // 0 restart, 1 menu, 2 exit
    int mainMenuSelection = 0; // 0 start, 1 exit
    int roundCompleteSelection = 0; // 0 next, 1 menu

    // Full game reset (used on game over -> R). Keeps loaded sprite atlases.
    auto resetGame = [&]() {
        player.reset(playerSpawn);
        player.setSpeed(220.f);
        player.setWeapon(std::make_unique<combat::SwordWeapon>(25, 55.f));
        enemies.clear();
        shooters.clear();
        loot.clear();
        playerProjectiles.clear();
        inventory = core::Inventory(20);
        killCount = 0;
        ammoCurrent = ammoMax;
        waveManager = core::WaveManager(4.f);
        waveManager.startNextWave();
        shop = core::Shop{};
        eventBus.reset();
        vampiricFang.reset();
        explosiveShells.reset();
        generateTerrain();
        generateObstacles();
        gameState = core::GameState::Playing;
        std::cout << "[Game] Restarted\n";
    };

    auto enterMainMenu = [&]() {
        // Minimal placeholder: clear world and show menu overlay.
        player.reset(playerSpawn);
        player.setSpeed(220.f);
        player.setWeapon(std::make_unique<combat::SwordWeapon>(25, 55.f));
        enemies.clear();
        shooters.clear();
        loot.clear();
        playerProjectiles.clear();
        inventory = core::Inventory(20);
        killCount = 0;
        waveManager = core::WaveManager(4.f);
        shop = core::Shop{};
        eventBus.reset();
        vampiricFang.reset();
        explosiveShells.reset();
        generateTerrain();
        generateObstacles();
        gameOverSelection = 0;
        mainMenuSelection = 0;
        roundCompleteSelection = 0;
        gameState = core::GameState::MainMenu;
        std::cout << "[Menu] Entered main menu (placeholder)\n";
    };

    auto startNextRound = [&]() {
        gameState = core::GameState::Playing;
        generateTerrain();
        generateObstacles();
        waveManager.startNextWave();
        std::cout << "[Wave] Wave " << waveManager.waveNumber() << " started!\n";
    };

    // Fixed timestep accumulator
    float accumulator = 0.f;
    sf::Clock gameClock;

    ui::HUD hud;

    while (window.isOpen()) {
        // ---- Input ----
        while (auto eventOpt = window.pollEvent()) {
            if (eventOpt->is<sf::Event::Closed>()) {
                window.close();
            }
            if (auto* resized = eventOpt->getIf<sf::Event::Resized>()) {
                const sf::Vector2f newSize{
                    static_cast<float>(resized->size.x),
                    static_cast<float>(resized->size.y)
                };
                camera.setSize(newSize);
                clampViewToBounds(camera, worldBounds);

                uiView.setSize(newSize);
                uiView.setCenter({newSize.x * 0.5f, newSize.y * 0.5f});
            }
            if (auto* mouse = eventOpt->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    const auto mousePx = sf::Vector2i{mouse->position};
                    const sf::Vector2f mouseUi = window.mapPixelToCoords(mousePx, uiView);

                    if (gameState == core::GameState::GameOver) {
                        const auto btn = layoutGameOverButtons(window.getSize());
                        if (btn.b0.contains(mouseUi)) resetGame();
                        else if (btn.b1.contains(mouseUi)) enterMainMenu();
                        else if (btn.b2.contains(mouseUi)) window.close();
                    } else if (gameState == core::GameState::MainMenu) {
                        const auto btn = layoutMainMenuButtons(window.getSize());
                        if (btn.b0.contains(mouseUi)) resetGame();
                        else if (btn.b1.contains(mouseUi)) window.close();
                    } else if (gameState == core::GameState::RoundComplete) {
                        const auto btn = layoutRoundCompleteButtons(window.getSize());
                        if (btn.b0.contains(mouseUi)) startNextRound();
                        else if (btn.b1.contains(mouseUi)) enterMainMenu();
                    }
                }
            }
            if (auto* key = eventOpt->getIf<sf::Event::KeyPressed>()) {
                if (gameState == core::GameState::MainMenu) {
                    if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
                        mainMenuSelection = (mainMenuSelection + 1) % 2;
                    } else if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                        mainMenuSelection = (mainMenuSelection + 1) % 2;
                    } else if (key->code == sf::Keyboard::Key::Enter) {
                        if (mainMenuSelection == 0) resetGame();
                        else window.close();
                    }
                } else if (gameState == core::GameState::Shop) {
                    // Shop input: 1/2/3 buy, Enter close
                    auto tryBuy = [&](std::size_t idx) {
                        if (shop.buy(idx, player, inventory)) {
                            std::cout << "[Shop] Bought " << shop.at(idx).name
                                      << " -> lvl " << shop.at(idx).level << "\n";
                        } else {
                            std::cout << "[Shop] Cannot buy " << shop.at(idx).name << "\n";
                        }
                    };
                    if (key->code == sf::Keyboard::Key::Num1) tryBuy(0);
                    else if (key->code == sf::Keyboard::Key::Num2) tryBuy(1);
                    else if (key->code == sf::Keyboard::Key::Num3) tryBuy(2);
                    else if (key->code == sf::Keyboard::Key::Enter) {
                        gameState = core::GameState::Playing;
                        waveManager.startNextWave();
                        std::cout << "[Wave] Wave " << waveManager.waveNumber()
                                  << " started!\n";
                    }
                } else if (gameState == core::GameState::GameOver) {
                    if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
                        gameOverSelection = (gameOverSelection + 2) % 3;
                    } else if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                        gameOverSelection = (gameOverSelection + 1) % 3;
                    } else if (key->code == sf::Keyboard::Key::Num1) {
                        resetGame();
                    } else if (key->code == sf::Keyboard::Key::Num2) {
                        enterMainMenu();
                    } else if (key->code == sf::Keyboard::Key::Num3) {
                        window.close();
                    } else if (key->code == sf::Keyboard::Key::R) {
                        resetGame();
                    } else if (key->code == sf::Keyboard::Key::Enter) {
                        if (gameOverSelection == 0) resetGame();
                        else if (gameOverSelection == 1) enterMainMenu();
                        else window.close();
                    }
                } else if (gameState == core::GameState::RoundComplete) {
                    if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W) {
                        roundCompleteSelection = (roundCompleteSelection + 1) % 2;
                    } else if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S) {
                        roundCompleteSelection = (roundCompleteSelection + 1) % 2;
                    } else if (key->code == sf::Keyboard::Key::Num1) {
                        startNextRound();
                    } else if (key->code == sf::Keyboard::Key::Num2) {
                        enterMainMenu();
                    } else if (key->code == sf::Keyboard::Key::Enter) {
                        if (roundCompleteSelection == 0) startNextRound();
                        else enterMainMenu();
                    }
                } else {
                    // Playing: E key to pick up loot, Q toggles weapon
                    if (key->code == sf::Keyboard::Key::E) {
                        for (auto& l : loot) {
                            if (!l->consumed() && l->getGlobalBounds().findIntersection(player.getGlobalBounds()).has_value()) {
                                if (l->itemName() == "hp" || l->itemName() == "ammo") continue;
                                std::cout << "[Loot] Picked up: " << l->itemName()
                                          << " x" << l->value() << "\n";
                                inventory.addItem(l->itemName(), {.stackSize = l->value()});
                                l->consume();
                            }
                        }
                    } else if (key->code == sf::Keyboard::Key::Q) {
                        if (player.weapon() && player.weapon()->name() == "Sword") {
                            player.setWeapon(std::make_unique<combat::ShotgunWeapon>());
                            std::cout << "[Weapon] Switched to Shotgun\n";
                        } else {
                            player.setWeapon(std::make_unique<combat::SwordWeapon>(25, 55.f));
                            std::cout << "[Weapon] Switched to Sword\n";
                        }
                    }
                }
            }
        }

        // ---- Variable dt for rendering ----
        const float realDt = gameClock.restart().asSeconds();
        (void)realDt;

        // ---- Fixed update loop (paused unless Playing) ----
        if (gameState == core::GameState::Playing) {
        accumulator += realDt;
        int steps = 0;
        while (accumulator >= kFixedDt && steps < kMaxFrameSkip) {
            // Build player obstacle list = static obstacles + all alive enemies.
            playerDynamicObstacles.clear();
            playerDynamicObstacles.insert(playerDynamicObstacles.end(),
                                          obstacleAABBs.begin(), obstacleAABBs.end());
            for (const auto& e : enemies) {
                if (e && e->isAlive()) {
                    playerDynamicObstacles.push_back(core::fromFloatRect(e->getGlobalBounds()));
                }
            }
            for (const auto& s : shooters) {
                if (s && s->isAlive()) {
                    playerDynamicObstacles.push_back(core::fromFloatRect(s->getGlobalBounds()));
                }
            }
            player.setObstacles(&playerDynamicObstacles);

            // Player
            player.update(kFixedDt);

            // Build enemy obstacle list = static obstacles + player.
            enemyDynamicObstacles.clear();
            enemyDynamicObstacles.insert(enemyDynamicObstacles.end(),
                                         obstacleAABBs.begin(), obstacleAABBs.end());
            const core::AABB playerAabb = core::fromFloatRect(player.getGlobalBounds());
            enemyDynamicObstacles.push_back(playerAabb);

            // Auto-pickup: health drops are consumed on contact (no E needed).
            for (auto& l : loot) {
                if (l->consumed()) continue;
                const bool isAuto = (l->itemName() == "hp" || l->itemName() == "ammo");
                if (!isAuto) continue;
                const sf::Vector2f toPlayer = player.getPosition() - l->getPosition();
                const float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
                const float attractSq = kLootAttractRadius * kLootAttractRadius;
                const float pickupSq = kLootPickupRadius * kLootPickupRadius;

                if (distSq <= attractSq) {
                    l->setAttracted(true);
                    const float dist = std::sqrt(distSq);
                    if (dist > 0.001f) {
                        const sf::Vector2f dir{toPlayer.x / dist, toPlayer.y / dist};
                        const float t = 1.f - std::min(dist / kLootAttractRadius, 1.f);
                        const float speed = kLootAttractSpeed * (0.4f + 0.6f * t);
                        l->move({dir.x * speed * kFixedDt, dir.y * speed * kFixedDt});
                    }
                } else {
                    l->setAttracted(false);
                }

                if (distSq > pickupSq) continue;

                if (l->itemName() == "hp") {
                    const int before = player.health();
                    player.heal(l->value());
                    const int healed = player.health() - before;
                    std::cout << "[Loot] Auto-picked: hp +" << healed << "\n";
                } else {
                    const int before = ammoCurrent;
                    ammoCurrent = std::min(ammoMax, ammoCurrent + l->value());
                    const int gained = ammoCurrent - before;
                    std::cout << "[Loot] Auto-picked: ammo +" << gained << "\n";
                }
                l->consume();
            }

            // Combat: fire active weapon if Space is held and weapon is ready.
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)
                && player.weapon() && player.weapon()->canFire()) {
                const bool isShotgun = (player.weapon()->name() == "Shotgun");
                if (isShotgun && ammoCurrent <= 0) {
                    // Out of ammo; skip firing.
                } else {
                    auto result = player.tryFire();
                    const float kb = result.knockbackMultiplier;
                    if (result.hasMeleeHitbox) {
                        const auto& hitbox = result.meleeHitbox;
                        for (auto& enemy : enemies) {
                            if (!enemy->isAlive()) continue;
                            if (hitbox.findIntersection(enemy->getGlobalBounds()).has_value()) {
                                enemy->takeDamage(player.attackDamage());
                                triggerShake(4.f);
                                const sf::Vector2f enemyPos = enemy->getPosition();
                                const sf::Vector2f playerPos = player.getPosition();
                                const sf::Vector2f toEnemy = enemyPos - playerPos;
                                const float dist = std::sqrt(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y);
                                if (dist > 0.001f) {
                                    const sf::Vector2f dir{toEnemy.x / dist, toEnemy.y / dist};
                                    enemy->applyKnockback({dir.x * 300.f * kb, dir.y * 300.f * kb});
                                }
                                std::cout << "[Combat] Hit chaser! HP: "
                                          << enemy->health() << "/" << enemy->maxHealth() << "\n";
                                if (enemy->isDead()) {
                                    std::cout << "[Combat] Chaser killed!\n";
                                    eventBus.emit(core::EnemyKilledEvent{++killCount});
                                    eventBus.emit(core::AreaDamageRequest{
                                        enemy->getPosition(), 70.f, 20, player.getPosition()});
                                    triggerShake(7.f);
                                    waveManager.onEnemyKilled();
                                    loot.push_back(std::make_unique<entities::Loot>(
                                        enemy->getPosition(), "ammo", kChaserAmmoDrop));
                                    if (rollChance(kHealthDropChance)) {
                                        loot.push_back(std::make_unique<entities::Loot>(
                                            enemy->getPosition(), "hp", kChaserHealthDrop));
                                    }
                                }
                            }
                        }
                        for (auto& shooter : shooters) {
                            if (!shooter->isAlive()) continue;
                            if (hitbox.findIntersection(shooter->getGlobalBounds()).has_value()) {
                                shooter->takeDamage(player.attackDamage());
                                triggerShake(4.f);
                                const sf::Vector2f shooterPos = shooter->getPosition();
                                const sf::Vector2f toShooter = shooterPos - player.getPosition();
                                const float dist = std::sqrt(toShooter.x * toShooter.x + toShooter.y * toShooter.y);
                                if (dist > 0.001f) {
                                    const sf::Vector2f dir{toShooter.x / dist, toShooter.y / dist};
                                    shooter->applyKnockback({dir.x * 250.f * kb, dir.y * 250.f * kb});
                                }
                                std::cout << "[Combat] Hit shooter! HP: "
                                          << shooter->health() << "/" << shooter->maxHealth() << "\n";
                                if (shooter->isDead()) {
                                    std::cout << "[Combat] Shooter killed!\n";
                                    eventBus.emit(core::EnemyKilledEvent{++killCount});
                                    eventBus.emit(core::AreaDamageRequest{
                                        shooter->getPosition(), 70.f, 20, player.getPosition()});
                                    triggerShake(8.f);
                                    waveManager.onEnemyKilled();
                                    loot.push_back(std::make_unique<entities::Loot>(
                                        shooter->getPosition(), "ammo", kShooterAmmoDrop));
                                    if (rollChance(kHealthDropChance)) {
                                        loot.push_back(std::make_unique<entities::Loot>(
                                            shooter->getPosition(), "hp", kShooterHealthDrop));
                                    }
                                }
                            }
                        }
                    }
                    for (auto& proj : result.projectiles) {
                        proj->setObstacles(&obstacleAABBs);
                        proj->setMaxBounces(2);
                        playerProjectiles.push_back(std::move(proj));
                    }
                    if (!result.projectiles.empty() || result.hasMeleeHitbox) {
                        if (isShotgun && !result.projectiles.empty()) {
                            ammoCurrent = std::max(0, ammoCurrent - 1);
                        }
                        // Weak shake just for firing weight on shotgun.
                        triggerShake(result.hasMeleeHitbox ? 0.f : 5.f);
                    }
                }
            }

            std::vector<std::vector<core::AABB>> enemyObstacleLists(enemies.size());
            std::vector<std::vector<core::AABB>> shooterObstacleLists(shooters.size());

            // Player position reference for enemy AI
            const sf::Vector2f playerPos = player.getPosition();

            // Enemies
            for (std::size_t i = 0; i < enemies.size(); ++i) {
                auto& enemy = enemies[i];
                if (!enemy->isAlive()) continue;
                enemy->setPlayerPosition(&playerPos);

                auto& obs = enemyObstacleLists[i];
                obs = enemyDynamicObstacles;
                for (std::size_t j = 0; j < enemies.size(); ++j) {
                    if (i == j || !enemies[j]->isAlive()) continue;
                    obs.push_back(core::fromFloatRect(enemies[j]->getGlobalBounds()));
                }
                for (const auto& s : shooters) {
                    if (s && s->isAlive()) {
                        obs.push_back(core::fromFloatRect(s->getGlobalBounds()));
                    }
                }
                enemy->setObstacles(&obs);
                enemy->update(kFixedDt);
            }

            // Resolve enemy melee strikes (wind-up completed).
            for (auto& enemy : enemies) {
                if (!enemy->isAlive() || player.isDead()) continue;
                const auto atk = enemy->consumePendingAttack();
                if (!atk) continue;
                const sf::Vector2f toPlayer = player.getPosition() - atk->origin;
                const float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
                if (distSq <= atk->radius * atk->radius) {
                    // Sector check via dot product: cos(theta) = dot(a,b)
                    // where both vectors are unit. Compare to cos(halfAngle).
                    const float len = std::sqrt(distSq);
                    sf::Vector2f dirToPlayer{0.f, 0.f};
                    if (len > 0.0001f) {
                        dirToPlayer = {toPlayer.x / len, toPlayer.y / len};
                    }
                    const float dot = atk->direction.x * dirToPlayer.x + atk->direction.y * dirToPlayer.y;
                    const float cosHalf = std::cos(atk->halfAngleRad);
                    if (dot >= cosHalf) {
                        player.takeDamage(atk->damage);
                        triggerShake(9.f);
                        std::cout << "[Combat] Player hit by chaser melee! HP: "
                                  << player.health() << "/" << player.maxHealth() << "\n";
                        if (player.isDead()) {
                            gameState = core::GameState::GameOver;
                            gameOverSelection = 0;
                            std::cout << "[Game] Player died -- GAME OVER\n";
                        }
                    }
                }
            }

            // Shooters
            for (std::size_t i = 0; i < shooters.size(); ++i) {
                auto& shooter = shooters[i];
                if (!shooter->isAlive()) continue;
                shooter->setPlayerPosition(&playerPos);

                auto& obs = shooterObstacleLists[i];
                obs = enemyDynamicObstacles;
                for (const auto& e : enemies) {
                    if (e && e->isAlive()) {
                        obs.push_back(core::fromFloatRect(e->getGlobalBounds()));
                    }
                }
                for (std::size_t j = 0; j < shooters.size(); ++j) {
                    if (i == j || !shooters[j]->isAlive()) continue;
                    obs.push_back(core::fromFloatRect(shooters[j]->getGlobalBounds()));
                }
                shooter->setObstacles(&obs);
                shooter->update(kFixedDt);
            }

            // Separation pass: gently push enemies away from each other.
            std::vector<sf::Vector2f> centers;
            std::vector<float> radii;
            std::vector<std::size_t> enemyIndex(enemies.size());
            std::vector<std::size_t> shooterIndex(shooters.size());
            centers.reserve(enemies.size() + shooters.size());
            radii.reserve(enemies.size() + shooters.size());

            auto addCenter = [&](const auto& entity) {
                const auto gb = entity->getGlobalBounds();
                centers.push_back({gb.position.x + gb.size.x * 0.5f,
                                   gb.position.y + gb.size.y * 0.5f});
                radii.push_back(0.5f * std::max(gb.size.x, gb.size.y));
            };

            for (std::size_t i = 0; i < enemies.size(); ++i) {
                if (!enemies[i]->isAlive()) {
                    enemyIndex[i] = static_cast<std::size_t>(-1);
                    continue;
                }
                enemyIndex[i] = centers.size();
                addCenter(enemies[i]);
            }
            for (std::size_t i = 0; i < shooters.size(); ++i) {
                if (!shooters[i]->isAlive()) {
                    shooterIndex[i] = static_cast<std::size_t>(-1);
                    continue;
                }
                shooterIndex[i] = centers.size();
                addCenter(shooters[i]);
            }

            auto computeSeparation = [&](std::size_t idx) {
                sf::Vector2f sep{0.f, 0.f};
                const auto selfPos = centers[idx];
                const float selfR = radii[idx];
                for (std::size_t j = 0; j < centers.size(); ++j) {
                    if (j == idx) continue;
                    const sf::Vector2f delta{selfPos.x - centers[j].x, selfPos.y - centers[j].y};
                    const float distSq = delta.x * delta.x + delta.y * delta.y;
                    const float minDist = selfR + radii[j] + 2.f;
                    if (distSq >= minDist * minDist) continue;
                    const float dist = std::sqrt(std::max(distSq, 0.0001f));
                    const float overlap = minDist - dist;
                    const sf::Vector2f dir{delta.x / dist, delta.y / dist};
                    sep.x += dir.x * overlap;
                    sep.y += dir.y * overlap;
                }
                return sep;
            };

            for (std::size_t i = 0; i < enemies.size(); ++i) {
                if (!enemies[i]->isAlive()) continue;
                const std::size_t idx = enemyIndex[i];
                if (idx == static_cast<std::size_t>(-1)) continue;
                sf::Vector2f sep = computeSeparation(idx);
                const float len = std::sqrt(sep.x * sep.x + sep.y * sep.y);
                if (len > 0.001f) {
                    const float scale = std::min(kEnemySeparationMaxPush, len) / len;
                    sep.x *= scale * kEnemySeparationStrength * kFixedDt;
                    sep.y *= scale * kEnemySeparationStrength * kFixedDt;
                    const sf::Vector2f resolved = enemies[i]->resolveMove(sep);
                    if (resolved.x != 0.f || resolved.y != 0.f) {
                        enemies[i]->move(resolved);
                    }
                }
            }

            for (std::size_t i = 0; i < shooters.size(); ++i) {
                if (!shooters[i]->isAlive()) continue;
                const std::size_t idx = shooterIndex[i];
                if (idx == static_cast<std::size_t>(-1)) continue;
                sf::Vector2f sep = computeSeparation(idx);
                const float len = std::sqrt(sep.x * sep.x + sep.y * sep.y);
                if (len > 0.001f) {
                    const float scale = std::min(kEnemySeparationMaxPush, len) / len;
                    sep.x *= scale * kEnemySeparationStrength * kFixedDt;
                    sep.y *= scale * kEnemySeparationStrength * kFixedDt;
                    const sf::Vector2f resolved = shooters[i]->resolveMove(sep);
                    if (resolved.x != 0.f || resolved.y != 0.f) {
                        shooters[i]->move(resolved);
                    }
                }
            }

            // Projectile-player collision
            for (auto& shooter : shooters) {
                for (auto& proj : shooter->projectiles()) {
                    if (proj->consumed()) continue;
                    if (!proj->getGlobalBounds().findIntersection(player.getGlobalBounds()).has_value()) {
                        continue;
                    }
                    if (proj->isHealing()) {
                        const int before = player.health();
                        player.heal(proj->healingAmount());
                        const int healed = player.health() - before;
                        if (healed > 0) {
                            std::cout << "[Combat] Caught healing shot! +" << healed << " HP\n";
                        }
                        proj->markConsumed();
                        continue;
                    }
                    player.takeDamage(proj->damage());
                    triggerShake(10.f);
                    std::cout << "[Combat] Player hit by projectile! HP: "
                              << player.health() << "/" << player.maxHealth() << "\n";
                    proj->markConsumed();
                    if (player.isDead()) {
                        gameState = core::GameState::GameOver;
                        gameOverSelection = 0;
                        std::cout << "[Game] Player died -- GAME OVER\n";
                    }
                }
            }

            // Player projectiles (pellets) — update + collide vs enemies
            for (auto& proj : playerProjectiles) proj->update(kFixedDt);
            for (auto& proj : playerProjectiles) {
                if (proj->consumed()) continue;
                const auto projBounds = proj->getGlobalBounds();
                auto applyHit = [&](auto& target, float kbForce, int ammoValue,
                                     const char* label) {
                    if (!target->isAlive()) return false;
                    if (!projBounds.findIntersection(target->getGlobalBounds()).has_value()) {
                        return false;
                    }
                    target->takeDamage(proj->damage());
                    triggerShake(3.f);
                    const sf::Vector2f to = target->getPosition() - player.getPosition();
                    const float dist = std::sqrt(to.x * to.x + to.y * to.y);
                    if (dist > 0.001f) {
                        const sf::Vector2f dir{to.x / dist, to.y / dist};
                        target->applyKnockback({dir.x * kbForce, dir.y * kbForce});
                    }
                    if (target->isDead()) {
                        ++killCount;
                        eventBus.emit(core::EnemyKilledEvent{killCount});
                        eventBus.emit(core::AreaDamageRequest{
                            target->getPosition(), 70.f, 20, player.getPosition()});
                        triggerShake(7.f);
                        waveManager.onEnemyKilled();
                        loot.push_back(std::make_unique<entities::Loot>(
                            target->getPosition(), "ammo", ammoValue));
                        const int hpDrop = (ammoValue >= kShooterAmmoDrop)
                            ? kShooterHealthDrop
                            : kChaserHealthDrop;
                        if (rollChance(kHealthDropChance)) {
                            loot.push_back(std::make_unique<entities::Loot>(
                                target->getPosition(), "hp", hpDrop));
                        }
                        std::cout << "[Combat] " << label << " killed by pellet!\n";
                    }
                    proj->markConsumed();
                    return true;
                };
                for (auto& enemy : enemies) {
                    if (applyHit(enemy, 220.f, kChaserAmmoDrop, "Chaser")) break;
                }
                if (proj->consumed()) continue;
                for (auto& shooter : shooters) {
                    if (applyHit(shooter, 220.f, kShooterAmmoDrop, "Shooter")) break;
                }
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

        // Wave system update
        waveManager.update(realDt);
        if (waveManager.canStartNextWave()) {
            waveManager.startNextWave();
            std::cout << "[Wave] Wave " << waveManager.waveNumber() << " started!\n";
        }

        // Spawn from wave
        const int totalAlive = static_cast<int>(enemies.size()) + static_cast<int>(shooters.size());
        const int toSpawn = waveManager.enemiesToSpawnNow(totalAlive);
        if (toSpawn > 0) {
            const auto positions = waveManager.spawnPositions(
                worldBounds, randomEdgePosition, toSpawn);
            for (const auto& pos : positions) {
                if (enemies.size() <= shooters.size()) {
                    enemies.push_back(std::make_unique<entities::Enemy>(
                        pos, sf::Vector2f{36.f, 36.f}, worldBounds, &res));
                    enemies.back()->setObstacles(&obstacleAABBs);
                    enemies.back()->setChaseSpeed(variedSpeed(kChaserSpeedBase));
                    std::cout << "[Spawn] Chaser at (" << pos.x << "," << pos.y << ")\n";
                } else {
                    shooters.push_back(std::make_unique<entities::ShooterEnemy>(
                        pos, worldBounds, &res));
                    shooters.back()->setObstacles(&obstacleAABBs);
                    shooters.back()->setProjectileObstacles(&obstacleAABBs);
                    shooters.back()->setChaseSpeed(variedSpeed(kShooterSpeedBase));
                    std::cout << "[Spawn] Shooter at (" << pos.x << "," << pos.y << ")\n";
                }
            }
        }

         // Open round-complete menu when the wave is fully cleared.
         // Note: we do deferred removal AFTER the update step, so containers may still hold
         // dead entities here. Check for "no alive enemies" instead of .empty().
         if (waveManager.waveActive() && waveManager.enemiesRemaining() == 0) {
             bool anyAlive = false;
             for (const auto& e : enemies) {
                 if (e && e->isAlive()) {
                     anyAlive = true;
                     break;
                 }
             }
             if (!anyAlive) {
                 for (const auto& s : shooters) {
                     if (s && s->isAlive()) {
                         anyAlive = true;
                         break;
                     }
                 }
             }

             if (!anyAlive) {
                 waveManager.forceClear();
                 gameState = core::GameState::RoundComplete;
                 roundCompleteSelection = 0;
                 std::cout << "[Wave] Wave " << waveManager.waveNumber()
                           << " cleared -- round complete\n";
             }
         }
        } // end if (gameState == Playing)

        // ---- Deferred removal (dead enemies and consumed loot) ----
        {
            auto deadEnemies = std::move(enemies);
            enemies.clear();
            for (auto& e : deadEnemies) {
                if (e->isAlive()) enemies.push_back(std::move(e));
            }
        }
        {
            auto deadShooters = std::move(shooters);
            shooters.clear();
            for (auto& s : deadShooters) {
                if (s->isAlive()) shooters.push_back(std::move(s));
            }
        }
        {
            auto activeLoot = std::move(loot);
            loot.clear();
            for (auto& l : activeLoot) {
                if (!l->consumed()) loot.push_back(std::move(l));
            }
        }
        {
            auto activeProj = std::move(playerProjectiles);
            playerProjectiles.clear();
            for (auto& p : activeProj) {
                if (p->consumed()) continue;
                // Despawn pellets that left the world
                const auto gb = p->getGlobalBounds();
                if (gb.position.x + gb.size.x < worldBounds.position.x ||
                    gb.position.y + gb.size.y < worldBounds.position.y ||
                    gb.position.x > worldBounds.position.x + worldBounds.size.x ||
                    gb.position.y > worldBounds.position.y + worldBounds.size.y) {
                    continue;
                }
                playerProjectiles.push_back(std::move(p));
            }
        }

        // ---- Render ----
        window.clear(sf::Color(20, 24, 32));

        // Decay shake and apply a temporary random offset to the camera for
        // this frame only. Magnitude decays exponentially so impacts feel
        // punchy but settle quickly.
        const sf::Vector2f preShakeCenter = camera.getCenter();
        sf::Vector2f shakeOffset{0.f, 0.f};
        if (shakeMagnitude > 0.f) {
            shakeOffset = {shakeDist(rng()) * shakeMagnitude,
                           shakeDist(rng()) * shakeMagnitude};
            camera.setCenter(preShakeCenter + shakeOffset);
            shakeMagnitude = std::max(0.f, shakeMagnitude - kShakeDecayPerSec * realDt);
        }

        // Set camera view (world space)
        window.setView(camera);

        // Terrain tiles
        window.draw(terrainMesh);

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

        // Shooters
        for (auto& s : shooters) window.draw(*s);

        // Player
        window.draw(player);

        // Player projectiles (pellets)
        for (auto& p : playerProjectiles) window.draw(*p);

        // UI (back to default view so it stays on screen)
        window.setView(uiView);

        ui::HudState hudState;
        hudState.waveNumber = waveManager.waveNumber();
        hudState.waveActive = waveManager.waveActive();
        hudState.waveBreakRemainingSec = static_cast<int>(waveManager.waveBreakRemaining());
        hudState.enemiesAlive = static_cast<int>(enemies.size() + shooters.size());
        hudState.enemiesRemainingInWave = waveManager.enemiesRemaining();
        hudState.killCount = killCount;
        hudState.lootOnGround = static_cast<int>(loot.size());
        hudState.playerHealth = player.health();
        hudState.playerMaxHealth = player.maxHealth();
        hudState.dashCooldownRemaining = player.dashCooldownRemaining();
        hudState.inventory = &inventory;
        hudState.shopOpen = (gameState == core::GameState::Shop);
        hudState.shop = &shop;
        hudState.isGameOver = (gameState == core::GameState::GameOver);
        hudState.isMainMenu = (gameState == core::GameState::MainMenu);
        hudState.isRoundComplete = (gameState == core::GameState::RoundComplete);
        hudState.weaponName = player.weapon()
            ? std::string(player.weapon()->name())
            : std::string("(none)");
        hudState.ammoInfinite = false;
        hudState.ammoMax = (hudState.weaponName == "Shotgun") ? ammoMax : 0;
        hudState.ammoCurrent = (hudState.weaponName == "Shotgun") ? ammoCurrent : 0;

        // Menu layouts
        if (hudState.isGameOver) {
            const auto btn = layoutGameOverButtons(window.getSize());
            hudState.gameOverBtnRestart = btn.b0;
            hudState.gameOverBtnMenu = btn.b1;
            hudState.gameOverBtnExit = btn.b2;
            hudState.gameOverSelected = gameOverSelection;
        }
        if (hudState.isMainMenu) {
            const auto btn = layoutMainMenuButtons(window.getSize());
            hudState.mainMenuBtnStart = btn.b0;
            hudState.mainMenuBtnExit = btn.b1;
            hudState.mainMenuSelected = mainMenuSelection;
        }
        if (hudState.isRoundComplete) {
            const auto btn = layoutRoundCompleteButtons(window.getSize());
            hudState.roundCompleteBtnNext = btn.b0;
            hudState.roundCompleteBtnMenu = btn.b1;
            hudState.roundCompleteSelected = roundCompleteSelection;
        }
        hud.draw(window, hudState);

        window.display();

        // Undo shake offset so smooth follow operates on the true center.
        if (shakeOffset.x != 0.f || shakeOffset.y != 0.f) {
            camera.setCenter(preShakeCenter);
        }

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