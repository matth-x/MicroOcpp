// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AO_FILESYSTEMADAPTER_H
#define AO_FILESYSTEMADAPTER_H

#ifndef AO_FILENAME_PREFIX
#define AO_FILENAME_PREFIX ""
#endif

#define MAX_PATH_SIZE 30

#define ARDUINO_LITTLEFS 1
#define ARDUINO_SPIFFS   2
#define ESPIDF_SPIFFS    3
#define POSIX_FILEAPI    4

#include <memory>

namespace ArduinoOcpp {

class FileAdapter {
public:
    virtual ~FileAdapter() = default;
    virtual size_t read(char *buf, size_t len) = 0;
    virtual size_t write(const char *buf, size_t len) = 0;
    virtual size_t seek(size_t offset) = 0;
    // virtual void close() = 0; implemented in deconstructor

    virtual int read() = 0;
};

class FilesystemAdapter {
public:
    virtual ~FilesystemAdapter() = default;
    virtual int stat(const char *path, size_t *size) = 0;
    virtual std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) = 0;
    virtual bool remove(const char *fn) = 0;
};

} //end namespace ArduinoOcpp

#ifndef AO_DEACTIVATE_FLASH

//Set default parameters; assume usage with Arduino if no build flags are present
#ifndef AO_USE_FILEAPI
#if defined(ESP32)
#define AO_USE_FILEAPI ARDUINO_LITTLEFS
#else
#define AO_USE_FILEAPI ARDUINO_SPIFFS
#endif
#endif //ndef AO_USE_FILEAPI

/*
 * Platform specific implementations. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 * 
 * You can add support for any file system by passing custom adapters to the initialize
 * function of ArduinoOcpp
 */

#if AO_USE_FILEAPI == ARDUINO_LITTLEFS || \
    AO_USE_FILEAPI == ARDUINO_SPIFFS || \
    AO_USE_FILEAPI == ESPIDF_SPIFFS || \
    AO_USE_FILEAPI == POSIX_FILEAPI

#include <ArduinoOcpp/Core/ConfigurationOptions.h>

namespace ArduinoOcpp {

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config);

} //end namespace ArduinoOcpp
#endif

#endif //ndef AO_DEACTIVATE_FLASH

#endif
