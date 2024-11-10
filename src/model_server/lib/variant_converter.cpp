/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <model_server/lib/variant.hpp>
#include <model_server/lib/unity_global.hpp>
#include <model_server/lib/variant_converter.hpp>
#include <core/storage/xframe_interface/unity_sarray.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>

#ifndef DISABLE_SDK_TYPES
#include <core/data/xframe/gl_xframe.hpp>
#include <core/data/xframe/gl_sarray.hpp>
#include <core/globals/globals.hpp>
#endif

#include <model_server/lib/api/function_closure_info.hpp>

namespace turi {

#ifndef DISABLE_SDK_TYPES
int64_t USE_GL_DATATYPE = 0;
REGISTER_GLOBAL(int64_t, USE_GL_DATATYPE, true);
#endif

std::shared_ptr<unity_sarray> variant_converter<std::shared_ptr<unity_sarray>, void>::get(const variant_type& val) {
  return std::static_pointer_cast<unity_sarray>(variant_get_ref<std::shared_ptr<unity_sarray_base>>(val));
}

variant_type variant_converter<std::shared_ptr<unity_sarray>, void>::set(std::shared_ptr<unity_sarray> val) {
  return variant_type(std::static_pointer_cast<unity_sarray_base>(val));
}


std::shared_ptr<unity_xframe> variant_converter<std::shared_ptr<unity_xframe>, void>::get(const variant_type& val) {
  return std::static_pointer_cast<unity_xframe>(variant_get_ref<std::shared_ptr<unity_xframe_base>>(val));
}

variant_type variant_converter<std::shared_ptr<unity_xframe>, void>::set(std::shared_ptr<unity_xframe> val) {
  return variant_type(std::static_pointer_cast<unity_xframe_base>(val));
}



#ifndef DISABLE_SDK_TYPES
gl_sarray variant_converter<gl_sarray, void>::get(const variant_type& val) {
  return variant_get_ref<std::shared_ptr<unity_sarray_base>>(val);
}
variant_type variant_converter<gl_sarray, void>::set(gl_sarray val) {
  if (USE_GL_DATATYPE) {
    return variant_type(std::dynamic_pointer_cast<model_base>(std::make_shared<gl_sarray>(val)));
  } else {
    return variant_type(std::shared_ptr<unity_sarray_base>(val));
  }
}

gl_xframe variant_converter<gl_xframe, void>::get(const variant_type& val) {
  return variant_get_ref<std::shared_ptr<unity_xframe_base>>(val);
}
variant_type variant_converter<gl_xframe, void>::set(gl_xframe val) {
  return variant_type(std::shared_ptr<unity_xframe_base>(val));
};

#endif

} // turicreate
