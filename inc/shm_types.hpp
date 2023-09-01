#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace midas {

enum CtrlOpCode {
  CONNECT,
  DISCONNECT,
  ALLOC,
  OVERCOMMIT,
  FREE,
  UPDLIMIT,
  UPDLIMIT_REQ,
  FORCE_RECLAIM,
  PROF_STATS,
  SET_WEIGHT,
  SET_LAT_CRITICAL,
};

enum CtrlRetCode {
  CONN_SUCC,
  CONN_FAIL,
  MEM_SUCC,
  MEM_FAIL,
};

struct MemMsg {
  int64_t region_id;
  union {
    uint64_t size;
    float weight;
    bool lat_critical;
  };
};

struct CtrlMsg {
  uint64_t id;
  CtrlOpCode op;
  CtrlRetCode ret;
  MemMsg mmsg;
};

struct StatsMsg {
  // cache stats
  uint64_t hits;
  uint64_t misses;
  double miss_penalty;
  // victim cache stats
  uint32_t vhits;
  // full threshold
  uint32_t headroom;
};
static_assert(sizeof(CtrlMsg) == sizeof(StatsMsg),
              "CtrlMsg and StatsMsg have different size!");

struct VRange {
  VRange() = default;
  VRange(void *addr_, size_t size_) : stt_addr(addr_), size(size_) {}

  void *stt_addr;
  size_t size;

  bool contains(const void *ptr) const noexcept {
    return ptr > stt_addr && ptr < reinterpret_cast<char *>(stt_addr) + size;
  }
};

namespace utils {
static inline const std::string get_region_name(uint64_t pid, uint64_t rid) {
  return "region-" + std::to_string(pid) + "-" + std::to_string(rid);
}
} // namespace utils

} // namespace midas