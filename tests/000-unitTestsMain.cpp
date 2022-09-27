//000- prefix is for the main-file to always be on top in folder view

// Powershell to compile tests and run them:
// g++ -Wall test/unitTests/000-unitTestsMain.cpp -o ./test/compiledTests ; ./test/compiledTests ; Remove-Item -Path ./test/compiledTests.exe

//use the main function provided by catch2
#define CATCH_CONFIG_MAIN
//include the catch2 library
#include "../catch2/catch.hpp"

//include the unit tests
#include "./exampleCheckNumbers.cpp"