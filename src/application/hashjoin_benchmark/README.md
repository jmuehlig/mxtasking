# HashJoin Benchmark
Benchmark of a parallel, task-based HashJoin.

## How to generate workload
* Download TPC-H benchmark and generate tables
* Specify joined tables and key-indices via CLI arguments

## Important CLI arguments
* The first argument is the number of cores:
    * `./bin/hashjoin_benchmark 1` for using a single core.
    * `./bin/hashjoin_benchmark 1:24` for using cores `1` up to `24`.
* `-i <NUMBER>` specifies the number of repetitions of each workload.
* `-s <NUMBER>` steps of the cores:
    * `-s 1` will increase the used cores by one (core ids: `0,1,2,3,4,5,6,7,..,23`).
    * `-s 2` will skip every second core (core ids: `0,1,3,5,7,..23`).
* `-pd <NUMBER>` specifies the prefetch distance.
* `-p` or `--perf` will activate performance counter (result will be printed to console and output file).
* `-R` specifies the TPC-H table file for the left relation.
* `-R-key` specifies the index of the join key for `R`.
* `-S` specifies the TPC-H table file for the right relation.
* `-S-key` specifies the index of the join key for `S`.
* `--batch` specifies the records per task (comma separated: `8,16,64,256`)

## Understanding the output
After started, the benchmark will print a summary of configured cores and workload:

    core configuration: 
      1: 0
      2: 0 1
      4: 0 1 2 3
    workload: customer.tbl.0 (#3000000) JOIN orders.tbl.1 (#30000000)

Here, we configured the benchmark to use one to four cores; each line of the core configuration displays the number of cores and the core identifiers.

Following, the benchmark will be started and print the results for every iteration:

    1	1	64	1478 ms	3.38295e+06 op/s
    2	1	64	964 ms	5.18672e+06 op/s
    4	1	64	935 ms	5.34759e+06 op/s
    
* The first column is the number of used cores.
* The second column displays the iteration of the benchmark (configured by `-i X`).
* Thirdly, the granularity of how many records per task will be processed.
* After that, the time and throughput are written.
* If `--perf` is enabled, the output will be extended by some perf counters, which are labeled (like throughput).

## Plot the results
When using `-o FILE`, the results will be written to the given file, using `JSON` format.
The plot script `scripts/plot_hashjoin_benchmark INPUT_FILE` will aggregate and plot the results using one `JSON` file.
