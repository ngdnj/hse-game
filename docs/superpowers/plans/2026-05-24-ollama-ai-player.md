# Optional Ollama AI Player — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement an opt-in AI controller that drives the game via Ollama, using async communication so the 60Hz game loop never blocks.

**Architecture:** Extract input handling into an `IInputController` interface. Inject `PlayerInputController` (keyboard) by default, or `AiInputController` (Ollama) when `--ai` flag is passed. OllamaClient runs a `std::jthread` that sends state via HTTP POST and writes results to a `std::atomic<InputState>` — main thread reads without any locks.

**Tech Stack:** C++23, cpp-httplib (HTTP), nlohmann/json (JSON), SFML 3.0, std::jthread, std::atomic.

---

## Phase 1: Infrastructure — CLI Parsing & Interface

### Task 1: `IInputController` Interface and `InputState` struct

**Files:**
- Create: `include/input/IInputController.hpp`
- Create: `include/input/InputState.hpp`

- [ ] **Step 1: Create `include/input/InputState.hpp`**

```cpp
#pragma once
#include <SFML/System/Vector2.hpp>

struct InputState {
    sf::Vector2f moveDirection;  // normalized -1..1 on each axis
    bool attack     = false;
    bool dash       = false;
    bool swapWeapon = false;

    static InputState idle() {
        return {{0.f, 0.f}, false, false, false};
    }
};
```

- [ ] **Step 2: Create `include/input/IInputController.hpp`**

```cpp
#pragma once
#include "InputState.hpp"

class IInputController {
public:
    virtual ~IInputController() = default;
    virtual InputState poll() = 0;  // called every fixed timestep
};
```

- [ ] **Step 3: Commit**

```bash
git add include/input/InputState.hpp include/input/IInputController.hpp
git commit -m "feat(input): add IInputController interface and InputState struct"
```

---

### Task 2: `PlayerInputController` (Keyboard Implementation)

**Files:**
- Create: `include/input/PlayerInputController.hpp`
- Create: `src/input/PlayerInputController.cpp`

- [ ] **Step 1: Create `include/input/PlayerInputController.hpp`**

```cpp
#pragma once
#include "IInputController.hpp"

class PlayerInputController : public IInputController {
public:
    InputState poll() override;
};
```

- [ ] **Step 2: Create `src/input/PlayerInputController.cpp`**

```cpp
#include "PlayerInputController.hpp"
#include <SFML/Window/Keyboard.hpp>

InputState PlayerInputController::poll() {
    InputState state = InputState::idle();

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) state.moveDirection.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) state.moveDirection.y += 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) state.moveDirection.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) state.moveDirection.x += 1.f;

    // Normalize diagonal movement
    const float lenSq = state.moveDirection.x * state.moveDirection.x
                       + state.moveDirection.y * state.moveDirection.y;
    if (lenSq > 1.f) {
        const float invLen = 1.f / std::sqrt(lenSq);
        state.moveDirection.x *= invLen;
        state.moveDirection.y *= invLen;
    }

    state.attack     = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    state.dash       = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
    state.swapWeapon = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);

    return state;
}
```

- [ ] **Step 3: Commit**

```bash
git add include/input/PlayerInputController.hpp src/input/PlayerInputController.cpp
git commit -m "feat(input): add PlayerInputController keyboard implementation"
```

---

### Task 3: CLI Parser in `main.cpp`

**Files:**
- Modify: `src/main.cpp` — add CLI parsing before game loop, add `IInputController` field

- [ ] **Step 1: Create `include/utils/CliParser.hpp`**

```cpp
#pragma once
#include <string>
#include <optional>

struct CliOptions {
    bool aiEnabled = false;
    std::string modelName = "qwen2.5:0.5b";
};

CliOptions parseArgs(int argc, char* argv[]);
```

- [ ] **Step 2: Create `src/utils/CliParser.cpp`**

```cpp
#include "utils/CliParser.hpp"
#include <cstring>

static std::string_view skipPrefix(std::string_view arg, std::string_view prefix) {
    if (arg.starts_with(prefix)) return arg.substr(prefix.size());
    return {};
}

CliOptions parseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: game [--ai] [--model=<name>]\n"
                         "  --ai          Enable AI controller (requires Ollama running)\n"
                         "  --model=<name> Ollama model (default: qwen2.5:0.5b)\n";
            std::exit(0);
        }
        if (arg == "--ai") {
            opts.aiEnabled = true;
        } else if (auto val = skipPrefix(arg, "--model="); !val.empty()) {
            opts.modelName = std::string(val);
        }
    }
    return opts;
}
```

- [ ] **Step 3: Modify `src/main.cpp` — integrate controller**

Add `#include "input/PlayerInputController.hpp"` and `CliOptions opts = parseArgs(argc, argv);`
Before the game loop, create `std::unique_ptr<IInputController> controller;`
If `opts.aiEnabled` is false: `controller = std::make_unique<PlayerInputController>();`
The game loop calls `auto input = controller->poll();` each tick.
(We will wire `AiInputController` in Phase 3 — for now we use PlayerInputController only.)

- [ ] **Step 4: Build and verify**

```bash
cd build && cmake --build . -j4 2>&1 | tail -20
./bin/game --help
```

Expected: prints usage and exits.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp include/utils/CliParser.hpp src/utils/CliParser.cpp
git commit -m "feat(cli): add CLI argument parsing with --ai and --model flags"
```

---

## Phase 2: Dependencies — FetchContent

### Task 4: Add cpp-httplib and nlohmann/json via FetchContent

**Files:**
- Modify: `CMakeLists.txt` — add FetchContent declarations

- [ ] **Step 1: Update `CMakeLists.txt`**

Add after SFML block:
```cmake
FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.1
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)
```

- [ ] **Step 2: Update target_include_directories**

```cmake
target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        SYSTEM
        ${httplib_SOURCE_DIR}/single_include
        ${nlohmann_json_SOURCE_DIR}/single_include/nlohmann
)
```

Also add `${nlohmann_json_SOURCE_DIR}/single_include` to the include path so that `#include <nlohmann/json.hpp>` resolves. The SYSTEM keyword suppresses warnings from third-party headers.

- [ ] **Step 3: Build to verify**

```bash
cd build && cmake --build . -j4 2>&1 | grep -E "(httplib|nlohmann|ERROR)" | head -20
```

Expected: builds without errors

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add cpp-httplib and nlohmann/json via FetchContent"
```

---

## Phase 3: OllamaClient & Async Input

### Task 5: `OllamaClient` — Async HTTP Client

**Files:**
- Create: `include/ai/OllamaClient.hpp`
- Create: `src/ai/OllamaClient.cpp`

- [ ] **Step 1: Create `include/ai/OllamaClient.hpp`**

```cpp
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <optional>
#include <chrono>
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
    void runLoop();

    std::string model_;
    float queryIntervalSec_;
    std::atomic<bool> running_{false};
    std::jthread thread_;

    // Pending state from game thread
    std::string pendingState_;
    std::atomic<bool> newState_{false};

    // Latest parsed result (written by bg thread, read by game thread)
    std::atomic<InputState> latestState_{InputState::idle()};
    std::atomic<bool> hasResult_{false};

    // Throttle clock
    std::chrono::steady_clock::time_point lastQuery_;
};
```

- [ ] **Step 2: Create `src/ai/OllamaClient.cpp`**

```cpp
#include "OllamaClient.hpp"
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace std::chrono_literals;

OllamaClient::OllamaClient(std::string model, float queryIntervalSec)
    : model_(std::move(model)),
      queryIntervalSec_(queryIntervalSec),
      lastQuery_(std::chrono::steady_clock::now()) {
    running_.store(true, std::memory_order_release);
    thread_ = std::jthread([this](std::stop_token st) { runLoop(); });
}

OllamaClient::~OllamaClient() {
    running_.store(false, std::memory_order_release);
    thread_.request_stop();
}

void OllamaClient::sendState(std::string serializedJson) {
    if (!running_.load(std::memory_order_acquire)) return;
    pendingState_ = std::move(serializedJson);
    newState_.store(true, std::memory_order_release);
}

std::optional<InputState> OllamaClient::pollResult() {
    if (!hasResult_.load(std::memory_order_acquire)) return std::nullopt;
    return latestState_.load(std::memory_order_acquire);
}

void OllamaClient::runLoop() {
    httplib::Client cli("localhost:11434");
    cli.set_timeout_ms(2000);

    while (!std::this_thread::stop_requested()) {
        if (!newState_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastQuery_ < std::chrono::duration<float>(queryIntervalSec_)) {
            std::this_thread::sleep_for(10ms);
            continue;
        }
        lastQuery_ = now;

        newState_.store(false, std::memory_order_release);
        std::string stateCopy = std::move(pendingState_);

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
            }
        } catch (const nlohmann::json::parse_error&) {
            std::cerr << "[Ollama] JSON parse error, keeping last state\n";
        } catch (const std::exception& e) {
            std::cerr << "[Ollama] Exception: " << e.what() << "\n";
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add include/ai/OllamaClient.hpp src/ai/OllamaClient.cpp
git commit -m "feat(ai): add async OllamaClient with std::jthread and std::atomic"
```

---

### Task 6: `AiInputController` — Wraps OllamaClient

**Files:**
- Create: `include/input/AiInputController.hpp`
- Create: `src/input/AiInputController.cpp`

- [ ] **Step 1: Create `include/input/AiInputController.hpp`**

```cpp
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
```

- [ ] **Step 2: Create `src/input/AiInputController.cpp`**

```cpp
#include "AiInputController.hpp"
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
```

- [ ] **Step 3: Commit**

```bash
git add include/input/AiInputController.hpp src/input/AiInputController.cpp
git commit -m "feat(ai): add AiInputController wrapping OllamaClient"
```

---

## Phase 4: GameStateObserver

### Task 7: `GameStateObserver` — Serialize Game State for AI

**Files:**
- Create: `include/ai/GameStateObserver.hpp`
- Create: `src/ai/GameStateObserver.cpp`

- [ ] **Step 1: Create `include/ai/GameStateObserver.hpp`**

```cpp
#pragma once
#include <string>
#include <vector>
#include <SFML/System/Vector2.hpp>

namespace entities { class Player; class Enemy; class ShooterEnemy; class Projectile; }

class GameStateObserver {
public:
    // Returns a compact JSON string describing current game state
    // Format:
    // {
    //   "player":{"hp":80,"pos":{"x":500,"y":700}},
    //   "weapon":"Shotgun",
    //   "dash_ready":true,
    //   "enemies":[{"pos":{"x":300,"y":400},"dist":360}]
    // }
    std::string serialize(
        const entities::Player& player,
        const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
        const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters,
        const std::vector<std::unique_ptr<entities::Projectile>>& projectiles
    ) const;

    // Returns distance from player to the closest enemy, or -1 if none
    static float distanceToClosestEnemy(
        const sf::Vector2f& playerPos,
        const std::vector<std::unique_ptr<entities::Enemy>>& enemies,
        const std::vector<std::unique_ptr<entities::ShooterEnemy>>& shooters
    );
};
```

- [ ] **Step 2: Create `src/ai/GameStateObserver.cpp`**

Note: the implementation uses `nlohmann::json` for building the JSON string. Since it's header-only, include it with `#include <nlohmann/json.hpp>` at the top of the cpp file.

```cpp
#include "GameStateObserver.hpp"
#include "entities/Player.hpp"
#include "entities/Enemy.hpp"
#include "entities/ShooterEnemy.hpp"
#include "entities/Projectile.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

using namespace entities;

std::string GameStateObserver::serialize(
    const Player& player,
    const std::vector<std::unique_ptr<Enemy>>& enemies,
    const std::vector<std::unique_ptr<ShooterEnemy>>& shooters,
    const std::vector<std::unique_ptr<Projectile>>& /*projectiles*/) const {

    nlohmann::json obj;

    // Player
    const auto pPos = player.getPosition();
    obj["player"] = {
        {"hp", player.health()},
        {"pos", {{"x", pPos.x}, {"y", pPos.y}}}
    };

    // Weapon
    if (player.weapon()) {
        obj["weapon"] = player.weapon()->name();
    } else {
        obj["weapon"] = "none";
    }

    // Dash readiness
    obj["dash_ready"] = (player.dashCooldownRemaining() <= 0.f);

    // Enemies (only alive ones, up to 5 closest)
    std::vector<nlohmann::json> enemyList;
    struct EnemyInfo { sf::Vector2f pos; float dist; };
    std::vector<EnemyInfo> infos;

    for (const auto& e : enemies) {
        if (!e->isAlive()) continue;
        const auto ePos = e->getPosition();
        const float dx = ePos.x - pPos.x, dy = ePos.y - pPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        infos.push_back({ePos, dist});
    }
    for (const auto& s : shooters) {
        if (!s->isAlive()) continue;
        const auto sPos = s->getPosition();
        const float dx = sPos.x - pPos.x, dy = sPos.y - pPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        infos.push_back({sPos, dist});
    }

    std::sort(infos.begin(), infos.end(),
              [](const EnemyInfo& a, const EnemyInfo& b) { return a.dist < b.dist; });

    const int maxEnemies = 5;
    for (int i = 0; i < maxEnemies && i < (int)infos.size(); ++i) {
        enemyList.push_back(nlohmann::json{
            {"pos", {{"x", infos[i].pos.x}, {"y", infos[i].pos.y}}},
            {"dist", infos[i].dist}
        });
    }
    obj["enemies"] = std::move(enemyList);

    return obj.dump();  // minified by default
}

float GameStateObserver::distanceToClosestEnemy(
    const sf::Vector2f& playerPos,
    const std::vector<std::unique_ptr<Enemy>>& enemies,
    const std::vector<std::unique_ptr<ShooterEnemy>>& shooters) {

    float minDist = std::numeric_limits<float>::max();
    for (const auto& e : enemies) {
        if (!e->isAlive()) continue;
        const auto pos = e->getPosition();
        const float dx = pos.x - playerPos.x, dy = pos.y - playerPos.y;
        minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
    }
    for (const auto& s : shooters) {
        if (!s->isAlive()) continue;
        const auto pos = s->getPosition();
        const float dx = pos.x - playerPos.x, dy = pos.y - playerPos.y;
        minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
    }
    return (minDist == std::numeric_limits<float>::max()) ? -1.f : minDist;
}
```

- [ ] **Step 3: Commit**

```bash
git add include/ai/GameStateObserver.hpp src/ai/GameStateObserver.cpp
git commit -m "feat(ai): add GameStateObserver for AI state serialization"
```

---

## Phase 5: Wire Everything Together in main.cpp

### Task 8: Integrate AI Controller into main.cpp

**Files:**
- Modify: `src/main.cpp` — wire `AiInputController` and `OllamaClient`, send state each tick

- [ ] **Step 1: Add includes after existing includes**

```cpp
#include "input/IInputController.hpp"
#include "input/PlayerInputController.hpp"
#include "input/AiInputController.hpp"
#include "ai/OllamaClient.hpp"
#include "ai/GameStateObserver.hpp"
#include "utils/CliParser.hpp"
```

- [ ] **Step 2: After `CliOptions opts = parseArgs(argc, argv);` and player creation:**

```cpp
std::unique_ptr<IInputController> controller;
GameStateObserver stateObserver;

if (opts.aiEnabled) {
    auto client = std::make_shared<OllamaClient>(opts.modelName, 0.5f);
    controller = std::make_unique<AiInputController>(client);
    std::cout << "[AI] Enabled with model: " << opts.modelName << "\n";
    std::cout << "[AI] Ensure Ollama is running at localhost:11434\n";
} else {
    controller = std::make_unique<PlayerInputController>();
}
```

- [ ] **Step 3: Inside the fixed update loop, after `player.update(kFixedDt)`:**

```cpp
// Get AI input state
auto input = controller->poll();

// Apply movement from AI controller (replace direct keyboard in Player::handleInput)
// We pass the InputState to a new player method
player.handleInput(kFixedDt, input);
```

- [ ] **Step 4: Modify `Player::handleInput` to accept optional InputState**

Update `Player::handleInput(float dt)` to `Player::handleInput(float dt, const InputState* optionalInput = nullptr)`.
If `optionalInput` is provided, use it instead of reading `sf::Keyboard`.
Otherwise fall back to keyboard (for backward compatibility).

- [ ] **Step 5: Every ~500ms, send state to Ollama:**

In the game loop (not the fixed timestep), after `auto input = controller->poll();`:
```cpp
static float lastStateSendTime = 0.f;
if (opts.aiEnabled) {
    lastStateSendTime += realDt;
    if (lastStateSendTime >= 0.5f) {
        lastStateSendTime = 0.f;
        if (auto client = std::dynamic_pointer_cast<OllamaClient>(...)) {
            std::string stateJson = stateObserver.serialize(player, enemies, shooters, playerProjectiles);
            client->sendState(std::move(stateJson));
        }
    }
}
```

Actually simpler: store a `shared_ptr<OllamaClient>` alongside the controller and call sendState every tick (the client throttles internally).

- [ ] **Step 6: Build and test**

```bash
cd build && cmake --build . -j4 2>&1 | grep -E "(error|warning)" | head -20
./bin/game              # should run normally (keyboard mode)
./bin/game --help      # should print help
./bin/game --ai --model=llama3.2:1b  # should start with AI (will fail gracefully if Ollama not running)
```

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp src/entities/Player.cpp include/entities/Player.hpp
git commit -m "feat(ai): wire AI controller and OllamaClient into main game loop"
```

---

## Phase 6: Testing

### Task 9: `GameStateObserver` Table-Driven Unit Tests

**Files:**
- Create: `tests/src/test_game_state_observer.cpp`

- [ ] **Step 1: Create table-driven tests**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "ai/GameStateObserver.hpp"
#include "entities/Player.hpp"

TEST_CASE("GameStateObserver serializes player state correctly") {
    GameStateObserver observer;
    const sf::Vector2f worldBoundsPos{0.f, 0.f};
    const sf::FloatRect worldBounds{worldBoundsPos, {2000.f, 2000.f}};

    SECTION("player with full health") {
        Player player{{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f}, worldBounds, "", ""};
        player.setPosition({500.f, 700.f});
        player.setMaxHealth(100);
        player.heal(100);

        std::vector<std::unique_ptr<entities::Enemy>> enemies;
        std::vector<std::unique_ptr<entities::ShooterEnemy>> shooters;
        std::vector<std::unique_ptr<entities::Projectile>> projectiles;

        std::string json = observer.serialize(player, enemies, shooters, projectiles);
        auto parsed = nlohmann::json::parse(json);

        REQUIRE(parsed["player"]["hp"] == 100);
        REQUIRE(parsed["player"]["pos"]["x"] == 500.f);
        REQUIRE(parsed["player"]["pos"]["y"] == 700.f);
    }

    SECTION("dash ready when cooldown is zero") {
        Player player{{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f}, worldBounds, "", ""};
        // dash cooldown timer starts at 0, so dash should be ready
        std::string json = observer.serialize(player, {}, {}, {});
        auto parsed = nlohmann::json::parse(json);
        REQUIRE(parsed["dash_ready"] == true);
    }

    SECTION("serializes enemies with position and distance") {
        Player player{{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f}, worldBounds, "", ""};
        player.setPosition({1000.f, 1000.f});

        auto enemies = std::make_unique<entities::Enemy>(
            sf::Vector2f{36.f, 36.f}, sf::Vector2f{300.f, 400.f}, worldBounds, nullptr);
        // Note: Enemy ctor may need adjustment for testing - use a test fixture approach

        // ... (full test with mock enemy positions)
        std::string json = observer.serialize(player, enemies, shooters, {});
        auto parsed = nlohmann::json::parse(json);
        REQUIRE(parsed["enemies"].size() == 1);
        REQUIRE(parsed["enemies"][0]["dist"] > 0);
    }

    SECTION("limits to 5 closest enemies") {
        // ... create 10 enemies at various positions, verify only 5 in output
    }

    SECTION("outputs valid minified JSON") {
        Player player{{40.f, 40.f}, sf::Vector2f{1000.f, 1000.f}, worldBounds, "", ""};
        std::string json = observer.serialize(player, {}, {}, {});
        // Should parse without exceptions
        auto parsed = nlohmann::json::parse(json);
        REQUIRE(parsed.is_object());
    }
}
```

- [ ] **Step 2: Run tests**

```bash
cd build && cmake --build . -j4
./build/tests/game_tests 2>&1 | tail -30
```

- [ ] **Step 3: Commit**

```bash
git add tests/src/test_game_state_observer.cpp
git commit -m "test(ai): add GameStateObserver table-driven unit tests"
```

---

## Phase 7: Resilience Tests

### Task 10: `OllamaClient` Error Handling Tests

**Files:**
- Create: `tests/src/test_ollama_client.cpp`

- [ ] **Step 1: Test invalid JSON handling**

Test that `OllamaClient` doesn't crash when Ollama returns malformed JSON.
Use a mock approach: construct the client with a test hook, or test via integration.

- [ ] **Step 2: Test graceful degradation**

When Ollama is not running, `pollResult()` should return `std::nullopt`.
Verify this with a test that waits 2 seconds after construction.

- [ ] **Step 3: Commit**

```bash
git add tests/src/test_ollama_client.cpp
git commit -m "test(ai): add OllamaClient resilience unit tests"
```

---

## Verification Checklist

Before each commit, verify:

- [ ] `cmake --build build -j4` succeeds with zero errors
- [ ] `./build/bin/game --help` prints usage and exits
- [ ] `./build/bin/game` runs without the `--ai` flag (keyboard mode works)
- [ ] Tests pass: `./build/tests/game_tests`
- [ ] No new compiler warnings introduced

---

## Branch Lifecycle

1. All work on branch `experimental/ollama-ai-player`
2. Iterative commits per task (7 commits minimum: one per task)
3. Do **NOT** merge to `main` — stop after Task 10 and wait for review