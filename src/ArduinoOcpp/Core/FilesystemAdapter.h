// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_FILESYSTEMADAPTER_H
#define AO_FILESYSTEMADAPTER_H

#include <memory>
#include <functional>

#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h>

#ifndef AO_FILENAME_PREFIX
#define AO_FILENAME_PREFIX "/"
#endif

#define DISABLE_FS       0
#define ARDUINO_LITTLEFS 1
#define ARDUINO_SPIFFS   2
#define ESPIDF_SPIFFS    3
#define POSIX_FILEAPI    4

// choose FileAPI if not given by build flag; assume usage with Arduino if no build flags are present
#ifndef AO_USE_FILEAPI
#if AO_PLATFORM == AO_PLATFORM_ARDUINO
#if defined(ESP32)
#define AO_USE_FILEAPI ARDUINO_LITTLEFS
#else
#define AO_USE_FILEAPI ARDUINO_SPIFFS
#endif
#elif AO_PLATFORM == AO_PLATFORM_ESPIDF
#define AO_USE_FILEAPI ESPIDF_SPIFFS
#elif AO_PLATFORM == AO_PLATFORM_UNIX
#define AO_USE_FILEAPI POSIX_FILEAPI
#else
#define AO_USE_FILEAPI DISABLE_FS
#endif //switch-case AO_PLATFORM
#endif //ndef AO_USE_FILEAPI

// set default max path size parameters
#ifndef AO_MAX_PATH_SIZE
#if AO_USE_FILEAPI == POSIX_FILEAPI
#define AO_MAX_PATH_SIZE 128
#else
#define AO_MAX_PATH_SIZE 30
#endif
#endif

namespace ArduinoOcpp {

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
    virtual int ftw_root(std::function<int(const char *fpath)> fn) = 0; //enumerate the files in the ao_store root folder
};

/*
 * Platform specific implementation. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 *     - POSIX-like API (tested on Ubuntu 20.04)
 * 
 * You can add support for other file systems by passing a custom adapter to ocpp_initialize(...)
 * 
 * Returns null if platform is not supported or Filesystem is disabled
 */
std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config);

#define AO_ENABLE_FILE_INDEX 1 //always enable in hotfix

#if AO_ENABLE_FILE_INDEX

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapterIndexed(FilesystemOpt config);

#endif // AO_ENABLE_FILE_INDEX

} //end namespace ArduinoOcpp

#endif
