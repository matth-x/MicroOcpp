// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_CONNECTION_H
#define MO_CONNECTION_H

#include <functional>
#include <memory>

#include <MicroOcpp/Platform.h>

//On all platforms other than Arduino, the integrated WS lib (links2004/arduinoWebSockets) cannot be
//used. On Arduino it's usage is optional.
#ifndef MO_CUSTOM_WS
#if MO_PLATFORM != MO_PLATFORM_ARDUINO
#define MO_CUSTOM_WS
#endif
#endif //ndef MO_CUSTOM_WS

namespace MicroOcpp {

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
    virtual bool sendTXT(const char *msg, size_t length) = 0;

    /*
     * The OCPP library calls this function once during initialization. It passes a callback function to
     * the socket. The socket should forward any incoming payload from the OCPP server to the receiveTXT callback
     */
    virtual void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) = 0;

    /*
     * Returns the timestamp of the last incoming message. Use mocpp_tick_ms() for creating the correct timestamp
     */
    virtual unsigned long getLastRecv() {return 0;}

    /*
     * Returns the timestamp of the last time a connection got successfully established. Use mocpp_tick_ms() for creating the correct timestamp
     */
    virtual unsigned long getLastConnected() = 0;
};

class LoopbackConnection : public Connection {
private:
    ReceiveTXTcallback receiveTXT;

    bool connected = true; //for simulating connection losses
    unsigned long lastRecv = 0;
    unsigned long lastConn = 0;
public:
    void loop() override;
    bool sendTXT(const char *msg, size_t length) override;
    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) override;
    unsigned long getLastRecv() override;
    unsigned long getLastConnected() override;

    void setConnected(bool connected);
    bool isConnected() {return connected;}
};

} //end namespace MicroOcpp

#ifndef MO_CUSTOM_WS

#include <WebSocketsClient.h>

namespace MicroOcpp {
namespace EspWiFi {

class WSClient : public Connection {
private:
    WebSocketsClient *wsock;
    unsigned long lastRecv = 0, lastConnected = 0;
public:
    WSClient(WebSocketsClient *wsock);

    void loop();

    bool sendTXT(const char *msg, size_t length);

    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT);

    unsigned long getLastRecv() override; //get time of last successful receive in millis

    unsigned long getLastConnected() override; //get last connection creation in millis
};

} //end namespace EspWiFi
} //end namespace MicroOcpp
#endif //ndef MO_CUSTOM_WS
#endif
