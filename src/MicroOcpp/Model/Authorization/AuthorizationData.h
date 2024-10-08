// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZATIONDATA_H
#define MO_AUTHORIZATIONDATA_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
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

class AuthorizationData : public MemoryManaged {
private:
    //data structure optimized for memory consumption

    char *parentIdTag = nullptr;
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

    const char *getIdTag() const;
    Timestamp *getExpiryDate() const;
    const char *getParentIdTag() const;
    AuthorizationStatus getAuthorizationStatus() const;

    void reset();
};

}

#endif //MO_ENABLE_LOCAL_AUTH
#endif
