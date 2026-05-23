#include <catch2/catch_test_macros.hpp>
#include "core/Inventory.hpp"
#include <string>

TEST_CASE("Inventory initial state", "[inventory]") {
    core::Inventory inv(20);
    REQUIRE(inv.capacity() == 20);
    REQUIRE(inv.usedSlots() == 0);
    REQUIRE(inv.count("coin") == 0);
}

TEST_CASE("Inventory addItem single item", "[inventory]") {
    core::Inventory inv(20);
    bool added = inv.addItem("coin", {.stackSize = 5});
    REQUIRE(added == true);
    REQUIRE(inv.count("coin") == 5);
    REQUIRE(inv.usedSlots() == 1);
}

TEST_CASE("Inventory addItem stacks into existing", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    inv.addItem("coin", {.stackSize = 3});
    REQUIRE(inv.count("coin") == 8);
    REQUIRE(inv.usedSlots() == 1);
}

TEST_CASE("Inventory addItem fills to maxStack then new slot", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5, .maxStack = 5});
    bool second = inv.addItem("coin", {.stackSize = 5, .maxStack = 5});
    REQUIRE(second == true);
    REQUIRE(inv.count("coin") == 10); // 5 + 5 across two slots
    REQUIRE(inv.usedSlots() == 2);
}

TEST_CASE("Inventory addItem overfills remaining space then uses new slot", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 8, .maxStack = 10});
    bool second = inv.addItem("coin", {.stackSize = 5, .maxStack = 10});
    REQUIRE(second == true);
    REQUIRE(inv.count("coin") == 13); // 8 in slot1, 5 in slot2
}

TEST_CASE("Inventory addItem rejects when full", "[inventory]") {
    core::Inventory inv(2);
    inv.addItem("coin", {.stackSize = 5, .maxStack = 5});
    inv.addItem("coin", {.stackSize = 5, .maxStack = 5});
    bool overflow = inv.addItem("coin", {.stackSize = 5, .maxStack = 5});
    REQUIRE(overflow == false);
    REQUIRE(inv.count("coin") == 10);
    REQUIRE(inv.usedSlots() == 2);
}

TEST_CASE("Inventory remove reduces count", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 10});
    int removed = inv.remove("coin", 3);
    REQUIRE(removed == 3);
    REQUIRE(inv.count("coin") == 7);
}

TEST_CASE("Inventory remove cannot go negative", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    int removed = inv.remove("coin", 100);
    REQUIRE(removed == 5);
    REQUIRE(inv.count("coin") == 0);
}

TEST_CASE("Inventory remove partial from stacked slot", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 10});
    int removed = inv.remove("coin", 4);
    REQUIRE(removed == 4);
    REQUIRE(inv.count("coin") == 6);
    REQUIRE(inv.usedSlots() == 1);
}

TEST_CASE("Inventory forEach iterates correctly", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    inv.addItem("health_potion", {.stackSize = 2});

    int itemCount = 0;
    int totalItems = 0;
    inv.forEach([&](const std::string& name, const core::ItemData& data) {
        (void)name;
        ++itemCount;
        totalItems += data.stackSize;
    });
    REQUIRE(itemCount == 2);
    REQUIRE(totalItems == 7);
}

TEST_CASE("Inventory forEach skips empty slots", "[inventory]") {
    core::Inventory inv(5);
    inv.addItem("coin", {.stackSize = 3});

    int count = 0;
    inv.forEach([&](const std::string&, const core::ItemData& data) {
        count += data.stackSize;
    });
    REQUIRE(count == 3);
    REQUIRE(inv.usedSlots() == 1);
}

TEST_CASE("Inventory count of missing item is zero", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    REQUIRE(inv.count("health_potion") == 0);
}

TEST_CASE("Inventory usedSlots counts only non-empty", "[inventory]") {
    core::Inventory inv(10);
    REQUIRE(inv.usedSlots() == 0);
    inv.addItem("coin", {.stackSize = 1});
    REQUIRE(inv.usedSlots() == 1);
    inv.addItem("gems", {.stackSize = 1});
    REQUIRE(inv.usedSlots() == 2);
}

TEST_CASE("Inventory remove of missing item does nothing", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    int removed = inv.remove("nothing", 10);
    REQUIRE(removed == 0);
    REQUIRE(inv.count("coin") == 5);
}

TEST_CASE("Inventory multiple different items", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    inv.addItem("gems", {.stackSize = 3});
    inv.addItem("key", {.stackSize = 1});
    REQUIRE(inv.count("coin") == 5);
    REQUIRE(inv.count("gems") == 3);
    REQUIRE(inv.count("key") == 1);
    REQUIRE(inv.usedSlots() == 3);
}

TEST_CASE("Inventory empty name does not crash", "[inventory]") {
    core::Inventory inv(20);
    bool added = inv.addItem("", {.stackSize = 1});
    // Should not crash; verifies no exception thrown
    REQUIRE(inv.usedSlots() == (added ? 1 : 0));
}

TEST_CASE("Inventory remove zeroes slot", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("coin", {.stackSize = 5});
    inv.remove("coin", 5);
    REQUIRE(inv.count("coin") == 0);
    REQUIRE(inv.usedSlots() == 0);
}

TEST_CASE("Inventory addItem with unit stack size", "[inventory]") {
    core::Inventory inv(20);
    inv.addItem("gem", {.stackSize = 1, .maxStack = 1});
    inv.addItem("gem", {.stackSize = 1, .maxStack = 1});
    inv.addItem("gem", {.stackSize = 1, .maxStack = 1});
    REQUIRE(inv.count("gem") == 3);
    REQUIRE(inv.usedSlots() == 3);
}