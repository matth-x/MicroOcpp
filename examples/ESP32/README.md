#### ESP32 port progress

The ESP32 port is almost finished. Only the persistent storage is still to be worked on. Please clone the branch `develop` and set the build flag `AO_DEACTIVATE_FLASH`. Alternatively, you can search for the `#ifndef AO_DEACTIVATE_FLASH` statements in `Configuration.cpp` and `SmartChargingService.cpp` and delete everything inside their scope.

#### How to test the ESP32 port on a Wemos D1 Mini or other esp-wrover-kit compatible board

First copy the [main.ino](main.ino) from the example/ESP32 directory to the [src](../../src) directory and change the [WiFi credentials](main.ino#L10-L11) according to your environment. 

Then you can just compile and run the code with PlatformIO.

```
pio run --target upload --target monitor --environment esp32-development-board
```

In this simple example the ESP32-ocpp-box will just talk to itself via the echo.websocket.org websocket service and you can watch that.

Now you can change the [websocket server](main.ino#L13-L15) and put your own instance of [SteVe](https://github.com/RWTH-i5-IDSG/steve) in there as well.
