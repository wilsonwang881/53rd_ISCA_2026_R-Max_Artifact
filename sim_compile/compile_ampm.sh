#!/bin/bash
echo "Building ChampSim with AMPM running in L2C"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/ampm_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/ampm_l2c

echo "AMPM in L2C build complete!"
