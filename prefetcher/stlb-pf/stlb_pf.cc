#include "stlb_pf.h"
#include "cache.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF; 
}

void stlb_pf::prefetcher::update(uint64_t addr) {
  uint64_t page_num = addr >> 12;
  auto el = std::find(translations.begin(), translations.end(), page_num);

  if (el != translations.end()) 
    translations.erase(el);

  translations.push_back(page_num);

  if (translations.size() > DQ_SIZE) 
    translations.pop_front();
}

void stlb_pf::prefetcher::pop_queue(uint64_t addr, std::deque<uint64_t> &q) {
  auto el = std::find(q.begin(), q.end(), addr);

  if (el != q.end())
    q.erase(el);
}

void stlb_pf::prefetcher::gather_pf() {
  cs_q.clear();
  int limit = 0; //translations.size() - std::round(translations.size() * accuracy) * (translations.size() * 1.0 / DQ_SIZE);
  
  for(int i = translations.size() - 1; i >= limit; i--) 
    cs_q.push_back(translations[i] << 12); 

  translations.clear();
}

void stlb_pf::prefetcher::issue(CACHE* cache) {
  //if (cache->current_cycle >= (last_issued_pf_moment + wait_interval) && (cache->get_mshr_occupancy() < 10)) {
    bool pf_res = cache->prefetch_line(cs_q.front(), true, 0); 
    
    if (pf_res) {
      pf_issued++;
      cs_q.pop_front(); 
      last_issued_pf_moment = cache->current_cycle;
    }
  //}
}

void stlb_pf::prefetcher::update_pf_stats() {
  printf("%s hits / accesses = %ld / %ld  = %f\n", "STLB", hits, accesses, 1.0 * hits / accesses);

  if ((pf_hit == 0 && pf_issued == 0) || (pf_issued == pf_issued_last_round))
    accuracy = 1.0 * hits / accesses;  
  else 
    accuracy = (pf_hit  - pf_hit_last_round * 1.0) / (pf_issued - pf_issued_last_round * 1.0) * 1.0;

  printf("STLB PF accuracy = %f\n", accuracy);
  pf_hit_last_round = pf_hit;
  pf_issued_last_round = pf_issued;
  wait_interval = std::round(4000000 * 1.0 / (accesses - accesses_last_round + 1.0));
  accesses_last_round = accesses;
  printf("STLB PF issue wait cycle = %ld\n", wait_interval);
}

