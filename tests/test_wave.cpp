#include <catch2/catch_test_macros.hpp>
#include "core/WaveManager.hpp"
#include <vector>
#include <optional>
#include <cstdint>

namespace {
constexpr std::uint32_t kTestSeed = 1337u;

constexpr int baseForWave(int w) {
    const int base = 4 + w * 3;
    return base > 20 ? 20 : base;
}

constexpr int varianceForBase(int base) {
    return base >= 12 ? 3 : 2;
}

constexpr int minForWave(int w) {
    const int base = baseForWave(w);
    const int minCount = base - varianceForBase(base);
    return minCount < 3 ? 3 : minCount;
}

constexpr int maxForWave(int w) {
    const int base = baseForWave(w);
    const int maxCount = base + varianceForBase(base);
    return maxCount > 20 ? 20 : maxCount;
}
} // namespace

TEST_CASE("WaveManager initial state", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);
    REQUIRE(wm.waveNumber() == 0);
    REQUIRE(wm.waveActive() == false);
    REQUIRE(wm.enemiesRemaining() == 0);
    REQUIRE(wm.totalEnemiesThisWave() == 0);
    REQUIRE(wm.timeSinceWaveCleared() == 0.f);
    REQUIRE(wm.waveBreakRemaining() == 3.f);
}

TEST_CASE("WaveManager startNextWave", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    SECTION("first wave starts correctly") {
        bool started = wm.startNextWave();
        REQUIRE(started == true);
        REQUIRE(wm.waveActive() == true);
        REQUIRE(wm.waveNumber() == 1);
        REQUIRE(wm.totalEnemiesThisWave() >= minForWave(1));
        REQUIRE(wm.totalEnemiesThisWave() <= maxForWave(1));
        REQUIRE(wm.enemiesRemaining() == wm.totalEnemiesThisWave());
    }

    SECTION("cannot start wave while active") {
        wm.startNextWave();
        bool secondStart = wm.startNextWave();
        REQUIRE(secondStart == false);
        REQUIRE(wm.waveNumber() == 1);
    }
}

TEST_CASE("WaveManager onEnemyKilled", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);
    wm.startNextWave();
    const int total = wm.enemiesRemaining();
    REQUIRE(total >= minForWave(1));
    REQUIRE(total <= maxForWave(1));

    wm.onEnemyKilled();
    REQUIRE(wm.enemiesRemaining() == total - 1);

    wm.onEnemyKilled();
    REQUIRE(wm.enemiesRemaining() == total - 2);
}

TEST_CASE("WaveManager update timer", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    SECTION("timer advances when not active") {
        wm.update(1.0f);
        REQUIRE(wm.timeSinceWaveCleared() == 1.0f);
        wm.update(1.5f);
        REQUIRE(wm.timeSinceWaveCleared() == 2.5f);
    }

    SECTION("timer does not advance during wave") {
        wm.startNextWave();
        wm.update(1.0f);
        REQUIRE(wm.timeSinceWaveCleared() == 0.f);
    }
}

TEST_CASE("WaveManager canStartNextWave", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    SECTION("cannot start while wave is active") {
        wm.startNextWave();
        REQUIRE(wm.canStartNextWave() == false);
    }

    SECTION("cannot start before break duration") {
        wm.startNextWave();
        wm.forceClear();
        wm.update(1.0f);
        REQUIRE(wm.canStartNextWave() == false);
    }

    SECTION("can start after break duration") {
        wm.startNextWave();
        wm.forceClear();
        wm.update(3.0f);
        REQUIRE(wm.canStartNextWave() == true);
    }
}

TEST_CASE("WaveManager enemy count scales with wave number", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    wm.startNextWave();
    REQUIRE(wm.totalEnemiesThisWave() >= minForWave(1));
    REQUIRE(wm.totalEnemiesThisWave() <= maxForWave(1));

    wm.forceClear();
    wm.startNextWave(); // wave 2
    REQUIRE(wm.totalEnemiesThisWave() >= minForWave(2));
    REQUIRE(wm.totalEnemiesThisWave() <= maxForWave(2));

    wm.forceClear();
    wm.startNextWave(); // wave 3
    REQUIRE(wm.totalEnemiesThisWave() >= minForWave(3));
    REQUIRE(wm.totalEnemiesThisWave() <= maxForWave(3));
}

TEST_CASE("WaveManager enemiesToSpawnNow", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);
    wm.startNextWave();
    const int total = wm.totalEnemiesThisWave();
    REQUIRE(total >= minForWave(1));
    REQUIRE(total <= maxForWave(1));

    SECTION("spawns all at once when no alive") {
        REQUIRE(wm.enemiesToSpawnNow(0) == total);
    }

    SECTION("spawns remaining when partially alive") {
        REQUIRE(wm.enemiesToSpawnNow(2) == std::max(0, total - 2));
        REQUIRE(wm.enemiesToSpawnNow(4) == std::max(0, total - 4));
    }

    SECTION("spawns zero when all alive") {
        REQUIRE(wm.enemiesToSpawnNow(total) == 0);
        REQUIRE(wm.enemiesToSpawnNow(total + 100) == 0);
    }

    SECTION("returns zero when wave not active") {
        wm.forceClear();
        REQUIRE(wm.enemiesToSpawnNow(0) == 0);
    }
}

TEST_CASE("WaveManager spawnPositions", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);
    wm.startNextWave();
    sf::FloatRect world{{0.f, 0.f}, {2000.f, 2000.f}};
    const int total = wm.totalEnemiesThisWave();

    auto fixedSpawner = [](const sf::FloatRect&) -> sf::Vector2f {
        return {100.f, 100.f};
    };

    SECTION("returns spawn count matching remaining") {
        auto positions = wm.spawnPositions(world, fixedSpawner, 3);
        REQUIRE(positions.size() == static_cast<std::size_t>(std::min(total, 3)));
    }

    SECTION("respects maxAlive cap") {
        auto positions = wm.spawnPositions(world, fixedSpawner, 2);
        REQUIRE(positions.size() == static_cast<std::size_t>(std::min(total, 2)));
    }

    SECTION("returns empty when not active") {
        wm.forceClear();
        auto positions = wm.spawnPositions(world, fixedSpawner, 5);
        REQUIRE(positions.size() == 0);
    }
}

TEST_CASE("WaveManager waveBreakRemaining", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    wm.update(1.f);
    REQUIRE(wm.waveBreakRemaining() == 2.f);

    wm.update(2.f);
    REQUIRE(wm.waveBreakRemaining() == 0.f);
}

TEST_CASE("WaveManager max cap at 20 enemies", "[wave]") {
    core::WaveManager wm(3.f, kTestSeed);

    wm.startNextWave(); // wave 1: 5
    wm.forceClear();
    wm.startNextWave(); // wave 2: 7
    wm.forceClear();
    wm.startNextWave(); // wave 3: 9
    wm.forceClear();
    wm.startNextWave(); // wave 4: 11
    wm.forceClear();
    wm.startNextWave(); // wave 5: 13
    wm.forceClear();
    wm.startNextWave(); // wave 6: 15
    wm.forceClear();
    wm.startNextWave(); // wave 7: 17
    wm.forceClear();
    wm.startNextWave(); // wave 8: 19
    wm.forceClear();
    wm.startNextWave(); // wave 9: 21 -> capped to 20

    REQUIRE(wm.totalEnemiesThisWave() <= 20);
    REQUIRE(wm.totalEnemiesThisWave() >= minForWave(9));
}