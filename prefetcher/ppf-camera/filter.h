#include <array>
#include <bitset>
#include <cstdint>

#include "champsim_constants.h"
//#include "spp.h"//HL
//#include "ppf.h"

namespace spp
{
  //extern PERCEPTRON PERC;
  constexpr std::size_t GLOBAL_COUNTER_BITS = 10;
  
  /*
  //HL
  // Prefetch filter parameters
  constexpr unsigned QUOTIENT_BIT=10;
  constexpr unsigned REMAINDER_BIT=6;
  constexpr unsigned HASH_BIT=(QUOTIENT_BIT + REMAINDER_BIT + 1);
  constexpr std::size_t FILTER_SET=(1 << QUOTIENT_BIT);

  constexpr unsigned  QUOTIENT_BIT_REJ=10;
  constexpr unsigned  REMAINDER_BIT_REJ=8;
  constexpr unsigned  HASH_BIT_REJ=(QUOTIENT_BIT_REJ + REMAINDER_BIT_REJ + 1);
  constexpr std::size_t FILTER_SET_REJ=(1 << QUOTIENT_BIT_REJ);
  //HL
  */

  enum confidence_t {REJECT, WEAKLY_ACCEPT, STRONGLY_ACCEPT}; // Request type for prefetch filter check
  //enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT};//HL

  class SPP_PREFETCH_FILTER
  {
    public:
      static constexpr std::size_t SET = 1 << 8;
      static constexpr std::size_t WAY = 1;
      static constexpr std::size_t TAG_BITS = 27;

      /*
      //HL
      uint64_t remainder_tag[FILTER_SET],
             pc[FILTER_SET],
             pc_1[FILTER_SET],
             pc_2[FILTER_SET],
             pc_3[FILTER_SET],
             address[FILTER_SET];
      bool     valid[FILTER_SET],  // Consider this as "prefetched"
             useful[FILTER_SET]; // Consider this as "used"
      int32_t  delta[FILTER_SET],
             perc_sum[FILTER_SET];
      uint32_t last_signature[FILTER_SET],
             confidence[FILTER_SET],
             cur_signature[FILTER_SET],
             la_depth[FILTER_SET];

      uint64_t remainder_tag_reject[FILTER_SET_REJ],
             pc_reject[FILTER_SET_REJ],
             pc_1_reject[FILTER_SET_REJ],
             pc_2_reject[FILTER_SET_REJ],
             pc_3_reject[FILTER_SET_REJ],
             address_reject[FILTER_SET_REJ];
      bool   valid_reject[FILTER_SET_REJ]; // Entries which the perceptron rejected
      int32_t  delta_reject[FILTER_SET_REJ],
             perc_sum_reject[FILTER_SET_REJ];
      uint32_t last_signature_reject[FILTER_SET_REJ],
             confidence_reject[FILTER_SET_REJ],
             cur_signature_reject[FILTER_SET_REJ],
             la_depth_reject[FILTER_SET_REJ];

      bool train_neg;
      
      //stat
      long reject_update;

      //HL
      */
      //used for managing the ppf system
      bool SAME_PAGE;
      bool PREFETCHED;
      /*
      SPP_PREFETCH_FILTER() {
      //std::cout << std::endl << "Initialize PREFETCH FILTER" << std::endl;
      //std::cout << "FILTER_SET: " << FILTER_SET << std::endl;

      for (uint32_t set = 0; set < FILTER_SET; set++) {
        remainder_tag[set] = 0;
        valid[set] = 0;
        useful[set] = 0;
      }
      for (uint32_t set = 0; set < FILTER_SET_REJ; set++) {
        valid_reject[set] = 0;
        remainder_tag_reject[set] = 0;
      }
      train_neg = 0;
    }
    */
    private:
      struct filter_entry_t
      {
        uint64_t     page_no = 0;
        unsigned     last_used = 0;
        std::bitset<PAGE_SIZE/BLOCK_SIZE> prefetched;
        std::bitset<PAGE_SIZE/BLOCK_SIZE> used;
      };

      unsigned access_count = 0;

      std::array<filter_entry_t, WAY * SET> prefetch_table;

    public:
      //friend class is_valid<filter_entry_t>;

      // Global counters to calculate global prefetching accuracy
      bool tagless = false;
      unsigned long pf_useful = 0, pf_issued = 0;
      const static int highconf_threshold = 40;

      unsigned repl_count = 0;

      confidence_t check(uint64_t pf_addr, int confidence = highconf_threshold) const;
      void update_demand(uint64_t pf_addr, std::size_t set);//HL modify
      void update_issue(uint64_t pf_addr, std::size_t set);
      //bool update_perc(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth,PERCEPTRON &PERC);//HL
      //bool add_to_filter(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth);//HL
  };
}

