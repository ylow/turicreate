/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_GROUPED_XFRAME_HPP
#define TURI_GROUPED_XFRAME_HPP

#include <model_server/lib/toolkit_class_macros.hpp>
#include <core/data/xframe/gl_xframe.hpp>
#include <model_server/lib/extensions/model_base.hpp>
#include <core/export.hpp>
#include <core/parallel/lambda_omp.hpp>

namespace turi {
std::vector<turi::toolkit_class_specification> get_toolkit_class_registration();

/**
 * Hash function for a row of flexible_type.
 */
struct EXPORT GroupKeyHash {
  inline size_t operator()(std::vector<flexible_type> f) const {
    size_t key_hash = 0;
    for(const auto &i : f) {
      key_hash = hash64_combine(key_hash, i.hash());
    }

    return key_hash;
  }
};

class EXPORT grouped_xframe: public model_base {
 public:
  /**
   * Groups an XFrame by the distinct values in one or more columns.
   *
   * Logically, this creates an XFrame for each "group" of values, where the
   * new XFrames all have the same number of columns as the original XFrame.
   * These XFrames are accessed through the interface of this data structure.
   *
   * If is_grouped is true, this function skips the grouping step and just sets
   * up the data structure to provide an interface to the grouped XFrame.
   *
   * Throws if group has already been called on this object, or the column
   * names are not valid.
   */
  void group(const gl_xframe &sf, const std::vector<std::string> column_names,
      bool is_grouped);


  /**
   * Get the XFrame that corresponds to the group named `key`.
   *
   * Each group's name is its distinct value, including its type. This means
   * that an XFrame grouped by a column of integers that has some 1s and some
   * 2s, the name of the group with ones is the integer 1, not the string '1'.
   * The key is given as a vector because more than one columns can be used to
   * group.
   */
  gl_xframe get_group(std::vector<flexible_type> key);

  /**
   * The number of distinct groups found.
   */
  inline size_t num_groups() const {
    return m_range_directory.size();
  }

  /**
   * A list of all the group names.
   */
  gl_sarray groups();

  /**
   * Begin iteration through the grouped XFrame.
   *
   * Works together with \ref iterator_get_next(). The usage pattern
   * is as follows:
   * \code
   * grouped_xframe.begin_iterator();
   * while(1) {
   *   auto ret = grouped_xframe.iterator_get_next(64);
   *   // do stuff
   *   if (ret.size() < 64) {
   *     // we are done
   *     break;
   *   }
   * }
   * \endcode
   */
  inline void begin_iterator() {
    m_iterating = true;
    m_cur_iterator_idx = 0;
  }

  /**
   * Obtains the next block of elements of size len from the grouped XFrame.
   * Works together with \ref begin_iterator(). See the code example
   * in \ref begin_iterator() for details.
   *
   * This function will always return a vector of length 'len' unless
   * at the end of the array, or if an error has occured.
   *
   * The element value is a pair of <group name, XFrame>.
   *
   * \param len The number of elements to return
   * \returns The next collection of elements in the array. Returns less then
   * len elements on end of file or failure.
   */
  std::vector<std::pair<flexible_type,gl_xframe>> iterator_get_next(size_t len);

  /**
   * Returns a single XFrame which contains all the data.
   */
  gl_xframe get_xframe() const {
    return m_grouped_sf;
  }

  /**
   * Return an XFrame with group_info i.e key columns + number of rows in each
   * key column.
   */
  gl_xframe group_info() const;

 protected:
 private:
  /// Methods

  /**
   * Get a group by its index in the range directory.
   *
   * Internal method
   */
  gl_xframe get_group_by_index(size_t range_dir_idx);

  /// Variables
  gl_xframe m_grouped_sf;

  // The first row in each range. The sequential order of the vector corresponds
  // to where the group is located in the underlying xframe e.g. 1st group in
  // the XFrame's last row is m_range_directory[0]. This data structure only
  // exists to preserve the ORDER of groups: the order the XFrame is sorted in.
  // This may have some significance.
  std::vector<size_t> m_range_directory;
  std::vector<std::string> m_key_col_names;
  std::vector<flexible_type> m_group_names;

  // Key: Hash value of "group" key
  // Value: Index of m_range_directory
  //TODO: This is what will run out of memory first when scaling up
  std::unordered_map<std::vector<flexible_type>, size_t, GroupKeyHash> m_key2range;
  gl_sarray m_groups_sa;
  bool m_inited = false;
  flex_type_enum m_group_type = flex_type_enum::UNDEFINED;
  bool m_iterating = false;
  size_t m_cur_iterator_idx = 0;

 public:
  BEGIN_CLASS_MEMBER_REGISTRATION("grouped_xframe")
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::group, "data", "column_names",
      "is_grouped")
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::get_group, "key")
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::num_groups)
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::groups)
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::begin_iterator)
  REGISTER_CLASS_MEMBER_FUNCTION(grouped_xframe::iterator_get_next, "num_items")

  REGISTER_GETTER("xframe", grouped_xframe::get_xframe)
  END_CLASS_MEMBER_REGISTRATION
};

} // namespace turi
#endif // TURI_GROUPED_XFRAME_HPP
