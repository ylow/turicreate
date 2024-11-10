/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_SHUFFLE_HPP
#define TURI_XFRAME_SHUFFLE_HPP

#include <vector>
#include <core/storage/xframe_data/xframe.hpp>

namespace turi {

/**
 * \ingroup xframe_physical
 * \addtogroup xframe_main Main XFrame Objects
 * \{
 */


/**
 * Shuffle the rows in one xframe into a collection of n xframes.
 * Each output XFrame contains one segment.
 *
 * \code
 * std::vector<xframe> ret(n);
 * for (auto& sf : ret) {
 *   INIT_WITH_NAMES_COLUMNS_AND_ONE_SEG(xframe_in.column_names(), xframe_in.column_types());
 * }
 * for (auto& row : xframe_in) {
 *   size_t idx = hash_fn(row) % n;
 *   add_row_to_xframe(ret[idx], row); // the order of addition is not guaranteed.
 * }
 * \endcode
 *
 * The result xframes have the same column names and types (including
 * empty xframes). A result xframe can have 0 rows if non of the
 * rows in the input xframe is hashed to it. (If n is greater than
 * the size of input xframe, there will be at (n - xframe_in.size())
 * empty xframes in the return vector.
 *
 * \param n the number of output xframe.
 * \param hash_fn the hash function for each row in the input xframe.
 *
 * \return A vector of n xframes.
 */
std::vector<xframe> shuffle(
     xframe xframe_in,
     size_t n,
     std::function<size_t(const std::vector<flexible_type>&)> hash_fn,
     std::function<void(const std::vector<flexible_type>&, size_t)> emit_call_back
      = std::function<void(const std::vector<flexible_type>&, size_t)>());

/// \}
//
} // turi

#endif
