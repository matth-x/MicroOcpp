// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPPSOCKET_H
#define OCPPSOCKET_H

#include <functional>
#include <memory>

namespace ArduinoOcpp {

using ReceiveTXTcallback = std::function<bool(const char*, size_t)>;

class OcppSocket {
public:
    OcppSocket() = default;
    virtual ~OcppSocket() = default;

    virtual void loop() = 0;

    virtual bool sendTXT(std::string &out) = 0;

    virtual void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) = 0; //ReceiveTXTcallback is defined in OcppServer.h
};

class OcppEchoSocket : public OcppSocket {
private:
    ReceiveTXTcallback receiveTXT;

    bool connected = true; //for simulating connection losses
public:
    void loop() override { }
    bool sendTXT(std::string &out) override {
        if (!connected) {
            return true;
        }
        if (receiveTXT) {
            return receiveTXT(out.c_str(), out.length());
        } else {
            return false;
        }
    }
    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) override {
        this->receiveTXT = receiveTXT;
    }

    void setConnected(bool connected) {this->connected = connected;}
    bool isConnected() {return connected;}
};

} //end namespace ArduinoOcpp

#ifndef AO_CUSTOM_WS

#include <WebSocketsClient.h>
#include <WebSocketsServer.h>

namespace ArduinoOcpp {
namespace EspWiFi {

class OcppClientSocket : public OcppSocket {
private:
    WebSocketsClient *wsock;
public:
    OcppClientSocket(WebSocketsClient *wsock);

    void loop();

    bool sendTXT(std::string &out);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);
};

class OcppServerSocket : public OcppSocket {
private:
    IPAddress ip_addr;
public:
    OcppServerSocket(IPAddress &ip_addr);
    ~OcppServerSocket();

    void loop();

    bool sendTXT(std::string &out);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);
};

} //end namespace EspWiFi
} //end namespace ArduinoOcpp
#endif //ndef AO_CUSTOM_WS
#endif
