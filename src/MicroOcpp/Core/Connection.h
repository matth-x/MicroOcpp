// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CONNECTION_H
#define MO_CONNECTION_H

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>

#define MO_WS_CUSTOM  0
#define MO_WS_ARDUINO 1

//On all platforms other than Arduino, the integrated WS lib (links2004/arduinoWebSockets) cannot be
//used. On Arduino its usage is optional.
#ifndef MO_WS_USE
#if MO_PLATFORM == MO_PLATFORM_ARDUINO
#define MO_WS_USE MO_WS_ARDUINO
#else 
#define MO_WS_USE MO_WS_CUSTOM
#endif
#endif //MO_WS_USE

#ifdef __cplusplus

namespace MicroOcpp {

class Context;

class Connection {
protected:
    Context *context = nullptr;
public:
    Connection();
    virtual ~Connection() = default;

    void setContext(Context *context); // MO will set this during `setConnection()`

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
     * Pass incoming data from the server to this function. The OCPP library will consume it
     * The OCPP library calls this function once during initialization. It passes a callback function to
     * the socket. The socket should forward any incoming payload from the OCPP server to the receiveTXT callback
     */
    bool receiveTXT(const char *msg, size_t len);

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

class LoopbackConnection : public Connection, public MemoryManaged {
private:
    //for simulating connection losses
    bool online = true;
    bool connected = true;
public:
    LoopbackConnection();

    void loop() override;
    bool sendTXT(const char *msg, size_t length) override;

    void setOnline(bool online); //"online": sent messages are going through
    bool isOnline();
    void setConnected(bool connected); //"connected": connection has been established, but messages may not go through (e.g. weak connection)
    bool isConnected() override;
};

} //namespace MicroOcpp

#endif //__cplusplus

#if MO_WS_USE == MO_WS_ARDUINO

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *backendUrl;       //e.g. "wss://example.com:8443/steve/websocket/CentralSystemService". Must be defined
    const char *chargeBoxId;      //e.g. "charger001". Can be NULL
    const char *authorizationKey; //authorizationKey present in the websocket message header. Can be NULL. Set this to enable OCPP Security Profile 2
    const char *CA_cert;          //TLS certificate. Can be NULL. Set this to enable OCPP Security Profile 2
} MO_ConnectionConfig;

#ifdef __cplusplus
} //extern "C"

class WebSocketsClient;

namespace MicroOcpp {

Connection *makeArduinoWSClient(WebSocketsClient& arduinoWebsockets); //does not take ownership of arduinoWebsockets
void freeArduinoWSClient(Connection *connection);

Connection *makeDefaultConnection(MO_ConnectionConfig config, int ocppVersion);
void freeDefaultConnection(Connection *connection);

} //namespace MicroOcpp
#endif //__cplusplus
#endif //MO_WS_USE
#endif
