#include "input/AiInputController.hpp"
#include "ai/OllamaClient.hpp"

AiInputController::AiInputController(std::shared_ptr<OllamaClient> client)
    : client_(std::move(client)) {}

InputState AiInputController::poll() {
    if (!client_) return InputState::idle();
    if (auto result = client_->pollResult()) {
        lastState_ = *result;
    }
    return lastState_;
}
