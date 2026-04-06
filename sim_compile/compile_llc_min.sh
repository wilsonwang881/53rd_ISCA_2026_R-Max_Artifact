#!/bin/bash
echo "Building ChampSim with Belady's Algorithm (MIN), No Prefetch running in LLC"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/llc_min_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/llc_min

echo "Belady's Algorithm (MIN) No Prefetch in LLC build complete!"
