/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <core/storage/xframe_data/xframe_iterators.hpp>
#include <core/storage/xframe_data/xframe_config.hpp>

namespace turi {

/**
 *  Set the global block to read. This allows us to create the initializer
 *  only once and change the row_start and row_end multiple times.
 *
 */
void parallel_xframe_iterator_initializer::set_global_block(
                  size_t _row_start,
                  size_t _row_end){

  row_start = _row_start;
  row_end = _row_end;

  // Make sure the row_end can't be more than sf_size
  if ((row_end == (size_t) -1) || (_row_end >= sf_size)){
    row_end = sf_size;
  } else {
    row_end = _row_end;
  }
  global_block_size = row_end - row_start;
}


/**
 * Initialize the Parallel XFrame iterator.
 * \note This operation is more expensive than the XFrame iterator creation.
 */
parallel_xframe_iterator_initializer::parallel_xframe_iterator_initializer(
    const std::vector<xframe>& data_sources,
    const size_t& _row_start,
    const size_t& _row_end) {

  column_offsets.clear();
  sources.clear();

  DASSERT_FALSE(data_sources.empty());

  size_t current_offset = 0;

  sf_size = data_sources.front().size();

  // Get each of the columns we want.
  for(const xframe& sf : data_sources) {
    column_offsets.push_back(current_offset);
    current_offset += sf.num_columns();

    for(size_t i = 0; i < sf.num_columns(); ++i) {
      sources.push_back(sf.select_column(i)->get_reader());
      ASSERT_EQ(sf.size(), sf_size);
    }
  }
  // One last one
  column_offsets.push_back(current_offset);
  set_global_block(_row_start, _row_end);

}

/**
 * Create an XFrame parallel iterator.
 */
parallel_xframe_iterator::parallel_xframe_iterator(
    const parallel_xframe_iterator_initializer& it_init, size_t thread_idx, size_t num_threads)
    : sources(it_init.sources)
    , column_offsets(it_init.column_offsets)
{

  DASSERT_LT(thread_idx, num_threads);

  buffers.resize(sources.size());

  start_idx = it_init.row_start +
        (thread_idx * it_init.global_block_size) / num_threads;
  end_idx   = it_init.row_start +
        ((thread_idx + 1) * it_init.global_block_size) / num_threads;

  max_block_size = std::min(xframe_config::XFRAME_READ_BATCH_SIZE, end_idx - start_idx);
  for(auto& b : buffers)
    b.reserve(max_block_size);
  reset();
}

/**
 * Load the current block
 */
void parallel_xframe_iterator::load_current_block() {
  DASSERT_EQ(current_idx, block_end_idx);

  block_start_idx = current_idx;
  block_end_idx = std::min(end_idx, block_end_idx + max_block_size);

  if(block_start_idx == block_end_idx) {
    for(size_t i = 0; i < buffers.size(); ++i)
      buffers[i].clear();
  }

  for(size_t i = 0; i < sources.size(); ++i) {
    sources[i]->read_rows(block_start_idx, block_end_idx, buffers[i]);
  }
}

}
