#!/bin/bash
echo "Building ChampSim with R-Max running in L1D"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/r_max_l1d_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/r_max_l1d

echo "R-Max in L1D build complete!"
