/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef XFRAME_ALGORITHM_EC_PERMUTE_HPP
#define XFRAME_ALGORITHM_EC_PERMUTE_HPP


#include <vector>
#include <memory>
/*
 * See ec_sort.hpp for details
 */
namespace turi {
class xframe;

template <typename T>
class sarray;

namespace query_eval {

/**
 * \ingroup xframe_query_engine
 * \addtogroup Algorithms Algorithms
 * \{
 */
/**
 * Permutes an xframe by a forward map.
 * forward_map has the same length as the xframe and must be a permutation
 * of all the integers [0, len-1].
 *
 * The input xframe is then permuted so that xframe row i is written to row
 * forward_map[i] of the returned xframe.
 *
 * \note The forward_map is not checked that it is a valid permutation
 * If the constraints is not met, either an exception will be thrown, or
 * the result is ill-defined.
 */
xframe permute_xframe(xframe &values_xframe,
                      std::shared_ptr<sarray<flexible_type> > forward_map);

/// \}
} // query_eval
} // turicreate

#endif
