// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPCONNECTION_H
#define OCPPCONNECTION_H

#include <deque>
#include <memory>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppOperation.h>

namespace ArduinoOcpp {

class OcppConnection {
private:
    OcppSocket *ocppSock;
    std::deque<OcppOperation*> initiatedOcppOperations;
    std::deque<OcppOperation*> receivedOcppOperations;

    void handleConfMessage(JsonDocument *json);
    void handleReqMessage(JsonDocument *json);
    void handleReqMessage(JsonDocument *json, OcppOperation *op);
    void handleErrMessage(JsonDocument *json);
public:
    OcppConnection(OcppSocket *ocppSock);

    void loop();

    void initiateOcppOperation(OcppOperation *o);
    
    bool processOcppSocketInputTXT(const char* payload, size_t length);
};

} //end namespace ArduinoOcpp
#endif