// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>
#include <numeric>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

AuthorizationList::AuthorizationList() : MemoryManaged("v16.Authorization.AuthorizationList") {

}

AuthorizationList::~AuthorizationList() {
    clear();
}

MicroOcpp::v16::AuthorizationData *AuthorizationList::get(const char *idTag) {

    if (!idTag) {
        return nullptr;
    }

    //binary search
    int l = 0;
    int r = ((int) localAuthorizationListSize) - 1;
    while (l <= r) {
        auto m = (r + l) / 2;
        auto diff = strcmp(localAuthorizationList[m]->getIdTag(), idTag);
        if (diff < 0) {
            l = m + 1;
        } else if (diff > 0) {
            r = m - 1;
        } else {
            return localAuthorizationList[m];
        }
    }
    return nullptr;
}

bool AuthorizationList::readJson(Clock& clock, JsonArray authlistJson, int listVersion, bool differential, bool internalFormat) {

    if (internalFormat) {
        //compact representations don't contain remove commands
        differential = false;
    }

    size_t resListSize = 0;
    size_t updateSize = 0;

    if (!differential) {
        //every entry will insert an idTag
        resListSize = authlistJson.size();
        updateSize = authlistJson.size();
    } else {
        //update type is differential; only unkown entries will insert an idTag

        resListSize = localAuthorizationListSize;

        for (size_t i = 0; i < authlistJson.size(); i++) {

            //check if locally stored auth info is present; if yes, apply it to the index
            AuthorizationData *found = get(authlistJson[i][AUTHDATA_KEY_IDTAG(internalFormat)]);

            if (found) {

                //remove or update?
                if (!authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //this entry should be removed
                    resListSize--;
                } else {
                    //this entry should be updated
                    updateSize++;
                }
            } else {
                //insert or ignore?
                if (authlistJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //add
                    resListSize++;
                    updateSize++;
                } else {
                    // invalid record
                    return false;
                }
            }
        }
    }

    if (resListSize > MO_LocalAuthListMaxLength) {
        MO_DBG_WARN("localAuthList capacity exceeded");
        return false;
    }


    AuthorizationData **updateList = nullptr; //list of newly allocated entries
    AuthorizationData **resList = nullptr; //resulting list after list update. Contains pointers to old auth list and updateList

    size_t updateWritten = 0;
    size_t resWritten = 0;

    if (updateSize > 0) {
        updateList = static_cast<AuthorizationData**>(MO_MALLOC(getMemoryTag(), sizeof(AuthorizationData*) * updateSize));
        if (!updateList) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
        memset(updateList, 0, sizeof(AuthorizationData*) * updateSize);
    }

    if (resListSize > 0) {
        resList = static_cast<AuthorizationData**>(MO_MALLOC(getMemoryTag(), sizeof(AuthorizationData*) * resListSize));
        if (!resList) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
        memset(resList, 0, sizeof(AuthorizationData*) * resListSize);
    }

    // Keep, update, or remove old entries
    if (differential) {
        for (size_t i = 0; i < localAuthorizationListSize; i++) {

            bool remove = false;
            bool update = false;

            for (size_t j = 0; j < authlistJson.size(); j++) {
                //remove or update?
                const char *key_i = localAuthorizationList[i]->getIdTag();
                const char *key_j = authlistJson[j][AUTHDATA_KEY_IDTAG(false)] | "";
                if (!*key_i || !*key_j || strcmp(key_i, key_j)) {
                    // Keys don't match
                    continue;
                }
                if (authlistJson[j].containsKey(AUTHDATA_KEY_IDTAGINFO)) {
                    //this entry should be updated
                    update = true;
                    break;
                } else {
                    //this entry should be removed
                    remove = true;
                    break;
                }
            }

            if (remove) {
                // resList won't include this entry
                (void)0;
            } else if (update) {
                updateList[updateWritten] = new AuthorizationData();
                if (!updateList[updateWritten]) {
                    MO_DBG_ERR("OOM");
                    goto fail;
                }
                if (!updateList[updateWritten]->readJson(clock, authlistJson[i], internalFormat)) {
                    MO_DBG_ERR("format error");
                    goto fail;
                }
                resList[resWritten] = updateList[updateWritten];
                updateWritten++;
                resWritten++;
            } else {
                resList[resWritten] = localAuthorizationList[i];
                resWritten++;
            }
        }
    }

    // Insert new entries
    for (size_t i = 0; i < authlistJson.size(); i++) {

        if (!internalFormat && !authlistJson[i].containsKey(AUTHDATA_KEY_IDTAGINFO)) {
            // remove already handled above
            continue;
        }

        bool insert = true;

        const char *key_i = authlistJson[i][AUTHDATA_KEY_IDTAG(false)] | "";

        for (size_t j = 0; j < resWritten; j++) {
            //insert?
            const char *key_j = resList[j]->getIdTag();
            if (*key_i && key_j && !strcmp(key_i, key_j)) {
                // Keys match, this entry was updated above, and should not be inserted
                insert = false;
                break;
            }
        }

        if (insert) {
            updateList[updateWritten] = new AuthorizationData();
            if (!updateList[updateWritten]) {
                MO_DBG_ERR("OOM");
                goto fail;
            }
            if (!updateList[updateWritten]->readJson(clock, authlistJson[i], internalFormat)) {
                MO_DBG_ERR("format error");
                goto fail;
            }
            resList[resWritten] = updateList[updateWritten];
            updateWritten++;
            resWritten++;
        }
    }

    // sanity check 1
    if (resWritten != resListSize) {
        MO_DBG_ERR("internal error");
        goto fail;
    }

    // sanity check 2
    for (size_t i = 0; i < resListSize; i++) {
        if (!resList[i]) {
            MO_DBG_ERR("internal error");
            goto fail;
        }
    }

    qsort(resList, resListSize, sizeof(resList[0]),
        [] (const void* a,const void* b) -> int {
            return strcmp(
                (*reinterpret_cast<const AuthorizationData* const *>(a))->getIdTag(),
                (*reinterpret_cast<const AuthorizationData* const *>(b))->getIdTag());
        });

    // success

    this->listVersion = listVersion;

    if (resListSize == 0) {
        this->listVersion = 0;
    }

    for (size_t i = 0; i < localAuthorizationListSize; i++) {
        bool found = false;
        for (size_t j = 0; j < resListSize; j++) {
            if (localAuthorizationList[i] == resList[j]) {
                found = true;
                break;
            }
        }

        if (!found) {
            //entry not used anymore
            delete localAuthorizationList[i];
            localAuthorizationList[i] = nullptr;
        }
    }
    MO_FREE(localAuthorizationList);
    localAuthorizationList = nullptr;
    localAuthorizationListSize = 0;

    localAuthorizationList = resList;
    localAuthorizationListSize = resListSize;

    MO_FREE(updateList);
    updateList = nullptr;
    updateSize = 0;

    return true;
fail:
    if (updateList) {
        for (size_t i = 0; i < updateSize; i++) {
            delete updateList[i];
            updateList[i] = nullptr;
        }
    }
    MO_FREE(updateList);
    updateList = nullptr;

    MO_FREE(resList);
    resList = nullptr;
    resListSize = 0;

    return false;
}

void AuthorizationList::clear() {
    for (size_t i = 0; i < localAuthorizationListSize; i++) {
        delete localAuthorizationList[i];
        localAuthorizationList[i] = nullptr;
    }
    MO_FREE(localAuthorizationList);
    localAuthorizationList = nullptr;
    localAuthorizationListSize = 0;
    listVersion = 0;
}

size_t AuthorizationList::getJsonCapacity(bool internalFormat) {
    size_t res = JSON_ARRAY_SIZE(localAuthorizationListSize);
    for (size_t i = 0; i < localAuthorizationListSize; i++) {
        res += localAuthorizationList[i]->getJsonCapacity(internalFormat);
    }
    return res;
}

void AuthorizationList::writeJson(Clock& clock, JsonArray authListOut, bool internalFormat) {
    for (size_t i = 0; i < localAuthorizationListSize; i++) {
        JsonObject entryJson = authListOut.createNestedObject();
        localAuthorizationList[i]->writeJson(clock, entryJson, internalFormat);
    }
}

size_t AuthorizationList::size() {
    return localAuthorizationListSize;
}

#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
