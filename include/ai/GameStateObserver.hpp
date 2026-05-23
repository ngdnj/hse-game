#pragma once

#include <SFML/System/Vector2.hpp>
#include <memory>
#include <string>
#include <vector>

class Player;

namespace entities {
class Enemy;
class ShooterEnemy;
class Projectile;
} // namespace entities

/**
 * @brief Serializes the current game state into a compact JSON string for the
 *        Ollama LLM controller.
 *
 * The output is intentionally token-efficient: short field names, no extra
 * whitespace (minified), and only the most relevant signals (player HP and
 * position, current weapon name, whether the dash is available, and up to the
 * five closest enemies).
 *
 * Format example:
 * @code
 * {
 *   "player":{"hp":80,"max_hp":100,"pos":{"x":500,"y":700}},
 *   "weapon":"Shotgun",
 *   "dash_ready":true,
 *   "enemies":[{"pos":{"x":300,"y":400},"dist":360}]
 * }
 * @endcode
 */
class GameStateObserver {
public:
    /// Returns a compact JSON string describing the current game state. The
    /// `projectiles` parameter is accepted for future use (e.g. surfacing
    /// incoming threats) and is currently summarised as a single count field.
    [[nodiscard]] std::string serialize(
        const Player& player,
        const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
        const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters,
        const std::vector<std::unique_ptr<entities::Projectile>>& projectiles
    ) const;

    /// Returns distance from `playerPos` to the closest alive enemy across
    /// melee and shooter enemy lists, or -1 if none are alive.
    [[nodiscard]] static float distanceToClosestEnemy(
        const sf::Vector2f& playerPos,
        const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
        const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters
    );

    /// Maximum number of enemy descriptors emitted by `serialize`.
    static constexpr std::size_t kMaxEnemiesReported{5};
};
