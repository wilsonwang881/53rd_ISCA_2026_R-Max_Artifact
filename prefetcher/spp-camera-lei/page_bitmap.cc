#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way) {
  uint64_t set_begin = i - (i % way);

  for (size_t j = set_begin; j < set_begin + way; j++) 
    l[j].lru_bits++;

  for (size_t j = set_begin; j < set_begin + way; j++) {
    if (l[j].lru_bits >= 0x3FF) {
      for (size_t k = set_begin; k < set_begin + way; k++) {
        if (l[k].lru_bits > 2) 
          l[k].lru_bits = l[k].lru_bits - 2; 
        else
          l[k].lru_bits = 0;
      }

      break;
    } 
  }

  l[i].lru_bits = 0;
}

void spp::SPP_PAGE_BITMAP::update(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  this_round_pg_acc[page].ct_add_non_saturate(block);

  // 1. Check the table.
  current_page = page;

  // Assign the group_access change if there is more than 7 groups.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (tb[i].valid && tb[i].page_no == page) {
      lru_operate(tb, i, TABLE_WAY);

      if (page != previous_page && tb[i].remaining_this_group <= 0) {
        tb[i].group_access++;
        tb[i].remaining_this_group = 20;
      } 

      if(tb[i].group_access <= CAPACITY) {
        tb[i].block_indicate[block] = tb[i].group_access;
        tb[i].remaining_this_group--;
      }
      else if(tb[i].group_access > CAPACITY) {

        // Compact pairs of groups.
        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (tb[i].bitmap[j] && 
              (tb[i].block_indicate[j] == 1 || 
               tb[i].block_indicate[j] == 2)) 
            tb[i].block_indicate[j] = 1;
          else if (tb[i].bitmap[j] && tb[i].block_indicate[j] > 2) 
             tb[i].block_indicate[j] = tb[i].block_indicate[j] - 1;

        }

        tb[i].group_access = CAPACITY;
        tb[i].remaining_this_group = 20;
      }

      tb[i].ct_add(block);
      previous_page = page;

      return; // Table updated.
    }
  }

  // 2. Check the filter 
  if (filter_operate(addr))
    return; // Filter updated.
   
  // 3. Check the level 1 filter.
  lv1_filter_operate(addr);
}

void spp::SPP_PAGE_BITMAP::promote_filter(uint64_t addr, std::vector<PAGE_R> &lf, std::vector<PAGE_R> &hf) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;
  std::vector<uint64_t> l_blks = {block};

  // Get block from the lower level filter.
  for (size_t i = 0; i < lf.size(); i++) {
    if (lf[i].valid && lf[i].page_no == page) {
      for(size_t j = 0; j < BITMAP_SIZE; j++) {
        if (lf[i].bitmap[j]) 
          l_blks.push_back(j);
      }

      lf[i].rst();

      break;
    } 
  }

  previous_page = current_page;

  // Find an invalid entry in the upper filter.
  for (size_t i = 0; i < hf.size(); i++) {
    if (!hf[i].valid) {
      hf[i].populate_entry(page, l_blks);
      lru_operate(hf, i, hf.size());

      return;
    }
  }

  // All entries in the upper filter valid.
  // Find LRU entry.
  // Only promotion, no demotion.
  auto lru_el = std::max_element(hf.begin(), hf.begin() + hf.size(),
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 
  lru_el->populate_entry(page, l_blks);
  lru_operate(hf, lru_el - hf.begin(), hf.size());
  previous_page = current_page;
}

void spp::SPP_PAGE_BITMAP::evict(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Check the table.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].page_no == page &&
        tb[i].valid)
      tb[i].ct_minus(block);
  }

  // Check the filter.
  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].page_no == page && filter[i].valid) {
      if(tb[i].bitmap[block] > 0)
        tb[i].bitmap[block]=tb[i].bitmap[block]-1;
    }
  }
}

std::vector<std::tuple<uint64_t, bool, int8_t>> spp::SPP_PAGE_BITMAP::gather_pf(uint64_t asid) {
  cs_pf.clear();
  std::vector<std::tuple<uint64_t, bool,int8_t>> pf;
  uint64_t filter_sum = 0;
  //compare_truth();
  //print_page_access();
  //adjust_filter_threshold();

  if (STORAGE_LIMIT_MODE) {    
    for (size_t i = 0; i < TABLE_SIZE; i++) {
      if (tb[i].valid) {
        uint64_t page_addr = tb[i].page_no << 12;
        //std::cout << "Page: " << page_addr << ": ";

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (tb[i].bitmap[j]) {
            cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true, tb[i].block_indicate[j]));
            //std::cout << (unsigned)tb[i].block_indicate[j] << " ";
          }
        }

        //std::cout << std::endl;
      }
    }

    /*
    for (size_t i = 0; i < FILTER_SIZE; i++) {
      if (filter[i].valid) {
        uint64_t page_addr = filter[i].page_no << 12;
        //uint64_t blocks = std::reduce(std::begin(filter[i].bitmap), std::end(filter[i].bitmap), 0);

        //if (blocks >= 2) {
          for (size_t j = 0; j < BITMAP_SIZE; j++) {
            if (filter[i].bitmap[j]) {
              cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true, 1));
              filter_sum++;
            }
          } 
        //}
      } 
    }
    */
 
    std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches. " << filter_sum << " are from the filter." << std::endl;

    uint64_t valid_tb_entry = 0;

    for(auto &var : tb) {
      if (var.valid) 
        valid_tb_entry++; 

      var.accessed++;

      if (var.accessed > 2) 
        var.rst(); 
    }

    pf_metadata_limit = (valid_tb_entry * (36 + 64 * 1)+ 1024 * 36) / 8; 
    uint64_t remainder = pf_metadata_limit % 64;

    if (remainder != 0) 
      pf_metadata_limit = pf_metadata_limit + (64 - remainder);

    std::cout << "Pb: pf_metadata_limit " << pf_metadata_limit << " bytes " << 1.0 * pf_metadata_limit / 1024 << " KB."<< std::endl;
  }
  else {
    for(auto var : this_round_pg_acc) {
      for(size_t i = 0; i < BITMAP_SIZE; i++) {
        if (var.second.bitmap[i]) 
          cs_pf.push_back(std::make_tuple((var.first << 12 )+ (i << 6), true, 1));
      }
    }

    pf_metadata_limit = 1024 * 42 / 8;
    std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from unlimited storage from " << this_round_pg_acc.size() << " pages." << std::endl;
  }

  for(auto &lv1_e : lv1_filter) 
    lv1_e.rst(); 

  for(auto &filter_e : filter) 
    filter_e.rst(); 

  for(auto var : cs_pf)
    pf.push_back(var); 

  this_round_pg_acc.clear();

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
        promote_filter(addr, lv1_filter, filter);

      return;
    } 
  }

  // Allocate new entry in the lv1_filter.
  // If any invalid entry exists.
  for (size_t i = set * LV1_FILTER_WAY; i < (set + 1) * LV1_FILTER_WAY; i++) {
    if (!lv1_filter[i].valid) {
      lv1_filter[i].populate_entry(page, {block});
      lru_operate(lv1_filter, i, LV1_FILTER_WAY);

      return;
    } 
  }

  // If filter full, use LRU to replace.
  auto lru_el = std::max_element(lv1_filter.begin() + set * LV1_FILTER_WAY, lv1_filter.begin() + (set + 1) * LV1_FILTER_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->populate_entry(page, {block});
  lru_operate(lv1_filter, lru_el - lv1_filter.begin(), LV1_FILTER_WAY);

  return;
}

bool spp::SPP_PAGE_BITMAP::filter_operate(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid && filter[i].page_no == page) {
      filter[i].bitmap[block] = true;
      lru_operate(filter, i, FILTER_WAY);
      uint64_t accu = std::accumulate(filter[i].bitmap, filter[i].bitmap + BITMAP_SIZE, 0);

      if (accu > FILTER_THRESHOLD) 
        promote_filter(addr, filter, tb);

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

uint64_t spp::SPP_PAGE_BITMAP::tb_max(std::vector<PAGE_R> l) {
  uint64_t max = 0, max_index = 0;
  std::map<uint64_t, uint64_t> mx;

  // Find the table max.
  for (size_t i = 1; i <= BITMAP_SIZE; i++) 
    mx[i] = 0; 

  for(auto var : l) {
    if (var.valid) 
      mx[std::accumulate(var.bitmap, var.bitmap + BITMAP_SIZE, 0)]++;
  }

  for (size_t i = 1; i <= BITMAP_SIZE; i++) {
    if (mx[i] > max) {
      max = mx[i];
      max_index = i;
    } 
  }

  return max_index;
}

void spp::SPP_PAGE_BITMAP::adjust_filter_threshold() {
  uint64_t max_from_tb = tb_max(tb);

  /*
  if (max_from_tb > FILTER_THRESHOLD) 
    FILTER_THRESHOLD = FILTER_THRESHOLD + (max_from_tb - FILTER_THRESHOLD) / 2;
  else 
    FILTER_THRESHOLD = max_from_tb; //FILTER_THRESHOLD - (FILTER_THRESHOLD - max) / 2;
  */

  std::cout << "Pb: filter threshold adjusted to " << FILTER_THRESHOLD << std::endl;

  // Find the filter max.
  uint64_t max_from_ft = tb_max(filter);
  std::cout << "Filter max: " << max_from_ft << " table max: " << max_from_tb << std::endl;
}

