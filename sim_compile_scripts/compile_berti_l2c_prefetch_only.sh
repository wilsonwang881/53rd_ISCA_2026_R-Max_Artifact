#!/bin/bash
echo "Building ChampSim with Berti running in L1D, only issue to L2C"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/berti_l2c_prefetch_only_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/berti_l1d_only_prefetch_l2c

echo "Berti in L1D, only issue to L2C build complete!"
