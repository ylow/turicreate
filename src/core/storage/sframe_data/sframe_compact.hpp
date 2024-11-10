/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_COMPACT_HPP
#define TURI_XFRAME_COMPACT_HPP
#include <vector>
#include <memory>
namespace turi {
class xframe;

template <typename T>
class sarray;
/**
 * xframe_fast_compact looks for runs of small segments
 * (comprising of less than FAST_COMPACT_BLOCKS_IN_SMALL_SEGMENT block), and
 * rebuilds them into bigger segments.
 * Returns true if any compaction was performed.
 */
bool xframe_fast_compact(const xframe& sf);

/**
 * Inplace compacts an XFrame. Fast compact is tried first and if
 * the number of segments do not fall below the target, a slow compaction
 * is performed.
 */
void xframe_compact(xframe& sf, size_t segment_threshold);

/**
 * sarray_fast_compact looks for runs of small segments
 * (comprising of less than FAST_COMPACT_BLOCKS_IN_SMALL_SEGMENT block), and
 * rebuilds them into bigger segments.
 * Returns true if any compaction was performed.
 */
template <typename T>
bool sarray_fast_compact(sarray<T>& column);

/**
 * Inplace compacts an SArray. Fast compact is tried first and if
 * the number of segments do not fall below the target, a slow compaction
 * is performed.
 */
template <typename T>
void sarray_compact(sarray<T>& column, size_t segment_threshold);

} // turi
#endif
