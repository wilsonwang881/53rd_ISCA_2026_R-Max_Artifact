#include "filter.h"

#include <algorithm>
#include <cassert>
//#include "spp.h"//HL
//#include "util.h"
//#include "ppf.h"//HL
#include "msl/bits.h"

auto spp::SPP_PREFETCH_FILTER::check(uint64_t check_addr, int confidence) const -> confidence_t
{
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way != set_end && match_way->prefetched.test(line_no))
    return REJECT;

  return confidence >= highconf_threshold ? STRONGLY_ACCEPT : WEAKLY_ACCEPT;
}

void spp::SPP_PREFETCH_FILTER::update_demand(uint64_t check_addr, std::size_t)
{
  /*
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
  */
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way != set_end)
  {
    //HL add the first boolean HL
    SAME_PAGE=true;

    if (match_way->prefetched.test(line_no) && !match_way->used[line_no])
    {
      //Add the second boolean
      PREFETCHED=true;

      pf_useful++;
      /*
      //HL
      useful[quotient] = 1;
      //move this part to the PPF.h with the valid[quotient]
      if (valid[quotient]) {
          // Prefetch leads to a demand hit
          PERC.perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 1, perc_sum[quotient]);

          // Histogramming Idea
          int32_t perc_sum_shifted = perc_sum[quotient] + (PERC_COUNTER_MAX+1)*PERC_FEATURES; 
          int32_t hist_index = perc_sum_shifted / 10;
      }
      
      //HL
      */
    if (pf_useful >= (1u << spp::GLOBAL_COUNTER_BITS)) {
      pf_useful /= 2;
      pf_issued /= 2;
    }

    match_way->used.set(line_no);

    // Update LRU
    match_way->last_used = ++access_count;
    }
    else
    {
      PREFETCHED=false;
    }

    /*
    if (!(valid[quotient] && remainder_tag[quotient] == remainder)) {//may need t0 change
        // AND If Rejected by Perc
        if (valid_reject[quotient_reject] && remainder_tag_reject[quotient_reject] == remainder_reject) {
          //std::cout<<"The train_neg is"<<train_neg<<std::endl;
          if (train_neg) {
            // Not prefetched but could have been a good idea to prefetch
            PERC.perc_update(address_reject[quotient_reject], pc_reject[quotient_reject], pc_1_reject[quotient_reject], pc_2_reject[quotient_reject], pc_3_reject[quotient_reject], delta_reject[quotient_reject], last_signature_reject[quotient_reject], cur_signature_reject[quotient_reject], confidence_reject[quotient_reject], la_depth_reject[quotient_reject], 0, perc_sum_reject[quotient_reject]);
            //std::cout<<"train_neg of perc_update"<<std::endl;
            valid_reject[quotient_reject] = 0;
            remainder_tag_reject[quotient_reject] = 0;
            // Printing Stats
            //::GHR.reject_update++;//HL ////////do I need to recover this ?
            reject_update++;
          }
        }
      }
      */
  }
  else
  {
    SAME_PAGE=false;
  }
}

void spp::SPP_PREFETCH_FILTER::update_issue(uint64_t check_addr, std::size_t)
{
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way == set_end)
  {
    match_way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.last_used < y.last_used; });
    match_way->prefetched.reset();
    match_way->used.reset();
  }

  assert(line_no < match_way->prefetched.size());
  match_way->page_no = page_no;
  match_way->prefetched.set(line_no);
  match_way->used.reset(line_no);

  // Update LRU
  match_way->last_used = ++access_count;

  pf_issued++;

  if (pf_issued >= (1u << spp::GLOBAL_COUNTER_BITS)) {
    pf_useful /= 2;
    pf_issued /= 2;
  }
}
/*
//HL need to add the prefetch evict
//HL need to add the add the spp perc reject

bool spp::SPP_PREFETCH_FILTER::update_perc(uint64_t check_addr, uint64_t base_addr, uint64_t ip,uint64_t ip_1,uint64_t ip_2,uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth,PERCEPTRON & PERC)
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
  case SPP_PERC_REJECT:
    //std::cout<<"enter filter_reject"<<std::endl;
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

      }
      break;

  case L2C_EVICT:

      // Decrease global pf_useful counter when there is a useless prefetch (prefetched but not used)
      if (valid[quotient] && !useful[quotient]) {
        if (pf_useful)//HL 
            pf_useful--;//HL
 

        // Prefetch leads to eviction
        //std::cout<<"evict occurs"<<std::endl;
        PERC.perc_update(address[quotient], pc[quotient], pc_1[quotient], pc_2[quotient], pc_3[quotient], delta[quotient], last_signature[quotient], cur_signature[quotient], confidence[quotient], la_depth[quotient], 0, perc_sum[quotient]);
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

//HL add_to_filter
bool spp::SPP_PREFETCH_FILTER::add_to_filter(uint64_t check_addr, uint64_t base_addr, uint64_t ip, uint64_t ip_1, uint64_t ip_2, uint64_t ip_3,FILTER_REQUEST filter_request, int cur_delta, uint32_t last_sig, uint32_t curr_sig, uint32_t conf, int32_t sum, uint32_t depth)
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

      break;

    case SPP_LLC_PREFETCH:
      // NOTE: SPP_LLC_PREFETCH has relatively low confidence (FILL_THRESHOLD <= SPP_LLC_PREFETCH < PF_THRESHOLD) 
      // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
      // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
      // we can get this cache line immediately from the LLC (not from DRAM)
      // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH

      //valid[quotient] = 1;
      //useful[quotient] = 0;

      break;
    default:
      std::cout << "[FILTER] Invalid filter request type: " << filter_request << std::endl;
      assert(0);
  }
  return true;
}
*/