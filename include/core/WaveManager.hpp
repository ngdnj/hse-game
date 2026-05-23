#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <functional>

namespace core {

// Pure mathematical wave state — no SFML dependencies, easily unit-testable
struct WaveState {
    int waveNumber{0};
    int enemiesRemaining{0};
    int totalEnemiesThisWave{0};
    float timeSinceWaveCleared{0.f};
    bool waveActive{false};
};

class WaveManager {
public:
    explicit WaveManager(float waveBreakDuration = 3.f)
        : waveBreakDuration_(waveBreakDuration)
    {}

    bool startNextWave() {
        if (state_.waveActive) return false;
        ++state_.waveNumber;
        state_.totalEnemiesThisWave = enemiesForWave(state_.waveNumber);
        state_.enemiesRemaining = state_.totalEnemiesThisWave;
        state_.timeSinceWaveCleared = 0.f;
        state_.waveActive = true;
        return true;
    }

    void update(float dt) {
        if (state_.waveActive) return;
        state_.timeSinceWaveCleared += dt;
    }

    void onEnemyKilled() {
        if (state_.enemiesRemaining > 0) --state_.enemiesRemaining;
    }

    [[nodiscard]] bool canStartNextWave() const {
        return !state_.waveActive &&
               state_.timeSinceWaveCleared >= waveBreakDuration_;
    }

    template<typename F>
    [[nodiscard]] std::vector<sf::Vector2f> spawnPositions(
        const sf::FloatRect& worldBounds, F&& randomPosFn, int maxAlive) const {
        std::vector<sf::Vector2f> result;
        if (!state_.waveActive) return result;
        const int toSpawn = std::min(state_.enemiesRemaining, maxAlive);
        result.reserve(static_cast<std::size_t>(toSpawn));
        for (int i = 0; i < toSpawn; ++i) {
            result.push_back(randomPosFn(worldBounds));
        }
        return result;
    }

    [[nodiscard]] int enemiesToSpawnNow(int aliveCount) const {
        if (!state_.waveActive) return 0;
        return std::max(0, state_.enemiesRemaining - aliveCount);
    }

    [[nodiscard]] int waveNumber() const noexcept { return state_.waveNumber; }
    [[nodiscard]] int totalEnemiesThisWave() const noexcept { return state_.totalEnemiesThisWave; }
    [[nodiscard]] int enemiesRemaining() const noexcept { return state_.enemiesRemaining; }
    [[nodiscard]] float timeSinceWaveCleared() const noexcept { return state_.timeSinceWaveCleared; }
    [[nodiscard]] bool waveActive() const noexcept { return state_.waveActive; }
    [[nodiscard]] float waveBreakRemaining() const noexcept {
        return std::max(0.f, waveBreakDuration_ - state_.timeSinceWaveCleared);
    }

    void forceClear() {
        state_.enemiesRemaining = 0;
        state_.waveActive = false;
        state_.timeSinceWaveCleared = 0.f;
    }

private:
    [[nodiscard]] static int enemiesForWave(int w) {
        return std::min(3 + w * 2, 20);
    }

    WaveState state_;
    float waveBreakDuration_{3.f};
};

} // namespace core