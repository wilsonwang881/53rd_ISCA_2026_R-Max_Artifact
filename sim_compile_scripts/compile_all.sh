#!/bin/bash

# If any of the compilation failed, exit.
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

echo "==================================="
echo " Start Building All Configurations."
echo "==================================="

echo -e "\n[1/24]"
bash "$SCRIPT_DIR/compile_always_hit_l1d.sh"

echo -e "\n[2/24]"
bash "$SCRIPT_DIR/compile_always_hit_l2c.sh"

echo -e "\n[3/24]"
bash "$SCRIPT_DIR/compile_always_hit_llc.sh"

echo -e "\n[4/24]"
bash "$SCRIPT_DIR/compile_ampm.sh"

echo -e "\n[5/24]"
bash "$SCRIPT_DIR/compile_berti.sh"

echo -e "\n[6/24]"
bash "$SCRIPT_DIR/compile_berti_cache_size_x100.sh"

echo -e "\n[7/24]"
bash "$SCRIPT_DIR/compile_berti_l2c_prefetch_only.sh"

echo -e "\n[8/24]"
bash "$SCRIPT_DIR/compile_berti_mshr_x100.sh"

echo -e "\n[9/24]"
bash "$SCRIPT_DIR/compile_berti_max_l1d.sh"

echo -e "\n[10/24]"
bash "$SCRIPT_DIR/compile_berti_max_l2c.sh"

echo -e "\n[11/24]"
bash "$SCRIPT_DIR/compile_berti_spp_ppf.sh"

echo -e "\n[12/24]"
bash "$SCRIPT_DIR/compile_ip_stride.sh"

echo -e "\n[13/24]"
bash "$SCRIPT_DIR/compile_ipcp.sh"

echo -e "\n[14/24]"
bash "$SCRIPT_DIR/compile_l1d_min.sh"

echo -e "\n[15/24]"
bash "$SCRIPT_DIR/compile_l2c_min.sh"

echo -e "\n[16/24]"
bash "$SCRIPT_DIR/compile_llc_min.sh"

echo -e "\n[17/24]"
bash "$SCRIPT_DIR/compile_r_max_l1d.sh"

echo -e "\n[18/24]"
bash "$SCRIPT_DIR/compile_r_max_l1d_l2c.sh"

echo -e "\n[19/24]"
bash "$SCRIPT_DIR/compile_r_max_l2c.sh"

echo -e "\n[20/24]"
bash "$SCRIPT_DIR/compile_r_max_llc.sh"

echo -e "\n[21/24]"
bash "$SCRIPT_DIR/compile_spp.sh"

echo -e "\n[22/24]"
bash "$SCRIPT_DIR/compile_spp_cache_size_x100.sh"

echo -e "\n[23/24]"
bash "$SCRIPT_DIR/compile_spp_max.sh"

echo -e "\n[24/24]"
bash "$SCRIPT_DIR/compile_spp_mshr_x100.sh"


echo "==================================="
echo "     All Configurations built."
echo "==================================="
