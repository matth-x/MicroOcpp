// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Connection.h>
#include <MicroOcpp/Core/MessageService.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/DataTransfer/DataTransferService.h>

using namespace MicroOcpp;


namespace MicroOcpp {
class DataTransferMockOperation : public Operation, public MemoryManaged {
private:
    char *msg = nullptr;
    static int receivedReqCount;
    static int receivedConfCount;
public:
    DataTransferMockOperation(const char *msg = nullptr) 
        : MemoryManaged("UnitTests.Operation.", "DataTransferMockOperation") 
    {
        if (msg) {
            size_t size = strlen(msg);
            this->msg = static_cast<char*>(MO_MALLOC(getMemoryTag(), size));
            if (this->msg) {
                memset(this->msg, 0, size);
                auto ret = snprintf(this->msg, size, "%s", msg);
                if (ret < 0 || (size_t)ret >= size) {
                    MO_DBG_ERR("snprintf: %i", ret);
                    MO_FREE(this->msg);
                    this->msg = nullptr;
                    //send empty string
                }
                //success
            } else {
                MO_DBG_ERR("OOM");
                //send empty string
            }
        }
        receivedConfCount = 0;
        receivedReqCount = 0;
    }
    ~DataTransferMockOperation()=default;

    const char* getOperationType() override {
        return "DataTransferMockOperation";
    }

    std::unique_ptr<JsonDoc> createReq() override {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
        JsonObject payload = doc->to<JsonObject>();
        payload["vendorId"] = "org.microocpp.test";
        payload["data"] = msg ? (const char*)msg : "";
        return doc;
    }

    void processConf(JsonObject payload) override {
        receivedConfCount++;
    }

    void processReq(JsonObject payload) override {
        receivedReqCount++;
    }

    std::unique_ptr<JsonDoc> createConf() override {
        auto res = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
        auto payload = res->to<JsonObject>();
        payload["status"] = "Accepted";
        return res;
    }

    static int receivedReqAndClear() {
        int temp = receivedReqCount;
        receivedReqCount = 0;
        return temp;
    }

    static int receivedConfAndClear() {
        int temp = receivedConfCount;
        receivedConfCount = 0;
        return temp;
    }
};

int DataTransferMockOperation::receivedReqCount = 0;
int DataTransferMockOperation::receivedConfCount = 0;
} // namespace MicroOcpp


TEST_CASE( "DataTransfer" ) {
    printf("\nRun %s\n",  "DataTransfer");

    mo_initialize();
    mo_useMockServer();

    mo_getContext()->setTicksCb(custom_timer_cb);

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);
    mo_setOcppVersion(ocppVersion);
    mo_setBootNotificationData("TestModel", "TestVendor");

    SECTION("Register and retrieve operation") {
        mo_setup();
        loop();

        auto dataTransferService = mo_getContext()->getModel().getDataTransferService();
        REQUIRE( dataTransferService != nullptr );

        // register operation
        REQUIRE( dataTransferService->registerOperation("org.microocpp.test", "test0", [](Context& context) -> Operation* {
            return new DataTransferMockOperation(); }) );
        REQUIRE( dataTransferService->registerOperation("org.microocpp.test", "test1", [](Context& context) -> Operation* {
            return new DataTransferMockOperation(); }) );
        REQUIRE( dataTransferService->registerOperation("org.microocpp.test", "test2", [](Context& context) -> Operation* {
            return new DataTransferMockOperation(); }) );

        // retrieve registered operation
        auto op = dataTransferService->getRegisteredOperation("org.microocpp.test", "test0");
        REQUIRE( op != nullptr );
        op = dataTransferService->getRegisteredOperation("org.microocpp.test", "test1");
        REQUIRE( op != nullptr );
        op = dataTransferService->getRegisteredOperation("org.microocpp.test", "test2");
        REQUIRE( op != nullptr );

        // retrieve non-registered operation
        dataTransferService->clearRegisteredOperation("org.mircoocpp.test", "test2");
        op = dataTransferService->getRegisteredOperation("org.mircoocpp.test", "test2");
        REQUIRE( op == nullptr );
        delete op;
    }
    


    SECTION("Dispatch registered operation") {
        auto dataTransferService = mo_getContext()->getModel().getDataTransferService();

        mo_setup();
        loop();

        // dispatch registered operation
        REQUIRE( dataTransferService->registerOperation("org.microocpp.mock", "mock", [](Context &context) -> Operation* {
            return new DataTransferMockOperation(); }) );

        // dispatch error message id 
        mo_getContext()->getMessageService().receiveMessage("[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.microocpp.mock\",\"messageId\":\"error\",\"data\":\"{\\\"text\\\":\\\"Hello MO\\\"}\"}]", 115);
        loop();

        // dispatch error vendor id
        mo_getContext()->getMessageService().receiveMessage("[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.microocpp.error\",\"messageId\":\"mock\",\"data\":\"{\\\"text\\\":\\\"Hello MO\\\"}\"}]", 115);
        loop();

        REQUIRE( DataTransferMockOperation::receivedReqAndClear() == 0 );

        // dispatch correct message ocpp v16
        mo_getContext()->getMessageService().receiveMessage("[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.microocpp.mock\",\"messageId\":\"mock\",\"data\":\"{\\\"text\\\":\\\"Hello MO\\\"}\"}]", 114);
        loop();

        REQUIRE( DataTransferMockOperation::receivedReqAndClear() == 1 );

        // dispatch correct message ocpp v201
        mo_getContext()->getMessageService().receiveMessage("[2,\"testmsg\",\"DataTransfer\",{\"vendorId\":\"org.microocpp.mock\",\"messageId\":\"mock\",\"data\": {\"text\":\"Hello MO\"}}]", 110);
        loop();

        REQUIRE( DataTransferMockOperation::receivedReqAndClear() == 1 );
    }


    mo_deinitialize();
}