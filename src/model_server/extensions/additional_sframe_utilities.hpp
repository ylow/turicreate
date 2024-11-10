/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_UNITY_EXTENSIONS_ADDITIONAL_XFRAME_UTILITIES_HPP
#define TURI_UNITY_EXTENSIONS_ADDITIONAL_XFRAME_UTILITIES_HPP
#include <core/data/xframe/gl_sarray.hpp>

void xframe_load_to_numpy(turi::gl_xframe input, size_t outptr_addr,
                     std::vector<size_t> outstrides,
                     std::vector<size_t> field_length,
                     size_t begin, size_t end);

#endif
