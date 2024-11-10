/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <set>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>
#include <core/storage/xframe_data/sarray.hpp>
#include <core/storage/xframe_data/xframe.hpp>
#include <core/storage/xframe_data/xframe_saving.hpp>
#include <core/storage/xframe_data/xframe_config.hpp>
#include <core/storage/xframe_data/sarray.hpp>
#include <core/storage/xframe_data/algorithm.hpp>
#include <core/storage/fileio/temp_files.hpp>
#include <core/storage/fileio/sanitize_url.hpp>
#include <model_server/lib/unity_global.hpp>
#include <model_server/lib/unity_global_singleton.hpp>
#include <core/storage/xframe_data/groupby_aggregate.hpp>
#include <core/storage/xframe_data/groupby_aggregate_operators.hpp>
#include <core/storage/xframe_data/csv_line_tokenizer.hpp>
#include <core/storage/xframe_data/csv_writer.hpp>
#include <core/data/flexible_type/flexible_type_spirit_parser.hpp>
#include <core/storage/xframe_data/join.hpp>
#include <model_server/lib/auto_close_sarray.hpp>
#include <core/storage/query_engine/planning/planner.hpp>
#include <core/storage/query_engine/planning/optimization_engine.hpp>
#include <core/storage/query_engine/operators/all_operators.hpp>
#include <core/storage/query_engine/operators/operator_properties.hpp>
#include <core/storage/query_engine/algorithm/sort.hpp>
#include <core/storage/query_engine/algorithm/ec_sort.hpp>
#include <core/storage/query_engine/algorithm/groupby_aggregate.hpp>
#include <core/storage/query_engine/operators/operator_properties.hpp>
#include <core/system/exceptions/error_types.hpp>

#include <visualization/server/plot.hpp>
#include <visualization/server/process_wrapper.hpp>
#include <visualization/server/histogram.hpp>
#include <visualization/server/escape.hpp>
#include <visualization/server/columnwise_summary.hpp>
#include <visualization/server/item_frequency.hpp>
#include <visualization/server/transformation.hpp>
#include <visualization/server/thread.hpp>
#include <visualization/server/summary_view.hpp>
#include <visualization/server/table.hpp>

#include <algorithm>
#include <random>
#include <string>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <core/logging/logger.hpp>

#ifdef TC_HAS_PYTHON
#include <core/system/lambda/pylambda_function.hpp>
#endif

namespace turi {

using namespace turi::query_eval;

static std::shared_ptr<xframe> get_empty_xframe() {
  // make empty xframe and keep it around, reusing it whenever
  // I need an empty xframe. We are intentionally leaking this object.
  // Otherwise the termination of this will race against the cleanup of the
  // cache files.
  static std::shared_ptr<xframe>* sf = nullptr;
  static turi::mutex static_sf_lock;
  std::lock_guard<turi::mutex> guard(static_sf_lock);
  if (sf == nullptr) {
    sf = new std::shared_ptr<xframe>();
    (*sf) = std::make_shared<xframe>();
    (*sf)->open_for_write({}, {}, "", 1);
    (*sf)->close();
  }
  return *sf;
}
unity_xframe::unity_xframe() {
  this->set_xframe(get_empty_xframe());
}

unity_xframe::~unity_xframe() { clear(); }

void unity_xframe::construct_from_dataframe(const dataframe_t& df) {
  log_func_entry();
  clear();
  this->set_xframe(std::make_shared<xframe>(df));
}

void unity_xframe::construct_from_xframe(const xframe& sf) {
  log_func_entry();
  clear();
  this->set_xframe(std::make_shared<xframe>(sf));
}

void unity_xframe::construct_from_xframe_index(std::string location) {
  logstream(LOG_INFO) << "Construct xframe from location: " << sanitize_url(location) << std::endl;
  clear();

  auto status = fileio::get_file_status(location);

  if ((status.first == fileio::file_status::REGULAR_FILE ||
       status.first == fileio::file_status::FS_UNAVAILABLE ||
       status.first == fileio::file_status::MISSING) &&
      fileio::is_web_protocol(fileio::get_protocol(location))) {
    // if it is a web protocol, we cannot be certain what type of file it is.
    // HEURISTIC:
    //   if we can open it, it is a regular file. Otherwise not.
    if (fileio::try_to_open_file(location + "/dir_archive.ini")) {
      status.first = fileio::file_status::DIRECTORY;
      status.second.clear();
    }
  }

  if (status.first == fileio::file_status::MISSING) {
    // missing file. fail quick
    log_and_throw_io_failure(sanitize_url(location) +
                             " not found. ErrMsg: " + status.second);
  } if (status.first == fileio::file_status::REGULAR_FILE) {
    // its a regular file, load it normally
    auto xframe_ptr = std::make_shared<xframe>(location);
    this->set_xframe(xframe_ptr);
  } else if (status.first == fileio::file_status::DIRECTORY) {
    // its a directory, open the directory and verify that it contains an
    // sarray and then load it if it does
    dir_archive dirarc;
    dirarc.open_directory_for_read(location);
    std::string content_value;
    if (dirarc.get_metadata("contents", content_value) == false ||
        content_value != "xframe") {
      log_and_throw_io_failure("Archive does not contain an XFrame");
    }
    std::string prefix = dirarc.get_next_read_prefix();
    auto xframe_ptr = std::make_shared<xframe>(prefix + ".frame_idx");
    this->set_xframe(xframe_ptr);
    dirarc.close();
  } else if(status.first == fileio::file_status::FS_UNAVAILABLE) {
    log_and_throw_io_failure(
        "Cannot read from filesystem. Check log for details. ErrMsg: " +
        status.second);
  }
}

std::map<std::string, std::shared_ptr<unity_sarray_base>> unity_xframe::construct_from_csvs(
    std::string url,
    std::map<std::string, flexible_type> csv_parsing_config,
    std::map<std::string, flex_type_enum> column_type_hints) {

  logstream(LOG_INFO) << "Construct xframe from csvs at "
                      << sanitize_url(url) << std::endl;
  std::stringstream ss;
  ss << "Parsing config:\n";
  for (auto& pair: csv_parsing_config) {
    ss << "\t" << pair.first << ": " << pair.second << "\n";
  }
  logstream(LOG_INFO) << ss.str();

  clear();
  csv_line_tokenizer tokenizer;
  // first the defaults
  bool use_header = true;
  bool continue_on_failure = false;
  bool store_errors = false;
  size_t row_limit = 0;
  size_t skip_rows = 0;
  std::vector<std::string> output_columns;
  tokenizer.delimiter = ",";
  tokenizer.has_comment_char = false;
  tokenizer.escape_char = '\\';
  tokenizer.use_escape_char = true;
  tokenizer.double_quote = true;
  tokenizer.quote_char = '\"';
  tokenizer.skip_initial_space = true;
  tokenizer.na_values.clear();

  if (csv_parsing_config.count("use_header")) {
    use_header = !csv_parsing_config["use_header"].is_zero();
  }
  if (csv_parsing_config.count("continue_on_failure")) {
    continue_on_failure = !csv_parsing_config["continue_on_failure"].is_zero();
  }
  if (csv_parsing_config.count("store_errors")) {
    store_errors = !csv_parsing_config["store_errors"].is_zero();
  }
  if (csv_parsing_config.count("row_limit")) {
    row_limit = (flex_int)(csv_parsing_config["row_limit"]);
  }
  if (csv_parsing_config.count("skip_rows")) {
    skip_rows = (flex_int)(csv_parsing_config["skip_rows"]);
  }
  if (csv_parsing_config["delimiter"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)csv_parsing_config["delimiter"];
    tokenizer.delimiter = tmp;
  } else if (csv_parsing_config["delimiter"].get_type() == flex_type_enum::UNDEFINED) {
    tokenizer.delimiter = "";
  }
  if (csv_parsing_config["comment_char"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)csv_parsing_config["comment_char"];
    if (tmp.length() > 0) {
      tokenizer.comment_char= tmp[0];
      tokenizer.has_comment_char = true;
    }
  }
  if (csv_parsing_config.count("use_escape_char")) {
    tokenizer.skip_initial_space = !csv_parsing_config["use_escape_char"].is_zero();
  }
  if (csv_parsing_config["escape_char"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)csv_parsing_config["escape_char"];
    if (tmp.length() > 0) tokenizer.escape_char = tmp[0];
  }
  if (csv_parsing_config.count("double_quote")) {
    tokenizer.double_quote = !csv_parsing_config["double_quote"].is_zero();
  }
  if (csv_parsing_config["quote_char"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)csv_parsing_config["quote_char"];
    if (tmp.length() > 0) tokenizer.quote_char = tmp[0];
  } else if (csv_parsing_config["quote_char"].get_type() == flex_type_enum::UNDEFINED) {
    tokenizer.quote_char = NULL;
  }
  if (csv_parsing_config.count("skip_initial_space")) {
    tokenizer.skip_initial_space = !csv_parsing_config["skip_initial_space"].is_zero();
  }
  if (csv_parsing_config.count("only_raw_string_substitutions")) {
    tokenizer.only_raw_string_substitutions = !csv_parsing_config["only_raw_string_substitutions"].is_zero();
  }
  if (csv_parsing_config["na_values"].get_type() == flex_type_enum::LIST) {
    flex_list rec = csv_parsing_config["na_values"];
    tokenizer.na_values.clear();
    for (size_t i = 0;i < rec.size(); ++i) {
      if (rec[i].get_type() == flex_type_enum::STRING) {
        tokenizer.na_values.push_back((std::string)rec[i]);
      }
    }
  }
  if (csv_parsing_config["line_terminator"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)csv_parsing_config["line_terminator"];
    tokenizer.line_terminator = tmp;
  } else if (csv_parsing_config["line_terminator"].get_type() == flex_type_enum::UNDEFINED) {
    tokenizer.line_terminator = "";
  }
  if (csv_parsing_config["output_columns"].get_type() == flex_type_enum::LIST) {
    flex_list rec = csv_parsing_config["output_columns"];
    output_columns.clear();
    for (size_t i = 0;i < rec.size(); ++i) {
      if (rec[i].get_type() == flex_type_enum::STRING) {
        output_columns.push_back((std::string)rec[i]);
      }
    }
  }
  if (csv_parsing_config["true_values"].get_type() == flex_type_enum::LIST) {
    flex_list rec = csv_parsing_config["true_values"];
    std::unordered_set<std::string> true_values;
    tokenizer.true_values.clear();
    for (size_t i = 0;i < rec.size(); ++i) {
      if (rec[i].get_type() == flex_type_enum::STRING) {
        tokenizer.true_values.insert((std::string)rec[i]);
      }
    }
  }

  if (csv_parsing_config["false_values"].get_type() == flex_type_enum::LIST) {
    flex_list rec = csv_parsing_config["false_values"];
    std::unordered_set<std::string> false_values;
    tokenizer.false_values.clear();
    for (size_t i = 0;i < rec.size(); ++i) {
      if (rec[i].get_type() == flex_type_enum::STRING) {
        tokenizer.false_values.insert((std::string)rec[i]);
      }
    }
  }
  tokenizer.init();

  auto xframe_ptr = std::make_shared<xframe>();

  auto errors = xframe_ptr->init_from_csvs(url,
                                           tokenizer,
                                           use_header,
                                           continue_on_failure,
                                           store_errors,
                                           column_type_hints,
                                           output_columns,
                                           row_limit,
                                           skip_rows);

  this->set_xframe(xframe_ptr);

  std::map<std::string, std::shared_ptr<unity_sarray_base>> errors_unity;
  for (auto& kv : errors) {
    std::shared_ptr<unity_sarray> sa(new unity_sarray());
    sa->construct_from_sarray(kv.second);
    errors_unity.insert(std::make_pair(kv.first, sa));
  }

  return errors_unity;
}


void unity_xframe::construct_from_planner_node(std::shared_ptr<planner_node> node,
                                               const std::vector<std::string>& column_names) {
  clear();

  materialize_options opts;
  opts.only_first_pass_optimizations = true;
  m_planner_node = optimization_engine::optimize_planner_graph(node, opts);

  // Do we need to materialize it for safety's sake?
  if(planner().online_materialization_recommended(m_planner_node)) {
    logstream(LOG_INFO) << "Forced materialization of XFrame due to size of lazy graph: " << std::endl;
    m_planner_node = planner().materialize_as_planner_node(m_planner_node);
  }

  m_column_names = column_names;
}

void unity_xframe::save_frame(std::string target_directory) {
  try {
    dir_archive dirarc;
    dirarc.open_directory_for_write(target_directory);
    dirarc.set_metadata("contents", "xframe");
    std::string prefix = dirarc.get_next_write_prefix();
    save_frame_by_index_file(prefix + ".frame_idx");
    dirarc.close();
  } catch(...) {
    throw;
  }
}


void unity_xframe::save_frame_reference(std::string target_directory) {
  try {
    dir_archive dirarc;
    dirarc.open_directory_for_write(target_directory);
    dirarc.set_metadata("contents", "xframe");
    std::string prefix = dirarc.get_next_write_prefix();
    xframe_save_weak_reference(*get_underlying_xframe(), prefix + ".frame_idx");
    dirarc.close();
  } catch(...) {
    throw;
  }
}

void unity_xframe::save_frame_by_index_file(std::string index_file) {
  log_func_entry();
  auto sf = get_underlying_xframe();
  sf->save(index_file);
}

void unity_xframe::save(oarchive& oarc) const {
  oarc << true;
  std::string prefix = oarc.get_prefix();
  const_cast<unity_xframe*>(this)->save_frame_by_index_file(prefix + ".frame_idx");
}

void unity_xframe::load(iarchive& iarc) {
  clear();
  bool has_xframe;
  iarc >> has_xframe;
  if (has_xframe) {
    xframe sf;
    iarc >> sf;
    construct_from_xframe(sf);
  }
}

void unity_xframe::clear() {
  m_planner_node.reset();
  m_column_names.clear();
  m_cached_xframe.reset();
}

size_t unity_xframe::size() {
  size_t ret = infer_planner_node_length(get_planner_node());
  if (ret == (size_t)(-1)) {
    return get_underlying_xframe()->size();
  }
  return ret;
}

size_t unity_xframe::num_columns() {
  return m_column_names.size();
}

size_t unity_xframe::column_index(const std::string &name) {
  Dlog_func_entry();

  auto it = std::find(m_column_names.begin(), m_column_names.end(), name);
  if(it == m_column_names.end()) {
    log_and_throw(std::string("Column '") + name + "' not found.");;
  }
  return std::distance(m_column_names.begin(), it);
}

const std::string& unity_xframe::column_name(size_t index) {
  Dlog_func_entry();

  return m_column_names.at(index);
}


bool unity_xframe::contains_column(const std::string& name) {
  Dlog_func_entry();

  const auto& sf = this->get_underlying_xframe();
  return sf->contains_column(name);
}

std::shared_ptr<unity_sarray_base> unity_xframe::select_column(const std::string& name) {
  Dlog_func_entry();

  // Error checking
  logstream(LOG_DEBUG) << "Select Column " << name << std::endl;
  auto _column_names = this->column_names();
  auto _column_index_iter = std::find(_column_names.begin(), _column_names.end(), name);
  if (_column_index_iter == _column_names.end()) {
    log_and_throw (std::string("Column name " + name + " does not exist."));
  }

  // Construct the project operator with the column index
  size_t column_index = _column_index_iter - _column_names.begin();
  auto ret = select_column(column_index);

  DASSERT_EQ(std::static_pointer_cast<unity_sarray>(ret)->dtype(), dtype(name));

  return ret;
}

std::shared_ptr<unity_sarray_base> unity_xframe::select_column(size_t column_index) {
  Dlog_func_entry();

  auto new_planner_node = op_project::make_planner_node(this->get_planner_node(), {column_index});

  std::shared_ptr<unity_sarray> ret(new unity_sarray());
  ret->construct_from_planner_node(new_planner_node);

  return ret;
}

std::shared_ptr<unity_xframe_base> unity_xframe::select_columns(
    const std::vector<std::string>& names) {
  Dlog_func_entry();

  auto ret = select_columns(_convert_column_names_to_indices(names));

#ifndef NDEBUG
  {
    std::shared_ptr<unity_xframe> X = std::static_pointer_cast<unity_xframe>(ret);

    DASSERT_EQ(X->num_columns(), names.size());

    for(size_t i = 0; i < names.size(); ++i) {
      DASSERT_EQ(names[i], X->column_name(i));
      DASSERT_EQ(this->dtype(names[i]), X->dtype(i));
    }
  }
#endif

  return ret;
}

std::shared_ptr<unity_xframe_base> unity_xframe::select_columns(
    const std::vector<size_t>& indices) {
  Dlog_func_entry();

  if(indices.empty()) {
    return std::make_shared<unity_xframe>();
  }

  std::vector<std::string> new_column_names(indices.size());

  if(std::set<size_t>(indices.begin(), indices.end()).size() != indices.size()) {
    log_and_throw("Duplicate columns selected.");
  }

  for(size_t i = 0; i < indices.size(); ++i) {
    size_t col_idx = indices[i];
    if(col_idx >= m_column_names.size()) {
      std_log_and_throw(std::range_error, "Column index out of bounds.");
    }
    new_column_names[i] = m_column_names[col_idx];
  }

  // Construct the project operator with the column index
  auto new_planner_node = op_project::make_planner_node(this->get_planner_node(), {indices});

  std::shared_ptr<unity_xframe> ret(new unity_xframe());
  ret->construct_from_planner_node(new_planner_node,
                                   new_column_names);
  return ret;
}

std::shared_ptr<unity_xframe_base> unity_xframe::copy(){
  auto ret = std::make_shared<unity_xframe>();
  auto new_planner_node = std::make_shared<planner_node>(*(this->get_planner_node()));
  ret->construct_from_planner_node(new_planner_node, this->column_names());
  return ret;
}

void unity_xframe::add_column(std::shared_ptr<unity_sarray_base> data,
                              const std::string& column_name) {
  Dlog_func_entry();

  // Sanity check
  ASSERT_TRUE(data != nullptr);

  // Auto generates column name for empty name input.
  std::string new_column_name = column_name;
  if (new_column_name.empty()) {
    new_column_name = generate_next_column_name();
  }

  auto colnames = this->column_names();
  if (std::find(colnames.begin(), colnames.end(), column_name) != colnames.end()) {
    log_and_throw("Column " + column_name + " already exists.");
  }

  // Base case:
  // If current xframe is empty, we construct a sarray source node
  std::shared_ptr<unity_sarray> new_column = std::static_pointer_cast<unity_sarray>(data);
  if (num_columns() == 0) {
    this->construct_from_planner_node(
      new_column->get_planner_node(),
      {new_column_name});
    return;
  }

  // Regular case:
  // Check that new column has the same size
  if (this->size() != new_column->size()) {
    log_and_throw(std::string("Column \"") + column_name +
                  "\" has different size than current columns!");
  }

  // Make a union operator node
  auto new_planner_node = op_union::make_planner_node(this->get_planner_node(),
                                                      new_column->get_planner_node());
  auto new_column_names = this->column_names();
  new_column_names.push_back(new_column_name);
  this->construct_from_planner_node(
      new_planner_node,
      new_column_names);
}

void unity_xframe::add_columns(
    std::list<std::shared_ptr<unity_sarray_base>> data_list,
    std::vector<std::string> name_vec) {
  Dlog_func_entry();
  std::vector<std::shared_ptr<unity_xframe_base>> ret_vec;
  std::vector<std::shared_ptr<unity_sarray_base>> data_vec(data_list.begin(), data_list.end());

  const std::string empty_str = std::string("");
  name_vec.resize(data_list.size(), empty_str);

  // Back up the planner node and column names
  auto backup_planner_node = std::make_shared<planner_node>(*(this->get_planner_node()));
  auto backup_column_names = this->column_names();

  for(size_t i = 0; i < data_vec.size(); ++i) {
    try {
      this->add_column(data_vec[i], name_vec[i]);
    } catch(...) {
      // rollback
      this->construct_from_planner_node(backup_planner_node,
                                        backup_column_names);
      throw;
    }
  }
  m_cached_xframe.reset();
}

void unity_xframe::set_column_name(size_t i, std::string name) {
  Dlog_func_entry();
  logstream(LOG_DEBUG) << "Args: " << i << "," << name << std::endl;
  if (i >= num_columns()) {
    log_and_throw("Column index out of bound.");
  }
  std::vector<std::string> colnames = column_names();
  for (size_t j = 0; j < num_columns(); ++j) {
    if (j != i && colnames[j] == name) {
      log_and_throw(std::string("Column name " + name + " already exists"));
    }
  }
  m_column_names[i] = name;
  m_cached_xframe.reset();
}

void unity_xframe::remove_column(size_t i) {
  Dlog_func_entry();
  logstream(LOG_INFO) << "Args: " << i << std::endl;
  if(i >= num_columns()) {
    log_and_throw("Column index out of bound.");
  }

  std::vector<size_t> project_column_indices;
  for (size_t j = 0; j < num_columns(); ++j) {
    if (j == i) continue;
    project_column_indices.push_back(j);
  }

  if (project_column_indices.empty()) {
    // make empty xframe
    auto sf = std::make_shared<xframe>();
    sf->open_for_write({}, {}, "", 1);
    sf->close();
    this->set_xframe(sf);
  } else {
    auto new_planner_node = op_project::make_planner_node(
        this->get_planner_node(), project_column_indices);
    auto new_column_names = this->column_names();
    new_column_names.erase(new_column_names.begin() + i);
    auto new_column_types = this->dtype();
    new_column_types.erase(new_column_types.begin() + i);
    this->construct_from_planner_node(
        new_planner_node,
        new_column_names);
  }
}

void unity_xframe::swap_columns(size_t i, size_t j) {
  Dlog_func_entry();
  logstream(LOG_DEBUG) << "Args: " << i << ", " << j << std::endl;
  if(i >= num_columns()) {
    log_and_throw("Column index value of " + std::to_string(i) + " is out of bound.");
  }
  if(j >= num_columns()) {
    log_and_throw("Column index value of " + std::to_string(j) + " is out of bound.");
  }

  std::vector<std::string> new_column_names = column_names();
  std::vector<size_t> new_column_indices(num_columns());
  for (size_t idx = 0; idx < num_columns(); ++idx) {
    new_column_indices[idx] = idx;

  }
  std::swap(new_column_indices[i], new_column_indices[j]);
  std::swap(new_column_names[i], new_column_names[j]);

  auto new_planner_node = op_project::make_planner_node(this->get_planner_node(),
                                                        new_column_indices);
  this->construct_from_planner_node(new_planner_node, new_column_names);
}

std::shared_ptr<xframe> unity_xframe::get_underlying_xframe() {
  Dlog_func_entry();

  if (!m_cached_xframe) {
    if (!is_materialized()) {
      materialize();
    }
    m_cached_xframe = std::make_shared<xframe>(
        planner().materialize(this->get_planner_node()));

    // make sure the physical xframe has consistant column names
    for (size_t i = 0; i < num_columns(); ++i) {
      m_cached_xframe->set_column_name(i, m_column_names[i]);
    }
  }

  return m_cached_xframe;
}

void unity_xframe::set_xframe(const std::shared_ptr<xframe>& sf_ptr) {
  Dlog_func_entry();
  m_planner_node = op_xframe_source::make_planner_node(*sf_ptr);
  m_column_names = sf_ptr->column_names();
  m_cached_xframe = sf_ptr;
}


std::shared_ptr<unity_sarray_base> unity_xframe::transform(const std::string& lambda,
                                           flex_type_enum type,
                                           bool skip_undefined, // unused
                                           uint64_t random_seed) {
  log_func_entry();
#ifdef TC_HAS_PYTHON
  auto new_planner_node = op_lambda_transform::make_planner_node(
      this->get_planner_node(), lambda, type,
      this->column_names(),
      skip_undefined, random_seed);

  std::shared_ptr<unity_sarray> ret(new unity_sarray());
  ret->construct_from_planner_node(new_planner_node);
  return ret;
#else
  log_and_throw("Python functions not supported");
#endif
}

std::shared_ptr<unity_sarray_base> unity_xframe::transform_native(const function_closure_info& toolkit_fn_name,
                                           flex_type_enum type,
                                           bool skip_undefined, // unused
                                           uint64_t seed) {
  log_func_entry();

  //  find the function
  auto native_execute_function =
      get_unity_global_singleton()
      ->get_toolkit_function_registry()
      ->get_native_function(toolkit_fn_name);
  std::vector<std::string> colnames = column_names();

  auto lambda =
      [native_execute_function, colnames](
          const xframe_rows::row& row)->flexible_type {
        std::vector<std::pair<flexible_type, flexible_type> > input(colnames.size());
        ASSERT_EQ(row.size(), colnames.size());
        for (size_t i = 0;i < colnames.size(); ++i) {
          input[i] = {colnames[i], row[i]};
        }
        variant_type var = to_variant(input);
        return variant_get_value<flexible_type>(native_execute_function({var}));
      };
  return this->transform_lambda(lambda, type, seed);
}

std::shared_ptr<unity_sarray_base> unity_xframe::transform_lambda(
      std::function<flexible_type(const xframe_rows::row&)> lambda,
      flex_type_enum type,
      uint64_t random_seed) {
  log_func_entry();

  auto new_planner_node = op_transform::make_planner_node(this->get_planner_node(),
                                                          lambda,
                                                          type,
                                                          random_seed);
  std::shared_ptr<unity_sarray> ret(new unity_sarray());
  ret->construct_from_planner_node(new_planner_node);
  return ret;
}

std::shared_ptr<unity_xframe_base> unity_xframe::flat_map(
    const std::string& lambda,
    std::vector<std::string> column_names,
    std::vector<flex_type_enum> column_types,
    bool skip_undefined,
    uint64_t seed) {
#ifdef TC_HAS_PYTHON
  log_func_entry();
  DASSERT_EQ(column_names.size(), column_types.size());
  DASSERT_TRUE(!column_names.empty());
  DASSERT_TRUE(!column_types.empty());

  xframe out_sf;
  out_sf.open_for_write(column_names, column_types, "", XFRAME_DEFAULT_NUM_SEGMENTS);

  lambda::pylambda_function pylambda_fn(lambda);
  pylambda_fn.set_skip_undefined(skip_undefined);
  pylambda_fn.set_random_seed(seed);
  auto this_column_names = this->column_names();

  auto transform_callback = [&](size_t segment_id, const std::shared_ptr<xframe_rows>& data) {
    auto output_iter = out_sf.get_output_iterator(segment_id);
    std::vector<flexible_type> lambda_output_rows;
    pylambda_fn.eval(this_column_names, *data, lambda_output_rows);
    for (flexible_type& result: lambda_output_rows) {
      if (result.get_type() == flex_type_enum::UNDEFINED) {
        continue;
      } else if (result.get_type() == flex_type_enum::LIST) {
        flex_list& out_rows = result.mutable_get<flex_list>();
        for (auto& out_row: out_rows) {
          *output_iter++ = std::move(out_row);
        }
      } else if (result.get_type() == flex_type_enum::VECTOR) {
        if (result.get<flex_vec>().size() > 0) {
          std::string message = "Cannot convert " + std::string(result) +
            " to " + flex_type_enum_to_name(flex_type_enum::LIST);
          logstream(LOG_ERROR) <<  message << std::endl;
          throw(bad_cast(message));

        }
      } else {
        std::string message = "Cannot convert " + std::string(result) +
          " to " + flex_type_enum_to_name(flex_type_enum::LIST);
        logstream(LOG_ERROR) <<  message << std::endl;
        throw(bad_cast(message));
      }
    }
    return false;
  };
  query_eval::planner().materialize(this->get_planner_node(), transform_callback, XFRAME_DEFAULT_NUM_SEGMENTS);
  out_sf.close();
  auto ret = std::make_shared<unity_xframe>();
  ret->construct_from_xframe(out_sf);
  return ret;
#else
  log_and_throw("Python lambda functions not supported");
#endif
}


std::vector<flex_type_enum> unity_xframe::dtype() {
  Dlog_func_entry();
  return infer_planner_node_type(this->get_planner_node());
}

flex_type_enum unity_xframe::dtype(size_t column_index) {
  Dlog_func_entry();

  return std::static_pointer_cast<unity_sarray>(select_column(column_index))->dtype();
}


flex_type_enum unity_xframe::dtype(const std::string& column_name) {
  Dlog_func_entry();

  return dtype(column_index(column_name));

}


std::vector<std::string> unity_xframe::column_names() {
  Dlog_func_entry();
  return m_column_names;
}



std::shared_ptr<unity_xframe_base> unity_xframe::head(size_t nrows) {
  log_func_entry();

  // prepare for writing to the new xframe
  xframe sf_head;
  sf_head.open_for_write(column_names(), dtype(), "", 1);
  auto out = sf_head.get_output_iterator(0);

  size_t row_counter = 0;
  if (nrows > 0)  {
    auto callback = [&out, &row_counter, nrows](size_t segment_id,
                                                const std::shared_ptr<xframe_rows>& data) {
      for (const auto& row : (*data)) {
        *out = row;
        ++out;
        ++row_counter;
        if (row_counter == nrows) return true;
      }
      return false;
    };

    query_eval::planner().materialize(this->get_planner_node(),
                                      callback,
                                      1 /* process in as 1 segment */);
  }
  sf_head.close();
  std::shared_ptr<unity_xframe> ret(new unity_xframe());
  ret->construct_from_xframe(sf_head);
  return ret;
}


dataframe_t unity_xframe::_head(size_t nrows) {
  auto result = head(nrows);
  dataframe_t ret = result->to_dataframe();
  return ret;
};

dataframe_t unity_xframe::_tail(size_t nrows) {
  auto result = tail(nrows);
  dataframe_t ret = result->to_dataframe();
  return ret;
};

std::shared_ptr<unity_xframe_base> unity_xframe::tail(size_t nrows) {
  log_func_entry();
  logstream(LOG_INFO) << "Args: " << nrows << std::endl;
  size_t end = size();
  nrows = std::min<size_t>(nrows, end);
  size_t start = end - nrows;
  return copy_range(start, 1, end);
}

std::list<std::shared_ptr<unity_xframe_base>> unity_xframe::logical_filter_split(
  std::shared_ptr<unity_sarray_base> logical_filter_array) {
  return {logical_filter(logical_filter_array),
          logical_filter(logical_filter_array->right_scalar_operator(1, "-"))};
}

std::shared_ptr<unity_xframe_base> unity_xframe::logical_filter(
    std::shared_ptr<unity_sarray_base> index) {
  log_func_entry();

  ASSERT_TRUE(index != nullptr);

  std::shared_ptr<unity_sarray> filter_array = std::static_pointer_cast<unity_sarray>(index);

  std::shared_ptr<unity_sarray> other_array_binarized =
      std::static_pointer_cast<unity_sarray>(
      filter_array->transform_lambda(
            [](const flexible_type& f)->flexible_type {
              return (flex_int)(!f.is_zero());
            }, flex_type_enum::INTEGER, true, 0));


  auto equal_length = query_eval::planner().test_equal_length(this->get_planner_node(),
                                                              other_array_binarized->get_planner_node());

  if (!equal_length) {
    log_and_throw("Logical filter array must have the same size");
  }


  auto new_planner_node = op_logical_filter::make_planner_node(this->get_planner_node(),
                                                               other_array_binarized->get_planner_node());

  std::shared_ptr<unity_xframe> ret_unity_xframe(new unity_xframe());
  ret_unity_xframe->construct_from_planner_node(new_planner_node,
                                                this->column_names());
  return ret_unity_xframe;
}

std::shared_ptr<unity_xframe_base> unity_xframe::append(
    std::shared_ptr<unity_xframe_base> other) {
  log_func_entry();

  DASSERT_TRUE(other != nullptr);
  std::shared_ptr<unity_xframe> other_xframe = std::static_pointer_cast<unity_xframe>(other);

  // zero columns
  if (this->num_columns() == 0) {
    return other;
  } else if (other_xframe->num_columns() == 0) {
    return copy();
  }

  // Error checking and reorder other xframe if necessary
  {
    bool needs_reorder = false;

    if (this->num_columns() != other_xframe->num_columns()) {
      log_and_throw("Two XFrames have different number of columns");
    }
    std::vector<std::string> column_names = this->column_names();
    std::vector<std::string> other_column_names = other_xframe->column_names();

    size_t num_columns = column_names.size();

    if(column_names != other_column_names) {
      needs_reorder = true;

      std::sort(column_names.begin(), column_names.end());
      std::sort(other_column_names.begin(), other_column_names.end());

      if(column_names != other_column_names) {

      std::vector<std::string> in_this;

      std::set_difference(column_names.begin(), column_names.end(),
                          other_column_names.begin(), other_column_names.end(),
                          std::inserter(in_this, in_this.begin()));

        std::ostringstream ss;
        ss << "Error: Columns [" << in_this
           << "] not found in appending XFrame.";

        log_and_throw(ss.str().c_str());
      }

      if (needs_reorder) {
        other_xframe = std::static_pointer_cast<unity_xframe>(
            other_xframe->select_columns(this->column_names()));
      }
    }


    auto column_types = this->dtype();
    auto other_column_types = other_xframe->dtype();

    for(size_t i = 0; i < num_columns; i++) {

      // check column type matches
      if (column_types[i] != other_column_types[i]) {
        std::ostringstream ss;
        ss << "Column types are not the same in two XFrames (Column "
           << column_names[i] << ", attempting to append column of type "
           << flex_type_enum_to_name(other_column_types[i])
           << " to column of type " << flex_type_enum_to_name(column_types[i])
           << ").";

        log_and_throw(ss.str().c_str());
      }
    }
  }

  auto new_planner_node = op_append::make_planner_node(this->get_planner_node(),
                                                       other_xframe->get_planner_node());
  std::shared_ptr<unity_xframe> ret_unity_xframe(new unity_xframe());
  ret_unity_xframe->construct_from_planner_node(new_planner_node,
                                                this->column_names());
  return ret_unity_xframe;
}

void unity_xframe::begin_iterator() {
  log_func_entry();

  // Empty xframe just return
  if (this->size() == 0)
    return;

  auto xframe_ptr = get_underlying_xframe();
  iterator_xframe_ptr = xframe_ptr->get_reader();
  // init the iterators
  iterator_current_segment_iter.reset(new xframe_iterator(iterator_xframe_ptr->begin(0)));
  iterator_current_segment_enditer.reset(new xframe_iterator(iterator_xframe_ptr->end(0)));
  iterator_next_segment_id = 1;
}

std::vector< std::vector<flexible_type> > unity_xframe::iterator_get_next(size_t len) {
  std::vector< std::vector<flexible_type> > ret;

  // Empty xframe just return
  if (this->size() == 0)
    return ret;

  // try to extract len elements
  ret.reserve(len);
  // loop across segments
  while(1) {
    // loop through current segment
    while(*iterator_current_segment_iter != *iterator_current_segment_enditer) {
      ret.push_back(**iterator_current_segment_iter);
      ++(*iterator_current_segment_iter);
      if (ret.size() >= len) break;
    }
    if (ret.size() >= len) break;
    // if we run out of data in the current segment, advance to the next segment
    // if we run out of segments, quit.
    if (iterator_next_segment_id >= iterator_xframe_ptr->num_segments()) break;
    iterator_current_segment_iter.reset(new xframe_iterator(
        iterator_xframe_ptr->begin(iterator_next_segment_id)));
    iterator_current_segment_enditer.reset(new xframe_iterator(
        iterator_xframe_ptr->end(iterator_next_segment_id)));
    ++iterator_next_segment_id;
  }
  return ret;
}

void unity_xframe::save_as_csv(const std::string& url,
                               std::map<std::string, flexible_type> writing_config) {
  log_func_entry();
  logstream(LOG_INFO) << "Args: " << sanitize_url(url) << std::endl;

  csv_writer writer;
  // first the defaults
  writer.delimiter = ",";
  writer.escape_char = '\\';
  writer.use_escape_char = true;
  writer.double_quote = true;
  writer.quote_char = '\"';
  writer.quote_level = csv_writer::csv_quote_level::QUOTE_NONNUMERIC;
  writer.header = true;
  writer.na_value = "";
  std::string file_header;
  std::string file_footer;
  std::string line_prefix;
  bool no_prefix_on_first_value = false;


  if (writing_config["delimiter"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string) writing_config["delimiter"];
    if(tmp.length() > 0) writer.delimiter = tmp;
  }
  if (writing_config["escape_char"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)writing_config["escape_char"];
    if (tmp.length() > 0) writer.escape_char = tmp[0];
    else writer.use_escape_char = false;
  }
  if (writing_config.count("double_quote")) {
    writer.double_quote = !writing_config["double_quote"].is_zero();
  }
  if (writing_config["quote_char"].get_type() == flex_type_enum::STRING) {
    std::string tmp = (flex_string)writing_config["quote_char"];
    if (tmp.length() > 0) writer.quote_char = tmp[0];
  }
  if (writing_config.count("quote_level")) {
    auto quote_level = writing_config["quote_level"];
    if (quote_level == 0) {
      writer.quote_level = csv_writer::csv_quote_level::QUOTE_MINIMAL;
    } else if (quote_level == 1) {
      writer.quote_level = csv_writer::csv_quote_level::QUOTE_ALL;
    } else if (quote_level == 2) {
      writer.quote_level = csv_writer::csv_quote_level::QUOTE_NONNUMERIC;
    } else if (quote_level == 3) {
      writer.quote_level = csv_writer::csv_quote_level::QUOTE_NONE;
    } else {
      log_and_throw("Invalid quote level");
    }
  }
  if (writing_config.count("header")) {
    writer.header= !writing_config["header"].is_zero();
  }

  if (writing_config.count("line_terminator")) {
    std::string tmp = (flex_string) writing_config["line_terminator"];
    if(tmp.length() > 0) writer.line_terminator = tmp;
  }

  if (writing_config.count("na_value")) {
    std::string tmp = (flex_string) writing_config["na_value"];
    if(tmp.length() > 0) writer.na_value = tmp;
  }

  if (writing_config.count("file_header")) {
    file_header = (flex_string) writing_config["file_header"];
  }
  if (writing_config.count("file_footer")) {
    file_footer = (flex_string) writing_config["file_footer"];
  }
  if (writing_config.count("line_prefix")) {
    line_prefix = (flex_string) writing_config["line_prefix"];
  }
  if (writing_config.count("_no_prefix_on_first_value")) {
    no_prefix_on_first_value = !writing_config["_no_prefix_on_first_value"].is_zero();
  }

  general_ofstream fout(url);
  if (!file_header.empty()) fout << file_header << writer.line_terminator;
  if (!fout.good()) {
    log_and_throw(std::string("Unable to open " + sanitize_url(url) + " for write"));
  }

  // write the header
  size_t num_cols = this->num_columns();
  if (num_cols == 0) return;

  if (writer.header) writer.write_verbatim(fout, this->column_names());

  bool first_value = true;
  auto write_callback = [&writer, &fout, &line_prefix,
       &no_prefix_on_first_value, &first_value]
      (size_t segment_id, const std::shared_ptr<xframe_rows>& data) {
    for (const auto& row : *(data)) {
      if (!line_prefix.empty()) {
        if ((!first_value) || // not the first value. write the line prefix
            (!no_prefix_on_first_value) // first value, write line prefix if
                                        // no_prefix_on_first_value == false
                                        // (yes the double negative is annoying)
            ) {
          fout.write(line_prefix.c_str(), line_prefix.size());
        }
      }
      first_value = false;
      writer.write(fout, row);
    }
    return false;
  };

  query_eval::planner().materialize(this->get_planner_node(), write_callback, 1);
  if (!fout.good()) {
    log_and_throw_io_failure("Fail to write.");
  }
  if (!file_footer.empty()) fout << file_footer << writer.line_terminator;
  fout.close();
}

std::shared_ptr<unity_xframe_base> unity_xframe::sample(float percent,
                                                        uint64_t random_seed,
                                                        bool exact) {
  logstream(LOG_INFO) << "Args: " << percent << ", " << random_seed << std::endl;
  if (percent == 1.0){
    return copy();
  }
  auto logical_filter_array = std::static_pointer_cast<unity_sarray>(
    unity_sarray::make_uniform_boolean_array(size(), percent, random_seed, exact));

  return logical_filter(logical_filter_array);
}

void unity_xframe::materialize() {
  query_eval::planner().materialize(m_planner_node);
}


bool unity_xframe::is_materialized() {
  auto optimized_node = optimization_engine::optimize_planner_graph(get_planner_node(),
                                                                    materialize_options());
  if (is_source_node(optimized_node)) {
    m_planner_node = optimized_node;
    return true;
  }
  return false;
}

bool unity_xframe::has_size() {
  return infer_planner_node_length(m_planner_node) != -1;
}

std::string unity_xframe::query_plan_string() {
  std::stringstream ss;
  ss << get_planner_node() << std::endl;
  return ss.str();
}

std::list<std::shared_ptr<unity_xframe_base>>
unity_xframe::random_split(float percent, uint64_t random_seed, bool exact) {
  log_func_entry();
  logstream(LOG_INFO) << "Args: " << percent << ", " << random_seed << std::endl;

  auto logical_filter_array = std::static_pointer_cast<unity_sarray>(
    unity_sarray::make_uniform_boolean_array(size(), percent, random_seed, exact));
  return logical_filter_split(logical_filter_array);
}

std::shared_ptr<unity_xframe_base> unity_xframe::shuffle() {
  log_func_entry();

  std::vector<std::string> column_names = this->column_names();
  const size_t num_buckets = (this->size() / XFRAME_SHUFFLE_BUCKET_SIZE) + 1;

  // Create a column of random ints between 0 and (num_buckets - 1).
  auto temp_groupby_column = std::static_pointer_cast<unity_sarray>(
    unity_sarray::make_uniform_int_array(this->size(), num_buckets));
  const std::string rand_int_column_name("Random Ints");
  std::shared_ptr<unity_xframe> temp(new unity_xframe());
  temp->add_column(temp_groupby_column, rand_int_column_name);

  // Pack columns so we can group by concatenate
  std::shared_ptr<unity_sarray_base> packed_columns = this->pack_columns(column_names,
                                                                         column_names,
                                                                         flex_type_enum::LIST,
                                                                         turi::flex_undefined());
  const std::string packed_data_column_name("Packed Data");
  temp->add_column(packed_columns, packed_data_column_name);

  // Group by concatenate on the random int column. This randomly bucketizes.
  const std::string buckets_column_name("Buckets");
  std::shared_ptr<unity_xframe_base> bucketized_xframe = temp->groupby_aggregate({rand_int_column_name},
                                                                                 {{packed_data_column_name}},
                                                                                 {buckets_column_name},
                                                                                 {"__builtin__concat__list__"});
  std::shared_ptr<unity_sarray_base> bucketized_sarray = bucketized_xframe->select_column(buckets_column_name);

  // Shuffle each bucket
  size_t num_threads = thread::cpu_count();
  gl_sarray_writer writer(flex_type_enum::LIST, num_threads);
  gl_sarray gl_bucketized_sarray = gl_sarray(bucketized_sarray);

  in_parallel([&](size_t thread_id, size_t n_threads) {
      size_t idx_start = (gl_bucketized_sarray.size() * thread_id) / n_threads;
      size_t idx_end = (gl_bucketized_sarray.size() * (thread_id + 1) ) / n_threads;

      gl_sarray_range ra = gl_bucketized_sarray.range_iterator(idx_start, idx_end);
      auto cur_bucket = ra.begin();

      unsigned int seed = static_cast<unsigned int>(random::pure_random_seed());
      auto rand_engine = std::default_random_engine(seed);
      while (cur_bucket != ra.end()) {
        // shuffle the indexes for the current bucket
        auto indexes = std::vector<int>(cur_bucket->size());
        std::iota(indexes.begin(), indexes.end(), 0);
        std::shuffle(indexes.begin(), indexes.end(), rand_engine);

        // output in random order
        for (size_t i = 0; i < cur_bucket->size(); i++) {
          writer.write(cur_bucket->array_at(indexes[i]), thread_id);
        }
        ++cur_bucket;
      }
  });
  gl_sarray packed_randomized = writer.close();

  std::string unpacked_column_prefix = "X";
  gl_xframe ret = packed_randomized.unpack(unpacked_column_prefix, this->dtype());
  DASSERT_EQ(this->num_columns(), ret.num_columns());
  DASSERT_EQ(this->size(), ret.size());

  // Rename columns back to original names
  std::map<std::string, std::string> columns_rename_map;
  for (size_t i = 0; i < column_names.size(); i++) {
    std::string to_name = unpacked_column_prefix + "." + std::to_string(i);
    columns_rename_map[to_name] = column_names[i];
  }
  ret.rename(columns_rename_map);

  return ret.get_proxy();
}

std::shared_ptr<unity_xframe_base> unity_xframe::groupby_aggregate(
    const std::vector<std::string>& key_columns,
    const std::vector<std::vector<std::string>>& group_columns,
    const std::vector<std::string>& group_output_columns,
    const std::vector<std::string>& group_operations) {

  std::vector<std::shared_ptr<group_aggregate_value>> operators;
  for (const auto& op: group_operations) operators.push_back(get_builtin_group_aggregator(op));
  return groupby_aggregate(key_columns, group_columns, group_output_columns, operators);
}

std::shared_ptr<unity_xframe_base> unity_xframe::groupby_aggregate(
    const std::vector<std::string>& key_columns,
    const std::vector<std::vector<std::string>>& group_columns,
    const std::vector<std::string>& group_output_columns,
    const std::vector<std::shared_ptr<group_aggregate_value>>& group_operations) {
  log_func_entry();

  // logging stuff
  {
    logstream(LOG_INFO) << "Args: Keys: ";
    for (auto i: key_columns) logstream(LOG_INFO) << i << ",";
    logstream(LOG_INFO) << "\tGroups: ";
    for (auto cols: group_columns) {
      for(auto col: cols) {
        logstream(LOG_INFO) << col << ",";
      }
      logstream(LOG_INFO) << " | ";
    }
    logstream(LOG_INFO) << "\tOperations: ";
    for (auto i: group_operations) logstream(LOG_INFO) << i << ",";
    logstream(LOG_INFO) << std::endl;
  }

  // Prepare the operators
  ASSERT_EQ(group_columns.size(), group_operations.size());
  std::vector<std::pair<std::vector<std::string>,
      std::shared_ptr<group_aggregate_value> > > operators;
  for (size_t i = 0;i < group_columns.size(); ++i) {
    // avoid copying empty column string
    // this is the case for aggregate::COUNT()
    std::vector<std::string> column_names;
    for (const auto& col : group_columns[i]) {
      if (!col.empty()) column_names.push_back(col);
    }
    operators.push_back( {column_names, group_operations[i]} );
  }

  auto grouped_sf = query_eval::groupby_aggregate(get_planner_node(),
                                                  column_names(),
                                                  key_columns,
                                                  group_output_columns,
                                                  operators);

  std::shared_ptr<unity_xframe> ret(new unity_xframe());
  ret->construct_from_xframe(*grouped_sf);
  return ret;
}


std::shared_ptr<unity_xframe_base> unity_xframe::join_with_custom_name(
    std::shared_ptr<unity_xframe_base> right,
    const std::string join_type,
    const std::map<std::string,std::string>& join_keys,
    const std::map<std::string,std::string>& alternative_names) {
  log_func_entry();
  std::shared_ptr<unity_xframe> ret(new unity_xframe());
  std::shared_ptr<unity_xframe> us_right = std::static_pointer_cast<unity_xframe>(right);

  auto xframe_ptr = get_underlying_xframe();
  auto right_xframe_ptr = us_right->get_underlying_xframe();
  xframe joined_sf = turi::join(*xframe_ptr, *right_xframe_ptr, join_type,
                                join_keys, alternative_names);
  ret->construct_from_xframe(joined_sf);
  return ret;
}

std::shared_ptr<unity_xframe_base>
unity_xframe::sort(const std::vector<std::string>& sort_keys,
                   const std::vector<int>& sort_ascending) {
  log_func_entry();

  if (sort_keys.size() != sort_ascending.size()) {
    log_and_throw("xframe::sort key vector and ascending vector size mismatch");
  }

  if (sort_keys.size() == 0) {
    log_and_throw("xframe::sort, nothing to sort");
  }

  std::vector<size_t> sort_indices;

  if(sort_keys.empty()) {
    sort_indices.resize(this->num_columns());
    std::iota(sort_indices.begin(), sort_indices.end(), 0);
  } else {
    sort_indices = _convert_column_names_to_indices(sort_keys);
  }

  std::vector<bool> b_sort_ascending;
  for(auto sort_order: sort_ascending) {
    b_sort_ascending.push_back((bool)sort_order);
  }

  auto sorted_sf = turi::ec_sort(this->get_planner_node(),
                                     this->column_names(),
                                     sort_indices,
                                     b_sort_ascending);
  std::shared_ptr<unity_xframe> ret(new unity_xframe());
  ret->construct_from_xframe(*sorted_sf);
  return ret;
}

std::shared_ptr<unity_sarray_base> unity_xframe::pack_columns(
    const std::vector<std::string>& pack_column_names,
    const std::vector<std::string>& key_names,
    flex_type_enum dtype,
    const flexible_type& fill_na) {

  log_func_entry();

  if (dtype != flex_type_enum::DICT &&
    dtype != flex_type_enum::LIST &&
    dtype != flex_type_enum::VECTOR) {
    log_and_throw("Resulting sarray dtype should be list/array/dict type");
  }

  std::set<flexible_type> pack_column_set(pack_column_names.begin(), pack_column_names.end());
  if (pack_column_set.size() != pack_column_names.size()) {
    throw "There are duplicate names in packed columns";
  }

  // select packing columns
  auto projected_sf = std::static_pointer_cast<unity_xframe>(this->select_columns(pack_column_names));

  auto dict_transform_callback = [=](const xframe_rows::row& row)->flexible_type{
    flex_dict out_val;
    out_val.reserve(row.size());
    for (size_t col = 0; col < row.size(); col++) {
      if (row[col] != FLEX_UNDEFINED) {
        out_val.push_back(std::make_pair(key_names[col], row[col]));
      } else {
        if (fill_na.get_type() != flex_type_enum::UNDEFINED) {
          out_val.push_back(std::make_pair(key_names[col], fill_na));
        }
      }
    }
    return out_val;
  };

  auto list_transform_callback = [=](const xframe_rows::row& row)->flexible_type {
    flex_list out_val(row.size());
    for (size_t col = 0; col < row.size(); col++) {
      if (row[col] != FLEX_UNDEFINED) {
        out_val[col] = row[col];
      } else {
        out_val[col] = fill_na;
      }
    }
    return out_val;
  };

  auto vector_transform_callback = [=](const xframe_rows::row& row)->flexible_type {
    flex_vec out_val(row.size());
    for (size_t col = 0; col < row.size(); col++) {
      if (!row[col].is_na()) {
        out_val[col] = row[col];
      } else {
        if (fill_na == FLEX_UNDEFINED) {
          out_val[col] = NAN;
        } else {
          out_val[col] = (double)fill_na;
        }
      }
    }
    return out_val;
  };

  std::shared_ptr<unity_sarray> ret(new unity_sarray());
  if (dtype == flex_type_enum::DICT) {
    auto new_planner_node = op_transform::make_planner_node(projected_sf->get_planner_node(),
                                                            dict_transform_callback,
                                                            dtype);
    ret->construct_from_planner_node(new_planner_node);
  } else if (dtype == flex_type_enum::LIST) {
    auto new_planner_node = op_transform::make_planner_node(projected_sf->get_planner_node(),
                                                            list_transform_callback,
                                                            dtype);
    ret->construct_from_planner_node(new_planner_node);
  } else {
    auto new_planner_node = op_transform::make_planner_node(projected_sf->get_planner_node(),
                                                            vector_transform_callback,
                                                            dtype);
    ret->construct_from_planner_node(new_planner_node);
  }
  return ret;
}

std::shared_ptr<unity_xframe_base> unity_xframe::stack(
    const std::string& stack_column_name,
    const std::vector<std::string>& new_column_names,
    const std::vector<flex_type_enum>& new_column_types,
    bool drop_na) {

  log_func_entry();

  // check validity of column names
  auto all_column_names = this->column_names();
  auto all_column_types = this->dtype();
  std::set<std::string> my_columns(all_column_names.begin(), all_column_names.end());
  bool stack_column_exists = false;
  for(auto name : new_column_names) {
    if (my_columns.count(name) && name != stack_column_name) {
      throw "Column name '" + name + "' is already used by current XFrame, pick a new column name";
    }
    if (my_columns.count(stack_column_name) > 0) {
      stack_column_exists = true;
    }
  }
  if (!stack_column_exists) {
    log_and_throw("Cannot find stack column " + stack_column_name);
  }

  // validate column types
  size_t new_column_count = 0;
  flex_type_enum stack_column_type = this->select_column(stack_column_name)->dtype();
  if (stack_column_type == flex_type_enum::DICT) {
    new_column_count = 2;
  } else if (stack_column_type == flex_type_enum::VECTOR) {
    new_column_count = 1;
  } else if (stack_column_type == flex_type_enum::LIST) {
    new_column_count = 1;
  } else {
    throw "Column type is not supported for stack";
  }

  if (new_column_types.size() != new_column_count) {
    throw "column types given is not matching the expected number";
  }
  if (new_column_names.size() != new_column_count) {
    throw "column names given is not matching the expected number";
  }
  // check uniqueness of output column name if given
  if (new_column_names.size() == 2 &&
     new_column_names[0] == new_column_names[1] &&
     !new_column_names[0].empty()) {
      throw "There is duplicate column names in new_column_names parameter";
  }

  // create return XFrame
  size_t num_columns = this->num_columns();
  std::vector<std::string> ret_column_names;
  std::vector<flex_type_enum> ret_column_types;
  ret_column_names.reserve(num_columns + new_column_count - 1);
  ret_column_types.reserve(num_columns + new_column_count - 1);

  for(size_t i = 0; i < num_columns; ++i) {
    if (all_column_names[i] != stack_column_name) {
      ret_column_names.push_back(all_column_names[i]);
      ret_column_types.push_back(all_column_types[i]);
    }
  }

  ret_column_names.insert(ret_column_names.end(),new_column_names.begin(), new_column_names.end());
  ret_column_types.insert(ret_column_types.end(),new_column_types.begin(), new_column_types.end());

  auto xframe_ptr = std::make_shared<xframe>();
  xframe_ptr->open_for_write(ret_column_names, ret_column_types,
                             "", XFRAME_DEFAULT_NUM_SEGMENTS);
  size_t stack_col_idx = column_index(stack_column_name);

  auto transform_callback = [&](size_t segment_id,
                                const std::shared_ptr<xframe_rows>& data) {

    auto output_iter = xframe_ptr->get_output_iterator(segment_id);
    std::vector<flexible_type> out_row_buffer(num_columns + new_column_count - 1);

    for (const auto& row: (*data)) {
      const flexible_type& val = row[stack_col_idx];
      if (val.get_type() == flex_type_enum::UNDEFINED || val.size() == 0) {
        if (!drop_na) {
          if (stack_column_type == flex_type_enum::DICT) {
            out_row_buffer[num_columns - 1] = FLEX_UNDEFINED;
            out_row_buffer[num_columns] = FLEX_UNDEFINED;
          } else {
            out_row_buffer[num_columns - 1] = FLEX_UNDEFINED;
          }
          // copy the rest columns
          for(size_t i = 0, j = 0; i < num_columns; i++) {
            if (i != stack_col_idx) {
              out_row_buffer[j++] = row[i];
            }
          }
          // write to out xframe
          *output_iter++ = out_row_buffer;
        }
      } else {
        for(size_t row_idx = 0; row_idx < val.size(); row_idx++) {
          if (stack_column_type == flex_type_enum::DICT) {
            const flex_dict& dict_val = val.get<flex_dict>();
            out_row_buffer[num_columns - 1] = dict_val[row_idx].first;
            out_row_buffer[num_columns] = dict_val[row_idx].second;
          } else if (stack_column_type == flex_type_enum::LIST) {
            out_row_buffer[num_columns - 1] = val.array_at(row_idx);
          } else {
            out_row_buffer[num_columns - 1] = val[row_idx];
          }
          // copy the rest columns
          for(size_t i = 0, j = 0; i < num_columns; i++) {
            if (i != stack_col_idx) {
              out_row_buffer[j++] = row[i];
            }
          }
          // write to out xframe
          *output_iter++ = out_row_buffer;
        }
      }
    }
    return false;
  };

  // turi::multi_transform(m_lazy_xframe, *xframe_ptr, transform_fn);
  query_eval::planner().materialize(this->get_planner_node(),
                                    transform_callback, XFRAME_DEFAULT_NUM_SEGMENTS);
  xframe_ptr->close();

  auto ret = std::make_shared<unity_xframe>();
  ret->construct_from_xframe(*xframe_ptr);
  return ret;
}

std::shared_ptr<unity_xframe_base>
unity_xframe::copy_range(size_t start, size_t step, size_t end) {
  log_func_entry();
  if (step == 0) log_and_throw("Range step size must be at least 1");
  // end cannot be past the end
  end = std::min(end, size());

  std::shared_ptr<unity_xframe> ret(new unity_xframe());

  // Fast path: range slice with step 1, we can slice the input using the query planner.
  if ((start < end) && (step == 1)) {
    auto current_node = this->get_planner_node();
    auto sliced_node = query_eval::planner().slice(current_node, start, end);
    // slice may partially materialize the node. Save it to avoid repeated materialization
    m_planner_node = current_node;
    ret->construct_from_planner_node(sliced_node, this->column_names());
    return ret;
  }

  xframe writer;
  writer.open_for_write(column_names(),
                        dtype(),
                        std::string(""), 1);
  if (start < end) {
    // If the range begins from the start, we do a lazy read.
    // Otherwise, we will materialize the xframe.
    //
    // This is quite an annoying heuristic.
    // We should also be able to do the lazy callback way
    // which carefully slices the inputs to get the right values.
    // This avoids the annoying sequential read. Ponder.
    if (is_materialized() || start > 0) {
      auto xframe_ptr = this->get_underlying_xframe();
      turi::copy_range(*xframe_ptr, writer, start, step, end);
    } else {
      size_t current_row = 0;
      auto out = writer.get_output_iterator(0);
      auto callback = [&current_row, &out, start, step, end](size_t segment_id,
                                                             const std::shared_ptr<xframe_rows>& data) {
        for (auto& row: (*data)) {
          if (current_row >= end) return true;
          if (current_row < start || (current_row - start) % step != 0) {
            ++current_row;
            continue;
          }
          *out++ = row;
          ++current_row;
        }
        return false;
      };
      query_eval::planner().materialize(this->get_planner_node(), callback, 1);
    }
  } // else we return an empty xframe.
  writer.close();
  ret->construct_from_xframe(writer);
  return ret;
}


std::list<std::shared_ptr<unity_xframe_base>> unity_xframe::drop_missing_values(
    const std::vector<std::string>& column_names, bool all, bool split, bool recursive) {
  log_func_entry();

  // Error checking
  if (column_names.size() > this->num_columns()) {
    log_and_throw("Too many column names given.");
  }

  // First see if we can do this on a single column:
  std::shared_ptr<unity_sarray> filter_sarray;

  if(column_names.size() == 1) {

    auto src_array = std::static_pointer_cast<unity_sarray>(select_column(column_names[0]));
    filter_sarray = std::static_pointer_cast<unity_sarray>(src_array->missing_mask(recursive, false));

  } else {

    std::vector<size_t> column_indices;

    if(column_names.empty()) {
      column_indices.resize(this->num_columns());
      std::iota(column_indices.begin(), column_indices.end(), 0);
    } else {
      column_indices = _convert_column_names_to_indices(column_names);
    }

    // Separate out the columns that require contains_na, which is more expensive.
    size_t n_recursive = 0, n_simple = column_indices.size();

    if(recursive) {

      // Partition the indices so that the first chunk don't need recursive types,
      // and the indices later need recursive.  This would make the filter function
      // more efficient.
      auto part_it = std::partition(column_indices.begin(), column_indices.end(),
          [&](size_t i) {
        flex_type_enum src_dtype = dtype(column_indices[i]);

        return !(src_dtype == flex_type_enum::VECTOR
                  || src_dtype == flex_type_enum::LIST
                  || src_dtype == flex_type_enum::DICT
                  || src_dtype == flex_type_enum::ND_VECTOR);
      });

      n_simple = part_it - column_indices.begin();
      n_recursive = column_indices.end() - part_it;
    }

    // Now, make a dedicated XFrame with the right columns.
    auto src_xframe = std::static_pointer_cast<unity_xframe>(select_columns(column_indices));
    std::function<flexible_type(const xframe_rows::row&)> filter_fn;

    if(n_recursive == 0) {
      if(all) {
        filter_fn = [](const xframe_rows::row& row) -> flexible_type {
          for(const flexible_type& v : row) {
            if(!v.is_na()) {
              return true;
            }
          }
          return false;
        };
      } else {
        filter_fn = [](const xframe_rows::row& row) -> flexible_type {
          for(const flexible_type& v : row) {
            if(v.is_na()) {
              return false;
            }
          }
          return true;
        };
      }
    } else {
      if(all) {
        filter_fn = [=](const xframe_rows::row& row) -> flexible_type {

          for (size_t i = 0; i < n_simple; ++i) {
            if(!row[i].is_na()) {
              return true;
            }
          }
          for(size_t i = n_simple; i < row.size(); ++i) {
            if(!row[i].contains_na()) {
              return true;
            }
          }
          return false;
        };
      } else {
        filter_fn = [=](const xframe_rows::row& row) -> flexible_type {

          for (size_t i = 0; i < n_simple; ++i) {
            if(row[i].is_na()) {
              return false;
            }
          }
          for(size_t i = n_simple; i < row.size(); ++i) {
            if(row[i].contains_na()) {
              return false;
            }
          }
          return true;
        };
      }
    }

    filter_sarray = std::static_pointer_cast<unity_sarray>(
       src_xframe->transform_lambda(filter_fn, flex_type_enum::INTEGER, 0));
  }

  if (split) {
    return logical_filter_split(filter_sarray);
  } else {
    return {logical_filter(filter_sarray), std::make_shared<unity_xframe>()};
  }
}

dataframe_t unity_xframe::to_dataframe() {
  dataframe_t ret;
  for (size_t i = 0; i < num_columns(); ++i) {
    auto name = column_names()[i];
    auto type = dtype()[i];
    ret.names.push_back(name);
    ret.types[name] = type;
    ret.values[name] = select_column(name)->to_vector();
  }
  return ret;
}

/**
 * Convert column names to column indices.
 *
 * If input column_names is empty, return 0,1,2,...num_columns-1
 *
 * Throw if column_names has duplication, or some column name does not exist.
 */
std::vector<size_t> unity_xframe::_convert_column_names_to_indices(
    const std::vector<std::string>& column_names) {

  std::set<size_t> dedup_set;
  std::vector<size_t> column_indices;

  if(column_names.size()) {

    column_indices.reserve(column_names.size());

    for(const auto& name : column_names) {
      // Fine if this throws, it will just be propagated back with a fine message
      auto iter = std::find(m_column_names.begin(), m_column_names.end(), name);

      if (iter == m_column_names.end()) {
        log_and_throw(std::string("Column ") + name + " does not exist");
      };
      size_t index_to_add = iter - m_column_names.begin();

      if (dedup_set.count(index_to_add)) {
        log_and_throw(std::string("Duplicate column name: ") + name);
      }

      dedup_set.insert(index_to_add);

      column_indices.push_back(index_to_add);

    }
  } else {
    column_indices = {};
  }

#ifndef NDEBUG
  {
    DASSERT_EQ(column_indices.size(), column_names.size());
    for(size_t i = 0; i < column_names.size(); ++i ) {
      DASSERT_EQ(column_names[i], this->column_name(column_indices[i]));
    }
  }
#endif


  return column_indices;
}

void unity_xframe::delete_on_close() {
  if (is_materialized()) {
    get_underlying_xframe()->delete_files_on_destruction();
  }
}

std::shared_ptr<planner_node> unity_xframe::get_planner_node() {
  ASSERT_MSG(m_planner_node != nullptr,
             "Unintialized XFrame planner node cannot be used for read");
  return m_planner_node;
}

/**
 * Generate a new column name given existing column names.
 * New column name is in the form of X.1
 */
std::string unity_xframe::generate_next_column_name() {
  const auto& current_column_names = this->column_names();
  std::string name = std::string("X") + std::to_string(current_column_names.size() + 1);
  std::unordered_set<std::string> current_name_set(current_column_names.begin(),
                                                   current_column_names.end());

  // Resolve conflicts if the name is already taken
  while (current_name_set.count(name)) {
    name += ".";
    size_t number = 1;
    std::string non_conflict_name = name + std::to_string(number);
    while(current_name_set.count(non_conflict_name)) {
      ++number;
      non_conflict_name = name + std::to_string(number);
    }
    name = non_conflict_name;
  }
  return name;
}

void unity_xframe::show(const std::string& path_to_client) {
  using namespace turi;
  using namespace turi::visualization;

  std::shared_ptr<Plot> plt = std::dynamic_pointer_cast<Plot>(this->plot());

  if(plt != nullptr){
    plt->show(path_to_client);
  }
}

std::shared_ptr<model_base> unity_xframe::plot(){
  using namespace turi;
  using namespace turi::visualization;

  std::shared_ptr<unity_xframe_base> self = this->select_columns(this->column_names());

  return plot_columnwise_summary(self);
}

void unity_xframe::explore(const std::string& path_to_client, const std::string& title) {
  using namespace turi;
  using namespace turi::visualization;

  std::shared_ptr<unity_xframe> self = std::static_pointer_cast<unity_xframe>(this->select_columns(this->column_names()));

  logprogress_stream << "Materializing XFrame" << std::endl;
  this->materialize();

  if(self->size() == 0){
    log_and_throw("Nothing to explore; XFrame is empty.");
  }

  ::turi::visualization::run_thread([path_to_client, self, title]() {

    visualization::process_wrapper ew(path_to_client);
    std::stringstream table_spec;
    table_spec << "{\"table_spec\":" << visualization::table_spec(self, title) << "}" << std::endl;
    ew << table_spec.str();

    // This materializes if not already
    std::shared_ptr<xframe> underlying_xframe = self->get_underlying_xframe();

    // Get a reader just once.
    std::shared_ptr<xframe_reader> reader = underlying_xframe->get_reader();

    ew << visualization::table_data(self, reader.get(), 0, 100);

    while (ew.good()) {
      // get input, send responses
      std::string input;
      ew >> input;
      if (input.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      // parse the message as json
      flex_int start = -1, end = -1, index = -1;
      std::string column_name;

      enum class MethodType {None = 0, GetRows = 1, GetAccordion = 2};
      MethodType response = MethodType::None;

      auto sa = gl_sarray(std::vector<flexible_type>(1, input)).astype(flex_type_enum::DICT);
      flex_dict dict = sa[0].get<flex_dict>();
      for (const auto& pair : dict) {
        const auto& key = pair.first.get<flex_string>();
        const auto& value = pair.second;
        if (key == "method") {
          if(value.get<flex_string>() == "get_rows"){
            response = MethodType::GetRows;
          } else if(value.get<flex_string>() == "get_accordion"){
            response = MethodType::GetAccordion;
          }
        } else if (key == "start") {
          start = value.get<flex_int>();
        } else if (key == "end") {
          end = value.get<flex_int>();
        }else if (key == "column") {
          column_name = value.get<flex_string>();
        }else if (key == "index"){
          index = value.get<flex_int>();
        }
      }

      if (response == MethodType::GetRows) {
        ew << visualization::table_data(self, reader.get(), start, end);
      } else if (response == MethodType::GetAccordion) {
        ew << visualization::table_accordion(self, column_name, index);
      } else {
        std_log_and_throw(
          std::runtime_error, "Unsupported case (should be either GetRows or GetAccordion).");
        ASSERT_UNREACHABLE();
      }
    }
  });
}

} // namespace turi
