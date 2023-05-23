// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <memory>
#include <functional>

namespace ArduinoOcpp {

class OcppMessage;

std::unique_ptr<OcppOperation> makeOcppOperation();

std::unique_ptr<OcppOperation> makeOcppOperation(OcppMessage *msg);

} //end namespace ArduinoOcpp
#endif
