// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/RequestQueueStorageStrategy.h>
#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>

#define MO_OPERATIONCACHE_MAXSIZE 10

using namespace MicroOcpp;

VolatileRequestQueue::VolatileRequestQueue() {

}

VolatileRequestQueue::~VolatileRequestQueue() {

}

Request *VolatileRequestQueue::front() {
    if (!queue.empty()) {
        return queue.front().get();
    } else {
        return nullptr;
    }
}

void VolatileRequestQueue::pop_front() {
    queue.pop_front();
}

void VolatileRequestQueue::push_back(std::unique_ptr<Request> op) {

    op->initiate(nullptr);

    if (queue.size() >= MO_OPERATIONCACHE_MAXSIZE) {
        MO_DBG_WARN("unsafe number of cached operations");
        (void)0;
    }

    queue.push_back(std::move(op));
}

void VolatileRequestQueue::drop_if(std::function<bool(std::unique_ptr<Request>&)> pred) {
    queue.erase(std::remove_if(queue.begin(), queue.end(), pred), queue.end());
}

PersistentRequestQueue::PersistentRequestQueue(Model *baseModel, std::shared_ptr<FilesystemAdapter> filesystem)
            : opStore(filesystem), baseModel(baseModel) { }

PersistentRequestQueue::~PersistentRequestQueue() {

}

Request *PersistentRequestQueue::front() {
    if (!head && !tailCache.empty()) {
        MO_DBG_ERR("invalid state");
        pop_front();
    }
    return head.get();
}

void PersistentRequestQueue::pop_front() {
    
    if (head && head->getStorageHandler() && head->getStorageHandler()->getOpNr() >= 0) {
        opStore.advanceOpNr(head->getStorageHandler()->getOpNr());
        MO_DBG_DEBUG("advanced %i to %u", head->getStorageHandler()->getOpNr(), opStore.getOpBegin());
    }

    head.reset();

    unsigned int nextOpNr = opStore.getOpBegin();

    /*
     * Find next operation to take as front. Two cases:
     * A) [front, tailCache] contains all operations stored on this device
     * B) [front, tailCache] does not contain all operations. The next operation after front is not present
     *                       in the cache. Fetch the next operation from the flash to front
     */

    auto found = std::find_if(tailCache.begin(), tailCache.end(),
        [nextOpNr] (std::unique_ptr<Request>& op) {
            return op->getStorageHandler() &&
                   op->getStorageHandler()->getOpNr() >= 0 &&
                   (unsigned int) op->getStorageHandler()->getOpNr() == nextOpNr;
    });

    if (found != tailCache.end()) {
        //cache hit -> case A) -> don't load from flash but just take the next element from tail
        head = std::move(tailCache.front());
        tailCache.pop_front();
    } else {
        //cache miss -> case B) or A) -> try to fetch operation from flash (check for case B)) or take first cached element as front
        auto storageHandler = opStore.makeOpHandler();
        
        std::unique_ptr<Request> fetched;

        unsigned int range = (opStore.getOpEnd() + MO_MAX_OPNR - nextOpNr) % MO_MAX_OPNR;
        for (size_t i = 0; i < range; i++) {
            bool exists = storageHandler->restore(nextOpNr);
            if (exists) {
                //case B) -> load operation from flash and take it as front element

                fetched = makeRequest();

                bool success = fetched->restore(std::move(storageHandler), baseModel);

                if (success) {
                    //loaded operation from flash and will place it at head position of the queue
                    break;
                }

                MO_DBG_ERR("could not restore operation");
                fetched.reset();
                opStore.advanceOpNr(nextOpNr);
                nextOpNr = opStore.getOpBegin();
            } else {
                //didn't find anything at this slot. Try next slot
                nextOpNr++;
                nextOpNr %= MO_MAX_OPNR;
            }
        }

        if (fetched) {
            //found operation in flash -> case B)
            head = std::move(fetched);
            MO_DBG_DEBUG("restored operation from flash");
        } else {
            //no operation anymore in flash -> case A) -> take next queued operation in tailCache
            if (tailCache.empty()) {
                //no operations anymore
            } else {
                head = std::move(tailCache.front());
                tailCache.pop_front();
            }
        }
    }

    MO_DBG_VERBOSE("popped front");
}

void PersistentRequestQueue::push_back(std::unique_ptr<Request> op) {

    op->initiate(opStore.makeOpHandler());

    if (!head && !tailCache.empty()) {
        MO_DBG_ERR("invalid state");
        pop_front();
    }

    if (!head) {
        head = std::move(op);
    } else {
        if (tailCache.size() >= MO_OPERATIONCACHE_MAXSIZE) {
            MO_DBG_INFO("Replace cached operation (cache full): %s", tailCache.front()->getOperationType());
            tailCache.front()->executeTimeout();
            tailCache.pop_front();
        }

        tailCache.push_back(std::move(op));
    }
}

void PersistentRequestQueue::drop_if(std::function<bool(std::unique_ptr<Request>&)> pred) {

    while (head && pred(head)) {
        pop_front();
    }

    tailCache.erase(std::remove_if(tailCache.begin(), tailCache.end(), pred), tailCache.end());
}
