// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OperationsQueue.h>
#include <ArduinoOcpp/Core/OperationStore.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Debug.h>

#include <algorithm>

#define AO_OPERATIONCACHE_MAXSIZE 10

using namespace ArduinoOcpp;

VolatileOperationsQueue::VolatileOperationsQueue() {

}

VolatileOperationsQueue::~VolatileOperationsQueue() {

}

OcppOperation *VolatileOperationsQueue::front() {
    if (!queue.empty()) {
        return queue.front().get();
    } else {
        return nullptr;
    }
}

void VolatileOperationsQueue::pop_front() {
    queue.pop_front();
}

void VolatileOperationsQueue::initiate(std::unique_ptr<OcppOperation> op) {

    op->initiate(nullptr);

    if (queue.size() >= AO_OPERATIONCACHE_MAXSIZE) {
        AO_DBG_WARN("unsafe number of cached operations");
        (void)0;
    }

    queue.push_back(std::move(op));
}

std::deque<std::unique_ptr<OcppOperation>>::iterator VolatileOperationsQueue::begin_tail() {
    if (queue.begin() != queue.end()) {
        return queue.begin() + 1;
    } else {
        return queue.end();
    }
}
std::deque<std::unique_ptr<OcppOperation>>::iterator VolatileOperationsQueue::end_tail() {
    return queue.end();
}
std::deque<std::unique_ptr<OcppOperation>>::iterator VolatileOperationsQueue::erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el) {
    return queue.erase(el);
}
void VolatileOperationsQueue::drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred) {
    queue.erase(std::remove_if(queue.begin(), queue.end(), pred), queue.end());
}

PersistentOperationsQueue::PersistentOperationsQueue(std::shared_ptr<OcppModel> baseModel, std::shared_ptr<FilesystemAdapter> filesystem)
            : opStore(filesystem), baseModel(baseModel) { }

PersistentOperationsQueue::~PersistentOperationsQueue() {

}

OcppOperation *PersistentOperationsQueue::front() {
    if (!head && !tailCache.empty()) {
        AO_DBG_ERR("invalid state");
        pop_front();
    }
    return head.get();
}

void PersistentOperationsQueue::pop_front() {
    
    if (head && head->getStorageHandler() && head->getStorageHandler()->getOpNr() >= 0) {
        opStore.advanceOpNr(head->getStorageHandler()->getOpNr());
        AO_DBG_DEBUG("advanced %i to %u", head->getStorageHandler()->getOpNr(), opStore.getOpBegin());
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
        [nextOpNr] (std::unique_ptr<OcppOperation>& op) {
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
        
        std::unique_ptr<OcppOperation> fetched;

        bool exists = false;
        unsigned int range = (opStore.getOpEnd() + AO_MAX_OPNR - nextOpNr) % AO_MAX_OPNR;
        for (size_t i = 0; i < range; i++) {
            exists = storageHandler->restore(nextOpNr);
            if (exists) {
                break;
            }
            nextOpNr++;
            nextOpNr %= AO_MAX_OPNR;
        }

        if (exists) {
            //case B) -> load operation from flash and take it as front element
            fetched = makeOcppOperation();
            
            bool success = fetched->restore(std::move(storageHandler), baseModel);

            if (!success) {
                AO_DBG_ERR("could not restore operation");
                fetched.reset();
            }

            if (!fetched->isFullyConfigured()) {
                AO_DBG_ERR("stored op initialization failure");
                fetched.reset();
            }
        }

        if (fetched) {
            //found operation in flash -> case B)
            head = std::move(fetched);
            AO_DBG_DEBUG("restored operation from flash");
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

    AO_DBG_VERBOSE("popped front");
}

void PersistentOperationsQueue::initiate(std::unique_ptr<OcppOperation> op) {

    op->initiate(opStore.makeOpHandler());

    if (!head && !tailCache.empty()) {
        AO_DBG_ERR("invalid state");
        pop_front();
    }

    if (!head) {
        head = std::move(op);
    } else {
        if (tailCache.size() >= AO_OPERATIONCACHE_MAXSIZE) {
            AO_DBG_INFO("Replace cached operation (cache full): %s", tailCache.front()->getOcppOperationType());
            tailCache.front()->executeTimeout();
            tailCache.pop_front();
        }

        tailCache.push_back(std::move(op));
    }
}

void PersistentOperationsQueue::drop_if(std::function<bool(std::unique_ptr<OcppOperation>&)> pred) {

    while (head && pred(head)) {
        pop_front();
    }

    tailCache.erase(std::remove_if(tailCache.begin(), tailCache.end(), pred), tailCache.end());
}

std::deque<std::unique_ptr<OcppOperation>>::iterator PersistentOperationsQueue::begin_tail() {
    return tailCache.begin();
}

std::deque<std::unique_ptr<OcppOperation>>::iterator PersistentOperationsQueue::end_tail() {
    return tailCache.end();
}

std::deque<std::unique_ptr<OcppOperation>>::iterator PersistentOperationsQueue::erase_tail(std::deque<std::unique_ptr<OcppOperation>>::iterator el) {
    return tailCache.erase(el);
}
