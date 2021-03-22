#pragma once

namespace mx::util {
/**
 * Queue item as abstraction for MPSC and SPSC queues.
 */
class QueueItem
{
public:
    constexpr QueueItem() noexcept = default;
    virtual ~QueueItem() noexcept = default;

    /**
     * @return Next item in the queue after this one.
     */
    [[nodiscard]] QueueItem *next() const noexcept { return _next; }

    /**
     * @param item Item to be set as next.
     */
    void next(QueueItem *item) noexcept { _next = item; }

private:
    QueueItem *_next{nullptr};
};

} // namespace mx::util