#include "entities/Player.hpp"
#include "combat/SwordWeapon.hpp"
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
    : sprite_(placeholderTexture_),
      movementBounds_(movementBounds),
      weapon_(std::make_unique<combat::SwordWeapon>()) {
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

    // Tick dash state machine
    if (isDashing_) {
        dashTimer_ -= dt;
        if (dashTimer_ <= 0.f) {
            isDashing_ = false;
        }
    }
    dashCooldownTimer_ = std::max(0.f, dashCooldownTimer_ - dt);
    flashTimer_ = std::max(0.f, flashTimer_ - dt);
    slashTimer_ = std::max(0.f, slashTimer_ - dt);
    if (weapon_) weapon_->update(dt);

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

    // Trigger dash on Shift (only when not already dashing and cooldown ready)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) && !isDashing_ && dashCooldownTimer_ <= 0.f) {
        dash();
    }

    // While dashing, override movement with dash direction
    if (isDashing_) {
        const sf::Vector2f resolved = resolveMove(dashDir_ * dashSpeed_ * dt);
        if (resolved.x != 0.f || resolved.y != 0.f) {
            move(resolved);
        }
    } else if (hasInput(dir)) {
        const sf::Vector2f norm = normalizeOrZero(dir);
        // Save last direction for dash/attack
        if (norm.x != 0.f || norm.y != 0.f) lastDir_ = norm;
        const sf::Vector2f resolved = resolveMove(norm * speed_ * dt);
        if (resolved.x != 0.f || resolved.y != 0.f) {
            move(resolved);
        }
    }

    isMoving_ = hasInput(dir);
    updateDirection(dir);
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

combat::AttackResult Player::tryFire() noexcept {
    if (!weapon_) return {};
    const sf::Vector2f dir = (lastDir_.x == 0.f && lastDir_.y == 0.f)
                                 ? sf::Vector2f{1.f, 0.f}
                                 : lastDir_;
    auto result = weapon_->fire(getPosition(), dir);
    if (result.hasMeleeHitbox) {
        slashTimer_ = kSlashVisibleDuration_;
        const auto& hb = result.meleeHitbox;
        slashRange_ = std::max(hb.size.x, hb.size.y);
    }
    return result;
}

void Player::setWeapon(std::unique_ptr<combat::Weapon> weapon) noexcept {
    if (weapon) {
        weapon_ = std::move(weapon);
    } else {
        weapon_ = std::make_unique<combat::SwordWeapon>();
    }
}

void Player::setAttackDamage(int dmg) noexcept {
    if (weapon_) weapon_->setDamage(dmg);
}

int Player::attackDamage() const noexcept {
    return weapon_ ? weapon_->damage() : 0;
}

void Player::setAttackRange(float range) noexcept {
    auto* sword = dynamic_cast<combat::SwordWeapon*>(weapon_.get());
    if (sword) {
        sword->setRange(range);
        slashRange_ = range;
    }
}

float Player::attackRange() const noexcept {
    const auto* sword = dynamic_cast<const combat::SwordWeapon*>(weapon_.get());
    return sword ? sword->range() : 0.f;
}

void Player::onDraw(sf::RenderTarget &target, sf::RenderStates states) const {
    const bool flashing = flashTimer_ > 0.f;
    if (hasTexture_) {
        if (flashing) {
            // Pulse between bright white and red while damaged.
            const float t = flashTimer_ / kFlashDuration_;
            const auto r = static_cast<std::uint8_t>(255);
            const auto gb = static_cast<std::uint8_t>(80 + 175 * (1.f - t));
            sf::Sprite tinted = sprite_;
            tinted.setColor(sf::Color(r, gb, gb));
            target.draw(tinted, states);
        } else {
            target.draw(sprite_, states);
        }
    } else {
        if (flashing) {
            sf::RectangleShape tinted = shape_;
            tinted.setFillColor(sf::Color(255, 120, 120));
            target.draw(tinted, states);
        } else {
            target.draw(shape_, states);
        }
    }

    // Attack slash arc — visible briefly after attack starts.
    if (slashTimer_ > 0.f) {
        drawSlash(target, states);
    }
}

void Player::drawSlash(sf::RenderTarget &target, sf::RenderStates states) const {
    (void)states;
    if (lastDir_.x == 0.f && lastDir_.y == 0.f) return;
    if (slashTimer_ <= 0.f) return;

    // 1.0 just after swing started, decays to 0.0 at end of visible window.
    const float t = std::clamp(slashTimer_ / kSlashVisibleDuration_, 0.f, 1.f);

    const float baseAngle = std::atan2(lastDir_.y, lastDir_.x);
    const float arcHalfAngle = std::numbers::pi_v<float> / 3.f; // 60° to each side
    const float radius = slashRange_;
    constexpr int kSegments = 16;

    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, kSegments + 2);
    const sf::Vector2f origin = getPosition();
    const auto alpha = static_cast<std::uint8_t>(220.f * t);
    const sf::Color tip(255, 240, 180, alpha);
    const sf::Color edge(255, 200, 80, static_cast<std::uint8_t>(60.f * t));

    fan[0].position = origin;
    fan[0].color = tip;
    for (int i = 0; i <= kSegments; ++i) {
        const float frac = static_cast<float>(i) / static_cast<float>(kSegments);
        const float angle = baseAngle - arcHalfAngle + frac * 2.f * arcHalfAngle;
        fan[i + 1].position = origin + sf::Vector2f{std::cos(angle), std::sin(angle)} * radius;
        fan[i + 1].color = edge;
    }
    sf::RenderStates worldStates; // world-space, ignore entity transform
    target.draw(fan, worldStates);
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

void Player::dash() noexcept {
    if (dashCooldownTimer_ > 0.f) return;
    // Dash in current lastDir_, or right if no direction yet
    if (lastDir_.x == 0.f && lastDir_.y == 0.f) {
        dashDir_ = {1.f, 0.f};
    } else {
        dashDir_ = normalizeOrZero(lastDir_);
    }
    isDashing_ = true;
    dashTimer_ = dashDuration_;
    dashCooldownTimer_ = dashCooldownDuration_;
}

float Player::dashCooldownRemaining() const noexcept {
    return std::max(0.f, dashCooldownTimer_);
}

void Player::takeDamage(int amount) noexcept {
    if (amount <= 0) return;
    health_ = std::max(0, health_ - amount);
    flashTimer_ = kFlashDuration_;
}

void Player::setMaxHealth(int hp) noexcept {
    const int delta = hp - maxHealth_;
    maxHealth_ = std::max(1, hp);
    if (delta > 0) health_ = std::min(maxHealth_, health_ + delta);
    else health_ = std::min(health_, maxHealth_);
}

void Player::heal(int amount) noexcept {
    if (amount <= 0) return;
    health_ = std::min(maxHealth_, health_ + amount);
}

void Player::reset(const sf::Vector2f& startPosition) noexcept {
    setPosition(startPosition);
    health_ = maxHealth_;
    isDashing_ = false;
    dashTimer_ = 0.f;
    dashCooldownTimer_ = 0.f;
    flashTimer_ = 0.f;
    slashTimer_ = 0.f;
    lastDir_ = {1.f, 0.f};
}
