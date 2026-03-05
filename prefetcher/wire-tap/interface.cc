#include "cache.h"
#include "wire-tap.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, wire_tap::prefetcher> WIRE_TAP;
}

void CACHE::prefetcher_initialize() {
  std::cout << "Wire-Tap initialized at " << this->NAME << std::endl;

  auto &pref = ::WIRE_TAP[{this, cpu}];

  pref.pf_acc_file.open(pref.PF_ADDR_FILE_NAME, std::ios::out | std::ios::trunc);
  pref.pf_acc_file.close();

  pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ios::out | std::ios::trunc);
  pref.cache_acc_file.close();
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::WIRE_TAP[{this, cpu}];

  if (access_type{type} == access_type::PREFETCH && !cache_hit) {
    struct wire_tap::prefetcher::PF pf{addr, current_cycle};
    pref.pf_acc.push_back(pf);

    if (pref.pf_acc.size() > pref.PF_ACC_THRESHOLD_LENGTH) {
      pref.pf_acc_file.open(pref.PF_ADDR_FILE_NAME, std::ofstream::app);

      for(auto var : pref.pf_acc) 
        pref.pf_acc_file << var.cycle << " " << var.addr << std::endl;

      pref.pf_acc.clear();
      pref.pf_acc_file.close();
    }
  }
  else {
    bool search_MSHR = std::find_if(std::begin(MSHR), std::end(MSHR),
           [match = addr >> OFFSET_BITS, shamt = OFFSET_BITS]
           (const auto& entry) {
             return (entry.address >> shamt) == match && entry.type == access_type::PREFETCH; 
           }) != MSHR.end();

    struct wire_tap::prefetcher::ACC acc{addr, current_cycle, useful_prefetch ? 1 : search_MSHR, type};
    pref.cache_acc.push_back(acc);

    if (pref.cache_acc.size() > pref.PF_ACC_THRESHOLD_LENGTH) {
      pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ofstream::app);

      for(auto var : pref.cache_acc) 
        pref.cache_acc_file << var.cycle << " " << var.addr << " " << var.hit_or_miss << " " << var.type << std::endl;

      pref.cache_acc.clear();
      pref.cache_acc_file.close();
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
  auto &pref = ::WIRE_TAP[{this, cpu}];

  pref.pf_acc_file.open(pref.PF_ADDR_FILE_NAME, std::ofstream::app);

  for(auto var : pref.pf_acc) 
    pref.pf_acc_file << var.cycle << " " << var.addr << std::endl;

  pref.pf_acc_file.close();

  pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ofstream::app);

  for(auto var : pref.cache_acc) 
    pref.cache_acc_file << var.cycle << " " << var.addr << " " << var.hit_or_miss << " " << var.type << std::endl;

  pref.cache_acc.clear();
  pref.cache_acc_file.close();
}
