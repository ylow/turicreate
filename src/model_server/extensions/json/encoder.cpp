/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include "encoder.hpp"
#include "types.hpp"

#include <core/logging/assertions.hpp>
#include <core/logging/logger.hpp>
#include <core/data/sframe/gl_sarray.hpp>
#include <core/data/sframe/gl_sframe.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using namespace turi;

// typedefs
typedef std::map<std::string, variant_type> schema_t;

// forward declarations
static void _any_to_serializable(flexible_type& data, schema_t& schema, const variant_type& input);

static void _to_serializable(flexible_type& data, schema_t& schema, flex_float input) {
  schema.insert(std::make_pair("type", JSON::types::FLOAT));
  if (!std::isnan(input) && !std::isinf(input)) {
    data = input;
    return;
  }
  if (std::isnan(input)) {
    data = "NaN";
  } else {
    ASSERT_TRUE(std::isinf(input));
    if (input > 0) {
      data = "Infinity";
    } else {
      data = "-Infinity";
    }
  }
}

static void _to_serializable(flexible_type& data, schema_t& schema, const flex_vec& input) {
  schema.insert(std::make_pair("type", JSON::types::VECTOR));
  flex_list ret;
  for (const auto& value : input) {
    flexible_type serialized_value;
    schema_t serialized_schema;
    _any_to_serializable(serialized_value, serialized_schema, value);
    ret.push_back(serialized_value);
  }
  data = ret;
}

static void _to_serializable(flexible_type& data, schema_t& schema, const flex_list& input) {
  schema.insert(std::make_pair("type", JSON::types::LIST));
  std::vector<variant_type> nested_schema;
  flex_list ret;

  for (const auto& value : input) {
    flexible_type serialized_value;
    schema_t serialized_schema;
    _any_to_serializable(serialized_value, serialized_schema, value);
    nested_schema.push_back(serialized_schema);
    ret.push_back(serialized_value);
  }

  schema.insert(std::make_pair("nested", nested_schema));
  data = ret;
}

template<typename T>
static void _dict_to_serializable(flexible_type& data, schema_t& schema, const T& input) {
  schema.insert(std::make_pair("type", JSON::types::DICT));
  schema_t nested;
  flex_dict result;
  for (const auto& kv : input) {
    flexible_type value;
    schema_t value_schema;
    _any_to_serializable(value, value_schema, kv.second);
    result.push_back(std::make_pair(kv.first, value));
    nested.insert(std::make_pair(kv.first, value_schema));
  }
  schema.insert(std::make_pair("nested", nested));
  data = result;
}

static void _to_serializable(flexible_type& data, schema_t& schema, const flex_dict& input) {
  return _dict_to_serializable<flex_dict>(data, schema, input);
}

static void _to_serializable(flexible_type& data, schema_t& schema, const flex_date_time& input) {
  schema.insert(std::make_pair("type", JSON::types::DATETIME));
  flex_dict ret;

  int32_t time_zone_offset = input.time_zone_offset();
  ret.push_back(std::make_pair("posix_timestamp", input.posix_timestamp()));
  ret.push_back(std::make_pair("tz_15_min_offset",
    time_zone_offset == 64 ? FLEX_UNDEFINED : flexible_type(flex_int(time_zone_offset))));
  ret.push_back(std::make_pair("microsecond", input.microsecond()));

  data = ret;
}


static void _to_serializable(flexible_type& data, schema_t& schema, const flexible_type& input) {
  switch (input.get_type()) {
    case flex_type_enum::INTEGER:
      data = input.get<flex_int>();
      schema.insert(std::make_pair("type", JSON::types::INTEGER));
      break;
    case flex_type_enum::FLOAT:
      _to_serializable(data, schema, input.get<flex_float>());
      break;
    case flex_type_enum::STRING:
      data = input.get<flex_string>();
      schema.insert(std::make_pair("type", JSON::types::STRING));
      break;
    case flex_type_enum::VECTOR:
      _to_serializable(data, schema, input.get<flex_vec>());
      break;
    case flex_type_enum::LIST:
      _to_serializable(data, schema, input.get<flex_list>());
      break;
    case flex_type_enum::DICT:
      _to_serializable(data, schema, input.get<flex_dict>());
      break;
    case flex_type_enum::DATETIME:
      _to_serializable(data, schema, input.get<flex_date_time>());
      break;
    case flex_type_enum::UNDEFINED:
      data = FLEX_UNDEFINED;
      schema.insert(std::make_pair("type", JSON::types::UNDEFINED));
      break;
    case flex_type_enum::ND_VECTOR:
      log_and_throw("Unsupported flex_type_enum case: ND_VECTOR");
      break;
    default:
      log_and_throw("Unsupported flex_type_enum case");
      break;
  }
}

static void _to_serializable(flexible_type& data, schema_t& schema, const gl_sframe& input) {
  schema.insert(std::make_pair("type", JSON::types::SFRAME));
  flex_dict data_dict;

  flex_list column_names;
  for (const auto& name : input.column_names()) {
    column_names.push_back(name);
  }
  data_dict.push_back(std::make_pair("column_names", column_names));

  std::vector<flexible_type> columns;
  for (const auto& name : column_names) {
    const auto& column = input.select_column(name);
    flexible_type serialized_column;
    schema_t serialized_schema;
    _any_to_serializable(serialized_column, serialized_schema, column);
    columns.push_back(serialized_column);
  }
  data_dict.push_back(std::make_pair("columns", columns));
  data = data_dict;
}

static void _to_serializable(flexible_type& data, schema_t& schema, const gl_sarray& input) {
  schema.insert(std::make_pair("type", JSON::types::SARRAY));
  flex_dict data_dict;
  data_dict.push_back(std::make_pair("dtype", flex_type_enum_to_name(input.dtype())));

  // will throw away schema from individual values
  flex_list values;
  for (const auto& value : input.range_iterator()) {
    flexible_type serialized_value;
    schema_t serialized_schema;
    _any_to_serializable(serialized_value, serialized_schema, value);
    values.push_back(serialized_value);
  }
  data_dict.push_back(std::make_pair("values", values));
  data = data_dict;
}

static void _to_serializable(flexible_type& data, schema_t& schema, const std::map<std::string, variant_type>& input) {
  _dict_to_serializable<std::map<std::string, variant_type>>(data, schema, input);
}

static void _to_serializable(flexible_type& data, schema_t& schema, const std::vector<variant_type>& input) {
  schema.insert(std::make_pair("type", JSON::types::LIST));
  flex_list ret;
  std::vector<variant_type> nested_schema;
  for (const auto& value : input) {
    flexible_type serialized_value;
    schema_t value_schema;
    _any_to_serializable(serialized_value, value_schema, value);
    ret.push_back(serialized_value);
    nested_schema.push_back(value_schema);
  }
  schema.insert(std::make_pair("nested", nested_schema));
  data = ret;
}

static void _any_to_serializable(flexible_type& data, schema_t& schema, const variant_type& input) {
  switch (input.which()) {
    case 0:
      // flexible type
      _to_serializable(data, schema, variant_get_value<flexible_type>(input));
      break;
    case 4:
      // sframe
      _to_serializable(data, schema, variant_get_value<gl_sframe>(input));
      break;
    case 5:
      // sarray
      _to_serializable(data, schema, variant_get_value<gl_sarray>(input));
      break;
    case 6:
      // dictionary
      _to_serializable(data, schema, variant_get_value<std::map<std::string, variant_type>>(input));
      break;
    case 7:
      // list
      _to_serializable(data, schema, variant_get_value<std::vector<variant_type>>(input));
      break;
    default:
      log_and_throw("Unsupported type for to_serializable. Expected a flexible_type, SFrame, SArray, dictionary, or list.");
  }
}

variant_type JSON::to_serializable(variant_type input) {
  flexible_type data;
  schema_t schema;
  _any_to_serializable(data, schema, input);
  std::vector<variant_type> ret({
    data,
    schema
  });
  return ret;
}
