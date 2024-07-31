// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

std::unique_ptr<Request> makeRequest(std::unique_ptr<Operation> operation){
    if (operation == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<Request>(new Request(std::move(operation)));
}

std::unique_ptr<Request> makeRequest(Operation *operation) {
    return makeRequest(std::unique_ptr<Operation>(operation));
}

} //end namespace MicroOcpp
