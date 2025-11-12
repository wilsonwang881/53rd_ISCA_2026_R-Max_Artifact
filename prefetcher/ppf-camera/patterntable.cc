#include "spp.h"

void spp::PATTERN_TABLE::update_pattern(uint32_t last_sig, int curr_delta)
{
  auto set_begin = std::begin(pattable[last_sig % SET].ways);
  auto set_end   = std::end(pattable[last_sig % SET].ways);
  auto &c_sig    = pattable[last_sig % SET].c_sig;

  constexpr auto c_sig_max = (1u << C_SIG_BIT)-1;
  constexpr auto c_delta_max = (1u << C_DELTA_BIT)-1;

  // If the signature counter overflows, divide all counters by 2
  if (c_sig >= c_sig_max || std::any_of(set_begin, set_end, [c_delta_max](auto x){ return x.c_delta >= c_delta_max; })) {
    for (auto it = set_begin; it != set_end; ++it)
      it->c_delta /= 2;
    c_sig /= 2;
  }

  // Check for a hit
  if (auto way = std::find_if(set_begin, set_end, [curr_delta](auto x){ return x.valid && (x.delta == curr_delta); }); way != set_end) {
    way->c_delta++;
  } else {
    // Find an invalid way
    way = std::find_if_not(set_begin, set_end, [](auto x){ return x.valid; });
    if (way == set_end)
      way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.c_delta < y.c_delta; }); // Find the smallest delta counter
    *way = pattable_entry_t(curr_delta);
  }

  c_sig++;
}

int mult_conf(double alpha, int lastconf, unsigned c_delta, unsigned c_sig, unsigned depth)
{
  return alpha * lastconf * c_delta / (c_sig + depth);
}

std::optional<std::pair<int, int>> spp::PATTERN_TABLE::lookahead_step(uint32_t sig, int confidence, uint32_t depth)
{
  auto set_begin = std::cbegin(pattable[sig % SET].ways);
  auto set_end   = std::cend(pattable[sig % SET].ways);
  auto c_sig     = pattable[sig % SET].c_sig;

  // Abort if the signature count is zero
  if (c_sig == 0)
    return std::nullopt;

  auto max_conf_way = std::max_element(set_begin, set_end, [](auto x, auto y){ return !x.valid || (y.valid && x.c_delta < y.c_delta); });

  if (!max_conf_way->valid)
    return std::nullopt;

  return std::pair{max_conf_way->delta, mult_conf(global_accuracy, confidence, max_conf_way->c_delta, c_sig, depth)};
}

