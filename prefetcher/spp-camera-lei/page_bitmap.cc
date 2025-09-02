#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::pb_file_read() {
  if (!READ_PAGE_ACCESS) 
    return; 

  std::ifstream file(PAGE_BITMAP_ACCESS);

  if (!file.good()) 
    return; 

  pb_file.open(PAGE_BITMAP_ACCESS, std::ifstream::in);
  uint64_t asid, pg_no;

  while(!pb_file.eof()) {
    pb_file >> asid >> pg_no;

    if (pg_no == 0 && asid == 0)
      break; 

    for (size_t i = 0; i < BITMAP_SIZE; i++) 
      pb_file >> pb_acc[asid][pg_no][i]; 
  }

  pb_file.close();
  pb_file.open(PAGE_BITMAP_ACCESS, std::ofstream::out | std::ofstream::trunc);
  std::cout << "Done reading page bitmap file." << std::endl;
  pb_file.close();
}

void spp::SPP_PAGE_BITMAP::pb_file_write(uint64_t asid) {
  if (!READ_PAGE_ACCESS) 
    return; 

  pb_file.open(PAGE_BITMAP_ACCESS, std::ofstream::app);

  for(auto pair : this_round_pg_acc) {
    uint64_t accu = std::accumulate(pair.second.block.begin(), pair.second.block.end(), 0);

    if (accu > FILTER_THRESHOLD) {
      pb_file << asid << " " << pair.first;

      for (size_t i = 0; i < BITMAP_SIZE; i++) 
        pb_file << (pair.second.block[i] ? "\u25FC" : "\u25FB"); 

      pb_file << std::endl;
    }
  }

  pb_file.close();
}

void spp::SPP_PAGE_BITMAP::lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way) {
  uint64_t set_begin = i - (i % way);

  for (size_t j = set_begin; j < set_begin + way; j++) {
    if (l[j].lru_bits >=  0x3FF) {
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

  if (RECORD_PAGE_ACCESS) {
    this_round_pg_acc[page].block[block] = true;
    this_round_pg_acc[page].total_access++;
    this_round_pg_acc[page].row_access[block / 8]++; 
    this_round_pg_acc[page].col_access[block % 8]++;

  }

  //HL
  //if(this_round_pg_acc[page].block[block] ==true)
  //{
    //this_round_pg_acc[page].row_number[block/8]++;
  //}

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.

  //HL
  //check whether or not there is the change of page
  current_page=page;

  for (size_t i=set*TABLE_WAY;i<(set+1)*TABLE_WAY;i++)
  {
    if(tb[i].valid && tb[i].page_no == previous_page && previous_page != current_page)
    {
      tb[i].group_access++;
    }
    if(tb[i].group_access>CAPACITY)
    {
      for(size_t x=0;x<BITMAP_SIZE;x++)
      {
        //merge the group
        if(tb[i].block_indicate[x]>((tb[i].group_access)%CAPACITY))
        {
          tb[i].block_indicate[x]=tb[i].block_indicate[x]-1;
        }       
      }
    }
  }
  //HL
  //assign the group_access change if there is more than 7 groups


  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (tb[i].valid && tb[i].page_no == page) {
      
      //tb[i].bitmap[block] = true;//WL
      //HL
      if(tb[i].bitmap[block]<COUNT_MAX)
      {
        tb[i].bitmap[block]=tb[i].bitmap[block]+1;
      }
      if(tb[i].bitmap[block]==COUNT_MAX)
      {
        tb[i].saturated_bit=true;
      }
      //HL

      //HL
      tb[i].total_access++;
      tb[i].row_access[block/8]++;
      tb[i].col_access[block%8]++;
      //HL

      //HL
      //assign the group_number
      if(tb[i].group_access<=CAPACITY)
        tb[i].block_indicate[block]=tb[i].group_access;
      else if(tb[i].group_access>CAPACITY)
      {
        tb[i].block_indicate[block]=CAPACITY;
      }
      tb[i].first_access=false;
      //HL

      lru_operate(tb, i, TABLE_WAY);
      tb[i].acc_counter++;
      previous_page=current_page;//HL
      return;
    }
  }

  // Page not found.
  // Check or update filter first.
  bool check_filter = filter_operate(addr);

  if (!check_filter)
    return; 

  // Allocate new entry for the new page with more than threshold number of blocks.
  std::vector<uint64_t> filter_blks = {block};

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid && filter[i].page_no == page) {
      for(size_t j = 0; j < BITMAP_SIZE; j++) {
        if (filter[i].bitmap[j]) 
          filter_blks.push_back(j);

        filter[i].bitmap[j] = false;
      }

      filter[i].valid = false;
      break;
    } 
  }

  // Find an invalid entry for the page.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (!tb[i].valid) {
      tb[i].valid = true;
      tb[i].page_no = page;

      //HL
      //first access of the page
      tb[i].first_access=true;
      tb[i].group_access=1;
      
      //HL

      for (size_t j = 0; j < BITMAP_SIZE; j++)
      {
        //tb[i].bitmap[j] = false;//WL
        tb[i].bitmap[j]=1;//HL
      } 
      //HL
      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].block_indicate[j] = 0;
      //HL 

      for(auto var : filter_blks) 
        {
          //tb[i].bitmap[var] = true;//WL
          tb[i].bitmap[var]=1;//HL
          //HL
          tb[i].block_indicate[var]=tb[i].group_access;
          //HL
          tb[i].total_access++;
          tb[i].row_access[block/8]++;
          tb[i].col_access[block%8]++;
          //HL
        }
      lru_operate(tb, i, TABLE_WAY);
      tb[i].acc_counter = filter_blks.size();
      previous_page=current_page;//HL

      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  auto lru_el = std::max_element(tb.begin() + set * TABLE_WAY, tb.begin() + (set + 1) * TABLE_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->page_no = page;
  //HL
  lru_el->group_access=1;
  lru_el->first_access=true;
  //HL

  for(auto &var : lru_el->bitmap)
  {
    //var = false;WL
    var=0;//HL
  }

  for(auto &var : lru_el->bitmap_store)
    var = false;

  //HL
  for(auto &var : lru_el->block_indicate)
    var = 0;
  //HL

  for(auto var : filter_blks) 
    {
      //lru_el->bitmap[var] = true;
      lru_el->bitmap[var]=1;//HL
      //HL
      lru_el->block_indicate[var]=lru_el->group_access;
      //HL
      lru_el->total_access++;
      lru_el->row_access[block/8]++;
      lru_el->col_access[block%8]++;
      //HL
    } 

  lru_el->acc_counter = filter_blks.size();

  lru_operate(tb, lru_el - tb.begin(), TABLE_WAY);

  previous_page=current_page;//HL
}

void spp::SPP_PAGE_BITMAP::evict(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Check tb first.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].page_no == page &&
        tb[i].valid)
      {
        //tb[i].bitmap[block] = false;WL
        if(tb[i].bitmap[block]>0)
        {
          tb[i].bitmap[block]=tb[i].bitmap[block]-1;
        }
      }

  }
  //deleted by HL
  // Check filter.
  /*
  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].page_no == page &&
        filter[i].valid) 
      filter[i].bitmap[block] = false;
  }
  */
}

void spp::SPP_PAGE_BITMAP::update_bitmap_store() {

  for (size_t i = 0; i < TABLE_SIZE; i++) 
  {
    if (tb[i].valid && (tb[i].saturated_bit==true)) 
    {
        for (size_t j = 0; j < BITMAP_SIZE; j++)
        { 
          tb[i].bitmap[j] = tb[i].bitmap[j]>>1; // | tb[i].bitmap_store[j];
        }
        tb[i].saturated_bit=false;
      
    }
  }
  /*
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid) {
      // If same pages found in the same entry.
      if (tb[i].page_no_store == tb[i].page_no) {
        for (size_t j = 0; j < BITMAP_SIZE; j++) 
          tb[i].bitmap_store[j] = tb[i].bitmap[j]; // | tb[i].bitmap_store[j];
      }
      // If same page found in different entries.
      else {
        bool found = false;

        for (size_t k = 0; k < TABLE_SIZE; k++) {
          if (tb[k].page_no == tb[i].page_no_store) {
            for (size_t j = 0; j < BITMAP_SIZE; j++) 
              tb[i].bitmap_store[j] = tb[k].bitmap[j]; // | tb[i].bitmap_store[j];

            found = true;
            break;
          }
        }

        if (!found) {
          for (size_t j = 0; j < BITMAP_SIZE; j++) {
            tb[i].bitmap_store[j] = tb[i].bitmap[j]; // | tb[i].bitmap_store[j];
            tb[i].bitmap[j] = false;
          }

          tb[i].page_no_store = tb[i].page_no;
        }
      }
    }
  }
  */
}

std::vector<std::tuple<uint64_t, bool,int8_t>> spp::SPP_PAGE_BITMAP::gather_pf(uint64_t asid) {
  cs_pf.clear();
  std::vector<std::tuple<uint64_t, bool,int8_t>> pf;

  if (READ_PAGE_ACCESS) {
    for(auto pair : pb_acc) {
      if (pair.first == asid) {
        for(auto sub_pair : pair.second) {
          uint64_t page_addr = sub_pair.first << 12;

          for (size_t i = 0; i < BITMAP_SIZE; i++) {
            if (sub_pair.second[i]) 
              cs_pf.push_back(std::make_tuple(page_addr + (i << 6), true,1)); 
          }

          std::cout << asid << " " << sub_pair.first;

          for (size_t i = 0; i < BITMAP_SIZE; i++) 
            std::cout << " " << sub_pair.second[i];

          std::cout << std::endl;
        } 
      } 
    } 
  }
  else {

    if (RECORD_PAGE_ACCESS)//HL 
      print_page_access();//HL 

    int page_match = 0;
    
    for (size_t i = 0; i < TABLE_SIZE; i++) {
      
      /*if (tb[i].page_no == tb[i].page_no_store &&
          tb[i].valid) {
        page_match++;
        uint64_t page_addr = tb[i].page_no << 12;

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (tb[i].bitmap[j] && tb[i].bitmap_store[j]) 
            cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
        } 
      }
      */
      
      //else {
      
        uint64_t page_addr = tb[i].page_no << 12;
        
        //std::cout << "page_no " << tb[i].page_no;

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          //if (tb[i].bitmap[j]) {
            if(tb[i].bitmap[j]>=2){//HL
            //if()
            cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true,tb[i].block_indicate[j])); 
            //std::cout << " " << j;
          }
        }

        //std::cout << std::endl;
     // }
      
    }
    
    
    //HL
    
    bool row_lock_block=0;
    bool row_lock_total=0;

    //Determine the priority

    for(size_t i = 0; i < TABLE_SIZE; i++)
    {
      uint64_t page_addr = tb[i].page_no << 12;
      for (size_t j = 0; j < BITMAP_SIZE; j++)
      {
        //row_access
        if(this_round_pg_acc[tb[i].page_no].row_number[j/8]!=0 && tb[i].valid)//condition 1, number of access block in the row
        {
          if(this_round_pg_acc[tb[i].page_no].fraction_row_block[j/8]>1)//condition 2, frequency of each block access in the row
          {
             this_round_pg_acc[tb[i].page_no].tot_hit[j]++;
          }
          else if(this_round_pg_acc[tb[i].page_no].fraction_row_block[j/8]==1)
          {
            if(this_round_pg_acc[tb[i].page_no].fraction_row_total[j/8]>=(1/8))//condition 3, frequency of row over the total access >(1/8)
            {
              this_round_pg_acc[tb[i].page_no].tot_hit[j]++;
            }
          }        
        }
        //col access
        if(this_round_pg_acc[tb[i].page_no].column_number[j%8]!=0 &&  tb[i].valid)//condition 1, number of access block in the col
        {
          if(this_round_pg_acc[tb[i].page_no].fraction_column_block[j%8]>1)//condition 2, frequency of each block access in the col
          {
             this_round_pg_acc[tb[i].page_no].tot_hit[j]++;
          }
          else if(this_round_pg_acc[tb[i].page_no].fraction_column_block[j%8]==1)
          {
            if(this_round_pg_acc[tb[i].page_no].fraction_column_total[j%8]>=(1/8))//condition 3, frequency of col over the total access >(1/8)
            {
              this_round_pg_acc[tb[i].page_no].tot_hit[j]++;
          
            }
          } 
        }
      }
    }

    //actual prefetch
    for(size_t i = 0; i < TABLE_SIZE; i++)
    {
      uint64_t page_addr = tb[i].page_no << 12;
      for (size_t j = 0; j < BITMAP_SIZE; j++)
      {
        //row_access
        if(this_round_pg_acc[tb[i].page_no].row_number[j/8]!=0 && tb[i].valid)//condition 1, number of access block in the row
        {
          if(this_round_pg_acc[tb[i].page_no].fraction_row_block[j/8]>1)//condition 2, frequency of each block access in the row
          {
             if(this_round_pg_acc[tb[i].page_no].tot_hit[j]==2)
                cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true,tb[i].block_indicate[j]));
             else if(this_round_pg_acc[tb[i].page_no].tot_hit[j]==1)
                cs_pf.push_back(std::make_tuple(page_addr + (j << 6), false,tb[i].block_indicate[j]));
             row_lock_block=1;
          }
          else if(this_round_pg_acc[tb[i].page_no].fraction_row_block[j/8]==1)
          {
            if(this_round_pg_acc[tb[i].page_no].fraction_row_total[j/8]>=(1/8))//condition 3, frequency of row over the total access >(1/8)
            {
             if(this_round_pg_acc[tb[i].page_no].tot_hit[j]==2)
                cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true,tb[i].block_indicate[j]));
             else if(this_round_pg_acc[tb[i].page_no].tot_hit[j]==1)
                cs_pf.push_back(std::make_tuple(page_addr + (j << 6), false,tb[i].block_indicate[j]));
              row_lock_total=1;
            }
          }        
        }
        //col access
        if(this_round_pg_acc[tb[i].page_no].column_number[j%8]!=0 &&  tb[i].valid)//condition 1, number of access block in the col
        {
          if(this_round_pg_acc[tb[i].page_no].fraction_column_block[j%8]>1)//condition 2, frequency of each block access in the col
          {
            if((row_lock_block==0) && (row_lock_total==0))
            {
              cs_pf.push_back(std::make_tuple(page_addr + (j << 6), false,tb[i].block_indicate[j]));
            }
          }
          else if(this_round_pg_acc[tb[i].page_no].fraction_column_block[j%8]==1)
          {
            if(this_round_pg_acc[tb[i].page_no].fraction_column_total[j%8]>=(1/8))//condition 3, frequency of col over the total access >(1/8)
            {
              if((row_lock_block==0) && (row_lock_total==0))
              {
                cs_pf.push_back(std::tuple(page_addr + (j << 6), false,tb[i].block_indicate[j]));
              }
            }
          } 
        }
        row_lock_block=0;
        row_lock_total=0;
      }
    }

    this_round_pg_acc.clear();
    //HL
    
    std::cout << "Page bitmap page matches: " << page_match << std::endl;
    uint64_t filter_sum = 0;

    for (size_t i = 0; i < FILTER_SIZE; i++) {
      if (filter[i].valid) {
        uint64_t page_addr = filter[i].page_no << 12;

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (filter[i].bitmap[j]) {
            cs_pf.push_back(std::make_tuple(page_addr + (j << 6), true,1));
            filter_sum++;
          }
        } 
      } 
    }
    // todo::clear the fitler
    for(size_t i=0;i<FILTER_SIZE;i++)
    {
      filter[i].valid=false;
      filter[i].page_no=0;
      filter[i].lru_bits=0;    
    }

    /*
    for (size_t i = 0; i < TABLE_SIZE; i++) {
      tb[i].valid = false;
      
      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].bitmap[j] = false; 
    }
    */

    //if (RECORD_PAGE_ACCESS) 
      //print_page_access(); 
  }

  std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;

  for(auto var : cs_pf)
    pf.push_back(var); 

  return pf;
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
        return true; 
      else 
        return false;
    } 
  }

  // Allocate new entry in the filter.
  // If any invalid entry exists.
  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (!filter[i].valid) {
      filter[i].valid = true;
      filter[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
        filter[i].bitmap[j] = false; 

      filter[i].bitmap[block] = true;
      lru_operate(filter, i, FILTER_WAY);

      return false;
    } 
  }

  // If filter full, use LRU to replace.
  auto lru_el = std::max_element(filter.begin() + set * FILTER_WAY, filter.begin() + (set + 1) * FILTER_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->page_no = page;
  lru_el->valid = true;

  for(auto &var : lru_el->bitmap)
    var = false;

  lru_el->bitmap[block] = true;
  lru_operate(filter, lru_el - filter.begin(), FILTER_WAY);

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

    uint64_t accu = std::accumulate(pair.second.block.begin(), pair.second.block.end(), 0);

    if (auto search = last_round_pg_acc.find(pair.first); search != last_round_pg_acc.end()) {

      if (accu > FILTER_THRESHOLD) {
        same_pg_cnt++;
        std::cout << "Page " << pair.first << " match, accesses: " << last_round_pg_acc[pair.first].total_access << "/" << this_round_pg_acc[pair.first].total_access << " last/this" << std::endl;

        for (size_t i = 0; i < BITMAP_SIZE / 8; i++) 
          std::cout << std::setw(4) << std::right << last_round_pg_acc[pair.first].col_access[i] << "/" << std::setw(4) << std::left << this_round_pg_acc[pair.first].col_access[i]; 

        std::cout << std::endl;

        for (std::size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          for (size_t j = 0; j < BITMAP_SIZE / 8; j++)  {
            std::cout << "   " << (last_round_pg_acc[pair.first].block[i * 8 + j] ? "\u25FC" : "\u25FB") << "/";
            std::cout << (pair.second.block[i * 8 + j] ? "\u25FC" : "\u25FB") << "   ";
            if(pair.second.block[i*8+j]==true)
            {
              //pair.second.row_number[i]++;
              this_round_pg_acc[pair.first].row_number[i]++;
              this_round_pg_acc[pair.first].column_number[(i*8+j)%8]++;
              //std::cout<<"The count at row "<<i<<"of page"<<pair.first<<"is"<<pair.second.row_number[i]<<std::endl;
            }
          }
          this_round_pg_acc[pair.first].fraction_row_block[i]=float(this_round_pg_acc[pair.first].row_access[i])/float(this_round_pg_acc[pair.first].row_number[i]);
          this_round_pg_acc[pair.first].fraction_row_total[i]=float(this_round_pg_acc[pair.first].row_access[i])/float(this_round_pg_acc[pair.first].total_access);
           
          //std::cout<<"The fraction is"<<this_round_pg_acc[pair.first].fraction_total[i]<<std::endl;
          std::cout << std::setw(4) << std::right << last_round_pg_acc[pair.first].row_access[i] << "/" << this_round_pg_acc[pair.first].row_access[i]<<std::setw(8) << this_round_pg_acc[pair.first].row_number[i] << std::setw(8)<< this_round_pg_acc[pair.first].fraction_row_total[i]<<std::setw(8)<<this_round_pg_acc[pair.first].fraction_row_block[i]<<std::endl;
        }

        for(std::size_t i=0; i<BITMAP_SIZE/8;i++)
        {
          this_round_pg_acc[pair.first].fraction_column_block[i]=float(this_round_pg_acc[pair.first].col_access[i])/float(this_round_pg_acc[pair.first].column_number[i]);
          this_round_pg_acc[pair.first].fraction_column_total[i]=float(this_round_pg_acc[pair.first].col_access[i])/float(this_round_pg_acc[pair.first].total_access); 
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
  //this_round_pg_acc.clear();

  std::cout << "Page bitmap same page: " << same_pg_cnt << std::endl;
}

//HL
//uint64_t spp::SPP_PAGE_BITMAP::access_block(int group, std::deque<>*group_queue, std::unordered_set<>*group_set) {}
