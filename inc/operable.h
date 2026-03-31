/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPERABLE_H
#define OPERABLE_H

// WL
#include <string>
#include <deque>
#include <unordered_set>
#include <tuple>
// WL

namespace champsim
{

class operable
{
public:
  const double CLOCK_SCALE;

  // WL

  const std::string L1I_name = "cpu0_L1I";
  const std::string L1D_name = "cpu0_L1D";
  const std::string L2C_name = "cpu0_L2C";
  const std::string LLC_name = "LLC";
  const std::string DTLB_name = "cpu0_DTLB";
  const std::string ITLB_name = "cpu0_ITLB";
  const std::string STLB_name = "cpu0_STLB";
  const std::string ORACLE_at = L2C_name;
  const std::string ORACLE_at_2nd = L2C_name;

  static uint64_t number_of_instructions_to_skip_before_log;

  static uint64_t cpu0_num_retired;

  static std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> lru_states;

  static std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> lru_states_L2C;
  // WL

  double leap_operation = 0;
  uint64_t current_cycle = 0;
  bool warmup = true;

  explicit operable(double scale) : CLOCK_SCALE(scale - 1) {}

  long _operate()
  {
    // skip periodically
    if (leap_operation >= 1) {
      leap_operation -= 1;
      return 0;
    }

    auto result = operate();

    leap_operation += CLOCK_SCALE;
    ++current_cycle;

    return result;
  }

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual long operate() = 0;
  virtual void begin_phase() {}       // LCOV_EXCL_LINE
  virtual void end_phase(unsigned) {} // LCOV_EXCL_LINE
  virtual void print_deadlock() {}    // LCOV_EXCL_LINE
};

} // namespace champsim

#endif
