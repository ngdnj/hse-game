#pragma once

#include "core/Events.hpp"
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace core {

class EventBus {
public:
    template <typename Event>
    using Handler = std::function<void(const Event&)>;

    template <typename Event>
    void subscribe(Handler<Event> handler) {
        auto idx = std::type_index(typeid(Event));
        auto& vec = subscribers_[idx];
        vec.push_back([h = std::move(handler)](const void* e) {
            h(*static_cast<const Event*>(e));
        });
    }

    template <typename Event>
    void emit(const Event& event) {
        auto idx = std::type_index(typeid(Event));
        auto it = subscribers_.find(idx);
        if (it == subscribers_.end()) return;
        for (auto& fn : it->second) {
            fn(&event);
        }
    }

    void reset() { subscribers_.clear(); }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> subscribers_;
};

} // namespace core