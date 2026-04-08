#!/bin/bash

echo "Copying page translations and issued prefetches".

echo "Working on berti_l1d to berti_max_l1d."
cp -r berti_l1d/results/* berti_max_l1d/results/

echo "Working on berti_l1d to berti_max_l2c."
cp -r berti_l1d/results/* berti_max_l2c/results/

echo "Working on spp_l2c to l1d_min."
cp -r spp_l2c/results/* l1d_min/results/

echo "Working on spp_l2c to l2c_min."
cp -r spp_l2c/results/* l2c_min/results/

echo "Working on spp_l2c to llc_min."
cp -r spp_l2c/results/* llc_min/results/

echo "Working on spp_l2c to r_max_l1d."
cp -r spp_l2c/results/* r_max_l1d/results/

echo "Working on spp_l2c to r_max_l1d_l2c."
cp -r spp_l2c/results/* r_max_l1d_l2c/results/

echo "Working on spp_l2c to r_max_l2c."
cp -r spp_l2c/results/* r_max_l2c/results/

echo "Working on spp_l2c to r_max_llc."
cp -r spp_l2c/results/* r_max_llc/results/

echo "Working on spp_l2c to spp_max_l2c."
cp -r spp_l2c/results/* spp_max_l2c/results/

echo "File transfer complete!"
