## Includes

[ArduinoJson](https://github.com/bblanchon/ArduinoJson) is the JSON (de)serializer of ArduinoOcpp and a dependency. It is a header-only library and can go into the `include` folder. Any other location will work too as long as the header file `ArduinoJson.h` is on the include-path, i.e. the following `#include` works:

```cpp
#include <ArduinoJson.h>
```

Version `6.19.4` is required. You can download [this header file](https://github.com/bblanchon/ArduinoJson/releases/download/v6.19.4/ArduinoJson-v6.19.4.h), rename it into `ArduinoJson.h` and move it here.
