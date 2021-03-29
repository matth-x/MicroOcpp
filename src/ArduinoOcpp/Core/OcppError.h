// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPERROR_H
#define OCPPERROR_H

#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include <ArduinoOcpp/Core/OcppOperation.h>

namespace ArduinoOcpp {

class NotImplemented : public OcppMessage {
public:
    const char *getErrorCode() {
        return "NotImplemented";
    }
};

class OutOfMemory : public OcppMessage {
private:
    uint32_t freeHeap;
    size_t msgLen;
public:
    OutOfMemory(uint32_t freeHeap, size_t msgLen) : freeHeap(freeHeap), msgLen(msgLen) { }
    const char *getErrorCode() {
        return "InternalError";
    }
    const char *getErrorDescription() {
        return "Too little free memory on the controller. Operation denied";
    }
    DynamicJsonDocument *getErrorDetails() {
        DynamicJsonDocument *errDoc = new DynamicJsonDocument(JSON_OBJECT_SIZE(2));
        JsonObject err = errDoc->to<JsonObject>();
        err["free_heap"] = freeHeap;
        err["msg_length"] = msgLen;
        return errDoc;
    }
};

class WebSocketError : public OcppMessage {
private:
    const char *description;
public:
    WebSocketError(const char *description) : description(description) { }
    const char *getErrorCode() {
        return "GenericError";
    }
    const char *getErrorDescription() {
        return description;
    }
};

} //end namespace ArduinoOcpp
#endif