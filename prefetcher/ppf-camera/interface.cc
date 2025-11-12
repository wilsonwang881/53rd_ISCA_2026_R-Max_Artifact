#include "cache.h"
#include "spp.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

void CACHE::prefetcher_initialize()
{
  std::cout << std::endl;
  std::cout << "Signature Path Prefetcher" << std::endl;
  std::cout << "Signature table" << " sets: " << spp::SIGNATURE_TABLE::SET << " ways: " << spp::SIGNATURE_TABLE::WAY << std::endl;
  std::cout << "Pattern table" << " sets: " << spp::PATTERN_TABLE::SET << " ways: " << spp::PATTERN_TABLE::WAY << std::endl;
  std::cout << "Prefetch filter" << " sets: " << spp::SPP_PREFETCH_FILTER::SET << " ways: " << spp::SPP_PREFETCH_FILTER::WAY << std::endl;
  std::cout <<" The PPF has feature "<<spp::PERC_FEATURES<<std::endl;
  std::cout <<" The PPF has PERC depth "<<spp::PERCEPTRON::PERC_DEPTH[8]<<std::endl;
  std::cout << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];

  pref.update_demand(base_addr, this->get_set_index(base_addr));
  pref.initiate_lookahead(base_addr,ip,cache_hit, useful_prefetch,type,metadata_in);

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];
  //pref.PERC.update_perc(evicted_addr, 0, 0,0,0,0,spp::L2C_EVICT, 0, 0, 0, 0, 0, 0);//HL
  pref.update_perceptron(evicted_addr);
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::SPP[{this, cpu}];
  // pref.warmup = warmup; 
  //pref.warmup = warmup_complete[cpu];//HL exclude
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?
  pref.issue(this);
  pref.step_lookahead();
}

void CACHE::prefetcher_final_stats()
{
  std::cout << "SPP STATISTICS" << std::endl;
  std::cout << std::endl;

  ::SPP[{this, cpu}].print_stats(std::cout);
}

