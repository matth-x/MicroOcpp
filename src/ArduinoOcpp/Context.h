// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_CONTEXT_H
#define AO_CONTEXT_H

#include <memory>

#include <ArduinoOcpp/Core/OperationRegistry.h>
#include <ArduinoOcpp/Core/RequestQueue.h>
#include <ArduinoOcpp/Model/Model.h>

namespace ArduinoOcpp {

class Connection;
class FilesystemAdapter;

class Context {
private:
    Connection& connection;
    OperationRegistry operationRegistry;
    Model model;
    RequestQueue reqQueue;

    std::unique_ptr<RequestQueue> preBootQueue;

public:
    Context(Connection& connection, std::shared_ptr<FilesystemAdapter> filesystem, uint16_t bootNr);
    ~Context();

    void loop();

    void activatePostBootCommunication();

    void initiateRequest(std::unique_ptr<Request> op);
    
    //for BootNotification and TriggerMessage: initiate operations before the first BootNotification was accepted (pre-boot mode)
    void initiatePreBootOperation(std::unique_ptr<Request> op);

    Model& getModel();

    OperationRegistry& getOperationRegistry();
};

} //end namespace ArduinoOcpp

#endif
