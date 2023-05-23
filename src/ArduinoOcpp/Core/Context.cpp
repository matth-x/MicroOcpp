// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Core/Request.h>
#include <ArduinoOcpp/Core/Connection.h>
#include <ArduinoOcpp/Core/Model.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

Context *ArduinoOcpp::defaultContext = nullptr;

Context::Context(Connection& connection, const Clock& system_clock, std::shared_ptr<FilesystemAdapter> filesystem)
        : connection(connection), model{std::make_shared<Model>(system_clock)}, reqQueue{operationRegistry, model, filesystem} {
    defaultContext = this;

    preBootQueue = std::unique_ptr<RequestQueue>(new RequestQueue(operationRegistry, model, nullptr)); //pre boot queue doesn't need persistency
    preBootQueue->setConnection(connection);
}

Context::~Context() {
    defaultContext = nullptr;
}

void Context::loop() {
    connection.loop();

    if (preBootQueue) {
        preBootQueue->loop(connection);
    } else {
        reqQueue.loop(connection);
    }

    model->loop();
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
    return *model;
}

OperationRegistry& Context::getOperationRegistry() {
    return operationRegistry;
}
