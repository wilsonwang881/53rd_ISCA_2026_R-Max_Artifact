# ChampSim

![GitHub](https://img.shields.io/github/license/ChampSim/ChampSim)
![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/ChampSim/ChampSim/test.yml)
![GitHub forks](https://img.shields.io/github/forks/ChampSim/ChampSim)
[![Coverage Status](https://coveralls.io/repos/github/ChampSim/ChampSim/badge.svg?branch=develop)](https://coveralls.io/github/ChampSim/ChampSim?branch=develop)

ChampSim is a trace-based simulator for a microarchitecture study. If you have questions about how to use ChampSim, we encourage you to search the threads in the Discussions tab or start your own thread. If you are aware of a bug or have a feature request, open a new Issue.

# Using ChampSim

ChampSim is the result of academic research. To support its continued growth, please cite our work when you publish results that use ChampSim by clicking "Cite this Repository" in the sidebar.

# Download dependencies

ChampSim uses [vcpkg](https://vcpkg.io) to manage its dependencies. In this repository, vcpkg is included as a submodule. You can download the dependencies with
```
git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
```

# Compile

ChampSim takes a JSON configuration script. Examine `champsim_config.json` for a fully-specified example. All options described in this file are optional and will be replaced with defaults if not specified. The configuration scrip can also be run without input, in which case an empty file is assumed.
```
$ ./config.sh <configuration file>
$ make
```

# Download DPC-3 trace

Traces used for the 3rd Data Prefetching Championship (DPC-3) can be found here. (https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/) A set of traces used for the 2nd Cache Replacement Championship (CRC-2) can be found from this link. (http://bit.ly/2t2nkUj)

Storage for these traces is kindly provided by Daniel Jimenez (Texas A&M University) and Mike Ferdman (Stony Brook University). If you find yourself frequently using ChampSim, it is highly encouraged that you maintain your own repository of traces, in case the links ever break.

# Run simulation

Execute the binary directly.
```
$ bin/champsim --warmup_instructions 200000000 --simulation_instructions 500000000 ~/path/to/traces/600.perlbench_s-210B.champsimtrace.xz
```

The number of warmup and simulation instructions given will be the number of instructions retired. Note that the statistics printed at the end of the simulation include only the simulation phase.

# Add your own branch predictor, data prefetchers, and replacement policy
**Copy an empty template**
```
$ mkdir prefetcher/mypref
$ cp prefetcher/no_l2c/no.cc prefetcher/mypref/mypref.cc
```

**Work on your algorithms with your favorite text editor**
```
$ vim prefetcher/mypref/mypref.cc
```

**Compile and test**
Add your prefetcher to the configuration file.
```
{
    "L2C": {
        "prefetcher": "mypref"
    }
}
```
Note that the example prefetcher is an L2 prefetcher. You might design a prefetcher for a different level.

```
$ ./config.sh <configuration file>
$ make
$ bin/champsim --warmup_instructions 200000000 --simulation_instructions 500000000 600.perlbench_s-210B.champsimtrace.xz
```

# How to create traces

Program traces are available in a variety of locations, however, many ChampSim users wish to trace their own programs for research purposes.
Example tracing utilities are provided in the `tracer/` directory.

# Evaluate Simulation

ChampSim measures the IPC (Instruction Per Cycle) value as a performance metric. <br>
There are some other useful metrics printed out at the end of simulation. <br>

Good luck and be a champion! <br>

# How to Compile Binaries for Simulations in Batch

**sim_configs** has all the configuration files.

**sim_compile** has all the scripts to use the configurations in **sim_config** for compiling. Run `./sim_compile/compile_all.sh` in the root level of this ChampSim directory to compile all configurations. Or one can choose to compile any individual configuration by invoking the corresponding script.

# How to Run Binaries for Simulations in Batch

**sim_run** has corresponding the scripts.

Create a directory and change directory into it.

Run `<path to ChampSim directory>/sim_run/generate_commands.sh <path to the ChampSim trace folder> <path to the compiled binaries>` to setup the directory structure for simulation.

If asked whether the simulated workloads are CVP workloads, answer yes. Use the file **./sim_run/cvp_public_trace_length.txt** for the trace length information for CVP traces.

The command creates two files: `phase_1_jobs.txt` and `phase_2_jobs.txt`.

Each line from those two job files corresponds to a single simulation.

Use GNU Parallel or adapt to Slurm on `phase_1_jobs.txt`. Wait for the jobs to finish.

Run `<path to ChampSim directory>/sim_run/copy_translations.sh` in the root level of the created simulation folder.

Use GNU Parallel or adapt to Slurm on `phase_2_jobs.txt`. Wait for the jobs to finish.

For phase_2 jobs, make sure to allocate enough memory for the jobs. Allocate at least 3 times the size of the memory trace file.

Different traces or placing R-Max at different cache levels require different amount of RAM.

# How to Process Results

Please create different simulation folders for different benchmark suites.

Each sub-directory in the created simulation folder corresponds to a single setting.

The sub-directory **baseline** in the created simulation folder has the baseline results that have no prefetching and use LRU for cache replacement.

**./sim_analyze/weights.csv** has the weights for simpoints. Only GAP, XSBench and SPEC have weights. CVP-1 traces have no weights.

Run **<path to ChampSim director>/sim_analyze/process_log.py --baseline <path to the baseline folder> --configs <paths to all the folders containing different configurations> at the root level of the created simulation folder to process the data.

The above command will ask if processing CVP traces. If not processing CVP traces, input the path to the weights file, which can be found in **./sim_analyze/weights.csv** in this repository.

A final **final_results.csv** file will be created to display the results. The final csv file contains information on speedups, prefetch coverage, prefetch accuracy and DRAM utilization.

Note that for some settings, i.e. always hit l1d, may not display any values for the metric like prefetch accuracy, because it does not issue any prefetches.

# Steps to Change Prefetchers for Simulations

Below are the changes covered by the compiling scripts to run different simulations.

Found in **./sim_configs** directory. We evaluated AMPM, Berti, SPP, Berti+SPP+PPF, IP Stride, IPCP, SPP-Max, Berti-Max, also R-Max at different cache levels.

The following changes are needed:

1. In **inc/vmem.h**, change 

`bool RECORD_IN_USE = true;` to `bool RECORD_IN_USE = false;`

if you are not running R-Max, or running R-Max for the first time. Otherwise, keep it to true. This modified version of ChampSim records virtual to physical page translations if `bool RECORD_IN_USE = false;`, otherwise, it will be using pre-recorded fixed virtual to physical page translations. 

After the first iteration of R-Max, change the line to `bool RECORD_IN_USE = true;` and recompile the code before running the next iterations. 

2. In **prefetcher/oracle-l2/oracle.h**, please make the following changes if needed:

```
    ...
    // The filename of the memory access trace that will be used to produce prefetch/replacement sequences in the next iteration. 
    std::string L2C_PHY_ACC_FILE_NAME = "cache_phy_acc.txt"; 
    ...
    // The filename of the file that contains the prefetches issued by another prefetcher, i.e. SPP. 
    // When using the prefetches issued by another prefetcher, the simulations have to use the same virtual to physical page translations.
    std::string PF_ACC_FILE_NAME = "pf_acc.txt";
    ...
    // Change to true if only issuing prefetches generated by another prefetcher.
    // A file named using PF_ACC_FILE_NAME has to be present in the same directory where the simulation happens.
    constexpr static bool PF_ACC_COMPARE_ENABLED = false;

    // Change to true if the prefetch addresses are in virtual space.
    constexpr static bool TRANSLATE_PF_ADDR = false;

    // Change the following numbers to match the numbers of cache sets/ways found in the configuration file used for the simulation.
    const static int SET_NUM = 2048;
    const static int WAY_NUM = 10;
    ...
```

4. In **prefetcher/oracle-l2/spp.h**, please make the following change if needed:

```
    // The filename of the file that has the timing/address information for prefetches successfully issued by R-Max.
    std::string PF_ADDR_FILE_NAME = "oracle_pf_timing.txt";
```

5. In **inc/operable.h**, please make the following changes if needed:

```
  ...
  const std::string L1I_name = "cpu0_L1I";
  const std::string L1D_name = "cpu0_L1D";
  const std::string L2C_name = "cpu0_L2C";
  const std::string LLC_name = "LLC";
  const std::string DTLB_name = "cpu0_DTLB";
  const std::string ITLB_name = "cpu0_ITLB";
  const std::string STLB_name = "cpu0_STLB";

  // Change the value of the following variable to match the cache level where R-Max is placed.
  // i.e. if R-Max is placed in LLC, change to LLC_name instead of L2C_name, using the above pre-defined strings shown.
  const std::string ORACLE_at = L2C_name;

  // If a second R-Max is used, change the value to match the cache where the second R-Max is placed.
  const std::string ORACLE_at_2nd = L2C_name;
  ...
```

If only a single R-Max is used, please make sure to use **oracle-l2** as the prefetcher in the ChampSim configuration.

If two instances of R-Max is used, make sure to use **oracle-2nd** as the second R-Max prefetcher in the ChampSim configuration. Make changes regarding cache sets/ways and filenames as needed.

# Steps to Simulate Always Hit Cache

In **src/cache.cc**, please make the following changes:

```
  ...
  // Uncomment and change LLC_name to L1D_name, L2C_name, or L1I_name,
  // to simulate always hit cache.
  //const auto hit = !LLC_name.compare(NAME) ? true : (way != set_end); // WL

  // Comment out this line to simulate always hit cache.
  // Otherwise, do not comment it out.
  const auto hit = (way != set_end); // WL
  ...
  // Uncomment the following line and change LLC_name to the always hit cache name.
  //if (LLC_name.compare(NAME)) { // WL
      impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, way->address, handle_pkt.ip, 0,
                                    champsim::to_underlying(handle_pkt.type), true);
    // Uncomment the following line for always hit cache simulations.
    //} // WL
  ...
```

# Step to Simulate Cache Size Multiplied by 100 Times

In the corresponding configuration file, adjust **ways**  or **sets** accordingly. The paper shows some results by multiplying the numbers of ways of all cache levels by 100.

# Step to Simulate MSHR Size Multiplied by 100 Times

In the corresponding configuration file, adjust **mshr_size** accordingly. The paper shows some results by multiplying the number of MSHRs at all cache levels by 100.

# An Example Walkthrough on How to Run R-Max in L2

1. Clone and install all dependencies needed for ChampSim.

2. Download the ChampSim traces.

3. Go to **inc/vmem.h**, change `RECORD_IN_USE` to `false`.

4. Go to **inc/operable.h**, change `ORACLE_at` to `L2C_name`.

5. Go to **prefetcher/oracle-l2/oracle.h**, change `SET_NUM` and `WAY_NUM` to the numbers found in the configuration file used. Change `L2C_PHY_ACC_FILE_NAME` to any string you want, to show it is a memory trace captured at the L2 cache level.

6. Compile and run the simulation for the first time.

7. Go to **inc/vmem.h**, change `RECORD_IN_USE` to `true`.

8. Recompile and run the simulation as many times as you want, until convergence is emerging. In rare cases, the IPC values may be fluctuating. Pick the highest performing one.

We suggest keep a copy of the memory trace file and the prefetch timing/address file generated every iteration before the decision on which iteration result should be used for detailed analysis is finalized.

Note that in the configuration file, for the `prefetch_activate` field of the cache where R-Max is placed, it has to include `LOAD`, `RFO`, `TRANSLATION`, `WRITE`, `PREFETCH`.


