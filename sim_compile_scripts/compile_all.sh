#!/bin/bash

# If any of the compilation failed, exit.
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

echo "==================================="
echo " Start Building All Configurations."
echo "==================================="

echo -e "\n[1/11]"
bash "$SCRIPT_DIR/compile_ampm.sh"

echo -e "\n[2/11]"
bash "$SCRIPT_DIR/compile_berti.sh"

echo -e "\n[3/11]"
bash "$SCRIPT_DIR/compile_berti_l2c_prefetch_only.sh"

echo -e "\n[4/11]"
bash "$SCRIPT_DIR/compile_berti_spp_ppf.sh"

echo -e "\n[5/11]"
bash "$SCRIPT_DIR/compile_ip_stride.sh"

echo -e "\n[6/11]"
bash "$SCRIPT_DIR/compile_ipcp.sh"

echo -e "\n[7/11]"
bash "$SCRIPT_DIR/compile_r_max_l1d.sh"

echo -e "\n[8/11]"
bash "$SCRIPT_DIR/compile_r_max_l1d_l2c.sh"

echo -e "\n[9/11]"
bash "$SCRIPT_DIR/compile_r_max_l2c.sh"

echo -e "\n[10/11]"
bash "$SCRIPT_DIR/compile_r_max_llc.sh"

echo -e "\n[11/11]"
bash "$SCRIPT_DIR/compile_spp.sh"


echo "==================================="
echo "     All Configurations built."
echo "==================================="
