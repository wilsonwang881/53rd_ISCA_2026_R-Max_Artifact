#include "ipcp_L1.h"
#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, ipcp_l1::prefetcher> IPCP_L1;
}

void CACHE::prefetcher_initialize() {
  std::cout << "IPCP L1D prefetcher running at " << this->NAME << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool userful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::IPCP_L1[{this, cpu}];
   uint32_t cpu = this->cpu;
   uint64_t curr_page = base_addr >> LOG2_PAGE_SIZE;
   uint64_t cl_addr = base_addr >> LOG2_BLOCK_SIZE;
   uint64_t cl_offset = (base_addr >> LOG2_BLOCK_SIZE) & 0x3F;
   uint16_t signature = 0, last_signature = 0;
   int prefetch_degree = 0;
   int spec_nl_threshold = 0;
   int num_prefs = 0;
   uint32_t metadata=0;
   uint16_t ip_tag = (ip >> NUM_IP_INDEX_BITS) & ((1 << NUM_IP_TAG_BITS)-1);

   if(NUM_CPUS == 1){
       prefetch_degree = 3;
       spec_nl_threshold = 15;
   } else {                                    // tightening the degree and MPKC constraints for multi-core
       prefetch_degree = 2;
       spec_nl_threshold = 5;
   }

   // update miss counter
   if(cache_hit == 0)
       pref.num_misses[cpu] += 1;

   // update spec nl bit when num misses crosses certain threshold
   if(pref.num_misses[cpu] == 256){
       pref.mpkc[cpu] = ((float) pref.num_misses[cpu]/(this->current_cycle-pref.prev_cpu_cycle[cpu]))*1000; // WL: need CPU current cycle instead of cache current cycle.
       pref.prev_cpu_cycle[cpu] = this->current_cycle;
       if(pref.mpkc[cpu] > spec_nl_threshold)
           pref.spec_nl[cpu] = 0;
       else
           pref.spec_nl[cpu] = 1;
       pref.num_misses[cpu] = 0;
   }

   // calculate the index bit
       int index = ip & ((1 << NUM_IP_INDEX_BITS)-1);
       if(pref.trackers_l1[cpu][index].ip_tag != ip_tag){               // new/conflict IP
           if(pref.trackers_l1[cpu][index].ip_valid == 0){              // if valid bit is zero, update with latest IP info
           pref.trackers_l1[cpu][index].ip_tag = ip_tag;
           pref.trackers_l1[cpu][index].last_page = curr_page;
           pref.trackers_l1[cpu][index].last_cl_offset = cl_offset;
           pref.trackers_l1[cpu][index].last_stride = 0;
           pref.trackers_l1[cpu][index].signature = 0;
           pref.trackers_l1[cpu][index].conf = 0;
           pref.trackers_l1[cpu][index].str_valid = 0;
           pref.trackers_l1[cpu][index].str_strength = 0;
           pref.trackers_l1[cpu][index].str_dir = 0;
           pref.trackers_l1[cpu][index].ip_valid = 1;
       } else {                                                    // otherwise, reset valid bit and leave the previous IP as it is
           pref.trackers_l1[cpu][index].ip_valid = 0;
       }

       // issue a next line prefetch upon encountering new IP
           uint64_t pf_address = ((base_addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE; // BASE NL=1, changing it to 3
           metadata = pref.encode_metadata(1, NL_TYPE, pref.spec_nl[cpu]);
           this->prefetch_line(pf_address, FILL_L1, metadata);
           return metadata_in;
       }
       else {                                                     // if same IP encountered, set valid bit
           pref.trackers_l1[cpu][index].ip_valid = 1;
       }


       // calculate the stride between the current address and the last address
       int64_t stride = 0;
       if (cl_offset > pref.trackers_l1[cpu][index].last_cl_offset)
           stride = cl_offset - pref.trackers_l1[cpu][index].last_cl_offset;
       else {
           stride = pref.trackers_l1[cpu][index].last_cl_offset - cl_offset;
           stride *= -1;
       }

       // don't do anything if same address is seen twice in a row
       if (stride == 0)
           return metadata_in;


   // page boundary learning
   if(curr_page != pref.trackers_l1[cpu][index].last_page){
       if(stride < 0)
           stride += 64;
       else
           stride -= 64;
   }

   // update constant stride(CS) confidence
   pref.trackers_l1[cpu][index].conf = pref.update_conf(stride, pref.trackers_l1[cpu][index].last_stride, pref.trackers_l1[cpu][index].conf);

   // update CS only if confidence is zero
   if(pref.trackers_l1[cpu][index].conf == 0)
       pref.trackers_l1[cpu][index].last_stride = stride;

   last_signature = pref.trackers_l1[cpu][index].signature;
   // update complex stride(CPLX) confidence
   pref.DPT_l1[cpu][last_signature].conf = pref.update_conf(stride, pref.DPT_l1[cpu][last_signature].delta, pref.DPT_l1[cpu][last_signature].conf);

   // update CPLX only if confidence is zero
   if(pref.DPT_l1[cpu][last_signature].conf == 0)
       pref.DPT_l1[cpu][last_signature].delta = stride;

   // calculate and update new signature in IP table
   signature = pref.update_sig_l1(last_signature, stride);
   pref.trackers_l1[cpu][index].signature = signature;

   // check GHB for stream IP
   pref.check_for_stream_l1(index, cl_addr, cpu);

   SIG_DP(
   std::cout << ip << ", " << cache_hit << ", " << cl_addr << ", " << base_addr << ", " << stride << "; ";
   std::cout << last_signature<< ", "  << DPT_l1[cpu][last_signature].delta<< ", "  << DPT_l1[cpu][last_signature].conf << "; ";
   std::cout << trackers_l1[cpu][index].last_stride << ", " << stride << ", " << trackers_l1[cpu][index].conf << ", " << "; ";
   );

       if(pref.trackers_l1[cpu][index].str_valid == 1){                         // stream IP
           // for stream, prefetch with twice the usual degree
               prefetch_degree = prefetch_degree*2;
           for (int i=0; i<prefetch_degree; i++) {
               uint64_t pf_address = 0;

               if(pref.trackers_l1[cpu][index].str_dir == 1){                   // +ve stream
                   pf_address = (cl_addr + i + 1) << LOG2_BLOCK_SIZE;
                   metadata = pref.encode_metadata(1, S_TYPE, pref.spec_nl[cpu]);    // stride is 1
               }
               else{                                                       // -ve stream
                   pf_address = (cl_addr - i - 1) << LOG2_BLOCK_SIZE;
                   metadata = pref.encode_metadata(-1, S_TYPE, pref.spec_nl[cpu]);   // stride is -1
               }

               // Check if prefetch address is in same 4 KB page
               if ((pf_address >> LOG2_PAGE_SIZE) != (base_addr >> LOG2_PAGE_SIZE)){
                   break;
               }

               this->prefetch_line(pf_address, FILL_L1, metadata);
               num_prefs++;
               SIG_DP(std::cout << "1, ");
               }

       } else if(pref.trackers_l1[cpu][index].conf > 1 && pref.trackers_l1[cpu][index].last_stride != 0){            // CS IP
           for (int i=0; i<prefetch_degree; i++) {
               uint64_t pf_address = (cl_addr + (pref.trackers_l1[cpu][index].last_stride*(i+1))) << LOG2_BLOCK_SIZE;

               // Check if prefetch address is in same 4 KB page
               if ((pf_address >> LOG2_PAGE_SIZE) != (base_addr >> LOG2_PAGE_SIZE)){
                   break;
               }

               metadata = pref.encode_metadata(pref.trackers_l1[cpu][index].last_stride, CS_TYPE, pref.spec_nl[cpu]);
               this->prefetch_line(pf_address, FILL_L1, metadata);
               num_prefs++;
               SIG_DP(std::cout << trackers_l1[cpu][index].last_stride << ", ");
           }
       } else if(pref.DPT_l1[cpu][signature].conf >= 0 && pref.DPT_l1[cpu][signature].delta != 0) {  // if conf>=0, continue looking for delta
           int pref_offset = 0,i=0;                                                        // CPLX IP
           for (i=0; i<prefetch_degree; i++) {
               pref_offset += pref.DPT_l1[cpu][signature].delta;
               uint64_t pf_address = ((cl_addr + pref_offset) << LOG2_BLOCK_SIZE);

               // Check if prefetch address is in same 4 KB page
               if (((pf_address >> LOG2_PAGE_SIZE) != (base_addr >> LOG2_PAGE_SIZE)) ||
                       (pref.DPT_l1[cpu][signature].conf == -1) ||
                       (pref.DPT_l1[cpu][signature].delta == 0)){
                   // if new entry in DPT or delta is zero, break
                   break;
               }

               // we are not prefetching at L2 for CPLX type, so encode delta as 0
               metadata = pref.encode_metadata(0, CPLX_TYPE, pref.spec_nl[cpu]);
               if(pref.DPT_l1[cpu][signature].conf > 0){                                 // prefetch only when conf>0 for CPLX
                   this->prefetch_line(pf_address, FILL_L1, metadata);
                   num_prefs++;
                   SIG_DP(std::cout << pref_offset << ", ");
               }
               signature = pref.update_sig_l1(signature, pref.DPT_l1[cpu][signature].delta);
           }
       }

   // if no prefetches are issued till now, speculatively issue a next_line prefetch
   if(num_prefs == 0 && pref.spec_nl[cpu] == 1){                                        // NL IP
       uint64_t pf_address = ((base_addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
       metadata = pref.encode_metadata(1, NL_TYPE, pref.spec_nl[cpu]);
       this->prefetch_line(pf_address, FILL_L1, metadata);
       SIG_DP(std::cout << "1, ");
   }

   SIG_DP(std::cout << std::endl);

   // update the IP table entries
   pref.trackers_l1[cpu][index].last_cl_offset = cl_offset;
   pref.trackers_l1[cpu][index].last_page = curr_page;

   // update GHB
   // search for matching cl addr
   int ghb_index=0;
   for(ghb_index = 0; ghb_index < NUM_GHB_ENTRIES; ghb_index++)
       if(cl_addr == pref.ghb_l1[cpu][ghb_index])
           break;
   // only update the GHB upon finding a new cl address
   if(ghb_index == NUM_GHB_ENTRIES){
   for(ghb_index=NUM_GHB_ENTRIES-1; ghb_index>0; ghb_index--)
       pref.ghb_l1[cpu][ghb_index] = pref.ghb_l1[cpu][ghb_index-1];
   pref.ghb_l1[cpu][0] = cl_addr;
   }

   return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
