// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHARGECONTROLCOMMON_H
#define CHARGECONTROLCOMMON_H

#include <ArduinoOcpp/Core/FilesystemAdapter.h>

namespace ArduinoOcpp {

class OcppEngine;

class ChargeControlCommon {
private:
    OcppEngine& context;
public:
    ChargeControlCommon(OcppEngine& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();
};

} //end namespace ArduinoOcpp

#endif
