#include <iostream>
#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Core/OcppSocket.h>
#include "./catch2/catch.hpp"

void cpp_console_out(const char *msg) {
    std::cout << msg;
}

TEST_CASE( "Do some loops", "[multi-file:2]" ) {

    ArduinoOcpp::OcppEchoSocket echoSocket;

    std::cout << "[main] Enter compilation test\n";

    std::cout << "[main] set custom console\n";
    ao_set_console_out(cpp_console_out);
    
    OCPP_initialize(echoSocket);
    std::cout << "[main] AO initialized\n---------------------\n";
    
    std::cout << "[main] enqueue first operation\n";
    bootNotification("Test charger", "Test vendor");
    
    std::cout << "[main] handle WebSocket traffic and run OCPP tasks\n"; 
    OCPP_loop();
    OCPP_loop();
    OCPP_loop();

    std::cout << "-------------------------\n[main] test routines done\n";
    OCPP_deinitialize();

    std::cout << "[main] exit\n";
}