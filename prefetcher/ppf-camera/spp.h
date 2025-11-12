#ifndef SPP_H
#define SPP_H

#include "signaturetable.h"
#include "bootstraptable.h"
#include "patterntable.h"
#include "filter.h"
#include "ppf.h"//HL

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <vector>
#include <algorithm> // Wang Lei: fixing error while compiling related to any_of

#include "champsim_constants.h"
#include "msl/bits.h"

// WL
#include <iostream>
#include <fstream>
#include <set>
#include <bitset>
// WL 
//#include "util.h"

class CACHE;

namespace spp {
  
  //#pragma once

  //PERCEPTRON PERC;//HL
  //SPP_PREFETCH_FILTER filter;
  
  class prefetcher
  {
    constexpr static std::size_t ACCURACY_BITS = 7;
    constexpr static std::size_t ISSUE_QUEUE_SIZE = 32;

    SIGNATURE_TABLE signature_table;
    BOOTSTRAP_TABLE bootstrap_table;
    PATTERN_TABLE pattern_table;
    SPP_PREFETCH_FILTER filter;
    PERCEPTRON PERC;//HL
    std::deque<std::pair<uint64_t, bool>> issue_queue;

    struct pfqueue_entry_t
    {
      uint32_t sig = 0;
      int32_t  delta = 0;
      uint32_t depth = 0;
      int confidence = 0;
      uint64_t address = 0;
      int32_t perc_sum=0;//HL
      uint64_t curr_ip=0;//HL
    };

    std::optional<pfqueue_entry_t> active_lookahead;

    // STATS
    std::unordered_map<uint32_t, unsigned> sig_count_tracker;
    std::vector<unsigned> depth_confidence_tracker;
    std::vector<unsigned> depth_interrupt_tracker;
    std::vector<unsigned> depth_offpage_tracker;
    std::vector<unsigned> depth_ptmiss_tracker;

    public:

    //PERCEPTRON PERC;
    //SPP_PREFETCH_FILTER filter;
    

    bool warmup = true;

    void update_demand(uint64_t base_addr, uint32_t set);
    void issue(CACHE* cache);
    void step_lookahead();
    //void initiate_lookahead(uint64_t base_addr);
    void initiate_lookahead(uint64_t base_addr,uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in);
    void print_stats(std::ostream& ostr);
    void update_perceptron(uint64_t evicted_addr);//HL
  };
} // namespace spp

#endif

