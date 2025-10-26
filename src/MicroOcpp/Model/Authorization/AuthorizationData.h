// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZATIONDATA_H
#define MO_AUTHORIZATIONDATA_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Version.h>

#include <ArduinoJson.h>
#include <memory>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

namespace MicroOcpp {
namespace v16 {

enum class AuthorizationStatus : uint8_t {
    Accepted,
    Blocked,
    Expired,
    Invalid,
    ConcurrentTx,
    UNDEFINED //not part of OCPP 1.6
};

#define AUTHDATA_KEY_IDTAG(INTERNAL_FORMAT)       (INTERNAL_FORMAT ? "it" : "idTag")
#define AUTHDATA_KEY_IDTAGINFO                    "idTagInfo"
#define AUTHDATA_KEY_EXPIRYDATE(INTERNAL_FORMAT)  (INTERNAL_FORMAT ? "ed" : "expiryDate")
#define AUTHDATA_KEY_PARENTIDTAG(INTERNAL_FORMAT) (INTERNAL_FORMAT ? "pi" : "parentIdTag")
#define AUTHDATA_KEY_STATUS(INTERNAL_FORMAT)      (INTERNAL_FORMAT ? "st" : "status")

const char *serializeAuthorizationStatus(AuthorizationStatus status);
AuthorizationStatus deserializeAuthorizationStatus(const char *cstr);

class AuthorizationData : public MemoryManaged {
private:
    char *parentIdTag = nullptr;
    Timestamp *expiryDate = nullptr; //has ownership

    char idTag [MO_IDTAG_LEN_MAX + 1] = {'\0'};

    AuthorizationStatus status = AuthorizationStatus::UNDEFINED;
public:
    AuthorizationData();
    AuthorizationData(AuthorizationData&& other);
    ~AuthorizationData();

    AuthorizationData& operator=(AuthorizationData&& other);

    bool readJson(Clock& clock, JsonObject entry, bool internalFormat); //internalFormat: compressed representation for flash storage

    size_t getJsonCapacity(bool internalFormat) const;
    void writeJson(Clock& clock, JsonObject& entry, bool internalFormat); //internalFormat: compressed representation for flash storage

    const char *getIdTag() const;
    Timestamp *getExpiryDate() const;
    const char *getParentIdTag() const;
    AuthorizationStatus getAuthorizationStatus() const;

    void reset();
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
#endif
