# Simple EVSE OCPP communication controller

With this sketch you can use an ESP8266/ESP32 as the central control unit of an EVSE. It controls the HW peripherals of the EVSE and connects the charge point to an OCPP server. When booting up the EVSE, it opens a maintenance web dashboard for setting the Wi-Fi and OCPP credentials (thanks to tzapu's WiFiManager).

## Architecture

The communication controller can connect to an SAE&nbsp;J1772 driver using the GPIO pins of the ESP. Furthermore, it can show the online status and charge point availability status via status LEDs. Displays, RFID readers or energy meters are not supported, though for your project you can integrate further HW components of course.

You can find the interface descriptions in `main.ino`. Please be aware that although this program modulates the maximum charge rate with PWM analogous to the SAE&nbsp;J1772 standard, the PWM signal does not reflect the EVSE state. The latter is encoded via the other GPIO pins.

## Usage

- Create an empty project in the PlatformIO IDE.
- Copy the `platformio.ini` from ArduinoOcpp's root directory to your source directory. Add `tzapu/WiFiManager@0.16.0` to the libdeps of the `platformio.ini`.
- Copy the `main.ino` from this example folder into your source directory. Adapt the pinout settings if needed.
- Finished. You should be able to compile and upload the sketch onto your ESP.

When booting, the ESP opens up the Wi-Fi configuration portal. Please connect your PC to the network with the SSID `EVSE_Maintenance_Portal` within the first 30s of the boot routine of the ESP (the portal has a timeout). The passphrase is `myEvseController`.

## Standalone mode

If you just want to check out ArduinoOcpp you can run this sketch on an ESP without any peripherals. The pinout of the ESP8266-NodeMCU&nbsp;board is already fully configured in `main.ino` so that board is the most convenient to start testing.
