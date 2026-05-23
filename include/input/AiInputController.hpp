#pragma once
#include "IInputController.hpp"
#include <memory>

class OllamaClient;

class AiInputController : public IInputController {
public:
    explicit AiInputController(std::shared_ptr<OllamaClient> client);

    InputState poll() override;

private:
    std::shared_ptr<OllamaClient> client_;
    InputState lastState_ = InputState::idle();
};
