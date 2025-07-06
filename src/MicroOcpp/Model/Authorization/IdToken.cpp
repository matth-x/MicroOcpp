// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <stdio.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

IdToken::IdToken(const char *token, MO_IdTokenType type, const char *memoryTag) : MemoryManaged(memoryTag ? memoryTag : "v201.Authorization.IdToken"), type(type) {
    if (token) {
        auto ret = snprintf(idToken, MO_IDTOKEN_LEN_MAX + 1, "%s", token);
        if (ret < 0 || ret >= MO_IDTOKEN_LEN_MAX + 1) {
            MO_DBG_ERR("invalid token");
            *idToken = '\0';
        }
    } else {
        *idToken = '\0';
    }
}

IdToken::IdToken(const IdToken& other, const char *memoryTag) : IdToken(other.idToken, other.type, memoryTag ? memoryTag : other.getMemoryTag()) {

}

bool IdToken::parseCstr(const char *token, const char *typeCstr) {
    if (!token || !typeCstr) {
        return false;
    }

    if (!strcmp(typeCstr, "Central")) {
        type = MO_IdTokenType_Central;
    } else if (!strcmp(typeCstr, "eMAID")) {
        type = MO_IdTokenType_eMAID;
    } else if (!strcmp(typeCstr, "ISO14443")) {
        type = MO_IdTokenType_ISO14443;
    } else if (!strcmp(typeCstr, "ISO15693")) {
        type = MO_IdTokenType_ISO15693;
    } else if (!strcmp(typeCstr, "KeyCode")) {
        type = MO_IdTokenType_KeyCode;
    } else if (!strcmp(typeCstr, "Local")) {
        type = MO_IdTokenType_Local;
    } else if (!strcmp(typeCstr, "MacAddress")) {
        type = MO_IdTokenType_MacAddress;
    } else if (!strcmp(typeCstr, "NoAuthorization")) {
        type = MO_IdTokenType_NoAuthorization;
    } else {
        return false;
    }

    auto ret = snprintf(idToken, sizeof(idToken), "%s", token);
    if (ret < 0 || (size_t)ret >= sizeof(idToken)) {
        return false;
    }

    return true;
}

const char *IdToken::get() const {
    return idToken;
}

MO_IdTokenType IdToken::getType() const {
    return type;
}

const char *IdToken::getTypeCstr() const {
    const char *res = "";
    switch (type) {
        case MO_IdTokenType_UNDEFINED:
            MO_DBG_ERR("internal error");
            break;
        case MO_IdTokenType_Central:
            res = "Central";
            break;
        case MO_IdTokenType_eMAID:
            res = "eMAID";
            break;
        case MO_IdTokenType_ISO14443:
            res = "ISO14443";
            break;
        case MO_IdTokenType_ISO15693:
            res = "ISO15693";
            break;
        case MO_IdTokenType_KeyCode:
            res = "KeyCode";
            break;
        case MO_IdTokenType_Local:
            res = "Local";
            break;
        case MO_IdTokenType_MacAddress:
            res = "MacAddress";
            break;
        case MO_IdTokenType_NoAuthorization:
            res = "NoAuthorization";
            break;
    }

    return res;
}

bool IdToken::equals(const IdToken& other) {
    return type == other.type && !strcmp(idToken, other.idToken);
}

#endif //MO_ENABLE_V201
