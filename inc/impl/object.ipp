#pragma once

#include <cassert>

#include "utils.hpp"

namespace cachebank {

/** Generic Object */
inline void GenericObjectHdr::set_invalid() noexcept {
  flags = kInvalidHdr;
}

inline bool GenericObjectHdr::is_valid() const noexcept {
  return flags != kInvalidHdr;
}

inline bool GenericObjectHdr::is_small_obj() const noexcept {
  return (flags & (1ull << kSmallObjBit));
}

/** Small Object */
inline void SmallObjectHdr::init(uint32_t size_, uint64_t rref) noexcept {
  set_size(size_);
  set_rref(rref);
  set_present();
  _small_obj();
}

inline void SmallObjectHdr::free() noexcept { clr_present(); }

inline void SmallObjectHdr::set_invalid() noexcept {
  auto *meta = reinterpret_cast<uint64_t *>(this);
  *meta = kInvalidHdr;
}

inline bool SmallObjectHdr::is_valid() noexcept {
  auto *meta = reinterpret_cast<uint64_t *>(this);
  return *meta != kInvalidHdr;
}

inline void SmallObjectHdr::set_size(uint32_t size_) noexcept {
  size_ = round_up_to_align(size_, kSmallObjSizeUnit);
  size_ /= kSmallObjSizeUnit;
  assert(size_ < (1 << 12));
  size = size_;
}
inline uint32_t SmallObjectHdr::get_size() const noexcept {
  return size * kSmallObjSizeUnit;
}

inline void SmallObjectHdr::set_rref(uint64_t addr) noexcept { rref = addr; }
inline uint64_t SmallObjectHdr::get_rref() const noexcept { return rref; }

inline bool SmallObjectHdr::is_present() const noexcept {
  return flags & (1 << kPresentBit);
}
inline void SmallObjectHdr::set_present() noexcept {
  flags |= (1 << kPresentBit);
}
inline void SmallObjectHdr::clr_present() noexcept {
  flags &= ~(1 << kPresentBit);
}

inline bool SmallObjectHdr::is_accessed() const noexcept {
  return flags & (1 << kAccessedBit);
}
inline void SmallObjectHdr::set_accessed() noexcept {
  flags |= (1 << kAccessedBit);
}
inline void SmallObjectHdr::clr_accessed() noexcept {
  flags &= ~(1 << kAccessedBit);
}

inline bool SmallObjectHdr::is_evacuate() const noexcept {
  return flags & (1 << kEvacuateBit);
}
inline void SmallObjectHdr::set_evacuate() noexcept {
  flags |= (1 << kEvacuateBit);
}
inline void SmallObjectHdr::clr_evacuate() noexcept {
  flags &= ~(1 << kEvacuateBit);
}

inline void SmallObjectHdr::_small_obj() noexcept {
  flags |= 1 << kSmallObjBit;
}

/** Large Object */
inline void LargeObjectHdr::init(uint32_t size_, uint64_t rref) noexcept {
  set_size(size_);
  set_rref(rref);
  set_present();
  _large_obj();
}

inline void LargeObjectHdr::set_invalid() noexcept {
  auto *meta = reinterpret_cast<uint64_t *>(this);
  *meta = kInvalidHdr;
}

inline bool LargeObjectHdr::is_valid() noexcept {
  auto *meta = reinterpret_cast<uint64_t *>(this);
  return *meta != kInvalidHdr;
}

inline void LargeObjectHdr::free() noexcept { clr_present(); }

inline void LargeObjectHdr::set_size(uint32_t size_) noexcept { size = size_; }
inline uint32_t LargeObjectHdr::get_size() const noexcept { return size; }

inline void LargeObjectHdr::set_rref(uint64_t addr) noexcept { rref = addr; }
inline uint64_t LargeObjectHdr::get_rref() const noexcept { return rref; }

inline bool LargeObjectHdr::is_present() const noexcept {
  return flags & (1 << kPresentBit);
}
inline void LargeObjectHdr::set_present() noexcept {
  flags |= (1 << kPresentBit);
}
inline void LargeObjectHdr::clr_present() noexcept {
  flags &= ~(1 << kPresentBit);
}

inline bool LargeObjectHdr::is_accessed() const noexcept {
  return flags & (1 << kAccessedBit);
}
inline void LargeObjectHdr::set_accessed() noexcept {
  flags |= (1 << kAccessedBit);
}
inline void LargeObjectHdr::clr_accessed() noexcept {
  flags &= ~(1 << kAccessedBit);
}

inline bool LargeObjectHdr::is_evacuate() const noexcept {
  return flags & (1 << kEvacuateBit);
}
inline void LargeObjectHdr::set_evacuate() noexcept {
  flags |= (1 << kEvacuateBit);
}
inline void LargeObjectHdr::clr_evacuate() noexcept {
  flags &= ~(1 << kEvacuateBit);
}

inline void LargeObjectHdr::_large_obj() noexcept {
  flags &= ~(1 << kSmallObjBit);
}

} // namespace cachebank