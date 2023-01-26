// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONDATA_H
#define AUTHORIZATIONDATA_H

#include <ArduinoOcpp/MessagesV16/CiStrings.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

enum class AuthorizationStatus:uint8_t {
    Accepted,
    Blocked,
    Expired,
    Invalid,
    ConcurrentTx,
    UNDEFINED //not part of OCPP 1.6
};

#define AUTHORIZATIONSTATUS_LEN_MAX (sizeof("ConcurrentTx") - 1) //max length of serialized AuthStatus

const char *serializeAuthorizationStatus(AuthorizationStatus status);
AuthorizationStatus deserializeAuthorizationStatus(const char *cstr);

class AuthorizationData {
private:
    //data structure optimized for memory consumption

    std::unique_ptr<char[]> parentIdTag;
    std::unique_ptr<OcppTimestamp> expiryDate;

    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};

    AuthorizationStatus status = AuthorizationStatus::UNDEFINED;
    uint16_t hist_order;
public:
    AuthorizationData();
    ~AuthorizationData();

    void readJson(JsonObject entry);

    size_t getJsonCapacity();
    void writeJson(JsonObject& entry);
};

}

#endif
