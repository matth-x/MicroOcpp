// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MEMORY_H
#define MO_MEMORY_H

#include <stddef.h>

#include <MicroOcpp/Debug.h> //only for evaluation. Remove before merging on main

#ifndef MO_OVERRIDE_ALLOCATION
#define MO_OVERRIDE_ALLOCATION 1
#endif

#ifndef MO_ENABLE_EXTERNAL_RAM
#define MO_ENABLE_EXTERNAL_RAM 1
#endif

#ifndef MO_ENABLE_HEAP_PROFILER
#define MO_ENABLE_HEAP_PROFILER 1
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if MO_OVERRIDE_ALLOCATION

void mo_mem_init(void (*malloc_override)(size_t)); //pass custom malloc function to be used with the OCPP lib. If NULL, defaults to standard malloc
void mo_mem_deinit(void (*free_override)(void*)); //pass custom free function to be used with the OCPP lib. If NULL, defaults to standard free

void *mo_mem_malloc(const char *tag, size_t size);

void mo_mem_free(void* ptr);

#define MO_MALLOC(TAG, SIZE) mo_mem_malloc(TAG, SIZE)
#define MO_FREE(PTR) mo_mem_free(PTR)

#else
#define MO_MALLOC(TAG, SIZE) malloc(SIZE) //default malloc provided by host system
#define MO_FREE(PTR) free(PTR)       //default free provided by host system
#endif //MO_OVERRIDE_ALLOCATION


#if MO_ENABLE_HEAP_PROFILER

void mo_mem_set_tag(void *ptr, const char *tag);

void mo_mem_print_stats();

#define MO_MEM_SET_TAG(PTR, TAG) mo_mem_set_tag(PTR, TAG)
#define MO_MEM_PRINT_STATS mo_mem_print_stats

#define MO_MEMORY_TAG_OPT(X) X //usage: Allocator<MyClass> alloc = Allocator<MyClass>(MO_MEMORY_TAG("Example module"));

#else
#define MO_MEM_SET_TAG(...) (void)0
#define MO_MEM_PRINT_STATS(...) (void)0
#define MO_MEMORY_TAG_OPT(X) //usage: Allocator<MyClass> alloc = Allocator<MyClass>(MO_MEMORY_TAG("Example module"));
#endif //MO_ENABLE_HEAP_PROFILER


#if MO_ENABLE_EXTERNAL_RAM

void mo_mem_init_ext(void (*malloc_override)(size_t));
void mo_mem_deinit_ext(void (*free_override)(void*));

void *mo_mem_malloc_ext(const char *tag, size_t size);

void mo_mem_free_ext(void* ptr);

#define MO_MALLOC_EXT(TAG, SIZE) mo_mem_malloc_ext(TAG, SIZE)
#define MO_FREE_EXT(PTR) mo_mem_free_ext(PTR)

#else
#define MO_MALLOC_EXT MO_MALLOC
#define MO_FREE_EXT MO_FREE
#endif //MO_ENABLE_EXTERNAL_RAM


#ifdef __cplusplus
}

#ifndef MO_OVERRIDE_NEW
#define MO_OVERRIDE_NEW 1
#endif

#include <memory>
#include <string>
#include <vector>

#include <ArduinoJson.h>

#if MO_OVERRIDE_ALLOCATION

#include <string.h>

namespace MicroOcpp {

class AllocOverrider {
private:
    #if MO_ENABLE_HEAP_PROFILER
    char *tag = nullptr;
    #endif
protected:
    void updateMemTag(const char *src1, const char *src2 = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        if (!src1 && !src2) {
            //empty source does not update tag
            return;
        }
        char src [64];
        snprintf(src, sizeof(src), "%s%s", src1 ? src1 : "", src2 ? src2 : "");
        if (tag) {
            if (!strcmp(src, tag)) {
                //nothing to do
                return;
            }
            MO_FREE(tag);
            tag = nullptr;
        }
        size_t size = strlen(src) + 1;
        tag = static_cast<char*>(MO_MALLOC("HeapProfilerInternal", size));
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
        #else
        (void)tag;
        #endif
    }
    const char *getMemoryTag() {
        #if MO_ENABLE_HEAP_PROFILER
        return tag;
        #else
        return nullptr;
        #endif
    }
public:
    void *operator new(size_t size) {
        MO_DBG_DEBUG("AllocOverrider new %zu B", size);
        return MO_MALLOC(nullptr, size);
    }
    void operator delete(void * ptr) {
        MO_DBG_DEBUG("AllocOverrider delete");
        MO_FREE(ptr);
    }

    AllocOverrider(const char *tag = "Unspecified", const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(tag, tag_suffix);
        mo_mem_set_tag(this, this->tag);
        #endif
    }

    ~AllocOverrider() {
        #if MO_ENABLE_HEAP_PROFILER
        MO_FREE(tag);
        tag = nullptr;
        #endif
    }
};

template<class T>
struct Allocator {

    Allocator(const char *tag = nullptr, const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(tag, tag_suffix);
        #endif
    }

    template<class U>
    Allocator(const Allocator<U>& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(other.tag);
        #endif
    }

    Allocator(const Allocator& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(other.tag);
        #endif
    }

    //template<class U>
    //Allocator(Allocator<U>&& other) {
    Allocator(Allocator&& other) {
        #if MO_ENABLE_HEAP_PROFILER
        tag = other.tag;
        other.tag = nullptr;
        #endif
    }

    ~Allocator() {
        #if MO_ENABLE_HEAP_PROFILER
        if (tag) {
            MO_FREE(tag);
            tag = nullptr;
        }
        #endif
    }

    T *allocate(size_t count) {
        MO_DBG_DEBUG("Allocator allocate %zu B (%s)", sizeof(T) * count, tag ? tag : "unspecified");
        return static_cast<T*>(
            MO_MALLOC(
                #if MO_ENABLE_HEAP_PROFILER
                    tag,
                #else
                    nullptr,
                #endif
                sizeof(T) * count));
    }
    void deallocate(T *ptr, size_t count) {
        MO_DBG_DEBUG("Allocator deallocate %zu B (%s)", sizeof(T) * count, tag ? tag : "unspecified");
        MO_FREE(ptr);
    }

    bool operator==(const Allocator<T>& other) {
        #if MO_ENABLE_HEAP_PROFILER
        if (!tag && !other.tag) {
            return true;
        } else if (tag && other.tag) {
            return !strcmp(tag, other.tag);
        } else {
            return false;
        }
        #else
        return true;
        #endif
    }

    bool operator!=(const Allocator<T>& other) {
        return !operator==(other);
    }

    typedef T value_type;

    #if MO_ENABLE_HEAP_PROFILER
    char *tag = nullptr;

    void updateMemTag(const char *src1, const char *src2 = nullptr) {
        if (!src1 && !src2) {
            //empty source does not update tag
            return;
        }
        char src [64];
        snprintf(src, sizeof(src), "%s%s", src1 ? src1 : "", src2 ? src2 : "");
        if (tag) {
            if (!strcmp(src, tag)) {
                //nothing to do
                return;
            }
            MO_FREE(tag);
            tag = nullptr;
        }
        size_t size = strlen(src) + 1;
        tag = static_cast<char*>(MO_MALLOC("HeapProfilerInternal", size));
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
    }
    #endif
};

template<class T>
Allocator<T> makeAllocator(const char *tag = nullptr) {
    return Allocator<T>(tag);
}

using MemString = std::basic_string<char, std::char_traits<char>, MicroOcpp::Allocator<char>>;

template<class T>
using MemVector = std::vector<T, Allocator<T>>;

template<class T>
MemVector<T> makeMemVector(const char *tag) {
    return MemVector<T>(Allocator<T>(tag));
}

class ArduinoJsonAllocator {
private:
    #if MO_ENABLE_HEAP_PROFILER
    char *tag = nullptr;

    void updateMemTag(const char *src1, const char *src2 = nullptr) {
        if (!src1 && !src2) {
            //empty source does not update tag
            return;
        }
        char src [64];
        snprintf(src, sizeof(src), "%s%s", src1 ? src1 : "", src2 ? src2 : "");
        if (tag) {
            if (!strcmp(src, tag)) {
                //nothing to do
                return;
            }
            MO_FREE(tag);
            tag = nullptr;
        }
        size_t size = strlen(src) + 1;
        tag = static_cast<char*>(MO_MALLOC("HeapProfilerInternal", size));
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
    }
    #endif
public:

    ArduinoJsonAllocator(const char *tag = nullptr, const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(tag, tag_suffix);
        #endif
    }

    ArduinoJsonAllocator(const ArduinoJsonAllocator& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemTag(other.tag);
        #endif
    }

    ArduinoJsonAllocator(ArduinoJsonAllocator&& other) {
        #if MO_ENABLE_HEAP_PROFILER
        tag = other.tag;
        other.tag = nullptr;
        #endif
    }

    ~ArduinoJsonAllocator() {
        #if MO_ENABLE_HEAP_PROFILER
        if (tag) {
            MO_FREE(tag);
            tag = nullptr;
        }
        #endif
    }

    void *allocate(size_t size) {
        MO_DBG_DEBUG("ArduinoJsonAllocator allocate %zu B (%s)", size, tag ? tag : "unspecified");
        return MO_MALLOC(
                    #if MO_ENABLE_HEAP_PROFILER
                        tag,
                    #else
                        nullptr,
                    #endif
                    size);
    }
    void deallocate(void *ptr) {
        MO_DBG_DEBUG("ArduinoJsonAllocator deallocate");
        MO_FREE(ptr);
    }
};

using MemJsonDoc = BasicJsonDocument<ArduinoJsonAllocator>;

template<class T, typename ...Args>
T *mo_mem_new(const char *tag, Args&& ...args)  {
    MO_DBG_DEBUG("mo_mem_new %zu B (%s)", sizeof(T), tag ? tag : "unspecified");
    if (auto ptr = MO_MALLOC(tag, sizeof(T))) {
        return new(ptr) T(std::forward<Args>(args)...);
    }
    return nullptr; //OOM
}

template<class T>
void mo_mem_delete(T *ptr)  {
    MO_DBG_DEBUG("mo_mem_delete");
    if (ptr) {
        ptr->~T();
        MO_FREE(ptr);
    }
}

} //namespace MicroOcpp

#else

#include <memory>

namespace MicroOcpp {

class AllocOverrider {
public:
    AllocOverrider(const char *unused = nullptr) {
        (void)unused;
    }
};

template<class T>
using Allocator = ::std::allocator<T>;

template<class T>
Allocator<T> makeAllocator(const char *tag = nullptr) {
    return Allocator<T>();
}

using MemString = std::string;

template<class T>
using MemVector = std::vector<T>;

template<class T>
MemVector<T> makeMemVector(const char *tag) {
    return MemVector<T>();
}

using MemJsonDoc = MemJsonDoc;

template <class T, typename ...Args>
T *mo_mem_new(Args&& ...args)  {
    return new T(std::forward<Args>(args)...);
}

template<class T>
void mo_mem_delete(T *ptr)  {
    delete ptr;
}

} //namespace MicroOcpp

#endif //MO_OVERRIDE_ALLOCATION

namespace MicroOcpp {

MemString makeMemString(const char *tag = nullptr);

MemJsonDoc initMemJsonDoc(size_t capacity, const char *tag = nullptr);
std::unique_ptr<MemJsonDoc> makeMemJsonDoc(size_t capacity, const char *tag = nullptr, const char *tag_suffix = nullptr);

}

#endif //__cplusplus
#endif
