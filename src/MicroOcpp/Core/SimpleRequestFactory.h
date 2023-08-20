// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_SIMPLEREQUESTFACTORY_H
#define AO_SIMPLEREQUESTFACTORY_H

#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/Request.h>
#include <memory>
#include <functional>

namespace ArduinoOcpp {

class Operation;

std::unique_ptr<Request> makeRequest();

std::unique_ptr<Request> makeRequest(std::unique_ptr<Operation> op);
std::unique_ptr<Request> makeRequest(Operation *op); //takes ownership of op

} //end namespace ArduinoOcpp
#endif
