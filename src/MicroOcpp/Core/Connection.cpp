// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

void LoopbackConnection::loop() { }

bool LoopbackConnection::sendTXT(std::string &out) {
    if (!connected) {
        return false;
    }
    if (receiveTXT) {
        lastRecv = mocpp_tick_ms();
        return receiveTXT(out.c_str(), out.length());
    } else {
        return false;
    }
}

void LoopbackConnection::setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) {
    this->receiveTXT = receiveTXT;
}

unsigned long LoopbackConnection::getLastRecv() {
    return lastRecv;
}

unsigned long LoopbackConnection::getLastConnected() {
    return lastConn;
}

void LoopbackConnection::setConnected(bool connected) {
    if (connected) {
        lastConn = mocpp_tick_ms();
    }
    this->connected = connected;
}

#ifndef MOCPP_CUSTOM_WS

using namespace MicroOcpp::EspWiFi;

WSClient::WSClient(WebSocketsClient *wsock) : wsock(wsock) {

}

void WSClient::loop() {
    wsock->loop();
}

bool WSClient::sendTXT(std::string &out) {
    return wsock->sendTXT(out.c_str(), out.length());
}

void WSClient::setReceiveTXTcallback(ReceiveTXTcallback &callback) {
    auto& captureLastRecv = lastRecv;
    auto& captureLastConnected = lastConnected;
    wsock->onEvent([callback, &captureLastRecv, &captureLastConnected](WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                MOCPP_DBG_INFO("Disconnected");
                break;
            case WStype_CONNECTED:
                MOCPP_DBG_INFO("Connected to url: %s", payload);
                captureLastRecv = mocpp_tick_ms();
                captureLastConnected = mocpp_tick_ms();
                break;
            case WStype_TEXT:
                if (callback((const char *) payload, length)) { //forward message to RequestQueue
                    captureLastRecv = mocpp_tick_ms();
                } else {
                    MOCPP_DBG_WARN("Processing WebSocket input event failed");
                }
                break;
            case WStype_BIN:
                MOCPP_DBG_WARN("Binary data stream not supported");
                break;
            case WStype_PING:
                // pong will be send automatically
                MOCPP_DBG_TRAFFIC_IN(8, "WS ping");
                captureLastRecv = mocpp_tick_ms();
                break;
            case WStype_PONG:
                // answer to a ping we send
                MOCPP_DBG_TRAFFIC_IN(8, "WS pong");
                captureLastRecv = mocpp_tick_ms();
                break;
            case WStype_FRAGMENT_TEXT_START: //fragments are not supported
            default:
                MOCPP_DBG_WARN("Unsupported WebSocket event type");
                break;
        }
    });
}

unsigned long WSClient::getLastRecv() {
    return lastRecv;
}

unsigned long WSClient::getLastConnected() {
    return lastConnected;
}

#endif
