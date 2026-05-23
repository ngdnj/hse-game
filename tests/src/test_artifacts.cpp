#include <cassert>
#include <vector>
#include "core/Events.hpp"
#include "combat/Artifacts.hpp"

using namespace core;
using namespace combat;

int main() {
    // VampiricFang tests
    {
        VampiricFang vf(5, 1); // heal 1 per 5 kills

        // No heals until 5 kills
        for (int i = 0; i < 4; ++i) {
            vf.onEnemyKilled({i + 1});
            assert(vf.pendingHeals() == 0);
        }

        // Kill 5 should trigger heal
        vf.onEnemyKilled({5});
        assert(vf.pendingHeals() == 1);

        // Consume heals
        assert(vf.pendingHeals() == 1);
        vf.consumeHeals();
        assert(vf.pendingHeals() == 0);

        // Next batch of 5
        for (int i = 0; i < 5; ++i) vf.onEnemyKilled({});
        assert(vf.pendingHeals() == 1);

        // Reset
        vf.reset();
        assert(vf.pendingHeals() == 0);
    }

    // VampiricFang with different rates
    {
        VampiricFang vf2(3, 2); // heal 2 per 3 kills
        for (int i = 0; i < 3; ++i) vf2.onEnemyKilled({});
        assert(vf2.pendingHeals() == 2);
    }

    // ExplosiveShells test
    {
        ExplosiveShells es;
        sf::Vector2f origin{100.f, 100.f};
        es.onAreaDamageRequest({origin, 70.f, 20, {0.f, 0.f}});
        assert(es.pendingExplosions().size() == 1);
        assert(es.pendingExplosions()[0].radius == 70.f);
        assert(es.pendingExplosions()[0].bonusDamage == 20);
        es.clearExplosions();
        assert(es.pendingExplosions().empty());
    }

    // Reset test
    {
        VampiricFang vf3(2, 1);
        vf3.onEnemyKilled({});
        vf3.onEnemyKilled({});
        assert(vf3.pendingHeals() == 1);
        vf3.reset();
        assert(vf3.pendingHeals() == 0);
    }

    printf("All artifact tests passed!\n");
    return 0;
}