// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Model/Model.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Context::Context(Connection& connection, std::shared_ptr<FilesystemAdapter> filesystem, uint16_t bootNr)
        : connection(connection), model{bootNr}, reqQueue{operationRegistry, &model, filesystem} {

    preBootQueue = std::unique_ptr<RequestQueue>(new RequestQueue(operationRegistry, &model, nullptr)); //pre boot queue doesn't need persistency
    preBootQueue->setConnection(connection);
}

Context::~Context() {

}

void Context::loop() {
    connection.loop();

    if (preBootQueue) {
        preBootQueue->loop(connection);
    } else {
        reqQueue.loop(connection);
    }

    model.loop();
}

void Context::activatePostBootCommunication() {
    //switch from pre boot connection to normal connetion
    reqQueue.setConnection(connection);
    preBootQueue.reset();
}

void Context::initiateRequest(std::unique_ptr<Request> op) {
    if (op) {
        reqQueue.sendRequest(std::move(op));
    }
}

void Context::initiatePreBootOperation(std::unique_ptr<Request> op) {
    if (op) {
        if (preBootQueue) {
            preBootQueue->sendRequest(std::move(op));
        } else {
            //not in pre boot mode anymore - initiate normally
            initiateRequest(std::move(op));
        }
    }
}

Model& Context::getModel() {
    return model;
}

OperationRegistry& Context::getOperationRegistry() {
    return operationRegistry;
}
