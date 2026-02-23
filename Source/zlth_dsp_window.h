#pragma once
#include <cmath>   
#include <numbers>
#include <concepts> 
#include <ranges>
#include <array>

namespace zlth::dsp::window
{
    template <typename R, std::floating_point CoeffT, std::size_t N>
        requires std::ranges::forward_range<R>&& std::floating_point<std::ranges::range_value_t<R>>
    static void fill_window(R&& target, const std::array<CoeffT, N>& coeffs) noexcept
    {
        using T = std::ranges::range_value_t<R>;
        const T step = (std::numbers::pi_v<T> *2) / static_cast<T>(std::ranges::size(target));
        std::size_t i = 0;
        for (auto& element : target)
        {
            const T phi = step * i;
            T result = 0;
            for (std::size_t k = 0; k < N; ++k)
            {
                result += static_cast<T>(coeffs[k]) * std::cos(phi * k);
            }
            element = result;
            ++i;
        }
    }
}
