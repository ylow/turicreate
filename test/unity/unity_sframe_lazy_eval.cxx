/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#define BOOST_TEST_MODULE
#include <boost/test/unit_test.hpp>
#include <core/util/test_macros.hpp>
#include <iostream>
#include <core/storage/fileio/temp_files.hpp>
#include <core/storage/xframe_interface/unity_xframe.hpp>
#include <core/storage/xframe_data/dataframe.hpp>
#include <core/storage/xframe_data/algorithm.hpp>
using namespace turi;

struct unity_xframe_lazy_eval_test {
  const size_t ARRAY_SIZE = 20000;

  dataframe_t _create_test_dataframe() {
    dataframe_t testdf;
    std::vector<flexible_type> a;
    std::vector<flexible_type> b;
    std::vector<flexible_type> c;
    // create a simple dataframe of 3 columns of 3 types
    for (size_t i = 0;i < ARRAY_SIZE; ++i) {
      a.push_back(i);
      b.push_back((float)i);
      c.push_back(std::to_string(i));
    }
    testdf.set_column("a", a, flex_type_enum::INTEGER);
    testdf.set_column("b", b, flex_type_enum::FLOAT);
    testdf.set_column("c", c, flex_type_enum::STRING);
    return testdf;
  }
 public:
  unity_xframe_lazy_eval_test() {
  }

  /**
   * initial sarray construction is materialized
   **/
  void test_basic() {
    dataframe_t testdf = _create_test_dataframe();
    // create a unity_xframe
    unity_xframe xframe;
    xframe.construct_from_dataframe(testdf);

    assert_materialized(xframe, true);
  }

  /**
   * Test logical filter
  **/
  void test_logical_filter() {
    dataframe_t testdf = _create_test_dataframe();
    unity_xframe xframe;
    xframe.construct_from_dataframe(testdf);

    // index array
    std::shared_ptr<unity_sarray_base> index_array(new unity_sarray);
    std::vector<flexible_type> vec(ARRAY_SIZE);
    for(size_t i = 0; i < ARRAY_SIZE; i++) {
      vec[i] = i % 2 == 0 ? 1 : 0;
    }
    index_array->construct_from_vector(vec, flex_type_enum::INTEGER);

    // logical filter
    auto new_sf = xframe.logical_filter(index_array);
    assert_materialized(new_sf, false);
  }

  /**
   * Test pipeline xframe and sarray without filter
  **/
  void test_pipe_line() {
    dataframe_t testdf = _create_test_dataframe();
    unity_xframe xframe;
    xframe.construct_from_dataframe(testdf);

    std::shared_ptr<unity_sarray_base> col_a = xframe.select_column(std::string("a"));
    std::shared_ptr<unity_sarray_base> col_b = xframe.select_column(std::string("b"));

    std::shared_ptr<unity_sarray_base> col_a_plus_b = col_a->vector_operator(col_b, "+");
    assert_materialized(col_a_plus_b, false);

    // construct new xframe
    unity_xframe new_xframe;

    new_xframe.add_column(col_b, std::string("a"));
    new_xframe.add_column(col_a_plus_b, std::string("ab"));
    assert_materialized(col_a_plus_b, false);
    assert_materialized(new_xframe, false);

    new_xframe.head(2);
    assert_materialized(new_xframe, false);
    assert_materialized(col_a_plus_b, false);

    new_xframe.tail(2);

  }

  /**
   * Test pipeline xframe and sarray with filter
   * filter will materialize some part of the tree that needs size
  **/
  void test_pipe_line_with_filter() {
    dataframe_t testdf = _create_test_dataframe();
    unity_xframe xframe;
    xframe.construct_from_dataframe(testdf);

    std::shared_ptr<unity_sarray_base> col_a = xframe.select_column(std::string("a"));
    std::shared_ptr<unity_sarray_base> col_b = xframe.select_column(std::string("b"));

    std::shared_ptr<unity_sarray_base> filter_a = col_a->logical_filter(col_b);
    assert_materialized(filter_a, false);

    // get size will cause materialization
    TS_ASSERT_EQUALS(filter_a->size(), ARRAY_SIZE - 1);
  }

  /**
   * Test sharing sarray object among different user
   * sf['one'] = sf['another'] = sa
   * sf[sf['a']]
  **/
  void test_share_operator() {
    dataframe_t testdf = _create_test_dataframe();
    unity_xframe xframe;
    xframe.construct_from_dataframe(testdf);

    std::shared_ptr<unity_sarray_base> col_a = xframe.select_column(std::string("a"));

    unity_xframe new_xframe;
    new_xframe.add_column(col_a, std::string("one"));
    new_xframe.add_column(col_a, std::string("another"));

    std::shared_ptr<unity_xframe_base> filtered_frame = new_xframe.logical_filter(col_a);
    filtered_frame->head(10);
  }

  void test_materialize_xframe() {
    unity_xframe xframe;

    // construct two columns in two different ways
    std::shared_ptr<unity_sarray_base> sa1(new unity_sarray);
    std::shared_ptr<unity_sarray_base> sa2(new unity_sarray);
    std::vector<flexible_type> vec1(100), vec2(100);
    for(size_t i = 0; i < 100; i++) {
      vec1[i] = i;
      vec2[i] = std::to_string(i);
    }

    sa1->construct_from_vector(vec1, flex_type_enum::INTEGER);
    sa2->construct_from_vector(vec2, flex_type_enum::STRING);

    // sa3 is lazily materialized
    auto sa3 = sa1->left_scalar_operator(1, "+");

    // cosntruct xframe
    std::shared_ptr<unity_xframe_base> sf(new unity_xframe);
    sf->add_column(sa2, "a");
    sf->add_column(sa3, "b");
    TS_ASSERT(sa1->is_materialized());
    TS_ASSERT(!sa3->is_materialized());
    TS_ASSERT(!sf->is_materialized());

    sf->materialize();
    TS_ASSERT(sf->is_materialized());
  }

  void assert_materialized(std::shared_ptr<unity_xframe_base> xframe_ptr, bool is_materialized) {
    TS_ASSERT_EQUALS(xframe_ptr->is_materialized(), is_materialized);
  }

  void assert_materialized(std::shared_ptr<unity_sarray_base> sarray_ptr, bool is_materialized) {
    TS_ASSERT_EQUALS(sarray_ptr->is_materialized(), is_materialized);
  }

  void assert_materialized(unity_xframe& xframe, bool is_materialized) {
    TS_ASSERT_EQUALS(xframe.is_materialized(), is_materialized);
  }

  void assert_materialized(unity_sarray& sarray, bool is_materialized) {
    TS_ASSERT_EQUALS(sarray.is_materialized(), is_materialized);
  }
};

BOOST_FIXTURE_TEST_SUITE(_unity_xframe_lazy_eval_test, unity_xframe_lazy_eval_test)
BOOST_AUTO_TEST_CASE(test_basic) {
  unity_xframe_lazy_eval_test::test_basic();
}
BOOST_AUTO_TEST_CASE(test_logical_filter) {
  unity_xframe_lazy_eval_test::test_logical_filter();
}
BOOST_AUTO_TEST_CASE(test_pipe_line) {
  unity_xframe_lazy_eval_test::test_pipe_line();
}
BOOST_AUTO_TEST_CASE(test_pipe_line_with_filter) {
  unity_xframe_lazy_eval_test::test_pipe_line_with_filter();
}
BOOST_AUTO_TEST_CASE(test_share_operator) {
  unity_xframe_lazy_eval_test::test_share_operator();
}
BOOST_AUTO_TEST_CASE(test_materialize_xframe) {
  unity_xframe_lazy_eval_test::test_materialize_xframe();
}
BOOST_AUTO_TEST_SUITE_END()
