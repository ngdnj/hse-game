#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <optional>
#include <chrono>
#include <mutex>
#include "input/InputState.hpp"

class OllamaClient {
public:
    OllamaClient(std::string model, float queryIntervalSec = 0.5f);
    ~OllamaClient();

    // Thread-safe: writes atomically
    std::optional<InputState> pollResult();

    // Call this each game tick with current serialized state
    void sendState(std::string serializedJson);

    bool isRunning() const { return running_.load(std::memory_order_acquire); }

private:
    void runLoop(std::stop_token st);

    std::string model_;
    float queryIntervalSec_;
    std::atomic<bool> running_{false};
    std::jthread thread_;

    // Pending state from game thread
    std::string pendingState_;
    std::atomic<bool> newState_{false};
    std::atomic<bool> busy_{false};

    // Latest parsed result (written by bg thread, read by game thread)
    std::atomic<InputState> latestState_{InputState::idle()};
    std::atomic<bool> hasResult_{false};

    // Throttle clock
    std::atomic<std::chrono::steady_clock::time_point> lastQuery_;
    std::mutex stateMutex_;
};