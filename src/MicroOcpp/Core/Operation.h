// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

/**
 * This framework considers OCPP operations to be a combination of two things: first, ensure that the message reaches its
 * destination properly (the "Remote procedure call" header, e.g. message Id). Second, transmit the application data
 * as specified in the OCPP 1.6 document.
 * 
 * The remote procedure call (RPC) part is implemented by the class Request. The application data part is implemented by
 * the respective Operation subclasses, e.g. BootNotification, StartTransaction, ect.
 * 
 * The resulting structure is that the RPC header (=instance of Request) holds a reference to the payload
 * message creator (=instance of BootNotification, StartTransaction, ...). Both objects working together give the complete
 * OCPP operation.
 */

 #ifndef MO_OPERATION_H
 #define MO_OPERATION_H

#include <ArduinoJson.h>
#include <memory>

#include <MicroOcpp/Core/RequestStore.h>

namespace MicroOcpp {

std::unique_ptr<DynamicJsonDocument> createEmptyDocument();

class Operation {
public:
    Operation();

    virtual ~Operation();
    
    virtual const char* getOperationType();

    virtual void initiate(StoredOperationHandler *rpcData);

    virtual bool restore(StoredOperationHandler *rpcData);

    /**
     * Create the payload for the respective OCPP message
     * 
     * For instance operation Authorize: creates Authorize.req(idTag)
     * 
     * This function is usually called multiple times by the Arduino loop(). On first call, the request is initially sent. In the
     * succeeding calls, the implementers decide to either recreate the request, or do nothing as the operation is still pending.
     */
    virtual std::unique_ptr<DynamicJsonDocument> createReq();


    virtual void processConf(JsonObject payload);
    
    /*
     * returns if the operation must be aborted
     */
    virtual bool processErr(const char *code, const char *description, JsonObject details) { return true;}

    /**
     * Processes the request in the JSON document. 
     */
    virtual void processReq(JsonObject payload);

    /**
     * After successfully processing a request sent by the communication counterpart, this function creates the payload for a confirmation
     * message.
     */
    virtual std::unique_ptr<DynamicJsonDocument> createConf();

    virtual const char *getErrorCode() {return nullptr;} //nullptr means no error
    virtual const char *getErrorDescription() {return "";}
    virtual std::unique_ptr<DynamicJsonDocument> getErrorDetails() {return createEmptyDocument();}
};

} //end namespace MicroOcpp
 #endif
