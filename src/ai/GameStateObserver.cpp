#include "ai/GameStateObserver.hpp"

#include "combat/Weapon.hpp"
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"
#include "entities/Projectile.hpp"
#include "entities/ShooterEnemy.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace {

float distanceBetween(const sf::Vector2f& a, const sf::Vector2f& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

struct EnemyEntry {
    sf::Vector2f pos;
    float dist;
};

} // namespace

std::string GameStateObserver::serialize(
    const Player& player,
    const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
    const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters,
    const std::vector<std::unique_ptr<entities::Projectile>>& projectiles) const {

    const sf::Vector2f playerPos = player.getPosition();

    // Gather alive enemies and shooters together, computing distances once.
    std::vector<EnemyEntry> entries;
    entries.reserve(enemies.size() + shooters.size());

    for (const auto& enemy : enemies) {
        if (!enemy || enemy->isDead()) {
            continue;
        }
        const sf::Vector2f pos = enemy->getPosition();
        entries.push_back({pos, distanceBetween(playerPos, pos)});
    }
    for (const auto& shooter : shooters) {
        if (!shooter || shooter->isDead()) {
            continue;
        }
        const sf::Vector2f pos = shooter->getPosition();
        entries.push_back({pos, distanceBetween(playerPos, pos)});
    }

    // Keep only the kMaxEnemiesReported closest, sorted by distance.
    const std::size_t reported =
        std::min<std::size_t>(entries.size(), kMaxEnemiesReported);
    std::partial_sort(
        entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(reported),
        entries.end(),
        [](const EnemyEntry& a, const EnemyEntry& b) { return a.dist < b.dist; });
    entries.resize(reported);

    nlohmann::json doc;
    doc["player"] = {
        {"hp", player.health()},
        {"max_hp", player.maxHealth()},
        {"pos", {{"x", static_cast<int>(std::lround(playerPos.x))},
                  {"y", static_cast<int>(std::lround(playerPos.y))}}},
    };

    const combat::Weapon* weapon = player.weapon();
    doc["weapon"] = weapon ? std::string{weapon->name()} : std::string{"None"};
    doc["dash_ready"] = player.dashCooldownRemaining() <= 0.f;

    auto enemiesJson = nlohmann::json::array();
    for (const auto& entry : entries) {
        enemiesJson.push_back({
            {"pos", {{"x", static_cast<int>(std::lround(entry.pos.x))},
                      {"y", static_cast<int>(std::lround(entry.pos.y))}}},
            {"dist", static_cast<int>(std::lround(entry.dist))},
        });
    }
    doc["enemies"] = std::move(enemiesJson);

    // Count live projectiles in flight (cheap situational signal for the LLM).
    int liveProjectiles = 0;
    for (const auto& proj : projectiles) {
        if (proj && proj->isAlive() && !proj->consumed()) {
            ++liveProjectiles;
        }
    }
    doc["projectiles"] = liveProjectiles;

    // dump(-1) -> compact (no indentation / spacing).
    return doc.dump();
}

float GameStateObserver::distanceToClosestEnemy(
    const sf::Vector2f& playerPos,
    const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
    const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters) {

    float closest = std::numeric_limits<float>::max();
    bool found = false;

    for (const auto& enemy : enemies) {
        if (!enemy || enemy->isDead()) {
            continue;
        }
        const float d = distanceBetween(playerPos, enemy->getPosition());
        if (d < closest) {
            closest = d;
            found = true;
        }
    }
    for (const auto& shooter : shooters) {
        if (!shooter || shooter->isDead()) {
            continue;
        }
        const float d = distanceBetween(playerPos, shooter->getPosition());
        if (d < closest) {
            closest = d;
            found = true;
        }
    }

    return found ? closest : -1.f;
}
