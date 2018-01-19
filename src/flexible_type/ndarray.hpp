#ifndef TURI_FLEXIBLE_TYPE_NDARRAY
#define TURI_FLEXIBLE_TYPE_NDARRAY
#include <tuple>
#include <logger/assertions.hpp>
#include <serialization/serialization_includes.hpp>
namespace turi {
namespace flexible_type_impl {

/**
 * A generic dense multidimensional array.
 *
 * This class implements a very minimal generic dense multidimensional 
 * array type.
 * 
 * The basic layout is simple.
 *  - elems: is a flattened array of all the elements
 *  - start: The offset of the 0th element in elems.
 *  - shape: is the dimensions of the ndarray. The product of all the values in
 *    shape should equal elems.length()
 *  - stride: is used to convert between ND-indices and element indices. stride
 *    is the same length as shape. Given an ND-index (x0,x1,x2...xn), the
 *    linear index is \f$ \prod_i x_i \times stride_i \f$. There are no 
 *    constraints on stride (i.e. with appropriate stride values, C, fortran, 
 *    or sub-matrix layouts on elements can be constructed).
 **/
template <typename T>
class ndarray {
 public:
  typedef size_t index_type;
  typedef T value_type;
  typedef std::vector<index_type> index_range_type;
  typedef std::vector<T> container_type;

 private:
  std::shared_ptr<container_type> m_elem;
  index_range_type m_shape;
  index_range_type m_stride;
  index_type m_start = 0;

 public:

  /// construct with custom stride ordering
  ndarray(const container_type& elements = container_type(), 
          const index_range_type& shape = index_range_type(),
          const index_range_type& stride = index_range_type(),
          const index_type start = 0):
              ndarray(std::make_shared<container_type>(elements), shape, stride, start) {}

  /// construct with custom stride ordering
  ndarray(const std::shared_ptr<container_type>& elements, 
          const index_range_type& shape = index_range_type(),
          const index_range_type& stride = index_range_type(),
          const index_type start = 0): 
              m_elem(elements), m_shape(shape), m_stride(stride), m_start(start) { 
    if (m_shape.size() == 0 && elements->size() - m_start > 0) {
      m_shape.push_back(elements->size() - m_start);
    }
    if (m_stride.size() == 0 && m_shape.size() > 0) {
      m_stride.resize(m_shape.size());
      m_stride[0] = 1;
      for (size_t i = 1;i < m_shape.size(); ++i) {
        m_stride[i] = m_stride[i - 1] * m_shape[i - 1];     
      }
    }
    ASSERT_TRUE(is_valid());
  }

  /// resizes the array only if the shape is 1D
  void resize(size_t size) {
    if (m_shape.size() == 0) {
      m_start = 0;
      m_shape.push_back(1);
      m_elem->resize(1);
    } else {
      ASSERT_EQ(m_start, 0);
      ASSERT_EQ(m_shape.size(), 1);
      ASSERT_EQ(m_shape[0], m_elem->size());
      m_elem->resize(size);
      m_shape[0] = size;
    }
  }


  /// push back only if the shape is 1D
  void push_back(const T& elem) {
    if (m_shape.size() == 0) {
      m_start = 0;
      m_shape.push_back(1);
      m_elem->push_back(elem);
    } else {
      ASSERT_EQ(m_start, 0);
      ASSERT_EQ(m_shape.size(), 1);
      ASSERT_EQ(m_shape[0], m_elem->size());
      m_elem->push_back(elem);
      m_shape[0] = m_elem->size();
    }
  }

  bool empty() const {
    return m_elem->empty();
  }

  /**
   * Returns the linear index given an N-d index
   * performing bounds checking on the index ranges.
   * 
   * \code
   * std::vector<size_t> indices = {1,5,2};
   * arr.at(arr.index(indices)) = 10 // also bounds check the linear index
   * arr[arr.index(indices)] = 10    // does not bounds check the linear index
   * \endcode
   */
  template <typename U>
  index_type index(const std::vector<U>& index) const {
    ASSERT_EQ(m_stride.size(), index.size());

    size_t idx = 0;
    for (size_t i = 0; i < index.size(); ++i) {
      index_type v = index[i];
      ASSERT_LT(v, m_shape[i]);
      idx += v * m_stride[i];
    }
    return idx;
  }

  /**
   * Returns the linear index given an N-d index
   * without performing bounds checking on the index ranges.
   *
   * \code
   * std::vector<size_t> indices = {1,5,2};
   * arr[arr.fast_index(indices)] = 10 // does not bounds check the linear index
   * arr.at(arr.fast_index(indices)) = 10 // bounds check the linear index
   * \endcode
   */
  template <typename U>
  index_type fast_index(const std::vector<U>& index) const {
    size_t idx = 0;
    for (size_t i = 0; i < index.size(); ++i) {
      index_type v = index[i];
      idx += v * m_stride[i];
    }
    return idx;
  }

  /**
   * Returns a reference to an element given the linear index, no bounds
   * checking is performed.
   */
  value_type& operator[](size_t elem_index) {
    return (*m_elem)[m_start + elem_index];
  }

  /**
   * Returns a const reference to an element given the linear index, no bounds
   * checking is performed.
   */
  const value_type& operator[](size_t elem_index) const {
    return (*m_elem)[m_start + elem_index];
  }

  /**
   * Returns a reference to an element given the linear index, performing bounds
   * checking on the index range.
   */
  value_type& at(size_t elem_index) {
    ASSERT_LT(m_start + elem_index, m_elem->size());
    return (*m_elem)[m_start + elem_index];
  }

  /**
   * Returns a const reference to an element given the linear index, performing
   * checking on the index range.
   */
  const value_type& at(size_t elem_index) const {
    ASSERT_LT(m_start + elem_index, m_elem->size());
    return (*m_elem)[m_start + elem_index];
  }

  /**
   * Returns a reference to all the elements in a linear layout.
   */
  container_type& elements() {
    return *m_elem;
  }
  /**
   * Returns a const reference to all the elements in a linear layout.
   */
  const container_type& elements() const {
    return *m_elem;
  }

  /**
   * Returns a const reference to the shape.
   */
  const index_range_type& shape() const {
    return m_shape;
  }

  /**
   * Returns a const reference to the stride.
   */
  const index_range_type& stride() const {
    return m_stride;
  }

  /**
   * Returns a const reference to the stride.
   */
  index_type start() const {
    return m_start;
  }

  /**
   * Returns the number of elements in the array.
   *
   * This is equivalent to the product of the values in the shape array.
   * Note that this may not be the same as elements().size().
   */
  size_t num_elem() const {
    if (m_shape.size() == 0) return 0;
    size_t p = 1;
    for (size_t s: m_shape) {
      p = p * s;
    }
    return p;
  }

  /**
   * Returns true if every element in elements() is reachable by an
   * N-d index.
   */
  bool is_full() const {
    return m_start == 0 && num_elem() == m_elem->size() && last_index() == m_elem->size();
  }

  /**
   * Returns true if the shape and stride of the array is laid out 
   * correctly such at all array indices are within elements().size().
   * 
   * An ndarray can be invalid for instance, if the stride is too large,
   * or if the shape is larger than the total number of elements.
   */
  bool is_valid() const {
    return num_elem() + m_start <= m_elem->size() && last_index() + m_start <= m_elem->size();
  }

  /** 
   * Returns true if the stride is ordered canonically.
   * The strides must be non-decreasing and non-zero.
  */
  bool has_canonical_stride() const {
    if (m_stride.size() == 0) return true;
    if (m_stride[0] == 0) return false;
    for (size_t i = 1; i < m_stride.size(); ++i) {
      if (m_stride[i] == 0 || m_stride[i - 1] > m_stride[i]) {
        return false;
      }
    }
    return true;
  }

  /// Returns true if the nd-array is in canonical ordering
  bool is_canonical() const {
    return is_full() && has_canonical_stride();
  }

  /**
   * Increments a vector representing an N-D index.
   *
   * Assumes that the index is valid to begin with.
   * Returns true while we have not reached the end of the index. Returns false
   * if we will increment past the end of the array.
   */
  template <typename U>
  bool inline increment_index(std::vector<U>& idx) const {
    DASSERT_TRUE(idx.size() == m_shape.size());
    size_t i = 0;
    for (;i < idx.size(); ++i) {
      ++idx[i];
      if (idx[i] < m_shape[i]) break;
      // we hit counter limit we need to advance the next counter;
      idx[i] = 0;
    }

    return i != idx.size();
  }

  /**
   * Returns an ndarray ordered canonically.
   *
   * The canonical ordering is full (\ref is_full()) and the stride array
   * is non-descending.
   *
   * Raises an exception if the array is not valid.
   *
   * \note The performance of this algorithm can probably be improved.
   */
  ndarray<T> canonicalize() const {
    if (is_canonical()) return (*this);
    ASSERT_TRUE(is_valid());

    ndarray<T> ret;
    ret.m_start = 0;
    ret.m_shape = m_shape;
    ret.m_elem->resize(num_elem());
    ret.m_stride.resize(m_shape.size());

    // empty array
    if (ret.m_shape.size() == 0) {
      return ret;
    }

    // compute the stride
    ret.m_stride[0] = 1;
    for (size_t i = 1;i < ret.m_shape.size(); ++i) {
      ret.m_stride[i] = ret.m_stride[i - 1] * ret.m_shape[i - 1];
    }

    std::vector<size_t> idx(m_shape.size(), 0);
    size_t ctr = 0;
    do {
      (*ret.m_elem)[ctr] = (*this)[fast_index(idx)];
      ++ctr;
    } while(increment_index(idx));

    return ret;
  }

  /**
   * Returns a compacted ndarray.
   * 
   * A compacted NDArray has the same stride ordering as the original array,
   * but enforces that the array is full. This essentially means that the
   * elements array has the same order of elements, but skipped elements are
   * removed.
   *
   * Raises an exception if the array is not valid.
   *
   * \note The performance of this algorithm can probably be improved.
   */
  ndarray<T> compact() const {
    ASSERT_TRUE(is_valid());
    if (is_full()) return (*this);

    ndarray<T> ret;
    ret.m_start = 0;
    ret.m_shape = m_shape;
    ret.m_elem->resize(num_elem());
    ret.m_stride.resize(m_shape.size());

    // empty array
    if (ret.m_shape.size() == 0) {
      return ret;
    }

    std::vector<std::pair<size_t, size_t>> stride_ordering(m_stride.size());
    for (size_t i = 0;i < m_stride.size(); ++i) stride_ordering[i] = {m_stride[i], i};
    std::sort(stride_ordering.begin(), stride_ordering.end());

    // compute the stride
    ret.m_stride[stride_ordering[0].second] = 1;
    for (size_t i = 1;i < m_stride.size(); ++i) {
      ret.m_stride[stride_ordering[i].second] = 
          ret.m_stride[stride_ordering[i - 1].second] * ret.m_shape[stride_ordering[i - 1].second];
    }

    std::vector<size_t> idx(m_shape.size(), 0);
    do {
      (*ret.m_elem)[ret.fast_index(idx)] = (*this)[fast_index(idx)];
    } while(increment_index(idx));

    return ret;
  }

  /// serializer
  void save(oarchive& oarc) const {
    ASSERT_TRUE(is_valid());
    oarc << char(0);
    if (is_full()) {
      oarc << m_shape;
      oarc << m_stride;
      oarc << *m_elem;
    } else {
      ndarray<T> c = compact();
      ASSERT_TRUE(c.is_full());
      oarc << c.m_shape;
      oarc << c.m_stride;
      oarc << *(c.m_elem);
    }
  }

  /// deserializer
  void load(iarchive& iarc) {
    char c;
    iarc >> c;
    ASSERT_TRUE(c == 0);
    m_start = 0;
    iarc >> m_shape;
    iarc >> m_stride;
    m_elem = std::make_shared<container_type>();
    iarc >> *m_elem;
  }
 private:
  /**
   * Returns one past the last valid linear index of the array according to the
   * shape and stride information.
   */
  size_t last_index() const {
    if (m_shape.size() == 0) return 0;
    size_t last_idx = 0;
    for (size_t i = 0; i < m_shape.size(); ++i) {
      last_idx += (m_shape[i]-1) * m_stride[i];
    }
    return last_idx + 1;
  }

};

// pointer to ndarray is constrained to pointer size
// to enforce that it will always fit in a flexible_type.
static_assert(sizeof(ndarray<int>*) == sizeof(size_t));

} // flexible_type_impl
} // namespace turi
#endif
