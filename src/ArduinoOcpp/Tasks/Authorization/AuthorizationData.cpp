// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Authorization/AuthorizationData.h>

#define AUTHDATA_KEY_IDTAG       "it"
#define AUTHDATA_KEY_EXPIRYDATE  "ed"
#define AUTHDATA_KEY_PARENTIDTAG "pi"
#define AUTHDATA_KEY_STATUS      "st"

using ArduinoOcpp::AuthorizationData;

AuthorizationData::AuthorizationData() {

}

AuthorizationData::~AuthorizationData() {

}

void AuthorizationData::readJson(JsonObject entry) {
    if (entry.containsKey(AUTHDATA_KEY_IDTAG)) {
        strncpy(idTag, entry[AUTHDATA_KEY_IDTAG], IDTAG_LEN_MAX + 1);
        idTag[IDTAG_LEN_MAX] = '\0';
    }

    if (entry.containsKey(AUTHDATA_KEY_EXPIRYDATE)) {
        expiryDate = std::unique_ptr<OcppTimestamp>(new OcppTimestamp());
        if (!expiryDate->setTime(entry[AUTHDATA_KEY_EXPIRYDATE])) {
            expiryDate.reset();
        }
    }

    if (entry.containsKey(AUTHDATA_KEY_PARENTIDTAG)) {
        parentIdTag = std::unique_ptr<char[]>(new char[IDTAG_LEN_MAX + 1]);
        strncpy(parentIdTag.get(), entry[AUTHDATA_KEY_PARENTIDTAG], IDTAG_LEN_MAX + 1);
        parentIdTag.get()[IDTAG_LEN_MAX] = '\0';
    }

    if (entry.containsKey(AUTHDATA_KEY_STATUS)) {
        status = deserializeAuthorizationStatus(entry[AUTHDATA_KEY_STATUS]);
    }
}

size_t AuthorizationData::getJsonCapacity() {
    return JSON_OBJECT_SIZE(1) +
            (idTag[0] != '\0' ? 
                JSON_OBJECT_SIZE(1) : 0) +
            (expiryDate ?
                JSON_OBJECT_SIZE(1) + JSONDATE_LENGTH + 1 : 0) +
            (parentIdTag ?
                JSON_OBJECT_SIZE(1) : 0) +
            (status != AuthorizationStatus::UNDEFINED ?
                JSON_OBJECT_SIZE(1) : 0);
}

void AuthorizationData::writeJson(JsonObject& entry) {
    if (idTag[0] != '\0') {
        entry[AUTHDATA_KEY_IDTAG] = (const char*) idTag;
    }

    if (expiryDate) {
        char buf [JSONDATE_LENGTH + 1];
        if (expiryDate->toJsonString(buf, JSONDATE_LENGTH + 1)) {
            entry[AUTHDATA_KEY_EXPIRYDATE] = buf;
        }
    }

    if (parentIdTag) {
        entry[AUTHDATA_KEY_PARENTIDTAG] = (const char *) parentIdTag.get();
    }

    if (status != AuthorizationStatus::UNDEFINED) {
        entry[AUTHDATA_KEY_STATUS] = serializeAuthorizationStatus(status);
    }
}

const char *ArduinoOcpp::serializeAuthorizationStatus(AuthorizationStatus status) {
    switch (status) {
        case (AuthorizationStatus::Accepted):
            return "Accepted";
        case (AuthorizationStatus::Blocked):
            return "Blocked";
        case (AuthorizationStatus::Expired):
            return "Expired";
        case (AuthorizationStatus::Invalid):
            return "Invalid";
        case (AuthorizationStatus::ConcurrentTx):
            return "ConcurrentTx";
        default:
            return "UNDEFINED";
    }
}

ArduinoOcpp::AuthorizationStatus ArduinoOcpp::deserializeAuthorizationStatus(const char *cstr) {
    if (!cstr) {
        return AuthorizationStatus::UNDEFINED;
    }

    if (!strcmp(cstr, "Accepted")) {
        return AuthorizationStatus::Accepted;
    } else if (!strcmp(cstr, "Blocked")) {
        return AuthorizationStatus::Blocked;
    } else if (!strcmp(cstr, "Expired")) {
        return AuthorizationStatus::Expired;
    } else if (!strcmp(cstr, "Invalid")) {
        return AuthorizationStatus::Invalid;
    } else if (!strcmp(cstr, "ConcurrentTx")) {
        return AuthorizationStatus::ConcurrentTx;
    } else {
        return AuthorizationStatus::UNDEFINED;
    }
}
