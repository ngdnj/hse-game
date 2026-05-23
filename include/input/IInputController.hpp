#pragma once
#include "InputState.hpp"

class IInputController {
public:
    virtual ~IInputController() = default;
    virtual InputState poll() = 0;  // called every fixed timestep
};