#include "ai/OllamaClient.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace std::chrono_literals;

OllamaClient::OllamaClient(std::string model, float queryIntervalSec)
    : model_(std::move(model)),
      queryIntervalSec_(queryIntervalSec),
      lastQuery_(std::chrono::steady_clock::now()) {
    running_.store(true, std::memory_order_release);
    thread_ = std::jthread([this](std::stop_token st) { runLoop(st); });
}

OllamaClient::~OllamaClient() {
    running_.store(false, std::memory_order_release);
    thread_.request_stop();
}

void OllamaClient::sendState(std::string serializedJson) {
    if (!running_.load(std::memory_order_acquire)) return;
    if (busy_.load(std::memory_order_acquire)) return; // drop if still processing
    std::lock_guard<std::mutex> lock(stateMutex_);
    pendingState_ = std::move(serializedJson);
    newState_.store(true, std::memory_order_release);
}

std::optional<InputState> OllamaClient::pollResult() {
    if (!hasResult_.load(std::memory_order_acquire)) return std::nullopt;
    return latestState_.load(std::memory_order_acquire);
}

void OllamaClient::runLoop(std::stop_token st) {
    httplib::Client cli("localhost:11434");
    cli.set_read_timeout(std::chrono::seconds(2));

    while (!st.stop_requested()) {
        if (!newState_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto lastQuery = lastQuery_.load(std::memory_order_acquire);
        if (now - lastQuery < std::chrono::duration<float>(queryIntervalSec_)) {
            std::this_thread::sleep_for(10ms);
            continue;
        }
        lastQuery_.store(now, std::memory_order_release);

        newState_.store(false, std::memory_order_release);
        busy_.store(true, std::memory_order_release);
        std::string stateCopy;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            stateCopy = std::move(pendingState_);
        }

        // Build JSON payload
        nlohmann::json payload = {
            {"model", model_},
            {"prompt", "You are an AI playing an action roguelite. Reply ONLY with a JSON object: {\"move_x\": -1/0/1, \"move_y\": -1/0/1, \"attack\": bool, \"dash\": bool, \"swap_weapon\": bool}. Current state:\n" + stateCopy},
            {"stream", false},
            {"options", {{"temperature", 0.0}}}
        };

        try {
            auto res = cli.Post("/api/generate", payload.dump(), "application/json");
            if (!res || res->status != 200) {
                std::cerr << "[Ollama] HTTP error: " << (res ? res->status : -1) << "\n";
                continue;
            }

            auto json = nlohmann::json::parse(res->body);
            std::string response = json.value("response", "");

            // Extract JSON object from response (handle markdown code blocks)
            auto startPos = response.find('{');
            auto endPos = response.find_last_of('}');
            if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
                std::string jsonStr = response.substr(startPos, endPos - startPos + 1);
                auto parsed = nlohmann::json::parse(jsonStr);

                InputState state = InputState::idle();
                state.moveDirection.x = parsed.value("move_x", 0);
                state.moveDirection.y = parsed.value("move_y", 0);
                state.attack     = parsed.value("attack", false);
                state.dash       = parsed.value("dash", false);
                state.swapWeapon = parsed.value("swap_weapon", false);

                latestState_.store(state, std::memory_order_release);
                hasResult_.store(true, std::memory_order_release);
                busy_.store(false, std::memory_order_release);
            }
        } catch (const nlohmann::json::parse_error&) {
            std::cerr << "[Ollama] JSON parse error, keeping last state\n";
            busy_.store(false, std::memory_order_release);
        } catch (const std::exception& e) {
            std::cerr << "[Ollama] Exception: " << e.what() << "\n";
            busy_.store(false, std::memory_order_release);
        }
    }
}