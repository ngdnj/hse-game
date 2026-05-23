#pragma once

#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace core {

// Represents a stackable or non-stackable item
struct ItemData {
    enum class Rarity { Common, Uncommon, Rare, Epic } rarity = Rarity::Common;
    int stackSize = 1;
    int maxStack = 99;
};

// Simple inventory with fixed capacity, stack support
class Inventory {
public:
    explicit Inventory(std::size_t capacity = 20) : slots_(capacity) {}

    // Returns true if item was added (stacked or placed in empty slot)
    bool addItem(std::string name, ItemData data = {}) {
        if (name.empty()) return false;

        // Try to stack first
        if (data.stackSize > 1) {
            for (auto& slot : slots_) {
                if (slot.name == name && slot.data.stackSize < slot.data.maxStack) {
                    const int space = static_cast<int>(slot.data.maxStack - slot.data.stackSize);
                    const int toAdd = std::min(space, data.stackSize);
                    slot.data.stackSize += toAdd;
                    data.stackSize -= toAdd;
                    if (data.stackSize <= 0) return true;
                }
            }
        }

        // Find empty slot
        for (auto& slot : slots_) {
            if (slot.name.empty()) {
                slot.name = std::move(name);
                slot.data = data;
                return true;
            }
        }
        return false; // no space
    }

    // Returns the total count of an item
    [[nodiscard]] int count(std::string_view name) const {
        int total = 0;
        for (const auto& slot : slots_) {
            if (slot.name == name) total += slot.data.stackSize;
        }
        return total;
    }

    // Remove items, returns actual removed count
    int remove(std::string_view name, int amount) {
        int remaining = amount;
        for (auto& slot : slots_) {
            if (slot.name == name && slot.data.stackSize > 0) {
                const int toRemove = std::min(remaining, slot.data.stackSize);
                slot.data.stackSize -= toRemove;
                remaining -= toRemove;
                if (slot.data.stackSize <= 0) slot.name.clear();
                if (remaining <= 0) break;
            }
        }
        return amount - remaining;
    }

    // Iterate items (name, count pairs)
    template<typename F>
    void forEach(F&& fn) const {
        for (const auto& slot : slots_) {
            if (!slot.name.empty() && slot.data.stackSize > 0) {
                fn(slot.name, slot.data);
            }
        }
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
    [[nodiscard]] std::size_t usedSlots() const {
        return std::count_if(slots_.begin(), slots_.end(),
            [](const auto& s) { return !s.name.empty() && s.data.stackSize > 0; });
    }

private:
    struct Slot {
        std::string name;
        ItemData data;
    };
    std::vector<Slot> slots_;
};

} // namespace core