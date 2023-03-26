// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {

std::unique_ptr<OcppOperation> makeOcppOperation(OcppMessage *msg){
    if (msg == nullptr) {
        return nullptr;
    }
    auto operation = makeOcppOperation();
    operation->setOcppMessage(std::unique_ptr<OcppMessage>(msg));
    return operation;
}

std::unique_ptr<OcppOperation> makeOcppOperation(){
    auto result = std::unique_ptr<OcppOperation>(new OcppOperation());
    return result;
}

} //end namespace ArduinoOcpp
