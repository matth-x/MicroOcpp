// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#if MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER

#include <map>

namespace MicroOcpp {
namespace Memory {

struct MemBlockInfo {
    void* tagger_ptr = nullptr;
    std::string tag;
    size_t size = 0;

    MemBlockInfo(void* ptr, const char *tag, size_t size) : size{size} {
        updateTag(ptr, tag);
    }

    void updateTag(void* ptr, const char *tag);
};

std::map<void*,MemBlockInfo> memBlocks; //key: memory address of malloc'd block

struct MemTagInfo {
    size_t current_size = 0;
    size_t max_size = 0;

    MemTagInfo(size_t size) {
        operator+=(size);
    }

    void operator+=(size_t size) {
        current_size += size;
        max_size = std::max(max_size, current_size);
    }

    void operator-=(size_t size) {
        if (size > current_size) {
            MO_DBG_ERR("captured size does not fit");
            //return; let it happen for now
        }
        current_size -= size;
    }

    void reset() {
        max_size = current_size;
    }
};

std::map<std::string,MemTagInfo> memTags;

size_t memTotal, memTotalMax;

void MemBlockInfo::updateTag(void* ptr, const char *tag) {
    if (!tag) {
        return;
    }
    if (tagger_ptr == nullptr || ptr < tagger_ptr) {
        MO_DBG_VERBOSE("update tag from %s to %s, ptr from %p to %p", this->tag.c_str(), tag, tagger_ptr, ptr);

        auto tagInfo = memTags.find(this->tag);
        if (tagInfo != memTags.end()) {
            tagInfo->second -= size;
        }

        tagInfo = memTags.find(tag);
        if (tagInfo != memTags.end()) {
            tagInfo->second += size;
        } else {
            memTags.emplace(tag, size);
        }

        tagger_ptr = ptr;
        this->tag = tag;
    }
}

} //namespace Memory
} //namespace MicroOcpp

#endif //MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER

#if MO_OVERRIDE_ALLOCATION

namespace MicroOcpp {
namespace Memory {

void* (*malloc_override)(size_t);
void (*free_override)(void*);

}
}

using namespace MicroOcpp::Memory;

void mo_mem_set_malloc_free(void* (*malloc_override)(size_t), void (*free_override)(void*)) {
    MicroOcpp::Memory::malloc_override = malloc_override;
    MicroOcpp::Memory::free_override = free_override;
}

void *mo_mem_malloc(const char *tag, size_t size) {
    MO_DBG_VERBOSE("malloc %zu B (%s)", size, tag ? tag : "unspecified");

    void *ptr;
    if (malloc_override) {
        ptr = malloc_override(size);
    } else {
        ptr = malloc(size);
    }

    #if MO_ENABLE_HEAP_PROFILER
    if (ptr) {
        memBlocks.emplace(ptr, MemBlockInfo(ptr, tag, size));

        memTotal += size;
        memTotalMax = std::max(memTotalMax, memTotal);
    }
    #endif
    return ptr;
}

void mo_mem_free(void* ptr) {
    MO_DBG_VERBOSE("free");

    #if MO_ENABLE_HEAP_PROFILER
    if (ptr) {

        auto blockInfo = memBlocks.find(ptr);
        if (blockInfo != memBlocks.end()) {
            auto tagInfo = memTags.find(blockInfo->second.tag);
            if (tagInfo != memTags.end()) {
                tagInfo->second -= blockInfo->second.size;
            }
            memTotal -= blockInfo->second.size;
        }

        if (blockInfo != memBlocks.end()) {
            memBlocks.erase(blockInfo);
        }
    }
    #endif

    if (free_override) {
        free_override(ptr);
    } else {
        free(ptr);
    }
}

#endif //MO_OVERRIDE_ALLOCATION

#if MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER

void mo_mem_deinit() {
    memBlocks.clear();
    memTags.clear();
}

void mo_mem_reset() {
    MO_DBG_DEBUG("Reset all maximum values to current values");

    for (auto tagInfo = (memTags).begin(); tagInfo != memTags.end(); ++tagInfo) {
        tagInfo->second.reset();
    }

    memTotalMax = memTotal;
}

void mo_mem_set_tag(void *ptr, const char *tag) {
    MO_DBG_VERBOSE("set tag (%s)", tag ? tag : "unspecified");

    if (!tag) {
        return;
    }

    bool hasTagged = false;

    if (tag) {
        auto foundBlock = memBlocks.upper_bound(ptr);
        if (foundBlock != memBlocks.begin()) {
            --foundBlock;
        }
        if (foundBlock != memBlocks.end() &&
                (unsigned char*)ptr - (unsigned char*)foundBlock->first < (std::ptrdiff_t)foundBlock->second.size) {
            foundBlock->second.updateTag(ptr, tag);
            hasTagged = true;
        }
    }

    if (!hasTagged) {
        MO_DBG_VERBOSE("memory area doesn't apply");
    }
}

void mo_mem_print_stats() {

    MO_DBG_INFO("\n\n *** Heap usage statistics ***");

    size_t size = 0;

    size_t untagged = 0, untagged_size = 0;

    for (const auto& heapEntry : memBlocks) {
        size += heapEntry.second.size;
        #if MO_DBG_LEVEL >= MO_DL_VERBOSE
        {
            MO_DBG_INFO("@%p - %zu B (%s)\n", heapEntry.first, heapEntry.second.size, heapEntry.second.tag.c_str());
        }
        #endif

        if (heapEntry.second.tag.empty()) {
            untagged ++;
            untagged_size += heapEntry.second.size;
        }
    }

    std::map<std::string, size_t> tags;
    for (const auto& heapEntry : memBlocks) {
        auto foundTag = tags.find(heapEntry.second.tag);
        if (foundTag != tags.end()) {
            foundTag->second += heapEntry.second.size;
        } else {
            tags.emplace(heapEntry.second.tag, heapEntry.second.size);
        }
    }

    size_t size_control = 0;

    for (const auto& tag : tags) {
        size_control += tag.second;
        #if MO_DBG_LEVEL >= MO_DL_VERBOSE
        {
            MO_DBG_INFO("%s - %zu B\n", tag.first.c_str(), tag.second);
        }
        #endif
    }

    size_t size_control2 = 0;
    for (const auto& tag : memTags) {
        size_control2 += tag.second.current_size;
        MO_DBG_INFO("%s - %zu B (max. %zu B)", tag.first.c_str(), tag.second.current_size, tag.second.max_size);
    }

    MO_DBG_INFO("\n *** Summary ***\nBlocks: %zu\nTags: %zu\nCurrent usage: %zu B\nMaximum usage: %zu B\n", memBlocks.size(), memTags.size(), memTotal, memTotalMax);
    #if MO_DBG_LEVEL >= MO_DL_DEBUG
    {
        MO_DBG_INFO("\n *** Debug information ***\nTotal blocks (control value 1): %zu B\nTags (control value): %zu\nTotal tagged (control value 2): %zu B\nTotal tagged (control value 3): %zu B\nUntagged: %zu\nTotal untagged: %zu B\n", size, tags.size(), size_control, size_control2, untagged, untagged_size);
    }
    #endif
}

int mo_mem_write_stats_json(char *buf, size_t size) {
    DynamicJsonDocument doc {size * 2};

    doc["total_current"] = memTotal;
    doc["total_max"] = memTotalMax;
    doc["total_blocks"] = memBlocks.size();

    JsonArray by_tag = doc.createNestedArray("by_tag");
    for (const auto& tag : memTags) {
        JsonObject entry = by_tag.createNestedObject();
        entry["tag"] = tag.first.c_str();
        entry["current"] = tag.second.current_size;
        entry["max"] = tag.second.max_size;
    }

    size_t untagged = 0, untagged_size = 0;

    for (const auto& heapEntry : memBlocks) {
        if (heapEntry.second.tag.empty()) {
            untagged ++;
            untagged_size += heapEntry.second.size;
        }
    }

    doc["untagged_blocks"] = untagged;
    doc["untagged_size"] = untagged_size;

    if (doc.overflowed()) {
        MO_DBG_ERR("exceeded JSON capacity");
        return -1;
    }

    return (int)serializeJson(doc, buf, size);
}

#endif //MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER

namespace MicroOcpp {

String makeString(const char *tag, const char *val) {
#if MO_OVERRIDE_ALLOCATION
    if (val) {
        return String(val, Allocator<char>(tag));
    } else {
        return String(Allocator<char>(tag));
    }
#else
    if (val) {
        return String(val);
    } else {
        return String();
    }
#endif
}

JsonDoc initJsonDoc(const char *tag, size_t capacity) {
#if MO_OVERRIDE_ALLOCATION
    return JsonDoc(capacity, ArduinoJsonAllocator(tag));
#else
    return JsonDoc(capacity);
#endif
}

std::unique_ptr<JsonDoc> makeJsonDoc(const char *tag, size_t capacity) {
#if MO_OVERRIDE_ALLOCATION
    auto doc = std::unique_ptr<JsonDoc>(new JsonDoc(capacity, ArduinoJsonAllocator(tag)));
    if (!doc || doc->capacity() < capacity) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    return doc;
#else
    return std::unique_ptr<JsonDoc>(new JsonDoc(capacity));
#endif
}

}
