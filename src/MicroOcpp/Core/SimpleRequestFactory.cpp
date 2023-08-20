// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

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

} //end namespace MicroOcpp
