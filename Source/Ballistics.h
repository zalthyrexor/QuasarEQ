#pragma once

#include <vector>
#include <span>
#include <cmath>

template <size_t Size>
class Ballistics {
public:
    Ballistics()
    {
        gains.fill(0.0f);
    }
    void process(std::span<float> inputMagnitudes, float dt)
    {
        if (inputMagnitudes.size() < Size) return;
        const float releaseSpeedFactor = 1.0f - std::exp(-dt * 50.0f);
        for (size_t i = 0; i < Size; ++i)
        {
            const float target = inputMagnitudes[i];
            if (target > gains[i])
            {
                gains[i] = target;
            }
            else
            {
                gains[i] += releaseSpeedFactor * (target - gains[i]);
            }
            inputMagnitudes[i] = gains[i];
        }
    }
private:
    std::array<float, Size> gains;
};
