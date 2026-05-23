#include "core/Shop.hpp"
#include "combat/Artifacts.hpp"
#include <utility>

namespace core {

Shop::Shop() {
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
    upgrades_[3] = Upgrade{
        .name = "Vampiric Fang",
        .description = "Heal 1 HP per 5 kills",
        .baseCost = 25, .costStep = 15,
        .apply = [](::Player&, int) {}};
    upgrades_[4] = Upgrade{
        .name = "Explosive Shells",
        .description = "20% chance: enemy death triggers AoE explosion",
        .baseCost = 30, .costStep = 20,
        .apply = [](::Player&, int) {}};
}

bool Shop::buy(std::size_t index, ::Player& player, Inventory& inv) {
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

} // namespace core