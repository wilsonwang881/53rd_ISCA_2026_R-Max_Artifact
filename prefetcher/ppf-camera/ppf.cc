#include <iostream>     // std::cout, std::endl
#include <ios>
#include <iomanip>      // std::setw
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <cmath>
#include "cache.h"
#include "ppf.h"
#include <assert.h>
#include <iterator>
//#include "spp.h"


// TODO: Find a good 64-bit hash function
namespace spp
{

//extern PERCEPTRON PERC;

uint64_t get_hash(uint64_t key)
{
  // Robert Jenkins' 32 bit mix function
  key += (key << 12);
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);

  // Knuth's multiplicative method
  key = (key >> 3) * 2654435761;

  return key;
}
}

  void spp::PERCEPTRON::get_perc_index(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, uint64_t perc_set[PERC_FEATURES],int32_t perc_sum)
  {
    //auto &pref = ::SPP[{this, cpu}];
    // Returns the imdexes for the perceptron tables
    uint64_t cache_line = base_addr >> LOG2_BLOCK_SIZE,
             page_addr  = base_addr >> LOG2_PAGE_SIZE;

    int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
    int sig_sum = (perc_sum < 0) ? (((-1) * perc_sum) + (1 << (SIG_DELTA_BIT - 1))) : perc_sum;//HL
    bool output = (perc_sum >= PERC_THRESHOLD_LO) ? 1 : 0;
    uint64_t  pre_hash[PERC_FEATURES];

    pre_hash[0] = base_addr;//base_addr
    pre_hash[1] = cache_line;
    pre_hash[2] = page_addr;//page_addr
    pre_hash[3] = confidence^page_addr;//confidence^page_addr
    pre_hash[4] = curr_sig^sig_delta;//curr_sig ^ sig_delta
    pre_hash[5] = ip_1 ^ (ip_2>>1) ^ (ip_3>>2);//ip_1 ^ (ip_2>>1) ^ (ip_3>>2)
    pre_hash[6] = ip^depth;//ip ^ depth
    pre_hash[7] = ip^sig_delta;//ip ^ sig_delta;
    pre_hash[8] = confidence;//confidence
    /*
    //HL
    pre_hash[9] = sig_delta;
    pre_hash[10] = depth;
    pre_hash[11] = ip;
    pre_hash[12] = curr_sig;
    pre_hash[13] = curr_sig^depth;
    pre_hash[14] = confidence^cache_line;
    pre_hash[15] = ip^page_addr;
    pre_hash[16] = ip^base_addr;
    pre_hash[17] = confidence^base_addr;
    pre_hash[18] = output;
    pre_hash[19] = sig_sum;
    pre_hash[20] = sig_sum^depth;
    pre_hash[21] = sig_sum^sig_delta;
    pre_hash[22] = output^page_addr;
    */

    for (int i = 0; i < PERC_FEATURES; i++) {
      perc_set[i] = (pre_hash[i]) % PERC_DEPTH[i]; // Variable depths
      //std::cout<<"The perc_DEPTH is"<<i<<"at"<<PERC_DEPTH[i]<<std::endl;
      /*
      SPP_DP (
          std::cout << "  Perceptron Set Index#: " << i << " = " <<  perc_set[i];
          );*/
    }
    /*
    SPP_DP (
        std::cout << std::endl;
        );*/		
  }

  int32_t	spp::PERCEPTRON::perc_predict(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, int32_t perc_sum)
  {
    /*
    SPP_DP (
        int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
        std::cout << "[PERC_PRED] Current IP: " << ip << "  and  Memory Adress: " << hex << base_addr << std::endl;
        std::cout << " Last Sig: " << last_sig << " Curr Sig: " << curr_sig << dec << std::endl;
        std::cout << " Cur Delta: " << cur_delta << " Sign Delta: " << sig_delta << " Confidence: " << confidence<< std::endl;
        std::cout << " ";
        );
    */

    uint64_t perc_set[PERC_FEATURES];
    // Get the indexes in perc_set[]
    get_perc_index(base_addr, ip, ip_1, ip_2, ip_3, cur_delta, last_sig, curr_sig, confidence, depth, perc_set,perc_sum);

    int32_t sum = 0;
    for (int i = 0; i < PERC_FEATURES; i++) {
      sum += perc_weights[perc_set[i]][i];	
      // Calculate Sum
    }
    /*
    SPP_DP (
        std::cout << " Sum of perceptrons: " << sum << " Prediction made: " << ((sum >= PERC_THRESHOLD_LO) ?  ((sum >= PERC_THRESHOLD_HI) ? FILL_L2 : FILL_LLC) : 0)  << std::endl;
        );
    */
    // Return the sum
    return sum;
  }

  void 	spp::PERCEPTRON::perc_update(uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3, int32_t cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t confidence, uint32_t depth, bool direction, int32_t perc_sum)
  {
    /*
    SPP_DP (
        int sig_delta = (cur_delta < 0) ? (((-1) * cur_delta) + (1 << (SIG_DELTA_BIT - 1))) : cur_delta;
        std::cout << "[PERC_UPD] (Recorded) IP: " << ip << "  and  Memory Adress: " << hex << base_addr << std::endl;
        std::cout << " Last Sig: " << last_sig << " Curr Sig: " << curr_sig << dec << std::endl;
        std::cout << " Cur Delta: " << cur_delta << " Sign Delta: " << sig_delta << " Confidence: "<< confidence << " Update Direction: " << direction << std::endl;
        std::cout << " ";
        );
    */
    uint64_t perc_set[PERC_FEATURES];
    // Get the perceptron indexes
    get_perc_index(base_addr, ip, ip_1, ip_2, ip_3, cur_delta, last_sig, curr_sig, confidence, depth, perc_set,perc_sum);

    int32_t sum = 0;
    for (int i = 0; i < PERC_FEATURES; i++) {
      // Marking the weights as touched for final dumping in the csv
      perc_touched[perc_set[i]][i] = 1;	
    }
    // Restore the sum that led to the prediction
    sum = perc_sum;

    if (!direction) { // direction = 1 means the sum was in the correct direction, 0 means it was in the wrong direction
      // Prediction wrong
      for (int i = 0; i < PERC_FEATURES; i++) {
        //std::cout<<"The direction is 0"<<std::endl;
        //std::cout<<"The sum is"<<sum<<std::endl;
        if (sum >= PERC_THRESHOLD_HI) {
          // Prediction was to prefectch -- so decrement counters
          //std::cout<<"we are here"<<std::endl;
          //std::cout<<"perc_weights[perc_set[i]][i] "<<perc_weights[perc_set[i]][i]<<std::endl;
          //std::cout<<"-1*(PERC_COUNTER_MAX+1)"<<(-1*(PERC_COUNTER_MAX+1))<<std::endl;
          if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
            {
              perc_weights[perc_set[i]][i]--;
              //std::cout<<"wrong Perc_weight reduction occurs"<<std::endl;
            }  
        }
        if (sum < PERC_THRESHOLD_HI) {
          // Prediction was to not prefetch -- so increment counters
          if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
            {
              perc_weights[perc_set[i]][i]++;
              //std::cout<<"wrong Perc_weight addition occurs"<<std::endl;
            }
        }
      }
      /*
      SPP_DP (
          int differential = (sum >= PERC_THRESHOLD_HI) ? -1 : 1;
          std::cout << " Direction is: " << direction << " and sum is:" << sum;
          std::cout << " Overall Differential: " << differential << std::endl;
          );
      */
    }
    if (direction && sum > NEG_UPDT_THRESHOLD && sum < POS_UPDT_THRESHOLD) {
      // Prediction correct but sum not 'saturated' enough
      for (int i = 0; i < PERC_FEATURES; i++) {
        if (sum >= PERC_THRESHOLD_HI) {
          // Prediction was to prefetch -- so increment counters
          if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
            {
              perc_weights[perc_set[i]][i]++;
              //std::cout<<"correct Perc_weight addition occurs"<<std::endl;
            }
        }
        if (sum < PERC_THRESHOLD_HI) {
          // Prediction was to not prefetch -- so decrement counters
          if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
            {
              perc_weights[perc_set[i]][i]--;
              //std::cout<<"correct Perc_weight reduction occurs"<<std::endl;
            }
        }
      }
      /*
      SPP_DP (
          int differential = 0;
          if (sum >= PERC_THRESHOLD_HI) differential =  1;
          if (sum  < PERC_THRESHOLD_HI) differential = -1;
          std::cout << " Direction is: " << direction << " and sum is:" << sum;
          std::cout << " Overall Differential: " << differential << std::endl;
          );*/
    }
  }

//void CACHE::prefetcher_cycle_operate(){}

void spp::PERCEPTRON::update_demand_ppf(uint64_t check_addr, std::size_t ,bool &SAME_PAGE,bool &PREFETCHED)
{
  //HL
  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = spp::get_hash(cache_line);

  //MAIN FILTER
  uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  //REJECT FILTER
  uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
           remainder_reject = hash % (1 << REMAINDER_BIT_REJ);

  //HL
  if(SAME_PAGE==true)
  {
    if(PREFETCHED==true)
    {
      //HL
      useful[quotient] = 1;
      
      if (valid[quotient]) {
          // Prefetch leads to a demand hit
          perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 1, perc_sum[quotient]);
          //std::cout<<"Perc demand valid"<<std::endl;
          // Histogramming Idea
          int32_t perc_sum_shifted = perc_sum[quotient] + (PERC_COUNTER_MAX+1)*PERC_FEATURES; 
          int32_t hist_index = perc_sum_shifted / 10;
        
          //Recording HL
          /*
          uint64_t cache_line_adr= address[quotient]>>LOG2_BLOCK_SIZE;
          uint64_t page_new_adr= address[quotient] >>LOG2_PAGE_SIZE;
          int sig_new_delta = (delta[quotient] < 0) ? (((-1) * delta[quotient]) + (1 << (SIG_DELTA_BIT - 1))) : delta[quotient];
          bool output_form = (perc_sum[quotient] >= PERC_THRESHOLD_LO) ? 1 : 0;
          int sig_sum_form = (perc_sum[quotient] < 0) ? (((-1) * perc_sum[quotient]) + (1 << (SIG_DELTA_BIT - 1))) : perc_sum[quotient];//HL          

          new_table.confidence_record=(confidence[quotient]);
          new_table.cache_adr=cache_line_adr;
          new_table.base_adr=address[quotient];
          new_table.page_adr=page_new_adr;
          new_table.PC_123_XOR= (pc_1[quotient])^ (pc_2[quotient]>>1) ^ (pc_3[quotient]>>2);
          new_table.PC_Del_XOR= pc[quotient]^sig_new_delta;
          new_table.PC_depth_XOR= pc[quotient] ^ la_depth[quotient];
          new_table.Sig_Del_XOR= cur_signature[quotient]^ sig_new_delta;
          new_table.Conf_pageadr_XOR=page_new_adr^confidence[quotient];
          new_table.hit_or_not=1;
          new_table.delta=sig_new_delta;//10
          new_table.depth=la_depth[quotient];//11
          new_table.ip=pc[quotient];//12
          new_table.signature = cur_signature[quotient];//13
          new_table.sig_xor_depth = cur_signature[quotient]^la_depth[quotient];//14
          new_table.conf_xor_cache = confidence[quotient]^cache_line_adr;//15
          new_table.pc_xor_pageadr = pc[quotient]^page_new_adr;//16
          new_table.pc_xor_baseadr = pc[quotient]^address[quotient];//17
          new_table.conf_xor_baseadr= confidence[quotient] ^ address[quotient];//18 
          new_table.output=output_form;//19
          new_table.sig_sum = sig_sum_form;//20
          new_table.sum_xor_depth = perc_sum[quotient]^la_depth[quotient];//21
          new_table.sum_xor_delta = perc_sum[quotient]^ sig_new_delta;//22
          new_table.output_xor_pageadr = output_form ^ page_new_adr;//23

           //Open the file is here
          myfile.open("ppf_confidence_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.confidence_record<<std::endl;
          myfile.close();

          myfile.open("ppf_cache_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.cache_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.base_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.page_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_123_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_123_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_Del_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_Depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_depth_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_Sig_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.Sig_Del_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_Conf_Page_Adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.Conf_pageadr_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_hit_miss_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.hit_or_not<<std::endl;
          myfile.close();
          
          myfile.open("ppf_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.delta<<std::endl;
          myfile.close(); //10
          
          myfile.open("ppf_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.depth<<std::endl;
          myfile.close(); //11

          myfile.open("ppf_pc_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.ip<<std::endl;
          myfile.close(); //12

          myfile.open("ppf_sig_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.signature<<std::endl;
          myfile.close(); //13
          
          myfile.open("ppf_sig_xor_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sig_xor_depth<<std::endl;
          myfile.close(); //14

          myfile.open("ppf_conf_xor_cache_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.conf_xor_cache<<std::endl;
          myfile.close(); //15

          myfile.open("ppf_pc_xor_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.pc_xor_pageadr<<std::endl;
          myfile.close(); //16

          myfile.open("ppf_pc_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.pc_xor_baseadr<<std::endl;
          myfile.close(); //17          
          
          myfile.open("ppf_conf_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.conf_xor_baseadr<<std::endl;
          myfile.close(); //18

          myfile.open("ppf_output_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.output<<std::endl;
          myfile.close(); //19

          myfile.open("ppf_perc_sum_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sig_sum<<std::endl;
          myfile.close(); //20
          
          myfile.open("ppf_perc_sum_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sum_xor_depth<<std::endl;
          myfile.close(); //21

          myfile.open("ppf_perc_sum_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sum_xor_delta<<std::endl;
          myfile.close(); //22
          
          myfile.open("ppf_output_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.output_xor_pageadr<<std::endl;
          myfile.close(); //23
          */ 
      }
      
      //HL
    }

    if (!(valid[quotient] && remainder_tag[quotient] == remainder)) {//may need t0 change
        // AND If Rejected by Perc
        if (valid_reject[quotient_reject] && remainder_tag_reject[quotient_reject] == remainder_reject) {
          //std::cout<<"The train_neg is"<<train_neg<<std::endl;
          if (train_neg) {
            // Not prefetched but could have been a good idea to prefetch
            perc_update(address_reject[quotient_reject], pc_reject[quotient_reject], pc_1_reject[quotient_reject], pc_2_reject[quotient_reject], pc_3_reject[quotient_reject], delta_reject[quotient_reject], last_signature_reject[quotient_reject], cur_signature_reject[quotient_reject], confidence_reject[quotient_reject], la_depth_reject[quotient_reject], 0, perc_sum_reject[quotient_reject]);
            //std::cout<<"train_neg of perc_update"<<std::endl;
            valid_reject[quotient_reject] = 0;
            remainder_tag_reject[quotient_reject] = 0;
            // Printing Stats
            //::GHR.reject_update++;//HL ////////do I need to recover this ?
            reject_update++;
            
            //RECORDING HL
            /*
            uint64_t cache_line_adr= address_reject[quotient_reject]>>LOG2_BLOCK_SIZE;
            uint64_t page_new_adr= address_reject[quotient_reject]>>LOG2_PAGE_SIZE;
            int sig_new_delta = (delta_reject[quotient_reject] < 0) ? (((-1) * delta_reject[quotient_reject]) + (1 << (SIG_DELTA_BIT - 1))) : delta_reject[quotient_reject];
            bool output_form_neg = (perc_sum_reject[quotient_reject] >= PERC_THRESHOLD_LO) ? 1 : 0;
            int sig_sum_form_neg = (perc_sum_reject[quotient_reject] < 0) ? (((-1) * perc_sum_reject[quotient_reject]) + (1 << (SIG_DELTA_BIT - 1))) : perc_sum_reject[quotient_reject];//HL 

            new_table.confidence_record=confidence_reject[quotient_reject];
            new_table.cache_adr=cache_line_adr;
            new_table.base_adr=address_reject[quotient_reject];
            new_table.page_adr=page_new_adr;
            new_table.PC_123_XOR= (pc_1_reject[quotient_reject])^ (pc_2_reject[quotient_reject]>>1) ^ (pc_3_reject[quotient_reject]>>2);
            new_table.PC_Del_XOR= (pc_reject[quotient_reject]^sig_new_delta);
            new_table.PC_depth_XOR= pc_reject[quotient_reject] ^ la_depth_reject[quotient_reject];
            new_table.Sig_Del_XOR=  cur_signature_reject[quotient_reject]^ sig_new_delta;
            new_table.Conf_pageadr_XOR=page_new_adr^confidence_reject[quotient_reject];
            new_table.hit_or_not=1;

            new_table.delta=sig_new_delta;//10
            new_table.depth=la_depth_reject[quotient_reject];//11
            new_table.ip=pc_reject[quotient_reject];//12
            new_table.signature = cur_signature_reject[quotient_reject];//13
            new_table.sig_xor_depth = cur_signature_reject[quotient_reject]^la_depth_reject[quotient_reject];//14
            new_table.conf_xor_cache = confidence_reject[quotient_reject]^cache_line_adr;//15
            new_table.pc_xor_pageadr = pc_reject[quotient_reject]^page_new_adr;//16
            new_table.pc_xor_baseadr = pc_reject[quotient_reject]^address_reject[quotient_reject];//17
            new_table.conf_xor_baseadr= confidence_reject[quotient_reject] ^ address_reject[quotient_reject];//18 
            new_table.output=output_form_neg;//19
            new_table.sig_sum = sig_sum_form_neg;//20
            new_table.sum_xor_depth = perc_sum_reject[quotient_reject]^la_depth_reject[quotient_reject];//21
            new_table.sum_xor_delta = perc_sum_reject[quotient_reject]^ sig_new_delta;//22
            new_table.output_xor_pageadr = output_form_neg ^ page_new_adr;//23


            //Open the file is here
            //Open the file is here
            myfile.open("ppf_confidence_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.confidence_record<<std::endl;
            myfile.close();

            myfile.open("ppf_cache_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.cache_adr<<std::endl;
            myfile.close();

            myfile.open("ppf_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.base_adr<<std::endl;
            myfile.close();

            myfile.open("ppf_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.page_adr<<std::endl;
            myfile.close();

            myfile.open("ppf_PC_123_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.PC_123_XOR<<std::endl;
            myfile.close();

            myfile.open("ppf_PC_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.PC_Del_XOR<<std::endl;
            myfile.close();

            myfile.open("ppf_PC_Depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.PC_depth_XOR<<std::endl;
            myfile.close();

            myfile.open("ppf_Sig_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.Sig_Del_XOR<<std::endl;
            myfile.close();

            myfile.open("ppf_Conf_Page_Adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.Conf_pageadr_XOR<<std::endl;
            myfile.close();

            myfile.open("ppf_hit_miss_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.hit_or_not<<std::endl;
            myfile.close();
          
            myfile.open("ppf_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.delta<<std::endl;
            myfile.close(); //10
          
            myfile.open("ppf_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.depth<<std::endl;
            myfile.close(); //11

            myfile.open("ppf_pc_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.ip<<std::endl;
            myfile.close(); //12

            myfile.open("ppf_sig_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.signature<<std::endl;
            myfile.close(); //13
          
            myfile.open("ppf_sig_xor_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.sig_xor_depth<<std::endl;
            myfile.close(); //14

            myfile.open("ppf_conf_xor_cache_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.conf_xor_cache<<std::endl;
            myfile.close(); //15

            myfile.open("ppf_pc_xor_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.pc_xor_pageadr<<std::endl;
            myfile.close(); //16

            myfile.open("ppf_pc_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.pc_xor_baseadr<<std::endl;
            myfile.close(); //17          
          
            myfile.open("ppf_conf_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.conf_xor_baseadr<<std::endl;
            myfile.close(); //18

            myfile.open("ppf_output_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.output<<std::endl;
            myfile.close(); //19

            myfile.open("ppf_perc_sum_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.sig_sum<<std::endl;
            myfile.close(); //20
          
            myfile.open("ppf_perc_sum_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.sum_xor_depth<<std::endl;
            myfile.close(); //21

            myfile.open("ppf_perc_sum_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.sum_xor_delta<<std::endl;
            myfile.close(); //22
          
            myfile.open("ppf_output_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
            myfile<<new_table.output_xor_pageadr<<std::endl;
            myfile.close(); //23           
            */
          }
        }
      }
  }  
}

//HL need to add the add the spp perc reject
bool spp::PERCEPTRON::update_perc(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth){
  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = spp::get_hash(cache_line);

  //MAIN FILTER
  uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  //REJECT FILTER
  uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
           remainder_reject = hash % (1 << REMAINDER_BIT_REJ);
  
  switch (filter_request) {
  case SPP_PERC_REJECT:
    //std::cout<<"enter filter_reject"<<std::endl;
    //std::cout<<"The train neg is"<<train_neg<<std::endl;
    if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
        // We want to check if the prefetch would have gone through had perc not rejected
        // So even in perc reject case, I'm checking in the accept filter for redundancy

        return false; // False return indicates "Do not prefetch"
      } else {
        if (train_neg) {

          //std::cout<<"add to the neg table"<<std::endl;
          valid_reject[quotient_reject] = 1;
          remainder_tag_reject[quotient_reject] = remainder_reject;

          // Logging perc features
          address_reject[quotient_reject] = base_addr;
          pc_reject[quotient_reject] = ip;
          pc_1_reject[quotient_reject] = ip_1;//HL
          pc_2_reject[quotient_reject] = ip_2;//HL
          pc_3_reject[quotient_reject] = ip_3;//HL
          delta_reject[quotient_reject] = cur_delta;
          perc_sum_reject[quotient_reject] = sum;
          last_signature_reject[quotient_reject] = last_sig;
          cur_signature_reject[quotient_reject] = curr_sig;
          confidence_reject[quotient_reject] = conf;
          la_depth_reject[quotient_reject] = depth;
        }
        /*
        SPP_DP (
            std::cout << "[FILTER] " << __func__ << " PF rejected by perceptron. Set valid_reject for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            std::cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag_reject[quotient_reject] << std::endl; 
            std::cout << " More Recorded Metadata: Addr: " << hex << address_reject[quotient_reject] << dec << " PC: " << pc_reject[quotient_reject] << " Delta: " << delta_reject[quotient_reject] << " Last Signature: " << last_signature_reject[quotient_reject] << " Current Signature: " << cur_signature_reject[quotient_reject] << " Confidence: " << confidence_reject[quotient_reject] << std::endl;
            );
        */
      }
      break;

  case L2C_EVICT:

      // Decrease global pf_useful counter when there is a useless prefetch (prefetched but not used)
      if (valid[quotient] && !useful[quotient]) {
        //if (pf_useful)//HL 
            //pf_useful--;//HL

        // Prefetch leads to eviction
        //std::cout<<"evict occurs"<<std::endl;
        
        //RECORDING HL
          /*
          uint64_t cache_line_adr= address[quotient]>>LOG2_BLOCK_SIZE;
          uint64_t page_new_adr= address[quotient] >>LOG2_PAGE_SIZE;
          int sig_new_delta = (delta[quotient] < 0) ? (((-1) * delta[quotient]) + (1 << (SIG_DELTA_BIT - 1))) : delta[quotient];
          bool output_form = (perc_sum[quotient] >= PERC_THRESHOLD_LO) ? 1 : 0;
          int sig_sum_form = (perc_sum[quotient] < 0) ? (((-1) * perc_sum[quotient]) + (1 << (SIG_DELTA_BIT - 1))) : perc_sum[quotient];//HL          

          new_table.confidence_record=(confidence[quotient]);
          new_table.cache_adr=cache_line_adr;
          new_table.base_adr=address[quotient];
          new_table.page_adr=page_new_adr;
          new_table.PC_123_XOR= (pc_1[quotient])^ (pc_2[quotient]>>1) ^ (pc_3[quotient]>>2);
          new_table.PC_Del_XOR= pc[quotient]^sig_new_delta;
          new_table.PC_depth_XOR= pc[quotient] ^ la_depth[quotient];
          new_table.Sig_Del_XOR= cur_signature[quotient]^ sig_new_delta;
          new_table.Conf_pageadr_XOR=page_new_adr^confidence[quotient];
          new_table.hit_or_not=0;
          new_table.delta=sig_new_delta;//10
          new_table.depth=la_depth[quotient];//11
          new_table.ip=pc[quotient];//12
          new_table.signature = cur_signature[quotient];//13
          new_table.sig_xor_depth = cur_signature[quotient]^la_depth[quotient];//14
          new_table.conf_xor_cache = confidence[quotient]^cache_line_adr;//15
          new_table.pc_xor_pageadr = pc[quotient]^page_new_adr;//16
          new_table.pc_xor_baseadr = pc[quotient]^address[quotient];//17
          new_table.conf_xor_baseadr= confidence[quotient] ^ address[quotient];//18 
          new_table.output=output_form;//19
          new_table.sig_sum = sig_sum_form;//20
          new_table.sum_xor_depth = perc_sum[quotient]^la_depth[quotient];//21
          new_table.sum_xor_delta = perc_sum[quotient]^ sig_new_delta;//22
          new_table.output_xor_pageadr = output_form ^ page_new_adr;//23

           //Open the file is here
          myfile.open("ppf_confidence_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.confidence_record<<std::endl;
          myfile.close();

          myfile.open("ppf_cache_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.cache_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.base_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.page_adr<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_123_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_123_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_Del_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_PC_Depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.PC_depth_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_Sig_Del_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.Sig_Del_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_Conf_Page_Adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.Conf_pageadr_XOR<<std::endl;
          myfile.close();

          myfile.open("ppf_hit_miss_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.hit_or_not<<std::endl;
          myfile.close();
          
          myfile.open("ppf_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.delta<<std::endl;
          myfile.close(); //10
          
          myfile.open("ppf_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.depth<<std::endl;
          myfile.close(); //11

          myfile.open("ppf_pc_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.ip<<std::endl;
          myfile.close(); //12

          myfile.open("ppf_sig_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.signature<<std::endl;
          myfile.close(); //13
          
          myfile.open("ppf_sig_xor_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sig_xor_depth<<std::endl;
          myfile.close(); //14

          myfile.open("ppf_conf_xor_cache_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.conf_xor_cache<<std::endl;
          myfile.close(); //15

          myfile.open("ppf_pc_xor_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.pc_xor_pageadr<<std::endl;
          myfile.close(); //16

          myfile.open("ppf_pc_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.pc_xor_baseadr<<std::endl;
          myfile.close(); //17          
          
          myfile.open("ppf_conf_xor_base_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.conf_xor_baseadr<<std::endl;
          myfile.close(); //18

          myfile.open("ppf_output_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.output<<std::endl;
          myfile.close(); //19

          myfile.open("ppf_perc_sum_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sig_sum<<std::endl;
          myfile.close(); //20
          
          myfile.open("ppf_perc_sum_depth_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sum_xor_depth<<std::endl;
          myfile.close(); //21

          myfile.open("ppf_perc_sum_delta_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.sum_xor_delta<<std::endl;
          myfile.close(); //22
          
          myfile.open("ppf_output_page_adr_603.txt", std::ios::in | std::ios::app | std::ios::binary);
          myfile<<new_table.output_xor_pageadr<<std::endl;
          myfile.close(); //23 
          */
        perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 0, perc_sum[quotient]);
      }
      // Reset filter entry
      valid[quotient] = 0;
      useful[quotient] = 0;
      remainder_tag[quotient] = 0;

      // Reset reject filter too
      valid_reject[quotient_reject] = 0;
      remainder_tag_reject[quotient_reject] = 0;

      break;
    }
  return true;    

}

//HL add_to_filter, migrate from filter.h

bool spp::PERCEPTRON::add_to_filter(uint64_t check_addr, uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth)
{

  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = spp::get_hash(cache_line);

  //MAIN FILTER
  uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  //REJECT FILTER
  uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
           remainder_reject = hash % (1 << REMAINDER_BIT_REJ);

  switch (filter_request) {
    case SPP_L2C_PREFETCH:
      valid[quotient] = 1;  // Mark as prefetched
      useful[quotient] = 0; // Reset useful bit
      remainder_tag[quotient] = remainder;

      // Logging perc features
      delta[quotient] = cur_delta;
      pc[quotient] = ip;
      pc_1[quotient] = ip_1;
      pc_2[quotient] = ip_2;
      pc_3[quotient] = ip_3;
      last_signature[quotient] = last_sig; 
      cur_signature[quotient] = curr_sig;
      confidence[quotient] = conf;
      address[quotient] = base_addr; 
      perc_sum[quotient] = sum;
      la_depth[quotient] = depth;
      /*
      SPP_DP (
          std::cout << "[FILTER] " << __func__ << " set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
          std::cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag[quotient] << " valid: " << valid[quotient] << " useful: " << useful[quotient] << std::endl; 
          std::cout << " More Recorded Metadata: Addr:" << hex << address[quotient] << dec << " PC: " << pc[quotient] << " Delta: " << delta[quotient] << " Last Signature: " << last_signature[quotient] << " Current Signature: " << cur_signature[quotient] << " Confidence: " << confidence[quotient] << std::endl;
          );
      */
      break;

    case SPP_LLC_PREFETCH:
      // NOTE: SPP_LLC_PREFETCH has relatively low confidence (FILL_THRESHOLD <= SPP_LLC_PREFETCH < PF_THRESHOLD) 
      // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
      // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
      // we can get this cache line immediately from the LLC (not from DRAM)
      // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH

      //valid[quotient] = 1;
      //useful[quotient] = 0;
      /*
      SPP_DP (
          std::cout << "[FILTER] " << __func__ << " don't set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
          std::cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << std::endl; 
          );
      */
      break;
    default:
      std::cout << "[FILTER] Invalid filter request type: " << filter_request << std::endl;
      assert(0);
  }
  return true;
}
