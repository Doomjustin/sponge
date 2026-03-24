#include <sponge/range.h>

#include <catch2/catch_test_macros.hpp>

using spg::Boundary;
using spg::Range;

TEST_CASE("range contains respects boundaries", "[range]") {
    Range<double, Boundary::Inclusive, Boundary::Inclusive> closed{ 1.0, 3.0 };
    REQUIRE(closed.contains(1.0));
    REQUIRE(closed.contains(3.0));

    Range<double, Boundary::Exclusive, Boundary::Exclusive> open{ 1.0, 3.0 };
    REQUIRE_FALSE(open.contains(1.0));
    REQUIRE_FALSE(open.contains(3.0));
    REQUIRE(open.contains(2.0));
}

TEST_CASE("range overlaps for separated and interior cases", "[range]") {
    Range<double, Boundary::Inclusive, Boundary::Inclusive> a{ 1.0, 3.0 };
    Range<double, Boundary::Inclusive, Boundary::Inclusive> b{ 2.0, 4.0 };
    Range<double, Boundary::Inclusive, Boundary::Inclusive> c{ 4.0, 6.0 };

    REQUIRE(a.overlaps(b));
    REQUIRE_FALSE(a.overlaps(c));
}

TEST_CASE("range overlaps touching boundary honors both sides inclusiveness", "[range]") {
    Range<double, Boundary::Inclusive, Boundary::Inclusive> closed_left{ 1.0, 2.0 };
    Range<double, Boundary::Inclusive, Boundary::Inclusive> closed_right{ 2.0, 3.0 };
    Range<double, Boundary::Inclusive, Boundary::Exclusive> right_open{ 1.0, 2.0 };
    Range<double, Boundary::Exclusive, Boundary::Inclusive> left_open{ 2.0, 3.0 };

    REQUIRE(closed_left.overlaps(closed_right));
    REQUIRE_FALSE(right_open.overlaps(closed_right));
    REQUIRE_FALSE(closed_left.overlaps(left_open));
    REQUIRE_FALSE(right_open.overlaps(left_open));
}

TEST_CASE("range overlaps works across mixed boundary templates", "[range]") {
    Range<double, Boundary::Exclusive, Boundary::Exclusive> open{ 0.0, 10.0 };
    Range<double, Boundary::Inclusive, Boundary::Inclusive> closed_inside{ 2.0, 8.0 };
    Range<double, Boundary::Inclusive, Boundary::Exclusive> right_open_outside{ 10.0, 12.0 };

    REQUIRE(open.overlaps(closed_inside));
    REQUIRE_FALSE(open.overlaps(right_open_outside));
}
