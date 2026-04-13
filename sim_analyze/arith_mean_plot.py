import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import sys
import os

groups = [
    # '600.perlbench',
    # '602.gcc',
    # '603.bwaves',
    # '605.mcf',
    # '607.cactuBSSN',
    # '619.lbm',
    # '620.omnetpp',
    # '621.wrf',
    # '623.xalancbmk',
    # '625.x264',
    # '627.cam4',
    # '628.pop2',
    # '631.deepsjeng',
    # '638.imagick',
    # '641.leela',
    # '644.nab',
    # '648.exchange2',
    # '649.fotonik3d',
    # '654.roms',
    # '657.xz',
    'compute_fp',
    'compute_int',
    'srv'
]

# 1. Function to process a single CSV and return the stats per category + Overall
def get_summaries(file_path):
    df = pd.read_csv(file_path)
    print(f'Reading {file_path}.')
    df.columns = df.columns.str.strip()
    
    def get_category(name):
        for g in groups:
            if g in name:
                return g
        return 'misc'
    
    df['Group'] = df['Trace'].apply(get_category)
    numeric_cols = ['Min', 'Q1', 'Median', 'Q3', 'Max', 'Mean']
    
    summaries = {}
    group_vectors = []
    
    # Compute stats for each specific group
    for group in groups:
        subset = df[df['Group'] == group][numeric_cols]
        if not subset.empty:
            # Custom vector: Min of Min, mean of Q1/Median/Q3/Mean, max of Max
            am_vector = [
                subset['Min'].min(),
                subset['Q1'].mean(),
                subset['Median'].mean(),
                subset['Q3'].mean(),
                subset['Max'].max(),
                subset['Mean'].mean()
            ]
            summaries[group] = am_vector
            group_vectors.append(am_vector)
        else:
            summaries[group] = None

    # Compute the stats for the right-hand summary
    if group_vectors:
        # Min of Mins, max of Maxes, mean of the rest across the collected category vectors
        summaries['all_groups'] = [
            np.min([v[0] for v in group_vectors]),  # Min
            np.mean([v[1] for v in group_vectors]), # Q1
            np.mean([v[2] for v in group_vectors]), # Median
            np.mean([v[3] for v in group_vectors]), # Q3
            np.max([v[4] for v in group_vectors]),  # Max
            np.mean([v[5] for v in group_vectors])  # Mean
        ]
    else:
        summaries['all_groups'] = None
        
    return summaries

# 2. Collect data from all files passed via CMD arguments
file_paths = sys.argv[1:]
if not file_paths:
    print("Usage: python script.py file1.csv file2.csv ...")
    sys.exit(1)

all_data = {}
for path in file_paths:
    label = os.path.splitext(os.path.basename(path))[0]
    all_data[label] = get_summaries(path)

# 3. Setup Plotting Structures
categories = groups + ['all_groups']
num_files = len(file_paths)
group_gap = 1.5          

stats_list = []
positions = []
box_colors = []

cmap = plt.get_cmap('tab10') 
file_colors = {label: cmap(i) for i, label in enumerate(all_data.keys())}

# 4. Build the Stats and Positions
for g_idx, group in enumerate(categories):
    base_pos = g_idx * (num_files + group_gap)
    
    for f_idx, (label, summaries) in enumerate(all_data.items()):
        data = summaries.get(group)
        if data is not None:
            stats_list.append({
                'group': group,         # Used for the terminal printout
                'file_label': label,    # Used for the terminal printout
                'label': label if g_idx == 0 else "", # Used by the plot (avoids duplicate x-axis text)
                'whislo': data[0], 
                'q1': data[1],
                'med': data[2], 
                'q3': data[3],
                'whishi': data[4],
                'mean': data[5]
            })
            positions.append(base_pos + f_idx)
            box_colors.append(file_colors[label])

# Print out the calculated statistics clearly
print("Trace, Min, Q1, Median, Q3, Max, Mean")

for prefetcher in stats_list:
    print(f"{prefetcher['group']},{prefetcher['file_label']},{prefetcher['whislo']},{prefetcher['q1']},{prefetcher['med']},{prefetcher['q3']},{prefetcher['whishi']},{prefetcher['mean']}")

# 5. Create the Plot
fig, ax = plt.subplots(figsize=(14, 7), dpi=300)

mean_style = dict(marker='D', markerfacecolor='yellow', markeredgecolor='black', markersize=5)

boxes = ax.bxp(
    stats_list, 
    positions=positions, 
    showfliers=False, 
    patch_artist=True, 
    widths=0.6,
    showmeans=True,       
    meanprops=mean_style 
)

# Apply colors
for patch, color in zip(boxes['boxes'], box_colors):
    patch.set_facecolor(color)
    patch.set_alpha(0.8)
    patch.set_edgecolor('black')

# 6. Formatting Axes and Legend
ax.set_yscale('log')
ax.set_title("Arithmetic Comparison Across Multiple Prefetchers", fontsize=16)
ax.set_ylabel("Cycles (Log Scale)", fontsize=12)

# Set X-Ticks labels (making 'all_groups' look like 'OVERALL')
group_centers = [i * (num_files + group_gap) + (num_files - 1) / 2 for i in range(len(categories))]
ax.set_xticks(group_centers)
ax.tick_params(axis='x', rotation=90)
clean_labels = [c.replace('all_groups', 'OVERALL').upper() for c in categories]
ax.set_xticklabels(clean_labels, fontsize=12)

# Add a vertical divider to separate individual groups from the overall summary
divider_pos = group_centers[-2] + (num_files + group_gap)/2
ax.axvline(x=divider_pos, color='black', linestyle='--', alpha=0.3)

# Legend
legend_elements = [Patch(facecolor=file_colors[label], label=label) for label in all_data.keys()]
ax.legend(handles=legend_elements, title="Prefetchers", loc='upper left', bbox_to_anchor=(1, 1))

ax.grid(axis='y', linestyle='--', alpha=0.5, which='both')
plt.tight_layout()
plt.savefig('prefetcher_comparison_with_overall.png', bbox_inches='tight')