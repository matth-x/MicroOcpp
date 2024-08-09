// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_HEAP_PROFILER

#include <map>

struct MemBlockInfo {
    void* tagger_ptr = nullptr;
    std::string tag;
    size_t size = 0;

    MemBlockInfo(void* ptr, const char *tag, size_t size) : size{size} {
        updateTag(ptr, tag);
    }

    void updateTag(void* ptr, const char *tag) {
        if (!tag) {
            return;
        }
        if (tagger_ptr == nullptr || ptr < tagger_ptr) {
            MO_DBG_DEBUG("update tag from %s to %s, ptr from %p to %p", this->tag.c_str(), tag, tagger_ptr, ptr);
            tagger_ptr = ptr;
            this->tag = tag;
        }
    }
};

std::map<void*,MemBlockInfo> memBlocks; //key: memory address of malloc'd block

#endif //MO_ENABLE_HEAP_PROFILER

void mo_mem_deinit() {
    #if MO_ENABLE_HEAP_PROFILER
    memBlocks.clear();
    #endif
}

void *mo_mem_malloc(const char *tag, size_t size) {
    MO_DBG_DEBUG("malloc %zu B (%s)", size, tag ? tag : "unspecified");

    auto ptr = malloc(size);

    #if MO_ENABLE_HEAP_PROFILER
    if (ptr) {
        memBlocks.emplace(ptr, MemBlockInfo(ptr, tag, size));
    }
    #endif
    return ptr;
}

void mo_mem_free(void* ptr) {
    MO_DBG_DEBUG("free");

    #if MO_ENABLE_HEAP_PROFILER
    if (ptr) {
        memBlocks.erase(ptr);
    }
    #endif
    free(ptr);
}

#if MO_ENABLE_HEAP_PROFILER

void mo_mem_set_tag(void *ptr, const char *tag) {
    MO_DBG_DEBUG("set tag (%s)", tag ? tag : "unspecified");

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
        MO_DBG_DEBUG("memory area doesn't apply");
    }
}

void mo_mem_print_stats() {

    printf("\n *** Heap usage statistics ***\n");

    size_t size = 0;

    size_t untagged = 0, untagged_size = 0;

    for (const auto& heapEntry : memBlocks) {
        size += heapEntry.second.size;
        printf("@%p - %zu B (%s)\n", heapEntry.first, heapEntry.second.size, heapEntry.second.tag.c_str());

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
        printf("%s - %zu B\n", tag.first.c_str(), tag.second);
    }

    printf("Blocks: %zu\nTotal usage: %zu B\nTags: %zu\nTotal tagged: %zu B\nUntagged: %zu\nTotal untagged: %zu B\n", memBlocks.size(), size, tags.size(), size_control, untagged, untagged_size);
}

#endif //MO_ENABLE_HEAP_PROFILER

namespace MicroOcpp {

MemString makeMemString(const char *val, const char *tag, const char *tag_suffix) {
#if !MO_OVERRIDE_ALLOCATION || !MO_ENABLE_HEAP_PROFILER
    if (val) {
        return MemString(val);
    } else {
        return MemString();
    }
#else
    if (val) {
        return MemString(val, Allocator<char>(tag, tag_suffix));
    } else {
        return MemString(Allocator<char>(tag, tag_suffix));
    }
#endif
}

MemJsonDoc initMemJsonDoc(size_t capacity, const char *tag) {
#if !MO_OVERRIDE_ALLOCATION || !MO_ENABLE_HEAP_PROFILER
    return MemJsonDoc(capacity);
#else
    return MemJsonDoc(capacity, ArduinoJsonAllocator(tag));
#endif
}

std::unique_ptr<MemJsonDoc> makeMemJsonDoc(size_t capacity, const char *tag, const char *tag_suffix) {
#if !MO_OVERRIDE_ALLOCATION || !MO_ENABLE_HEAP_PROFILER
    return std::unique_ptr<MemJsonDoc>(new MemJsonDoc(capacity));
#else
    return std::unique_ptr<MemJsonDoc>(new MemJsonDoc(capacity, ArduinoJsonAllocator(tag, tag_suffix)));
#endif
}

}
