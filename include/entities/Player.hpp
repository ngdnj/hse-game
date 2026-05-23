#pragma once

#include "combat/Weapon.hpp"
#include "core/Collision.hpp"
#include "core/Direction.hpp"
#include "core/Entity.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * @brief Игрок, управляемый клавишами WASD, с поддержкой спрайтовой анимации.
 *
 * Может отрисовываться как прямоугольник (fallback) или как спрайт из атласа.
 * Поддерживает 8 направлений, автоматический парсинг тайлсета с прозрачными
 * линиями-«гаттерами», отдельные атласы для состояний (idle/run/attack/...)
 * и ограничение движения в заданных границах.
 */
class Player final : public Entity {
  public:
    /**
     * @brief Создаёт игрока.
     *
     * @param size Логический размер сущности в пикселях;
     *             используется как габарит для коллизий и масштабирования
     *             спрайта.
     * @param startPosition Начальная позиция в мировых координатах.
     * @param movementBounds Прямоугольник, внутри которого можно двигаться.
     * @param moveTexturePath Путь к атласу движения ("run"). Если не задан
     *        или не загрузился, используется fallback-прямоугольник.
     * @param idleTexturePath Путь к атласу покоя ("idle"). Если пусто, в покое
     *        используется тот же атлас, что и для движения.
     */
    Player(const sf::Vector2f &size, const sf::Vector2f &startPosition,
           const sf::FloatRect &movementBounds,
           const std::string &moveTexturePath = "",
           const std::string &idleTexturePath = "");

    /** @brief Обновляет ввод, анимацию и ограничение по границам. */
    void update(float dt) override;

    /** @brief Локальные границы (коллизия/отрисовка). */
    [[nodiscard]] sf::FloatRect getLocalBounds() const override;

    /** @brief Установить скорость перемещения (пикселей в секунду). */
    void setSpeed(float speed) noexcept { speed_ = speed; }
    /** @brief Текущая скорость перемещения. */
    [[nodiscard]] float getSpeed() const noexcept { return speed_; }

    /**
     * @brief Загрузить или заменить атлас состояния (e.g. "attack").
     * @return true если атлас успешно загружен и добавлен.
     */
    [[nodiscard]] bool loadAnimation(const std::string &stateName,
                                     const std::string &path);

    /**
     * @brief Зафиксировать состояние анимации по имени ("attack", "hurt" и
     * т.д.). Пока override активен, автоматический выбор run/idle не
     * выполняется.
     */
    void setStateOverride(const std::string &stateName);

    /** @brief Снять принудительное состояние и вернуться к run/idle логике. */
    void clearStateOverride();

    // ---- Combat ----
    /// Try to fire the currently equipped weapon. Returns the result for the
    /// game loop to translate into gameplay effects. Empty result (no melee
    /// hitbox, no projectiles) if the weapon is on cooldown.
    [[nodiscard]] combat::AttackResult tryFire() noexcept;

    /// Replace the equipped weapon. nullptr resets to a default Sword.
    void setWeapon(std::unique_ptr<combat::Weapon> weapon) noexcept;

    [[nodiscard]] combat::Weapon* weapon() noexcept { return weapon_.get(); }
    [[nodiscard]] const combat::Weapon* weapon() const noexcept { return weapon_.get(); }

    /// Convenience accessors that forward to the active weapon.
    void setAttackDamage(int dmg) noexcept;
    [[nodiscard]] int attackDamage() const noexcept;

    /// Sword-only setter. No-op for other weapon types.
    void setAttackRange(float range) noexcept;
    [[nodiscard]] float attackRange() const noexcept;

    /// True briefly after a melee swing — drives the slash arc visual.
    [[nodiscard]] bool isAttacking() const noexcept { return slashTimer_ > 0.f; }

    // ---- Health ----
    void takeDamage(int amount) noexcept;
    void setMaxHealth(int hp) noexcept;
    void heal(int amount) noexcept;
    [[nodiscard]] int health() const noexcept { return health_; }
    [[nodiscard]] int maxHealth() const noexcept { return maxHealth_; }
    [[nodiscard]] bool isDead() const noexcept { return health_ <= 0; }

    // ---- Lifecycle ----
    /** Reset combat/dash state and position. Keeps loaded textures and
     *  movement bounds. Used by the game-over restart flow. */
    void reset(const sf::Vector2f& startPosition) noexcept;

    // ---- Dash ----
    void dash() noexcept;
    [[nodiscard]] bool isDashing() const noexcept { return isDashing_; }
    [[nodiscard]] float dashCooldownRemaining() const noexcept;
    void setDashSpeed(float s) noexcept { dashSpeed_ = s; }
    void setDashDuration(float d) noexcept { dashDuration_ = d; }
    void setDashCooldown(float c) noexcept { dashCooldownDuration_ = c; }

    /** @brief Provide collision geometry for resolution (nullptr to clear). */
    void setObstacles(const std::vector<core::AABB>* obstacles) noexcept {
        obstacles_ = obstacles;
    }

    /** @brief Resolve a movement intent against stored obstacles and apply it.
     *  Returns actual movement after slide resolution. */
    sf::Vector2f resolveMove(sf::Vector2f desired) noexcept;

  protected:
    void onDraw(sf::RenderTarget &target,
                sf::RenderStates states) const override;

  private:
    /// Обрабатывает клавиатурный ввод и движение.
    void handleInput(float dt);

    /// Продвигает анимацию спрайта по таймеру и состоянию (idle/run/override).
    void updateAnimation(float dt);

    /// Выбирает строку атласа по текущему направлению.
    void updateDirection(const sf::Vector2f &dir);

    /// Держит игрока внутри заданных границ.
    void clampToBounds();

    struct Sheet {
        sf::Texture texture;
        sf::Vector2i sheetGrid{8, 8};
        sf::Vector2i frameSize{0, 0};
        sf::Vector2i frameStart{0, 0};
        sf::Vector2i frameStride{0, 0};
        int framesPerRow{8};
        bool loaded{false};
    };

    // Логический размер задаётся shape_; по нему считаются коллизии и границы.
    // Спрайт подгоняется под эти локальные границы, если текстура загружена.
    sf::RectangleShape shape_;
    sf::Texture placeholderTexture_;
    sf::Sprite sprite_;
    bool hasTexture_{false};

    // Настройки анимации по умолчанию.
    static constexpr sf::Vector2i kDefaultGrid_{8, 8};
    static constexpr float kDefaultFrameDuration_{0.12f};
    static constexpr float kDefaultSpeed_{220.f};

    std::unordered_map<std::string, Sheet> sheets_;
    const Sheet *activeSheet_{nullptr};
    std::string activeState_{"run"};
    std::string overrideState_{};
    bool hasOverride_{false};
    int currentFrame_{0};
    int currentRow_{0};
    sf::Vector2f lastDir_{1.f, 0.f};
    float frameDuration_{kDefaultFrameDuration_};
    float frameTimer_{0.f};
    bool isMoving_{false};

    // Порядок строк атласа.
    static constexpr std::array<DirectionSector, 8> kRowDirectionOrder_{
        DirectionSector::Up,    DirectionSector::UpRight,
        DirectionSector::Right, DirectionSector::DownRight,
        DirectionSector::Down,  DirectionSector::DownLeft,
        DirectionSector::Left,  DirectionSector::UpLeft};

    sf::FloatRect movementBounds_;
    float speed_{kDefaultSpeed_};

    // Combat
    std::unique_ptr<combat::Weapon> weapon_;
    float slashTimer_{0.f};
    float slashRange_{55.f};
    static constexpr float kSlashVisibleDuration_{0.12f};

    // Health
    int maxHealth_{100};
    int health_{100};

    // Damage flash
    float flashTimer_{0.f};
    static constexpr float kFlashDuration_{0.18f};

    // Dash
    bool isDashing_{false};
    float dashTimer_{0.f};
    float dashDuration_{0.2f};
    float dashCooldownDuration_{1.5f};
    float dashCooldownTimer_{0.f};
    float dashSpeed_{600.f};
    sf::Vector2f dashDir_{1.f, 0.f};

    // Collision obstacles reference (not owned)
    const std::vector<core::AABB>* obstacles_{nullptr};

    bool loadSheet(const std::string &name, const std::string &path);
    const Sheet *findSheet(const std::string &name) const;
    int rowIndexFor(DirectionSector sector) const;
    void applySheet(const Sheet &sheet);
    void applyFrame(const Sheet &sheet, int col, int row);
    void drawSlash(sf::RenderTarget &target, sf::RenderStates states) const;
};
