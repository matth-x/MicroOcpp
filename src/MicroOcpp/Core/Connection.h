// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONNECTION_H
#define MO_CONNECTION_H

#include <functional>
#include <memory>

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>

//On all platforms other than Arduino, the integrated WS lib (links2004/arduinoWebSockets) cannot be
//used. On Arduino its usage is optional.
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
     *
     * DEPRECATED: this function is superseded by isConnected(). Will be removed in MO v2.0
     */
    virtual unsigned long getLastRecv() {return 0;}

    /*
     * Returns the timestamp of the last time a connection got successfully established. Use mocpp_tick_ms() for creating the correct timestamp
     */
    virtual unsigned long getLastConnected() = 0;

    /*
     * NEW IN v1.1
     *
     * Returns true if the connection is open; false if the charger is known to be offline.
     * 
     * This function determines if MO is in "offline mode". In offline mode, MO doesn't wait for Authorize responses
     * before performing fully local authorization. If the connection is disrupted but isConnected is still true, then
     * MO will first wait for a timeout to expire (20 seconds) before going into offline mode.
     * 
     * Returning true will have no further effects other than  using the timeout-then-offline mechanism. If the
     * connection status is uncertain, it's best to return true by default.
     */
    virtual bool isConnected() {return true;} //MO ignores true. This default implementation keeps backwards-compatibility
};

class LoopbackConnection : public Connection, public AllocOverrider {
private:
    ReceiveTXTcallback receiveTXT;

    //for simulating connection losses
    bool online = true;
    bool connected = true;
    unsigned long lastRecv = 0;
    unsigned long lastConn = 0;
public:
    LoopbackConnection();

    void loop() override;
    bool sendTXT(const char *msg, size_t length) override;
    void setReceiveTXTcallback(ReceiveTXTcallback &receiveTXT) override;
    unsigned long getLastRecv() override;
    unsigned long getLastConnected() override;

    void setOnline(bool online); //"online": sent messages are going through
    bool isOnline() {return online;}
    void setConnected(bool connected); //"connected": connection has been established, but messages may not go through (e.g. weak connection)
    bool isConnected() override {return connected;}
};

} //end namespace MicroOcpp

#ifndef MO_CUSTOM_WS

#include <WebSocketsClient.h>

namespace MicroOcpp {
namespace EspWiFi {

class WSClient : public Connection, public AllocOverrider {
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

    bool isConnected() override;
};

} //end namespace EspWiFi
} //end namespace MicroOcpp
#endif //ndef MO_CUSTOM_WS
#endif
