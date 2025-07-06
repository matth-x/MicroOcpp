// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/CustomOperation.h>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

namespace MicroOcpp {

std::unique_ptr<MicroOcpp::JsonDoc> makeDeserializedJson(const char *memoryTag, const char *jsonString) {
    std::unique_ptr<MicroOcpp::JsonDoc> doc;

    size_t capacity = MO_MAX_JSON_CAPACITY / 8;
    DeserializationError err = DeserializationError::NoMemory;

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = makeJsonDoc(memoryTag, capacity);
        if (!doc) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        err = deserializeJson(*doc, jsonString);

        capacity *= 2;
    }

    if (err) {
        MO_DBG_ERR("JSON deserialization error: %s", err.c_str());
        return nullptr;
    }

    return doc;
}

char *makeSerializedJsonString(const char *memoryTag, JsonObject json) {
    char *str = nullptr;

    size_t capacity = MO_MAX_JSON_CAPACITY / 8;

    while (!str && capacity <= MO_MAX_JSON_CAPACITY) {
        str = static_cast<char*>(MO_MALLOC(memoryTag, capacity));
        if (!str) {
            MO_DBG_ERR("OOM");
            break;
        }
        auto written = serializeJson(json, str, capacity);

        if (written >= 2 && written < capacity) {
            //success
            break;
        }

        MO_FREE(str);
        capacity *= 2;

        if (written <= 1) {
            MO_DBG_ERR("serialization error");
            break; //don't retry
        }
    }

    if (!str) {
        MO_DBG_ERR("could not serialize JSON");
    }

    return str;
}

void freeSerializedJsonString(char *str) {
    MO_FREE(str);
}

} //namespace MicroOcpp

CustomOperation::CustomOperation() : MemoryManaged("v16/v201.CustomOperation") {

}

CustomOperation::~CustomOperation() {
    if (finally) {
        finally(operationType, userStatus, userData);
        finally = nullptr;
    }
    MO_FREE(request);
    request = nullptr;
}

bool CustomOperation::setupEvseInitiated(const char *operationType, const char *request, void (*onResponse)(const char *payloadJson, void *userData), void *userData) {
    this->operationType = operationType;
    MO_FREE(this->request);
    this->request = nullptr;
    if (request) {
        size_t size = strlen(request) + 1;
        this->request = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
        if (!this->request) {
            MO_DBG_ERR("OOM");
            return false;
        }
        int ret = snprintf(this->request, size, "%s", request);
        if (ret < 0 || (size_t)ret >= size) {
            MO_DBG_ERR("snprintf: %i", ret);
            MO_FREE(this->request);
            return false;
        }
    }
    this->onResponse = onResponse;
    this->userData = userData;

    return true;
}

//for operations receied from remote
bool CustomOperation::setupCsmsInitiated(const char *operationType, void (*onRequest)(const char *operationType, const char *payloadJson, void **userStatus, void *userData), int (*writeResponse)(const char *operationType, char *buf, size_t size, void *userStatus, void *userData), void (*finally)(const char *operationType, void *userStatus, void *userData), void *userData) {
    this->operationType = operationType;
    this->onRequest = onRequest;
    this->writeResponse = writeResponse;
    this->userData = userData;
    this->finally = finally;
    return true;
}

const char *CustomOperation::getOperationType() {
    return operationType;
}

std::unique_ptr<MicroOcpp::JsonDoc> CustomOperation::createReq() {
    return makeDeserializedJson(getMemoryTag(), request ? request : "{}");
}

void CustomOperation::processConf(JsonObject payload) {
    if (!onResponse) {
        return;
    }
    char *jsonString = makeSerializedJsonString(getMemoryTag(), payload);
    if (!jsonString) {
        MO_DBG_ERR("serialization error");
        return;
    }
    onResponse(jsonString, userData);

    freeSerializedJsonString(jsonString);
}

void CustomOperation::processReq(JsonObject payload) {
    if (!onRequest) {
        return;
    }
    char *jsonString = makeSerializedJsonString(getMemoryTag(), payload);
    if (!jsonString) {
        MO_DBG_ERR("serialization error");
        return;
    }
    onRequest(operationType, jsonString, &userStatus, userData);

    freeSerializedJsonString(jsonString);
}

std::unique_ptr<JsonDoc> CustomOperation::createConf() {
    if (!writeResponse) {
        return createEmptyDocument();
    }

    char *str = nullptr;

    size_t capacity = MO_MAX_JSON_CAPACITY / 8;

    while (!str && capacity <= MO_MAX_JSON_CAPACITY) {
        str = static_cast<char*>(MO_MALLOC(getMemoryTag(), capacity));
        if (!str) {
            MO_DBG_ERR("OOM");
            break;
        }

        auto ret = writeResponse(operationType, str, capacity, userStatus, userData);
        if (ret >= 0 && ret < 1) {
            MO_DBG_ERR("cannot process empty string");
        } else if (ret >= 0 && (size_t)ret < capacity) {
            //success
            break;
        }

        MO_FREE(str);
        capacity *= 2;
    }

    if (!str) {
        MO_DBG_ERR("failure to create conf");
    }

    auto ret = makeDeserializedJson(getMemoryTag(), str);
    freeSerializedJsonString(str);
    return ret;
}
