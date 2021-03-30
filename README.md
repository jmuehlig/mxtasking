# MxTasking: Task-based framework with built-in prefetching and synchronization

MxTasking is a task-based framework that assists the design of latch-free and parallel data structures. 
MxTasking eases the information exchange between applications and the operating system, resulting in novel opportunities to manage resources in a truly hardware- and application-conscious way.

## Dependencies
### For building
#### Required
* `cmake` `>= 3.10`
* `clang` `>= 10`
* `clang-tidy` `>= 10`
* `libnuma` or `libnuma-dev`

#### Optional
* `libgtest-dev` for tests in `test/`

### For generating the YCSB workload
* `python` `>= 3`
* `java`
* `curl`

## How to build
* Call `cmake .` to generate `Makefile`.
* Call `make` to generate all binaries.

## How to run
For detailed information please see README files in `src/application/<app>` folders:
* [B Link Tree benchmark](src/application/blinktree_benchmark/README.md)  (`src/application/blinktree_benchmark`)
* [Hash Join benchmark](src/application/hashjoin_benchmark/README.md) (`src/application/hashjoin_benchmark`)

### Simple example for B Link Tree
* Call `make ycsb-a` to generate the default workload
* Call `./bin/blinktree_benchmark 1:4` to run benchmark for one to four cores.

## External Libraries
* `argparse` ([view on github](https://github.com/p-ranav/argparse)) under MIT license
* `json` ([view on github](https://github.com/nlohmann/json)) under MIT license
* Yahoo! Cloud Serving Benchmark ([view on github](https://github.com/brianfrankcooper/YCSB)) under  Apache License 2.0
