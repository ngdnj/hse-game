#pragma once

#include "core/Events.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace combat {

class Artifact {
public:
    virtual ~Artifact() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    virtual void onEnemyKilled(const core::EnemyKilledEvent&) {}
    virtual void onAreaDamageRequest(const core::AreaDamageRequest&) {}
    virtual void reset() {}
};

class VampiricFang final : public Artifact {
public:
    explicit VampiricFang(int healPerKills = 5, int healAmount = 1)
        : healPerKills_(healPerKills), healAmount_(healAmount) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Vampiric Fang"; }

    void onEnemyKilled(const core::EnemyKilledEvent& e) override {
        (void)e;
        vampiricTimer_ += 1;
        if (vampiricTimer_ >= healPerKills_) {
            vampiricTimer_ = 0;
            healCount_ += healAmount_;
        }
    }

    void reset() override { vampiricTimer_ = 0; healCount_ = 0; }

    [[nodiscard]] int pendingHeals() const noexcept { return healCount_; }
    void consumeHeals() noexcept { healCount_ = 0; }

private:
    int healPerKills_{5};
    int healAmount_{1};
    int vampiricTimer_{0};
    int healCount_{0};
};

class ExplosiveShells final : public Artifact {
public:
    explicit ExplosiveShells(float = 0.2f, float radius = 70.f, int bonusDamage = 20)
        : radius_(radius), bonusDamage_(bonusDamage) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "Explosive Shells"; }

    void onAreaDamageRequest(const core::AreaDamageRequest& req) override {
        pendingExplosions_.push_back({req.position, radius_, bonusDamage_, req.source});
    }

    void reset() override { pendingExplosions_.clear(); }

    [[nodiscard]] const std::vector<core::AreaDamageRequest>& pendingExplosions() const noexcept {
        return pendingExplosions_;
    }
    void clearExplosions() noexcept { pendingExplosions_.clear(); }

private:
    float radius_{70.f};
    int bonusDamage_{20};
    std::vector<core::AreaDamageRequest> pendingExplosions_;
};

} // namespace combat