// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

std::unique_ptr<Request> makeRequest(std::unique_ptr<Operation> operation){
    if (operation == nullptr) {
        return nullptr;
    }
    auto request = makeRequest();
    request->setOperation(std::move(operation));
    return request;
}

std::unique_ptr<Request> makeRequest(Operation *operation) {
    return makeRequest(std::unique_ptr<Operation>(operation));
}

std::unique_ptr<Request> makeRequest(){
    auto result = std::unique_ptr<Request>(new Request());
    return result;
}

} //end namespace ArduinoOcpp
