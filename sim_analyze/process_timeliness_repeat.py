#!/usr/bin/env python3
import sys
import numpy as np
import pathlib
import re
from collections import defaultdict, deque

# Regex patterns
IPC_PATTERN = re.compile(r'cumulative IPC:\s+([\d.]+)')
WARMUP_PATTERN = re.compile(r'cycles:\s+(\d+)')
ORACLE_NAME_PATTERN = re.compile(r'oracle\.(\d+)\.txt')

def get_best_log_info(subdir):
    """
    Finds the oracle.n.txt (n >= 1) with the highest final IPC.
    Only considers IPC values found after "*** Reached end of trace:".
    """
    best_ipc = -1.0
    best_n = None
    best_warmup = None

    for f in subdir.glob("log_*.txt"):
        match_n = ORACLE_NAME_PATTERN.match(f.name)
        if not match_n:
            continue
        
        current_n_str = match_n.group(1)
        current_n_int = int(current_n_str)
        
        # Condition: n must be at least 2
        if current_n_int < 1:
            continue
            
        last_ipc_in_file = None
        warmup_found = None
        reached_end = False

        try:
            with open(f, 'r') as file:
                for line in file:
                    # Capture Warmup cycles
                    if "Warmup finished" in line:
                        w_match = WARMUP_PATTERN.search(line)
                        if w_match:
                            warmup_found = int(w_match.group(1))
                    
                    # Look for end of trace marker
                    if "*** Reached end of trace:" in line:
                        reached_end = True
                    
                    # Only record IPC if we are in the "Post-Trace" section
                    if reached_end:
                        ipc_match = IPC_PATTERN.search(line)
                        if ipc_match:
                            last_ipc_in_file = float(ipc_match.group(1))
            
            # Update overall best if this file's final IPC is the highest seen
            if last_ipc_in_file is not None and last_ipc_in_file > best_ipc:
                best_ipc = last_ipc_in_file
                best_n = current_n_str
                best_warmup = warmup_found

        except Exception as e:
            print(f"Error reading {f}: {e}", file=sys.stderr)

    return best_n, best_warmup


# --- load_oracle and match_saved_cycles functions remain largely as you had them ---

def load_oracle(oracle_file, skip):
    pf_queue = defaultdict(deque)
    total_prefetches = 0
    skip_val = int(skip)

    with open(oracle_file) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 2: continue
            cyc, addr = int(parts[0]), int(parts[1])
            if cyc > skip_val:
                pf_queue[addr].append(cyc)
                total_prefetches += 1
    return pf_queue, total_prefetches

def match_saved_cycles(pf_queue, access_file, skip):
    saved_array = []
    skip_val = int(skip)

    with open(access_file) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3: continue
            cyc, addr, hit = int(parts[0]), int(parts[1]), int(parts[2])

            if hit != 1 or cyc < skip_val:
                continue

            q = pf_queue.get(addr)
            if q and q[0] <= cyc:
                pf_issue_time = q.popleft()
                diff = cyc - pf_issue_time
                saved_array.append(diff)

    return saved_array

def main():
    if len(sys.argv) < 2:
        print("Usage: ./script.py <root_directory>")
        sys.exit(1)

    root = pathlib.Path(sys.argv[1])
    print("Trace, Min, Q1, Mean, Q3, Max, Mean")

    for subdir in sorted(root.iterdir()):
        if subdir.is_dir():
            trace_name = subdir.name
            
            # Step 1: Find best 'n' (n >= 1) and its warmup cycles
            best_n, skip_cycle = get_best_log_info(subdir)

            if best_n is None:
                continue

            # Step 2: Set paths using the best 'n'
            pf_file = subdir / f"oracle_of_timing.{best_n}.txt"
            cache_file = subdir / f"L2C_phy_acc.{best_n}.txt"

            if pf_file.exists() and cache_file.exists():
                warmup = skip_cycle if skip_cycle is not None else 0
                
                pf_queue, _ = load_oracle(pf_file, warmup)
                saved_array = match_saved_cycles(pf_queue, cache_file, warmup)
                
                # Step 3: Calculate custom stats
                stats = []

                if len(saved_array) < 5:
                    stats = np.array([0, 0, 0, 0, 0, 0])
                else:
                    percentiles = np.percentile(saved_array, [0, 25, 50, 75, 100]).astype(int)
                    mean_val = int(np.mean(saved_array))
                    stats = np.append(percentiles, mean_val)
                
                minimum, q1_cs, median, q3_cs, maximum, mean = stats
                print(f"{trace_name}, {minimum}, {q1_cs}, {median}, {q3_cs}, {maximum}, {mean}")

if __name__ == "__main__":
    main()