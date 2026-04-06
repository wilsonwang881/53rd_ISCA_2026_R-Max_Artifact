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

enum class Q_TYPE {
  MSHR,
  INFLIGHT_WRITES
};

namespace spp_l2 {
  class prefetcher
  {
    public:

    std::deque<std::tuple<uint64_t, uint64_t, bool, bool>> pending_pf_q; // addr, cycle, pf_to_this_level, RFO/WRITE
    SPP_ORACLE oracle;
    bool debug_print = false;
    uint64_t last_handled_addr;
    uint64_t cache_cycle;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;

    std::string occupancy_info = "occupancy.txt";
    std::fstream occupancy_info_file;

    struct PF {
      uint64_t addr;
      uint64_t cycle;
      uint64_t level;
    };

    std::string PF_ADDR_FILE_NAME = "oracle_pf_timing_2nd.txt";
    std::fstream pf_acc_file;
    std::deque<struct PF> pf_acc;
    uint64_t PF_ACC_THRESHOLD_LENGTH = 100000;

    uint64_t issue(CACHE* cache);
    void call_poll(CACHE* cache);
    void erase_duplicate_entry_in_ready_queue(CACHE* cache, uint64_t addr);
    void update_do_not_fill_queue(std::deque<uint64_t> &dq, uint64_t addr, bool erase, CACHE* cache, std::string q_name);
    spp_l2::SPP_ORACLE::acc_timestamp rollback(uint64_t addr, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, CACHE* cache);
    void update_MSHR_inflight_write_rollback(CACHE* cache, SPP_ORACLE::acc_timestamp rollback_pf);
    void place_rollback(CACHE* cache, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, uint64_t set, uint64_t way);
    bool check_issued(CACHE* cache, uint64_t addr);
    bool search_MSHR_inflight_writes(CACHE* cache, Q_TYPE type, uint64_t addr);
    bool search_do_not_fill_qs(CACHE* cache, std::deque<uint64_t> q, uint64_t addr);
    void rollback_op(CACHE* cache, uint64_t addr, uint8_t type, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, std::deque<SPP_ORACLE::acc_timestamp> &q, bool bkp_q, std::string debug_info);
    void cache_db_op(CACHE* cache, uint64_t addr, uint64_t set, uint8_t type, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, std::deque<SPP_ORACLE::acc_timestamp> &q, bool bkp_q, std::string debug_info, bool &found_in_not_ready_queue, bool &found_in_pending_queue);
  };
} // namespace spp_l2

#endif

