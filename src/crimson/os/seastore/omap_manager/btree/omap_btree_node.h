// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#pragma once

#include <string>
#include <vector>

//#include <boost/iterator/counting_iterator.hpp>

#include "crimson/common/log.h"
#include "crimson/os/seastore/seastore_types.h"
#include "crimson/os/seastore/transaction_manager.h"
#include "crimson/os/seastore/omap_manager.h"
#include "crimson/os/seastore/omap_manager/btree/omap_types.h"

namespace crimson::os::seastore::omap_manager{

const std::string BEGIN_KEY = "";
const std::string END_KEY(64, (char)(-1));

struct omap_context_t {
  TransactionManager &tm;
  Transaction &t;
  laddr_t hint;
  omap_type_t type;
};

enum class mutation_status_t : uint8_t {
  SUCCESS = 0,
  WAS_SPLIT = 1,
  NEED_MERGE = 2,
  FAIL = 3
};

struct OMapNode : LogicalChildNode {
  using base_iertr = OMapManager::base_iertr;

  using OMapNodeRef = TCachedExtentRef<OMapNode>;

  struct mutation_result_t {
    mutation_status_t status;
    /// Only populated if WAS_SPLIT, indicates the newly created left and right nodes
    /// from splitting the target entry during insertion.
    std::optional<std::tuple<OMapNodeRef, OMapNodeRef, std::string>> split_tuple;
    /// only sopulated if need merged, indicate which entry need be doing merge in upper layer.
    std::optional<OMapNodeRef> need_merge;

    mutation_result_t(mutation_status_t s, std::optional<std::tuple<OMapNodeRef,
                      OMapNodeRef, std::string>> tuple, std::optional<OMapNodeRef> n_merge)
    : status(s),
      split_tuple(tuple),
      need_merge(n_merge) {}
  };

  explicit OMapNode(ceph::bufferptr &&ptr) : LogicalChildNode(std::move(ptr)) {}
  explicit OMapNode(extent_len_t length) : LogicalChildNode(length) {}
  OMapNode(const OMapNode &other)
  : LogicalChildNode(other),
    root(other.root),
    begin(other.begin),
    end(other.end) {}

  using get_value_iertr = base_iertr;
  using get_value_ret = OMapManager::omap_get_value_ret;
  virtual get_value_ret get_value(
    omap_context_t oc,
    const std::string &key) = 0;

  using insert_iertr = base_iertr::extend<
    crimson::ct_error::value_too_large>;
  using insert_ret = insert_iertr::future<mutation_result_t>;
  virtual insert_ret insert(
    omap_context_t oc,
    const std::string &key,
    const ceph::bufferlist &value) = 0;

  using rm_key_iertr = base_iertr;
  using rm_key_ret = rm_key_iertr::future<mutation_result_t>;
  virtual rm_key_ret rm_key(
    omap_context_t oc,
    const std::string &key) = 0;

  using iterate_iertr = base_iertr;
  using iterate_ret = OMapManager::omap_iterate_ret;
  using omap_iterate_cb_t = OMapManager::omap_iterate_cb_t;
  virtual iterate_ret iterate(
    omap_context_t oc,
    ObjectStore::omap_iter_seek_t &start_from,
    omap_iterate_cb_t callback) = 0;

  using omap_list_config_t = OMapManager::omap_list_config_t;
  using list_iertr = base_iertr;
  using list_bare_ret = OMapManager::omap_list_bare_ret;
  using list_ret = OMapManager::omap_list_ret;
  virtual list_ret list(
    omap_context_t oc,
    const std::optional<std::string> &first,
    const std::optional<std::string> &last,
    omap_list_config_t config) = 0;

  using clear_iertr = base_iertr;
  using clear_ret = clear_iertr::future<>;
  virtual clear_ret clear(omap_context_t oc) = 0;

  using full_merge_iertr = base_iertr;
  using full_merge_ret = full_merge_iertr::future<OMapNodeRef>;
  virtual full_merge_ret make_full_merge(
    omap_context_t oc,
    OMapNodeRef right) = 0;

  using make_balanced_iertr = base_iertr;
  using make_balanced_ret = make_balanced_iertr::future
          <std::tuple<OMapNodeRef, OMapNodeRef, std::string>>;
  virtual make_balanced_ret make_balanced(
    omap_context_t oc,
    OMapNodeRef _right,
    uint32_t pivot_idx) = 0;

  virtual omap_node_meta_t get_node_meta() const = 0;
  virtual bool extent_will_overflow(
    size_t ksize,
    std::optional<size_t> vsize) const = 0;
  virtual bool can_merge(OMapNodeRef right) const = 0;
  virtual bool extent_is_below_min() const = 0;
  virtual uint32_t get_node_size() = 0;

  virtual ~OMapNode() = default;

  virtual bool exceeds_max_kv_limit(
    const std::string &key,
    const ceph::bufferlist &value) const = 0;

  void init_range(std::string _begin, std::string _end) {
    assert(begin.empty());
    assert(end.empty());
    if (_begin == BEGIN_KEY && _end == END_KEY) {
      root = true;
    }
    begin = std::move(_begin);
    end = std::move(_end);
  }
  const std::string &get_begin() const {
    return begin;
  }
  const std::string &get_end() const {
    return end;
  }
  bool is_btree_root() const { return root; }
protected:
  void set_root(bool is_root) {
    root = is_root;
  }
private:
  bool root = false;
  std::string begin;
  std::string end;
};

using OMapNodeRef = OMapNode::OMapNodeRef;

}

#if FMT_VERSION >= 90000
template <> struct fmt::formatter<crimson::os::seastore::omap_manager::OMapNode> : fmt::ostream_formatter {};
#endif
