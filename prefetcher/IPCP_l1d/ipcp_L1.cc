#include "ipcp_L1.h"
#include "cache.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, ipcp_l1::prefetcher> ipcp_l1;
}

/***************Updating the signature*************************************/
uint16_t ipcp_l1::prefetcher::update_sig_l1(uint16_t old_sig, int delta)
{
    uint16_t new_sig = 0;
    int sig_delta = 0;

// 7-bit sign magnitude form, since we need to track deltas from +63 to -63
    sig_delta = (delta < 0) ? (((-1) * delta) + (1 << 6)) : delta;
    new_sig = ((old_sig << 1) ^ sig_delta) & 0xFFF;                     // 12-bit signature

    return new_sig;
}



/****************Encoding the metadata***********************************/
uint32_t ipcp_l1::prefetcher::encode_metadata(int stride, uint16_t type, int _spec_nl)
{
   uint32_t metadata = 0;

   // first encode stride in the last 8 bits of the metadata
   if(stride > 0)
       metadata = stride;
   else
       metadata = ((-1*stride) | 0b1000000);

   // encode the type of IP in the next 4 bits
   metadata = metadata | (type << 8);

   // encode the speculative NL bit in the next 1 bit
   metadata = metadata | (_spec_nl << 12);

   return metadata;

}


/*********************Checking for a global stream (GS class)***************/

void ipcp_l1::prefetcher::check_for_stream_l1(int index, uint64_t cl_addr, uint8_t cpu)
{
   int pos_count=0, neg_count=0, count=0;
   uint64_t check_addr = cl_addr;

   // check for +ve stream
       for(int i=0; i<NUM_GHB_ENTRIES; i++)
       {
           check_addr--;
           for(int j=0; j<NUM_GHB_ENTRIES; j++)
               if(check_addr == ghb_l1[cpu][j])
               {
                   pos_count++;
                   break;
               }
       }

   check_addr = cl_addr;
   // check for -ve stream
       for(int i=0; i<NUM_GHB_ENTRIES; i++)
       {
           check_addr++;
           for(int j=0; j<NUM_GHB_ENTRIES; j++)
               if(check_addr == ghb_l1[cpu][j])
               {
                   neg_count++;
                   break;
               }
       }

       if(pos_count > neg_count){                                // stream direction is +ve
           trackers_l1[cpu][index].str_dir = 1;
           count = pos_count;
       }
       else{                                                     // stream direction is -ve
           trackers_l1[cpu][index].str_dir = 0;
           count = neg_count;
       }

   if(count > NUM_GHB_ENTRIES/2)
   {                                // stream is detected
       trackers_l1[cpu][index].str_valid = 1;
       if(count >= (NUM_GHB_ENTRIES*3)/4)                        // stream is classified as strong if more than 3/4th entries belong to stream
           trackers_l1[cpu][index].str_strength = 1;
   }
   else
   {
       if(trackers_l1[cpu][index].str_strength == 0)             // if identified as weak stream, we need to reset
           trackers_l1[cpu][index].str_valid = 0;
   }

}

/**************************Updating confidence for the CS class****************/
int ipcp_l1::prefetcher::update_conf(int stride, int pred_stride, int conf)
{
    if(stride == pred_stride){             // use 2-bit saturating counter for confidence
        conf++;
        if(conf > 3)
            conf = 3;
    } else {
        conf--;
        if(conf < 0)
            conf = 0;
    }

    return conf;
}
