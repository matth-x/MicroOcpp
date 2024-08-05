// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

void *mo_mem_malloc(const char *tag, size_t size) {
    MO_DBG_DEBUG("malloc %zu B (%s)", size, tag ? tag : "unspecified");
    return malloc(size);
}

void mo_mem_free(void* ptr) {
    MO_DBG_DEBUG("free");
    free(ptr);
}

#if MO_ENABLE_HEAP_PROFILER

void mo_mem_set_tag(void *ptr, const char *tag) {
    MO_DBG_DEBUG("set tag (%s)", tag ? tag : "unspecified");
}

void mo_mem_print_stats();

#endif //MO_ENABLE_HEAP_PROFILER

namespace MicroOcpp {

MemString makeMemString(const char *tag) {
#if !MO_OVERRIDE_ALLOCATION || !MO_ENABLE_HEAP_PROFILER
    return MemString();
#else
    return MemString(Allocator<char>(tag));
#endif
}

}
