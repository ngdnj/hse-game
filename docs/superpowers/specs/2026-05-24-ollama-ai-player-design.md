# Optional Ollama AI Player — Design

> **Goal:** Make the game controllable by a local LLM via Ollama, strictly opt-in via CLI flags.

---

## 1. Architecture Overview

The Player entity currently hard-codes keyboard input inside `Player::handleInput()`. We extract input into an **`IInputController` interface** and inject the concrete implementation from `main()`.

```
main()
  ├─ parse --ai / --model=<name>
  ├─ no flags  → new PlayerInputController()   // keyboard
  ├─ --ai      → new OllamaClient(...) → new AiInputController(...)
  └─ player.setController(ctrl)
```

The game loop never blocks on AI responses — `AiInputController` holds the last known action state and updates it asynchronously.

---

## 2. Interface Design

```cpp
// include/input/IInputController.hpp
class IInputController {
public:
    virtual ~IInputController() = default;
    virtual InputState poll() = 0;   // called every fixed timestep
};
```

```cpp
// InputState — what the player can do
struct InputState {
    sf::Vector2f moveDirection;  // normalized -1..1
    bool attack     = false;
    bool dash       = false;
    bool swapWeapon = false;
};
```

Two implementations:
- `PlayerInputController` — reads `sf::Keyboard` (WASD, Shift, Space, Q)
- `AiInputController` — holds an `OllamaClient&`, returns the latest `InputState` (guaranteed non-blocking)

---

## 3. Dependency Injection in main

1. Parse `argc/argv` with a simple helper (no external library).
2. `Player` gets a `std::unique_ptr<IInputController>` member set via `player.setController()`.
3. In the game loop, call `controller->poll()` and pass the `InputState` into `player.update(dt, input)`.

**Thread synchronization:** `OllamaClient` runs a `std::jthread` that calls the Ollama API and writes results to a `std::atomic<InputState>` (lock-free). `AiInputController::poll()` reads that atomic — no mutex, no locks, just `std::atomic<InputState>` loads.

---

## 4. Dependencies

Added via CMake `FetchContent` (header-only):

| Library | Purpose |
|---------|---------|
| `yhirose/cpp-httplib` | HTTP POST to `localhost:11434` |
| `nlohmann/json` | Parse/generate JSON |

```cmake
FetchContent_Declare(cpp-httplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git GIT_TAG v0.18.1 GIT_SHALLOW ON EXCLUDE_FROM_ALL)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github/nlohmann/json.git GIT_TAG v3.12.0 GIT_SHALLOW ON EXCLUDE_FROM_ALL)
```

---

## 5. GameStateObserver — State Serialization

```cpp
// include/ai/GameStateObserver.hpp
class GameStateObserver {
public:
    // Returns a compact JSON string for the AI prompt
    std::string serialize(const Player&, enemies, shooters, projectiles) const;

    // Example output:
    // {
    //   "player":{"hp":80,"pos":{"x":500,"y":700}},
    //   "weapon":"Shotgun",
    //   "dash_ready":true,
    //   "enemies":[
    //     {"pos":{"x":300,"y":400},"dist":360},
    //     {"pos":{"x":800,"y":200},"dist":640}
    //   ]
    // }
};
```

Token-efficient: no strings like "Player health is", no extra whitespace, minified.

---

## 6. OllamaClient — Async HTTP

```cpp
// include/ai/OllamaClient.hpp
class OllamaClient {
public:
    OllamaClient(std::string model, float queryIntervalSec = 0.5f);
    ~OllamaClient();

    // thread-safe: writes atomically
    std::optional<InputState> pollResult();  // returns latest, or nullopt

    // Call every game tick with current serialized state
    void sendState(std::string serializedJson);

private:
    // background thread using std::jthread
    void runLoop();  // infinite loop, stops on destruction
};
```

**Throttling:** Uses a high-resolution steady clock to rate-limit requests to once per `queryIntervalSec` (default 500ms). Ignores responses that arrive late (stale).

---

## 7. Resilience & Error Handling

- HTTP errors, network timeouts → `AiInputController` returns last known state
- Invalid JSON from Ollama → `std::expected<InputState, JsonParseError>` with graceful fallback
- Ollama not running → log warning once, continue with keyboard fallback
- `std::expected` is used for all parse results so errors don't throw exceptions

---

## 8. Testing Strategy

| Component | Test Type |
|-----------|-----------|
| `GameStateObserver` | Table-Driven unit tests (various mock states → expected JSON) |
| `OllamaClient` | Unit tests with mocked HTTP responses (intercept via dependency injection) |
| `AiInputController` | Unit test: verify `poll()` returns last value or nullopt |

---

## 9. File Map

```
include/
  input/IInputController.hpp        — interface
  input/PlayerInputController.hpp   — keyboard impl
  input/AiInputController.hpp       — AI wrapper, holds OllamaClient&
  ai/OllamaClient.hpp                — async HTTP client
  ai/GameStateObserver.hpp           — state serialization
  ai/InputState.hpp                 — data structure

src/
  input/PlayerInputController.cpp
  input/AiInputController.cpp
  ai/OllamaClient.cpp
  ai/GameStateObserver.cpp

tests/
  src/test_game_state_observer.cpp  — Table-Driven tests
  src/test_ollama_client.cpp        — mocked HTTP tests
```

---

## 10. CLI Contract

| Flag | Meaning |
|------|---------|
| (none) | Normal keyboard play |
| `--ai` | Enable AI controller |
| `--model=<name>` | Ollama model name (default: `qwen2.5:0.5b`) |
| `--help` | Print usage and exit |

Example: `./game --ai --model=llama3.2:1b`

---

## 11. Constraints

- Game runs at 60Hz — AI response takes >500ms → **never block the main thread**
- Strictly optional — keyboard is **always** the default
- Async via `std::jthread` + `std::atomic<InputState>`
- Single-header libraries via `FetchContent`