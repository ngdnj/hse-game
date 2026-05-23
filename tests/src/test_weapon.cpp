#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <SFML/Graphics.hpp>

namespace combat {
namespace {

sf::Vector2f normalizeOrRight(sf::Vector2f v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= std::numeric_limits<float>::epsilon()) return {1.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

} // namespace

inline sf::FloatRect swordHitbox(sf::Vector2f origin, sf::Vector2f dir, float range) {
    const sf::Vector2f d = normalizeOrRight(dir);
    constexpr float halfH = 30.f;
    const float halfW = range * 0.5f;
    const sf::Vector2f centerOffset = d * range * 0.5f;
    return sf::FloatRect{
        sf::Vector2f{origin.x + centerOffset.x - halfW,
                     origin.y + centerOffset.y - halfH},
        sf::Vector2f{range, halfH * 2.f}};
}

inline std::vector<sf::Vector2f> shotgunPelletDirections(sf::Vector2f baseDir, int pelletCount, float spreadHalfAngleRad) {
    const float lenSq = baseDir.x * baseDir.x + baseDir.y * baseDir.y;
    sf::Vector2f d;
    if (lenSq <= std::numeric_limits<float>::epsilon()) {
        d = {1.f, 0.f};
    } else {
        const float invLen = 1.f / std::sqrt(lenSq);
        d = {baseDir.x * invLen, baseDir.y * invLen};
    }

    std::vector<sf::Vector2f> result;
    result.reserve(static_cast<std::size_t>(pelletCount));
    if (pelletCount == 1) {
        result.push_back(d);
        return result;
    }

    const float baseAngle = std::atan2(d.y, d.x);
    const float totalSpread = spreadHalfAngleRad * 2.f;
    const float step = totalSpread / static_cast<float>(pelletCount - 1);
    const float startAngle = baseAngle - spreadHalfAngleRad;

    for (int i = 0; i < pelletCount; ++i) {
        const float angle = startAngle + step * static_cast<float>(i);
        result.push_back({std::cos(angle), std::sin(angle)});
    }
    return result;
}

} // namespace combat

using namespace combat;

TEST_CASE("Sword hitbox - right direction", "[weapon]") {
    auto hb = swordHitbox({0.f, 0.f}, {1.f, 0.f}, 55.f);
    CHECK(hb.position.x >= -0.5f);
    CHECK(hb.position.x <= 0.5f);
    CHECK(hb.position.y >= -31.f);
    CHECK(hb.position.y <= -29.f);
    CHECK(hb.size.x >= 54.f);
    CHECK(hb.size.x <= 56.f);
    CHECK(hb.size.y >= 58.f);
    CHECK(hb.size.y <= 62.f);
}

TEST_CASE("Sword hitbox - up direction", "[weapon]") {
    auto hb = swordHitbox({100.f, 100.f}, {0.f, -1.f}, 55.f);
    CHECK(hb.position.x >= 71.f);
    CHECK(hb.position.x <= 74.f);
    CHECK(hb.position.y >= 41.f);
    CHECK(hb.position.y <= 44.f);
}

TEST_CASE("Sword hitbox - diagonal direction", "[weapon]") {
    auto hb = swordHitbox({0.f, 0.f}, {1.f, 1.f}, 55.f);
    CHECK(hb.position.x >= -10.f);
    CHECK(hb.position.x <= -7.f);
    CHECK(hb.position.y >= -12.f);
    CHECK(hb.position.y <= -9.f);
}

TEST_CASE("Sword hitbox - zero direction (defaults to right)", "[weapon]") {
    auto hb = swordHitbox({0.f, 0.f}, {0.f, 0.f}, 55.f);
    CHECK(hb.position.x >= -0.5f);
    CHECK(hb.position.x <= 0.5f);
}

TEST_CASE("Shotgun pellet directions - 3 pellets, no spread", "[weapon]") {
    auto dirs = shotgunPelletDirections({1.f, 0.f}, 3, 0.f);
    CHECK(dirs.size() == 3);
    for (const auto& d : dirs) {
        CHECK(std::abs(d.x - 1.f) < 0.001f);
        CHECK(std::abs(d.y) < 0.001f);
    }
}

TEST_CASE("Shotgun pellet directions - 5 pellets with spread", "[weapon]") {
    auto dirs = shotgunPelletDirections({1.f, 0.f}, 5, 0.2618f);
    CHECK(dirs.size() == 5);
    for (const auto& d : dirs) {
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        CHECK(std::abs(len - 1.f) < 0.001f);
    }
    CHECK(std::abs(dirs[2].x - 1.f) < 0.01f);
    CHECK(std::abs(dirs[2].y) < 0.01f);
    CHECK(dirs[0].x <= dirs[4].x); // leftmost <= rightmost
}

TEST_CASE("Shotgun pellet directions - 1 pellet", "[weapon]") {
    auto dirs = shotgunPelletDirections({0.f, 1.f}, 1, 0.5f);
    CHECK(dirs.size() == 1);
    CHECK(std::abs(dirs[0].x) < 0.001f);
    CHECK(std::abs(dirs[0].y - 1.f) < 0.001f);
}

TEST_CASE("Shotgun pellet directions - up direction", "[weapon]") {
    auto dirs = shotgunPelletDirections({0.f, -1.f}, 5, 0.3f);
    CHECK(dirs.size() == 5);
    for (const auto& d : dirs) {
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        CHECK(std::abs(len - 1.f) < 0.001f);
    }
    CHECK(dirs[2].y < -0.9f);
}

TEST_CASE("Shotgun pellet directions - 7 pellets wide spread", "[weapon]") {
    auto dirs = shotgunPelletDirections({1.f, 0.f}, 7, 0.5f);
    CHECK(dirs.size() == 7);
    for (const auto& d : dirs) {
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        CHECK(std::abs(len - 1.f) < 0.001f);
    }
    CHECK(dirs[0].x <= dirs[6].x);
    CHECK(dirs[3].x > 0.9f); // center should be almost horizontal
}

TEST_CASE("Shotgun pellet directions - diagonal base direction", "[weapon]") {
    auto dirs = shotgunPelletDirections({1.f, 1.f}, 3, 0.2f);
    CHECK(dirs.size() == 3);
    for (const auto& d : dirs) {
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        CHECK(std::abs(len - 1.f) < 0.001f);
    }
}

TEST_CASE("Shotgun pellet directions - even pellet count", "[weapon]") {
    auto dirs = shotgunPelletDirections({1.f, 0.f}, 4, 0.3f);
    CHECK(dirs.size() == 4);
    for (const auto& d : dirs) {
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        CHECK(std::abs(len - 1.f) < 0.001f);
    }
}