#include "perf.h"

using namespace benchmark;

/**
 * Counter "Instructions Retired"
 * Counts when the last uop of an instruction retires.
 */
[[maybe_unused]] PerfCounter Perf::INSTRUCTIONS = {"instr", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS};

/**
 */
[[maybe_unused]] PerfCounter Perf::CYCLES = {"cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES};

/**
 */
[[maybe_unused]] PerfCounter Perf::L1_MISSES = {"l1-miss", PERF_TYPE_HW_CACHE,
                                                PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                                                    (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)};

/**
 * Counter "LLC Misses"
 * Accesses to the LLC in which the data is not present(miss).
 */
[[maybe_unused]] PerfCounter Perf::LLC_MISSES = {"llc-miss", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES};

/**
 * Counter "LLC Reference"
 * Accesses to the LLC, in which the data is present(hit) or not present(miss)
 */
[[maybe_unused]] PerfCounter Perf::LLC_REFERENCES = {"llc-ref", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES};

/**
 * Micro architecture "Skylake"
 * Counter "CYCLE_ACTIVITY.STALLS_MEM_ANY"
 * EventSel=A3H,UMask=14H, CMask=20
 * Execution stalls while memory subsystem has an outstanding load.
 */
PerfCounter Perf::STALLS_MEM_ANY = {"memory-stall", PERF_TYPE_RAW, 0x145314a3};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.NTA"
 * EventSel=32H,UMask=01H
 * Number of PREFETCHNTA instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_NTA = {"sw-prefetch-nta", PERF_TYPE_RAW, 0x530132};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.T0"
 * EventSel=32H,UMask=02H
 * Number of PREFETCHT0 instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_T0 = {"sw-prefetch-t0", PERF_TYPE_RAW, 0x530232};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.T1_T2"
 * EventSel=32H,UMask=04H
 * Number of PREFETCHT1 or PREFETCHT2 instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_T1_T2 = {"sw-prefetch-t1t2", PERF_TYPE_RAW, 0x530432};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.PREFETCHW"
 * EventSel=32H,UMask=08H
 * Number of PREFETCHW instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_WRITE = {"sw-prefetch-w", PERF_TYPE_RAW, 0x530832};