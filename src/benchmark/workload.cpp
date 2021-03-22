#include "workload.h"
#include <limits>

using namespace benchmark;

std::pair<std::uint64_t, std::uint64_t> Workload::next(const std::uint64_t count) noexcept
{
    const auto index = this->_current_index.fetch_add(count, std::memory_order_relaxed);
    const auto workload_size = this->_workload_set[this->_current_phase].size();

    return index < workload_size ? std::make_pair(index, std::min(count, workload_size - index))
                                 : std::make_pair(std::numeric_limits<std::uint64_t>::max(), 0UL);
}

namespace benchmark {
std::ostream &operator<<(std::ostream &stream, const Workload &workload)
{
    return stream << workload._workload_set << std::flush;
}
} // namespace benchmark
