// [Midas]
#include "array.hpp"
#include "cache_manager.hpp"
#include "resource_manager.hpp"
#include "perf.hpp"
#include "time.hpp"
#include "utils.hpp"
#include "zipf.hpp"

constexpr static uint64_t kVSize = 1024;
constexpr static uint64_t kNumValues = (1ULL << 20);

struct Value {
  char data[kVSize];
};

int main() {
  auto cmanager = midas::CacheManager::global_cache_manager();
  // setting up the memory pool
  cmanager->create_pool("pool_name");
  auto pool = cmanager->get_pool("pool_name");
  pool->update_limit(kNumValues * kVSize * 2);
  auto cache = std::make_unique<midas::Array<Value>>(pool, kNumValues);

  // populate the data structure

  for (uint64_t i =0 ; i < kNumValues; i++) {
    Value v;
    cache->set(i, v);
  }

  std::cout << "Finished Setting Up" << std::endl;
  // pool->update_limit(kNumValues * kVSize / 2);

  // count the number of recomputations when iterating over
  uint64_t recon_cnt = 0;
  for (uint64_t i = 0; i < kNumValues; i++) {
    auto ok = cache->get(i);
    if (!ok) {
      recon_cnt++;
    }
  }
  // print the % of the data structure recomputed
  std::cout << "Recomputed " << recon_cnt
    << " " << (recon_cnt / (double) kNumValues) * 100 << "%" << std::endl;



}