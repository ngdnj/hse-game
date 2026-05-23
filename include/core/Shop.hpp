#pragma once

#include "core/Inventory.hpp"
#include "entities/Player.hpp"
#include <array>
#include <functional>
#include <string>

namespace core {

// One purchasable upgrade. cost grows with level: baseCost + level * costStep.
struct Upgrade {
    std::string name;
    std::string description;
    int level{0};
    int baseCost{5};
    int costStep{5};
    int maxLevel{10};

    // Applied when purchased. Receives Player ref so it can mutate stats.
    std::function<void(::Player&, int newLevel)> apply;

    [[nodiscard]] int currentCost() const noexcept { return baseCost + level * costStep; }
    [[nodiscard]] bool maxedOut() const noexcept { return level >= maxLevel; }
};

// Three-upgrade shop. Currency = "coin" in the inventory.
class Shop {
public:
    static constexpr const char* kCurrency = "coin";
    static constexpr std::size_t kNumUpgrades = 3;

    Shop() {
        upgrades_[0] = Upgrade{
            .name = "Max HP",
            .description = "+20 max health",
            .baseCost = 5, .costStep = 5,
            .apply = [](::Player& p, int) {
                p.setMaxHealth(p.maxHealth() + 20);
                p.heal(20);
            }};
        upgrades_[1] = Upgrade{
            .name = "Move Speed",
            .description = "+30 speed",
            .baseCost = 8, .costStep = 6,
            .apply = [](::Player& p, int) {
                p.setSpeed(p.getSpeed() + 30.f);
            }};
        upgrades_[2] = Upgrade{
            .name = "Attack Damage",
            .description = "+10 damage",
            .baseCost = 10, .costStep = 8,
            .apply = [](::Player& p, int) {
                p.setAttackDamage(p.attackDamage() + 10);
            }};
    }

    // Attempts to buy upgrade at index. Returns true if purchased.
    bool buy(std::size_t index, ::Player& player, Inventory& inv) {
        if (index >= upgrades_.size()) return false;
        Upgrade& u = upgrades_[index];
        if (u.maxedOut()) return false;
        const int cost = u.currentCost();
        if (inv.count(kCurrency) < cost) return false;
        if (inv.remove(kCurrency, cost) != cost) return false;
        ++u.level;
        if (u.apply) u.apply(player, u.level);
        return true;
    }

    [[nodiscard]] const Upgrade& at(std::size_t i) const { return upgrades_[i]; }
    [[nodiscard]] std::size_t size() const noexcept { return upgrades_.size(); }

private:
    std::array<Upgrade, kNumUpgrades> upgrades_;
};

} // namespace core
