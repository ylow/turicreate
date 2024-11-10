/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_ITERATORS_H_
#define TURI_XFRAME_ITERATORS_H_

#include <vector>
#include <core/storage/xframe_data/xframe.hpp>
#include <core/logging/assertions.hpp>

namespace turi {

class parallel_xframe_iterator;


/**
 * \ingroup xframe_physical
 * \addtogroup xframe_main Main XFrame Objects
 * \{
 */

/**
 * Utlity to provide parallel iteration over an XFrame
 */
class parallel_xframe_iterator_initializer {
 public:

  /** Create an xframe iterator initializer initialized with a single
   *  xframe.  The xframe is divided into num_threads blocks of
   *  approximately equal size.  This iterator claims the thread_idx
   *  block.
   *
   * \param[in]  data_sources Collection of XFrames
   * \param[in] row_start First row to read
   * \param[in] row_end   One past last row to read (i.e. EXCLUSIVE).
   *                      Row_end can be beyond the end of the array, in which
   *                      case, fewer rows will be read.
   *                      Default -1 reads all rows.
   *
   */
  explicit parallel_xframe_iterator_initializer(xframe data,
                                                const size_t& _row_start=0,
                                                const size_t& _row_end =-1)
      : parallel_xframe_iterator_initializer(std::vector<xframe>{data}, _row_start, _row_end)
  {}


  /** Initialize the xframe iterator with a vector of xframes.  Each
   *  xframe is divided into num_threads blocks of approximately equal
   *  size.  This iterator claims the thread_idx block.
   *
   *  With multiple xframes, elements in the current row can be
   *  accessed by it.value(xframe_index, column_index), where
   *  xframe_index refers to the index in data_sources, and
   *  column_index refers to the index of the column within that
   *  xframe.
   *
   * \param[in]  data_sources Collection of XFrames
   * \param[in] row_start First row to read
   * \param[in] row_end   One past last row to read (i.e. EXCLUSIVE).
   *                      Row_end can be beyond the end of the array, in which
   *                      case, fewer rows will be read.
   *                      Default -1 reads all rows.
   */
  explicit parallel_xframe_iterator_initializer(const std::vector<xframe>& data_sources,
                                                const size_t& _row_start=0,
                                                const size_t& _row_end =-1);

  /**
   *  Set the global block to read. This allows us to create the initializer
   *  only once and change the row_start and row_end multiple times.
   *
   * \param[in] row_start First row to read
   * \param[in] row_end   One past last row to read (i.e. EXCLUSIVE).
   *                      Row_end can be beyond the end of the array, in which
   *                      case, fewer rows will be read.
   *                      Default -1 reads all rows.
   */
  void set_global_block(size_t _row_start=0, size_t _row_end=-1);

 private:

  friend class parallel_xframe_iterator;
  size_t row_start = 0;                 /**< Row start for global block.*/
  size_t row_end = -1;                  /**< Row end of the global block.*/

  size_t global_block_size;         /**< Global block size being read.*/
  size_t sf_size;                   /**< XFrame size. */

  std::vector<std::shared_ptr<sarray<flexible_type>::reader_type> > sources;
  std::vector<size_t> column_offsets;

};

/**
 *  A simple convienience iterator for doing parallel iteration over
 *  the rows of one or more xframes.  It is designed for easy
 *  integration with the in_parallel function.
 *
 * This iterator class provides two features:
 *
 *  1. The ability to easily and efficiently iterate over multiple
 *  sections of an xframe, divided evenly by thread.
 *
 *  2. The ability to easily iterate over multiple xframes of the same
 *  length simultaneously.
 *
 *
 *  To use this iterator:
 *
 *  parallel_xframe_iterator_initializer it_init(data);
 *
 *  in_parallel([&](size_t thread_idx, size_t num_threads) {
 *      for(parallel_xframe_iterator it(it_init, thread_idx, num_threads); !it.done(); ++it) {
 *          size_t row_idx = it.row_index();
 *          double value_0 = it.value(0);
 *          double value_1 = it.value(1);
 *          ...
 *      }
 *  });
 *
 */
class parallel_xframe_iterator {

 public:

  /**
   * Default empty constructor.
   */
  parallel_xframe_iterator()
      : current_idx(0), start_idx(0), end_idx(0)
      , block_start_idx(0), block_end_idx(0), max_block_size(0)
  {}


  /**
   *  Initialize the xframe iterator with a single xframe.  The xframe
   *  is divided into num_threads blocks of approximately equal size.
   *  This iterator claims the thread_idx block.
   *
   * \param[in]  data XFrame
   * \param[in]  thread_idx  Thread id (Ranges from 0-num_threads)
   * \param[in]  num_threads Number of threads in the turi::thread_pool
   *
   */
  explicit parallel_xframe_iterator(xframe data, size_t thread_idx = 0, size_t num_threads = 1)
      : parallel_xframe_iterator(parallel_xframe_iterator_initializer(data), thread_idx, num_threads)
  {}

  /**  Initialize the xframe iterator with a vector of xframes.  The
   *  xframe is divided into num_threads blocks of approximately equal
   *  size.  This iterator claims the thread_idx block.
   *
   * \param[in]  data XFrame
   * \param[in]  thread_idx  Thread id (Ranges from 0-num_threads)
   * \param[in]  num_threads Number of threads in the turi::thread_pool
   *
   */
  explicit parallel_xframe_iterator(const std::vector<xframe>& data, size_t thread_idx = 0, size_t num_threads = 1)
      : parallel_xframe_iterator(parallel_xframe_iterator_initializer(data), thread_idx, num_threads)
  {}

  /**
   * Initialize the parallel XFrame iterator.
   *
   * \param[in]  data        Parallel xframe initializer
   * \param[in]  thread_idx  Thread id (Ranges from 0-num_threads)
   * \param[in]  num_threads Number of threads in the turi::thread_pool
   */
  parallel_xframe_iterator(const parallel_xframe_iterator_initializer& data,
                           size_t thread_idx, size_t num_threads);

  /**
   * Increments the parallel XFrame iterator to the next row.
   */
  inline const parallel_xframe_iterator& operator++() {
    DASSERT_GE(current_idx, start_idx);
    DASSERT_LT(current_idx, end_idx);

    ++current_idx;

    if(current_idx != end_idx && current_idx == block_end_idx)
      load_current_block();

    return *this;
  }

  /**
   *  Check if the iterator is done (applies to the global block)
   *  \returns True if the iterator is done.
   */
  bool done() const {
    DASSERT_GE(current_idx, start_idx);
    DASSERT_LE(current_idx, end_idx);
    return current_idx == end_idx;
  }

  /**
   *  Resets the iterator to the state it had upon initialization.
   */
  void reset() {
    current_idx = start_idx;
    block_start_idx = start_idx;
    block_end_idx = start_idx;

    load_current_block();
  }

  /**
   * Returns the current row index that the iterator is at.
   */
  size_t row_index() const {
    return current_idx;
  }

  /**
   * Fills a vector x with the current row of data, If there are
   * multiple xframes provided initially, then values from all columns
   * are concatenated into a single vector of length
   * data_sources[0].num_columns() + data_sources[1].num_columns() +
   * ...
   *
   * \param[in,out] x  Fills the std::vector with the contents of the XFrame
   */
  void fill(std::vector<flexible_type>& x) const {
    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    x.resize(buffers.size());

    size_t idx = current_idx - block_start_idx;
    for(size_t i = 0; i < buffers.size(); ++i)
      x[i] = buffers[i][idx];
  }

  /**
   * Fills a vector x with the current row of data from
   * data_sources[idx].
   *
   * \param[in]     idx XFrame idx to use.
   * \param[in,out] x   Fills the std::vector with the contents of the XFrame
   */
  void fill(size_t xframe_idx, std::vector<flexible_type>& x) const {
    DASSERT_LT(xframe_idx, column_offsets.size() - 1);
    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    size_t start_col_idx = column_offsets[xframe_idx];
    size_t end_col_idx   = column_offsets[xframe_idx + 1];

    x.resize(end_col_idx - start_col_idx);

    size_t idx = current_idx - block_start_idx;
    size_t i = 0;
    for(size_t col_idx = start_col_idx; col_idx < end_col_idx; ++col_idx, ++i)
      x[i] = buffers[col_idx][idx];
  }

  /**
   * Returns the current value in xframe data_sources[xframe_idx],
   * column column_idx.
   *
   * \param[in] xframe_idx XFrame idx.
   * \param[in] column_idx Column idx.
   *
   * \returns Value corresponding to XFrame[xframe_idx][column_idx]
   */
  const flexible_type& value(size_t xframe_idx, size_t column_idx) const {
    DASSERT_LT(xframe_idx, column_offsets.size() - 1);

    size_t row_idx = column_offsets[xframe_idx] + column_idx;

    DASSERT_LT(row_idx, column_offsets[xframe_idx + 1]);

    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    return buffers[row_idx][current_idx - block_start_idx];
  }

  /**
   *  Returns the current value in column_idx of the first xframe 0.
   *  If multiple xframes are provided at initialization time, then
   *  this indexes the values as if all the columns were concatenated
   *  (in similar fashion to fill(x); )
   *
   * \param[in] xframe_idx XFrame idx.
   *
   * \returns Value corresponding to XFrame[0][column_idx]
   */
  const flexible_type& value(size_t idx) const {
    DASSERT_LT(idx, buffers.size());

    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    return buffers[idx][current_idx - block_start_idx];
  }

  /**\overload
   *
   * Exactly like value(...), except it returns a move reference to
   * the current value, invalidating the present one.
   *
   * \param[in] xframe_idx XFrame idx.
   * \param[in] column_idx Column idx.
   *
   * \returns Moved value corresponding to XFrame[xframe_idx][column_idx]
   */
  flexible_type&& move_value(size_t xframe_idx, size_t column_idx) {
    DASSERT_LT(xframe_idx, column_offsets.size() - 1);

    size_t row_idx = column_offsets[xframe_idx] + column_idx;

    DASSERT_LT(row_idx, column_offsets[xframe_idx + 1]);

    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    return std::move(buffers[row_idx][current_idx - block_start_idx]);
  }

  /** \overload
   *  Returns a move reference to the current value in column_idx of
   *  the first xframe 0, invalidating that reference.  If multiple
   *  xframes are provided at initialization time, then this indexes
   *  the values as if all the columns were concatenated (in similar
   *  fashion to fill(x); )
   *
   * \param[in] xframe_idx XFrame idx.
   *
   * \returns Value corresponding to XFrame[0][column_idx]
   */
  flexible_type&& move_value(size_t idx) {
    DASSERT_LT(idx, buffers.size());

    DASSERT_GE(current_idx, block_start_idx);
    DASSERT_LT(current_idx, block_end_idx);

    return std::move(buffers[idx][current_idx - block_start_idx]);
  }


 private:

  /**
   *  Loads the current block.
   */
  void load_current_block();

  size_t current_idx;              /**< Current id of the iterator.*/
  size_t start_idx;                /**< Row start for global block.*/
  size_t end_idx;                  /**< Row end for global block.*/

  size_t block_start_idx;          /**< Row start for current block.*/
  size_t block_end_idx;            /**< Row start for current block.*/
  size_t max_block_size;           /**< Max block size.*/

  std::vector< std::vector<flexible_type> > buffers;
  std::vector<std::shared_ptr<sarray<flexible_type>::reader_type> > sources;
  std::vector<size_t> column_offsets;
};

/// \}
}

#endif /* TURI_XFRAME_ITERATORS_H_ */
