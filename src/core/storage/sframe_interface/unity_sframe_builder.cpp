/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <core/storage/xframe_interface/unity_xframe_builder.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>

namespace turi {

void unity_xframe_builder::init(size_t num_segments,
                                size_t history_size,
                                std::vector<std::string> column_names,
                                std::vector<flex_type_enum> column_types,
                                std::string save_location) {
  if(m_inited)
    log_and_throw("This xframe_builder has already been initialized!");

  //m_xframe = std::make_shared<xframe>();
  if(save_location.size() > 0) {
    try {
      m_dirarc.open_directory_for_write(save_location);
      m_dirarc.set_metadata("contents", "xframe");
      std::string prefix = m_dirarc.get_next_write_prefix();
      m_xframe_index_file = prefix + ".frame_idx";
    } catch(...) {
      throw;
    }
  }

  m_xframe.open_for_write(column_names, column_types, m_xframe_index_file, num_segments);
  m_out_iters.resize(num_segments);
  m_history.resize(num_segments);
  for(size_t i = 0; i < num_segments; ++i) {
    m_out_iters[i] = m_xframe.get_output_iterator(i);
    m_history[i] = std::make_shared<row_history_t>(history_size);
  }

  m_inited = true;
}

void unity_xframe_builder::append(const std::vector<flexible_type> &row, size_t segment) {
  if(!m_inited)
    log_and_throw("Must call 'init' first!");

  if(m_closed)
    log_and_throw("Cannot append values when closed.");

  if(segment >= m_out_iters.size()) {
    log_and_throw("Invalid segment number!");
  }

  m_history[segment]->push_back(row);

  *(m_out_iters[segment]) = row;
}

void unity_xframe_builder::append_multiple(const std::vector<std::vector<flexible_type>> &vals, size_t segment) {
  for(const auto &i : vals) {
    this->append(i, segment);
  }
}

std::vector<std::string> unity_xframe_builder::column_names() {
  return m_xframe.column_names();
}

std::vector<flex_type_enum> unity_xframe_builder::column_types() {
  return m_xframe.column_types();
}

std::vector<std::vector<flexible_type>> unity_xframe_builder::read_history(
    size_t num_elems, size_t segment) {
  if(!m_inited)
    log_and_throw("Must call 'init' first!");

  if(m_closed)
    log_and_throw("History is invalid when closed.");

  if(segment >= m_history.size())
    log_and_throw("Invalid segment.");

  auto history = m_history[segment];

  if(num_elems > history->size())
    num_elems = history->size();
  if(num_elems == size_t(-1))
    num_elems = history->size();

  std::vector<std::vector<flexible_type>> ret_vec(num_elems);

  if(num_elems == 0)
    return ret_vec;

  std::copy_n(history->rbegin(), num_elems, ret_vec.rbegin());

  return ret_vec;
}

std::shared_ptr<unity_xframe_base> unity_xframe_builder::close() {
  if(!m_inited)
    log_and_throw("Must call 'init' first!");

  if(m_closed)
    log_and_throw("Already closed.");

  m_xframe.close();
  if(m_xframe_index_file.size() > 0) {
    m_dirarc.close();
  }

  m_closed = true;
  auto ret = std::make_shared<unity_xframe>();
  ret->construct_from_xframe(m_xframe);
  return ret;
}

} // namespace turi
