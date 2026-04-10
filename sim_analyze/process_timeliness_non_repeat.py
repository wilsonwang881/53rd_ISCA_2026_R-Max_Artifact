#!/usr/bin/env python3
import sys
import numpy as np
import math
from collections import defaultdict, deque
# import seaborn as sns
# import matplotlib.pyplot as plt
import pathlib
import re


def extract_warmup(log_file):
    pattern = re.compile(r'cycles:\s+(\d+)')
    
    try:
        with open(log_file, 'r') as f:
            for line in f:
                # First, check if this is the correct line to save processing time
                if "Warmup finished" in line:
                    match = pattern.search(line)
                    if match:
                        # Return the first captured group as an integer
                        return int(match.group(1))
    except FileNotFoundError:
        print(f"Error: The file {log_file} was not found.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        
    return None

# ------------------------------------------------------------
# Load oracle prefetch issue times
# ------------------------------------------------------------
def load_oracle(oracle_file, skip):
    pf_queue = defaultdict(deque)
    total_prefetches = 0
    skip_ins = int(skip)

    with open(oracle_file) as f:
        for line in f:
            if not line.strip():
                continue

            cyc, addr = line.split()[0:2]
            cyc = int(cyc)
            addr = int(addr)

            if cyc > skip_ins:
                pf_queue[addr].append(cyc)
                total_prefetches += 1

    return pf_queue, total_prefetches


# ------------------------------------------------------------
# Match demand events with earlier prefetches
# ------------------------------------------------------------
def match_saved_cycles(pf_queue, access_file, skip):

    saved = {}
    saved_array = []
    useful_prefetches = 0
    hits_seen = 0
    skipped_no_pf = 0
    skipped_future_pf = 0
    skip = int(skip)

    with open(access_file) as f:
        for line in f:
            if not line.strip():
                continue

            cyc, addr, hit, typ = line.split()

            demand = int(cyc)
            addr = int(addr)
            hit = int(hit)

            if hit != 1 or demand < skip:
                continue

            hits_seen += 1

            q = pf_queue.get(addr)
            if not q:
                skipped_no_pf += 1
                continue

            if q[0] > demand:
                skipped_future_pf += 1
                continue

            if q and q[0] <= demand:
                pf = q.popleft()

            diff = demand - pf
            diff = int(diff / 1)

            if diff in saved:
                saved[diff] = saved[diff] + 1
            else:
                saved[diff] = 1

            saved_array.append(diff)

            useful_prefetches += 1

    return saved, saved_array, useful_prefetches, hits_seen, \
           skipped_no_pf, skipped_future_pf

# ------------------------------------------------------------
# Main
# ------------------------------------------------------------
def main():
    root = pathlib.Path(sys.argv[1])
    print("Trace, Min, Q1, Median, Q3, Max, Mean")

    # Iterate through every item in the root folder
    for subdir in sorted(root.iterdir()):
        # Check if it's a directory (the Trace folder)
        if subdir.is_dir():
            trace_name = subdir.name
            
            # Define the paths to your specific files
            pf_file = subdir / "pf_acc.txt"
            cache_file = subdir / "cache_acc.txt"
            log_file = subdir / "log.txt"

            # Check if both files actually exist before processing
            if pf_file.exists() and cache_file.exists() and log_file.exists():
                skip_cycle = extract_warmup(log_file)
                pf_queue, total_pf = load_oracle(pf_file, skip_cycle)
    
                saved, saved_array, useful_pf, hits_seen, \
                    skipped_no_pf, skipped_future = \
                    match_saved_cycles(pf_queue, cache_file, skip_cycle)
                
                stats = []

                if len(saved_array) < 5:
                    stats = np.array([0, 0, 0, 0, 0, 0])
                else:
                    percentiles = np.percentile(saved_array, [0, 25, 50, 75, 100]).astype(int)
                    mean_val = int(np.mean(saved_array))
                    stats = np.append(percentiles, mean_val)

                minimum, q1, median, q3, maximum, mean = stats
                print(f"{trace_name}, {minimum}, {q1}, {median}, {q3}, {maximum}, {mean}")
            else:
                print(f"Skipping {trace_name}: Missing required .txt files.")

if __name__ == "__main__":
    main()
