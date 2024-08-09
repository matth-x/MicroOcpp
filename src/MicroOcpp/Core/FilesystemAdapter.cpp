// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/ConfigurationOptions.h> //FilesystemOpt
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#include <cstring>

/*
 * Platform specific implementations. Currently supported:
 *     - Arduino LittleFs
 *     - Arduino SPIFFS
 *     - ESP-IDF SPIFFS
 *     - POSIX-like API (tested on Ubuntu 20.04)
 * Plus a filesystem index decorator working with any of the above
 * 
 * You can add support for other file systems by passing a custom adapter to mocpp_initialize(...)
 */

#if MO_ENABLE_FILE_INDEX

#include <vector>
#include <algorithm>
#include <string>

namespace MicroOcpp {

class FilesystemAdapterIndex;

class IndexedFileAdapter : public FileAdapter, public AllocOverrider {
private:
    FilesystemAdapterIndex& index;
    char fn [MO_MAX_PATH_SIZE];
    std::unique_ptr<FileAdapter> file;

    size_t written = 0;
public:
    IndexedFileAdapter(FilesystemAdapterIndex& index, const char *fn, std::unique_ptr<FileAdapter> file)
            : AllocOverrider("FilesystemIndex"), index(index), file(std::move(file)) {
        snprintf(this->fn, sizeof(this->fn), "%s", fn);
    }

    ~IndexedFileAdapter(); // destructor updates file index with written size

    size_t read(char *buf, size_t len) override {
        return file->read(buf, len);
    }

    size_t write(const char *buf, size_t len) override {
        auto ret = file->write(buf, len);
        written += ret;
        return ret;
    }

    size_t seek(size_t offset) override {
        auto ret = file->seek(offset);
        written = ret;
        return ret;
    }

    int read() override {
        return file->read();
    }
};

class FilesystemAdapterIndex : public FilesystemAdapter, public AllocOverrider {
private:
    std::shared_ptr<FilesystemAdapter> filesystem;

    struct IndexEntry {
        MemString fname;
        size_t size;

        IndexEntry(const char *fname, size_t size) : fname(makeMemString(fname, "FilesystemIndex")), size(size) { }
    };

    MemVector<IndexEntry> index;

    IndexEntry *getEntryByFname(const char *fn) {
        auto entry = std::find_if(index.begin(), index.end(),
            [fn] (const IndexEntry& el) -> bool {
                return el.fname.compare(fn) == 0;
            });

        if (entry != index.end()) {
            return &(*entry);
        } else {
            return nullptr;
        }
    }

    IndexEntry *getEntryByPath(const char *path) {
        if (strlen(path) < sizeof(MO_FILENAME_PREFIX) - 1) {
            MO_DBG_ERR("invalid fn");
            return nullptr;
        }

        const char *fn = path + sizeof(MO_FILENAME_PREFIX) - 1;
        return getEntryByFname(fn);
    }
public:
    FilesystemAdapterIndex(std::shared_ptr<FilesystemAdapter> filesystem) : AllocOverrider("FilesystemIndex"), filesystem(std::move(filesystem)), index(makeMemVector<IndexEntry>(getMemoryTag())) { }

    ~FilesystemAdapterIndex() = default;

    int stat(const char *path, size_t *size) override {
        if (auto file = getEntryByPath(path)) {
            *size = file->size;
            return 0;
        } else {
            return -1;
        }
    }

    std::unique_ptr<FileAdapter> open(const char *path, const char *mode) {
        if (!strcmp(mode, "r")) {
            return filesystem->open(path, "r");
        } else if (!strcmp(mode, "w")) {

            if (strlen(path) < sizeof(MO_FILENAME_PREFIX) - 1) {
                MO_DBG_ERR("invalid fn");
                return nullptr;
            }

            const char *fn = path + sizeof(MO_FILENAME_PREFIX) - 1;

            auto file = filesystem->open(path, "w");
            if (!file) {
                return nullptr;
            }

            IndexEntry *entry = nullptr;
            if (!(entry = getEntryByFname(fn))) {
                index.emplace_back(fn, 0);
                entry = &index.back();
            }

            if (!entry) {
                MO_DBG_ERR("internal error");
                return nullptr;
            }

            entry->size = 0; //write always empties the file

            return std::unique_ptr<IndexedFileAdapter>(new IndexedFileAdapter(*this, entry->fname.c_str(), std::move(file)));
        } else {
            MO_DBG_ERR("only support r or w");
            return nullptr;
        }
    }

    bool remove(const char *path) override {
        if (strlen(path) >= sizeof(MO_FILENAME_PREFIX) - 1) {
            //valid path
            const char *fn = path + sizeof(MO_FILENAME_PREFIX) - 1;
            index.erase(std::remove_if(index.begin(), index.end(),
                [fn] (const IndexEntry& el) -> bool {
                    return el.fname.compare(fn) == 0;
                }), index.end());
        }

        return filesystem->remove(path);
    }

    int ftw_root(std::function<int(const char *fpath)> fn) {
        // allow fn to remove elements
        for (size_t it = 0; it < index.size();) {
            auto size_before = index.size();
            auto err = fn(index[it].fname.c_str());
            if (err) {
                return err;
            }
            if (index.size() + 1 == size_before) {
                // element removed
                continue;
            }
            // normal execution
            it++;
        }

        return 0;
    }

    bool createIndex() {
        if (!index.empty()) {
            return false;
        }
        auto ret = filesystem->ftw_root([this] (const char *fn) -> int {
            int ret;
            char path [MO_MAX_PATH_SIZE];

            ret = snprintf(path, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "%s", fn);
            if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                MO_DBG_ERR("fn error: %i", ret);
                return 0; //ignore this entry and continue ftw
            }

            size_t size;
            ret = filesystem->stat(path, &size);
            if (ret == 0) {
                //add fn and size to index
                MO_DBG_DEBUG("add file to index: %s (%zuB)", fn, size);
                index.emplace_back(fn, size);
                return 0; //successfully added filename to index
            } else {
                MO_DBG_ERR("unexpected entry: %s", fn);
                return 0; //ignore this entry and continue ftw
            }
        });

        MO_DBG_DEBUG("create fs index: %s, %zu entries", ret == 0 ? "success" : "failure", index.size());

        return ret == 0;
    }

    void updateFilesize(const char *fn, size_t size) {
        if (auto entry = getEntryByFname(fn)) {
            entry->size = size;
            MO_DBG_DEBUG("update index: %s (%zuB)", entry->fname.c_str(), entry->size);
        }
    }
};

IndexedFileAdapter::~IndexedFileAdapter() {
    index.updateFilesize(fn, written);
}

std::shared_ptr<FilesystemAdapter> decorateIndex(std::shared_ptr<FilesystemAdapter> filesystem) {

    auto fsIndex = std::allocate_shared<FilesystemAdapterIndex>(Allocator<FilesystemAdapterIndex>("FilesystemIndex"), std::move(filesystem));
    if (!fsIndex) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    if (!fsIndex->createIndex()) {
        MO_DBG_ERR("createIndex err");
        return nullptr;
    }

    return fsIndex;
}

} // namespace MicroOcpp

#endif //MO_ENABLE_FILE_INDEX

#if MO_USE_FILEAPI == ARDUINO_LITTLEFS || MO_USE_FILEAPI == ARDUINO_SPIFFS

#if MO_USE_FILEAPI == ARDUINO_LITTLEFS
#include <LittleFS.h>
#include <vfs_api.h>
#define USE_FS LittleFS
#elif MO_USE_FILEAPI == ARDUINO_SPIFFS
#include <FS.h>
#define USE_FS SPIFFS
#endif

namespace MicroOcpp {

class ArduinoFileAdapter : public FileAdapter, public AllocOverrider {
    File file;
public:
    ArduinoFileAdapter(File&& file) : AllocOverrider("Filesystem"), file(file) {}

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

class ArduinoFilesystemAdapter : public FilesystemAdapter, public AllocOverrider {
private:
    bool valid = false;
    FilesystemOpt config;
public:
    ArduinoFilesystemAdapter(FilesystemOpt config) : AllocOverrider("Filesystem"), config(config) {
        valid = true;

        if (config.mustMount()) { 
#if MO_USE_FILEAPI == ARDUINO_LITTLEFS
            if(!USE_FS.begin(config.formatOnFail())) {
                MO_DBG_ERR("Error while mounting LITTLEFS");
                valid = false;
            } else {
                MO_DBG_DEBUG("LittleFS mount success");
            }
#elif MO_USE_FILEAPI == ARDUINO_SPIFFS
            //ESP8266
            SPIFFSConfig cfg;
            cfg.setAutoFormat(config.formatOnFail());
            SPIFFS.setConfig(cfg);

            if (!SPIFFS.begin()) {
                MO_DBG_ERR("Unable to initialize: unable to mount SPIFFS");
                valid = false;
            }
#else
#error
#endif
        } //end if mustMount()

    }

    ~ArduinoFilesystemAdapter() {
        if (config.mustMount()) {
            USE_FS.end();
        }
    }

    operator bool() {return valid;}

    int stat(const char *path, size_t *size) override {
#if MO_USE_FILEAPI == ARDUINO_LITTLEFS
        char partition_path [MO_MAX_PATH_SIZE];
        auto ret = snprintf(partition_path, MO_MAX_PATH_SIZE, "/littlefs%s", path);
        if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", ret);
            return -1;
        }
        struct ::stat st;
        auto status = ::stat(partition_path, &st);
        if (status == 0) {
            *size = st.st_size;
        }
        return status;
#elif MO_USE_FILEAPI == ARDUINO_SPIFFS
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
            //fetch more information for directory when MicroOcpp also uses them
            //status = 0;
        }

        f.close();
        return status;
#else
#error
#endif
    } //end stat

    std::unique_ptr<FileAdapter> open(const char *fn, const char *mode) override {
        File file = USE_FS.open(fn, mode);
        if (file && !file.isDirectory()) {
            MO_DBG_DEBUG("File open successful: %s", fn);
            return std::unique_ptr<FileAdapter>(new ArduinoFileAdapter(std::move(file)));
        } else {
            return nullptr;
        }
    }
    bool remove(const char *fn) override {
        return USE_FS.remove(fn);
    };
    int ftw_root(std::function<int(const char *fpath)> fn) override {
#if MO_USE_FILEAPI == ARDUINO_LITTLEFS
        auto dir = USE_FS.open(MO_FILENAME_PREFIX);
        if (!dir) {
            MO_DBG_ERR("cannot open root directory: " MO_FILENAME_PREFIX);
            return -1;
        }

        int err = 0;
        while (auto entry = dir.openNextFile()) {

            char fname [MO_MAX_PATH_SIZE];
            auto ret = snprintf(fname, MO_MAX_PATH_SIZE, "%s", entry.name());
            entry.close();

            if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                MO_DBG_ERR("fn error: %i", ret);
                return -1;
            }

            err = fn(fname);
            if (err) {
                break;
            }
        }
        return err;
#elif MO_USE_FILEAPI == ARDUINO_SPIFFS
        auto dir = USE_FS.openDir(MO_FILENAME_PREFIX);
        int err = 0;
        while (dir.next()) {
            auto fname = dir.fileName();
            if (fname.c_str()) {
                err = fn(fname.c_str() + strlen(MO_FILENAME_PREFIX));
            } else {
                MO_DBG_ERR("fs error");
                err = -1;
            }
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
        MO_DBG_DEBUG("Access to Arduino FS not allowed by config");
        return nullptr;
    }

    auto fs_concrete = new ArduinoFilesystemAdapter(config);
    auto fs = std::shared_ptr<FilesystemAdapter>(fs_concrete, std::default_delete<FilesystemAdapter>(), Allocator<FilesystemAdapter>("Filesystem"));

#if MO_ENABLE_FILE_INDEX
    fs = decorateIndex(fs);
#endif // MO_ENABLE_FILE_INDEX

    filesystemCache = fs;

    if (*fs_concrete) {
        return fs;
    } else {
        return nullptr;
    }
}

} //end namespace MicroOcpp

#elif MO_USE_FILEAPI == ESPIDF_SPIFFS

#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"

#ifndef MO_PARTITION_LABEL
#define MO_PARTITION_LABEL "mo"
#endif

namespace MicroOcpp {

class EspIdfFileAdapter : public FileAdapter, public AllocOverrider {
    FILE *file {nullptr};
public:
    EspIdfFileAdapter(FILE *file) : AllocOverrider("Filesystem"), file(file) {}

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

    int read() override {
        return fgetc(file);
    }
};

class EspIdfFilesystemAdapter : public FilesystemAdapter, public AllocOverrider {
public:
    FilesystemOpt config;
public:
    EspIdfFilesystemAdapter(FilesystemOpt config) : AllocOverrider("Filesystem"), config(config) { }

    ~EspIdfFilesystemAdapter() {
        if (config.mustMount()) {
            esp_vfs_spiffs_unregister(MO_PARTITION_LABEL);
            MO_DBG_DEBUG("SPIFFS unmounted");
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
            MO_DBG_DEBUG("Failed to open file path %s", fn);
            return nullptr;
        }
    }

    bool remove(const char *fn) override {
        return unlink(fn) == 0;
    }

    int ftw_root(std::function<int(const char *fpath)> fn) override {
        //open MO root directory
        char dname [MO_MAX_PATH_SIZE];
        auto dlen = snprintf(dname, MO_MAX_PATH_SIZE, "%s", MO_FILENAME_PREFIX);
        if (dlen < 0 || dlen >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("fn error: %i", dlen);
            return -1;
        }

        // trim trailing '/' if not root directory
        if (dlen >= 2 && dname[dlen - 1] == '/') {
            dname[dlen - 1] = '\0';
        }

        auto dir = opendir(dname);
        if (!dir) {
            MO_DBG_ERR("cannot open root directory: %s", dname);
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
        MO_DBG_DEBUG("Access to ESP-IDF SPIFFS not allowed by config");
        return nullptr;
    }

    bool mounted = true;

    if (config.mustMount()) {
        mounted = false;

        char fnpref [MO_MAX_PATH_SIZE];
        auto fnpref_len = snprintf(fnpref, MO_MAX_PATH_SIZE, "%s", MO_FILENAME_PREFIX);
        if (fnpref_len < 0 || fnpref_len >= MO_MAX_PATH_SIZE) {
            MO_DBG_ERR("MO_FILENAME_PREFIX error %i", fnpref_len);
            return nullptr;
        } else if (fnpref_len <= 2) { //shortest possible prefix: "/p/", i.e. length = 3
            MO_DBG_ERR("MO_FILENAME_PREFIX cannot be root on ESP-IDF (working example: \"/mo_store/\")");
            return nullptr;
        }

        // trim trailing '/'
        if (fnpref[fnpref_len - 1] == '/') {
            fnpref[fnpref_len - 1] = '\0';
        }

        esp_vfs_spiffs_conf_t conf = {
            .base_path = fnpref,
            .partition_label = MO_PARTITION_LABEL,
            .max_files = 5,
            .format_if_mount_failed = config.formatOnFail()
        };

        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret == ESP_OK) {
            mounted = true;
            MO_DBG_DEBUG("SPIFFS mounted");
        } else {
            if (ret == ESP_FAIL) {
                MO_DBG_ERR("Failed to mount or format filesystem");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                MO_DBG_ERR("Failed to find SPIFFS partition");
            } else {
                MO_DBG_ERR("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            }
        }
    }

    if (mounted) {
        auto fs = std::shared_ptr<FilesystemAdapter>(new EspIdfFilesystemAdapter(config), std::default_delete<FilesystemAdapter>(), Allocator<FilesystemAdapter>("Filesystem"));

#if MO_ENABLE_FILE_INDEX
        fs = decorateIndex(fs);
#endif // MO_ENABLE_FILE_INDEX

        filesystemCache = fs;
        return fs;
    } else {
        return nullptr;
    }
}

} //end namespace MicroOcpp

#elif MO_USE_FILEAPI == POSIX_FILEAPI

#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

namespace MicroOcpp {

class PosixFileAdapter : public FileAdapter, public AllocOverrider {
    FILE *file {nullptr};
public:
    PosixFileAdapter(FILE *file) : AllocOverrider("Filesystem"), file(file) {}

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

    int read() override {
        return fgetc(file);
    }
};

class PosixFilesystemAdapter : public FilesystemAdapter, public AllocOverrider {
public:
    FilesystemOpt config;
public:
    PosixFilesystemAdapter(FilesystemOpt config) : AllocOverrider("Filesystem"), config(config) { }

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
            MO_DBG_DEBUG("Failed to open file path %s", fn);
            return nullptr;
        }
    }

    bool remove(const char *fn) override {
        return ::remove(fn) == 0;
    }

    int ftw_root(std::function<int(const char *fpath)> fn) override {
        auto dir = opendir(MO_FILENAME_PREFIX); // use c_str() to convert the path string to a C-style string
        if (!dir) {
            MO_DBG_ERR("cannot open root directory: " MO_FILENAME_PREFIX);
            return -1;
        }

        int err = 0;
        while (auto entry = readdir(dir)) {
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                continue; //files . and .. are specific to desktop systems and rarely appear on microcontroller filesystems. Filter them
            }
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
        MO_DBG_DEBUG("Access to FS not allowed by config");
        return nullptr;
    }

    if (config.mustMount()) {
        MO_DBG_DEBUG("Skip mounting on UNIX host");
    }

    auto fs = std::shared_ptr<FilesystemAdapter>(new PosixFilesystemAdapter(config), std::default_delete<FilesystemAdapter>(), Allocator<FilesystemAdapter>("Filesystem"));

#if MO_ENABLE_FILE_INDEX
    fs = decorateIndex(fs);
#endif // MO_ENABLE_FILE_INDEX

    filesystemCache = fs;
    return fs;
}

} //end namespace MicroOcpp

#else //filesystem disabled

namespace MicroOcpp {

std::shared_ptr<FilesystemAdapter> makeDefaultFilesystemAdapter(FilesystemOpt config) {
    return nullptr;
}

} //end namespace MicroOcpp

#endif //switch-case MO_USE_FILEAPI
