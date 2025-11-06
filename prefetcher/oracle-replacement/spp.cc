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
    update_do_not_fill_queue(search_mshr_rollback->type == access_type::WRITE ? cache->do_not_fill_write_address : cache->do_not_fill_address, 
                             rollback_pf.addr, 
                             false, 
                             cache, 
                             search_mshr_rollback->type == access_type::WRITE ? "do_not_fill_write_address rollback_pf in MSHR" : "do_not_fill_address rollback_pf in MSHR");

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

bool spp_l3::prefetcher::check_issued(CACHE* cache, uint64_t addr) {
  uint64_t check_cache = cache->get_way(addr, oracle.calc_set(addr));
  auto search_mshr = std::find_if(std::begin(cache->MSHR), std::end(cache->MSHR),
                     [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS]
                     (const auto& entry) {
                       return (entry.address >> shamt) == match; 
                     });

  return (check_cache < cache->NUM_WAY) || (search_mshr != cache->MSHR.end());
}

bool spp_l3::prefetcher::search_MSHR_inflight_writes(CACHE* cache, Q_TYPE type, uint64_t addr) {
  auto q = (type == Q_TYPE::MSHR) ? cache->MSHR : cache->inflight_writes;

  return std::find_if(std::begin(q), std::end(q),
         [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS]
         (const auto& entry) {
           return (entry.address >> shamt) == match; 
         }) != q.end();
}

bool spp_l3::prefetcher::search_do_not_fill_qs(CACHE* cache, std::deque<uint64_t> q, uint64_t addr) {
  return std::find_if(std::begin(q), std::end(q),
         [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS]
         (const auto& entry) {
           return (entry >> shamt) == match; 
         }) != q.end();
}


void spp_l3::prefetcher::rollback_op(CACHE* cache, uint64_t addr, uint8_t type, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, std::deque<SPP_ORACLE::acc_timestamp> &q, bool bkp_q, std::string debug_info) {
  uint64_t set = oracle.calc_set(addr);

  if (oracle.ROLLBACK_ENABLED){
    // Generate a rollback prefetch.
    spp_l3::SPP_ORACLE::acc_timestamp rollback_pf = rollback(addr, search, cache);

    // Erase the moved ahead prefetch in not ready queue. 
    q.erase(search); 

    if (check_issued(cache, rollback_pf.addr)) 
      // Put back the rollback prefetch to not ready queue.
      oracle.bkp_pf[set].push_back(rollback_pf);
    else {
      oracle.oracle_pf[set].push_front(rollback_pf);
      oracle.oracle_pf_size++;
    }

    if (!bkp_q) 
      oracle.oracle_pf_size--;

    // If the rollback prefetch is in MSHR, push to do not fill.
    update_MSHR_inflight_write_rollback(cache, rollback_pf);

    if (debug_print) 
      std::cout << debug_info << " miss in set " << set << " addr " << addr << " type " << (unsigned)type << " caused a rollback." << std::endl;
  }
  else {
    assert(!oracle.ROLLBACK_ENABLED);
    search->miss_or_hit--;
    update_do_not_fill_queue(type == 3 ? cache->do_not_fill_write_address : cache->do_not_fill_address,
                             addr, 
                             false,
                             cache,
                             type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
  }
}


void spp_l3::prefetcher::cache_db_op(CACHE* cache, uint64_t addr, uint64_t set, uint8_t type, std::deque<SPP_ORACLE::acc_timestamp>::iterator search, std::deque<SPP_ORACLE::acc_timestamp> &q, bool bkp_q, std::string debug_info, bool &found_in_not_ready_queue, bool &found_in_pending_queue) {
  uint64_t way = oracle.check_set_pf_avail(search->addr);
  found_in_not_ready_queue = true;
  found_in_pending_queue = true;

  if (debug_print) {
    std::cout << "Hit in bkp set " << set << " addr " << addr << " type " << (unsigned)type << std::endl;
    std::cout << "Found addr " << search->addr << " set " << search->set << " counter " << search->miss_or_hit << " set_availability " << oracle.set_availability[search->set] << " found way " << way << std::endl;
  }

  if (search->miss_or_hit == 1) {
    // Do not fill the missed address. 
    update_do_not_fill_queue(type == 3 ? cache->do_not_fill_write_address : cache->do_not_fill_address,
                                  addr, 
                                  false,
                                  cache,
                                  type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
    q.erase(search);
    oracle.unhandled_misses_not_replaced++;

    if (!bkp_q) 
      oracle.oracle_pf_size--;
  }
  else if(way < oracle.WAY_NUM) {
    place_rollback(cache, search, set, way);
    q.erase(search); 

    if (!bkp_q) {
      oracle.oracle_pf_size--;
      oracle.oracle_pf_hits++;
    }
  }
  else {
    // Rollback prefetch.
    // Find the counter with the missed address.
    // If the counter is 1, do not replace.
    // If the counter > 1, replace.
    rollback_op(cache, addr, type, search, q, bkp_q, debug_info); 
  }
}
