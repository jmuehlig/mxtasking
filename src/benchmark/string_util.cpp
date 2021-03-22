#include "string_util.h"
#include <sstream>

using namespace benchmark;

void string_util::split(const std::string &text, const char delimiter,
                        const std::function<void(const std::string &line)> &callback)
{
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, delimiter))
    {
        callback(token);
    }
}