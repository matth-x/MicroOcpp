// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPENGINE_H
#define OCPPENGINE_H

#include <ArduinoOcpp/Core/OcppConnection.h>
#include <ArduinoOcpp/OperationDeserializer.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <memory>

namespace ArduinoOcpp {

class OcppSocket;
class OperationDeserializer;
class OcppModel;
class FilesystemAdapter;

class OcppEngine {
private:
    OcppSocket& oSock;
    OperationDeserializer operationDeserializer;
    std::shared_ptr<OcppModel> oModel;
    OcppConnection oConn;

    std::unique_ptr<OcppConnection> preBootConn;

public:
    OcppEngine(OcppSocket& ocppSocket, const OcppClock& system_clock, std::shared_ptr<FilesystemAdapter> filesystem);
    ~OcppEngine();

    void loop();

    void activatePostBootCommunication();

    void initiateOperation(std::unique_ptr<OcppOperation> op);
    
    //for BootNotification and TriggerMessage: initiate operations before the first BootNotification was accepted (pre-boot mode)
    void initiatePreBootOperation(std::unique_ptr<OcppOperation> op);

    OcppModel& getOcppModel();

    OperationDeserializer& getOperationDeserializer();
};

extern OcppEngine *defaultOcppEngine;

} //end namespace ArduinoOcpp

#endif
