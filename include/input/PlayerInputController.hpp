#pragma once
#include "IInputController.hpp"

class PlayerInputController : public IInputController {
public:
    InputState poll() override;
};