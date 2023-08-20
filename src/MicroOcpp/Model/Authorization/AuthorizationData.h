// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONDATA_H
#define AUTHORIZATIONDATA_H

#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Core/Time.h>
#include <ArduinoJson.h>
#include <memory>

namespace MicroOcpp {

enum class AuthorizationStatus : uint8_t {
    Accepted,
    Blocked,
    Expired,
    Invalid,
    ConcurrentTx,
    UNDEFINED //not part of OCPP 1.6
};

#define AUTHDATA_KEY_IDTAG(COMPACT)       (COMPACT ? "it" : "idTag")
#define AUTHDATA_KEY_IDTAGINFO            "idTagInfo"
#define AUTHDATA_KEY_EXPIRYDATE(COMPACT)  (COMPACT ? "ed" : "expiryDate")
#define AUTHDATA_KEY_PARENTIDTAG(COMPACT) (COMPACT ? "pi" : "parentIdTag")
#define AUTHDATA_KEY_STATUS(COMPACT)      (COMPACT ? "st" : "status")

#define AUTHORIZATIONSTATUS_LEN_MAX (sizeof("ConcurrentTx") - 1) //max length of serialized AuthStatus

const char *serializeAuthorizationStatus(AuthorizationStatus status);
AuthorizationStatus deserializeAuthorizationStatus(const char *cstr);

class AuthorizationData {
private:
    //data structure optimized for memory consumption

    std::unique_ptr<char[]> parentIdTag;
    std::unique_ptr<Timestamp> expiryDate;

    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};

    AuthorizationStatus status = AuthorizationStatus::UNDEFINED;
public:
    AuthorizationData();
    AuthorizationData(AuthorizationData&& other);
    ~AuthorizationData();

    AuthorizationData& operator=(AuthorizationData&& other);

    void readJson(JsonObject entry, bool compact = false); //compact: compressed representation for flash storage

    size_t getJsonCapacity() const;
    void writeJson(JsonObject& entry, bool compact = false); //compact: compressed representation for flash storage

    const char *getIdTag() const {return idTag;}
    Timestamp *getExpiryDate() const {return expiryDate.get();}
    const char *getParentIdTag() const {return parentIdTag.get();}
    AuthorizationStatus getAuthorizationStatus() const {return status;}

    void reset();
};

}

#endif
