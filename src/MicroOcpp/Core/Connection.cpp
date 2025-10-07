// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>

void mo_connection_init(MO_Connection *connection) {
    memset(connection, 0, sizeof(MO_Connection));
}

bool mo_receiveTXT(MO_Context *ctx, const char *msg, size_t len) {
    if (!ctx) {
        MO_DBG_WARN("MO Context not assigned");
        return false;
    }
    auto context = reinterpret_cast<MicroOcpp::Context*>(ctx);
    return context->getMessageService().receiveMessage(msg, len);
}

namespace MicroOcpp {

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

namespace CppConnectionAdapter {

struct CompatData : public MemoryManaged {
    Connection *cppConnection = nullptr; //"Legacy" MO v1.x Connection class
    MO_Context *trackCtx = nullptr; //MO Context

    CompatData() : MemoryManaged("CppConnectionAdapter") { }
};

void loop(MO_Connection *conn) {
    auto data = reinterpret_cast<CompatData*>(conn->userData);
    if (conn->ctx != data->trackCtx) {
        data->cppConnection->setContext(reinterpret_cast<Context*>(conn->ctx));
        data->trackCtx = conn->ctx;
    }
    data->cppConnection->loop();
}

bool sendTXT(MO_Connection *conn, const char *msg, size_t length) {
    auto data = reinterpret_cast<CompatData*>(conn->userData);
    return data->cppConnection->sendTXT(msg, length);
}

bool isConnected(MO_Connection *conn) {
    auto data = reinterpret_cast<CompatData*>(conn->userData);
    return data->cppConnection->isConnected();
}

} //namespace CppConnectionAdapter

MO_Connection *makeCppConnectionAdapter(Connection *cppConnection) {

    MO_Connection *connection = nullptr;
    CppConnectionAdapter::CompatData *data = nullptr;

    data = new CppConnectionAdapter::CompatData();
    if (!data) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    data->cppConnection = cppConnection;

    connection = reinterpret_cast<MO_Connection*>(MO_MALLOC("CppConnectionAdapter", sizeof(MO_Connection)));
    if (!connection) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    connection->userData = reinterpret_cast<void*>(data);
    connection->loop = CppConnectionAdapter::loop;
    connection->sendTXT = CppConnectionAdapter::sendTXT;
    connection->isConnected = CppConnectionAdapter::isConnected;

    return connection;

fail:
    delete data;
    MO_FREE(connection);
    return nullptr;
}

void freeCppConnectionAdapter(MO_Connection *conn) {
    auto data = reinterpret_cast<CppConnectionAdapter::CompatData*>(conn->userData);
    if (conn->ctx != data->trackCtx) {
        data->cppConnection->setContext(reinterpret_cast<Context*>(conn->ctx));
        data->trackCtx = conn->ctx;
    }
    delete data;
    MO_FREE(conn);
}

} //namespace MicroOcpp

namespace MicroOcpp {
namespace LoopbackConnection {

struct LoopbackData : public MemoryManaged {
    bool online = true;
    bool connected = true;

    LoopbackData() : MemoryManaged("WebSocketLoopback") { }
};

bool sendTXT(MO_Connection *conn, const char *msg, size_t length) {
    auto data = reinterpret_cast<LoopbackData*>(conn->userData);
    if (!data->connected || !data->online) {
        return false;
    }
    return mo_receiveTXT(conn->ctx, msg, length);
}

bool isConnected(MO_Connection *conn) {
    auto data = reinterpret_cast<LoopbackData*>(conn->userData);
    return data->connected;
}

} //namespace LoopbackConnection
} //namespace MicroOcpp

MO_Connection *mo_loopback_make() {

    MO_Connection *connection = nullptr;
    MicroOcpp::LoopbackConnection::LoopbackData *data = nullptr;

    data = new MicroOcpp::LoopbackConnection::LoopbackData();
    if (!data) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    connection = reinterpret_cast<MO_Connection*>(MO_MALLOC("WebSocketLoopback", sizeof(MO_Connection)));
    if (!connection) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    connection->userData = reinterpret_cast<void*>(data);
    connection->sendTXT = MicroOcpp::LoopbackConnection::sendTXT;
    connection->isConnected = MicroOcpp::LoopbackConnection::isConnected;

    return connection;

fail:
    delete data;
    MO_FREE(connection);
    return nullptr;
}

void mo_loopback_free(MO_Connection *connection) {
    if (connection) {
        auto data = reinterpret_cast<MicroOcpp::LoopbackConnection::LoopbackData*>(connection->userData);
        delete data;
        MO_FREE(connection);
    }
}

void mo_loopback_setConnected(MO_Connection *connection, bool connected) {
    auto data = reinterpret_cast<MicroOcpp::LoopbackConnection::LoopbackData*>(connection->userData);
    data->connected = connected;
}

void mo_loopback_setOnline(MO_Connection *connection, bool online) {
    auto data = reinterpret_cast<MicroOcpp::LoopbackConnection::LoopbackData*>(connection->userData);
    data->online = online;
}

#if MO_WS_USE == MO_WS_ARDUINO

void mo_connectionConfig_init(MO_ConnectionConfig *config) {
    memset(config, 0, sizeof(*config));
}

bool mo_connectionConfig_copy(MO_ConnectionConfig *dst, MO_ConnectionConfig *src) {

    size_t size = 0;
    size += src->backendUrl ? strlen(src->backendUrl) + 1 : 0;
    size += src->chargeBoxId ? strlen(src->chargeBoxId) + 1 : 0;
    size += src->authorizationKey ? strlen(src->authorizationKey) + 1 : 0;

    char *buf = static_cast<char*>(MO_MALLOC("WebSocketsClient", size));
    if (!buf) {
        MO_DBG_ERR("OOM");
        return false;
    }

    mo_connectionConfig_init(dst);

    size_t written = 0;
    if (src->backendUrl) {
        dst->backendUrl = buf + written;
        auto ret = snprintf(buf + written, size - written, "%s", src->backendUrl);
        if (ret < 0 || (size_t)ret >= size - written) {
            goto fail;
        }
        written += ret + 1;
    }

    if (src->chargeBoxId) {
        dst->chargeBoxId = buf + written;
        auto ret = snprintf(buf + written, size - written, "%s", src->chargeBoxId);
        if (ret < 0 || (size_t)ret >= size - written) {
            goto fail;
        }
        written += ret + 1;
    }

    if (src->authorizationKey) {
        dst->authorizationKey = buf + written;
        auto ret = snprintf(buf + written, size - written, "%s", src->authorizationKey);
        if (ret < 0 || (size_t)ret >= size - written) {
            goto fail;
        }
        written += ret + 1;
    }

    if (written != size) {
        MO_DBG_ERR("internal error");
        goto fail;
    }

    dst->internalBuf = buf;
    return true;
fail:
    MO_DBG_ERR("copy failed");
    MO_FREE(buf);
    return false;
}

void mo_connectionConfig_deinit(MO_ConnectionConfig *config) {
    MO_FREE(config->internalBuf);
    memset(config, 0, sizeof(*config));
}

#include <WebSocketsClient.h>

namespace MicroOcpp {
namespace ArduinoWS {

struct ArduinoWSClientData : public MemoryManaged {
    WebSocketsClient *wsock = nullptr;
    bool isWSockOwner = false;

    ArduinoWSClientData() : MemoryManaged("WebSocketsClient") {

    }
};

void loop(MO_Connection *conn) {
    auto data = reinterpret_cast<ArduinoWSClientData*>(conn->userData);
    data->wsock->loop();
}

bool sendTXT(MO_Connection *conn, const char *msg, size_t length) {
    auto data = reinterpret_cast<ArduinoWSClientData*>(conn->userData);
    return data->wsock->sendTXT(msg, length)
}

bool isConnected(MO_Connection *conn) {
    auto data = reinterpret_cast<ArduinoWSClientData*>(conn->userData);
    return data->wsock->isConnected();
}

void handleWebSocketsClientEvent(MO_Connection *conn, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            MO_DBG_INFO("Disconnected");
            break;
        case WStype_CONNECTED:
            MO_DBG_INFO("Connected (path: %s)", payload);
            break;
        case WStype_TEXT:
            if (!mo_receiveTXT(conn->ctx, (const char *)payload, length)) { //`mo_receiveTXT()` forwards message to MicroOcpp
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
}

} //namespace ArduinoWS
} //namespace MicroOcpp

MO_Connection *mo_makeDefaultConnection(MO_ConnectionConfig config, int ocppVersion) {

    MO_Connection *connection = nullptr;
    WebSocketsClient *wsock = nullptr;

    if (!config.backendUrl) {
        MO_DBG_ERR("invalid args");
        return nullptr;
    }

    wsock = new WebSocketsClient();
    if (!wsock) {
        MO_DBG_ERR("OOM");
        return nullptr;
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
        return nullptr;
    }

    //parse host, port
    auto host_port_path = url.substr(url.find_first_of("://") + strlen("://"));
    auto host_port = host_port_path.substr(0, host_port_path.find_first_of('/'));
    auto path = host_port_path.substr(host_port.length());
    auto host = host_port.substr(0, host_port.find_first_of(':'));
    if (host.empty()) {
        MO_DBG_ERR("could not parse host: %s", url.c_str());
        return nullptr;
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
                return nullptr;
            }
            auto p = port * 10U + (*c - '0');
            if (p < port) {
                MO_DBG_ERR("could not parse port (overflow): %s", url.c_str());
                return nullptr;
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

    if (isTLS) {
        // server address, port, path and TLS certificate
        wsock->beginSslWithCA(host.c_str(), port, path.c_str(), config.CA_cert, ocppVersionStr);
    } else {
        // server address, port, path
        wsock->begin(host.c_str(), port, path.c_str(), ocppVersionStr);
    }

    // try ever 5000 again if connection has failed
    wsock->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    wsock->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers


    // add authentication data (optional)
    {
        size_t chargeBoxIdLen = config.chargeBoxId ? strlen(config.chargeBoxId) : 0;
        size_t authorizationKeyLen = config.authorizationKey ? strlen(config.authorizationKey) : 0;

        if (config.authorizationKey && (authorizationKeyLen + chargeBoxIdLen >= 4)) {
            wsock->setAuthorization(config.chargeBoxId ? config.chargeBoxId : "", config.authorizationKey);
        }
    }

    connection = MicroOcpp::ArduinoWS::makeArduinoWSClient(wsock, true);
    if (!connection) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    //success
    return connection;
fail:
    delete connection;
    delete wsock;
    return nullptr;
}

void mo_freeDefaultConnection(MO_Connection *connection) {
    freeArduinoWSClient(connection);
}

namespace MicroOcpp {

MO_Connection *makeArduinoWSClient(WebSocketsClient *wsock, bool transferOwnership) {

    MO_Connection *connection = nullptr;
    ArduinoWSClientData *data = nullptr;

    data = new ArduinoWSClientData();
    if (!data) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    data->wsock = wsock;
    data->isWSockOwner = transferOwnership; //only applies if this function returns successfully

    connection = reinterpret_cast<MO_Connection*>(MO_MALLOC("WebSocketsClient", sizeof(MO_Connection)));
    if (!connection) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    connection->userData = reinterpret_cast<void*>(data);
    connection->loop = loop;
    connection->sendTXT = sendTXT;
    connection->isConnected = isConnected;

    wsock->onEvent([connection](WStype_t type, uint8_t * payload, size_t length) {
        handleWebSocketsClientEvent(connection, type, payload, length);
    });

    //success
    return connection;
fail:
    MO_FREE(connection);
    delete data;
    return nullptr;
}

MO_Connection *makeArduinoWSClient(WebSocketsClient& arduinoWebsockets) {
    return makeArduinoWSClient(&arduinoWebsockets, false);
}

void freeArduinoWSClient(MO_Connection *connection) {
    auto data = reinterpret_cast<ArduinoWSClientData*>(connection->userData);
    if (data->isWSockOwner) {
        delete data->wsock;
    }
    delete data;
    MO_FREE(connection);
}

} //namespace MicroOcpp
#endif //MO_WS_USE
