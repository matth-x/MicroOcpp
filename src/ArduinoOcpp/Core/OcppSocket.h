// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPSOCKET_H
#define OCPPSOCKET_H

#include <WebSocketsClient.h>
#include <WebSocketsServer.h>

#include <ArduinoOcpp/Core/OcppServer.h>

namespace ArduinoOcpp {

class OcppSocket {
public:
    OcppSocket();
    virtual ~OcppSocket() = default;

    virtual void loop() = 0;

    virtual bool sendTXT(String &out) = 0;

    virtual void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) = 0; //ReceiveTXTcallback is defined in OcppServer.h
};

namespace EspWiFi {

class OcppClientSocket : public OcppSocket {
private:
    //std::shared_ptr<WebSocketsClient> wsock;
    WebSocketsClient *wsock;
public:
    //OcppClientSocket(ReceiveTXTcallback &receiveTXT, std::shared_ptr<WebSocketsClient> wsock);
    OcppClientSocket(WebSocketsClient *wsock);

    void loop();

    bool sendTXT(String &out);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);
};

class OcppServerSocket : public OcppSocket {
private:
    IPAddress ip_addr;
public:
    OcppServerSocket(IPAddress &ip_addr);
    ~OcppServerSocket();

    void loop();

    bool sendTXT(String &out);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);
};

} //end namespace EspWiFi
} //end namespace ArduinoOcpp
#endif
