#ifndef SPP_H
#define SPP_H

#include "oracle.h" // WL 

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <vector>
#include <algorithm> 
#include "champsim_constants.h"
#include "msl/bits.h"

// WL
#include <iostream>
#include <fstream>
#include <set>
#include <bitset>
#include <cassert>
// WL 

class CACHE;

namespace spp_l3 {
  class prefetcher
  {
    public:

    SPP_ORACLE oracle;
    bool debug_print = false;
    uint64_t last_handled_addr;
    uint64_t cache_cycle;

    void update_do_not_fill_queue(std::deque<uint64_t> &dq, uint64_t addr, bool erase, CACHE* cache, std::string q_name);
    spp_l3::SPP_ORACLE::acc_timestamp rollback(uint64_t addr, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, CACHE* cache);
    void update_MSHR_inflight_write_rollback(CACHE* cache, SPP_ORACLE::acc_timestamp rollback_pf);
    void place_rollback(CACHE* cache, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, uint64_t set, uint64_t way);
  };
} // namespace spp_l3

#endif

