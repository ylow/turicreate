/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_READER_BUFFER
#define TURI_XFRAME_READER_BUFFER
#include <memory>
#include <vector>
#include <core/data/flexible_type/flexible_type.hpp>
#include <core/storage/xframe_data/xframe.hpp>
#include <core/storage/xframe_data/xframe_constants.hpp>
namespace turi {
class xframe;


/**
 * \ingroup xframe_physical
 * \addtogroup xframe_main Main XFrame Objects
 * \{
 */

/**
 * A buffered reader reading from a range of an xframe<T>.
 *
 * \code
 * xframe<flexible_type> myxframe = ...;
 *
 * // Reader for the first thousand lines
 * xframe_reader_buffer<flexible_type> reader(myxframe, 0, 1000);
 *
 * while(reader.has_next()) {
 *  flexible_type val = reader.next();
 *  ... do some thing with val ...
 * }
 *
 * // Reader for the entire xframe
 * reader = xframe_reader_buffer<flexible_type>(myxframe, 0, (size_t)(-1));
 * ...
 * \endcode
 *
 * Internally, the reader maintains a vector as buffer, and when reading
 * reaches the end of the buffer, refill the buffer by reading from xframe.
 */
class xframe_reader_buffer {
 public:
  typedef xframe_rows::row value_type;

  xframe_reader_buffer() = default;

  /// Construct from xframe reader with begin and end row.
  xframe_reader_buffer(
      std::shared_ptr<typename xframe::reader_type> reader,
      size_t row_start, size_t row_end,
      size_t buffer_size = DEFAULT_SARRAY_READER_BUFFER_SIZE) {
    init(reader, row_start, row_end, buffer_size);
  }

  void init(std::shared_ptr<typename xframe::reader_type>& reader,
            size_t row_start, size_t row_end,
            size_t internal_buffer_size = DEFAULT_SARRAY_READER_BUFFER_SIZE) {
    m_reader = reader;
    m_buffer_pos = 0;
    m_iter = row_start;
    m_original_row_start = row_start;
    m_row_start = row_start;
    m_row_end = std::min(row_end, m_reader->size());
    m_buffer_size = internal_buffer_size;
    m_buffer.clear();
  }

  /// Return the next element in the reader.
  const xframe_rows::row& next();

  /// Returns the current element
  const xframe_rows::row& current();

  /// Return true if the reader has more element.
  bool has_next();

  /// Return the buffer.
  inline xframe_rows& get_buffer() {return m_buffer;}

  /// Return the Number of elements between row_start and row_end.
  inline size_t size() {return m_row_end - m_original_row_start;}

  /** Resets the buffer to the initial starting conditions. Reading
   *  from the buffer again will start from row_start.
   */
  void clear();

 private:
  /// Refill the chunk buffer form the xframe reader.
  void refill();

  typedef xframe::reader_type reader_type;

  /// Buffer the prefetched elements.
  xframe_rows m_buffer;

  /// Current value
  xframe_rows::row m_current;

  /// The underlying reader as a data source.
  std::shared_ptr<reader_type> m_reader;

  /// Current position of the buffer reader.
  size_t m_buffer_pos = 0;
  /// The initial starting point. clear() will reset row_start to here.
  size_t m_original_row_start = 0;
  /// Start row of the remaining chunk.
  size_t m_row_start = 0;
  /// End row of the chunk.
  size_t m_row_end = 0;
  /// The size of the buffer vector
  size_t m_buffer_size = 0;
  /// The current iterator location
  size_t m_iter = 0;
};

/// \}
//
/// Return the next element in the chunk.
inline const xframe_rows::row& xframe_reader_buffer::next() {
  if (m_buffer_pos == m_buffer.num_rows()) {
    refill();
    m_buffer_pos = 0;
  }
  DASSERT_LT(m_buffer_pos, m_buffer.num_rows());
  ++m_iter;
  m_current.copy_reference(m_buffer[m_buffer_pos++]);
  return m_current;
}

inline const xframe_rows::row& xframe_reader_buffer::current() {
  return m_current;
}

/// Return true if the chunk has remaining element.
inline bool xframe_reader_buffer::has_next() {
  return m_iter < m_row_end;
}

/// Refill the chunk buffer form the xframe reader.
inline void xframe_reader_buffer::refill() {
  size_t size_of_refill = std::min<size_t>(m_row_end - m_row_start, m_buffer_size);
  m_reader->read_rows(m_row_start, m_row_start + size_of_refill, m_buffer);
  m_row_start += size_of_refill;
}


inline void xframe_reader_buffer::clear() {
  m_buffer.clear();
  m_row_start = m_original_row_start;
  m_iter = m_original_row_start;
  m_buffer_pos = 0;
}
}

#endif
