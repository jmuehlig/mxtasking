#include "cores.h"
#include <mx/system/topology.h>
#include <regex>
#include <sstream>

using namespace benchmark;

Cores::Cores(const std::uint16_t min_cores, const std::uint16_t max_cores, const std::uint16_t steps,
             const mx::util::core_set::Order order)
{
    this->add_for_range(min_cores, max_cores, steps, order);
}

Cores::Cores(const std::string &cores, const std::uint16_t steps, const mx::util::core_set::Order order)
{
    const std::regex single_core_regex("(\\d+)$");
    const std::regex from_core_regex("(\\d+):$");
    const std::regex core_range_regex("(\\d+):(\\d+)");

    std::stringstream stream(cores);
    std::string token;
    while (std::getline(stream, token, ';'))
    {
        std::smatch match;

        if (std::regex_match(token, match, single_core_regex))
        {
            const auto core = std::stoi(match[1].str());
            this->add_for_range(core, core, steps, order);
        }
        else if (std::regex_match(token, match, from_core_regex))
        {
            this->add_for_range(std::stoi(match[1].str()), mx::system::topology::count_cores(), steps, order);
        }
        else if (std::regex_match(token, match, core_range_regex))
        {
            this->add_for_range(std::stoi(match[1].str()), std::stoi(match[2].str()), steps, order);
        }
    }
}

void Cores::add_for_range(const std::uint16_t min_cores, const std::uint16_t max_cores, const std::uint16_t steps,
                          const mx::util::core_set::Order order)
{
    if (min_cores == 0U || min_cores == max_cores)
    {
        this->_core_sets.push_back(mx::util::core_set::build(max_cores, order));
    }
    else
    {
        auto cores = min_cores;
        if (cores % steps != 0U)
        {
            this->_core_sets.push_back(mx::util::core_set::build(cores, order));
            cores++;
        }

        for (auto count_cores = cores; count_cores <= max_cores; count_cores++)
        {
            if (count_cores % steps == 0U)
            {
                this->_core_sets.push_back(mx::util::core_set::build(count_cores, order));
            }
        }

        if (max_cores % steps != 0U)
        {
            this->_core_sets.push_back(mx::util::core_set::build(max_cores, order));
        }
    }
}

std::string Cores::dump(const std::uint8_t indent) const
{
    std::stringstream stream;

    for (auto i = 0U; i < this->_core_sets.size(); ++i)
    {
        if (i > 0U)
        {
            stream << "\n";
        }
        const auto &core_set = this->_core_sets[i];
        if (indent > 0U)
        {
            stream << std::string(indent, ' ');
        }
        stream << core_set.size() << ": " << core_set;
    }
    stream << std::flush;

    return stream.str();
}

namespace benchmark {
std::ostream &operator<<(std::ostream &stream, const Cores &cores)
{
    return stream << cores.dump(0U) << std::endl;
}
} // namespace benchmark