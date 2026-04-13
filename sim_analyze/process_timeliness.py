#!/usr/bin/env python3
import sys
import argparse
import numpy as np
import pathlib
import re
from collections import defaultdict, deque

# --- Regex patterns ---
IPC_PATTERN = re.compile(r'cumulative IPC:\s+([\d.]+)')
WARMUP_PATTERN = re.compile(r'cycles:\s+(\d+)')
ORACLE_NAME_PATTERN = re.compile(r'log_(\d+)\.txt')

def extract_warmup(log_file):
    """Extracts warmup cycles from a standard log file (non-repeat mode)."""
    try:
        with open(log_file, 'r') as f:
            for line in f:
                if "Warmup finished" in line:
                    match = WARMUP_PATTERN.search(line)
                    if match:
                        return int(match.group(1))
    except FileNotFoundError:
        print(f"Error: The file {log_file} was not found.", file=sys.stderr)
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
    return None

def get_best_log_info(subdir):
    """
    Finds the oracle.n.txt (n >= 1) with the highest final IPC (repeat mode).
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
        if current_n_int <= 1:
            continue
            
        last_ipc_in_file = None
        warmup_found = None
        reached_end = False

        try:
            with open(f, 'r') as file:
                for line in file:
                    if "Warmup finished" in line:
                        w_match = WARMUP_PATTERN.search(line)
                        if w_match:
                            warmup_found = int(w_match.group(1))
                    
                    if "*** Reached end of trace:" in line:
                        reached_end = True
                    
                    if reached_end:
                        ipc_match = IPC_PATTERN.search(line)
                        if ipc_match:
                            last_ipc_in_file = float(ipc_match.group(1))
            
            if last_ipc_in_file is not None and last_ipc_in_file > best_ipc:
                best_ipc = last_ipc_in_file
                best_n = current_n_str
                best_warmup = warmup_found

        except Exception as e:
            print(f"Error reading {f}: {e}", file=sys.stderr)

    return best_n, best_warmup

def load_oracle(oracle_file, skip, verbose=False):
    """Loads oracle prefetch issue times."""
    pf_queue = defaultdict(deque)
    total_prefetches = 0
    skip_val = int(skip)

    with open(oracle_file) as f:
        for line in f:
            if not line.strip():
                continue
            parts = line.split()
            if len(parts) < 2: 
                continue
                
            cyc, addr = int(parts[0]), int(parts[1])
            if cyc > skip_val:
                pf_queue[addr].append(cyc)
                total_prefetches += 1
        
    return pf_queue

def match_saved_cycles(pf_queue, access_file, skip):
    """Matches demand events with earlier prefetches and calculates timeliness."""
    saved_array = []
    skip_val = int(skip)

    with open(access_file) as f:
        for line in f:
            if not line.strip():
                continue
            parts = line.split()
            # Handles both 3-column (repeat) and 4-column (non-repeat) formats
            if len(parts) < 3: 
                continue
                
            cyc, addr, hit = int(parts[0]), int(parts[1]), int(parts[2])

            if hit == 0 or cyc < skip_val:
                continue

            q = pf_queue.get(addr)
            if q and q[0] <= cyc:
                pf_issue_time = q.popleft()
                diff = cyc - pf_issue_time
                saved_array.append(diff)

    return saved_array

def main():
    parser = argparse.ArgumentParser(description="Process timeliness data for cache logs.")
    parser.add_argument("root_directory", help="Root directory containing the 'results' folder.")
    parser.add_argument("--mode", choices=["repeat", "non-repeat"], default="repeat", 
                        help="Select the processing mode (repeat or non-repeat).")
    
    args = parser.parse_args()
    root = pathlib.Path(args.root_directory) / "results"
    
    if not root.exists() or not root.is_dir():
        print(f"Error: Could not find results directory at {root}", file=sys.stderr)
        sys.exit(1)

    print("Trace, Min, Q1, Median, Q3, Max, Mean")

    for subdir in sorted(root.iterdir()):
        if not subdir.is_dir():
            continue
            
        trace_name = subdir.name
        
        # --- Mode Switching Logic ---
        if args.mode == "repeat":
            best_n, skip_cycle = get_best_log_info(subdir)
            if best_n is None:
                continue
            pf_file = subdir / f"oracle_pf_timing_{best_n}.txt"
            cache_file = subdir / f"cache_phy_acc_{best_n}.txt"
            warmup = skip_cycle if skip_cycle is not None else 0
            verbose = False

        else: # non-repeat
            pf_file = subdir / "pf_acc.txt"
            cache_file = subdir / "cache_acc.txt"
            log_file = subdir / "log.txt"
            
            if not log_file.exists():
                print(f"Skipping {trace_name}: Missing log.txt")
                continue
                
            skip_cycle = extract_warmup(log_file)
            warmup = skip_cycle if skip_cycle is not None else 0
            verbose = True

        # --- Shared File Processing ---
        if pf_file.exists() and cache_file.exists():
            pf_queue = load_oracle(pf_file, warmup, verbose=verbose)
            saved_array = match_saved_cycles(pf_queue, cache_file, warmup)
            
            stats = []
            if len(saved_array) < 5:
                stats = np.array([0, 0, 0, 0, 0, 0])
            else:
                percentiles = np.percentile(saved_array, [0, 25, 50, 75, 100]).astype(int)
                mean_val = int(np.mean(saved_array))
                stats = np.append(percentiles, mean_val)
            
            minimum, q1_cs, median, q3_cs, maximum, mean = stats
            print(f"{trace_name}, {minimum}, {q1_cs}, {median}, {q3_cs}, {maximum}, {mean}")
        else:
            print(f"Skipping {trace_name}: Missing required .txt files.")

if __name__ == "__main__":
    main()