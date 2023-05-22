// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHARGECONTROLSERVICE_H
#define CHARGECONTROLSERVICE_H

#include <ArduinoOcpp/Core/FilesystemAdapter.h>

namespace ArduinoOcpp {

class OcppEngine;

class ChargeControlService {
private:
    OcppEngine& context;
public:
    ChargeControlService(OcppEngine& context, unsigned int numConnectors, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();
};

} //end namespace ArduinoOcpp

#endif
