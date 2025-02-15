/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <string>
#include <vector>
#include <core/parallel/lambda_omp.hpp>
#include <core/parallel/pthread_tools.hpp>
#include <core/data/sframe/gl_sframe.hpp>
#include <model_server/lib/toolkit_function_macros.hpp>
#include <core/storage/sframe_data/sframe_config.hpp>
#include "additional_sframe_utilities.hpp"

using namespace turi;

void copy_to_memory(const sframe_rows::row& data,
                    float* outptr,
                    const std::vector<size_t>& outstrides,
                    const std::vector<size_t>& outshape) {
  ASSERT_GE(data.size(), 1);

  for (size_t i = 0; i < data.size(); ++i) {
    ASSERT_NE((int)data[i].get_type(), (int)flex_type_enum::UNDEFINED);
  }

  auto type = data[0].get_type();
  if (data.size() == 1 && (type == flex_type_enum::FLOAT || type == flex_type_enum::INTEGER)) {
    // Case 2: Single value type (should really get rid of this special case)
    ASSERT_EQ(outstrides.size(), 0);
    (*outptr) = (float)(data[0]);
    return;
  } else if (data.size() == 1 && type == flex_type_enum::LIST) {
    // Case 3: 2D arrays: list of vectors or list of lists of values
    ASSERT_EQ(outshape.size(), 2);
    const flex_list& dim0_lst = data[0].to<flex_list>();
    ASSERT_EQ(dim0_lst.size(), outshape[0]);
    for (size_t i = 0; i < dim0_lst.size(); ++i) {
      auto dim1_type = dim0_lst[i].get_type();
      if (dim1_type == flex_type_enum::VECTOR) {
        const flex_vec& dim1_vec = dim0_lst[i].to<flex_vec>();
        ASSERT_EQ(dim1_vec.size(), outshape[1]);
        for (size_t j = 0; j < dim1_vec.size(); ++j) {
          outptr[outstrides[0] * i + outstrides[1] * j] = (float)(dim1_vec[j]);
        }
      } else if (dim1_type == flex_type_enum::LIST) {
        const flex_list& dim1_lst = dim0_lst[i].to<flex_list>();
        ASSERT_EQ(dim1_lst.size(), outshape[1]);
        for (size_t j = 0; j < dim1_lst.size(); ++j) {
          auto value_type = dim1_lst[j].get_type();
          if (value_type == flex_type_enum::INTEGER ||
              value_type == flex_type_enum::FLOAT) {
            outptr[outstrides[0] * i + outstrides[1] * j] = (float)(dim1_lst[j]);
          } else {
            ASSERT_MSG(false, "Unsupported typed");
          }
        }
      } else {
        ASSERT_MSG(false, "Unsupported typed");
      }
    }
  } else {
    // Case 4: Array type or mixed types
    ASSERT_EQ(outstrides.size(), 1);
    ASSERT_EQ(outshape.size(), 1);
    size_t pos = 0;
    for (size_t i = 0; i < data.size(); ++i) {
      auto type = data[i].get_type();
      if (type == flex_type_enum::VECTOR) {
        const flex_vec& v = data[i].to<flex_vec>();
        for (size_t j = 0; j < v.size(); ++j) {
          outptr[outstrides[0] * pos] = (float)(v[j]);
          ++pos;
        }
      } else if (type == flex_type_enum::INTEGER ||
                 type == flex_type_enum::FLOAT) {
        outptr[outstrides[0] * pos] = (float)(data[i]);
        ++pos;
      } else {
        ASSERT_MSG(false, "Unsupported type");
      }
    }
    ASSERT_EQ(pos, outshape[0]);
  }
  return;
}

void sframe_load_to_numpy(turi::gl_sframe input, size_t outptr_addr,
			  std::vector<size_t> outstrides,
			  std::vector<size_t> outshape,
			  size_t begin, size_t end) {
  if (!input.is_materialized()) {
    input.materialize();
  }

  ASSERT_MSG(input.num_columns() > 0, "SFrame has no column");
  float* outptr = reinterpret_cast<float*>(outptr_addr);

  ASSERT_EQ(outstrides.size(), outshape.size());
  ASSERT_GE(outstrides.size(), 1);
  for (size_t& stride: outstrides) {
    stride /= sizeof(float);
  }

  // we consume the first index. copy_to_memory takes the rest
  size_t row_stride = outstrides[0];
  outstrides.erase(outstrides.begin());
  outshape.erase(outshape.begin());

  const size_t num_rows = end - begin;
  in_parallel([&](size_t worker_idx, size_t num_workers) {
    // Compute the input range and output address for this thread.
    size_t worker_begin = begin + num_rows * worker_idx / num_workers;
    size_t worker_end = begin + num_rows * (worker_idx + 1) / num_workers;
    float* worker_out = outptr + row_stride * (worker_begin - begin);

    for (const auto& row : input.range_iterator(worker_begin, worker_end)) {
      copy_to_memory(row, worker_out, outstrides, outshape);
      worker_out += row_stride;
    }
  });
}


BEGIN_FUNCTION_REGISTRATION
REGISTER_FUNCTION(sframe_load_to_numpy, "input", "outptr_addr", "outstrides", "outshape", "begin", "end");
END_FUNCTION_REGISTRATION
