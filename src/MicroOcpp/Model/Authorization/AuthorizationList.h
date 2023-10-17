// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONLIST_H
#define AUTHORIZATIONLIST_H

#include <MicroOcpp/Model/Authorization/AuthorizationData.h>
#include <vector>

#ifndef MO_LocalAuthListMaxLength
#define MO_LocalAuthListMaxLength 48
#endif

#ifndef MO_SendLocalListMaxLength
#define MO_SendLocalListMaxLength MO_LocalAuthListMaxLength
#endif

namespace MicroOcpp {

class AuthorizationList {
private:
    int listVersion = 0;
    std::vector<AuthorizationData> localAuthorizationList; //sorted list
public:
    AuthorizationList();
    ~AuthorizationList();

    AuthorizationData *get(const char *idTag);

    bool readJson(JsonArray localAuthorizationList, int listVersion, bool differential = false, bool compact = false); //compact: if true, then use compact non-ocpp representation
    void clear();

    size_t getJsonCapacity();
    void writeJson(JsonArray authListOut, bool compact = false);

    int getListVersion() {return listVersion;}
    size_t size(); //used in unit tests

};

}

#endif
