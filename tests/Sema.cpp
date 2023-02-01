#include "UnitTestParser.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Expr", "[Sema]") {
  UnitTestParser<Expr> parser;
  SECTION("example") { REQUIRE_THROWS(parser.parse(0, "a || b =c")); }
}