#include "cache.h"
#include "spp.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

void CACHE::prefetcher_initialize() {
  std::cout << std::endl;
  std::cout << "Signature Path Prefetcher" << std::endl;
  std::cout << "Signature table" << " sets: " << spp::SIGNATURE_TABLE::SET << " ways: " << spp::SIGNATURE_TABLE::WAY << std::endl;
  std::cout << "Pattern table" << " sets: " << spp::PATTERN_TABLE::SET << " ways: " << spp::PATTERN_TABLE::WAY << std::endl;
  std::cout << "Prefetch filter" << " sets: " << spp::SPP_PREFETCH_FILTER::SET << " ways: " << spp::SPP_PREFETCH_FILTER::WAY << std::endl;
  std::cout << std::endl;

  auto &pref = ::SPP[{this, cpu}];
  pref.prefetcher_state_file.open("prefetcher_states.txt", std::ios::out);

  // Clear the file that records issued prefetches.
  pref.pf_acc_file.open(pref.PF_ADDR_FILE_NAME, std::ios::out | std::ios::trunc);
  pref.pf_acc_file.close();

  pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ios::out | std::ios::trunc);
  pref.cache_acc_file.close();
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::SPP[{this, cpu}];

  // WL: for recording issued SPP prefetches 
  bool search_MSHR = std::find_if(std::begin(MSHR), std::end(MSHR),
         [match = base_addr >> OFFSET_BITS, shamt = OFFSET_BITS]
         (const auto& entry) {
           return (entry.address >> shamt) == match && entry.type == access_type::PREFETCH; 
         }) != MSHR.end();

  uint64_t hit_miss_late = 0;

  if (useful_prefetch) 
    hit_miss_late = 1; 
  else if (search_MSHR) 
    hit_miss_late = 2; 
  else 
    hit_miss_late = 0;

  struct spp::prefetcher::ACC acc{base_addr, current_cycle, hit_miss_late, type};
  pref.cache_acc.push_back(acc);

  if (pref.cache_acc.size() > pref.PF_ACC_THRESHOLD_LENGTH) {
    pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : pref.cache_acc) 
      pref.cache_acc_file << var.cycle << " " << var.addr << " " << var.hit_or_miss_or_late << " " << var.type << std::endl;

    pref.cache_acc.clear();
    pref.cache_acc_file.close();
  }
  // WL

  pref.update_demand(base_addr,this->get_set_index(base_addr));
  pref.initiate_lookahead(base_addr);

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  auto &pref = ::SPP[{this, cpu}];
  //pref.warmup = warmup; 

  //pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?
  pref.issue(this);
  pref.step_lookahead();
}

void CACHE::prefetcher_final_stats() {
  std::cout << "SPP STATISTICS" << std::endl << std::endl;
  auto &pref = ::SPP[{this, cpu}];
  pref.print_stats(std::cout);

  // WL
  pref.pf_acc_file.open(pref.PF_ADDR_FILE_NAME, std::ofstream::app);

  for(auto var : pref.pf_acc) 
    pref.pf_acc_file << var.cycle << " " << var.addr << std::endl;

  pref.pf_acc_file.close();

  pref.cache_acc_file.open(pref.CACHE_ACC_FILE_NAME, std::ofstream::app);

  for(auto var : pref.cache_acc) 
    pref.cache_acc_file << var.cycle << " " << var.addr << " " << var.hit_or_miss_or_late << " " << var.type << std::endl;

  pref.cache_acc.clear();
  pref.cache_acc_file.close();
  // WL
}

