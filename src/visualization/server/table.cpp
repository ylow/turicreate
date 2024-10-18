/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <visualization/server/escape.hpp>
#include <visualization/server/server.hpp>
#include <visualization/server/table.hpp>

#include <core/data/sframe/gl_sframe.hpp>
#include <core/storage/sframe_interface/unity_sframe.hpp>
#include <core/storage/sframe_data/sframe.hpp>

namespace turi {
namespace visualization {

std::string table_spec(const std::shared_ptr<unity_sframe>& table, const std::string& title, std::string table_id) {
  // If table ID is unspecified, add it to the web server so we can get an ID reference to it
  // for image loading and other data streaming over HTTP
  if (table_id == "") {
    table_id = WebServer::get_instance().add_table(table, title);
  }

  std::string titleString = turi::visualization::extra_label_escape(title);
  const auto& column_names = table->column_names();
  const auto& column_types = table->dtype();

  std::stringstream ss;
  ss << "{\"column_names\": [";
  for (size_t i=0; i<table->num_columns(); i++) {
    const auto& name = column_names[i];
    ss << visualization::extra_label_escape(name);
    if (i != table->num_columns() - 1) {
      ss << ",";
    }
  }
  ss << "], \"size\": ";
  ss << table->size();
  ss << ", \"title\": ";
  ss << titleString;
  ss << ", \"column_types\": [";
  for (size_t i=0; i<table->num_columns(); i++) {
    const auto& type = column_types[i];
    ss << "\"" << flex_type_enum_to_name(type) << "\"";
    if (i != table->num_columns() - 1) {
      ss << ",";
    }
  }
  ss << "], \"table_id\": " << table_id;
  ss << ", \"base_url\": " << escape_string(WebServer::get_base_url());
  ss << "}";
  return ss.str();
}

std::string table_data(const std::shared_ptr<unity_sframe>& table, sframe_reader* reader, size_t start, size_t end) {

  const auto& column_names = table->column_names();

  sframe_rows rows;
  reader->read_rows(start, end, rows);
  std::stringstream ss;

  // for DateTime string formatting
  ss.exceptions(std::ios_base::failbit);
  ss.imbue(std::locale(ss.getloc(),
                        new boost::local_time::local_time_facet("%Y-%m-%d %H:%M:%S%ZP")));
  // {"data_spec": {"values": [{"a": "A","b": 28}, {"a": "B","b": 55}, {"a": "C","b": 43},{"a": "D","b": 91}, {"a": "E","b": 81}, {"a": "F","b": 53},{"a": "G","b": 19}, {"a": "H","b": 87}, {"a": "I","b": 52}]}}
  ss << "{\"data_spec\": {\"values\": [";
  size_t i = 0;
  for (const auto& row: rows) {
    ss << "{";
    size_t count = start + i;
    ss << "\"__idx\": \"" << count << "\",";
    for (size_t j=0; j<row.size(); j++) {
      const auto& columnName = column_names[j];
      const auto& value = row[j];

      ss << visualization::extra_label_escape(columnName) << ": ";
      ss << escapeForTable(value, count, columnName);

      if (j != row.size() - 1) {
        ss << ",";
      }
    }
    ss << "}";
    if (i != rows.num_rows() - 1) {
      ss << ",";
    }
    ++i;
  }
  ss << "]}}" << std::endl;
  return ss.str();
}

std::string table_accordion(const std::shared_ptr<unity_sframe>& table, const std::string& column_name, size_t row_idx) {
  using namespace boost;
  using namespace local_time;
  using namespace gregorian;
  using posix_time::time_duration;

  static time_zone_names empty_timezone("", "", "", "");
  static time_duration empty_utc_offset(0,0,0);
  static dst_adjustment_offsets empty_adj_offsets(time_duration(0,0,0),
                                              time_duration(0,0,0),
                                              time_duration(0,0,0));
  static time_zone_ptr empty_tz(
      new custom_time_zone(empty_timezone, empty_utc_offset,
                            empty_adj_offsets,
                            boost::shared_ptr<dst_calc_rule>()));

  const auto& column_names = table->column_names();
  ASSERT_TRUE(std::find(column_names.begin(), column_names.end(), column_name) != column_names.end());
  DASSERT_LT(row_idx, table->size());
  DASSERT_GE(row_idx, 0);

  auto accordion_sa = table->select_column(column_name);
  auto gl_sa = gl_sarray(accordion_sa);

  flexible_type value = gl_sa[row_idx];

  switch (value.get_type()) {
    case flex_type_enum::UNDEFINED:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": null";
        ss << "}}" << std::endl;
        return ss.str();
      }
    case flex_type_enum::FLOAT:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": " << value.get<flex_float>();
        ss << "}}" << std::endl;
        return ss.str();
      }
    case flex_type_enum::INTEGER:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": " << value.get<flex_int>();
        ss << "}}" << std::endl;
        return ss.str();
      }
    case flex_type_enum::DATETIME:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": ";
        ss << "\"";
        const auto& dt = value.get<flex_date_time>();

        if (dt.time_zone_offset() != flex_date_time::EMPTY_TIMEZONE) {
          std::string prefix = "0.";
          int sign_adjuster = 1;
          if(dt.time_zone_offset() < 0) {
            sign_adjuster = -1;
            prefix = "-0.";
          }
          boost::local_time::time_zone_ptr zone(
              new boost::local_time::posix_time_zone(
                  "GMT" + prefix +
                  std::to_string(sign_adjuster *
                                  dt.time_zone_offset() *
                                  flex_date_time::TIMEZONE_RESOLUTION_IN_MINUTES)));
          boost::local_time::local_date_time az(
              flexible_type_impl::ptime_from_time_t(dt.posix_timestamp(),
                                                    dt.microsecond()), zone);
          ss << az;
        } else {
          boost::local_time::local_date_time az(
              flexible_type_impl::ptime_from_time_t(dt.posix_timestamp(),
                                                    dt.microsecond()),
              empty_tz);
          ss << az;
        }

        ss << "\"}}" << std::endl;
        return ss.str();
      }
    case flex_type_enum::VECTOR:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": ";
        std::stringstream strm;
        const flex_vec& vec = value.get<flex_vec>();

        strm << "[";
        for (size_t i = 0; i < vec.size(); ++i) {
          strm << vec[i];
          if (i + 1 < vec.size()) strm << ", ";
        }
        strm << "]";
        std::string default_string;

        ss << turi::visualization::extra_label_escape(strm.str());
        ss << "}}" << std::endl;
        return ss.str();
      }
    case flex_type_enum::LIST:
    case flex_type_enum::DICT:
    case flex_type_enum::ND_VECTOR:
    case flex_type_enum::STRING:
    default:
      {
        std::stringstream ss;
        ss << "{\"accordion_spec\": {\"index\": " << row_idx << ", \"column\":" << turi::visualization::extra_label_escape(column_name);
        ss << ", \"type\": " << value.get_type();
        ss << ", \"data\": " << escapeForTable(value);
        ss << "}}" << std::endl;
        return ss.str();
      }
  };
}

}} // turi::visualization
