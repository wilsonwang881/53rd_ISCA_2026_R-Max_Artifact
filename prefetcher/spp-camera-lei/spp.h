#ifndef SPP_H
#define SPP_H

#include "signaturetable.h"
#include "bootstraptable.h"
#include "patterntable.h"
#include "filter.h"
#include "page_bitmap.h" // WL

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <vector>
#include <algorithm> // WL: fixing error while compiling related to any_of

#include "champsim_constants.h"
#include "msl/bits.h"

// WL
#include <iostream>
#include <fstream>
#include <set>
#include <bitset>
// WL 

class CACHE;

namespace spp {
  class prefetcher
  {
    constexpr static std::size_t ACCURACY_BITS = 7;
    constexpr static std::size_t ISSUE_QUEUE_SIZE = 32;

    SIGNATURE_TABLE signature_table;
    BOOTSTRAP_TABLE bootstrap_table;
    PATTERN_TABLE pattern_table;
    SPP_PREFETCH_FILTER filter;
    std::deque<std::pair<uint64_t, bool>> issue_queue;

    struct pfqueue_entry_t
    {
      uint32_t sig = 0;
      int32_t  delta = 0;
      uint32_t depth = 0;
      int confidence = 0;
      uint64_t address = 0;
    };

    std::optional<pfqueue_entry_t> active_lookahead;

    // STATS
    std::unordered_map<uint32_t, unsigned> sig_count_tracker;
    std::vector<unsigned> depth_confidence_tracker;
    std::vector<unsigned> depth_interrupt_tracker;
    std::vector<unsigned> depth_offpage_tracker;
    std::vector<unsigned> depth_ptmiss_tracker;

    public:

    std::map<uint64_t, std::deque<std::tuple<uint64_t, bool, int8_t>>> available_prefetches;    
    std::deque<std::tuple<uint64_t, bool, int8_t>> context_switch_issue_queue;
    constexpr static bool PB_ENABLED = false; // WL
    SPP_PAGE_BITMAP page_bitmap; // WL
    uint64_t curr_addr; // WL

    bool warmup = true;
    
    void update_demand(uint64_t base_addr, uint32_t set);
    void issue(CACHE* cache);
    void step_lookahead();
    void initiate_lookahead(uint64_t base_addr);
    void print_stats(std::ostream& ostr);
    
    // WL 
    bool context_switch_prefetch_gathered = false;
    std::ofstream prefetcher_state_file;

    struct PF {
      uint64_t addr;
      uint64_t cycle;
    };

    struct ACC {
      uint64_t addr;
      uint64_t cycle;
      bool hit_or_miss;
      uint64_t type;
    };

    std::string PF_ADDR_FILE_NAME = "pf_acc.txt";
    std::fstream pf_acc_file;
    std::string CACHE_ACC_FILE_NAME = "cache_acc.txt";
    std::fstream cache_acc_file;
    std::deque<struct PF> pf_acc;
    std::deque<struct ACC> cache_acc;

    uint64_t PF_ACC_THRESHOLD_LENGTH = 100000;

    uint64_t cache_cycle;

    void clear_states();
    void context_switch_gather_prefetches(CACHE* cache);
    std::optional<uint64_t> context_switch_aux(uint32_t &sig, int32_t delta, float &confidence, uint64_t page_num, uint32_t &last_offset);
    void record_spp_states();
    float CUTOFF_THRESHOLD = 0.1;
    uint64_t gathered_pf_this_round;
    uint64_t issued_pf_this_round;
    uint64_t hit_pf_this_round;
    // WL 
  };
} // namespace spp

#endif

