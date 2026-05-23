#include <SFML/Graphics.hpp>
#include <array>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

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

    // Create player with sprite atlases (gracefully falls back to colored
    // rectangle if textures fail to load — see Player ctor).
    Player player(sf::Vector2f{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f},
                 worldBounds,
                 "assets/player/run.png",
                 "assets/player/idle.png");
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
    std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
    std::vector<std::unique_ptr<entities::Loot>> loot;
    std::vector<std::unique_ptr<entities::Projectile>> playerProjectiles;

    int killCount = 0;

    // Wave system
    core::WaveManager waveManager(4.f);
    waveManager.startNextWave();

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
    const sf::Vector2f playerSpawn{1000.f, 1000.f};

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
        waveManager = core::WaveManager(4.f);
        waveManager.startNextWave();
        shop = core::Shop{};
        eventBus.reset();
        vampiricFang.reset();
        explosiveShells.reset();
        gameState = core::GameState::Playing;
        std::cout << "[Game] Restarted\n";
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
            if (auto* key = eventOpt->getIf<sf::Event::KeyPressed>()) {
                if (gameState == core::GameState::Shop) {
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
                    if (key->code == sf::Keyboard::Key::R) {
                        resetGame();
                    }
                } else {
                    // Playing: E key to pick up loot, Q toggles weapon
                    if (key->code == sf::Keyboard::Key::E) {
                        for (auto& l : loot) {
                            if (!l->consumed() && l->getGlobalBounds().findIntersection(player.getGlobalBounds()).has_value()) {
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
            // Player
            player.update(kFixedDt);

            // Combat: fire active weapon if Space is held and weapon is ready.
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space)
                && player.weapon() && player.weapon()->canFire()) {
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
                                    enemy->getPosition(), "coin", 5));
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
                                    shooter->getPosition(), "coin", 10));
                            }
                        }
                    }
                }
                for (auto& proj : result.projectiles) {
                    playerProjectiles.push_back(std::move(proj));
                }
                if (!result.projectiles.empty() || result.hasMeleeHitbox) {
                    // Weak shake just for firing weight on shotgun.
                    triggerShake(result.hasMeleeHitbox ? 0.f : 5.f);
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

            // Shooters
            for (auto& shooter : shooters) {
                if (!shooter->isAlive()) continue;
                shooter->setPlayerPosition(&playerPos);
                shooter->update(kFixedDt);
            }

            // Projectile-player collision
            for (auto& shooter : shooters) {
                for (auto& proj : shooter->projectiles()) {
                    if (!proj->consumed() && proj->getGlobalBounds().findIntersection(player.getGlobalBounds()).has_value()) {
                        player.takeDamage(proj->damage());
                        triggerShake(10.f);
                        std::cout << "[Combat] Player hit by projectile! HP: "
                                  << player.health() << "/" << player.maxHealth() << "\n";
                        proj->markConsumed();
                        if (player.isDead()) {
                            gameState = core::GameState::GameOver;
                            std::cout << "[Game] Player died -- GAME OVER\n";
                        }
                    }
                }
            }

            // Player projectiles (shotgun pellets) — update + collide vs enemies
            for (auto& proj : playerProjectiles) proj->update(kFixedDt);
            for (auto& proj : playerProjectiles) {
                if (proj->consumed()) continue;
                const auto projBounds = proj->getGlobalBounds();
                auto applyHit = [&](auto& target, float kbForce, int coinValue,
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
                            target->getPosition(), "coin", coinValue));
                        std::cout << "[Combat] " << label << " killed by pellet!\n";
                    }
                    proj->markConsumed();
                    return true;
                };
                for (auto& enemy : enemies) {
                    if (applyHit(enemy, 220.f, 5, "Chaser")) break;
                }
                if (proj->consumed()) continue;
                for (auto& shooter : shooters) {
                    if (applyHit(shooter, 220.f, 10, "Shooter")) break;
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
                    std::cout << "[Spawn] Chaser at (" << pos.x << "," << pos.y << ")\n";
                } else {
                    shooters.push_back(std::make_unique<entities::ShooterEnemy>(
                        pos, worldBounds, &res));
                    shooters.back()->setObstacles(&obstacleAABBs);
                    std::cout << "[Spawn] Shooter at (" << pos.x << "," << pos.y << ")\n";
                }
            }
        }

        // Auto-open shop when the wave is fully cleared
        if (waveManager.waveActive() && waveManager.enemiesRemaining() == 0
            && enemies.empty() && shooters.empty()) {
            waveManager.forceClear();
            gameState = core::GameState::Shop;
            std::cout << "[Shop] Wave " << waveManager.waveNumber()
                      << " cleared -- shop opened\n";
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

        // Shooters
        for (auto& s : shooters) window.draw(*s);

        // Player
        window.draw(player);

        // Player projectiles (pellets)
        for (auto& p : playerProjectiles) window.draw(*p);

        // UI (back to default view so it stays on screen)
        window.setView(window.getDefaultView());

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
        hudState.weaponName = player.weapon()
            ? std::string(player.weapon()->name())
            : std::string("(none)");
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