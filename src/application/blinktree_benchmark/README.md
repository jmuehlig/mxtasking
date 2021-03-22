# BLinkTree Benchmark
The BLinkTree-benchmark stores `8` byte numeric keys and values.
Call `./bin/blinktree_benchmark -h` for help and parameters.

## How to generate YCSB workload
* Workload specifications are done by files in `workloads_specification/`.
* Call `make ycsb-a` and `make ycsb-c` to generate workloads **A** and **C**.
* Workload files are stored in `workloads/`
* Use `./bin/blinktree_benchmark -f <fill-file> <mixed-file>` to pass the desired workload.
* Default (if not specified) is `-f workloads/fill_randint_workloada workloads/mixed_randint_workloada`.

## Important CLI arguments
* The first argument is the number of cores:
    * `./bin/blinktree_benchmark 1` for using a single core.
    * `./bin/blinktree_benchmark 1:24` for using cores `1` up to `24`.
* `-i <NUMBER>` specifies the number of repetitions of each workload.
* `-s <NUMBER>` steps of the cores:
    * `-s 1` will increase the used cores by one (core ids: `0,1,2,3,4,5,6,7,..,23`).
    * `-s 2` will skip every second core (core ids: `0,1,3,5,7,..23`).
* `-pd <NUMBER>` specifies the prefetch distance.
* `-p` or `--perf` will activate performance counter (result will be printed to console and output file).
* `--latched` will enable latches for synchronization (default off).
* `--exclusive` forces the tasks to access tree nodes exclusively (e.g. by using spinlocks or core-based sequencing) (default off).
*  `--sync4me` will use built-in synchronization selection to choose the matching primitive based on annotations.
* `-o <FILE>` will write the results in **json** format to the given file.

## Understanding the output
After started, the benchmark will print a summary of configured cores and workload:

    core configuration: 
      1: 0
      2: 0 1
      4: 0 1 2 3
    workload: fill: 5m / readonly: 5m

Here, we configured the benchmark to use one to four cores; each line of the core configuration displays the number of cores and the core identifiers.

Following, the benchmark will be started and print the results for every iteration:

    1	1	0	1478 ms	3.38295e+06 op/s
    1	1	1	1237 ms	4.04204e+06 op/s
    2	1	0	964 ms	5.18672e+06 op/s
    2	1	1	675 ms	7.40741e+06 op/s
    4	1	0	935 ms	5.34759e+06 op/s
    4	1	1	532 ms	9.3985e+06 op/s
    
* The first column is the number of used cores.
* The second column displays the iteration of the benchmark (configured by `-i X`).
* Thirdly, the phase-identifier will be printed: `0` for initialization phase (which will be only inserts) and `1` for the workload phase (which is read-only here).
* After that, the time and throughput are written.
* If `--perf` is enabled, the output will be extended by some perf counters, which are labeled (like throughput).

## Plot the results
When using `-o FILE`, the results will be written to the given file, using `JSON` format.
The plot script `scripts/plot_blinktree_benchmark INPUT_FILE [INPUT_FILE ...]` will aggregate and plot the results using one or more of those `JSON` files.

## Examples

###### Running workload A using optimistic synchronization

    ./bin/blinktree_benchmark 1: -s 2 -i 3 -pd 3 -p -f workloads/fill_randint_workloada workloads/mixed_randint_workloada -o optimistic.json

###### Running workload A using best matching synchronization

    ./bin/blinktree_benchmark 1: -s 2 -i 3 -pd 3 -p --sync4me -f workloads/fill_randint_workloada workloads/mixed_randint_workloada -o sync4me.json

###### Running workload A using reader/writer-locks
    
    ./bin/blinktree_benchmark 1: -s 2 -i 3 -pd 3 -p --latched -f workloads/fill_randint_workloada workloads/mixed_randint_workloada -o rwlocked.json
    
###### Running workload A using core-based sequencing
    
    ./bin/blinktree_benchmark 1: -s 2 -i 3 -pd 3 -p --exclusive -f workloads/fill_randint_workloada workloads/mixed_randint_workloada -o core-sequenced.json
    
###### Running workload A using spin-locks
        
    ./bin/blinktree_benchmark 1: -s 2 -i 3 -pd 3 -p --latched --exclusive -f workloads/fill_randint_workloada workloads/mixed_randint_workloada -o spinlocked.json
        
        