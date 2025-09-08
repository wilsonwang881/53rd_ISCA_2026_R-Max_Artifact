#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <vector>
#include <set>
#include <math.h>
#include "champsim_constants.h"
#include <cassert>

namespace spp {
  class SPP_PAGE_BITMAP {
    constexpr static uint64_t TABLE_SET = 1;
    constexpr static uint64_t LV1_FILTER_WAY = 512;
    constexpr static uint64_t LV1_FILTER_SIZE = TABLE_SET * LV1_FILTER_WAY;
    constexpr static uint64_t FILTER_WAY = 512;
    constexpr static std::size_t FILTER_SIZE = TABLE_SET * FILTER_WAY;
    constexpr static uint64_t TABLE_WAY = 512;
    constexpr static std::size_t TABLE_SIZE = TABLE_SET * TABLE_WAY;
    constexpr static std::size_t BITMAP_SIZE = 64;
    std::size_t FILTER_THRESHOLD = 10;
    constexpr static bool STORAGE_LIMIT_MODE = false;

    struct PAGE_R {
      bool valid = false;
      uint64_t accessed = 0;
      uint16_t lru_bits;
      uint64_t page_no;
      bool bitmap[BITMAP_SIZE] = {false};
      uint64_t col_access[BITMAP_SIZE / 8] = {0};
      uint64_t row_access[BITMAP_SIZE / 8] = {0};
      uint64_t acc_counter = 0;

      void rst() {
        valid = false;
        accessed = 0;
        lru_bits = 0;
        page_no = 0;

        for (size_t i = 0; i < BITMAP_SIZE; i++)
          bitmap[i] = false;

        for (size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          col_access[i] = 0;
          row_access[i] = 0;
        }

        acc_counter = 0;
      }

      void ct_check(uint64_t blk) {
        uint64_t row_blk = 0;
        uint64_t col_blk = 0;

        for (size_t k = (blk - blk % 8); k < ((blk - blk % 8) + 8); k++) 
          row_blk += bitmap[k]; 

        for (size_t k = blk % 8; k < 64; k += 8) 
          col_blk += bitmap[k]; 

        if (row_blk == 0)
          row_access[blk / 8] = 0; 

        if (col_blk == 0) 
          col_access[blk % 8] = 0;

        if (row_blk > row_access[blk / 8]) {
          std::cout << "row_blk " << row_blk << " row_access " << row_access[blk / 8] << std::endl; 
        }

        assert(row_blk <= row_access[blk / 8]);
        assert(col_blk <= col_access[blk % 8]);
      }

      void ct_add(uint64_t blk) {
        bitmap[blk] = true;
        row_access[blk / 8] = row_access[blk / 8] >= 0b11111 ? 0b11111 : (row_access[blk / 8] + 1);
        col_access[blk % 8] = col_access[blk % 8] >= 0b11111 ? 0b11111 : (col_access[blk % 8] + 1);
        acc_counter = acc_counter >= 0b11111111 ? 0b11111111 : (acc_counter + 1);
        accessed = 0;
        //ct_check(blk);
      }

      void populate_entry(uint64_t pg, std::vector<uint64_t> blks) {
        rst();
        valid = true;
        page_no = pg;

        for(auto var : blks) 
          ct_add(var); 
      }

      void ct_add_non_saturate(uint64_t blk) {
        bitmap[blk] = true;
        row_access[blk / 8]++;
        col_access[blk % 8]++;
        acc_counter++;
      }

      void ct_minus(uint64_t blk) {
        if (bitmap[blk]) {
          bitmap[blk] = false;
          row_access[blk / 8] = row_access[blk / 8] > 0 ? (row_access[blk / 8] - 1) : 0;
          col_access[blk % 8] = col_access[blk % 8] > 0 ? (col_access[blk % 8] - 1) : 0;
          acc_counter = acc_counter > 0 ? (acc_counter - 1) : 0;
        }

        ct_check(blk);
      }
    };

    public:

    uint64_t pf_metadata = 0;
    uint64_t pf_metadata_limit = 12 * 1024;

    std::map<uint64_t, PAGE_R> last_round_pg_acc;
    std::map<uint64_t, PAGE_R> this_round_pg_acc;

    std::vector<PAGE_R> lv1_filter = std::vector<PAGE_R>(LV1_FILTER_SIZE);
    std::vector<PAGE_R> filter = std::vector<PAGE_R>(FILTER_SIZE);
    std::vector<PAGE_R> tb = std::vector<PAGE_R>(TABLE_SIZE);

    std::deque<std::pair<uint64_t, bool>> cs_pf;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;

    void lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way);
    void update(uint64_t addr);
    void promote_filter(uint64_t addr, std::vector<PAGE_R> &lf, std::vector<PAGE_R> &hf);
    void evict(uint64_t addr);
    std::vector<std::pair<uint64_t, bool>> gather_pf(uint64_t asid);
    void lv1_filter_operate(uint64_t);
    bool filter_operate(uint64_t addr);
    void update_usefulness(uint64_t addr);
    uint64_t calc_set(uint64_t addr);
    void print_page_access();
    void compare_truth();
    uint64_t tb_max(std::vector<PAGE_R> l);
    void adjust_filter_threshold();
  };
}

