/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_UNITY_XFRAME_BUILDER_HPP
#define TURI_UNITY_XFRAME_BUILDER_HPP

#include <vector>
#include <set>
#include <core/storage/xframe_data/xframe.hpp>
#include <boost/circular_buffer.hpp>
#include <model_server/lib/api/unity_xframe_builder_interface.hpp>

typedef boost::circular_buffer<std::vector<turi::flexible_type>> row_history_t;

namespace turi {

/**
 * Provides a Python interface to incrementally build an XFrame.
 *
 * Unlike most other unity objects, this is not a wrapper of another
 * "xframe_builder" class, but provides the implementation. This is because it
 * is a slightly embellished wrapper around the SArray's output iterator, so
 * there is no further functionality that needs to be available for the C++
 * side.
 *
 * The unity_xframe_builder is designed to append values until \ref close is
 * called, which returns the XFrame. No "reopening" is allowed, and no
 * operations in that instance of unity_xframe_builder will work after close is
 * called.
 *
 * This also doesn't wrap the already existing \ref unity_sarray_builder
 * despite its similarity, because using the xframe output iterator allows for
 * multiple columns to be kept in the same file.
 */
class unity_xframe_builder: public unity_xframe_builder_base {
 public:
  /**
   * Default constructor. Does nothing
   */
  unity_xframe_builder() {}

  /**
   * Initialize the unity_sarray_buidler.
   *
   * This essentially opens the output iterator for writing. Column names and
   * column types are required arguments.
   */
  void init(size_t num_segments,
      size_t history_size,
      std::vector<std::string> column_names,
      std::vector<flex_type_enum> column_types,
      std::string save_location);

  /**
   * Add a single row of flexible_types to the XFrame.
   *
   * The \p segment number allows the user to use the parallel interface provided
   * by the underlying output_iterator.
   *
   * Throws if:
   *  - init hasn't been called or close has been called
   *  - segment number is invalid
   *  - the type of \p row differs from the type of the elements already
   *    appended (except if only UNDEFINED elements have been appended).
   *
   */
  void append(const std::vector<flexible_type> &row, size_t segment);

  /**
   * A wrapper of \ref append which adds multiple rows to XFrame.
   *
   * Throws if:
   *  - init hasn't been called or close has been called
   *  - segment number is invalid
   *  - the type of any values in \p rows differs from the type of the
   *    elements already appended (except if only UNDEFINED elements have been
   *    appended).
   */
  void append_multiple(const std::vector<std::vector<flexible_type>> &rows,
      size_t segment);

  /**
   * Return the column names of the future XFrame.
   */
  std::vector<std::string> column_names();

  /**
   * Return the column types of the future XFrame.
   */
  std::vector<flex_type_enum> column_types();

  /**
   * Return the last \p num_elems rows appended.
   */
  std::vector<std::vector<flexible_type>> read_history(size_t num_elems,
      size_t segment);

  /**
   * Finalize XFrame and return it.
   */
  std::shared_ptr<unity_xframe_base> close();

  unity_xframe_builder(const unity_xframe_builder&) = delete;
  unity_xframe_builder& operator=(const unity_xframe_builder&) = delete;
 private:
  /// Methods

  /// Variables
  bool m_inited = false;
  bool m_closed = false;
  xframe m_xframe;
  std::vector<xframe::iterator> m_out_iters;
  std::string m_xframe_index_file;

  std::vector<std::shared_ptr<row_history_t>> m_history;

  dir_archive m_dirarc;

};

} // namespace turi
#endif // TURI_UNITY_XFRAME_BUILDER_HPP
