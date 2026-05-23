#pragma once

#include <SFML/Graphics.hpp>

/**
 * @brief Базовый абстрактный класс для всех игровых сущностей.
 *
 * Класс представляет собой объект, который можно рисовать и трансформировать
 * с помощью SFML. Он обеспечивает интерфейс для обновления состояния
 * сущности, получения её границ, а также управляет флагами жизненного
 * цикла, активности и видимости.
 *
 * Контракт:
 * - update(float dt) вызывается каждое обновление кадра с дельтой времени.
 * - onDraw(...) реализует конкретную отрисовку сущности в локальных
 *   координатах (до применения трансформации).
 */
class Entity : public sf::Drawable, public sf::Transformable {
public:
  ~Entity() override = default;

  /**
   * @brief Обновляет состояние сущности.
   *
   * Вызывается каждый кадр игровым циклом. Реализация в подклассе должна
   * обновлять позицию, анимацию, физику и т.д. в соответствии со временем
   * dt.
   *
   * @param dt Время в секундах, прошедшее с предыдущего кадра.
   */
  virtual void update(float dt) = 0;

  /**
   * @brief Возвращает локальные (до применения трансформации) ограничивающий
   * прямоугольник сущности.
   *
   * Используется для вычисления пересечений и попаданий по координатам.
   *
   * @return sf::FloatRect Локальные границы сущности.
   */
  virtual sf::FloatRect getLocalBounds() const = 0;

  /**
   * @brief Возвращает глобальные границы сущности с учётом трансформации.
   *
   * Реализация использует getLocalBounds() и текущую матрицу трансформации
   * (sf::Transformable::getTransform()).
   *
   * @return sf::FloatRect Глобальные границы сущности в координатах целевого
   * рендеринга.
   */
  sf::FloatRect getGlobalBounds() const {
    return this->getTransform().transformRect(this->getLocalBounds());
  }

  /**
   * @brief Проверяет, помечена ли сущность как живоя (не для удаления).
   *
   * @return true если сущность жива и должна оставаться в контейнерах; false
   * если сущность помечена для удаления.
   */
  bool isAlive() const noexcept { return this->alive_; }

  /**
   * @brief Пометить сущность для удаления.
   *
   * Флаг используется менеджером сущностей для безопасного удаления на
   * следующей итерации игрового цикла.
   */
  void markForRemoval() noexcept { this->alive_ = false; }

  /**
   * @brief Проверить, активна ли сущность (участвует ли в обновлениях).
   *
   * Если сущность неактивна, вызовы update могут пропускаться, но она
   * всё ещё может быть видимой или участвующей в отображении/коллизиях
   * в зависимости от логики приложения.
   *
   * @return true если активна и должна получать update(), иначе false.
   */
  bool isActive() const noexcept { return this->active_; }

  /**
   * @brief Установить состояние активности сущности.
   *
   * @param active Новое состояние активности.
   */
  void setActive(bool active) noexcept { this->active_ = active; }

  /**
   * @brief Проверить, видима ли сущность (участвует ли в отрисовке).
   *
   * @return true если сущность должна рендериться, иначе false.
   */
  bool isVisible() const noexcept { return this->visible_; }

  /**
   * @brief Установить флаг видимости сущности.
   *
   * @param visible Новое состояние видимости.
   */
  void setVisible(bool visible) noexcept { this->visible_ = visible; }

  /**
   * @brief Проверяет пересечение глобальных границ с другой сущностью.
   *
   * Метод использует getGlobalBounds() обеих сущностей и проверяет
   * пересечение прямоугольников.
   *
   * @param other Сравниваемая сущность.
   * @return true если границы пересекаются, иначе false.
   */
  bool intersects(const Entity &other) const noexcept {
    return this->getGlobalBounds()
        .findIntersection(other.getGlobalBounds())
        .has_value();
  }

  /**
   * @brief Проверяет, содержит ли глобальная область сущности точку.
   *
   * @param point Координата в глобальной (экранной) системе координат.
   * @return true если точка находится внутри глобальных границ,
   * иначе false.
   */
  bool contains(sf::Vector2f point) const noexcept {
    return this->getGlobalBounds().contains(point);
  }

  /**
   * @brief Apply a resolved movement delta to the entity position,
   *        respecting world bounds.
   *
   * @param delta Movement vector from collision resolver (already computed).
   */
  void applyResolvedMovement(sf::Vector2f delta) {
    sf::Vector2f pos = getPosition();
    pos += delta;
    setPosition(pos);
  }

protected:
  /**
   * @brief Внутренний метод отрисовки, реализованный в подклассе.
   *
   * Этот метод вызывается после проверки флагов видимости/жизни и после
   * применения текущей трансформации. Реализация должна рисовать объект в
   * локальных координатах (без дополнительной трансформации).
   *
   * @param target Цель отрисовки (окно или render texture).
   * @param states Текущие состояния рендеринга (включая шейдеры, цвет и т.д.).
   */
  virtual void onDraw(sf::RenderTarget &target,
                      sf::RenderStates states) const = 0;

  /**
   * @brief Финальная реализация метода draw из sf::Drawable.
   *
   * Метод автоматически пропускает отрисовку, если сущность не видима или
   * помечена на удаление. Иначе к переданным состояниям применяется
   * текущая трансформация сущности и затем вызывается onDraw().
   */
  void draw(sf::RenderTarget &target, sf::RenderStates states) const final {
    if (!this->visible_ || !this->alive_) {
      return;
    }
    states.transform *= this->getTransform();
    this->onDraw(target, states);
  }

private:
  /**
   * @brief Флаг жизненного цикла сущности.
   *
   * true  — сущность жива и должна оставаться в менеджере сущностей.
   * false — сущность помечена для удаления и должна быть удалена при
   * следующей чистке.
   */
  bool alive_ = true;

  /**
   * @brief Флаг активности сущности (участие в обновлениях).
   *
   * Если false, update() можно пропускать.
   */
  bool active_ = true;

  /// @brief Флаг видимости сущности (участие в отрисовке).
  bool visible_ = true;
};
