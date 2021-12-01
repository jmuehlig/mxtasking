cmake_minimum_required(VERSION 3.10)
project(mxtasking)

# Check SSE is available
INCLUDE(scripts/FindSSE.cmake)
FindSSE()

# Set compile flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--extra-arg-before=-std=c++17 --system-headers=0")
set(CMAKE_CXX_FLAGS "-pedantic -Wall -Wextra -Werror \
 -Wno-invalid-offsetof -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization \
 -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Woverloaded-virtual \
 -Wredundant-decls -Wshadow -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef \
 -Wno-unused -Wold-style-cast -Wno-uninitialized")

# Set compile flag for x86_64
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
endif()

# Set SSE flag if available
IF(SSE4_2_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -DUSE_SSE2")
ENDIF(SSE4_2_FOUND)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG -flto")
set(CMAKE_BUILD_TYPE RELEASE)

# Directories for output binaries and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# External libraries
find_library(GTEST gtest)

# Include folders
include_directories(src/ lib/)

# Source files
SET(MX_TASKING_SRC
    src/mx/resource/builder.cpp
    src/mx/tasking/scheduler.cpp
    src/mx/tasking/worker.cpp
    src/mx/tasking/task.cpp
    src/mx/tasking/profiling/profiling_task.cpp
    src/mx/util/core_set.cpp
    src/mx/util/random.cpp
    src/mx/memory/dynamic_size_allocator.cpp
    src/mx/memory/reclamation/epoch_manager.cpp
)

SET(MX_BENCHMARKING_SRC
    src/benchmark/workload_set.cpp
    src/benchmark/workload.cpp
    src/benchmark/cores.cpp
    src/benchmark/perf.cpp
    src/benchmark/string_util.cpp
)

# Build libraries
add_library(mxtasking SHARED ${MX_TASKING_SRC})
add_library(mxbenchmarking SHARED ${MX_BENCHMARKING_SRC})

# Build executables
add_executable(blinktree_benchmark
    src/application/blinktree_benchmark/main.cpp
    src/application/blinktree_benchmark/benchmark.cpp
)
target_link_libraries(blinktree_benchmark pthread numa atomic mxtasking mxbenchmarking)

add_executable(hashjoin_benchmark
    src/application/hashjoin_benchmark/main.cpp
    src/application/hashjoin_benchmark/benchmark.cpp
    src/application/hashjoin_benchmark/merge_task.cpp
    src/application/hashjoin_benchmark/tpch_table_reader.cpp
    src/application/hashjoin_benchmark/notifier.cpp
)
target_link_libraries(hashjoin_benchmark pthread numa atomic mxtasking mxbenchmarking)

add_executable(hello_world src/application/hello_world/main.cpp)
target_link_libraries(hello_world pthread numa atomic mxtasking mxbenchmarking)

# Add tests
if (GTEST)
    set(TESTS
        test/mx/memory/alignment_helper.test.cpp
        test/mx/memory/dynamic_size_allocator.test.cpp
        test/mx/memory/fixed_size_allocator.test.cpp
        test/mx/memory/tagged_ptr.test.cpp
        test/mx/util/aligned_t.test.cpp
        test/mx/util/mpsc_queue.test.cpp
        test/mx/util/queue.test.cpp
        test/mx/util/core_set.test.cpp
        test/mx/util/vector.test.cpp
    )

    add_executable(mxtests test/test.cpp ${TESTS})
    target_link_libraries(mxtests pthread numa atomic mxtasking mxbenchmarking gtest)
else()
    message("Library 'gtest' not found. Please install 'libgtest-dev' for unit tests.")
endif()

# Custom targets
add_custom_target(ycsb-a ${CMAKE_SOURCE_DIR}/scripts/generate_ycsb a randint)
add_custom_target(ycsb-c ${CMAKE_SOURCE_DIR}/scripts/generate_ycsb c randint)
