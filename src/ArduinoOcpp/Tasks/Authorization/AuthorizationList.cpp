// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Authorization/AuthorizationList.h>
#include <ArduinoOcpp/Debug.h>

#include <algorithm>
#include <numeric>

using namespace ArduinoOcpp;

AuthorizationList::AuthorizationList() {

}

AuthorizationList::~AuthorizationList() {
    
}

ArduinoOcpp::AuthorizationData *AuthorizationList::get(const char *idTag) {
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

bool AuthorizationList::readJson(JsonObject payload, bool compact) {

    bool differential = !strcmp("Differential", payload["updateType"] | "Invalid"); //if Json also contains delete commands

    if (compact) {
        //compact representations don't contain remove commands
        differential = false;
    }

    JsonArray collection = payload["localAuthorizationList"];

    for (int i = 0; i < collection.size(); i++) {

        //check if JSON object is valid
        if (!collection[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAG(compact))) {
            return false;
        }
    }

    std::vector<int> collection_index;
    std::vector<int> remove_list;

    unsigned int resultingListLength = 0;

    if (!differential) {
        //every entry will insert an idTag
        resultingListLength = collection.size();
    } else {
        //update type is differential; only unkown entries will insert an idTag

        resultingListLength = localAuthorizationList.size();

        //also, build index here
        collection_index = std::vector<int>(collection.size(), -1);

        for (int i = 0; i < collection.size(); i++) {

            //check if locally stored auth info is present; if yes, apply it to the index
            AuthorizationData *found = get(collection[i][AUTHDATA_KEY_IDTAG(compact)]);

            if (found) {

                collection_index[i] = (int) (found - localAuthorizationList.data());

                //remove or update?
                if (!collection[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //this entry should be removed
                    found->reset(); //mark for deletion
                    remove_list.push_back((int) (found - localAuthorizationList.data()));
                    resultingListLength--;
                } //else: this entry should be updated
            } else {
                //insert or ignore?
                if (collection[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //add
                    resultingListLength++;
                } //else: ignore
            }
        }
    }

    if (resultingListLength > AO_LocalAuthListMaxLength) {
        AO_DBG_WARN("localAuthList capacity exceeded");
        return false;
    }

    //apply new list
    listVersion = payload["listVersion"] | -1;

    if (compact) {
        localAuthorizationList.clear();

        for (int i = 0; i < collection.size(); i++) {
            localAuthorizationList.push_back(AuthorizationData());
            localAuthorizationList.back().readJson(collection[i], compact);
        }
    } else if (differential) {

        for (int i = 0; i < collection.size(); i++) {

            //is entry a remove command?
            if (!collection[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                continue; //yes, remove command, will be deleted afterwards
            }

            //update, or insert

            if (collection_index[i] < 0) {
                //auth list does not contain idTag yet -> insert new entry

                //reuse removed AuthData object?
                if (!remove_list.empty()) {
                    //yes, reuse
                    collection_index[i] = remove_list.back();
                    remove_list.pop_back();
                } else {
                    //no, create new
                    collection_index[i] = localAuthorizationList.size();
                    localAuthorizationList.push_back(AuthorizationData());
                }
            }

            localAuthorizationList[collection_index[i]].readJson(collection[i], compact);
        }

    } else {
        localAuthorizationList.clear();

        for (int i = 0; i < collection.size(); i++) {
            if (collection[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                localAuthorizationList.push_back(AuthorizationData());
                localAuthorizationList.back().readJson(collection[i], compact);
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
    
    return true;
}

void AuthorizationList::clear() {
    localAuthorizationList.clear();
}

size_t AuthorizationList::getJsonCapacity(bool compact) {
    return JSON_OBJECT_SIZE(3) +
           JSON_ARRAY_SIZE(localAuthorizationList.size()) +
            std::accumulate(localAuthorizationList.begin(), localAuthorizationList.end(), 0,
                    [] (const int& acc, const AuthorizationData& v) {
                        return acc + (int) v.getJsonCapacity();
                    } );
}

void AuthorizationList::writeJson(JsonObject& payload, bool compact) {
    payload["listVersion"] = listVersion;

    JsonArray collection = payload.createNestedArray("localAuthorizationList");

    for (auto& entry : localAuthorizationList) {
        JsonObject entryJson = collection.createNestedObject();
        entry.writeJson(entryJson, compact);
    }
}
