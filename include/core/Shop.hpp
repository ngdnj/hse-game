#pragma once

#include "core/Inventory.hpp"
#include "entities/Player.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace core {

// One purchasable upgrade. cost grows with level: baseCost + level * costStep.
struct Upgrade {
    std::string name;
    std::string description;
    int level{0};
    int baseCost{5};
    int costStep{5};
    int maxLevel{10};

    std::function<void(::Player&, int newLevel)> apply;

    [[nodiscard]] int currentCost() const noexcept { return baseCost + level * costStep; }
    [[nodiscard]] bool maxedOut() const noexcept { return level >= maxLevel; }
};

class Shop {
public:
    static constexpr const char* kCurrency = "coin";
    static constexpr std::size_t kNumUpgrades = 5;

    Shop();

    bool buy(std::size_t index, ::Player& player, Inventory& inv);

    [[nodiscard]] const Upgrade& at(std::size_t i) const { return upgrades_[i]; }
    [[nodiscard]] std::size_t size() const noexcept { return upgrades_.size(); }

private:
    std::array<Upgrade, kNumUpgrades> upgrades_;
};

} // namespace core