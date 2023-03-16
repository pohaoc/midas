#pragma once

namespace midas {

template <size_t NBuckets, typename Alloc, typename Lock>
SyncKV<NBuckets, Alloc, Lock>::SyncKV() {
  pool_ = CachePool::global_cache_pool();
  memset(buckets_, 0, sizeof(buckets_));
}

template <size_t NBuckets, typename Alloc, typename Lock>
SyncKV<NBuckets, Alloc, Lock>::SyncKV(CachePool *pool) : pool_(pool) {
  memset(buckets_, 0, sizeof(buckets_));
}

/** Storage layout in soft memory:
 *    | KeyLen (8B) | ValueLen (8B) | Key (`KeyLen`B) | Value (`ValueLen`B) |
 */

/** Base Interfaces */
template <size_t NBuckets, typename Alloc, typename Lock>
void *SyncKV<NBuckets, Alloc, Lock>::get(const void *k, size_t kn, size_t *vn) {
  return get_(k, kn, nullptr, vn);
}

template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::get(const void *k, size_t kn, void *v,
                                        size_t vn) {
  if (!v)
    return false;
  size_t stored_vn = 0;
  if (get_(k, kn, v, &stored_vn) == nullptr)
    return false;
  if (stored_vn < vn) // value size check
    return false;
  return true;
}

template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::remove(const void *k, size_t kn) {
  auto key_hash = hash_(k, kn);
  auto bucket_idx = key_hash % NBuckets;

  auto &lock = locks_[bucket_idx];
  lock.lock();

  auto prev_next = &buckets_[bucket_idx];
  BNPtr node = buckets_[bucket_idx];
  bool found = false;
  while (node) {
    found = iterate_list(key_hash, k, kn, nullptr, prev_next, node);
    if (found)
      break;
  }
  if (!found) {
    lock.unlock();
    return false;
  }
  assert(node);
  delete_node(prev_next, node);
  lock.unlock();
  /* should not count access for remove() */
  // LogAllocator::count_access();
  return true;
}

template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::set(const void *k, size_t kn, const void *v,
                                        size_t vn) {
  auto key_hash = hash_(k, kn);
  auto bucket_idx = key_hash % NBuckets;

  auto &lock = locks_[bucket_idx];
  lock.lock();

  size_t stored_vn = 0;
  auto prev_next = &buckets_[bucket_idx];
  auto node = buckets_[bucket_idx];
  while (node) {
    auto found = iterate_list(key_hash, k, kn, &stored_vn, prev_next, node);
    if (found) {
      if (vn <= stored_vn && !node->pair.null() && // try to set in place if fit
          node->pair.copy_from(&vn, sizeof(size_t), sizeof(size_t)) &&
          node->pair.copy_from(v, vn, kn + sizeof(size_t) * 2)) {
        lock.unlock();
        LogAllocator::count_access();
        return true;
      } else {
        node = delete_node(prev_next, node);
        break;
      }
    }
  }

  auto new_node = create_node(key_hash, k, kn, v, vn);
  if (!new_node) {
    lock.unlock();
    return false;
  }
  *prev_next = new_node;
  lock.unlock();
  LogAllocator::count_access();
  return true;
}

template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::clear() {
  for (int idx = 0; idx < NBuckets; idx++) {
    auto &lock = locks_[idx];
    lock.lock();
    auto prev_next = &buckets_[idx];
    auto node = buckets_[idx];
    while (node)
      node = delete_node(prev_next, node);
    lock.unlock();
  }
  return true;
}

template <size_t NBuckets, typename Alloc, typename Lock>
void *SyncKV<NBuckets, Alloc, Lock>::get_(const void *k, size_t kn, void *v,
                                          size_t *vn) {
  auto key_hash = hash_(k, kn);
  auto bucket_idx = key_hash % NBuckets;

  auto &lock = locks_[bucket_idx];
  lock.lock();

  size_t stored_vn = 0;
  auto prev_next = &buckets_[bucket_idx];
  BNPtr node = buckets_[bucket_idx];
  bool found = false;
  while (node) {
    found = iterate_list(key_hash, k, kn, &stored_vn, prev_next, node);
    if (found)
      break;
  }
  if (!found) {
    lock.unlock();
    pool_->inc_cache_miss();
    return nullptr;
  }
  assert(node);
  void *stored_v = v ? v : malloc(stored_vn);
  if (node->pair.null() ||
      !node->pair.copy_to(stored_v, stored_vn, kn + sizeof(size_t) * 2)) {
    if (node->pair.is_victim())
      pool_->inc_cache_victim_hit();
    node = delete_node(prev_next, node);
    lock.unlock();
    return nullptr;
  }
  lock.unlock();
  if (vn)
    *vn = stored_vn;

  pool_->inc_cache_hit();
  LogAllocator::count_access();
  return stored_v;
}

/** Ordered Set */
/** Value Format:
 *    | NumEle (8B) | Len<V1> (8B) | Score<V1> (8B) | V1 (Len<V1>) | ...
 *      | Len<Vn> (8B) | Score<Vn> (8B) | Vn (Len<Vn>) |
 */
namespace ordered_set {
struct OSetEle {
  size_t len;
  double score;
  char data[]; // value

  static inline size_t total_size(size_t vn) { return sizeof(OSetEle) + vn; }

  inline void init(const void *v, size_t vn, double score) {
    this->len = total_size(vn);
    this->score = score;
    std::memcpy(data, v, vn);
  };
};
static_assert(sizeof(OSetEle) == sizeof(size_t) + sizeof(double),
              "OSetEle is not corretly aligned!");

struct OSet {
  size_t num_ele;
  OSetEle data[]; // value array
};
static_assert(sizeof(OSet) == sizeof(size_t), "OSet is not corretly aligned!");

static inline OSetEle *oset_iter(OSet *oset, size_t oset_len, OSetEle *pos) {
  if (!pos || offset_ptrs(pos, oset) >= oset_len)
    return nullptr;
  size_t vn = pos->len;
  auto next = ptr_offset(pos, vn);
  return next;
}

static inline OSet *oset_init(const void *v, size_t vn, double score) {
  // allocate NumEle (size_t) and buffer for the single element
  OSet *oset = reinterpret_cast<OSet *>(
      malloc(sizeof(size_t) + OSetEle::total_size(vn)));
  if (!oset)
    return nullptr;
  oset->num_ele = 1;
  auto e = reinterpret_cast<OSetEle *>(oset->data);
  e->init(v, vn, score);
  return oset;
}

static inline OSetEle *oset_search(OSet *oset, size_t oset_len, const void *v,
                                   size_t vn) {
  assert(oset->num_ele > 0);

  bool found = false;
  OSetEle *iter = oset->data;
  while (iter) {
    if (iter->len == vn && std::memcmp(iter->data, v, vn) == 0) {
      found = true;
      break;
    }
    iter = oset_iter(oset, oset_len, iter);
  }
  if (found)
    return iter;
  return nullptr;
}

/** Insert an element @v into ordered set. v must not in the set before. */
static inline bool oset_insert(OSet *&oset_, size_t &oset_len_, const void *v,
                               size_t vn, double score) {
  constexpr static bool DUP_CHECK = false;
  OSet *oset = oset_;
  size_t oset_len = oset_len_;
  OSetEle *oset_end = reinterpret_cast<OSetEle *>(ptr_offset(oset, oset_len));
  size_t num_ele = *reinterpret_cast<size_t *>(oset);
  assert(num_ele > 0);
  auto data = ptr_offset(oset, sizeof(size_t));

  OSetEle *iter = oset->data;
  while (iter < oset_end) {
    if (DUP_CHECK && iter->len == vn && std::memcmp(iter->data, v, vn) == 0) {
      return false;
    }
    if (iter->score > score) {
      break;
    }
    iter = oset_iter(oset, oset_len, iter);
  }
  size_t new_oset_len = oset_len + OSetEle::total_size(vn);
  OSet *new_oset = reinterpret_cast<OSet *>(malloc(new_oset_len));
  if (!new_oset)
    return false;
  new_oset->num_ele = oset->num_ele + 1;
  size_t offset = offset_ptrs(iter, oset->data);
  size_t remaining_size = oset_len - sizeof(size_t) - offset;
  if (offset)
    std::memcpy(new_oset->data, oset->data, offset);
  auto new_data = new_oset->data;
  auto e = reinterpret_cast<OSetEle *>(ptr_offset(new_data, offset));
  e->init(v, vn, score);
  if (remaining_size)
    std::memcpy(ptr_offset(new_data, offset + OSetEle::total_size(vn)),
                ptr_offset(data, offset), remaining_size);

  // update oset
  free(oset);
  oset_ = new_oset;
  oset_len_ = new_oset_len;
  return true;
}
} // namespace ordered_set

/** insert a value into the ordered set by its timestamp */
template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::zadd(const void *k, size_t kn,
                                         const void *v, size_t vn, double score,
                                         UpdateType type) {
  size_t oset_len = 0;
  auto oset =
      reinterpret_cast<ordered_set::OSet *>(get_(k, kn, nullptr, &oset_len));
  if (!oset) {
    if (type == UpdateType::EXIST)
      return false;
    else if (type == UpdateType::NOT_EXIST) {
      oset = ordered_set::oset_init(v, vn, score);
      if (!oset)
        return false;
      auto oset_len = sizeof(size_t) + ordered_set::OSetEle::total_size(vn);
      return set(k, kn, oset, oset_len);
    } else {
      MIDAS_ABORT("Not implemented yet!");
    }
  }

  auto *pos = ordered_set::oset_search(oset, oset_len, v, vn);
  bool found = pos != nullptr;

  if (type == UpdateType::EXIST) {
    if (!found)
      goto failed;
    pos->init(v, vn, score); // must have pos->len == vn
    bool ret = set(k, kn, oset, oset_len);
    return ret;
  } else if (type == UpdateType::NOT_EXIST) {
    if (found)
      goto failed;
    bool ret = ordered_set::oset_insert(oset, oset_len, v, vn, score);
    if (!ret)
      goto failed;
    ret = set(k, kn, oset, oset_len);
    free(oset);
    return ret;
  }

failed:
  if (oset)
    free(oset);
  return false;
}

/** Fetch range [start, end] */
template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::zrange(
    const void *key, size_t klen, int64_t start, int64_t end,
    std::back_insert_iterator<std::vector<ordered_set::Value>> bi) {
  size_t oset_len;
  auto oset = reinterpret_cast<ordered_set::OSet *>(
      get_(key, klen, nullptr, &oset_len));
  if (!oset)
    return false;
  if (start < 0 || end > oset->num_ele)
    return false;
  auto iter = oset->data;
  for (size_t i = 0; i < start; i++)
    iter = ordered_set::oset_iter(oset, oset_len, iter);
  for (size_t i = start; i <= end; i++) {
    auto len = iter->len - sizeof(ordered_set::OSetEle);
    auto data = malloc(len);
    std::memcpy(data, iter->data, len);
    bi = std::make_pair(data, len);
    iter = ordered_set::oset_iter(oset, oset_len, iter);
  }
  return true;
}

template <size_t NBuckets, typename Alloc, typename Lock>
bool SyncKV<NBuckets, Alloc, Lock>::zrevrange(
    const void *key, size_t klen, int64_t start, int64_t end,
    std::back_insert_iterator<std::vector<ordered_set::Value>> bi) {
  std::vector<ordered_set::Value> values;
  if (!zrange(key, klen, start, end, std::back_inserter(values)))
    return false;
  for (auto iter = values.rbegin(); iter != values.rend(); ++iter)
    bi = *iter;
  return true;
}

/** Utility functions */
template <size_t NBuckets, typename Alloc, typename Lock>
using BNPtr = typename SyncKV<NBuckets, Alloc, Lock>::BucketNode *;

template <size_t NBuckets, typename Alloc, typename Lock>
inline uint64_t SyncKV<NBuckets, Alloc, Lock>::hash_(const void *k, size_t kn) {
  return kn == sizeof(uint64_t)
             ? robin_hood::hash_int(*(reinterpret_cast<const uint64_t *>(k)))
             : robin_hood::hash_bytes(k, kn);
}

template <size_t NBuckets, typename Alloc, typename Lock>
inline BNPtr<NBuckets, Alloc, Lock> SyncKV<NBuckets, Alloc, Lock>::create_node(
    uint64_t key_hash, const void *k, size_t kn, const void *v, size_t vn) {
  auto *new_node = new BucketNode();
  if (!pool_->alloc_to(sizeof(size_t) * 2 + kn + vn, &new_node->pair) ||
      !new_node->pair.copy_from(&kn, sizeof(size_t)) ||
      !new_node->pair.copy_from(&vn, sizeof(size_t), sizeof(size_t)) ||
      !new_node->pair.copy_from(k, kn, sizeof(size_t) * 2) ||
      !new_node->pair.copy_from(v, vn, kn + sizeof(size_t) * 2)) {
    delete new_node;
    return nullptr;
  }
  // assert(!new_node->pair.null());
  if (new_node->pair.null()) {
    MIDAS_LOG(kError) << "new node KV pair is freed!";
    delete new_node;
    return nullptr;
  }
  new_node->key_hash = key_hash;
  new_node->next = nullptr;
  return new_node;
}

/** remove bucket_node from the list */
// should always use as `node = delete_node()` when iterating the list
template <size_t NBuckets, typename Alloc, typename Lock>
inline BNPtr<NBuckets, Alloc, Lock>
SyncKV<NBuckets, Alloc, Lock>::delete_node(BNPtr *prev_next, BNPtr node) {
  assert(*prev_next == node);
  if (!node)
    return nullptr;
  auto next = node->next;

  pool_->free(node->pair);
  delete node;

  *prev_next = next;
  return next;
}

/** return: <equal, valid> */
template <size_t NBuckets, typename Alloc, typename Lock>
inline bool
SyncKV<NBuckets, Alloc, Lock>::iterate_list(uint64_t key_hash, const void *k,
                                            size_t kn, size_t *vn,
                                            BNPtr *&prev_next, BNPtr &node) {
  size_t stored_kn = 0;
  void *stored_k = nullptr;
  if (key_hash != node->key_hash)
    goto notequal;
  if (node->pair.null() || !node->pair.copy_to(&stored_kn, sizeof(size_t)))
    goto faulted;
  if (stored_kn != kn)
    goto notequal;
  stored_k = malloc(kn);
  if (!node->pair.copy_to(stored_k, kn, sizeof(size_t) * 2))
    goto faulted;
  if (strncmp(reinterpret_cast<const char *>(k),
              reinterpret_cast<const char *>(stored_k), kn) != 0)
    goto notequal;
  if (vn && !node->pair.copy_to(vn, sizeof(size_t), sizeof(size_t)))
    goto faulted;
  if (stored_k)
    free(stored_k);
  return true;

faulted:
  if (stored_k)
    free(stored_k);
  if (node->pair.is_victim())
    pool_->inc_cache_victim_hit();
  // prev remains the same when current node is deleted.
  node = delete_node(prev_next, node);
  return false;
notequal:
  if (stored_k)
    free(stored_k);
  prev_next = &(node->next);
  node = node->next;
  return false;
}

} // namespace midas
