#include "entities/Player.hpp"
#include "core/Collision.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kRunState{"run"};
constexpr std::string_view kIdleState{"idle"};

// Нормализует вектор или возвращает нулевой, избегая деления на 0.
sf::Vector2f normalizeOrZero(const sf::Vector2f &v) {
    const float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= std::numeric_limits<float>::epsilon()) return {0.f, 0.f};
    const float invLen = 1.f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen};
}

bool hasInput(const sf::Vector2f &v) {
    return v.x != 0.f || v.y != 0.f;
}

int wrapToRange(int value, int range) {
    return range > 0 ? value % range : 0;
}

} // namespace

Player::Player(const sf::Vector2f &size, const sf::Vector2f &startPosition,
               const sf::FloatRect &movementBounds,
               const std::string &moveTexturePath,
               const std::string &idleTexturePath)
    : sprite_(placeholderTexture_), movementBounds_(movementBounds) {
    shape_.setSize(size);
    shape_.setFillColor(sf::Color(100, 200, 250));
    shape_.setOutlineColor(sf::Color::White);
    shape_.setOutlineThickness(2.f);
    shape_.setOrigin(sf::Vector2f{size.x * 0.5f, size.y * 0.5f});
    shape_.setPosition(sf::Vector2f{0.f, 0.f});

    setPosition(startPosition);

    (void)loadAnimation(std::string{kRunState}, moveTexturePath);
    (void)loadAnimation(std::string{kIdleState}, idleTexturePath);

    activeSheet_ = findSheet(std::string{kIdleState});
    if (!activeSheet_) activeSheet_ = findSheet(std::string{kRunState});
    if (activeSheet_) applySheet(*activeSheet_);
}

void Player::update(float dt) {
    if (!isActive()) return;

    handleInput(dt);
    updateAnimation(dt);
    clampToBounds();
}

sf::FloatRect Player::getLocalBounds() const {
    if (hasTexture_ && activeSheet_) {
        const sf::FloatRect spriteBounds = sprite_.getLocalBounds();
        return sf::FloatRect(spriteBounds.position, spriteBounds.size);
    }
    // For the rectangle shape, account for its origin.
    sf::Vector2f sz = shape_.getSize();
    sf::Vector2f origin = shape_.getOrigin();
    return sf::FloatRect(sf::Vector2f{-origin.x, -origin.y}, sz);
}

void Player::handleInput(float dt) {
    sf::Vector2f dir{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        dir.y -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
        dir.y += 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        dir.x -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        dir.x += 1.f;
    }
    if (hasInput(dir)) {
        const sf::Vector2f norm = normalizeOrZero(dir);
        const sf::Vector2f resolved = resolveMove(norm * speed_ * dt);
        if (resolved.x != 0.f || resolved.y != 0.f) {
            move(resolved);
        }
    }

    isMoving_ = hasInput(dir);
    updateDirection(dir);

    // Attack input (Space key)
    attackCooldown_ = std::max(0.f, attackCooldown_ - dt);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) && attackCooldown_ <= 0.f) {
        (void)attack();
    }
}

void Player::updateAnimation(float dt) {
    if (!hasTexture_) return;

    std::string nextState = hasOverride_
        ? overrideState_
        : (isMoving_ ? std::string{kRunState} : std::string{kIdleState});

    const Sheet *nextSheet = findSheet(nextState);

    if (!nextSheet && nextState != kRunState) {
        nextState = std::string{kRunState};
        nextSheet = findSheet(nextState);
    }
    if (!nextSheet && nextState != kIdleState) {
        nextState = std::string{kIdleState};
        nextSheet = findSheet(nextState);
    }
    if (!nextSheet && !sheets_.empty()) {
        nextState = sheets_.begin()->first;
        nextSheet = &sheets_.begin()->second;
    }

    if (nextSheet && activeSheet_ != nextSheet) {
        activeSheet_ = nextSheet;
        activeState_ = nextState;
        currentFrame_ = 0;
        frameTimer_ = 0.f;
        applySheet(*activeSheet_);
    }

    if (!activeSheet_ || activeSheet_->frameSize.x <= 0 ||
        activeSheet_->frameSize.y <= 0) {
        return;
    }

    const int frameCount = activeSheet_->framesPerRow;
    if (frameCount > 0) {
        frameTimer_ += dt;
        if (frameTimer_ >= frameDuration_) {
            frameTimer_ -= frameDuration_;
            currentFrame_ = (currentFrame_ + 1) % frameCount;
        }
    }

    const int col = frameCount > 0 ? wrapToRange(currentFrame_, frameCount) : 0;
    const int row = wrapToRange(currentRow_, std::max(1, activeSheet_->sheetGrid.y));
    applyFrame(*activeSheet_, col, row);
}

void Player::updateDirection(const sf::Vector2f &dir) {
    if (!hasTexture_ || !activeSheet_ || activeSheet_->frameSize.x <= 0 ||
        activeSheet_->frameSize.y <= 0)
        return;

    // Keep the last non-zero direction to preserve facing when idle.
    sf::Vector2f d = dir;
    if (d.x == 0.f && d.y == 0.f) {
        d = lastDir_;
    } else {
        lastDir_ = d;
    }

    // Map direction to one of 8 rows (clockwise from right, screen coords: +Y
    // down): 0: right, 1: down-right, 2: down, 3: down-left, 4: left, 5:
    // up-left, 6: up, 7: up-right.
    const float angle = std::atan2(d.y, d.x);                 // -pi..pi
    const float sectorSize = std::numbers::pi_v<float> / 4.f; // 45 degrees
    const float offset = sectorSize / 2.f;                    // center sectors
    int sector = static_cast<int>(std::floor((angle + offset) / sectorSize));
    sector = (sector % 8 + 8) % 8; // Normalize to [0,7]
    const int rowIdx = wrapToRange(
        rowIndexFor(static_cast<DirectionSector>(sector)),
        std::max(1, activeSheet_->sheetGrid.y));
    currentRow_ = rowIdx;
}

void Player::clampToBounds() {
    auto clampAxis = [](float entityMin, float entityMax, float boundMin,
                        float boundMax, float currentPos) {
        if (entityMin < boundMin) return currentPos + (boundMin - entityMin);
        if (entityMax > boundMax) return currentPos - (entityMax - boundMax);
        return currentPos;
    };

    const sf::FloatRect gb = getGlobalBounds();
    sf::Vector2f pos = getPosition();

    const float boundRight = movementBounds_.position.x + movementBounds_.size.x;
    const float boundBottom = movementBounds_.position.y + movementBounds_.size.y;

    pos.x = clampAxis(gb.position.x, gb.position.x + gb.size.x,
                      movementBounds_.position.x, boundRight, pos.x);
    pos.y = clampAxis(gb.position.y, gb.position.y + gb.size.y,
                      movementBounds_.position.y, boundBottom, pos.y);

    setPosition(pos);
}

sf::FloatRect Player::attack() noexcept {
    attackCooldown_ = attackDuration_;

    const auto pos = getPosition();
    sf::Vector2f dir{1.f, 0.f};
    if (lastDir_.x != 0.f || lastDir_.y != 0.f) {
        dir = normalizeOrZero(lastDir_);
    }

    const float halfW = attackRange_ * 0.5f;
    const float halfH = 30.f;
    const sf::Vector2f centerOffset = dir * attackRange_ * 0.5f;

    attackHitbox_ = sf::FloatRect{
        sf::Vector2f{pos.x + centerOffset.x - halfW, pos.y + centerOffset.y - halfH},
        sf::Vector2f{attackRange_, halfH * 2.f}
    };
    return attackHitbox_;
}

void Player::onDraw(sf::RenderTarget &target, sf::RenderStates states) const {
    if (hasTexture_) {
        target.draw(sprite_, states);
    } else {
        target.draw(shape_, states);
    }
}

bool Player::loadSheet(const std::string &name, const std::string &path) {
    if (path.empty()) return false;

    Sheet sheet;
    if (!sheet.texture.loadFromFile(path)) {
        std::cerr << "[Player] Failed to load texture: " << path
                  << " | cwd=" << std::filesystem::current_path() << "\n";
        return false;
    }

    sheet.loaded = true;
    sheet.sheetGrid = kDefaultGrid_;

    const sf::Vector2u texSz = sheet.texture.getSize();
    const sf::Image image = sheet.texture.copyToImage();

    // Находим последовательности полностью прозрачных линий (гаттеры) по
    // горизонтали или вертикали, чтобы автоматически вычислить сетку кадров.
    auto findRuns = [&](bool horizontal) {
        std::vector<std::pair<int, int>> runs;
        const int primary =
            horizontal ? static_cast<int>(texSz.x) : static_cast<int>(texSz.y);
        const int secondary =
            horizontal ? static_cast<int>(texSz.y) : static_cast<int>(texSz.x);
        auto isClear = [&](int idx) {
            for (int s = 0; s < secondary; ++s) {
                const auto px =
                    horizontal ? image.getPixel(
                                     sf::Vector2u{static_cast<unsigned>(idx),
                                                  static_cast<unsigned>(s)})
                               : image.getPixel(
                                     sf::Vector2u{static_cast<unsigned>(s),
                                                  static_cast<unsigned>(idx)});
                if (px.a != 0)
                    return false;
            }
            return true;
        };

        int i = 0;
        while (i < primary) {
            if (!isClear(i)) {
                ++i;
                continue;
            }
            const int start = i;
            while (i < primary && isClear(i))
                ++i;
            runs.emplace_back(start, i - 1);
        }
        return runs;
    };

    // Проверяем, что расстояния между прозрачными линиями одинаковые, и
    // вычисляем размер кадра и шаг по атласу.
    auto adoptRuns = [](const std::vector<std::pair<int, int>> &runs,
                        int &frame, int &stride) {
        if (runs.size() < 2)
            return false;
        const int gutter = runs.front().second - runs.front().first + 1;
        frame = runs[1].first - runs.front().second - 1;
        stride = runs[1].first - runs.front().first;
        if (gutter <= 0 || frame <= 0 || stride <= 0)
            return false;

        for (std::size_t i = 1; i < runs.size(); ++i) {
            const int g = runs[i].second - runs[i].first + 1;
            if (g != gutter)
                return false;
            if (i + 1 < runs.size()) {
                const int f = runs[i + 1].first - runs[i].second - 1;
                const int s = runs[i + 1].first - runs[i].first;
                if (f != frame || s != stride)
                    return false;
            }
        }
        return true;
    };

    const auto runsX = findRuns(true);
    const auto runsY = findRuns(false);
    int frameW = 0, frameH = 0;
    int strideX = 0, strideY = 0;
    const bool detected =
        adoptRuns(runsX, frameW, strideX) && adoptRuns(runsY, frameH, strideY);

    if (detected) {
        sheet.sheetGrid.x = static_cast<int>(runsX.size()) - 1;
        sheet.sheetGrid.y = static_cast<int>(runsY.size()) - 1;
        sheet.frameSize = sf::Vector2i{frameW, frameH};
        sheet.frameStart =
            sf::Vector2i{runsX.front().second + 1, runsY.front().second + 1};
        sheet.frameStride = sf::Vector2i{strideX, strideY};
        sheet.framesPerRow = sheet.sheetGrid.x;
    } else {
        sheet.frameSize = sf::Vector2i{
            static_cast<int>(texSz.x) / std::max(1, sheet.sheetGrid.x),
            static_cast<int>(texSz.y) / std::max(1, sheet.sheetGrid.y)};
        sheet.frameStart = sf::Vector2i{0, 0};
        sheet.frameStride = sheet.frameSize;
        sheet.framesPerRow = sheet.sheetGrid.x;
    }

    sheets_[name] = std::move(sheet);
    return true;
}

void Player::applySheet(const Sheet &sheet) {
    sprite_.setTexture(sheet.texture, true);
    sprite_.setPosition(sf::Vector2f{0.f, 0.f});
    const int row = currentRow_ % std::max(1, sheet.sheetGrid.y);
    applyFrame(sheet, 0, row);
}

void Player::applyFrame(const Sheet &sheet, int col, int row) {
    if (sheet.frameSize.x <= 0 || sheet.frameSize.y <= 0)
        return;
    const sf::Vector2i texPos =
        sheet.frameStart +
        sf::Vector2i{col * sheet.frameStride.x, row * sheet.frameStride.y};
    sprite_.setTextureRect(sf::IntRect(texPos, sheet.frameSize));
    sprite_.setOrigin(
        sf::Vector2f{sheet.frameSize.x * 0.5f, sheet.frameSize.y * 0.5f});
    sprite_.setScale(sf::Vector2f{
        shape_.getSize().x / static_cast<float>(sheet.frameSize.x),
        shape_.getSize().y / static_cast<float>(sheet.frameSize.y)});
}

const Player::Sheet *Player::findSheet(const std::string &name) const {
    const auto it = sheets_.find(name);
    if (it == sheets_.end()) return nullptr;
    if (!it->second.loaded) return nullptr;
    return &it->second;
}

int Player::rowIndexFor(DirectionSector sector) const {
    for (std::size_t i = 0; i < kRowDirectionOrder_.size(); ++i) {
        if (kRowDirectionOrder_[i] == sector) return static_cast<int>(i);
    }
    return 0;
}

bool Player::loadAnimation(const std::string &stateName, const std::string &path) {
    const bool ok = loadSheet(stateName, path);
    if (ok) {
        hasTexture_ = true;
        if (!activeSheet_) {
            activeSheet_ = findSheet(stateName);
            if (activeSheet_) {
                activeState_ = stateName;
                applySheet(*activeSheet_);
            }
        }
    }
    return ok;
}

void Player::setStateOverride(const std::string &stateName) {
    overrideState_ = stateName;
    hasOverride_ = true;
}

void Player::clearStateOverride() {
    overrideState_.clear();
    hasOverride_ = false;
}

sf::Vector2f Player::resolveMove(sf::Vector2f desired) noexcept {
    if (!obstacles_ || (desired.x == 0.f && desired.y == 0.f)) {
        return desired;
    }
    const sf::FloatRect gb = getGlobalBounds();
    core::AABB entityAABB = core::fromFloatRect(gb);
    return core::resolveMovement(entityAABB, desired, *obstacles_);
}
