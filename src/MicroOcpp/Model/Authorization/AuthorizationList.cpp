// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>
#include <numeric>

using namespace MicroOcpp;

AuthorizationList::AuthorizationList() {

}

AuthorizationList::~AuthorizationList() {
    
}

MicroOcpp::AuthorizationData *AuthorizationList::get(const char *idTag) {
    //binary search

    if (!idTag) {
        return nullptr;
    }

    int l = 0;
    int r = ((int) localAuthorizationList.size()) - 1;
    while (l <= r) {
        auto m = (r + l) / 2;
        auto diff = strcmp(localAuthorizationList[m].getIdTag(), idTag);
        if (diff < 0) {
            l = m + 1;
        } else if (diff > 0) {
            r = m - 1;
        } else {
            return &localAuthorizationList[m];
        }
    }
    return nullptr;
}

bool AuthorizationList::readJson(JsonArray authlistJson, int listVersion, bool differential, bool compact) {

    if (compact) {
        //compact representations don't contain remove commands
        differential = false;
    }

    for (size_t i = 0; i < authlistJson.size(); i++) {

        //check if JSON object is valid
        if (!authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAG(compact))) {
            return false;
        }
    }

    std::vector<int> authlist_index;
    std::vector<int> remove_list;

    unsigned int resultingListLength = 0;

    if (!differential) {
        //every entry will insert an idTag
        resultingListLength = authlistJson.size();
    } else {
        //update type is differential; only unkown entries will insert an idTag

        resultingListLength = localAuthorizationList.size();

        //also, build index here
        authlist_index = std::vector<int>(authlistJson.size(), -1);

        for (size_t i = 0; i < authlistJson.size(); i++) {

            //check if locally stored auth info is present; if yes, apply it to the index
            AuthorizationData *found = get(authlistJson[i][AUTHDATA_KEY_IDTAG(compact)]);

            if (found) {

                authlist_index[i] = (int) (found - localAuthorizationList.data());

                //remove or update?
                if (!authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //this entry should be removed
                    found->reset(); //mark for deletion
                    remove_list.push_back((int) (found - localAuthorizationList.data()));
                    resultingListLength--;
                } //else: this entry should be updated
            } else {
                //insert or ignore?
                if (authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //add
                    resultingListLength++;
                } //else: ignore
            }
        }
    }

    if (resultingListLength > MO_LocalAuthListMaxLength) {
        MO_DBG_WARN("localAuthList capacity exceeded");
        return false;
    }

    //apply new list

    if (compact) {
        localAuthorizationList.clear();

        for (size_t i = 0; i < authlistJson.size(); i++) {
            localAuthorizationList.push_back(AuthorizationData());
            localAuthorizationList.back().readJson(authlistJson[i], compact);
        }
    } else if (differential) {

        for (size_t i = 0; i < authlistJson.size(); i++) {

            //is entry a remove command?
            if (!authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                continue; //yes, remove command, will be deleted afterwards
            }

            //update, or insert

            if (authlist_index[i] < 0) {
                //auth list does not contain idTag yet -> insert new entry

                //reuse removed AuthData object?
                if (!remove_list.empty()) {
                    //yes, reuse
                    authlist_index[i] = remove_list.back();
                    remove_list.pop_back();
                } else {
                    //no, create new
                    authlist_index[i] = localAuthorizationList.size();
                    localAuthorizationList.push_back(AuthorizationData());
                }
            }

            localAuthorizationList[authlist_index[i]].readJson(authlistJson[i], compact);
        }

    } else {
        localAuthorizationList.clear();

        for (size_t i = 0; i < authlistJson.size(); i++) {
            if (authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                localAuthorizationList.push_back(AuthorizationData());
                localAuthorizationList.back().readJson(authlistJson[i], compact);
            }
        }
    }

    localAuthorizationList.erase(std::remove_if(localAuthorizationList.begin(), localAuthorizationList.end(),
            [] (const AuthorizationData& elem) {
                return elem.getIdTag()[0] == '\0'; //"" means no idTag --> marked for removal
            }), localAuthorizationList.end());

    std::sort(localAuthorizationList.begin(), localAuthorizationList.end(),
            [] (const AuthorizationData& lhs, const AuthorizationData& rhs) {
                return strcmp(lhs.getIdTag(), rhs.getIdTag()) < 0;
            });
    
    this->listVersion = listVersion;

    if (localAuthorizationList.empty()) {
        this->listVersion = 0;
    }

    return true;
}

void AuthorizationList::clear() {
    localAuthorizationList.clear();
    listVersion = 0;
}

size_t AuthorizationList::getJsonCapacity() {
    size_t res = JSON_ARRAY_SIZE(localAuthorizationList.size());
    for (auto& entry : localAuthorizationList) {
        res += entry.getJsonCapacity();
    }
    return res;
}

void AuthorizationList::writeJson(JsonArray authListOut, bool compact) {
    for (auto& entry : localAuthorizationList) {
        JsonObject entryJson = authListOut.createNestedObject();
        entry.writeJson(entryJson, compact);
    }
}

size_t AuthorizationList::size() {
    return localAuthorizationList.size();
}
