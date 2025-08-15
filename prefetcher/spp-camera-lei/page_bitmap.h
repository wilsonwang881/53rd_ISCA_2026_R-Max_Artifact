#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include "champsim_constants.h"
#include <cassert>

namespace spp {
  class SPP_PAGE_BITMAP {
    constexpr static uint64_t TABLE_SET = 1;
    constexpr static uint64_t TABLE_WAY = 512;
    constexpr static std::size_t TABLE_SIZE = TABLE_SET * TABLE_WAY;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static uint64_t FILTER_WAY = 512;
    constexpr static std::size_t FILTER_SIZE = TABLE_SET * FILTER_WAY;
    constexpr static bool PAGE_BITMAP_DEBUG_PRINT = false;
    constexpr static bool RECORD_PAGE_ACCESS = true;
    constexpr static bool READ_PAGE_ACCESS = false;
    std::size_t FILTER_THRESHOLD = 10;
    std::string PAGE_BITMAP_ACCESS = "pb_acc.txt";
    std::fstream pb_file;

    struct PAGE_R {
      bool valid = false;
      uint16_t lru_bits;
      uint64_t page_no;
      uint64_t page_no_store;
      bool bitmap[BITMAP_SIZE] = {false};
      std::array<bool, BITMAP_SIZE> block;
      bool bitmap_store[BITMAP_SIZE] = {false};
      uint64_t col_access[BITMAP_SIZE / 8] = {0};
      uint64_t row_access[BITMAP_SIZE / 8] = {0};
      uint64_t acc_counter = 0;

      void rst() {
        valid = false;
        lru_bits = 0;
        page_no = 0;
        page_no_store = 0;

        for (size_t i = 0; i < BITMAP_SIZE; i++) {
          bitmap[i] = false;
          bitmap_store[i] = false;
        }

        for (size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          col_access[i] = 0;
          row_access[i] = 0;
        }

        acc_counter = 0;
      }

      uint64_t saturating_counter(uint64_t before, uint64_t threshold) {
        return before >= threshold ? threshold : (before+1);
      }

      void row_ct_add(uint64_t blk) {
        row_access[blk / 8] = saturating_counter(row_access[blk / 8], 0b11111);
      }

      void col_ct_add(uint64_t blk) {
        col_access[blk % 8] = saturating_counter(col_access[blk % 8], 0b11111);
      }

      void acc_ct_add() {
        acc_counter = saturating_counter(acc_counter, 0b11111111);
      }
      
      void ct_check(uint64_t blk) {
        uint64_t row_blk = 0;
        uint64_t col_blk = 0;

        for (size_t k = (blk - blk % 8); k < ((blk - blk % 8) + 8); k++) 
          row_blk += bitmap[k]; 

        for (size_t k = blk % 8; k < 64; k += 8) {
          if (bitmap[k]) 
            col_blk += bitmap[k]; 
        }

        if (row_access[blk / 8] > 0) 
          assert(row_blk > 0); 

        if (col_access[blk % 8] > 0) 
          assert(col_blk > 0);

        assert(row_blk <= row_access[blk / 8]);
        assert(col_blk <= col_access[blk % 8]);
      }

      void ct_add(uint64_t blk) {
        bitmap[blk] = true;
        row_ct_add(blk);
        col_ct_add(blk);
        acc_ct_add();
        ct_check(blk);
      }
    };

    public:

    uint64_t pf_metadata = 0;
    uint64_t pf_metadata_limit = 12 * 1024;

    std::map<uint64_t, PAGE_R> last_round_pg_acc;
    std::map<uint64_t, PAGE_R> this_round_pg_acc;

    std::map<uint64_t, std::map<uint64_t, std::array<bool, BITMAP_SIZE>>> pb_acc;

    std::vector<PAGE_R> tb = std::vector<PAGE_R>(TABLE_SIZE);
    std::vector<PAGE_R> filter = std::vector<PAGE_R>(FILTER_SIZE);

    std::deque<std::pair<uint64_t, bool>> cs_pf;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;

    void pb_file_read();
    void pb_file_write(uint64_t asid);
    void lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way);
    void update(uint64_t addr);
    void evict(uint64_t addr);
    void update_bitmap_store();
    std::vector<std::pair<uint64_t, bool>> gather_pf(uint64_t asid);
    bool filter_operate(uint64_t addr);
    void update_usefulness(uint64_t addr);
    uint64_t calc_set(uint64_t addr);
    void print_page_access();
  };
}

