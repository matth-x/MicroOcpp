// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_REQUESTQUEUE_H
#define MO_REQUESTQUEUE_H

#ifdef __cplusplus
#include <limits>
#include <memory>

#include <ArduinoJson.h>
#endif //__cplusplus

#include <MicroOcpp/Core/Memory.h>

#ifndef MO_REQUEST_CACHE_MAXSIZE
#define MO_REQUEST_CACHE_MAXSIZE 10
#endif

#ifndef MO_NUM_REQUEST_QUEUES
#define MO_NUM_REQUEST_QUEUES 10
#endif

#ifdef __cplusplus

namespace MicroOcpp {

class OperationRegistry;
class Request;

class RequestQueue {
public:
    static const unsigned int NoOperation = std::numeric_limits<unsigned int>::max();

    virtual unsigned int getFrontRequestOpNr() = 0; //return OpNr of front request or NoOperation if queue is empty
    virtual std::unique_ptr<Request> fetchFrontRequest() = 0;
};

class VolatileRequestQueue : public RequestQueue, public MemoryManaged {
private:
    std::unique_ptr<Request> requests [MO_REQUEST_CACHE_MAXSIZE];
    size_t front = 0, len = 0;
public:
    VolatileRequestQueue();
    ~VolatileRequestQueue();
    void loop();

    unsigned int getFrontRequestOpNr() override;
    std::unique_ptr<Request> fetchFrontRequest() override;

    bool pushRequestBack(std::unique_ptr<Request> request);
};

} //namespace MicroOcpp
#endif //__cplusplus
#endif
