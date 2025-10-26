// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

VolatileRequestQueue::VolatileRequestQueue() : MemoryManaged("VolatileRequestQueue") {

}

VolatileRequestQueue::~VolatileRequestQueue() = default;

void VolatileRequestQueue::loop() {

    /*
     * Drop timed out operations
     */
    size_t i = 0;
    while (i < len) {
        size_t index = (front + i) % MO_REQUEST_CACHE_MAXSIZE;
        auto& request = requests[index];

        if (request->isTimeoutExceeded()) {
            MO_DBG_INFO("operation timeout: %s", request->getOperationType());
            request->executeTimeout();

            if (index == front) {
                requests[front].reset();
                front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
                len--;
            } else {
                requests[index].reset();
                for (size_t i = (index + MO_REQUEST_CACHE_MAXSIZE - front) % MO_REQUEST_CACHE_MAXSIZE; i < len - 1; i++) {
                    requests[(front + i) % MO_REQUEST_CACHE_MAXSIZE] = std::move(requests[(front + i + 1) % MO_REQUEST_CACHE_MAXSIZE]);
                }
                len--;
            }
        } else {
            i++;
        }
    }
}

unsigned int VolatileRequestQueue::getFrontRequestOpNr() {
    if (len == 0) {
        return NoOperation;
    }

    return 1; //return OpNr 1 to grant PreBoot queue higher priority (=0), but send messages before tx-msg queue (starting with 10)
}

std::unique_ptr<Request> VolatileRequestQueue::fetchFrontRequest() {
    if (len == 0) {
        return nullptr;
    }

    std::unique_ptr<Request> result = std::move(requests[front]);
    front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
    len--;

    MO_DBG_VERBOSE("front %zu len %zu", front, len);

    return result;
}

bool VolatileRequestQueue::pushRequestBack(std::unique_ptr<Request> request) {

    // Don't queue up multiple StatusNotification messages for the same connectorId
    #if 0 // Leads to ASAN failure when executed by Unit test suite (CustomOperation is casted to StatusNotification)
    if (strcmp(request->getOperationType(), "StatusNotification") == 0)
    {
        size_t i = 0;
        while (i < len) {
            size_t index = (front + i) % MO_REQUEST_CACHE_MAXSIZE;

            if (strcmp(requests[index]->getOperationType(), "StatusNotification")!= 0)
            {
                i++;
                continue;
            }
            auto new_status_notification = static_cast<v16::StatusNotification*>(request->getOperation());
            auto old_status_notification = static_cast<v16::StatusNotification*>(requests[index]->getOperation());
            if (old_status_notification->getConnectorId() == new_status_notification->getConnectorId()) {
                requests[index].reset();
                for (size_t i = (index + MO_REQUEST_CACHE_MAXSIZE - front) % MO_REQUEST_CACHE_MAXSIZE; i < len - 1; i++) {
                    requests[(front + i) % MO_REQUEST_CACHE_MAXSIZE] = std::move(requests[(front + i + 1) % MO_REQUEST_CACHE_MAXSIZE]);
                }
                len--;
            } else {
                i++;
            }
        }
    }
    #endif

    if (len >= MO_REQUEST_CACHE_MAXSIZE) {
        MO_DBG_INFO("Drop cached operation (cache full): %s", requests[front]->getOperationType());
        requests[front]->executeTimeout();
        requests[front].reset();
        front = (front + 1) % MO_REQUEST_CACHE_MAXSIZE;
        len--;
    }

    requests[(front + len) % MO_REQUEST_CACHE_MAXSIZE] = std::move(request);
    len++;
    return true;
}
