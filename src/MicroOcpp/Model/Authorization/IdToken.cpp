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

const char *IdToken::get() {
    return *idToken ? idToken : nullptr;;
}

const char *IdToken::getTypeCstr() {
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

#endif // MO_ENABLE_V201
