#!/bin/bash
echo "Building ChampSim with Always Hit L2C"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/always_hit_l2c_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/always_hit_l2c

echo "Always Hit L2C build complete!"
