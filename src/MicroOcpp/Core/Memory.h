// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MEMORY_H
#define MO_MEMORY_H

#include <stddef.h>

#ifndef MO_OVERRIDE_ALLOCATION
#define MO_OVERRIDE_ALLOCATION 0
#endif

#ifndef MO_ENABLE_EXTERNAL_RAM
#define MO_ENABLE_EXTERNAL_RAM 0
#endif

#ifndef MO_ENABLE_HEAP_PROFILER
#define MO_ENABLE_HEAP_PROFILER 0
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if MO_OVERRIDE_ALLOCATION

void mo_mem_set_malloc_free(void* (*malloc_override)(size_t), void (*free_override)(void*)); //pass custom malloc and free function to be used with the OCPP lib. If not set or NULL, defaults to standard malloc

void *mo_mem_malloc(const char *tag, size_t size);

void mo_mem_free(void* ptr);

#define MO_MALLOC mo_mem_malloc
#define MO_FREE mo_mem_free

#else
#define MO_MALLOC(TAG, SIZE) malloc(SIZE) //default malloc provided by host system
#define MO_FREE(PTR) free(PTR)       //default free provided by host system
#endif //MO_OVERRIDE_ALLOCATION


#if MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER

void mo_mem_deinit(); //release allocated memory and deinit
void mo_mem_reset(); //reset maximum heap occuption

void mo_mem_set_tag(void *ptr, const char *tag);

void mo_mem_get_current_heap(const char *tag);
void mo_mem_get_maximum_heap(const char *tag);
void mo_mem_get_current_heap_by_tag(const char *tag);
void mo_mem_get_maximum_heap_by_tag(const char *tag);

int mo_mem_write_stats_json(char *buf, size_t size);

void mo_mem_print_stats();

#define MO_MEM_DEINIT mo_mem_deinit
#define MO_MEM_RESET mo_mem_reset
#define MO_MEM_SET_TAG mo_mem_set_tag
#define MO_MEM_PRINT_STATS mo_mem_print_stats

#else
#define MO_MEM_DEINIT(...) (void)0
#define MO_MEM_RESET(...) (void)0
#define MO_MEM_SET_TAG(...) (void)0
#define MO_MEM_PRINT_STATS(...) (void)0
#endif //MO_OVERRIDE_ALLOCATION && MO_ENABLE_HEAP_PROFILER


#if MO_ENABLE_EXTERNAL_RAM

void mo_mem_set_malloc_free_ext(void* (*malloc_override)(size_t), void (*free_override)(void*)); //pass malloc and free function to external RAM to be used with the OCPP lib. If not set or NULL, defaults to standard malloc

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


#include <memory>
#include <string>
#include <vector>

#include <ArduinoJson.h>

#if MO_OVERRIDE_ALLOCATION

#include <string.h>

namespace MicroOcpp {

class MemoryManaged {
private:
    #if MO_ENABLE_HEAP_PROFILER
    char *tag = nullptr;
    #endif
protected:
    void updateMemoryTag(const char *src1, const char *src2 = nullptr) {
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
        tag = static_cast<char*>(malloc(size)); //heap profiler bypasses custom malloc to not count into the statistics
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
        mo_mem_set_tag(this, tag);
        #else
        (void)src1;
        (void)src2;
        #endif
    }
    const char *getMemoryTag() const {
        #if MO_ENABLE_HEAP_PROFILER
        return tag;
        #else
        return nullptr;
        #endif
    }
public:
    void *operator new(size_t size) {
        return MO_MALLOC(nullptr, size);
    }
    void operator delete(void * ptr) {
        MO_FREE(ptr);
    }

    MemoryManaged(const char *tag = nullptr, const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(tag, tag_suffix);
        #endif
    }

    MemoryManaged(MemoryManaged&& other) {
        #if MO_ENABLE_HEAP_PROFILER
        tag = other.tag;
        other.tag = nullptr;
        #endif
    }

    ~MemoryManaged() {
        #if MO_ENABLE_HEAP_PROFILER
        MO_FREE(tag);
        tag = nullptr;
        #endif
    }

    void operator=(const MemoryManaged& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(other.tag);
        #endif
    }
};

template<class T>
struct Allocator {

    Allocator(const char *tag = nullptr, const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(tag, tag_suffix);
        #endif
    }

    template<class U>
    Allocator(const Allocator<U>& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(other.tag);
        #endif
    }

    Allocator(const Allocator& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(other.tag);
        #endif
    }

    //template<class U>
    //Allocator(Allocator<U>&& other) {
    Allocator(Allocator&& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(other.tag); //ignore move semantics for allocators as it simplifies moving std::vector<T, Allocator<T>>. This is okay because the Allocator's state is only the memory tag which is not exclusively owned
        #endif
    }

    ~Allocator() {
        #if MO_ENABLE_HEAP_PROFILER
        if (tag) {
            //MO_FREE(tag);
            free(tag);
            tag = nullptr;
        }
        #endif
    }

    T *allocate(size_t count) {
        #if MO_ENABLE_HEAP_PROFILER
            return static_cast<T*>(MO_MALLOC(tag, sizeof(T) * count));
        #else
            return static_cast<T*>(MO_MALLOC(nullptr, sizeof(T) * count));
        #endif
    }

    void deallocate(T *ptr, size_t count) {
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

    void updateMemoryTag(const char *src1, const char *src2 = nullptr) {
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
            //MO_FREE(tag);
            free(tag);
            tag = nullptr;
        }
        size_t size = strlen(src) + 1;
        tag = static_cast<char*>(malloc(size));
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
    }
    #endif
};

template<class T>
Allocator<T> makeAllocator(const char *tag, const char *tag_suffix = nullptr) {
    return Allocator<T>(tag, tag_suffix);
}

using String = std::basic_string<char, std::char_traits<char>, MicroOcpp::Allocator<char>>;

template<class T>
using Vector = std::vector<T, Allocator<T>>;

template<class T>
Vector<T> makeVector(const char *tag) {
    return Vector<T>(Allocator<T>(tag));
}

class ArduinoJsonAllocator {
private:
    #if MO_ENABLE_HEAP_PROFILER
    char *tag = nullptr;

    void updateMemoryTag(const char *src1, const char *src2 = nullptr) {
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
        //tag = static_cast<char*>(MO_MALLOC("HeapProfilerInternal", size));
        tag = static_cast<char*>(malloc(size));
        memset(tag, 0, size);
        snprintf(tag, size, "%s", src);
    }
    #endif
public:

    ArduinoJsonAllocator(const char *tag = nullptr, const char *tag_suffix = nullptr) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(tag, tag_suffix);
        #endif
    }

    ArduinoJsonAllocator(const ArduinoJsonAllocator& other) {
        #if MO_ENABLE_HEAP_PROFILER
        updateMemoryTag(other.tag);
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
        #if MO_ENABLE_HEAP_PROFILER
            return MO_MALLOC(tag, size);
        #else
            return MO_MALLOC(nullptr, size);
        #endif
    }
    void deallocate(void *ptr) {
        MO_FREE(ptr);
    }
};

using JsonDoc = BasicJsonDocument<ArduinoJsonAllocator>;

template<class T, typename ...Args>
T *mo_mem_new(const char *tag, Args&& ...args)  {
    if (auto ptr = MO_MALLOC(tag, sizeof(T))) {
        return new(ptr) T(std::forward<Args>(args)...);
    }
    return nullptr; //OOM
}

template<class T>
void mo_mem_delete(T *ptr)  {
    if (ptr) {
        ptr->~T();
        MO_FREE(ptr);
    }
}

} //namespace MicroOcpp

#else

#include <memory>

namespace MicroOcpp {

class MemoryManaged {
protected:
    const char *getMemoryTag() const {return nullptr;}
    void updateMemoryTag(const char*,const char*) { } 
public:
    MemoryManaged() { }
    MemoryManaged(const char*) { }
    MemoryManaged(const char*,const char*) { }
};

template<class T>
using Allocator = ::std::allocator<T>;

template<class T>
Allocator<T> makeAllocator(const char *, const char *unused = nullptr) {
    (void)unused;
    return Allocator<T>();
}

using String = std::string;

template<class T>
using Vector = std::vector<T>;

template<class T>
Vector<T> makeVector(const char *tag) {
    return Vector<T>();
}

using JsonDoc = DynamicJsonDocument;

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

String makeString(const char *tag, const char *val = nullptr);

JsonDoc initJsonDoc(const char *tag, size_t capacity = 0);
std::unique_ptr<JsonDoc> makeJsonDoc(const char *tag, size_t capacity = 0);

}

#endif //__cplusplus
#endif
