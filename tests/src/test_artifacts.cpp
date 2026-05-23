#include <catch2/catch_test_macros.hpp>
#include "core/Events.hpp"
#include "combat/Artifacts.hpp"

TEST_CASE("VampiricFang - no heal until 5 kills", "[artifact]") {
    combat::VampiricFang vf(5, 1);
    for (int i = 0; i < 4; ++i) {
        vf.onEnemyKilled({i + 1});
        CHECK(vf.pendingHeals() == 0);
    }
}

TEST_CASE("VampiricFang - heal triggers after 5 kills", "[artifact]") {
    combat::VampiricFang vf(5, 1);
    for (int i = 0; i < 5; ++i) vf.onEnemyKilled({});
    CHECK(vf.pendingHeals() == 1);
}

TEST_CASE("VampiricFang - consume heals", "[artifact]") {
    combat::VampiricFang vf(5, 1);
    for (int i = 0; i < 5; ++i) vf.onEnemyKilled({});
    CHECK(vf.pendingHeals() == 1);
    vf.consumeHeals();
    CHECK(vf.pendingHeals() == 0);
}

TEST_CASE("VampiricFang - reset clears state", "[artifact]") {
    combat::VampiricFang vf(2, 1);
    vf.onEnemyKilled({});
    vf.onEnemyKilled({});
    CHECK(vf.pendingHeals() == 1);
    vf.reset();
    CHECK(vf.pendingHeals() == 0);
}

TEST_CASE("VampiricFang - different rate", "[artifact]") {
    combat::VampiricFang vf2(3, 2);
    for (int i = 0; i < 3; ++i) vf2.onEnemyKilled({});
    CHECK(vf2.pendingHeals() == 2);
}

TEST_CASE("ExplosiveShells - stores pending explosion", "[artifact]") {
    combat::ExplosiveShells es;
    sf::Vector2f origin{100.f, 100.f};
    es.onAreaDamageRequest({origin, 70.f, 20, {0.f, 0.f}});
    CHECK(es.pendingExplosions().size() == 1);
    CHECK(es.pendingExplosions()[0].radius == 70.f);
    CHECK(es.pendingExplosions()[0].bonusDamage == 20);
}

TEST_CASE("ExplosiveShells - clearExplosions works", "[artifact]") {
    combat::ExplosiveShells es;
    es.onAreaDamageRequest({{100.f, 100.f}, 70.f, 20, {0.f, 0.f}});
    CHECK(!es.pendingExplosions().empty());
    es.clearExplosions();
    CHECK(es.pendingExplosions().empty());
}

TEST_CASE("ExplosiveShells - reset clears explosions", "[artifact]") {
    combat::ExplosiveShells es;
    es.onAreaDamageRequest({{100.f, 100.f}, 70.f, 20, {0.f, 0.f}});
    es.reset();
    CHECK(es.pendingExplosions().empty());
}