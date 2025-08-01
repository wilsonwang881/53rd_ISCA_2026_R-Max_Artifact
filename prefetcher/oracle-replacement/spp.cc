#include "spp.h"
#include "cache.h"

#include <array>
#include <iostream>
#include <map>
#include <numeric>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp_l3::prefetcher> SPP_L3;
}

void spp_l3::prefetcher::update_do_not_fill_queue(std::deque<uint64_t> &dq, uint64_t addr, bool erase, CACHE* cache, std::string q_name){
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));
  auto search_res = std::find_if(dq.begin(), dq.end(), 
                    [match = addr >> (cache->OFFSET_BITS), shamt = cache->OFFSET_BITS]
                    (const auto& entry) {
                      return (entry >> shamt) == match; 
                    });

  if (erase) {
    if (search_res != dq.end()) {
      dq.erase(search_res);

      if (debug_print) 
        std::cout << "Addr " << addr << " in set " << set << " erased from " << q_name << std::endl; 
    }
  }
  else {
    if (search_res == dq.end()) {
      dq.push_back(addr); 

      if (debug_print) 
        std::cout << "Addr " << addr << " in set " << set << " pushed to " << q_name << std::endl; 
    }
  }
}

spp_l3::SPP_ORACLE::acc_timestamp spp_l3::prefetcher::rollback(uint64_t addr, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, CACHE* cache) {
  // Find the target to replace.
  uint64_t rollback_cache_state_index = oracle.rollback_prefetch(addr); 
  assert(search->miss_or_hit > 1);

  SPP_ORACLE::acc_timestamp rollback_pf;
  rollback_pf.cycle_demanded = oracle.cache_state[rollback_cache_state_index].timestamp;
  rollback_pf.set = oracle.calc_set(addr);
  rollback_pf.addr = oracle.cache_state[rollback_cache_state_index].addr;
  rollback_pf.miss_or_hit = oracle.cache_state[rollback_cache_state_index].pending_accesses;
  rollback_pf.type = oracle.cache_state[rollback_cache_state_index].type;
  rollback_pf.reuse_dist_lst_timestmp = oracle.cache_state[rollback_cache_state_index].last_access_timestamp;

  // Update cache_state.
  oracle.cache_state[rollback_cache_state_index].addr = addr;
  oracle.cache_state[rollback_cache_state_index].pending_accesses = search->miss_or_hit;
  oracle.cache_state[rollback_cache_state_index].timestamp = search->cycle_demanded;
  oracle.cache_state[rollback_cache_state_index].type = search->type;
  oracle.cache_state[rollback_cache_state_index].accessed = true;
  oracle.cache_state[rollback_cache_state_index].last_access_timestamp = search->reuse_dist_lst_timestmp - search->cycle_demanded + cache->current_cycle;

  if (debug_print) 
    std::cout << "Replaced prefetch in set " << oracle.calc_set(addr) << " addr " << rollback_pf.addr << " counter " << rollback_pf.miss_or_hit << std::endl;

  return rollback_pf;
}

void spp_l3::prefetcher::update_MSHR_inflight_write_rollback(CACHE* cache, SPP_ORACLE::acc_timestamp rollback_pf) {
  // If the rollback prefetch is in MSHR, push to do not fill.
  auto search_mshr_rollback = std::find_if(std::begin(cache->MSHR), std::end(cache->MSHR),
                              [match = rollback_pf.addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                                return (entry.address >> shamt) == match; 
                              });

  if (search_mshr_rollback != cache->MSHR.end()) 
    update_do_not_fill_queue(cache->do_not_fill_address, 
                             rollback_pf.addr, 
                             false, 
                             cache, 
                             " do_not_fill_address rollback_pf in MSHR");

  // If the rollback prefetch is in inflight_writes, push to do not fill.
  auto search_writes_rollback = std::find_if(std::begin(cache->inflight_writes), std::end(cache->inflight_writes),
                                [match = rollback_pf.addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                                  return (entry.address >> shamt) == match; 
                                });

  if (search_writes_rollback != cache->inflight_writes.end()) 
    update_do_not_fill_queue(cache->do_not_fill_write_address,
                             rollback_pf.addr, 
                             false, 
                             cache, 
                             "do_not_fill_write_address rollback_pf in inflight_writes");

  // If the rollback prefetch is already in cache, set LRU to 0.
  uint64_t rollback_set = oracle.calc_set(rollback_pf.addr);
  uint64_t rollback_way = cache->get_way(rollback_pf.addr, rollback_set);

  if (rollback_way < cache->NUM_WAY) 
    champsim::operable::lru_states.push_back(std::make_tuple(rollback_set, rollback_way, 0));

  // Update metric
  oracle.unhandled_misses_replaced++;
}

void spp_l3::prefetcher::place_rollback(CACHE* cache, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, uint64_t set, uint64_t way) {
  uint64_t i = set * cache->NUM_WAY + way;

  assert(oracle.cache_state[i].addr == 0 || 
         oracle.cache_state[i].addr == search->addr);

  if (oracle.cache_state[i].addr == 0) 
    oracle.set_availability[set]--;

  oracle.cache_state[i].pending_accesses += search->miss_or_hit;
  oracle.cache_state[i].addr = search->addr;
  oracle.cache_state[i].timestamp = search->cycle_demanded;
  oracle.cache_state[i].type = search->type;
  oracle.cache_state[i].last_access_timestamp = search->reuse_dist_lst_timestmp - search->cycle_demanded + cache->current_cycle;
  oracle.cache_state[i].accessed = true;
  assert(oracle.set_availability[set] >= 0);
}

