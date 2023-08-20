// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationData.h>

using namespace MicroOcpp;

AuthorizationData::AuthorizationData() {

}

AuthorizationData::AuthorizationData(AuthorizationData&& other) {
    operator=(std::move(other));
}

AuthorizationData::~AuthorizationData() {

}

AuthorizationData& AuthorizationData::operator=(AuthorizationData&& other) {
    parentIdTag = std::move(other.parentIdTag);
    expiryDate = std::move(other.expiryDate);
    strncpy(idTag, other.idTag, IDTAG_LEN_MAX + 1);
    idTag[IDTAG_LEN_MAX] = '\0';
    status = other.status;
    return *this;
}

void AuthorizationData::readJson(JsonObject entry, bool compact) {
    if (entry.containsKey(AUTHDATA_KEY_IDTAG(compact))) {
        strncpy(idTag, entry[AUTHDATA_KEY_IDTAG(compact)], IDTAG_LEN_MAX + 1);
        idTag[IDTAG_LEN_MAX] = '\0';
    } else {
        idTag[0] = '\0';
    }

    JsonObject idTagInfo;
    if (compact){
        idTagInfo = entry;
    } else {
        idTagInfo = entry[AUTHDATA_KEY_IDTAGINFO];
    }

    if (idTagInfo.containsKey(AUTHDATA_KEY_EXPIRYDATE(compact))) {
        expiryDate = std::unique_ptr<Timestamp>(new Timestamp());
        if (!expiryDate->setTime(idTagInfo[AUTHDATA_KEY_EXPIRYDATE(compact)])) {
            expiryDate.reset();
        }
    } else {
        expiryDate.reset();
    }

    if (idTagInfo.containsKey(AUTHDATA_KEY_PARENTIDTAG(compact))) {
        parentIdTag = std::unique_ptr<char[]>(new char[IDTAG_LEN_MAX + 1]);
        strncpy(parentIdTag.get(), idTagInfo[AUTHDATA_KEY_PARENTIDTAG(compact)], IDTAG_LEN_MAX + 1);
        parentIdTag.get()[IDTAG_LEN_MAX] = '\0';
    } else {
        parentIdTag.reset();
    }

    if (idTagInfo.containsKey(AUTHDATA_KEY_STATUS(compact))) {
        status = deserializeAuthorizationStatus(idTagInfo[AUTHDATA_KEY_STATUS(compact)]);
    } else {
        if (compact) {
            status = AuthorizationStatus::Accepted;
        } else {
            status = AuthorizationStatus::UNDEFINED;
        }
    }
}

size_t AuthorizationData::getJsonCapacity() const {
    return JSON_OBJECT_SIZE(2) +
            (idTag[0] != '\0' ? 
                JSON_OBJECT_SIZE(1) : 0) +
            (expiryDate ?
                JSON_OBJECT_SIZE(1) + JSONDATE_LENGTH + 1 : 0) +
            (parentIdTag ?
                JSON_OBJECT_SIZE(1) : 0) +
            (status != AuthorizationStatus::UNDEFINED ?
                JSON_OBJECT_SIZE(1) : 0);
}

void AuthorizationData::writeJson(JsonObject& entry, bool compact) {
    if (idTag[0] != '\0') {
        entry[AUTHDATA_KEY_IDTAG(compact)] = (const char*) idTag;
    }

    JsonObject idTagInfo;
    if (compact) {
        idTagInfo = entry;
    } else {
        idTagInfo = entry.createNestedObject(AUTHDATA_KEY_IDTAGINFO);
    }

    if (expiryDate) {
        char buf [JSONDATE_LENGTH + 1];
        if (expiryDate->toJsonString(buf, JSONDATE_LENGTH + 1)) {
            idTagInfo[AUTHDATA_KEY_EXPIRYDATE(compact)] = buf;
        }
    }

    if (parentIdTag) {
        idTagInfo[AUTHDATA_KEY_PARENTIDTAG(compact)] = (const char *) parentIdTag.get();
    }

    if (status != AuthorizationStatus::Accepted) {
        idTagInfo[AUTHDATA_KEY_STATUS(compact)] = serializeAuthorizationStatus(status);
    } else if (!compact) {
        idTagInfo[AUTHDATA_KEY_STATUS(compact)] = serializeAuthorizationStatus(AuthorizationStatus::Invalid);
    }
}

void AuthorizationData::reset() {
    idTag[0] = '\0';
}

const char *MicroOcpp::serializeAuthorizationStatus(AuthorizationStatus status) {
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

MicroOcpp::AuthorizationStatus MicroOcpp::deserializeAuthorizationStatus(const char *cstr) {
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
