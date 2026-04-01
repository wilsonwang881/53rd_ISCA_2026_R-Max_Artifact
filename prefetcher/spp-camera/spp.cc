#include "spp.h"
#include "cache.h"

#include <array>
#include <iostream>
#include <map>
#include <numeric>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

namespace {
  template <typename R>
    void increment_at(R& range, std::size_t idx)
    {
      while (std::size(range) <= idx)
        range.push_back(0);
      range.at(idx)++;
    }

  // Signature calculation parameters
  constexpr std::size_t SIG_SHIFT = 3;
  constexpr std::size_t SIG_BIT = 12;
  constexpr std::size_t SIG_DELTA_BIT = 2;

  uint32_t generate_signature(uint32_t last_sig, int32_t delta)
  {
    if (delta == 0)
      return last_sig;

    // Sign-magnitude representation
    int32_t pos_delta = (delta < 0) ? -1*delta : delta;
    int32_t sig_delta = ((pos_delta << 1) & ~champsim::bitmask(SIG_DELTA_BIT+1)) | (pos_delta & champsim::bitmask(SIG_DELTA_BIT)) | (((delta < 0) ? 1 : 0) << SIG_DELTA_BIT);

    return ((last_sig << SIG_SHIFT) ^ sig_delta) & champsim::bitmask(SIG_BIT);
  }
}

void spp::prefetcher::issue(CACHE* cache)
{
  // Issue eligible outstanding prefetches
  if (!std::empty(issue_queue)) {
    auto [addr, priority] = issue_queue.front();

    // If this fails, the queue was full.
    bool prefetched = cache->prefetch_line(addr, priority, 0);
    if (prefetched) {

      // WL: for recording issued SPP prefetches 
      struct PF pf{addr, cache->current_cycle};
      pf_acc.push_back(pf);

      if (pf_acc.size() > PF_ACC_THRESHOLD_LENGTH) {
        pf_acc_file.open(PF_ADDR_FILE_NAME, std::ofstream::app);

        for(auto var : pf_acc) 
          pf_acc_file << var.cycle << " " << var.addr << std::endl;

        pf_acc.clear();
        pf_acc_file.close();
      }
      
      // WL

      filter.update_issue(addr, cache->get_set(addr));
      issue_queue.pop_front();
    }
  }
}

void spp::prefetcher::update_demand(uint64_t addr, uint32_t set)
{
  filter.update_demand(addr, set);
}

void spp::prefetcher::step_lookahead()
{
  pattern_table.global_accuracy = warmup ? 0.95 : 1.0 * filter.pf_useful/filter.pf_issued;

  // Operate the pattern table
  if (active_lookahead.has_value()) {
    auto current_page = active_lookahead->address >> LOG2_PAGE_SIZE;
    auto current_depth = active_lookahead->depth;
    auto pattable_result = pattern_table.lookahead_step(active_lookahead->sig, active_lookahead->confidence, active_lookahead->depth);

    std::optional<pfqueue_entry_t> next_step;
    if (pattable_result.has_value()) {
      auto [next_delta, next_conf] = pattable_result.value();
      next_step = {::generate_signature(active_lookahead->sig, next_delta), next_delta, active_lookahead->depth + 1, next_conf, active_lookahead->address + (next_delta << LOG2_BLOCK_SIZE)};
    }

    if (!next_step.has_value()) {
      //STATS
      if (!warmup) {
        ::increment_at(depth_ptmiss_tracker, current_depth);
      }
    } else if (next_step->confidence < pattern_table.fill_threshold) {
      // The path has become unconfident
      //STATS
      if (!warmup) {
        ::increment_at(depth_confidence_tracker, current_depth);
      }

      next_step.reset();
    } else if ((next_step->address >> LOG2_PAGE_SIZE) != current_page) {
      // Prefetch request is crossing the physical page boundary
      bootstrap_table.update(next_step->address, next_step->sig, next_step->confidence, next_step->delta);

      //STATS
      if (!warmup) {
        ::increment_at(depth_offpage_tracker, current_depth);
      }

      next_step.reset(); // TODO should this kill the lookahead?
    } else if (std::size(issue_queue) < ISSUE_QUEUE_SIZE) {
      // Check the filter
      if (auto filter_check_result = filter.check(next_step->address); filter_check_result != spp::REJECT) {
        issue_queue.push_back({next_step->address, filter_check_result == spp::STRONGLY_ACCEPT});

        //STATS
        if (!warmup)
          sig_count_tracker[next_step->sig]++;
      }
      // continue even if filter rejects
    } else {
      // If the queue has no capacity, try again next cycle
      next_step = active_lookahead;
    }

    active_lookahead = next_step;
  }
}

void spp::prefetcher::initiate_lookahead(uint64_t base_addr)
{
  int confidence = (1<<ACCURACY_BITS) - 1;

  // Stage 1: Read and update a sig stored in ST
  auto sig_and_delta = signature_table.read(base_addr);
  if (!sig_and_delta.has_value()) {
    if (auto bst_returnval = bootstrap_table.check(base_addr); bst_returnval.has_value()) {
      auto [sig, conf, del] = *bst_returnval;
      sig_and_delta = {sig, del};
      confidence = conf;
    }
  }
  auto [last_sig, delta] = sig_and_delta.value_or(std::pair{0,0});

  // Stage 2: Update delta patterns stored in PT
  // If we miss the ST and BST, we skip this update, because the pattern won't be present
  if (sig_and_delta.has_value()) pattern_table.update_pattern(last_sig, delta);

  auto curr_sig = ::generate_signature(last_sig, delta);
  signature_table.update(base_addr, curr_sig);

  // Stage 3: Start prefetching
  if (sig_and_delta.has_value()) {
    //STATS
    if (!warmup) {
      if (active_lookahead.has_value()) {
        ::increment_at(depth_interrupt_tracker, active_lookahead->depth);
      }
    }

    active_lookahead = {curr_sig, delta, 0, confidence, base_addr & ~champsim::bitmask(LOG2_BLOCK_SIZE)};
  }
}

void spp::prefetcher::print_stats(std::ostream& ostr)
{
  /*
   * Histogram of signature counts
   */
  std::vector<unsigned> counts, acc_counts;
  std::for_each(std::begin(sig_count_tracker), std::end(sig_count_tracker), [&counts](auto x){
      ::increment_at(counts, x.second);
      });

  auto total = std::accumulate(std::begin(counts), std::end(counts), 0u);
  ostr << "Signature histogram:" << std::endl;
  ostr << "  total: " << total << std::endl;

  auto partial_sum = 0;
  for (auto c_it = std::begin(counts); c_it != std::end(counts); ++c_it) {
    partial_sum += *c_it;
    if (partial_sum > 0.1*total) {
      ostr << std::distance(std::begin(counts), c_it) << "\t:\t" << partial_sum << std::endl;
      partial_sum = 0;
    }
  }
  ostr << std::endl;

  /*
   * List of depths
   */
  ostr << "DEPTH (confidence):" << std::endl;
  unsigned i = 0;
  for (auto val : depth_confidence_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (interrupt):" << std::endl;
  i = 0;
  for (auto val : depth_interrupt_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (offpage):" << std::endl;
  i = 0;
  for (auto val : depth_offpage_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (pattern miss):" << std::endl;
  i = 0;
  for (auto val : depth_ptmiss_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;
}

