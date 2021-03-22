#pragma once

#include <mx/synchronization/synchronization.h>

namespace db::index::blinktree {
class config
{
public:
    static constexpr auto node_size() { return 1024U; }
};
} // namespace db::index::blinktree