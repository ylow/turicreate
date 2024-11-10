/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_CONFIG_HPP
#define TURI_XFRAME_CONFIG_HPP
#include <cstddef>
namespace turi {


/**
 * \ingroup xframe_physical
 * \addtogroup xframe_main Main XFrame Objects
 * \{
 */

/**
** Global configuration for xframe, keep them as non-constants because we want to
** allow user/server to change the configuration according to the environment
**/
namespace xframe_config {
  /**
  **  The max buffer size to keep for sorting in memory
  **/
  extern size_t XFRAME_SORT_BUFFER_SIZE;

  /**
  **  The number of rows to read each time for paralleliterator
  **/
  extern size_t XFRAME_READ_BATCH_SIZE;
}

/// \}
}
#endif //TURI_XFRAME_CONFIG_HPP
