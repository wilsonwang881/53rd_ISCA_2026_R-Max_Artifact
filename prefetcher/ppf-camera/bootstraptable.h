#include <array>
#include <cstdint>
#include <optional>

namespace spp
{
  // Global register parameters
  
  //HL
  constexpr unsigned  GLOBAL_COUNTER_BIT=10;
  constexpr uint32_t GLOBAL_COUNTER_MAX=((1 << GLOBAL_COUNTER_BIT) - 1); 
  constexpr std::size_t PAGES_TRACKED=6;
  
  //HL

  class BOOTSTRAP_TABLE
  {
    //HL
  public:
    uint64_t ip_0,
             ip_1,
             ip_2,
             ip_3;

    uint64_t page_tracker[PAGES_TRACKED];

    // Stats Collection
    /*
    double    depth_val,
              depth_sum,
              depth_num;
    double    pf_total,
              pf_l2c,
              pf_llc,
              pf_l2c_good;
    long    perc_pass,
            perc_reject,
            //reject_update;
    */
    //HL

    BOOTSTRAP_TABLE() {
      //pf_useful = 0;
      //pf_issued = 0;
      //global_accuracy = 0;
      ip_0 = 0;
      ip_1 = 0;
      ip_2 = 0;
      ip_3 = 0;

      // These are just for stats printing
      //depth_val = 0;
      //depth_sum = 0;
      //depth_num = 0;
      //pf_total = 0;
      //pf_l2c = 0;
      //pf_llc = 0;
      //pf_l2c_good = 0;
      //perc_pass = 0;
      //perc_reject = 0;
      //reject_update = 0;

    }
    
    //HL

    static constexpr std::size_t MAX_GHR_ENTRY = 8;
    struct ghr_entry_t
    {
      bool     valid = false;
      uint32_t sig = 0;
      int      confidence = 0;
      uint32_t offset = 0;
      int      delta = 0;
    };

    // Global History Register (GHR) entries
    std::array<ghr_entry_t, MAX_GHR_ENTRY> page_bootstrap_table;

    public:
    void update(uint64_t addr, uint32_t sig, int confidence, int delta);
    std::optional<std::tuple<uint32_t, int, int>> check(uint64_t addr);
  };
}

