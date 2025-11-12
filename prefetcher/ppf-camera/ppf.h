#include <iostream>
#include <fstream>
//#include "spp.h"//HL
//using namespace std;

#pragma once  // modern way, prevents multiple inclusion

// or the classic guard
#ifndef PERCEPTRON_H
#define PERCEPTRON_H

namespace spp{

//migrate from the filter.h
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

enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT};//HL

//HL Migrate from the filter.h

// SPP functional knobs
constexpr bool LOOKAHEAD_ON=true;
constexpr bool FILTER_ON=true;
constexpr bool GHR_ON=true;
constexpr bool SPP_SANITY_CHECK=true;

// Perceptron paramaters
constexpr int32_t  PERC_ENTRIES=4096; //Upto 12-bit addressing in hashed perceptron //unsigned
constexpr int32_t  PERC_FEATURES=9; //Keep increasing based on new features //unsigned
constexpr int32_t  PERC_COUNTER_MAX=15; //-16 to +15: 5 bits counter //unsigned
constexpr int32_t PERC_THRESHOLD_HI=-5;  //-5
constexpr int32_t PERC_THRESHOLD_LO=-15; //-15
constexpr int32_t POS_UPDT_THRESHOLD=90;
constexpr int32_t NEG_UPDT_THRESHOLD=-80;

//original SIG
constexpr uint32_t SIG_DELTA_BIT=7;

//enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT}; // Request type for prefetch filter

uint64_t get_hash(uint64_t key);

struct record_table {
    //
    uint64_t confidence_record=0;
    uint64_t cache_adr=0;
    uint64_t base_adr=0;
    uint64_t page_adr=0;
    uint64_t PC_123_XOR=0;
    uint64_t PC_Del_XOR=0;
    uint64_t PC_depth_XOR=0;
    uint64_t Sig_Del_XOR=0;
    uint64_t Conf_pageadr_XOR=0;
    
    //Add new feature
    int32_t delta;
    uint32_t depth;
    uint64_t ip;
    uint32_t signature;
    uint64_t sig_xor_depth;
    uint64_t conf_xor_cache;
    uint64_t pc_xor_pageadr;
    uint64_t pc_xor_baseadr;
    uint64_t conf_xor_baseadr;
    bool output;
    int32_t sig_sum;
    int32_t sum_xor_depth;
    int32_t sum_xor_delta;
    int32_t output_xor_pageadr;

    uint64_t hit_or_not=0;

}; //new_table;

//void get_perc_index(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, uint64_t perc_set[PERC_FEATURES]);

class PERCEPTRON {
  public:

    //Migrate from the filter.h
          //HL
      uint64_t remainder_tag[FILTER_SET]={0};
      uint64_t pc[FILTER_SET],
             pc_1[FILTER_SET],
             pc_2[FILTER_SET],
             pc_3[FILTER_SET],
             address[FILTER_SET];
      bool     valid[FILTER_SET]={0};  // Consider this as "prefetched"
      bool     useful[FILTER_SET]={0}; // Consider this as "used"
      int32_t  delta[FILTER_SET],
             perc_sum[FILTER_SET];
      uint32_t last_signature[FILTER_SET],
             confidence[FILTER_SET],
             cur_signature[FILTER_SET],
             la_depth[FILTER_SET];

      uint64_t remainder_tag_reject[FILTER_SET_REJ]={0};
      uint64_t pc_reject[FILTER_SET_REJ],
             pc_1_reject[FILTER_SET_REJ],
             pc_2_reject[FILTER_SET_REJ],
             pc_3_reject[FILTER_SET_REJ],
             address_reject[FILTER_SET_REJ];
      bool   valid_reject[FILTER_SET_REJ]={0}; // Entries which the perceptron rejected
      int32_t  delta_reject[FILTER_SET_REJ],
             perc_sum_reject[FILTER_SET_REJ];
      uint32_t last_signature_reject[FILTER_SET_REJ],
             confidence_reject[FILTER_SET_REJ],
             cur_signature_reject[FILTER_SET_REJ],
             la_depth_reject[FILTER_SET_REJ];

      bool train_neg=0;
      //stat
      long reject_update;
    //HL Migrate from the filter.h

    // Perc Weights
    int32_t perc_weights[PERC_ENTRIES][PERC_FEATURES]={0};

    // Only for dumping csv
    bool    perc_touched[PERC_ENTRIES][PERC_FEATURES]={0};

    // CONST depths for different features
    //static constexpr int32_t PERC_DEPTH[PERC_FEATURES]={2048,4096,4096,4096,1024,4096,1024,2048,128,2048,1024,4096,2048,1024,4096,2048,2048,4096,1024,2048,1024,1024,1024};
    static constexpr int32_t PERC_DEPTH[PERC_FEATURES]={2048,4096,4096,4096,1024,4096,1024,2048,128};
    record_table new_table;
    std::ofstream myfile;
    /*
    PERCEPTRON() {
      std::cout << "\nInitialize PERCEPTRON" << std::endl;
      std::cout << "PERC_ENTRIES: " << PERC_ENTRIES << std::endl;
      std::cout << "PERC_FEATURES: " << PERC_FEATURES << std::endl;

      PERC_DEPTH[0] = 2048;   //base_addr;
      PERC_DEPTH[1] = 4096;   //cache_line;
      PERC_DEPTH[2] = 4096;  	//page_addr;
      PERC_DEPTH[3] = 4096;   //confidence ^ page_addr;
      PERC_DEPTH[4] = 1024;	//curr_sig ^ sig_delta;
      PERC_DEPTH[5] = 4096; 	//ip_1 ^ ip_2 ^ ip_3;		
      PERC_DEPTH[6] = 1024; 	//ip ^ depth;
      PERC_DEPTH[7] = 2048;   //ip ^ sig_delta;
      PERC_DEPTH[8] = 128;   	//confidence;

      for (int i = 0; i < PERC_ENTRIES; i++) {
        for (int j = 0;j < PERC_FEATURES; j++) {
          perc_weights[i][j] = 0;
          perc_touched[i][j] = 0;
        }
      }
      //Migrate from filter.h
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
      //Migrate from filter.h

    }
    */
    void update_demand_ppf(uint64_t pf_addr, std::size_t set,bool &SAME_PAGE,bool &PREFETCHED);//rename and migrate from the filter.h
    bool update_perc(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth);//migrate filter.h
    bool add_to_filter(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth);//migrate from filter.h
    
    void	 perc_update(uint64_t check_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, bool direction, int32_t perc_sum);
    int32_t	perc_predict(uint64_t check_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth,int32_t perc_sum);
    void get_perc_index(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, uint64_t perc_set[PERC_FEATURES],int32_t perc_sum);//HL
};
//extern PERCEPTRON PERC;
}
#endif
