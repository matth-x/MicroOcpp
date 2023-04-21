// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h> //FilesystemOpt
#include <ArduinoOcpp/Debug.h>

#ifndef AO_DEACTIVATE_FLASH

/*
 * Platform specific implementations. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 *     - POSIX-like API (tested on Ubuntu 20.04)
 * 
 * You can add support for other file systems by passing a custom adapter to OCPP_initialize(...)
 */


#if AO_USE_FILEAPI == ARDUINO_LITTLEFS || AO_USE_FILEAPI == ARDUINO_SPIFFS

#if AO_USE_FILEAPI == ARDUINO_LITTLEFS
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#elif AO_USE_FILEAPI == ARDUINO_SPIFFS
#include <FS.h>
#define USE_FS SPIFFS
#endif

namespace ArduinoOcpp {

class ArduinoFileAdapter : public FileAdapter {
    File file;
public:
    ArduinoFileAdapter(File&& file) : file(file) {}

    ~ArduinoFileAdapter() {
        if (file) {
            file.close();
        }
    }
    
    int read() override {
        return file.read();
    };
    size_t read(char *buf, size_t len) override {
        return file.readBytes(buf, len);
    }
    size_t write(const char *buf, size_t len) override {
        return file.printf("%.*s", len, buf);
    }
    size_t seek(size_t offset) override {
        return file.seek(offset);
    }
};

class ArduinoFilesystemAdapter : public FilesystemAdapter {
private:
    bool valid = false;
    FilesystemOpt config;
public:
    ArduinoFilesystemAdapter(FilesystemOpt config) : config(config) {
        valid = true;

        if (config.mustMount()) { 
#if AO_USE_FILEAPI == ARDUINO_LITTLEFS
            if(!USE_FS.begin(config.formatOnFail())) {
                AO_DBG_ERR("Error while mounting LITTLEFS");
                valid = false;
            } else {
                AO_DBG_DEBUG("LittleFS mount success");
            }
#elif AO_USE_FILEAPI == ARDUINO_SPIFFS
            //ESP8266
            SPIFFSConfig cfg;
            cfg.setAutoFormat(config.formatOnFail());
            SPIFFS.setConfig(cfg);

            if (!SPIFFS.begin()) {
                AO_DBG_ERR("Unable to initialize: unable to mount SPIFFS");
                valid = false;
            }
#else
#error
#endif
        } //end if mustMount()

#if AO_DBG_LEVEL >= AO_DL_DEBUG
        if (valid) {
            File root = USE_FS.open("/");
            File file = root.openNextFile();

            AO_DBG_DEBUG("filesystem content:");
            while(file) {
                AO_CONSOLE_PRINTF("[AO]     > %s\n", file.name() ? file.name() : "null");
                file = root.openNextFile();       
            }

            root.close();
        }
#endif //endif AO_DBG_LEVEL >= AO_DL_DBUG
    }

    ~ArduinoFilesystemAdapter() {
        if (config.mustMount()) {
            USE_FS.end();
        }
    }

    operator bool() {return valid;}

    int stat(const char *path, size_t *size) override {
        if (!USE_FS.exists(path)) {
            return -1;
        }
        File f = USE_FS.open(path, "r");
        if (!f) {
            return -1;
        }

        int status = -1;
        if (!f.isDirectory()) {
            *size = f.size();
            status = 0;
        } else {
            //fetch more information for directory when ArduinoOcpp also uses them
            //status = 0;
        }

        f.close();
        return status;
    }

    std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) override {
        File file = USE_FS.open(fn, mode);
        if (file && !file.isDirectory()) {
            AO_DBG_DEBUG("File open successful: %s", fn);
            return std::unique_ptr<FileAdapter>(new ArduinoFileAdapter(std::move(file)));
        } else {
            return nullptr;
        }
    }
    bool remove(const char *fn) override {
        return USE_FS.remove(fn);
    };
    int ftw_root(std::function<int(const char *fpath)> fn) {
#if AO_USE_FILEAPI == ARDUINO_LITTLEFS

#elif AO_USE_FILEAPI == ARDUINO_SPIFFS
        auto dir = USE_FS.open(AO_FILENAME_PREFIX);
        if (!dir) {
            AO_DBG_ERR("cannot open root directory: " AO_FILENAME_PREFIX);
            return -1;
        }

        int err = 0;
        while (auto entry = dir.openNextFile()) {
            err = fn(entry.name());
            if (err) {
                break;
            }
        }
        return err;
#else
#error
#endif
    }
};

std::weak_ptr<FilesystemAdapter> filesystemCache;

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config) {

    if (auto cached = filesystemCache.lock()) {
        return cached;
    }

    if (!config.accessAllowed()) {
        AO_DBG_DEBUG("Access to Arduino FS not allowed by config");
        return nullptr;
    }

    auto fs_concrete = new ArduinoFilesystemAdapter(config);
    auto fs = std::shared_ptr<FilesystemAdapter>(fs_concrete);
    filesystemCache = fs;

    if (*fs_concrete) {
        return fs;
    } else {
        return nullptr;
    }
}

} //end namespace ArduinoOcpp

#elif AO_USE_FILEAPI == ESPIDF_SPIFFS

#include <sys/stat.h>
#include "esp_spiffs.h"

namespace ArduinoOcpp {

class EspIdfFileAdapter : public FileAdapter {
    FILE *file {nullptr};
public:
    EspIdfFileAdapter(FILE *file) : file(file) {}

    ~EspIdfFileAdapter() {
        fclose(file);
    }

    size_t read(char *buf, size_t len) override {
        return fread(buf, 1, len, file);
    }

    size_t write(const char *buf, size_t len) override {
        return fwrite(buf, 1, len, file);
    }

    size_t seek(size_t offset) override {
        return fseek(file, offset, SEEK_SET);
    }

    int read() {
        return fgetc(file);
    }
};

class EspIdfFilesystemAdapter : public FilesystemAdapter {
public:
    FilesystemOpt config;
public:
    EspIdfFilesystemAdapter(FilesystemOpt config) : config(config) { }

    ~EspIdfFilesystemAdapter() {
        if (config.mustMount()) {
            esp_vfs_spiffs_unregister("ao"); //partition label
            AO_DBG_DEBUG("SPIFFS unmounted");
        }
    }

    int stat(const char *path, size_t *size) override {
        struct ::stat st;
        auto ret = ::stat(path, &st);
        if (ret == 0) {
            *size = st.st_size;
        }
        return ret;
    }

    std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) override {
        auto file = fopen(fn, mode);
        if (file) {
            return std::unique_ptr<FileAdapter>(new EspIdfFileAdapter(std::move(file)));
        } else {
            AO_DBG_DEBUG("Failed to open file path %s", fn);
            return nullptr;
        }
    }

    bool remove(const char *fn) override {
        return unlink(fn) == 0;
    }
};

std::weak_ptr<FilesystemAdapter> filesystemCache;

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config) {

    if (auto cached = filesystemCache.lock()) {
        return cached;
    }

    if (!config.accessAllowed()) {
        AO_DBG_DEBUG("Access to ESP-IDF SPIFFS not allowed by config");
        return nullptr;
    }

    bool mounted = true;

    if (config.mustMount()) {
        mounted = false;
        
        esp_vfs_spiffs_conf_t conf = {
            .base_path = AO_FILENAME_PREFIX,
            .partition_label = "ao", //also see deconstructor
            .max_files = 5,
            .format_if_mount_failed = config.formatOnFail()
        };

        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret == ESP_OK) {
            mounted = true;
            AO_DBG_DEBUG("SPIFFS mounted");
        } else {
            if (ret == ESP_FAIL) {
                AO_DBG_ERR("Failed to mount or format filesystem");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                AO_DBG_ERR("Failed to find SPIFFS partition");
            } else {
                AO_DBG_ERR("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            }
        }
    }

    if (mounted) {
        auto fs = std::shared_ptr<FilesystemAdapter>(new EspIdfFilesystemAdapter(config));
        filesystemCache = fs;
        return fs;
    } else {
        return nullptr;
    }
}

} //end namespace ArduinoOcpp

#elif AO_USE_FILEAPI == POSIX_FILEAPI

#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

namespace ArduinoOcpp {

class PosixFileAdapter : public FileAdapter {
    FILE *file {nullptr};
public:
    PosixFileAdapter(FILE *file) : file(file) {}

    ~PosixFileAdapter() {
        fclose(file);
    }

    size_t read(char *buf, size_t len) override {
        return fread(buf, 1, len, file);
    }

    size_t write(const char *buf, size_t len) override {
        return fwrite(buf, 1, len, file);
    }

    size_t seek(size_t offset) override {
        return fseek(file, offset, SEEK_SET);
    }

    int read() {
        return fgetc(file);
    }
};

class PosixFilesystemAdapter : public FilesystemAdapter {
public:
    FilesystemOpt config;
public:
    PosixFilesystemAdapter(FilesystemOpt config) : config(config) { }

    ~PosixFilesystemAdapter() = default;

    int stat(const char *path, size_t *size) override {
        struct ::stat st;
        auto ret = ::stat(path, &st);
        if (ret == 0) {
            *size = st.st_size;
        }
        return ret;
    }

    std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) override {
        auto file = fopen(fn, mode);
        if (file) {
            return std::unique_ptr<FileAdapter>(new PosixFileAdapter(std::move(file)));
        } else {
            AO_DBG_DEBUG("Failed to open file path %s", fn);
            return nullptr;
        }
    }

    bool remove(const char *fn) override {
        return ::remove(fn) == 0;
    }

    int ftw_root(std::function<int(const char *fpath)> fn) {
        auto dir = opendir(AO_FILENAME_PREFIX); // use c_str() to convert the path string to a C-style string
        if (!dir) {
            AO_DBG_ERR("cannot open root directory: " AO_FILENAME_PREFIX);
            return -1;
        }

        int err = 0;
        while (auto entry = readdir(dir)) {
            err = fn(entry->d_name);
            if (err) {
                break;
            }
        }

        closedir(dir);
        return err;
    }
};

std::weak_ptr<FilesystemAdapter> filesystemCache;

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config) {

    if (auto cached = filesystemCache.lock()) {
        return cached;
    }

    if (!config.accessAllowed()) {
        AO_DBG_DEBUG("Access to FS not allowed by config");
        return nullptr;
    }

    if (config.mustMount()) {
        AO_DBG_DEBUG("Skip mounting on UNIX host");
    }

    auto fs = std::shared_ptr<FilesystemAdapter>(new PosixFilesystemAdapter(config));
    filesystemCache = fs;
    return fs;
}

} //end namespace ArduinoOcpp

#endif //switch-case AO_USE_FILEAPI

#endif //!AO_DEACTIVATE_FLASH
