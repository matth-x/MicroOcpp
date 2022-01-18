// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/OcppSocket.h>
#include <ArduinoOcpp/Core/OcppServer.h>
#include <Variants.h>

#ifndef AO_CUSTOM_WS

using namespace ArduinoOcpp;

using namespace ArduinoOcpp::EspWiFi;

OcppClientSocket::OcppClientSocket(WebSocketsClient *wsock) : wsock(wsock) {

}

void OcppClientSocket::loop() {
    wsock->loop();
}

bool OcppClientSocket::sendTXT(String &out) {
    return wsock->sendTXT(out);
}

void OcppClientSocket::setReceiveTXTcallback(ReceiveTXTcallback &callback) {
    wsock->onEvent([callback](WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                if (DEBUG_OUT) Serial.print(F("[OcppClientSocket] Disconnected!\n"));
                break;
            case WStype_CONNECTED:
                if (DEBUG_OUT) Serial.printf("[OcppClientSocket] Connected to url: %s\n", payload);
                break;
            case WStype_TEXT:
                if (DEBUG_OUT || TRAFFIC_OUT) Serial.printf("[OcppClientSocket] get text: %s\n", payload);

                if (!callback((const char *) payload, length)) { //forward message to OcppEngine
                    if (DEBUG_OUT) Serial.print(F("[OcppClientSocket] Processing WebSocket input event failed!\n"));
                }
                break;
            case WStype_BIN:
                if (DEBUG_OUT) Serial.print(F("[OcppClientSocket] Incoming binary data stream not supported"));
                break;
            case WStype_PING:
                // pong will be send automatically
                if (DEBUG_OUT || TRAFFIC_OUT) Serial.print(F("[OcppClientSocket] get ping\n"));
                break;
            case WStype_PONG:
                // answer to a ping we send
                if (DEBUG_OUT || TRAFFIC_OUT) Serial.print(F("[OcppClientSocket] get pong\n"));
                break;
            case WStype_FRAGMENT_TEXT_START: //fragments are not supported
            default:
                if (DEBUG_OUT) Serial.print(F("[OcppClientSocket] Unsupported WebSocket event type\n"));
                break;
        }
    });
}

OcppServerSocket::OcppServerSocket(IPAddress &ip_addr) : ip_addr(ip_addr) {
    
}

OcppServerSocket::~OcppServerSocket() {
    OcppServer::getInstance()->removeReceiveTXTcallback(this->ip_addr);
}

void OcppServerSocket::loop() {
    //nothing here. The client must call the EspWiFi server loop function
}

bool OcppServerSocket::sendTXT(String &out) {
    if (DEBUG_OUT) Serial.print(F("[OcppServerSocket] Send TXT: "));
    if (DEBUG_OUT) Serial.println(out);
    return OcppServer::getInstance()->sendTXT(ip_addr, out);
}

void OcppServerSocket::setReceiveTXTcallback(ReceiveTXTcallback &callback) {
    OcppServer::getInstance()->setReceiveTXTcallback(ip_addr, callback);
}

#endif
