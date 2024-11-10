/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include<core/storage/xframe_data/shuffle.hpp>
#include<core/storage/xframe_data/xframe_rows.hpp>
#include<core/storage/fileio/buffered_writer.hpp>
#include<memory>

namespace turi {

std::vector<xframe> shuffle(
    xframe xframe_in,
    size_t n,
    std::function<size_t(const std::vector<flexible_type>&)> hash_fn,
    std::function<void(const std::vector<flexible_type>&, size_t)> emit_call_back) {

    ASSERT_GT(n, 0);

    // split the work to threads
    // for n bins let's assign n / log(n) workers, assuming rows are evenly distributed.
    size_t num_rows = xframe_in.num_rows();
    size_t num_workers = turi::thread::cpu_count();
    size_t rows_per_worker = num_rows / num_workers;

    // prepare the out xframe
    std::vector<xframe> xframe_out;
    std::vector<xframe::iterator> xframe_out_iter;
    xframe_out.resize(n);
    for (auto& sf: xframe_out) {
      sf.open_for_write(xframe_in.column_names(), xframe_in.column_types(), "",  1);
      xframe_out_iter.push_back(sf.get_output_iterator(0));
    }
    std::vector<std::unique_ptr<turi::mutex>> xframe_out_locks;
    for (size_t i = 0; i < n; ++i) {
      xframe_out_locks.push_back(std::unique_ptr<turi::mutex>(new turi::mutex));
    }

    auto reader = xframe_in.get_reader();
    parallel_for(0, num_workers, [&](size_t worker_id) {
        size_t start_row = worker_id * rows_per_worker;
        size_t end_row = (worker_id == (num_workers-1)) ? num_rows
                                                        : (worker_id + 1) * rows_per_worker;

        // prepare thread local output buffer for each xframe
        std::vector<buffered_writer<std::vector<flexible_type>, xframe::iterator>> writers;
        for (size_t i = 0; i < n; ++i) {
          writers.push_back(
            buffered_writer<std::vector<flexible_type>, xframe::iterator>
            (xframe_out_iter[i], *xframe_out_locks[i],
             XFRAME_WRITER_BUFFER_SOFT_LIMIT, XFRAME_WRITER_BUFFER_HARD_LIMIT)
          );
        }

        while (start_row < end_row) {
          // read a chunk of rows to shuffle
          xframe_rows rows;
          size_t rows_to_read = std::min<size_t>((end_row - start_row), DEFAULT_SARRAY_READER_BUFFER_SIZE);
          size_t rows_read = reader->read_rows(start_row, start_row + rows_to_read, rows);
          DASSERT_EQ(rows_read, rows_to_read);
          start_row += rows_read;

          for (auto& row : rows) {
            size_t out_index = hash_fn(row) % n;
            if (emit_call_back) {
              emit_call_back(row, worker_id);
            }
            writers[out_index].write(std::move(row));
          }
        } // end of while

        // flush the rest of the buffer
        for (size_t i = 0; i < n; ++i) {
          writers[i].flush();
        }
    });

    // close all xframe writers
    for (auto& sf: xframe_out) {
      sf.close();
    }
    return xframe_out;
}
}
