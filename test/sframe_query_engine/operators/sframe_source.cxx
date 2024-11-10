#define BOOST_TEST_MODULE
#include <boost/test/unit_test.hpp>
#include <core/util/test_macros.hpp>
#include <core/storage/query_engine/execution/execution_node.hpp>
#include <core/storage/query_engine/operators/xframe_source.hpp>
#include <core/storage/xframe_data/xframe.hpp>
#include <core/storage/xframe_data/algorithm.hpp>

#include "check_node.hpp"

using namespace turi;
using namespace turi::query_eval;

struct xframe_source_test  {
 public:
  void test_empty_source() {
    xframe sf;
    std::vector<std::string> column_names;
    std::vector<flex_type_enum> column_types;
    sf.open_for_write(column_names, column_types);
    sf.close();
    auto node = make_node(sf);
    check_node(node, std::vector<std::vector<flexible_type>>());
  }

  void test_simple_xframe() {
    std::vector<std::vector<flexible_type>> expected {
      {0, "s0"}, {1, "s1"}, {2, "s2"}, {3, "s3"}, {4, "s4"}, {5, "s5"}
    };

    xframe sf;
    std::vector<std::string> column_names{"int", "string"};
    std::vector<flex_type_enum> column_types{flex_type_enum::INTEGER, flex_type_enum::STRING};
    sf.open_for_write(column_names, column_types);
    turi::copy(expected.begin(), expected.end(), sf);
    sf.close();
    auto node = make_node(sf);
    check_node(node, expected);
  }

  std::shared_ptr<execution_node> make_node(xframe source) {
    auto node = std::make_shared<execution_node>(std::make_shared<op_xframe_source>(source));
    return node;
  }

};

BOOST_FIXTURE_TEST_SUITE(_xframe_source_test, xframe_source_test)
BOOST_AUTO_TEST_CASE(test_empty_source) {
  xframe_source_test::test_empty_source();
}
BOOST_AUTO_TEST_CASE(test_simple_xframe) {
  xframe_source_test::test_simple_xframe();
}
BOOST_AUTO_TEST_SUITE_END()
