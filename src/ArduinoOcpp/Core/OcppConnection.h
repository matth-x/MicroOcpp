// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
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
    
    OperationsQueue initiatedOcppOperations;
    std::deque<std::unique_ptr<OcppOperation>> receivedOcppOperations;

    void handleConfMessage(JsonDocument& json);
    void handleReqMessage(JsonDocument& json);
    void handleReqMessage(JsonDocument& json, std::unique_ptr<OcppOperation> op);
    void handleErrMessage(JsonDocument& json);
public:
    OcppConnection(OcppSocket& oSock, std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop(OcppSocket& oSock);

    void initiateOcppOperation(std::unique_ptr<OcppOperation> o);
    
    bool processOcppSocketInputTXT(const char* payload, size_t length);
};

} //end namespace ArduinoOcpp
#endif
