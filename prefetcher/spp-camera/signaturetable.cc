#include "spp.h"
#include "msl/bits.h"

#include <algorithm>

std::optional<std::pair<uint32_t, int32_t>> spp::SIGNATURE_TABLE::read(uint64_t addr)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_idx = page % SET;
  auto tag = (page / SET) & champsim::bitmask(TAG_BIT);

  auto set_begin = std::next(std::begin(sigtable), WAY * set_idx);
  auto set_end   = std::next(set_begin, WAY);

  // Try to find a hit in the set
  auto match_way = std::find_if(set_begin, set_end, [tag](const auto& x){ return x.valid && x.partial_page == tag; });

  if (match_way == set_end)
    return std::nullopt;

  match_way->last_used = ++access_count;
  return std::pair{match_way->sig, (signed)page_offset - (signed)match_way->last_offset};
}

void spp::SIGNATURE_TABLE::update(uint64_t addr, uint32_t sig)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_idx = page % SET;
  auto tag = (page / SET) & champsim::bitmask(TAG_BIT);

  auto set_begin = std::next(std::begin(sigtable), WAY * set_idx);
  auto set_end   = std::next(set_begin, WAY);

  // Try to find a hit in the set
  auto match_way = std::find_if(set_begin, set_end, [tag](const auto& x){ return x.valid && x.partial_page == tag; });

  if (match_way == set_end) {
    match_way = std::find_if_not(set_begin, set_end, [](const auto& x){ return x.valid; });
  }
  if (match_way == set_end) {
    match_way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.last_used < y.last_used; });
  }

  *match_way = {true, tag, page_offset, sig, ++access_count};
}

