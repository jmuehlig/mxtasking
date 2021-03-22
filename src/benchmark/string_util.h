#pragma once

#include <functional>
#include <string>

namespace benchmark {
class string_util
{
public:
    static void split(const std::string &text, char delimiter,
                      const std::function<void(const std::string &line)> &callback);
};
} // namespace benchmark