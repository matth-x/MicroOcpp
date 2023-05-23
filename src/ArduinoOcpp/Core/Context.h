// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPENGINE_H
#define OCPPENGINE_H

#include <ArduinoOcpp/Core/RequestQueue.h>
#include <ArduinoOcpp/Core/OperationRegistry.h>
#include <ArduinoOcpp/Core/Time.h>
#include <memory>

namespace ArduinoOcpp {

class Connection;
class OperationRegistry;
class Model;
class FilesystemAdapter;

class Context {
private:
    Connection& connection;
    OperationRegistry operationRegistry;
    std::shared_ptr<Model> model;
    RequestQueue reqQueue;

    std::unique_ptr<RequestQueue> preBootQueue;

public:
    Context(Connection& connection, const Clock& system_clock, std::shared_ptr<FilesystemAdapter> filesystem);
    ~Context();

    void loop();

    void activatePostBootCommunication();

    void initiateRequest(std::unique_ptr<Request> op);
    
    //for BootNotification and TriggerMessage: initiate operations before the first BootNotification was accepted (pre-boot mode)
    void initiatePreBootOperation(std::unique_ptr<Request> op);

    Model& getModel();

    OperationRegistry& getOperationRegistry();
};

extern Context *defaultContext;

} //end namespace ArduinoOcpp

#endif
