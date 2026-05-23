#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/System/Vector2.hpp>
#include <array>
#include <memory>
#include <unordered_map>

namespace core {

// Placeholder shape descriptor — easily swappable to Texture later
struct ShapeDesc {
    enum class Type { Rectangle, Circle } type = Type::Rectangle;
    sf::Vector2f size{32.f, 32.f};
    sf::Color fillColor{sf::Color::White};
    sf::Color outlineColor{sf::Color::Transparent};
    float outlineThickness{0.f};
    float cornerRadius{0.f}; // for rectangles
    float radius{16.f};      // for circles
};

class ResourceManager {
public:
    ResourceManager() = default;

    // Register a named shape for entity creation
    void registerShape(std::string name, ShapeDesc desc) {
        shapes_[std::move(name)] = std::move(desc);
    }

    // Get a shape by name (returns default if not found)
    [[nodiscard]] const ShapeDesc& getShape(std::string_view name) const {
        static const ShapeDesc kDefault{};
        auto it = shapes_.find(std::string{name});
        if (it != shapes_.end()) {
            return it->second;
        }
        return kDefault;
    }

    // Pre-register common shapes for the prototype
    void initDefaults() {
        registerShape("player", ShapeDesc{
            .type = ShapeDesc::Type::Rectangle,
            .size = {40.f, 40.f},
            .fillColor = sf::Color(100, 200, 255),
            .outlineColor = sf::Color::White,
            .outlineThickness = 2.f
        });
        registerShape("enemy", ShapeDesc{
            .type = ShapeDesc::Type::Rectangle,
            .size = {36.f, 36.f},
            .fillColor = sf::Color(220, 80, 80),
            .outlineColor = sf::Color::White,
            .outlineThickness = 1.f
        });
        registerShape("loot", ShapeDesc{
            .type = ShapeDesc::Type::Circle,
            .size = {16.f, 16.f},
            .fillColor = sf::Color(255, 215, 0),
            .outlineColor = sf::Color::White,
            .outlineThickness = 1.f,
            .radius = 10.f
        });
    }

    // Build an sf::Shape from a ShapeDesc (for rendering)
    [[nodiscard]] std::unique_ptr<sf::Shape> makeShape(std::string_view name) const {
        const auto& desc = getShape(name);
        std::unique_ptr<sf::Shape> shape;

        if (desc.type == ShapeDesc::Type::Rectangle) {
            auto rect = std::make_unique<sf::RectangleShape>(desc.size);
            rect->setFillColor(desc.fillColor);
            rect->setOutlineColor(desc.outlineColor);
            rect->setOutlineThickness(desc.outlineThickness);
            shape = std::move(rect);
        } else {
            auto circle = std::make_unique<sf::CircleShape>(desc.radius);
            circle->setFillColor(desc.fillColor);
            circle->setOutlineColor(desc.outlineColor);
            circle->setOutlineThickness(desc.outlineThickness);
            shape = std::move(circle);
        }

        return shape;
    }

private:
    std::unordered_map<std::string, ShapeDesc> shapes_;
};

} // namespace core