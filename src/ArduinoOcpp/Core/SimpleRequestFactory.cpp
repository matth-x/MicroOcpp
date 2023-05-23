// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

std::unique_ptr<Request> makeRequest(Operation *msg){
    if (msg == nullptr) {
        return nullptr;
    }
    auto operation = makeRequest();
    operation->setOperation(std::unique_ptr<Operation>(msg));
    return operation;
}

std::unique_ptr<Request> makeRequest(){
    auto result = std::unique_ptr<Request>(new Request());
    return result;
}

} //end namespace ArduinoOcpp
