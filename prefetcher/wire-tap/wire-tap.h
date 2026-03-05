#ifndef WIRE_TAP_H 
#define WIRE_TAP_H

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

namespace wire_tap {
  class prefetcher
  {
    public:

    struct PF {
      uint64_t addr;
      uint64_t cycle;
      uint64_t level;
    };

    struct ACC {
      uint64_t addr;
      uint64_t cycle;
      bool hit_or_miss;
      uint64_t type;
    };

    std::string PF_ADDR_FILE_NAME = "pf_2nd_timing.txt";
    std::fstream pf_acc_file;
    std::string CACHE_ACC_FILE_NAME = "cache_acc_2nd.txt";
    std::fstream cache_acc_file;
    std::deque<struct PF> pf_acc;
    std::deque<struct ACC> cache_acc;
    uint64_t PF_ACC_THRESHOLD_LENGTH = 100000;
  };
} 

#endif

