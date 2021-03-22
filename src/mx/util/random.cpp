#include "random.h"
#include <chrono>

using namespace mx::util;

Random::Random() noexcept : Random(std::uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

Random::Random(const std::uint32_t seed) noexcept : _register({0U}), _ic_state(seed), _addend(123456U)
{
    const auto index = 69069U;

    for (auto &reg : this->_register)
    {
        this->_ic_state = (69607U + 8U * index) * this->_ic_state + this->_addend;
        reg = (this->_ic_state >> 8U) & 0xffffff;
    }
    this->_ic_state = (69607U + 8U * index) * this->_ic_state + this->_addend;

    this->_multiplier = 100005U + 8U * index;
}

std::int32_t Random::next() noexcept
{
    std::int32_t rand = (((this->_register[5] >> 7U) | (this->_register[6] << 17U)) ^
                         ((this->_register[4] >> 1U) | (this->_register[5] << 23U))) &
                        0xffffff;

    this->_register[6] = this->_register[5];
    this->_register[5] = this->_register[4];
    this->_register[4] = this->_register[3];
    this->_register[3] = this->_register[2];
    this->_register[2] = this->_register[1];
    this->_register[1] = this->_register[0];
    this->_register[0] = std::uint32_t(rand);
    this->_ic_state = this->_ic_state * this->_multiplier + this->_addend;

    return rand ^ ((this->_ic_state >> 8) & 0xffffff);
}
