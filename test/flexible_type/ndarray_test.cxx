#define BOOST_TEST_MODULE
#include <boost/test/unit_test.hpp>
#include <util/test_macros.hpp>
#include <vector>
#include <iostream>
#include <typeinfo>       // operator typeid


#include <flexible_type/ndarray.hpp>

using namespace turi;
using namespace flexible_type_impl;
namespace tt = boost::test_tools;

template <typename T>
void nd_assert_equal(const ndarray<T>& a, const ndarray<T>& b) {
  BOOST_CHECK(a.is_valid());
  BOOST_CHECK(b.is_valid());

  BOOST_TEST(a.num_elem() == b.num_elem());
  BOOST_TEST(a.shape() == b.shape());
  if (a.shape().size() > 0) {
    std::vector<size_t> idx(a.shape().size(), 0);
    do {
      double aval = a.at(a.index(idx));
      double bval = b.at(b.index(idx));
      BOOST_TEST(aval == bval);
    } while(a.increment_index(idx));
  }
}

template <typename T>
void test_save_load(const ndarray<T>& a) {
  oarchive oarc;
  oarc << a;
  iarchive iarc(oarc.buf, oarc.off);
  ndarray<T> b;
  iarc >> b;
  nd_assert_equal(a, b);
  BOOST_TEST(b.is_valid() == true);
  BOOST_TEST(b.is_full() == true);
}


BOOST_AUTO_TEST_CASE(test_empty) {
 ndarray<int> i;
 BOOST_TEST(i.is_valid());
 BOOST_TEST(i.is_full());
 test_save_load(i);
}

BOOST_AUTO_TEST_CASE(test_canonical) {
 ndarray<int> fortran({0,1,2,3,4,5,6,7,8,9},
                      {2,5},
                      {5,1});
 BOOST_TEST(fortran.is_valid());
 BOOST_TEST(fortran.is_full());
 ndarray<int> c = fortran.canonicalize();

 std::vector<size_t> desired_stride{1,2};
 std::vector<size_t> desired_shape{2,5};
 std::vector<size_t> desired_elements{0,5,1,6,2,7,3,8,4,9};
 BOOST_TEST(c.stride() == desired_stride, tt::per_element());
 BOOST_TEST(c.shape() == desired_shape, tt::per_element());
 BOOST_TEST(c.elements() == desired_elements, tt::per_element());
 BOOST_TEST(c.is_valid() == true);
 BOOST_TEST(c.is_full() == true);
 BOOST_TEST(c.is_canonical() == true);
 nd_assert_equal(c, fortran);

 test_save_load(fortran);
 test_save_load(c);
}


BOOST_AUTO_TEST_CASE(test_subarray) {
 ndarray<int> subarray({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
                      {2,2},
                      {1,4}); // top left corner of array
 BOOST_TEST(subarray.is_valid());
 BOOST_TEST(subarray.is_full() == false);
 BOOST_TEST(subarray.is_canonical() == false);
 ndarray<int> c = subarray.canonicalize();

 std::vector<size_t> desired_elements{0,1,4,5};
 std::vector<size_t> desired_shape{2,2};
 std::vector<size_t> desired_stride{1,2};
 BOOST_TEST(c.elements() == desired_elements, tt::per_element());
 BOOST_TEST(c.shape() == desired_shape, tt::per_element());
 BOOST_TEST(c.stride() == desired_stride, tt::per_element());
 BOOST_TEST(c.is_valid() == true);
 BOOST_TEST(c.is_full() == true);
 BOOST_TEST(c.is_canonical() == true);
 nd_assert_equal(c, subarray);

 test_save_load(subarray);
 test_save_load(c);
}

BOOST_AUTO_TEST_CASE(test_invalid) {
 ndarray<int> a({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
                {2,3},
                {2,8});
 BOOST_TEST(a.is_valid() == false);

 ndarray<int> b({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
                {3,8},
                {1,1});
 BOOST_TEST(b.is_valid() == false);
}


BOOST_AUTO_TEST_CASE(test_bad_shapes) {
 ndarray<int> bad_shape({0,1,2,3,4,5,6,7,8,9},
                        {0,0},
                        {1,5});
 BOOST_TEST(bad_shape.is_valid() == false);
 ndarray<int> bad_shape2({0,1,2,3,4,5,6,7,8,9},
                        {1,0},
                        {1,5});
 BOOST_TEST(bad_shape2.is_valid() == false);
}

BOOST_AUTO_TEST_CASE(test_odd_stride) {
 // a stride of 0 is technically valid
 // though a little odd
 {
   ndarray<int> zero_stride({0,1,2,3,4,5,6,7,8,9},
                           {2,5},
                           {1,0});
   BOOST_TEST(zero_stride.is_valid() == true);
   BOOST_TEST(zero_stride.is_full() == false);
   BOOST_TEST(zero_stride.is_canonical() == false);
   ndarray<int> zero_stride_c = zero_stride.canonicalize();
   std::vector<size_t> desired_elements{0,1,0,1,0,1,0,1,0,1};
   std::vector<size_t> desired_shape{2,5};
   std::vector<size_t> desired_stride{1,2};
   BOOST_TEST(zero_stride_c.elements() == desired_elements, tt::per_element());
   BOOST_TEST(zero_stride_c.shape() == desired_shape, tt::per_element());
   BOOST_TEST(zero_stride_c.stride() == desired_stride, tt::per_element());
   test_save_load(zero_stride);
 }

 // test dim 1
 {
   ndarray<int> dim1({0,1,2},
                     {1,1,3},
                     {0,0,1}); 
   BOOST_TEST(dim1.is_valid() == true);
   BOOST_TEST(dim1.is_full() == true);
   BOOST_TEST(dim1.is_canonical() == false);
   ndarray<int> dim1_c = dim1.canonicalize();
   std::vector<size_t> desired_elements{0,1,2};
   std::vector<size_t> desired_shape{1,1,3};
   std::vector<size_t> desired_stride{1,1,1};
   BOOST_TEST(dim1_c.elements() == desired_elements, tt::per_element());
   BOOST_TEST(dim1_c.shape() == desired_shape, tt::per_element());
   BOOST_TEST(dim1_c.stride() == desired_stride, tt::per_element());
   test_save_load(dim1);
 }
 // another test dim 1
 {
   ndarray<int> dim1({0,1,2,3,4,5},
                     {3,1,1,2},
                     {1,0,0,3}); 
   BOOST_TEST(dim1.is_valid() == true);
   BOOST_TEST(dim1.is_full() == true);
   BOOST_TEST(dim1.is_canonical() == false);
   ndarray<int> dim1_c = dim1.canonicalize();
   std::vector<size_t> desired_elements{0,1,2,3,4,5};
   std::vector<size_t> desired_shape{3,1,1,2};
   std::vector<size_t> desired_stride{1,3,3,3};
   BOOST_TEST(dim1_c.elements() == desired_elements, tt::per_element());
   BOOST_TEST(dim1_c.shape() == desired_shape, tt::per_element());
   BOOST_TEST(dim1_c.stride() == desired_stride, tt::per_element());
   test_save_load(dim1);
   test_save_load(dim1_c);
 }
}
