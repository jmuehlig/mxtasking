#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace application::hash_join {
class TPCHTableReader
{
public:
    static void read(const std::string &file_name,
                     std::function<void(const std::uint16_t, const std::string &)> &&callback);
};
} // namespace application::hash_join