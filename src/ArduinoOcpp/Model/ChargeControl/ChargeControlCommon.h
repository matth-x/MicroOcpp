// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHARGECONTROLCOMMON_H
#define CHARGECONTROLCOMMON_H

#include <ArduinoOcpp/Core/FilesystemAdapter.h>

namespace ArduinoOcpp {

class Context;

class ChargeControlCommon {
private:
    Context& context;
public:
    ChargeControlCommon(Context& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();
};

} //end namespace ArduinoOcpp

#endif
