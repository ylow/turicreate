/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <model_server/server/registration.hpp>

#include <model_server/lib/simple_model.hpp>
#include <core/storage/xframe_interface/unity_sarray.hpp>
#include <core/storage/xframe_interface/unity_sarray_builder.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>
#include <core/storage/xframe_interface/unity_xframe_builder.hpp>

#include <model_server/lib/extensions/ml_model.hpp>

#include <visualization/server/show.hpp>


#include <core/export.hpp>
#include <model_server/lib/extensions/model_base.hpp>

namespace turi {

void register_functions(toolkit_function_registry& registry) {
  registry.register_toolkit_function(visualization::get_toolkit_function_registration());
}

namespace registration_internal {

// Define get_toolkit_class_registration for simple_model so that some toolkits
// can just wrap their outputs in a simple_model instance (without subclassing).
BEGIN_CLASS_REGISTRATION
REGISTER_CLASS(simple_model)
END_CLASS_REGISTRATION

}  // registration_internal namespace

void register_models(toolkit_class_registry& registry) {
  // Toolkits using simple_model
  registry.register_toolkit_class(
      registration_internal::get_toolkit_class_registration());
}

}
