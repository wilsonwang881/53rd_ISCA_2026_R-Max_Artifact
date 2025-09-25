#include "cache.h"
#include "spp.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

void CACHE::prefetcher_initialize() {
  std::cout << std::endl;
  std::cout << "Signature Path Prefetcher SPP-Camera <LEI>" << std::endl;
  std::cout << "Signature table" << " sets: " << spp::SIGNATURE_TABLE::SET << " ways: " << spp::SIGNATURE_TABLE::WAY << std::endl;
  std::cout << "Pattern table" << " sets: " << spp::PATTERN_TABLE::SET << " ways: " << spp::PATTERN_TABLE::WAY << std::endl;
  std::cout << "Prefetch filter" << " sets: " << spp::SPP_PREFETCH_FILTER::SET << " ways: " << spp::SPP_PREFETCH_FILTER::WAY << std::endl;
  std::cout << std::endl;

  auto &pref = ::SPP[{this, cpu}];
  pref.prefetcher_state_file.open("prefetcher_states.txt", std::ios::out);
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::SPP[{this, cpu}];

  pref.update_demand(base_addr,this->get_set_index(base_addr));
  pref.initiate_lookahead(base_addr);

  if (access_type{type} != access_type::PREFETCH) 
    pref.page_bitmap.update(base_addr);

  if (useful_prefetch) 
    pref.page_bitmap.update_usefulness(base_addr);

  if (access_type{type} == access_type::LOAD) {
    pref.curr_addr = (base_addr >> 6) << 6;
    //uint64_t page_addr = base_addr >> 12;
    //std::tuple<uint64_t, bool,int8_t>demand_itself = std::make_tuple(((base_addr >> 6) << 6), true, 0);
    //pref.available_prefetches[curr_pg].erase(demand_itself);
    //int8_t group = 0;

    /*
    //find the matching adr in the avaialble prefetches
    for (auto var:pref.available_prefetches) {
      if(std::get<0>(var)==std::get<0>(demand_itself))
        group=std::get<2>(var);
    }
    
    for(auto var : pref.available_prefetches) {
      if ((std::get<0>(var) >> 12) == page_addr) {
        if(std::get<2>(var) == group && group != 0)
          pref.context_switch_issue_queue.push_back(var);
        else if((base_addr >> 10)==((std::get<0>(var)) >> 10))
          pref.context_switch_issue_queue.push_back(var);
      }
    }
    
    for(auto var : pref.context_switch_issue_queue) 
      pref.available_prefetches.erase(var); 
    */
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  auto &pref = ::SPP[{this, cpu}];
  //uint32_t blk_asid_match = (metadata_in >> 2) & 0x1;

  if (addr != 0 && addr >= 13 * 1024 && !prefetch) 
    pref.page_bitmap.update(addr);

  /*
  if (blk_asid_match)
    pref.page_bitmap.evict(evicted_addr);
  */
  
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  auto &pref = ::SPP[{this, cpu}];
  //pref.warmup = warmup; 

  //pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?

  // Gather and issue prefetches after a context switch.
  if (champsim::operable::context_switch_mode && !champsim::operable::L2C_have_issued_context_switch_prefetches) {
    // Gather prefetches via the signature and pattern tables.
    if (!pref.context_switch_prefetch_gathered) {
      //pref.context_switch_gather_prefetches(this);
      pref.context_switch_prefetch_gathered = true;
      champsim::operable::context_switch_data_exchange = pref.page_bitmap.pf_metadata_limit;
    }

    /*
    if (pref.page_bitmap.pf_metadata < pref.page_bitmap.pf_metadata_limit) {
      uint64_t pf_addr = 64 + pref.page_bitmap.pf_metadata; //0xffffffffff5500 
      bool prefetched = this->prefetch_line(pf_addr, true, 0); 

      if (prefetched) 
        pref.page_bitmap.pf_metadata += 64; 

      if (pref.page_bitmap.pf_metadata >= pref.page_bitmap.pf_metadata_limit) 
        std::cout << "Pb has requested " << 1.0 * pref.page_bitmap.pf_metadata_limit/1024 << " KB of metadata to L2." << std::endl; 
    }
    */
    bool ready = (SIMULATE_WITH_BTB_RESET ? !champsim::operable::have_cleared_BTB : true) && 
                 (SIMULATE_WITH_BRANCH_PREDICTOR_RESET ? !champsim::operable::have_cleared_BP : true) &&
                 (SIMULATE_WITH_PREFETCHER_RESET ? !champsim::operable::have_cleared_prefetcher : true) &&
                 (SIMULATE_WITH_CACHE_RESET ? (champsim::operable::cache_clear_counter == 7) : true) &&
                 cpu_side_reset_ready;

    if (ready) {
        //) && pref.page_bitmap.pf_metadata == pref.page_bitmap.pf_metadata_limit) {
        //&& champsim::operable::Pb_metadata_loaded >= (champsim::operable::context_switch_data_exchange - 64)) {
      champsim::operable::context_switch_mode = false;
      std::cout << "Pb has loaded " << 1.0 * champsim::operable::Pb_metadata_loaded/1024 << " KB of metadata to L2." << std::endl;
      champsim::operable::Pb_metadata_loaded = 0;
      pref.page_bitmap.pf_metadata = 0;
      champsim::operable::cpu_side_reset_ready = false;
      champsim::operable::L2C_have_issued_context_switch_prefetches = true;
      champsim::operable::cache_clear_counter = 0;
      pref.context_switch_prefetch_gathered = false;
      champsim::operable::emptied_cache.clear();
      pref.page_bitmap.issued_cs_pf.clear();
      //pref.clear_states();
      std::cout << "SPP states not cleared." << std::endl;
      reset_misc::can_record_after_access = true;
      std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycle(s)" << " done at cycle " << current_cycle << std::endl;
    }
  }
  // Normal operation.
  // No prefetch gathering via the signature and pattern tables.
  else {
    pref.issue(this);
    pref.step_lookahead();
  }
}

void CACHE::prefetcher_final_stats() {
  std::cout << "SPP STATISTICS" << std::endl << std::endl;
  auto &pref = ::SPP[{this, cpu}];
  pref.print_stats(std::cout);
  std::cout << "Context switch prefetch accuracy: " << pref.page_bitmap.issued_cs_pf_hit << "/" << pref.page_bitmap.total_issued_cs_pf << "." << std::endl;
}

// WL
void CACHE::reset_prefetcher() {
  auto &pref = ::SPP[{this, cpu}];
  pref.clear_states();
  std::cout << "=> Prefetcher cleared at CACHE " << NAME << " at cycle " << current_cycle << std::endl;
}

// WL 
void CACHE::record_spp_camera_states() {
  std::cout << "Recording SPP states at CACHE " << NAME << std::endl;
  
  auto &pref = ::SPP[{this, cpu}];
  pref.cache_cycle = current_cycle;
  pref.record_spp_states();
}
