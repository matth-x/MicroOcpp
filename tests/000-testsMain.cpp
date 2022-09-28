//000- prefix is for the main-file to always be on top in folder view
#include "./000-testsMain.h"
//use the main function provided by catch2
#define CATCH_CONFIG_MAIN
//include the catch2 library
#include "./catch2/catch.hpp"

void cpp_console_out(const char *msg) {
    std::cout << msg;
}

//include the tests
#include "./initializeDeinitialize.h"