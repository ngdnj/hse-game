#pragma once

#include "core/Entity.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>

/**
 * @brief Базовый класс для сущностей с анимированными спрайтами и fallback-формой.
 */
class AnimatedSpriteEntity : public Entity {
  public:
    explicit AnimatedSpriteEntity(const sf::Vector2f &size);

    sf::FloatRect getLocalBounds() const override;

    void setFrameDuration(float seconds) noexcept { frameDuration_ = seconds; }

    bool loadAnimation(const std::string &stateName, const std::string &path);

    void setStateOverride(const std::string &stateName);
    void clearStateOverride();

  protected:
    struct Sheet {
      sf::Texture texture;
      sf::Vector2i sheetGrid{8, 8};
      sf::Vector2i frameSize{0, 0};
      sf::Vector2i frameStart{0, 0};
      sf::Vector2i frameStride{0, 0};
      int framesPerRow{8};
      bool loaded{false};
    };

    // Обновляет анимацию с учётом текущего ряда и режима движения.
    void updateAnimation(float dt, bool isMoving,
                         const std::string &moveState = "run",
                         const std::string &idleState = "idle");

    // Переход в указанное состояние, если атлас загружен.
    bool switchToState(const std::string &stateName);

    const Sheet *findSheet(const std::string &name) const;

    // Управление текущей строкой атласа (например, от направления).
    void setCurrentRow(int row) { currentRow_ = row; }
    int currentRow() const noexcept { return currentRow_; }

    bool hasTexture() const noexcept { return hasTexture_; }

    const Sheet *activeSheet() const noexcept { return activeSheet_; }

    sf::RectangleShape &shape() noexcept { return shape_; }
    const sf::RectangleShape &shape() const noexcept { return shape_; }

    sf::Sprite &sprite() noexcept { return sprite_; }
    const sf::Sprite &sprite() const noexcept { return sprite_; }

    void onDraw(sf::RenderTarget &target, sf::RenderStates states) const override;

  private:
    bool loadSheet(const std::string &name, const std::string &path);
    void applySheet(const Sheet &sheet);
    void applyFrame(const Sheet &sheet, int col, int row);

  private:
    sf::Vector2i defaultGrid_{8, 8};
    sf::RectangleShape shape_;
    sf::Texture placeholderTexture_;
    sf::Sprite sprite_;
    bool hasTexture_{false};

    std::unordered_map<std::string, Sheet> sheets_;
    const Sheet *activeSheet_{nullptr};
    std::string activeState_{};
    std::string overrideState_{};
    bool hasOverride_{false};
    int currentFrame_{0};
    int currentRow_{0};
    float frameDuration_{0.12f};
    float frameTimer_{0.f};
};
