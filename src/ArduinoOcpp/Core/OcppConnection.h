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

class OperationDeserializer;
class OcppModel;
class OcppSocket;
class OcppOperation;
class FilesystemAdapter;

class OcppConnection {
private:
    OperationDeserializer& operationDeserializer;
    
    std::unique_ptr<OperationsQueue> initiatedOcppOperations;
    std::deque<std::unique_ptr<OcppOperation>> receivedOcppOperations;

    void receiveRequest(JsonArray json);
    void receiveRequest(JsonArray json, std::unique_ptr<OcppOperation> op);
    void receiveResponse(JsonArray json);

    unsigned long sendBackoffTime = 0;
    unsigned long sendBackoffPeriod = 0;
    const unsigned long BACKOFF_PERIOD_MAX = 65536;
    const unsigned long BACKOFF_PERIOD_INCREMENT = BACKOFF_PERIOD_MAX / 4;
public:
    OcppConnection(OperationDeserializer& operationDeserializer, std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem = nullptr);

    void setSocket(OcppSocket& sock);

    void loop(OcppSocket& ocppSock);

    void sendRequest(std::unique_ptr<OcppOperation> o); //send an OCPP operation request to the server
    
    bool receiveMessage(const char* payload, size_t length); //receive from  server: either a request or response
};

} //end namespace ArduinoOcpp
#endif
