#!/bin/bash
echo "Building ChampSim with IPCP running in L1D"

# Return to the project root.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR/.." || exit

# Configure ChampSim using the configuration file.
./config.sh ./sim_configs/ipcp_config.json

# Compile.
make -j 8

# Rename the compiled binary
mv bin/champsim bin/ipcp_l1d

echo "IPCP in L1D build complete!"
