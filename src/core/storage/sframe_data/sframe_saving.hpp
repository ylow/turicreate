/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_SAVING_HPP
#define TURI_XFRAME_SAVING_HPP
namespace turi {
class xframe;


/**
 * \ingroup xframe_physical
 * \addtogroup xframe_main Main XFrame Objects
 * \{
 */


/**
 * Saves an XFrame to another index file location using the most naive method:
 * decode rows, and write them
 */
void xframe_save_naive(const xframe& sf,
                       std::string index_file);

/**
 * Saves an XFrame to another index file location using a more efficient method,
 * block by block.
 */
void xframe_save_blockwise(const xframe& sf,
                           std::string index_file);

/**
 * Automatically determines the optimal strategy to save an xframe
 */
void xframe_save(const xframe& sf,
                 std::string index_file);

/**
 * Performs an "incomplete save" to a target index file location.
 * All this ensures is that the xframe's contents are located on the
 * same "file-system" (protocol) as the index file. Essentially the reference
 * save is guaranteed to be valid for only as long as no other XFrame files are
 * deleted.
 *
 * Essentially this can be used to build a "delta" XFrame.
 * - You already have an XFrame on disk somewhere. Say... /data/a
 * - You open it and add a column
 * - Calling xframe_save_weak_reference to save it to /data/b
 * - The saved XFrame in /data/b will include just the new column, but reference
 * /data/a for the remaining columns.
 *
 * \param sf The XFrame to save
 * \param index_file The output file location
 *
 */
void xframe_save_weak_reference(const xframe& sf,
                                std::string index_file);


/// \}
}; // naemspace turicreate

#endif
