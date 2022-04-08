#include "ArduinoOcpp_c.h"
#include "ArduinoOcpp.h"

#ifdef __cplusplus
extern "C" {
#endif

void ao_initialize() {
    //OCPP_initialize("echo.websocket.events", 80, "ws://echo.websocket.events/");
}

void ao_loop() {
    OCPP_loop();
}

void ao_bootNotification() {
    bootNotification("model", "vendor");
}

#ifdef __cplusplus
}
#endif
