#!/bin/bash
echo "Building ChampSim with SPP-Max running in L2C"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/spp_max_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/spp_max_l2c

echo "SPP-Max in L2C build complete!"
