#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way) {
  uint64_t set_begin = i - (i % way);

  for (size_t j = set_begin; j < set_begin + way; j++) {
    if (l[j].lru_bits >= 0x7FF) {
      for (size_t k = set_begin; k < set_begin + way; k++) {
        if (l[k].lru_bits > 2) 
          l[k].lru_bits = l[k].lru_bits - 2; 
        else
          l[k].lru_bits = 0;
      }

      break;
    } 
  }

  for (size_t j = set_begin; j < set_begin + way; j++) 
    l[j].lru_bits++;

  l[i].lru_bits = 0;
}

void spp::SPP_PAGE_BITMAP::update(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  this_round_pg_acc[page].ct_add_non_saturate(block);

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (tb[i].valid && tb[i].page_no == page) {
      lru_operate(tb, i, TABLE_WAY);
      tb[i].ct_add(block);

      return; // Table updated.
    }
  }

  // Page not found.
  // Check or update filter first.
  bool check_filter = filter_operate(addr);

  if (check_filter)
    return; // Filter updated.
  
  lv1_filter_operate(addr);
}

void spp::SPP_PAGE_BITMAP::promote_from_filter_to_tb(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Allocate new entry for the new page with more than threshold number of blocks.
  std::vector<uint64_t> filter_blks = {block};
  uint64_t filter_index = 0;

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid && filter[i].page_no == page) {
      for(size_t j = 0; j < BITMAP_SIZE; j++) {
        if (filter[i].bitmap[j]) 
          filter_blks.push_back(j);
      }

      filter[i].rst();
      filter_index = i;
      break;
    } 
  }

  // Find an invalid entry for the page.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (!tb[i].valid) {
      tb[i].rst();
      tb[i].valid = true;
      tb[i].page_no = page;

      for(auto var : filter_blks) 
        tb[i].ct_add(var);

      lru_operate(tb, i, TABLE_WAY);

      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  auto lru_el = std::max_element(tb.begin() + set * TABLE_WAY, tb.begin() + (set + 1) * TABLE_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  PAGE_R tmpp = *lru_el;
  /*
  filter[filter_index] = tmpp;
  filter[filter_index].valid = true;
  filter[filter_index].lru_bits = 0;
  */
  filter[filter_index].rst();
  lru_el->rst();
  lru_el->valid = true;
  lru_el->page_no = page;

  for(auto var : filter_blks) 
    lru_el->ct_add(var);

  lru_operate(tb, lru_el - tb.begin(), TABLE_WAY);
}

void spp::SPP_PAGE_BITMAP::promote_from_lv1_filter_to_filter(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Allocate new entry for the new page with more than threshold number of blocks.
  std::vector<uint64_t> lv1_filter_blks = {block};
  uint64_t lv1_filter_index = 0;

  for (size_t i = set * LV1_FILTER_WAY; i < (set + 1) * LV1_FILTER_WAY; i++) {
    if (lv1_filter[i].valid && lv1_filter[i].page_no == page) {
      for(size_t j = 0; j < BITMAP_SIZE; j++) {
        if (lv1_filter[i].bitmap[j]) 
          lv1_filter_blks.push_back(j);
      }

      lv1_filter[i].rst();
      lv1_filter_index = i;
      break;
    } 
  }

  // Find an invalid entry in the filter.
  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (!filter[i].valid) {
      filter[i].rst();
      filter[i].valid = true;
      filter[i].page_no = page;

      for(auto var : lv1_filter_blks) 
        filter[i].bitmap[var] = true;

      lru_operate(filter, i, FILTER_WAY);

      return;
    }
  }

  // All filter entries valid.
  // Find LRU entry.
  auto lru_el = std::max_element(filter.begin() + set * FILTER_WAY, filter.begin() + (set + 1) * FILTER_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  PAGE_R tmpp = *lru_el;
  lv1_filter[lv1_filter_index].valid = false;
  lv1_filter[lv1_filter_index].lru_bits = 0;
  lru_el->rst();
  lru_el->page_no = page;

  for(auto var : lv1_filter_blks) 
    lru_el->ct_add(var);

  lru_operate(filter, lru_el - filter.begin(), FILTER_WAY);
}

void spp::SPP_PAGE_BITMAP::evict(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Check tb first.
  /*
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].page_no == page &&
        tb[i].valid)
      tb[i].ct_minus(block);
  }
  */

  // Check filter.
  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].page_no == page &&
        filter[i].valid) 
      filter[i].bitmap[block] = false;
  }
}

std::vector<std::pair<uint64_t, bool>> spp::SPP_PAGE_BITMAP::gather_pf(uint64_t asid) {
  cs_pf.clear();
  std::vector<std::pair<uint64_t, bool>> pf;
  uint64_t filter_sum = 0;
  compare_truth();
  //adjust_filter_threshold();
  this_round_pg_acc.clear();

  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid) {
      uint64_t page_addr = tb[i].page_no << 12;
      
      for (size_t j = 0; j < BITMAP_SIZE; j++) {
        if (tb[i].bitmap[j]) {
          bool pf_check_row = tb[i].row_access[j / 8] < (tb[i].acc_counter >> 3);
          bool pf_check_col = tb[i].col_access[j % 8] < (tb[i].acc_counter >> 3);

          // Accumulate the number of blocks accessed in the row.
          uint64_t row_blk = 0;

          for (size_t k = (j - j % 8); k < ((j - j % 8) + 8); k++) 
            row_blk += tb[i].bitmap[k]; 

          // Accumulate the number of blocks accesses in the column.
          uint64_t col_blk = 0;

          for (size_t k = j % 8; k < 64; k += 8) 
              col_blk += tb[i].bitmap[k]; 

          if (tb[i].row_access[j / 8] > 0) 
            assert(row_blk > 0); 

          if (tb[i].col_access[j % 8] > 0) 
            assert(col_blk > 0);

          assert(row_blk <= tb[i].row_access[j / 8]);
          assert(col_blk <= tb[i].col_access[j % 8]);

          if (!((pf_check_row && pf_check_col) || (row_blk == tb[i].row_access[j / 8] || col_blk == tb[i].col_access[j % 8])))
            cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
        }
      }
    }
  }

  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].valid) {
      uint64_t page_addr = filter[i].page_no << 12;
      uint64_t blocks = std::reduce(std::begin(filter[i].bitmap), std::end(filter[i].bitmap), 0);
      
      if (blocks >= 2) {
        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (filter[i].bitmap[j]) {
            cs_pf.push_back(std::make_pair(page_addr + (j << 6), true));
            filter_sum++;
          }
        } 
      }
    } 
  }

  std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;
  std::cout << "Filter prefetches: " << filter_sum << std::endl;

  uint64_t valid_tb_entry = 0;

  for(auto var : tb) {
    if (var.valid) 
      valid_tb_entry++; 
  }

  pf_metadata_limit = (valid_tb_entry * 100 + 1024 * 42) / 8; 
  uint64_t remainder = pf_metadata_limit % 64;

  if (remainder != 0) 
    pf_metadata_limit = pf_metadata_limit + (64 - remainder);

  std::cout << "Pb: pf_metadata_limit " << pf_metadata_limit << " bytes " << 1.0 * pf_metadata_limit / 1024 << " KB."<< std::endl;

  for(auto &lv1_e : lv1_filter) 
    lv1_e.rst(); 

  for(auto &filter_e : filter) 
    filter_e.rst(); 

  for(auto var : cs_pf)
    pf.push_back(var); 

  return pf;
}

void spp::SPP_PAGE_BITMAP::lv1_filter_operate(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  for (size_t i = 0; i < LV1_FILTER_SIZE; i++) {
    if (lv1_filter[i].valid && lv1_filter[i].page_no == page) {
      lv1_filter[i].bitmap[block] = true;
      lru_operate(lv1_filter, i, LV1_FILTER_WAY);
      uint64_t accu = std::accumulate(lv1_filter[i].bitmap, lv1_filter[i].bitmap + BITMAP_SIZE, 0);

      if (accu > 2) 
        promote_from_lv1_filter_to_filter(addr); 

      return;
    } 
  }

  // Allocate new entry in the lv1_filter.
  // If any invalid entry exists.
  for (size_t i = set * LV1_FILTER_WAY; i < (set + 1) * LV1_FILTER_WAY; i++) {
    if (!lv1_filter[i].valid) {
      lv1_filter[i].rst();
      lv1_filter[i].valid = true;
      lv1_filter[i].page_no = page;
      lv1_filter[i].bitmap[block] = true;
      lru_operate(lv1_filter, i, LV1_FILTER_WAY);
      //std::cout << "Allocated in lv1 filter i = " << i << std::endl;

      return;
    } 
  }

  // If filter full, use LRU to replace.
  auto lru_el = std::max_element(lv1_filter.begin() + set * LV1_FILTER_WAY, lv1_filter.begin() + (set + 1) * LV1_FILTER_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->rst();
  lru_el->page_no = page;
  lru_el->valid = true;
  lru_el->bitmap[block] = true;
  lru_operate(lv1_filter, lru_el - lv1_filter.begin(), LV1_FILTER_WAY);
  //std::cout << "replaced" << std::endl;

  return;
}

bool spp::SPP_PAGE_BITMAP::filter_operate(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid &&
        filter[i].page_no == page) {
      filter[i].bitmap[block] = true;
      lru_operate(filter, i, FILTER_WAY);
      uint64_t accu = std::accumulate(filter[i].bitmap, filter[i].bitmap + BITMAP_SIZE, 0);

      if (accu > FILTER_THRESHOLD) 
        promote_from_filter_to_tb(addr); 

      return true;
    } 
  }

  return false;
}

void spp::SPP_PAGE_BITMAP::update_usefulness(uint64_t addr) {
  if (issued_cs_pf.find((addr >> 6) << 6) != issued_cs_pf.end()) {
    issued_cs_pf_hit++; 
    issued_cs_pf.erase((addr >> 6) << 6);
  }
}

uint64_t spp::SPP_PAGE_BITMAP::calc_set(uint64_t addr) {
  return (addr >> 6) & champsim::bitmask(champsim::lg2(TABLE_SET));
}

void spp::SPP_PAGE_BITMAP::print_page_access() {
  uint64_t same_pg_cnt = 0;

  for(auto pair : this_round_pg_acc) {

    uint64_t accu = std::accumulate(pair.second.bitmap, pair.second.bitmap + BITMAP_SIZE, 0);

    if (auto search = last_round_pg_acc.find(pair.first); search != last_round_pg_acc.end()) {
      if (accu > FILTER_THRESHOLD) {
        same_pg_cnt++;
        std::cout << "Page " << pair.first << " match, accesses: " << last_round_pg_acc[pair.first].acc_counter << "/" << this_round_pg_acc[pair.first].acc_counter << " last/this" << std::endl;

        for (size_t i = 0; i < BITMAP_SIZE / 8; i++) 
          std::cout << std::setw(4) << std::right << last_round_pg_acc[pair.first].col_access[i] << "/" << std::setw(4) << std::left << this_round_pg_acc[pair.first].col_access[i]; 

        std::cout << std::endl;

        for (std::size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          for (size_t j = 0; j < BITMAP_SIZE / 8; j++)  {
            std::cout << "   " << (last_round_pg_acc[pair.first].bitmap[i * 8 + j] ? "\u25FC" : "\u25FB") << "/";
            std::cout << (pair.second.bitmap[i * 8 + j] ? "\u25FC" : "\u25FB") << "   ";
          }

          std::cout << std::setw(4) << std::right << last_round_pg_acc[pair.first].row_access[i] << "/" << this_round_pg_acc[pair.first].row_access[i] << std::endl; 
        }

        /*
        std::cout << "last round" << std::endl;

        for (std::size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          for (size_t j = 0; j < BITMAP_SIZE / 8; j++) 
            std::cout << (pair.second.block[i * 8 + j] ? "\u25FC" : "\u25FB");
          
          std::cout << std::setw(5) << this_round_pg_acc[pair.first].row_access[i] << " " << std::endl;
        }

        std::cout << "this round " << std::endl;
        */
      }
    } 
  }

  last_round_pg_acc = this_round_pg_acc;
  this_round_pg_acc.clear();
  std::cout << "Page bitmap same page: " << same_pg_cnt << std::endl;
}

void spp::SPP_PAGE_BITMAP::compare_truth() {
  uint64_t same_pg = 0;
  uint64_t same_blk = 0;
  uint64_t pg_blk = 0;
  uint64_t total_pg = this_round_pg_acc.size();
  uint64_t total_blk = 0;
  uint64_t more_than_threshold_pg = 0;
  std::map<uint64_t, uint64_t> pg_dist;
  std::map<uint64_t, uint64_t> tb_dist;

  for (size_t i = 1; i <= BITMAP_SIZE; i++) {
    pg_dist[i] = 0; 
    tb_dist[i] = 0;
  }

  for(auto truth: this_round_pg_acc) {
    for(auto pb : tb) {
      if (pb.page_no == truth.first) {
        same_pg++; 
        pg_blk = std::accumulate(pb.bitmap, pb.bitmap + BITMAP_SIZE, 0);
        same_blk += pg_blk;
        tb_dist[pg_blk]++;
      }
    } 

    uint64_t blk_cnt = std::accumulate(truth.second.bitmap, truth.second.bitmap + BITMAP_SIZE, 0);
    pg_dist[blk_cnt]++;

    if (blk_cnt > FILTER_THRESHOLD) 
      more_than_threshold_pg++; 

    total_blk += blk_cnt;
  }

  std::cout << "Pb: same page: " << same_pg << " more than threshold page: " << more_than_threshold_pg << " total page: " << total_pg << std::endl;
  std::cout << "Pb: same blk: " << same_blk << " total blk: " << total_blk << " average no. blocks per recorded page: " << 1.0 * same_blk / same_pg << std::endl;

  for (size_t i = 1; i <= BITMAP_SIZE; i++) {
    std::cout << "No. blocks: " << i << "->"; 

    for (size_t j = 0; j < std::log(pg_dist[i]) * 10; j++) 
      std::cout << "-";

    std::cout << pg_dist[i] << std::endl;
  }

  std::cout << "============================" << std::endl;

  for (size_t i = 1; i <= BITMAP_SIZE; i++) {
    std::cout << "No. blocks: " << i << "->"; 

    for (size_t j = 0; j < std::log(tb_dist[i]) * 10; j++) 
      std::cout << "-";

    std::cout << tb_dist[i] << std::endl;
  }
}

void spp::SPP_PAGE_BITMAP::adjust_filter_threshold() {
  uint64_t max = 0;
  uint64_t max_index = 0;
  std::map<uint64_t, uint64_t> pk_mx;

  // Find the table max.
  for (size_t i = 1; i <= BITMAP_SIZE; i++) 
    pk_mx[i] = 0; 

  for(auto var : tb) {
    if (var.valid) 
      pk_mx[std::accumulate(var.bitmap, var.bitmap + BITMAP_SIZE, 0)]++;
  }

  for (size_t i = 1; i <= BITMAP_SIZE; i++) {
    if (pk_mx[i] > max) {
      max = pk_mx[i];
      max_index = i;
    } 
  }

  if (max_index > FILTER_THRESHOLD) 
    FILTER_THRESHOLD = FILTER_THRESHOLD + (max_index - FILTER_THRESHOLD) / 2;
  else 
    FILTER_THRESHOLD = max_index; //FILTER_THRESHOLD - (FILTER_THRESHOLD - max) / 2;

  std::cout << "Pb: filter threshold adjusted to " << FILTER_THRESHOLD << std::endl;

  // Find the filter max.
  max = 0;
  max_index = 0;
  std::map<uint64_t, uint64_t> ft_mx;

  for (size_t i = 1; i <= BITMAP_SIZE; i++) 
    ft_mx[i] = 0; 

  for(auto var : filter) {
    if (var.valid) 
      ft_mx[std::accumulate(var.bitmap, var.bitmap + BITMAP_SIZE, 0)]++;
  }

  for (size_t i = 2; i <= BITMAP_SIZE; i++) {
    if (ft_mx[i] > max) {
      max = ft_mx[i];
      max_index = i;
    } 
  }

  std::cout << "Filter max: " << max_index << std::endl;
}

