// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Connection::Connection() {

}

void Connection::setContext(Context *context) {
    this->context = context;
}

bool Connection::receiveTXT(const char *msg, size_t len) {
    if (!context) {
        MO_DBG_WARN("must assign Connection to a Context");
        return false;
    }
    return context->getMessageService().receiveMessage(msg, len);
}

LoopbackConnection::LoopbackConnection() : MemoryManaged("WebSocketLoopback") { }

void LoopbackConnection::loop() { }

bool LoopbackConnection::sendTXT(const char *msg, size_t length) {
    if (!connected || !online) {
        return false;
    }
    return receiveTXT(msg, length); //`Connection::receiveTXT()` passes message back to MicroOcpp
}

void LoopbackConnection::setOnline(bool online) {
    this->online = online;
}

bool LoopbackConnection::isOnline() {
    return online;
}

void LoopbackConnection::setConnected(bool connected) {
    this->connected = connected;
}

bool LoopbackConnection::isConnected() {
    return connected;
}

#if MO_WS_USE == MO_WS_ARDUINO

#include <WebSocketsClient.h>

namespace MicroOcpp {

class ArduinoWSClient : public Connection, public MemoryManaged {
private:
    WebSocketsClient *wsock = nullptr;
    bool isWSockOwner = false;
public:

    ArduinoWSClient(WebSocketsClient *wsock, bool transferOwnership) : MemoryManaged("WebSocketsClient"), wsock(wsock), isWSockOwner(transferOwnership) {
        wsock->onEvent([callback](WStype_t type, uint8_t * payload, size_t length) {
            switch (type) {
                case WStype_DISCONNECTED:
                    MO_DBG_INFO("Disconnected");
                    break;
                case WStype_CONNECTED:
                    MO_DBG_INFO("Connected (path: %s)", payload);
                    break;
                case WStype_TEXT:
                    if (!receiveTXT((const char *) payload, length)) { //`Connection::receiveTXT()` forwards message to MicroOcpp
                        MO_DBG_WARN("Processing WebSocket input event failed");
                    }
                    break;
                case WStype_BIN:
                    MO_DBG_WARN("Binary data stream not supported");
                    break;
                case WStype_PING:
                    // pong will be send automatically
                    MO_DBG_VERBOSE("Recv WS ping");
                    break;
                case WStype_PONG:
                    // answer to a ping we send
                    MO_DBG_VERBOSE("Recv WS pong");
                    break;
                case WStype_FRAGMENT_TEXT_START: //fragments are not supported
                default:
                    MO_DBG_WARN("Unsupported WebSocket event type");
                    break;
            }
        });
    }

    ~ArduinoWSClient() {
        if (isWSockOwner) {
            delete wsock;
            wsock = nullptr;
            isWSockOwner = false;
        }
    }

    void loop() overide {
        wsock->loop();
    }

    bool sendTXT(const char *msg, size_t length) override {
        return wsock->sendTXT(msg, length);
    }

    bool isConnected() override {
        return wsock->isConnected();
    }
};

Connection *makeArduinoWSClient(WebSocketsClient& arduinoWebsockets) {
    return static_cast<Connection*>(new ArduinoWSClient(&arduinoWebsockets, false));
}

void freeArduinoWSClient(Connection *connection) {
    delete connection;
}

Connection *makeDefaultConnection(MO_ConnectionConfig config, int ocppVersion) {

    Connection *connection = nullptr;
    WebSocketsClient *wsock = nullptr;

    if (!config.backendUrl) {
        MO_DBG_ERR("invalid args");
        goto fail;
    }

    wsock = new WebSocketsClient();
    if (!wsock) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    /*
     * parse backendUrl so that it suits the links2004/arduinoWebSockets interface
     */
    auto url = makeString("WebSocketsClient", config.backendUrl);

    //tolower protocol specifier
    for (auto c = url.begin(); *c != ':' && c != url.end(); c++) {
        *c = tolower(*c);
    }

    bool isTLS = true;
    if (!strncmp(url.c_str(),"wss://",strlen("wss://"))) {
        isTLS = true;
    } else if (!strncmp(url.c_str(),"ws://",strlen("ws://"))) {
        isTLS = false;
    } else {
        MO_DBG_ERR("only ws:// and wss:// supported");
        return;
    }

    //parse host, port
    auto host_port_path = url.substr(url.find_first_of("://") + strlen("://"));
    auto host_port = host_port_path.substr(0, host_port_path.find_first_of('/'));
    auto path = host_port_path.substr(host_port.length());
    auto host = host_port.substr(0, host_port.find_first_of(':'));
    if (host.empty()) {
        MO_DBG_ERR("could not parse host: %s", url.c_str());
        return;
    }
    uint16_t port = 0;
    auto port_str = host_port.substr(host.length());
    if (port_str.empty()) {
        port = isTLS ? 443U : 80U;
    } else {
        //skip leading ':'
        port_str = port_str.substr(1);
        for (auto c = port_str.begin(); c != port_str.end(); c++) {
            if (*c < '0' || *c > '9') {
                MO_DBG_ERR("could not parse port: %s", url.c_str());
                return; 
            }
            auto p = port * 10U + (*c - '0');
            if (p < port) {
                MO_DBG_ERR("could not parse port (overflow): %s", url.c_str());
                return;
            }
            port = p;
        }
    }

    if (path.empty()) {
        path = "/";
    }

    if (config.chargeBoxId) {
        if (path.back() != '/') {
            path += '/';
        }

        path += config.chargeBoxId;
    }

    const char *ocppVersionStr = "ocpp1.6";
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        ocppVersionStr = "ocpp1.6";
    }
    #endif
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        ocppVersionStr = "ocpp2.0.1";
    }
    #endif

    MO_DBG_INFO("connecting to %s -- (host: %s, port: %u, path: %s)", url.c_str(), host.c_str(), port, path.c_str());

    wsock = new WebSocketsClient();
    if (!wsock) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    if (config.isTLS) {
        // server address, port, path and TLS certificate
        webSocket->beginSslWithCA(host.c_str(), port, path.c_str(), config.CA_cert, ocppVersionStr);
    } else {
        // server address, port, path
        webSocket->begin(host.c_str(), port, path.c_str(), ocppVersionStr);
    }

    // try ever 5000 again if connection has failed
    webSocket->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    size_t chargeBoxIdLen = config.chargeBoxId ? strlen(config.chargeBoxId) : 0;
    size_t authorizationKeyLen = config.authorizationKey ? strlen(config.authorizationKey) : 0;

    // add authentication data (optional)
    if (config.authorizationKey && (authorizationKeyLen + chargeBoxIdLen >= 4)) {
        webSocket->setAuthorization(config.chargeBoxId ? config.chargeBoxId : "", config.authorizationKey);
    }

    connection = new ArduinoWSClient(webSocket, true);
    if (!connection) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    //success
    return static_cast<Connection*>(connection);
fail:
    delete connection;
    delete wsock;
}

void freeDefaultConnection(Connection *connection) {
    delete connection;
}

} //namespace MicroOcpp
#endif //MO_WS_USE
