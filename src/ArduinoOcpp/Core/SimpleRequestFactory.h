// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SIMPLEOCPPOPERATIONFACTORY_H
#define SIMPLEOCPPOPERATIONFACTORY_H

#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/Request.h>
#include <memory>
#include <functional>

namespace ArduinoOcpp {

class Operation;

std::unique_ptr<Request> makeRequest();

std::unique_ptr<Request> makeRequest(Operation *msg);

} //end namespace ArduinoOcpp
#endif
