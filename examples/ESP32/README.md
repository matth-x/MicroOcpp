#### ESP32 port progress

The ESP32 port is almost finished. Only the persistent storage is still to be worked on. Please clone the branch `develop` and set the build flag `AO_DEACTIVATE_FLASH`. Alternatively, you can search for the `#ifndef AO_DEACTIVATE_FLASH` statements in `Configuration.cpp` and `SmartChargingService.cpp` and delete everything inside their scope.
