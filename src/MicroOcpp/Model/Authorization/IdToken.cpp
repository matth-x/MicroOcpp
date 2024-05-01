// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Authorization/IdToken.h>

#include <string.h>
#include <stdio.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

IdToken::IdToken() {
    idToken[0] = '\0';
}

IdToken::IdToken(const char *token, Type type) : type(type) {
    auto ret = snprintf(idToken, MO_IDTOKEN_LEN_MAX + 1, "%s", token);
    if (ret < 0 || ret >= MO_IDTOKEN_LEN_MAX + 1) {
        MO_DBG_ERR("invalid token");
        *idToken = '\0';
    }
}

bool IdToken::parseCstr(const char *token, const char *typeCstr) {
    if (!token || !typeCstr) {
        MO_DBG_DEBUG("TRACE");
        return false;
    }

    if (!strcmp(typeCstr, "Central")) {
        type = Type::Central;
    } else if (!strcmp(typeCstr, "eMAID")) {
        type = Type::eMAID;
    } else if (!strcmp(typeCstr, "ISO14443")) {
        type = Type::ISO14443;
    } else if (!strcmp(typeCstr, "ISO15693")) {
        type = Type::ISO15693;
    } else if (!strcmp(typeCstr, "KeyCode")) {
        MO_DBG_DEBUG("TRACE");
        type = Type::KeyCode;
    } else if (!strcmp(typeCstr, "Local")) {
        type = Type::Local;
    } else if (!strcmp(typeCstr, "MacAddress")) {
        type = Type::MacAddress;
    } else if (!strcmp(typeCstr, "NoAuthorization")) {
        type = Type::NoAuthorization;
    } else {
        MO_DBG_DEBUG("TRACE");
        return false;
    }

    auto ret = snprintf(idToken, sizeof(idToken), "%s", token);
    if (ret < 0 || (size_t)ret >= sizeof(idToken)) {
        return false;
    }

    return true;
}

const char *IdToken::get() const {
    return *idToken ? idToken : nullptr;;
}

const char *IdToken::getTypeCstr() const {
    const char *res = nullptr;
    switch (type) {
        case Type::Central:
            res = "Central";
            break;
        case Type::eMAID:
            res = "eMAID";
            break;
        case Type::ISO14443:
            res = "ISO14443";
            break;
        case Type::ISO15693:
            res = "ISO15693";
            break;
        case Type::KeyCode:
            res = "KeyCode";
            break;
        case Type::Local:
            res = "Local";
            break;
        case Type::MacAddress:
            res = "MacAddress";
            break;
        case Type::NoAuthorization:
            res = "NoAuthorization";
            break;
    }

    if (!res) {
        MO_DBG_ERR("internal error");
    }
    return res ? res : "";
}

bool IdToken::equals(const IdToken& other) {
    return type == other.type && !strcmp(idToken, other.idToken);
}

#endif // MO_ENABLE_V201
