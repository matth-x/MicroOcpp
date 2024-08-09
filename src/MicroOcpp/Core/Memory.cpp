// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_HEAP_PROFILER

#include <map>

std::map<void*,std::tuple<std::string, size_t>> memBlocks; //key: memory address of malloc'd block; value tuple: {tag of block, size of block}

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
        memBlocks.emplace(ptr, decltype(memBlocks)::mapped_type{tag ? tag : "", size});
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

    bool hasTagged = false;

    if (tag) {
        auto foundBlock = memBlocks.find(ptr);
        if (foundBlock != memBlocks.end()) {
            std::get<0>(foundBlock->second) = tag;
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

    for (const auto& heapEntry : memBlocks) {
        size += std::get<1>(heapEntry.second);
        printf("@%p - %zu B (%s)\n", heapEntry.first, std::get<1>(heapEntry.second), std::get<0>(heapEntry.second).c_str());
    }

    std::map<std::string, size_t> tags;
    for (const auto& heapEntry : memBlocks) {
        auto foundTag = tags.find(std::get<0>(heapEntry.second));
        if (foundTag != tags.end()) {
            foundTag->second += std::get<1>(heapEntry.second);
        } else {
            tags.emplace(std::get<0>(heapEntry.second), std::get<1>(heapEntry.second));
        }
    }

    size_t size_control = 0;

    for (const auto& tag : tags) {
        size_control += tag.second;
        printf("%s - %zu B\n", tag.first.c_str(), tag.second);
    }

    printf("Blocks: %zu\nTotal usage: %zu B\nTags: %zu\nTotal tagged: %zu B\n\n", memBlocks.size(), size, tags.size(), size_control);
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
