#pragma once

#include <array>

/**
 * @brief 8-секторное направление (0=вправо, далее по часовой стрелке).
 */
enum class DirectionSector : int {
    Right = 0,
    DownRight = 1,
    Down = 2,
    DownLeft = 3,
    Left = 4,
    UpLeft = 5,
    Up = 6,
    UpRight = 7
};

// Маппинг строк атласа может различаться между сущностями и их спрайтами,
// поэтому храним его локально в нужных классах, а не глобально.
