// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationData.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

AuthorizationData::AuthorizationData() : MemoryManaged("v16.Authorization.AuthorizationData") {

}

AuthorizationData::AuthorizationData(AuthorizationData&& other) : MemoryManaged("v16.Authorization.AuthorizationData") {
    operator=(std::move(other));
}

AuthorizationData::~AuthorizationData() {
    reset();
}

AuthorizationData& AuthorizationData::operator=(AuthorizationData&& other) {
    parentIdTag = other.parentIdTag;
    other.parentIdTag = nullptr;
    expiryDate = other.expiryDate;
    other.expiryDate = nullptr;
    snprintf(idTag, sizeof(idTag), "%s", other.idTag);
    memset(other.idTag, 0, sizeof(other.idTag));
    status = other.status;
    return *this;
}

bool AuthorizationData::readJson(Clock& clock, JsonObject entry, bool internalFormat) {
    if (entry.containsKey(AUTHDATA_KEY_IDTAG(internalFormat))) {
        snprintf(idTag, sizeof(idTag), "%s", entry[AUTHDATA_KEY_IDTAG(internalFormat)].as<const char*>());
    } else {
        MO_DBG_ERR("format error");
        return false;
    }

    JsonObject idTagInfo;
    if (internalFormat){
        idTagInfo = entry;
    } else {
        idTagInfo = entry[AUTHDATA_KEY_IDTAGINFO];
    }

    delete expiryDate;
    expiryDate = nullptr;

    if (idTagInfo.containsKey(AUTHDATA_KEY_EXPIRYDATE(internalFormat))) {
        expiryDate = new Timestamp();
        if (!expiryDate) {
            MO_DBG_ERR("OOM");
            return false;
        }
        if (!clock.parseString(idTagInfo[AUTHDATA_KEY_EXPIRYDATE(internalFormat)], *expiryDate)) {
            MO_DBG_ERR("format error");
            return false;
        }
    }

    MO_FREE(parentIdTag);
    parentIdTag = nullptr;

    if (idTagInfo.containsKey(AUTHDATA_KEY_PARENTIDTAG(internalFormat))) {
        parentIdTag = static_cast<char*>(MO_MALLOC(getMemoryTag(), MO_IDTAG_LEN_MAX + 1));
        if (parentIdTag) {
            snprintf(parentIdTag, MO_IDTAG_LEN_MAX + 1, "%s", idTagInfo[AUTHDATA_KEY_PARENTIDTAG(internalFormat)].as<const char*>());
        } else {
            MO_DBG_ERR("OOM");
            return false;
        }
    }

    if (idTagInfo.containsKey(AUTHDATA_KEY_STATUS(internalFormat))) {
        status = deserializeAuthorizationStatus(idTagInfo[AUTHDATA_KEY_STATUS(internalFormat)]);
    } else {
        if (internalFormat) {
            status = AuthorizationStatus::Accepted;
        } else {
            status = AuthorizationStatus::UNDEFINED;
        }
    }

    if (status == AuthorizationStatus::UNDEFINED) {
        MO_DBG_ERR("format error");
        return false;
    }

    return true;
}

size_t AuthorizationData::getJsonCapacity(bool internalFormat) const {
    return JSON_OBJECT_SIZE(2) +
            (idTag[0] != '\0' ?
                JSON_OBJECT_SIZE(1) : 0) +
            (expiryDate ?
                JSON_OBJECT_SIZE(1) +
                (internalFormat ?
                    MO_INTERNALTIME_SIZE : MO_JSONDATE_SIZE)
                : 0) +
            (parentIdTag ?
                JSON_OBJECT_SIZE(1) : 0) +
            (status != AuthorizationStatus::UNDEFINED ?
                JSON_OBJECT_SIZE(1) : 0);
}

void AuthorizationData::writeJson(Clock& clock, JsonObject& entry, bool internalFormat) {
    if (idTag[0] != '\0') {
        entry[AUTHDATA_KEY_IDTAG(internalFormat)] = (const char*) idTag;
    }

    JsonObject idTagInfo;
    if (internalFormat) {
        idTagInfo = entry;
    } else {
        idTagInfo = entry.createNestedObject(AUTHDATA_KEY_IDTAGINFO);
    }

    if (expiryDate) {
        if (internalFormat) {
            char buf [MO_INTERNALTIME_SIZE];
            if (clock.toInternalString(*expiryDate, buf, sizeof(buf))) {
                idTagInfo[AUTHDATA_KEY_EXPIRYDATE(internalFormat)] = buf;
            }
        } else {
            char buf [MO_JSONDATE_SIZE];
            if (clock.toJsonString(*expiryDate, buf, sizeof(buf))) {
                idTagInfo[AUTHDATA_KEY_EXPIRYDATE(internalFormat)] = buf;
            }
        }
    }

    if (parentIdTag) {
        idTagInfo[AUTHDATA_KEY_PARENTIDTAG(internalFormat)] = (const char *) parentIdTag;
    }

    if (status != AuthorizationStatus::Accepted) {
        idTagInfo[AUTHDATA_KEY_STATUS(internalFormat)] = serializeAuthorizationStatus(status);
    } else if (!internalFormat) {
        idTagInfo[AUTHDATA_KEY_STATUS(internalFormat)] = serializeAuthorizationStatus(AuthorizationStatus::Invalid);
    }
}

const char *AuthorizationData::getIdTag() const {
    return idTag;
}
Timestamp *AuthorizationData::getExpiryDate() const {
    return expiryDate;
}
const char *AuthorizationData::getParentIdTag() const {
    return parentIdTag;
}
AuthorizationStatus AuthorizationData::getAuthorizationStatus() const {
    return status;
}

void AuthorizationData::reset() {
    delete expiryDate;
    expiryDate = nullptr;
    MO_FREE(parentIdTag);
    parentIdTag = nullptr;
    idTag[0] = '\0';
}

namespace MicroOcpp {
namespace v16 {

const char *serializeAuthorizationStatus(AuthorizationStatus status) {
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

AuthorizationStatus deserializeAuthorizationStatus(const char *cstr) {
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

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
