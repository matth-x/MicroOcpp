// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPCONNECTION_H
#define OCPPCONNECTION_H

#include <ArduinoOcpp/Core/OperationsQueue.h>

#include <deque>
#include <memory>
#include <ArduinoJson.h>

namespace ArduinoOcpp {

class OcppModel;
class OcppSocket;
class OcppOperation;
class FilesystemAdapter;

class OcppConnection {
private:
    std::shared_ptr<OcppModel> baseModel;
    std::shared_ptr<FilesystemAdapter> filesystem;
    
    std::unique_ptr<OperationsQueue> initiatedOcppOperations;
    std::deque<std::unique_ptr<OcppOperation>> receivedOcppOperations;

    void handleConfMessage(JsonDocument& json);
    void handleReqMessage(JsonDocument& json);
    void handleReqMessage(JsonDocument& json, std::unique_ptr<OcppOperation> op);
    void handleErrMessage(JsonDocument& json);
public:
    OcppConnection(std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem = nullptr);

    void setSocket(OcppSocket& sock);

    void loop(OcppSocket& ocppSock);

    void initiateOperation(std::unique_ptr<OcppOperation> o); //send an OCPP operation request to the server
    
    bool receiveMessage(const char* payload, size_t length); //receive an OCPP operation from the server - either a confirmation or a request
};

} //end namespace ArduinoOcpp
#endif
