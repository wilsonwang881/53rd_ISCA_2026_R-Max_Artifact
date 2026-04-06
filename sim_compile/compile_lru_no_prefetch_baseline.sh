#!/bin/bash
echo "Building ChampSim with LRU, No Prefetch Baseline"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/lru_no_prefetch_baseline_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/baseline

echo "LRU, No Prefetch Baseline build complete!"
