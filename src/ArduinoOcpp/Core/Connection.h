// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPSOCKET_H
#define OCPPSOCKET_H

#include <functional>
#include <memory>
#include <string>

namespace ArduinoOcpp {

using ReceiveTXTcallback = std::function<bool(const char*, size_t)>;

class Connection {
public:
    Connection() = default;
    virtual ~Connection() = default;

    /*
     * The OCPP library will call this function frequently. If you need to execute regular routines, like
     * calling the loop-function of the WebSocket library, implement them here
     */
    virtual void loop() = 0;

    /*
     * The OCPP library calls this function for sending out OCPP messages to the server
     */
    virtual bool sendTXT(std::string &out) = 0;

    /*
     * The OCPP library calls this function once during initialization. It passes a callback function to
     * the socket. The socket should forward any incoming payload from the OCPP server to the receiveTXT callback
     */
    virtual void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) = 0;

    /*
     * Returns the timestamp of the last incoming message. Use ao_tick_ms() for creating the correct timestamp
     */
    virtual unsigned long getLastRecv() {return 0;}
};

class LoopbackConnection : public Connection {
private:
    ReceiveTXTcallback receiveTXT;

    bool connected = true; //for simulating connection losses
    unsigned long lastRecv = 0;
public:
    void loop() override;
    bool sendTXT(std::string &out) override;
    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) override;
    unsigned long getLastRecv() override;

    void setConnected(bool connected) {this->connected = connected;}
    bool isConnected() {return connected;}
};

} //end namespace ArduinoOcpp

#ifndef AO_CUSTOM_WS

#include <WebSocketsClient.h>

namespace ArduinoOcpp {
namespace EspWiFi {

class WSClient : public Connection {
private:
    WebSocketsClient *wsock;
    unsigned long lastRecv = 0;
public:
    WSClient(WebSocketsClient *wsock);

    void loop();

    bool sendTXT(std::string &out);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);

    unsigned long getLastRecv() override; //get time of last successful receive in millis
};

} //end namespace EspWiFi
} //end namespace ArduinoOcpp
#endif //ndef AO_CUSTOM_WS
#endif
