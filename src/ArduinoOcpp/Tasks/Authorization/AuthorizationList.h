// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONLIST_H
#define AUTHORIZATIONLIST_H

#include <ArduinoOcpp/Tasks/Authorization/AuthorizationData.h>
#include <vector>

namespace ArduinoOcpp {

class AuthorizationList {
private:
    int localAuthListVersion = -1; //version number if list is LocalAuthorizationList
    std::vector<std::unique_ptr<AuthorizationData>> authData; //sorted list

    uint16_t updateCnt = 0; //used for temporal order of authData elements

    const size_t MAX_SIZE;
private:
    AuthorizationList(size_t MAX_SIZE);
    ~AuthorizationList();

    void readJson(JsonObject entry);

    size_t getJsonCapacity();
    void writeJson(JsonObject& entry);

    void update(JsonArray collection);
    void update(std::unique_ptr<AuthorizationData> entry);
    void clear();
};

}

#endif
