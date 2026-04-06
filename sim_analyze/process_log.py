import argparse
import os
import re
import csv
import math
from pathlib import Path

def get_cache_level_from_path(config_path):
    config_lower = config_path.lower()
    if 'l1d' in config_lower: return 'L1D'
    elif 'l2c' in config_lower: return 'L2C'
    elif 'llc' in config_lower: return 'LLC'
    return 'L2C'

def parse_log_file(file_path):
    with open(file_path, 'r') as f:
        content = f.read()

    if "ChampSim completed all CPUs" not in content:
        return None

    ipc_match = re.search(r"CPU\s+\d+\s+cumulative IPC:\s+([0-9.]+)", content)
    ipc = float(ipc_match.group(1)) if ipc_match else 0.0

    dram_util = 0
    dram_pattern = r"Channel\s+\d+\s+(?:RQ|WQ)\s+ROW_BUFFER_HIT:\s+(\d+)\s+ROW_BUFFER_MISS:\s+(\d+)"
    for hit, miss in re.findall(dram_pattern, content):
        dram_util += int(hit) + int(miss)

    cache_stats = {}
    for cl in ["L1D", "L2C", "LLC"]:
        pf_accuracy = 0.0
        total_miss = 0
        prefetch_miss = 0

        pf_pattern = rf"(?:cpu\d+_)?{cl}\s+PREFETCH REQUESTED:.*?USEFUL:\s+(\d+)\s+USELESS:\s+(\d+)"
        pf_match = re.search(pf_pattern, content)
        if pf_match:
            useful = int(pf_match.group(1))
            useless = int(pf_match.group(2))
            total_reqs = useful + useless
            if total_reqs > 0:
                pf_accuracy = useful / total_reqs

        total_pattern = rf"(?:cpu\d+_)?{cl}\s+TOTAL\s+ACCESS:\s+\d+\s+HIT:\s+\d+\s+MISS:\s+(\d+)"
        total_match = re.search(total_pattern, content)
        if total_match:
            total_miss = int(total_match.group(1))

        pf_miss_pattern = rf"(?:cpu\d+_)?{cl}\s+PREFETCH\s+ACCESS:\s+\d+\s+HIT:\s+\d+\s+MISS:\s+(\d+)"
        pf_miss_match = re.search(pf_miss_pattern, content)
        if pf_miss_match:
            prefetch_miss = int(pf_miss_match.group(1))

        adjusted_misses = total_miss - prefetch_miss
        
        cache_stats[cl] = {
            "pf_accuracy": pf_accuracy,
            "adjusted_misses": adjusted_misses
        }

    return {"ipc": ipc, "dram_util": dram_util, "cache_stats": cache_stats}

def process_trace_folder(trace_dir):
    trace_path = Path(trace_dir)
    log_files = list(trace_path.glob("log*.txt"))
    if not log_files: return None

    best_result = None
    for log_file in log_files:
        result = parse_log_file(log_file)
        if result:
            if best_result is None or result["ipc"] > best_result["ipc"]:
                best_result = result
    return best_result

def process_configuration(config_path, label, results_db):
    config_path = Path(config_path)
    results_dir = config_path / "results"
    
    if not results_dir.exists() or not results_dir.is_dir():
        print(f"[WARNING] Results directory not found for {label}: {results_dir}")
        return

    print(f"Parsing {label}...")
    for trace_dir in sorted(results_dir.iterdir()):
        if trace_dir.is_dir():
            best_res = process_trace_folder(trace_dir)
            if best_res:
                clean_name = trace_dir.name.replace(".champsimtrace.xz", "")
                if clean_name not in results_db:
                    results_db[clean_name] = {}
                results_db[clean_name][label] = best_res

# --- Math Helper Functions ---
def geomean(iterable):
    clean_vals = [x for x in iterable if x > 0]
    if not clean_vals: return 0.0
    return math.exp(math.fsum(math.log(x) for x in clean_vals) / len(clean_vals))

def geomean_epsilon(iterable, epsilon=1e-6):
    if not iterable: return 0.0
    gm = math.exp(math.fsum(math.log(x + epsilon) for x in iterable) / len(iterable))
    return max(0.0, gm - epsilon)

def get_trace_group(trace_name):
    name_lower = trace_name.lower()
    if "compute_fp" in name_lower: return "compute_fp"
    if "compute_int" in name_lower: return "compute_int"
    if "srv" in name_lower: return "srv"
    return "other"

def write_csv(results_db, config_labels, config_cache_levels, is_cvp, trace_info, output_filename="final_results.csv"):
    with open(output_filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        
        headers = ['Trace', 'Baseline_IPC', 'Baseline_DRAM_Util']
        for config in config_labels:
            cl = config_cache_levels[config]
            headers.extend([
                f'{config}_IPC_Improvement_%', 
                f'{config}_DRAM_Change_%', 
                f'{config}_{cl}_PF_Accuracy_%', 
                f'{config}_{cl}_PF_Coverage_%'
            ])
        writer.writerow(headers)

        group_metrics = {}
        all_group_key = "CVP_ALL" if is_cvp else "SP_ALL"

        workloads = {}
        for clean_name, data in results_db.items():
            if "Baseline" not in data:
                continue

            if clean_name not in trace_info:
                if is_cvp:
                    trace_info[clean_name] = {'workload': clean_name, 'group': get_trace_group(clean_name), 'weight': 1.0}
                else:
                    print(f"[WARNING] Trace {clean_name} not found in weights.csv! Skipping.")
                    continue

            wl = trace_info[clean_name]['workload']
            if wl not in workloads:
                workloads[wl] = []
            workloads[wl].append(clean_name)

        aggregate_rows = [] # List to store aggregate rows to print later

        # Process each workload
        for wl, traces in sorted(workloads.items()):
            group = trace_info[traces[0]]['group']
            if group not in group_metrics:
                group_metrics[group] = {cfg: {'ipc_ratios': [], 'dram_ratios': [], 'acc_fracs': [], 'miss_ratios': []} for cfg in config_labels}
            if all_group_key not in group_metrics:
                group_metrics[all_group_key] = {cfg: {'ipc_ratios': [], 'dram_ratios': [], 'acc_fracs': [], 'miss_ratios': []} for cfg in config_labels}

            # 1. Print Individual Trace Rows
            for t in sorted(traces):
                base = results_db[t]["Baseline"]
                row = [t, f"{base['ipc']:.4f}", base['dram_util']]

                for config in config_labels:
                    conf_data = results_db[t].get(config)
                    cl = config_cache_levels[config]

                    if conf_data:
                        ipc_ratio = conf_data['ipc'] / base['ipc'] if base['ipc'] else 0.0
                        ipc_impr = (ipc_ratio - 1.0) * 100

                        dram_ratio = conf_data['dram_util'] / base['dram_util'] if base['dram_util'] else 0.0
                        dram_change = (dram_ratio - 1.0) * 100 if base['dram_util'] > 0 else 0.0

                        acc_pct = conf_data['cache_stats'][cl]['pf_accuracy'] * 100

                        b_miss = base['cache_stats'][cl]['adjusted_misses']
                        c_miss = conf_data['cache_stats'][cl]['adjusted_misses']
                        miss_ratio = c_miss / b_miss if b_miss > 0 else 1.0
                        cov_pct = (1.0 - miss_ratio) * 100

                        row.extend([f"{ipc_impr:.4f}", f"{dram_change:.4f}", f"{acc_pct:.4f}", f"{cov_pct:.4f}"])

                        if is_cvp:
                            group_metrics[group][config]['ipc_ratios'].append(ipc_ratio)
                            group_metrics[group][config]['dram_ratios'].append(dram_ratio)
                            group_metrics[group][config]['acc_fracs'].append(acc_pct / 100.0)
                            group_metrics[group][config]['miss_ratios'].append(miss_ratio)

                            group_metrics[all_group_key][config]['ipc_ratios'].append(ipc_ratio)
                            group_metrics[all_group_key][config]['dram_ratios'].append(dram_ratio)
                            group_metrics[all_group_key][config]['acc_fracs'].append(acc_pct / 100.0)
                            group_metrics[all_group_key][config]['miss_ratios'].append(miss_ratio)
                    else:
                        row.extend(["N/A", "N/A", "N/A", "N/A"])
                
                writer.writerow(row)

            # Compute Workload Aggregate (SimPoint ONLY) and save it for later
            if not is_cvp:
                wl_row = [f"AGGREGATE_{wl}", "", ""] 
                
                sum_base_w = sum(trace_info[t]['weight'] for t in traces)
                if sum_base_w > 0:
                    wl_base_ipc = sum(results_db[t]["Baseline"]['ipc'] * trace_info[t]['weight'] for t in traces) / sum_base_w
                    wl_base_dram = sum(results_db[t]["Baseline"]['dram_util'] * trace_info[t]['weight'] for t in traces) / sum_base_w
                    wl_row[1] = f"{wl_base_ipc:.4f}"
                    wl_row[2] = f"{wl_base_dram:.0f}"

                for config in config_labels:
                    cl = config_cache_levels[config]

                    valid_t = [t for t in traces if config in results_db[t]]
                    failed_t = [t for t in traces if config not in results_db[t]]

                    if failed_t:
                        print(f"[WARNING] Traces {failed_t} failed or missing logs for '{config}'. Renormalizing weights for workload '{wl}'.")

                    sum_w = sum(trace_info[t]['weight'] for t in valid_t)
                    if sum_w > 0:
                        w_base_ipc = sum(results_db[t]["Baseline"]['ipc'] * trace_info[t]['weight'] for t in valid_t) / sum_w
                        w_conf_ipc = sum(results_db[t][config]['ipc'] * trace_info[t]['weight'] for t in valid_t) / sum_w
                        w_ipc_impr = (w_conf_ipc / w_base_ipc - 1.0) * 100 if w_base_ipc > 0 else 0.0

                        w_dram_pct = 0.0
                        w_acc_pct = 0.0
                        w_cov_pct = 0.0

                        for t in valid_t:
                            t_w = trace_info[t]['weight'] / sum_w
                            
                            b_dram = results_db[t]["Baseline"]['dram_util']
                            c_dram = results_db[t][config]['dram_util']
                            t_dram_pct = ((c_dram / b_dram) - 1.0) * 100 if b_dram > 0 else 0.0
                            w_dram_pct += t_dram_pct * t_w

                            t_acc = results_db[t][config]['cache_stats'][cl]['pf_accuracy'] * 100
                            w_acc_pct += t_acc * t_w

                            b_miss = results_db[t]["Baseline"]['cache_stats'][cl]['adjusted_misses']
                            c_miss = results_db[t][config]['cache_stats'][cl]['adjusted_misses']
                            t_cov_pct = (1.0 - c_miss / b_miss) * 100 if b_miss > 0 else 0.0
                            w_cov_pct += t_cov_pct * t_w

                        wl_row.extend([f"{w_ipc_impr:.4f}", f"{w_dram_pct:.4f}", f"{w_acc_pct:.4f}", f"{w_cov_pct:.4f}"])

                        group_metrics[group][config]['ipc_ratios'].append(1.0 + w_ipc_impr / 100.0)
                        group_metrics[group][config]['dram_ratios'].append(1.0 + w_dram_pct / 100.0)
                        group_metrics[group][config]['acc_fracs'].append(w_acc_pct / 100.0)
                        group_metrics[group][config]['miss_ratios'].append(1.0 - w_cov_pct / 100.0)

                        group_metrics[all_group_key][config]['ipc_ratios'].append(1.0 + w_ipc_impr / 100.0)
                        group_metrics[all_group_key][config]['dram_ratios'].append(1.0 + w_dram_pct / 100.0)
                        group_metrics[all_group_key][config]['acc_fracs'].append(w_acc_pct / 100.0)
                        group_metrics[all_group_key][config]['miss_ratios'].append(1.0 - w_cov_pct / 100.0)
                    else:
                        wl_row.extend(["N/A", "N/A", "N/A", "N/A"])

                aggregate_rows.append(wl_row)

        # 2. Print Workload Aggregates Block
        if aggregate_rows:
            writer.writerow([]) # Visual spacing
            for row in aggregate_rows:
                writer.writerow(row)

        # 3. Print Group Geomean Summaries
        writer.writerow([]) # Visual spacing
        for g_name in list(group_metrics.keys()):
            if not any(group_metrics[g_name][cfg]['ipc_ratios'] for cfg in config_labels):
                continue

            summary_row = [f"GEOMEAN_{g_name.upper()}", "", ""]

            for config in config_labels:
                metrics = group_metrics[g_name][config]

                gm_ipc_ratio = geomean(metrics['ipc_ratios'])
                gm_dram_ratio = geomean(metrics['dram_ratios'])
                gm_acc_frac = geomean_epsilon(metrics['acc_fracs'], epsilon=1e-6)
                gm_miss_ratio = geomean_epsilon(metrics['miss_ratios'], epsilon=1e-6)

                mean_ipc_impr = (gm_ipc_ratio - 1.0) * 100 if gm_ipc_ratio > 0 else 0.0
                mean_dram_change = (gm_dram_ratio - 1.0) * 100 if gm_dram_ratio > 0 else 0.0
                mean_acc_pct = gm_acc_frac * 100
                mean_cov_pct = (1.0 - gm_miss_ratio) * 100

                summary_row.extend([f"{mean_ipc_impr:.4f}", f"{mean_dram_change:.4f}", f"{mean_acc_pct:.4f}", f"{mean_cov_pct:.4f}"])

            writer.writerow(summary_row)

    print(f"\n[✔] Successfully wrote results and summaries to {output_filename}")

def main():
    parser = argparse.ArgumentParser(description="Parse ChampSim logs to CSV.")
    parser.add_argument("--baseline", required=True, help="Path to baseline folder")
    parser.add_argument("--configs", nargs="+", required=True, help="Paths to other config folders")
    args = parser.parse_args()

    print("---------------------------------------------------------")
    ans = input("Are the traces being parsed CVP traces? (y/n): ").strip().lower()
    is_cvp = True if ans == 'y' else False
    
    trace_info = {}
    if not is_cvp:
        weights_file = input("Please enter the path to the weights.csv file: ").strip()
        if not os.path.exists(weights_file):
            print(f"ERROR: Weights file '{weights_file}' not found.")
            return
            
        with open(weights_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                t_clean = Path(row['trace']).name.replace('.champsimtrace.xz', '')
                trace_info[t_clean] = {
                    'workload': row['workload'],
                    'group': row['group'],
                    'weight': float(row['weight'])
                }
    print("---------------------------------------------------------")

    results_db = {}
    config_labels = []
    config_cache_levels = {}

    process_configuration(args.baseline, "Baseline", results_db)

    for config_path in args.configs:
        label = Path(config_path).name
        config_labels.append(label)
        config_cache_levels[label] = get_cache_level_from_path(label)
        process_configuration(config_path, label, results_db)

    write_csv(results_db, config_labels, config_cache_levels, is_cvp, trace_info)

if __name__ == "__main__":
    main()