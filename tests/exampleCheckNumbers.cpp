#include "../catch2/catch.hpp"

int Factorial( int number ) {
    return number <= 1 ? number : Factorial( number - 1 ) * number;  // fail
    // return number <= 1 ? 1      : Factorial( number - 1 ) * number;  // pass
}

TEST_CASE( "2: Factorials of 1 and higher are computed (pass)", "[multi-file:2]" ) {
    REQUIRE( Factorial(1) == 1 );
    REQUIRE( Factorial(2) == 2 );
    REQUIRE( Factorial(3) == 6 );
    REQUIRE( Factorial(10) == 3628800 );
}