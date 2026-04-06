#!/bin/bash
echo "Building ChampSim with SPP running in L2C with MSHR sizes x100"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/spp_mshr_x100_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/spp_l2c_mshr_x100

echo "SPP in L2C with MSHR sizes x100 build complete!"
