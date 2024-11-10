/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef TURI_XFRAME_OUTPUT_ITERATOR_HPP
#define TURI_XFRAME_OUTPUT_ITERATOR_HPP
#include <iterator>

namespace turi {
class xframe_rows;

/**
 * \internal
 * An output iterator that accepts a stream of values writing them to
 * an XFrame.
 */
template <typename T, typename ConstRefFunction, typename MoveFunction, typename XFrameRowsFunction>
class xframe_function_output_iterator {
  typedef xframe_function_output_iterator self;
public:
  typedef std::output_iterator_tag iterator_category;
  typedef void                value_type;
  typedef void                difference_type;
  typedef void                pointer;
  typedef void                reference;

  explicit xframe_function_output_iterator() {}

  explicit xframe_function_output_iterator(const ConstRefFunction& f,
                                           const MoveFunction& f2,
                                           const XFrameRowsFunction& f3)
    : m_f(f), m_f2(f2), m_f3(f3) {}

  struct output_proxy {
    output_proxy(const ConstRefFunction& f,
                 const MoveFunction& f2,
                 const XFrameRowsFunction& f3) : m_f(f), m_f2(f2), m_f3(f3) { }

    output_proxy& operator=(const T& value) {
      m_f(value);
      return *this;
    }

    output_proxy& operator=(T&& value) {
      m_f2(std::move(value));
      return *this;
    }

    output_proxy& operator=(const xframe_rows& value) {
      m_f3(value);
      return *this;
    }

    const ConstRefFunction& m_f;
    const MoveFunction& m_f2;
    const XFrameRowsFunction& m_f3;
  };
  output_proxy operator*() { return output_proxy(m_f, m_f2, m_f3); }
  self& operator++() { return *this; }
  self& operator++(int) { return *this; }
private:
  ConstRefFunction m_f;
  MoveFunction m_f2;
  XFrameRowsFunction m_f3;
};

} // namespace turi

#endif // TURI_XFRAME_OUTPUT_ITERATOR_HPP
