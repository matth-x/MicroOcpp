# ESP-IDF integration example

To run ArduinoOcpp on the ESP-IDF platform, please take this example as the starting point. It is widely based on the [Wi-Fi Station Example](https://github.com/espressif/esp-idf/tree/release/v4.4/examples/wifi/getting_started/station) of Espressif. This example works with the ESP-IDF version `4.4`. For a general guide how to setup and use the ESP-IDF, please refer to the documentation of Espressif.

## Setup guide

### Dependencies

Please clone the following repositories into the respective components-directories:

- [ArduinoOcpp](https://github.com/matth-x/ArduinoOcpp) into `components/ArduinoOcpp`
- [Mongoose (ESP-IDF integration)](https://github.com/cesanta/mongoose-esp-idf) into `components/mongoose`
- [Mongoose adapter for ArduinoOcpp](https://github.com/matth-x/AOcppMongoose) into `components/ArduinoOcppMongoose`
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) into `components/ArduinoJson`

Alternatively, ArduinoJson can go into into an include folder as it is a header-only library:

- [ArduinoJson header file](https://github.com/bblanchon/ArduinoJson/releases/download/v6.19.4/ArduinoJson-v6.19.4.h), renamed and moved to `include/ArduinoJson.h`. Any other location will work too as long as the header file `ArduinoJson.h` is on the include-path and can be found by the statement `#include <ArduinoJson.h>`

For setup, the following commands could come handy (change to the root directory of the ESP-IDF project first):

```
rm components/mongoose/.gitkeep
rm components/ArduinoOcpp/.gitkeep
rm components/AOcppMongoose/.gitkeep
git clone https://github.com/matth-x/ArduinoOcpp components/ArduinoOcpp
git clone --recurse-submodules https://github.com/cesanta/mongoose-esp-idf.git components/mongoose
git clone https://github.com/matth-x/AOcppMongoose components/AOcppMongoose
git clone https://github.com/bblanchon/ArduinoJson
```

The setup is done if the following include statements work:

```cpp
#include <ArduinoOcpp.h>
#include <mongoose.h>
#include <ArduinoOcppMongooseClient.h>
#include <ArduinoJson.h>
```

### Configure the project

Open the project configuration menu (`idf.py menuconfig`). 

In the `Example Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.
    * Set `OCPP backend URL`. (e.g. `ws://ocpp.example.com/steve/websocket/CentralSystemService`)
    * Set `ChargeBoxId`. (e.g. `my-charger` - last part of the WebSocket URL
    * Set `AuthorizationKey`, or leave empty if not necessary

Optional: If you need, change the other options according to your requirements.
