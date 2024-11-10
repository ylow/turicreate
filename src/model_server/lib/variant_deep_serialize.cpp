/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#include <model_server/lib/variant.hpp>
#include <model_server/lib/variant_converter.hpp>
#include <model_server/lib/variant_deep_serialize.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>
#include <core/storage/xframe_interface/unity_sarray.hpp>
#include <core/storage/serialization/serialization_includes.hpp>
namespace turi {
/**
 * Serialize the variant type, deep copying the pointer types.
 */
void variant_deep_save(const variant_type& v, oarchive& oarc) {
  oarc << v.which();
  switch(v.which()) {
   case 0:
     oarc << boost::get<flexible_type>(v);
     break;
   case 1:
     oarc << boost::get<dataframe_t>(v);
     break;
   case 2:
     log_and_throw(std::string("Unable to serialize unity model pointer"));
     break;
   case 3:
     {
       std::shared_ptr<unity_xframe> s =
           std::static_pointer_cast<unity_xframe>(variant_get_ref<std::shared_ptr<unity_xframe_base>>(v));
       oarc << *s;
       break;
     }
     break;
   case 4:
     {
       std::shared_ptr<unity_sarray> s =
           std::static_pointer_cast<unity_sarray>(variant_get_ref<std::shared_ptr<unity_sarray_base>>(v));
       oarc << *s;
       break;
     }
     break;
   case 5:
     {
       const variant_map_type& varmap = variant_get_ref<variant_map_type>(v);
       oarc << (size_t)varmap.size();
       for(const auto& elem : varmap) {
         oarc << elem.first;
         variant_deep_save(elem.second, oarc);
       }
       break;
     }
   case 6:
     {
       const variant_vector_type& varvec = variant_get_ref<variant_vector_type>(v);
       oarc << (size_t)varvec.size();
       for(const auto& elem : varvec) {
         variant_deep_save(elem, oarc);
       }
       break;
     }
   default:
     break;
  };
}

/**
 * Deserialize the variant type, allocate new resources for the pointer types.
 */
void variant_deep_load(variant_type& v, iarchive& iarc) {
  int which;
  iarc >> which;
  switch(which) {
   case 0:
     {
       v = flexible_type();
       iarc >> boost::get<flexible_type>(v);
       break;
     }
   case 1:
     {
       v = dataframe_t();
       iarc >> boost::get<dataframe_t>(v);
       break;
     }
   case 2:
     log_and_throw(std::string("Unable to deseralize unity model pointer"));
     break;
   case 3:
     {
       std::shared_ptr<unity_xframe> s(new unity_xframe());
       iarc >> *s;
       variant_set_value<std::shared_ptr<unity_xframe>>(v, s);
       break;
     }
   case 4:
     {
       std::shared_ptr<unity_sarray> s(new unity_sarray());
       iarc >> *s;
       variant_set_value<std::shared_ptr<unity_sarray>>(v, s);
       break;
     }
   case 5:
     {
       size_t numvals;
       iarc >> numvals;
       variant_map_type varmap;
       for (size_t i = 0;i < numvals; ++i) {
         std::string key;
         variant_type value;
         iarc >> key;
         variant_deep_load(value, iarc);
         varmap[key] = std::move(value);
       }
       variant_set_value<variant_map_type>(v, varmap);
       break;
     }
   case 6:
     {
       size_t numvals;
       iarc >> numvals;
       variant_vector_type varvec;
       varvec.resize(numvals);
       for (size_t i = 0;i < numvals; ++i) {
         variant_type value;
         variant_deep_load(value, iarc);
         varvec[i] = std::move(value);
       }
       variant_set_value<variant_vector_type>(v, varvec);
       break;
     }
   default:
     break;
  }
};
} // namespace turi
