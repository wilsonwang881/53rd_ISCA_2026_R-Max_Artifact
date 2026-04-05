#!/bin/bash
echo "Building ChampSim with Berti running in L1D with MSHR sizes x100"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/berti_mshr_x100_configs.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/berti_l1d_mshr_x100

echo "Berti in L1D with MSHR sizes x100 build complete!"
