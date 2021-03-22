#pragma once
#include <cstdint>
namespace benchmark {
enum class phase : std::uint8_t
{
    FILL = 0U,
    MIXED = 1U
};
}