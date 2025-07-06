// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/Heartbeat.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>
#include <string.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

Heartbeat::Heartbeat(Context& context) : MemoryManaged("v16.Operation.", "Heartbeat"), context(context) {

}

const char* Heartbeat::getOperationType(){
    return "Heartbeat";
}

std::unique_ptr<JsonDoc> Heartbeat::createReq() {
    return createEmptyDocument();
}

void Heartbeat::processConf(JsonObject payload) {

    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (context.getClock().setTime(currentTime)) {
            //success
            MO_DBG_DEBUG("Request has been accepted");
        } else {
            MO_DBG_WARN("Could not read time string. Expect format like 2020-02-01T20:53:32.486Z");
        }
    } else {
        MO_DBG_WARN("Missing field currentTime. Expect format like 2020-02-01T20:53:32.486Z");
    }
}

#if MO_ENABLE_MOCK_SERVER
int Heartbeat::writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData) {
    (void)userStatus;

    auto& context = *reinterpret_cast<Context*>(userData);

    char timeStr [MO_JSONDATE_SIZE];

    if (context.getClock().now().isUnixTime()) {
        context.getClock().toJsonString(context.getClock().now(), timeStr, sizeof(timeStr));
    } else {
        (void)snprintf(timeStr, sizeof(timeStr), "2025-05-18T18:55:13Z");
    }

    return snprintf(buf, size, "{\"currentTime\":\"%s\"}", timeStr);
}
#endif //MO_ENABLE_MOCK_SERVER

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
