// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MOCPP_FILESYSTEMADAPTER_H
#define MOCPP_FILESYSTEMADAPTER_H

#include <memory>
#include <functional>

#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Core/ConfigurationOptions.h>

#ifndef MOCPP_FILENAME_PREFIX
#define MOCPP_FILENAME_PREFIX "/"
#endif

#define DISABLE_FS       0
#define ARDUINO_LITTLEFS 1
#define ARDUINO_SPIFFS   2
#define ESPIDF_SPIFFS    3
#define POSIX_FILEAPI    4

// choose FileAPI if not given by build flag; assume usage with Arduino if no build flags are present
#ifndef MOCPP_USE_FILEAPI
#if MOCPP_PLATFORM == MOCPP_PLATFORM_ARDUINO
#if defined(ESP32)
#define MOCPP_USE_FILEAPI ARDUINO_LITTLEFS
#else
#define MOCPP_USE_FILEAPI ARDUINO_SPIFFS
#endif
#elif MOCPP_PLATFORM == MOCPP_PLATFORM_ESPIDF
#define MOCPP_USE_FILEAPI ESPIDF_SPIFFS
#elif MOCPP_PLATFORM == MOCPP_PLATFORM_UNIX
#define MOCPP_USE_FILEAPI POSIX_FILEAPI
#else
#define MOCPP_USE_FILEAPI DISABLE_FS
#endif //switch-case MOCPP_PLATFORM
#endif //ndef MOCPP_USE_FILEAPI

// set default max path size parameters
#ifndef MOCPP_MAX_PATH_SIZE
#if MOCPP_USE_FILEAPI == POSIX_FILEAPI
#define MOCPP_MAX_PATH_SIZE 128
#else
#define MOCPP_MAX_PATH_SIZE 30
#endif
#endif

namespace MicroOcpp {

class FileAdapter {
public:
    virtual ~FileAdapter() = default;
    virtual size_t read(char *buf, size_t len) = 0;
    virtual size_t write(const char *buf, size_t len) = 0;
    virtual size_t seek(size_t offset) = 0;

    virtual int read() = 0;
};

class FilesystemAdapter {
public:
    virtual ~FilesystemAdapter() = default;
    virtual int stat(const char *path, size_t *size) = 0;
    virtual std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) = 0;
    virtual bool remove(const char *fn) = 0;
    virtual int ftw_root(std::function<int(const char *fpath)> fn) = 0; //enumerate the files in the mo_store root folder
};

/*
 * Platform specific implementation. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 *     - POSIX-like API (tested on Ubuntu 20.04)
 * 
 * You can add support for other file systems by passing a custom adapter to mocpp_initialize(...)
 * 
 * Returns null if platform is not supported or Filesystem is disabled
 */
std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config);

} //end namespace MicroOcpp

#endif
