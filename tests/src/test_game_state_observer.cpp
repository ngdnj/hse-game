#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "ai/GameStateObserver.hpp"
#include "combat/SwordWeapon.hpp"
#include "entities/Player.hpp"
#include "entities/Enemy.hpp"
#include "entities/ShooterEnemy.hpp"
#include "entities/Projectile.hpp"

#include <memory>
#include <vector>

// Helper to create an Enemy at a given position (res can be nullptr)
static std::unique_ptr<entities::Enemy> makeTestEnemy(const sf::Vector2f& position) {
    sf::Vector2f size{32.f, 32.f};
    sf::FloatRect bounds{sf::Vector2f{0.f, 0.f}, sf::Vector2f{1920.f, 1080.f}};
    auto enemy = std::make_unique<entities::Enemy>(position, size, bounds, nullptr);
    return enemy;
}

// Helper to create a Player as a unique_ptr
static std::unique_ptr<Player> makeTestPlayer(const sf::Vector2f& position, int hp = 100) {
    sf::Vector2f size{32.f, 32.f};
    sf::FloatRect bounds{sf::Vector2f{0.f, 0.f}, sf::Vector2f{1920.f, 1080.f}};
    auto player = std::make_unique<Player>(size, position, bounds, "", "");
    if (hp < 100) {
        player->takeDamage(100 - hp);
    }
    return player;
}

// Test fixture for GameStateObserver tests - simple struct without custom destructor
struct GameStateObserverFixture {
    std::vector<std::unique_ptr<entities::Enemy>> enemies;
    std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
    std::vector<std::unique_ptr<entities::Projectile>> projectiles;

    std::unique_ptr<Player> createPlayer(const sf::Vector2f& position, int hp = 100) {
        return makeTestPlayer(position, hp);
    }
};

TEST_CASE("GameStateObserver serializes player state correctly", "[GameStateObserver]") {
    GameStateObserver observer;
    GameStateObserverFixture fixture;

    SECTION("Player with full health") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["player"]["hp"] == 100);
        REQUIRE(json["player"]["max_hp"] == 100);
    }

    SECTION("Player with partial health") {
        auto player = fixture.createPlayer({100.f, 200.f}, 50);
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["player"]["hp"] == 50);
        REQUIRE(json["player"]["max_hp"] == 100);
    }

    SECTION("Player position") {
        auto player = fixture.createPlayer({123.f, 456.f}, 100);
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["player"]["pos"]["x"] == 123);
        REQUIRE(json["player"]["pos"]["y"] == 456);
    }

    SECTION("Dash ready when cooldown is zero") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        player->setDashCooldown(1.5f);
        // dashCooldownTimer starts at 0, so dash should be ready
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["dash_ready"] == true);
    }

    SECTION("Dash not ready when on cooldown") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        player->setDashCooldown(1.5f);
        player->dash(); // Triggers cooldown
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["dash_ready"] == false);
    }

    SECTION("Enemy list empty") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"] == nlohmann::json::array());
    }

    SECTION("Weapon name serialization - Sword") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        player->setWeapon(std::make_unique<combat::SwordWeapon>());
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["weapon"] == "Sword");
    }

    SECTION("Weapon name serialization - default is Sword") {
        // Player always has a default SwordWeapon (never nullptr per Player constructor)
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["weapon"] == "Sword");
    }

    SECTION("Outputs valid JSON") {
        auto player = fixture.createPlayer({100.f, 200.f}, 100);
        std::string jsonStr = observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles);
        // Should not throw - verifies valid JSON
        auto json = nlohmann::json::parse(jsonStr);
        REQUIRE(json.contains("player"));
        REQUIRE(json.contains("weapon"));
        REQUIRE(json.contains("dash_ready"));
        REQUIRE(json.contains("enemies"));
    }

    SECTION("Enemy serialization includes position and distance") {
        auto player = fixture.createPlayer({0.f, 0.f}, 100);
        // Enemy at (3, 4) has distance 5 from player at (0, 0)
        fixture.enemies.push_back(makeTestEnemy({3.f, 4.f}));
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"].size() == 1);
        REQUIRE(json["enemies"][0]["pos"]["x"] == 3);
        REQUIRE(json["enemies"][0]["pos"]["y"] == 4);
        REQUIRE(json["enemies"][0]["dist"] == 5);
    }

    SECTION("Multiple enemies serialized with correct positions and distances") {
        auto player = fixture.createPlayer({0.f, 0.f}, 100);
        fixture.enemies.push_back(makeTestEnemy({3.f, 4.f})); // dist 5
        fixture.enemies.push_back(makeTestEnemy({6.f, 8.f})); // dist 10
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"].size() == 2);
        REQUIRE(json["enemies"][0]["pos"]["x"] == 3);
        REQUIRE(json["enemies"][0]["pos"]["y"] == 4);
        REQUIRE(json["enemies"][0]["dist"] == 5);
        REQUIRE(json["enemies"][1]["pos"]["x"] == 6);
        REQUIRE(json["enemies"][1]["pos"]["y"] == 8);
        REQUIRE(json["enemies"][1]["dist"] == 10);
    }
}

TEST_CASE("GameStateObserver::distanceToClosestEnemy", "[GameStateObserver]") {
    SECTION("Returns -1 when no enemies") {
        sf::Vector2f playerPos{100.f, 100.f};
        std::vector<std::unique_ptr<entities::Enemy>> enemies;
        std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
        auto dist = GameStateObserver::distanceToClosestEnemy(playerPos, enemies, shooters);
        REQUIRE(dist == -1.f);
    }

    SECTION("Returns distance to single enemy") {
        sf::Vector2f playerPos{0.f, 0.f};
        std::vector<std::unique_ptr<entities::Enemy>> enemies;
        enemies.push_back(makeTestEnemy({3.f, 4.f})); // 3-4-5 triangle, distance = 5
        std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
        auto dist = GameStateObserver::distanceToClosestEnemy(playerPos, enemies, shooters);
        REQUIRE(dist == 5.f);
    }

    SECTION("Returns distance to closest when multiple enemies") {
        sf::Vector2f playerPos{0.f, 0.f};
        std::vector<std::unique_ptr<entities::Enemy>> enemies;
        enemies.push_back(makeTestEnemy({10.f, 0.f}));  // distance = 10
        enemies.push_back(makeTestEnemy({5.f, 0.f}));   // distance = 5 (closest)
        enemies.push_back(makeTestEnemy({0.f, 10.f}));  // distance = 10
        std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
        auto dist = GameStateObserver::distanceToClosestEnemy(playerPos, enemies, shooters);
        REQUIRE(dist == 5.f);
    }

    SECTION("Ignores dead enemies") {
        sf::Vector2f playerPos{0.f, 0.f};
        std::vector<std::unique_ptr<entities::Enemy>> enemies;
        enemies.push_back(makeTestEnemy({3.f, 4.f})); // 3-4-5 triangle, distance = 5
        enemies.back()->takeDamage(1000); // Kill the close enemy
        std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
        // No alive enemies, should return -1
        auto dist = GameStateObserver::distanceToClosestEnemy(playerPos, enemies, shooters);
        REQUIRE(dist == -1.f);
    }
}

TEST_CASE("GameStateObserver limits to 5 closest enemies", "[GameStateObserver]") {
    GameStateObserver observer;
    GameStateObserverFixture fixture;
    auto player = fixture.createPlayer({500.f, 500.f}, 100);

    SECTION("Returns exactly 5 enemies when more than 5 exist") {
        // Create 10 enemies at different positions (distances from player at 500,500)
        for (int i = 0; i < 10; ++i) {
            fixture.enemies.push_back(makeTestEnemy({static_cast<float>(i * 100), 500.f}));
        }
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"].size() == 5);
    }

    SECTION("Returns all enemies when fewer than 5 exist") {
        fixture.enemies.push_back(makeTestEnemy({100.f, 100.f}));
        fixture.enemies.push_back(makeTestEnemy({200.f, 200.f}));
        fixture.enemies.push_back(makeTestEnemy({300.f, 300.f}));
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"].size() == 3);
    }

    SECTION("Returns closest enemies when more than 5 exist") {
        // Create enemies at distances: 100, 200, 300, 400, 500, 1000
        fixture.enemies.push_back(makeTestEnemy({600.f, 500.f})); // dist = 100
        fixture.enemies.push_back(makeTestEnemy({700.f, 500.f})); // dist = 200
        fixture.enemies.push_back(makeTestEnemy({800.f, 500.f})); // dist = 300
        fixture.enemies.push_back(makeTestEnemy({900.f, 500.f})); // dist = 400
        fixture.enemies.push_back(makeTestEnemy({1000.f, 500.f})); // dist = 500
        fixture.enemies.push_back(makeTestEnemy({1500.f, 500.f})); // dist = 1000
        auto json = nlohmann::json::parse(observer.serialize(*player, fixture.enemies, fixture.shooters, fixture.projectiles));
        REQUIRE(json["enemies"].size() == 5);
        // First enemy should be the closest (distance 100)
        REQUIRE(json["enemies"][0]["dist"] == 100);
        // Last included enemy should have dist = 500
        REQUIRE(json["enemies"][4]["dist"] == 500);
        // The 6th enemy (dist 1000) should not appear
        REQUIRE(json["enemies"].size() == 5);
    }
}