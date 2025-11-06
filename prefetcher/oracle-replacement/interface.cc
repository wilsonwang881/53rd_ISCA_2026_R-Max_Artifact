#include "cache.h"
#include "spp.h"

#include <algorithm>
#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp_l3::prefetcher> SPP_L3;
}

void CACHE::prefetcher_initialize() {
  std::cout << "Oracle replacement at " << this->NAME << std::endl;

  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE) 
    pref.oracle.init();
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  if (type == 2) 
    return metadata_in; 

  auto &pref = ::SPP_L3[{this, cpu}];
  base_addr = (base_addr>> 6) << 6;

  // Return if a cache request misses and cannot merge in MSHR and MSHR is full.
  // Exclude WRITE misses for L2 and LLC.
  // Include WRITE misses for L1D.
  bool initiate_MSHR_search = pref.oracle.ORACLE_ACTIVE && 
                              !cache_hit &&
                              ((type != 3 && NAME.compare(champsim::operable::L1D_name)) ||
                              (!NAME.compare(champsim::operable::L1D_name)));

  // Return if a demand misses and cannot merge in MSHR and MSHR is full.
  if (initiate_MSHR_search) {
    bool search_mshr = pref.search_MSHR_inflight_writes(this, Q_TYPE::MSHR, base_addr); 

    if (!search_mshr && this->get_mshr_occupancy() == this->get_mshr_size()) 
      return metadata_in; 
  }

  uint64_t set = this->get_set_index(base_addr);

  if (pref.debug_print) 
    std::cout << "Hit/miss " << (unsigned)cache_hit << " set " << set << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << " MSHR usage " << this->get_mshr_occupancy() << std::endl;

  bool found_in_MSHR = false;
  bool found_in_inflight_writes = false;
  bool found_in_ready_queue = false;
  bool found_in_not_ready_queue = false;
  bool found_in_pending_queue = false;
  bool not_found_hit = false;

  if (pref.oracle.ORACLE_ACTIVE && !cache_hit) {
    bool search_mshr = pref.search_MSHR_inflight_writes(this, Q_TYPE::MSHR, base_addr); 
    bool search_inflight_writes = pref.search_MSHR_inflight_writes(this, Q_TYPE::INFLIGHT_WRITES, base_addr);

    if (search_mshr || search_inflight_writes) {
      found_in_pending_queue = true; 
      found_in_MSHR = search_mshr;
      pref.oracle.MSHR_hits += search_mshr; 
      found_in_inflight_writes = search_inflight_writes;
      pref.oracle.inflight_write_hits += search_inflight_writes;

      // The miss can be found in MSHR.
      // The MSHR is opened by a previous operation like prefetching.
      // But the previous operation gets rolled back.
      // So the cache structure in R-Max is not aware of it.
      // The miss may exist in do not fill queues.
      // Need to perform a rollback to update the cache structure in R-Max.
      // May need to remove the entry in the do not fill queues to let it fill.
      if (search_mshr && pref.oracle.check_pf_status(base_addr) < 0) {
        if (pref.debug_print) 
         std::cout << "Hit in " << (search_mshr ? "MSHR" : "") << (search_inflight_writes ? "inflight_writes" : "") << " set " << set << " addr " << base_addr << " type " << (unsigned)type << std::endl;

        bool search_do_not_fill_q = pref.search_do_not_fill_qs(this, this->do_not_fill_address, base_addr);
        bool search_do_not_fill_wq = pref.search_do_not_fill_qs(this, this->do_not_fill_write_address, base_addr);

        // If address found in do not fill write queue, now a non-write miss, move to do not fill queue.
        if (search_do_not_fill_wq && type != 3) {
          pref.update_do_not_fill_queue(this->do_not_fill_write_address, base_addr, true, this, "do_not_fill_write_address");    
          pref.update_do_not_fill_queue(this->do_not_fill_address, base_addr, false, this, "do_not_fill_address");    
        }
        // Else if address found in do not fill queue, now a write miss, move to do not fill write queue.
        else if (search_do_not_fill_q && type == 3) {
          pref.update_do_not_fill_queue(this->do_not_fill_address, base_addr, true, this, "do_not_fill_address");    
          pref.update_do_not_fill_queue(this->do_not_fill_write_address, base_addr, false, this, "do_not_fill_write_address");    
        }
      }
    } 
    else {
      {
        auto search_bkp_q = std::find_if(std::begin(pref.oracle.bkp_pf[set]), std::end(pref.oracle.bkp_pf[set]),
                            [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                              return (entry.addr >> shamt) == match; 
                            });

        if (search_bkp_q != pref.oracle.bkp_pf[set].end()) 
          pref.cache_db_op(this, base_addr, set, type, search_bkp_q, pref.oracle.bkp_pf[set], true, " 2 ", found_in_not_ready_queue, found_in_pending_queue);
        else {
          auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf[set]), std::end(pref.oracle.oracle_pf[set]),
                                  [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.addr >> shamt) == match; 
                                  });

          if (search_oracle_pq != pref.oracle.oracle_pf[set].end()) 
            pref.cache_db_op(this, base_addr, set, type, search_oracle_pq, pref.oracle.oracle_pf[set], false, " 1 ", found_in_not_ready_queue, found_in_pending_queue);
          else {
            if (pref.oracle.oracle_pf_size != 0) {
              pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                            base_addr, 
                                            false,
                                            this,
                                            type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");

              if (type != 3) 
                pref.oracle.unhandled_non_write_misses_not_filled++;
              else 
                pref.oracle.unhandled_write_misses_not_filled++;
            }

            if (pref.debug_print) 
               std::cout << "Unhandled miss set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << " not found in oracle_pf" << std::endl;
          }
        }
      }
    }

    if (found_in_pending_queue) {
      if (pref.debug_print) 
        std::cout << "Hit in pending queues ? " << (unsigned)cache_hit << " set " << set << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;

      useful_prefetch = true; 
      cache_hit = true;
      pref.oracle.runahead_hits++;
    }
  }
  else if (pref.oracle.check_pf_status(base_addr) < 0 && cache_hit) {
    auto search_bkp_q = std::find_if(std::begin(pref.oracle.bkp_pf[set]), std::end(pref.oracle.bkp_pf[set]),
                        [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                          return (entry.addr >> shamt) == match; 
                        });

    if (search_bkp_q != pref.oracle.bkp_pf[set].end()) 
      pref.cache_db_op(this, base_addr, set, type, search_bkp_q, pref.oracle.bkp_pf[set], true, " 3 ", found_in_not_ready_queue, found_in_pending_queue);
    else {
      auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf[set]), std::end(pref.oracle.oracle_pf[set]),
                              [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                return (entry.addr >> shamt) == match; 
                              });

      if (search_oracle_pq != pref.oracle.oracle_pf[set].end()) 
        pref.cache_db_op(this, base_addr, set, type, search_oracle_pq, pref.oracle.oracle_pf[set], false, " 4 ", found_in_not_ready_queue, found_in_pending_queue);
      else {
        uint64_t lru_zero_set = set;
        uint64_t lru_zero_way = this->get_way(base_addr, lru_zero_set);
        champsim::operable::lru_states.push_back(std::make_tuple(lru_zero_set, lru_zero_way, 0));
        not_found_hit = true;
      }
    }
  }
  else 
    assert(cache_hit);

  pref.oracle.update_demand(this->current_cycle, base_addr, useful_prefetch ? 0 : cache_hit, type);

  uint64_t way = this->get_way(base_addr, set);

  if (not_found_hit) 
    cache_hit = false; 

  if (pref.oracle.ORACLE_ACTIVE && cache_hit) {
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if (remaining_acc == 0) {   
      int updated_remaining_acc = pref.oracle.check_pf_status(base_addr);

      if (updated_remaining_acc == -1) {
        if (found_in_MSHR || found_in_ready_queue || found_in_not_ready_queue || found_in_inflight_writes) {
          pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address :this->do_not_fill_address ,
                                        base_addr, 
                                        false,
                                        this,
                                        "do_not_fill_write_address");

          if (found_in_inflight_writes) 
            pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                          base_addr, 
                                          false,
                                          this,
                                          "do_not_fill_write_address");
        } 

        if (way < NUM_WAY) {
          if (pref.debug_print) 
            std::cout << "set " << set << " addr " << base_addr << " cleared LRU bits" << std::endl; 

          champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
        }
      }
      else 
        pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                      base_addr, 
                                      true,
                                      this,
                                      type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
    }
    else if (remaining_acc > 0 && way < NUM_WAY) 
      champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  }
  
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];
  addr = (addr >> 6) << 6;

  int filled_addr_counter = pref.oracle.check_pf_status(addr);

  if (pref.oracle.ORACLE_ACTIVE && (pref.oracle.oracle_pf.size() != 0) && filled_addr_counter < 0) 
    pref.oracle.error_c++; 

  if (pref.debug_print) {
    int evicted_addr_counter = pref.oracle.check_pf_status(evicted_addr);
    std::cout << "Filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << " remaining access " << pref.oracle.check_pf_status(addr) << ((evicted_addr_counter < 0 ) ? " ERROR" : " OK") << " evicted_addr counter " << evicted_addr_counter << std::endl;
  } 

  if (pref.oracle.ORACLE_ACTIVE && pref.oracle.check_pf_status(addr) > 0) 
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  else {
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));

    if (pref.debug_print) 
      std::cout << "set " << this->get_set_index(addr) << " addr " << addr << " cleared LRU bits in cache fill" << std::endl; 
  }

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
  auto &pref = ::SPP_L3[{this, cpu}];
  std::cout << "Oracle STATISTICS" << std::endl;

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.finish();
}

