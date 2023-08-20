// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONLIST_H
#define AUTHORIZATIONLIST_H

#include <ArduinoOcpp/Model/Authorization/AuthorizationData.h>
#include <vector>

#ifndef AO_LocalAuthListMaxLength
#define AO_LocalAuthListMaxLength 48
#endif

#define AO_SendLocalListMaxLength AO_LocalAuthListMaxLength

namespace ArduinoOcpp {

class AuthorizationList {
private:
    int listVersion = 0;
    std::vector<AuthorizationData> localAuthorizationList; //sorted list
public:
    AuthorizationList();
    ~AuthorizationList();

    AuthorizationData *get(const char *idTag);

    bool readJson(JsonObject payload, bool compact = false); //compact: if true, then use compact non-ocpp representation
    void clear();

    size_t getJsonCapacity(bool compact = false);
    void writeJson(JsonObject& entry, bool compact = false);

    int getListVersion() {return listVersion;}

};

}

#endif
