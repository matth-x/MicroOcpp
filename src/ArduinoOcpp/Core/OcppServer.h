// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPPSERVER_H
#define OCPPSERVER_H

#include <vector>
#include <ArduinoOcpp/Core/OcppSocket.h>

namespace ArduinoOcpp {

using WsClient = uint8_t;

} //end namespace ArduinoOcpp

#ifndef AO_CUSTOM_WS

#include <WebSocketsServer.h>

namespace ArduinoOcpp {
namespace EspWiFi {

struct ReceiveTXTroute {
    IPAddress ip_addr;
    WsClient num;
    ReceiveTXTcallback processTXT;
};

class OcppServer {
private:

    std::vector<ReceiveTXTroute> receiveTXTrouting;

    WebSocketsServer wsockServer = WebSocketsServer(80);

    OcppServer();
    static OcppServer *instance;
public:
    static OcppServer *getInstance();

    void loop();

    void wsockEvent(WsClient num, WStype_t type, uint8_t * payload, size_t length);

    void setReceiveTXTcallback(IPAddress &ip_addr, ReceiveTXTcallback &callback);

    void removeReceiveTXTcallback(IPAddress &ip_addr);

    bool sendTXT(IPAddress &ip_addr, std::string &out);
};

} //end namespace EspWiFi
} //end namespace ArduinoOcpp
#endif //ndef AO_CUSTOM_WS
#endif
