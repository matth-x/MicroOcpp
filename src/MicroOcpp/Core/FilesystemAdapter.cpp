// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

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

#include <algorithm>

namespace MicroOcpp {
namespace IndexedFS {

struct FilesystemAdapterIndex;

struct IndexedFileAdapter : public MemoryManaged {
    FilesystemAdapterIndex& index;
    MO_File *file = nullptr; //takes ownership
    char *fname = nullptr; //no ownership
    size_t written = 0;

    IndexedFileAdapter(FilesystemAdapterIndex& index) : MemoryManaged("FilesystemIndex"), index(index) { }
};

struct FilesystemAdapterIndex : public MemoryManaged {
public:
    MO_FilesystemAdapter *filesystem = nullptr; //takes ownership

    struct IndexEntry {
        char *fname = nullptr; //takes ownership
        size_t size;
        IndexEntry(const char *fname, size_t size) : fname(fname), size(size) { }
        ~IndexEntry() {
            MO_FREE(fname);
            fname = nullptr;
        }
    };

    Vector<IndexEntry> fileEntries;

    IndexEntry *getEntryByPath(const char *path) {
        const char *prefix = filesystem->path_prefix ? filesystem->path_prefix : "";
        size_t prefix_len = strlen(prefix);

        if (strncmp(path, prefix, prefix_len)) {
            return nullptr;
        }

        const char *fname = path + prefix_len;

        for (size_t i = 0; i < fileEntries.size(); i++) {
            if (!strcmp(fileEntries[i].fname, fname)) {
                return &fileEntries[i];
            }
        }
        return nullptr;
    }
};

Vector<FilesystemAdapterIndex*> indexedFilesystems;
FilesystemAdapterIndex *getFilesystemAdapterIndexByPath(const char *path) {
    if (!path) {
        path = "";
    }
    for (size_t i = 0; i < indexedFilesystems.size(); i++) {
        auto index = &indexedFilesystems[i];
        const char *prefix = index->filesystem->path_prefix ? index->filesystem->path_prefix : "";
        size_t prefix_len = strlen(prefix);
        if (!*path || !*prefix) {
            //filesystem adapters are not distinguishable by a path
            return index;
        }
        if (!strncmp(path, index->filesystem->path_prefix, prefix_len)) {
            return index;
        }
    }
    return nullptr;
}

int stat(const char *path, size_t *size) {
    auto index = getFilesystemAdapterIndexByPath(path);
    if (!index) {
        MO_DBG_ERR("path not recognized");
        return -1;
    }
    auto entry = index->getEntryByPath(path);
    if (entry) {
        *size = entry->size;
    } else {
        return -1;
    }
}

bool remove(const char *path) {
    auto index = getFilesystemAdapterIndexByPath(path);
    if (!index) {
        MO_DBG_ERR("path not recognized");
        return false;
    }
    auto entry = index.getEntryByPath(path);
    if (!entry) {
        return true;
    }
    bool removed = index.filesystem->remove(path);
    if (removed) {
        index.fileEntries.erase(index.fileEntries.begin() + (entry - &index.fileEntries[0]));
    }
    return removed;
}

int ftw(const char *path_prefix, int(*fn)(const char *fname, void *mo_user_data), void *mo_user_data) {
    auto index = getFilesystemAdapterIndexByPath(path_prefix);
    if (!index) {
        MO_DBG_ERR("path not recognized");
        return -1;
    }
    for (size_t it = 0; it < index.size();) {
        auto size_before = index.size();
        auto err = fn(index[it].fname);
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

MO_File* open(const char *path, const char *mode) {
    auto index = getFilesystemAdapterIndexByPath(path);
    if (!index) {
        MO_DBG_ERR("path not recognized");
        return nullptr;
    }

    IndexEntry *entry = nullptr;
    const char *fnCopy = nullptr;
    bool created = false;
    if (!(entry = getEntryByPath(path))) {

        size_t capacity = index->fileEntries.size() + 1;
        index->fileEntries.reserve(capacity);
        if (index->fileEntries.capacity() < size) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }

        const char *prefix = index.path_prefix ? index.path_prefix : "";
        size_t prefix_len = strlen(prefix);
        if (!strncmp(path, prefix, prefix_len)) {
            MO_DBG_ERR("internal error");
            return nullptr;
        }

        char *fn = path + prefix_len;
        size_t fn_size = strlen(path) - prefix_len + 1;

        char *fnCopy = static_cast<char*>(MO_MALLOC("FilesystemIndex", fn_size));
        if (!fnCopy) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }

        (void)snprintf(fnCopy, fn_size, "%s", fn);
        created = true;
    }

    auto indexedFile = new IndexedFileAdapter(*index);
    if (!indexedFile) {
        MO_DBG_ERR("OOM");
        MO_FREE(fnCopy);
        fnCopy = nullptr
        return nullptr
    }

    auto file = filesystem->open(path, mode);
    if (!file) {
        MO_FREE(fnCopy);
        fnCopy = nullptr;
        delete indexedFile;
        indexedFile = nullptr;
        return nullptr;
    }

    if (created) {
        index.emplace_back(fnCopy, 0); //transfer ownership of fnCopy
        entry = &index.back();
    }

    if (!strcmp(mode, "w")) {
        entry->size = 0; //write always empties the file
    }

    indexedFile->file = file; //transfer ownership
    indexedFile->fname = entry->fname; //no ownership

    return reinterpret_cast<MO_File*>(indexedFile);
}

bool close(MO_File *fileHandle) {
    auto indexedFile = reinterpret_cast<IndexedFileAdapter*>(fileHandle);
    auto index = indexedFile->index;
    auto file = indexedFile->file;
    auto filesystem = index->filesystem;
    bool success = filesystem->close(file);
    if (success) {
        auto entry = index->getEntryByFname(indexedFile->fname);
        if (entry) {
            entry->size = indexedFile->written;
            MO_DBG_DEBUG("update index: %s (%zuB)", entry->fname, entry->size);
        } else {
            MO_DBG_ERR("index consistency failure");
            //don't return false, the contents have been written and the file operation was successful
        }
    }
    delete indexedFile;
    indexedFile = nullptr;
    return success;
}

size_t read(MO_File *file, char *buf, size_t len) {
    auto indexedFile = reinterpret_cast<IndexedFileAdapter*>(fileHandle);
    auto file = indexedFile->file;
    auto filesystem = indexedFile->index->filesystem;
    return filesystem->read(file, buf, len);
}

int getc(MO_File *file) {
    auto indexedFile = reinterpret_cast<IndexedFileAdapter*>(fileHandle);
    auto file = indexedFile->file;
    auto filesystem = indexedFile->index->filesystem;
    return filesystem->getc(file);
}

size_t write(MO_File *file, const char *buf, size_t len) {
    auto indexedFile = reinterpret_cast<IndexedFileAdapter*>(fileHandle);
    auto file = indexedFile->file;
    auto filesystem = indexedFile->index->filesystem;

    auto ret = filesystem->write(file, buf, len);
    indexedFile->written += ret;
    return ret;
}

int seek(MO_File *file, size_t fpos) {
    auto indexedFile = reinterpret_cast<IndexedFileAdapter*>(fileHandle);
    auto file = indexedFile->file;
    auto filesystem = indexedFile->index->filesystem;

    auto ret = filesystem->seek(file, fpos);
    if (ret >= 0) {
        indexedFile->written = (size_t)ret;
    }
    return ret;
}

int loadIndexEntry(const char *fname, void *user_data) {
    auto index = reinterpret_cast<FilesystemAdapterIndex*>(user_data);
    int ret;
    char path [MO_MAX_PATH_SIZE];

    const char *prefix = index.path_prefix ? index.path_prefix : "";

    ret = snprintf(path, sizeof(path), "%s%s", prefix, fname);
    if (ret < 0 || ret >= sizeof(path),) {
        MO_DBG_ERR("snprintf: %i", ret);
        return -1;
    }

    auto filesystem = index->filesystem;

    size_t size;
    ret = filesystem->stat(path, &size);
    if (ret == 0) {
        //add fn and size to index
        MO_DBG_DEBUG("add file to index: %s (%zuB)", fname, size);

        size_t fnameSize = strlen(fname) + 1;
        char *fnameCopy = static_cast<char*>(MO_MALLOC("FilesystemIndex", fnameSize));
        if (!fnameCopy) {
            MO_DBG_ERR("OOM");
            return -1;
        }

        size_t capacity = index->fileEntries.size() + 1;
        index->fileEntries.reserve(capacity);
        if (index->fileEntries.capacity() < size) {
            MO_DBG_ERR("OOM");
            MO_FREE(fnameCopy);
            fnameCopy = nullptr
            return -1;
        }

        index.emplace_back(fnameCopy, size);
        return 0; //successfully added filename to index
    } else {
        MO_DBG_ERR("unexpected entry: %s", fname);
        return 0; //ignore this entry and continue ftw
    }
}

MO_FilesystemAdapter *decorateIndex(MO_FilesystemAdapter *filesystem) {
    if (getFilesystemAdapterIndexByPath(filesystem->path_prefix)) {
        MO_DBG_ERR("Multiple filesystem indexes only supported with unique MO folder paths");
        goto fail;
    }

    auto index = new FilesystemAdapterIndex();
    if (!index) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    auto decorator = static_cast<MO_FilesystemAdapter*>(MO_MALLOC("Filesystem", sizeof(MO_FilesystemAdapter)));
    if (!decorator) {
        MO_DBG_ERR("OOM");
        goto fail;
    }
    memset(decorator, 0, sizeof(MO_FilesystemAdapter));

    decorator->path_prefix = filesystem->path_prefix;
    decorator->stat = stat;
    decorator->remove = remove;
    decorator->ftw = ftw;
    decorator->open = open;
    decorator->close = close;
    decorator->read = read;
    decorator->getc = getc;
    decorator->write = write;
    decorator->seek = seek;

    size_t capacity = indexedFilesystems.size() + 1;
    indexedFilesystems.reserve(capacity);
    if (indexedFilesystems.capacity() < size) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    index->filesystem = filesystem; //transfer ownership

    //enumerate files in MO folder and load each file into the index structure
    auto ret = filesystem->ftw(filesystem->path_prefix, loadIndexEntry, reinterpret_cast<void*>(index));
    if (ret == 0) {
        MO_DBG_DEBUG("create fs index: found %zu entries", index->fileEntries.size());
    } else {
        MO_DBG_ERR("error loading fs index");
        goto fail;
    }

    indexedFilesystems.push_back(index);
    return decorator;
fail:
    index->filesystem = nullptr; //revoke ownership
    delete index;
    index = nullptr;
    MO_FREE(decorator);
    decorator = nullptr;
    return filesystem;
}

MO_FilesystemAdapter *resetIndex(MO_FilesystemAdapter *decorator) {
    auto index = getFilesystemAdapterIndexByPath(decorator->path_prefix);
    if (!index) {
        MO_DBG_ERR("Index not found");
        return decorator;
    }
    auto filesystem = index->filesystem;
    indexedFilesystems.erase(indexedFilesystems.begin() + (index - &indexedFilesystems[0]));
    delete index;
    index = nullptr;
    MO_FREE(decorator);
    decorator = nullptr;
    return filesystem;
}

} //namespace IndexedFS
} //namespace MicroOcpp

#endif //MO_ENABLE_FILE_INDEX

#if MO_USE_FILEAPI != MO_CUSTOM_FS

void mo_filesystemConfig_init(MO_FilesystemConfig *config) {
    memset(config, 0, sizeof(*config));
}

bool mo_filesystemConfig_copy(MO_FilesystemConfig *dst, MO_FilesystemConfig *src) {

    char *prefix_copy = nullptr;

    const char *prefix = src->path_prefix;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > 0) {
        size_t prefix_size = prefix_len + 1;
        prefix_copy = static_cast<char*>(MO_MALLOC("Filesystem", prefix_size));
        if (!prefix_copy) {
            MO_DBG_ERR("OOM");
            return false;
        }
        (void)snprintf(prefix_copy, prefix_size, "%s", prefix);
    }

    dst->internalBuf = prefix_copy;
    dst->path_prefix = dst->internalBuf;
    dst->opt = src->opt;
    return true;
}

void mo_filesystemConfig_deinit(MO_FilesystemConfig *config) {
    MO_FREE(config->internalBuf);
    memset(config, 0, sizeof(*config));
}

#endif //MO_USE_FILEAPI != MO_CUSTOM_FS

#if MO_USE_FILEAPI == MO_ARDUINO_LITTLEFS || MO_USE_FILEAPI == MO_ARDUINO_SPIFFS

#if MO_USE_FILEAPI == MO_ARDUINO_LITTLEFS
#include <LittleFS.h>
#include <vfs_api.h>
#define USE_FS LittleFS
#elif MO_USE_FILEAPI == MO_ARDUINO_SPIFFS
#include <FS.h>
#define USE_FS SPIFFS
#endif

namespace MicroOcpp {
namespace ArduinoFS {

int stat(const char *path, size_t *size) {
#if MO_USE_FILEAPI == MO_ARDUINO_LITTLEFS
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
#elif MO_USE_FILEAPI == MO_ARDUINO_SPIFFS
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
    }

    f.close();
    return status;
#else
#error
#endif
} //end stat

bool remove(const char *path) {
    return USE_FS.remove(path);
}

int ftw(const char *path_prefix, int(*fn)(const char *fname, void *mo_user_data), void *mo_user_data) {
    const char *prefix = path_prefix ? path_prefix : "";
#if MO_USE_FILEAPI == MO_ARDUINO_LITTLEFS
    auto dir = USE_FS.open(prefix);
    if (!dir) {
        MO_DBG_ERR("cannot open root directory: %s", prefix);
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

        err = fn(fname, mo_user_data);
        if (err) {
            break;
        }
    }
    return err;
#elif MO_USE_FILEAPI == MO_ARDUINO_SPIFFS
    size_t prefix_len = strlen(prefix);
    auto dir = USE_FS.openDir(prefix);
    int err = 0;
    while (dir.next()) {
        auto path = dir.fileName();
        if (path.c_str()) {
            const char *fname = path.c_str() + prefix_len;
            err = fn(fname, mo_user_data);
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

MO_File* open(const char *path, const char *mode) {
    auto *file = new File();
    if (!file) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    *file = USE_FS.open(path, mode);
    if (!*file) {
        MO_DBG_ERR("open file failure");
        goto fail;
    }
    if (file->isDirectory()) {
        MO_DBG_ERR("file failure");
        goto fail;
    }
    MO_DBG_DEBUG("File open successful: %s", path);
    return reinterpret_cast<MO_File*>(file);
fail:
    if (file) {
        file->close();
    }
    delete file;
    file = nullptr;
    return nullptr;
}

bool close(MO_File *fileHandle) {
    auto file = reinterpret_cast<File*>(fileHandle);
    file->close();
    delete file;
    return true;
}

size_t read(MO_File *fileHandle, char *buf, size_t len) {
    auto file = reinterpret_cast<File*>(fileHandle);
    return file->readBytes(buf, len);
}

int getc(MO_File *fileHandle) {
    auto file = reinterpret_cast<File*>(fileHandle);
    return file->read();
}

size_t write(MO_File *fileHandle, const char *buf, size_t len) {
    auto file = reinterpret_cast<File*>(fileHandle);
    return file->printf("%.*s", len, buf);
}

int seek(MO_File *fileHandle, size_t fpos) {
    auto file = reinterpret_cast<File*>(fileHandle);
    return file->seek(fpos);
}

int useCount;
MO_FilesystemOpt opt = MO_FS_OPT_DISABLE;

} //namespace ArduinoFS
} //namespace MicroOcpp

MO_FilesystemAdapter *mo_makeDefaultFilesystemAdapter(MO_FilesystemConfig config) {
    
    if (config.opt == MO_FS_OPT_DISABLE) {
        MO_DBG_DEBUG("Access to filesystem not allowed by opt");
        return nullptr;
    }

    char *prefix_copy = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;

    const char *prefix = config.path_prefix;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > 0) {
        size_t prefix_size = prefix_len + 1;
        prefix_copy = static_cast<char*>(MO_MALLOC("Filesystem", prefix_size));
        if (!prefix_copy) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
        (void)snprintf(prefix_copy, prefix_size, "%s", prefix);
    }

    filesystem = static_cast<MO_FilesystemAdapter*>(MO_MALLOC("Filesystem", sizeof(MO_FilesystemAdapter)));
    if (!filesystem) {
        MO_DBG_ERR("OOM");
        goto fail;
    }
    memset(filesystem, 0, sizeof(MO_FilesystemAdapter));

    filesystem->path_prefix = prefix_copy;
    filesystem->stat = MicroOcpp::ArduinoFS::stat;
    filesystem->remove = MicroOcpp::ArduinoFS::remove;
    filesystem->ftw = MicroOcpp::ArduinoFS::ftw;
    filesystem->open = MicroOcpp::ArduinoFS::open;
    filesystem->close = MicroOcpp::ArduinoFS::close;
    filesystem->read = MicroOcpp::ArduinoFS::read;
    filesystem->getc = MicroOcpp::ArduinoFS::getc;
    filesystem->write = MicroOcpp::ArduinoFS::write;
    filesystem->seek = MicroOcpp::ArduinoFS::seek;

    if (config.opt >= MO_FS_OPT_USE_MOUNT && MicroOcpp::ArduinoFS::opt < MO_FS_OPT_USE_MOUNT) { 
        #if MO_USE_FILEAPI == MO_ARDUINO_LITTLEFS
        {
            if(!USE_FS.begin(config.opt == MO_FS_OPT_USE_MOUNT_FORMAT_ON_FAIL)) {
                MO_DBG_ERR("Error while mounting LITTLEFS");
                goto fail;
            } else {
                MO_DBG_DEBUG("LittleFS mount success");
            }
        }
        #elif MO_USE_FILEAPI == MO_ARDUINO_SPIFFS
        {
            //ESP8266
            SPIFFSConfig cfg;
            cfg.setAutoFormat(config.opt == MO_FS_OPT_USE_MOUNT_FORMAT_ON_FAIL);
            SPIFFS.setConfig(cfg);
    
            if (!SPIFFS.begin()) {
                MO_DBG_ERR("Unable to initialize: unable to mount SPIFFS");
                goto fail;
            }
        }
        #else
        #error
        #endif

        //mounted successfully

    } //if MO_FS_OPT_USE_MOUNT

    if (filesystem) {
        MicroOcpp::ArduinoFS::useCount++;
        if (config.opt > MicroOcpp::ArduinoFS::opt) {
            MicroOcpp::ArduinoFS::opt = config.opt;
        }
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = MicroOcpp::IndexedFS::decorateIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    return filesystem;
fail:
    MO_FREE(prefix_copy);
    MO_FREE(filesystem);
    return nullptr;
}

void mo_freeDefaultFilesystemAdapter(MO_FilesystemAdapter *filesystem) {
    if (!filesystem) {
        return; //noop
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = MicroOcpp::IndexedFS::resetIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    MicroOcpp::ArduinoFS::useCount--;
    if (MicroOcpp::ArduinoFS::useCount <= 0 && MicroOcpp::ArduinoFS::opt >= MO_FS_OPT_USE_MOUNT) {
        USE_FS.end();
        MicroOcpp::ArduinoFS::opt = MO_FS_OPT_DISABLE;
    }

    //the default FS adapter owns the path_prefix buf and needs to free it
    char *path_prefix_buf = const_cast<char*>(filesystem->path_prefix);
    MO_FREE(path_prefix_buf);

    MO_FREE(filesystem);
}

#elif MO_USE_FILEAPI == MO_ESPIDF_SPIFFS

#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"

#ifndef MO_PARTITION_LABEL
#define MO_PARTITION_LABEL "mo"
#endif

namespace MicroOcpp {
namespace EspIdfFS {

int stat(const char *path, size_t *size) {
    struct ::stat st;
    auto ret = ::stat(path, &st);
    if (ret == 0) {
        *size = st.st_size;
    }
    return ret;
}

bool remove(const char *path) {
    return unlink(path) == 0;
}

int ftw(const char *path_prefix, int(*fn)(const char *fname, void *mo_user_data), void *mo_user_data) {
    //open MO root directory
    const char *prefix = path_prefix ? path_prefix : "";
    char dpath [MO_MAX_PATH_SIZE];
    auto path_len = snprintf(dpath, MO_MAX_PATH_SIZE, "%s", prefix);
    if (path_len < 0 || path_len >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("fn error: %i", path_len);
        return -1;
    }

    // trim trailing '/' if not root directory
    if (path_len >= 2 && dpath[path_len - 1] == '/') {
        dpath[path_len - 1] = '\0';
    }

    auto dir = opendir(dpath);
    if (!dir) {
        MO_DBG_ERR("cannot open root directory: %s", dpath);
        return -1;
    }

    int err = 0;
    while (auto entry = readdir(dir)) {
        err = fn(entry->d_name, mo_user_data);
        if (err) {
            break;
        }
    }

    closedir(dir);
    return err;
}

MO_File* open(const char *path, const char *mode) {
    return reinterpret_cast<MO_File*>(fopen(path, mode));
}

bool close(MO_File *file) {
    return fclose(reinterpret_cast<FILE*>(file)) == 0;
}

size_t read(MO_File *file, char *buf, size_t len) {
    return fread(buf, 1, len, reinterpret_cast<FILE*>(file));
}

int getc(MO_File *file) {
    return fgetc(reinterpret_cast<FILE*>(file));
}

size_t write(MO_File *file, const char *buf, size_t len) {
    return fwrite(buf, 1, len, reinterpret_cast<FILE*>(file));
}

int seek(MO_File *file, size_t fpos) {
    return fseek(file, fpos, SEEK_SET);
}

int useCount;
MO_FilesystemOpt opt = MO_FS_OPT_DISABLE;

} //namespace EspIdfFS
} //namespace MicroOcpp

using namespace MicroOcpp;

MO_FilesystemAdapter *mo_makeDefaultFilesystemAdapter(MO_FilesystemConfig config) {

    char *prefix_copy = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;

    if (config.opt == MO_FS_OPT_DISABLE) {
        MO_DBG_DEBUG("Access to filesystem not allowed by opt");
        goto fail;
    }
    
    const char *prefix = config.path_prefix;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > 0) {
        size_t prefix_size = prefix_len + 1;
        prefix_copy = static_cast<char*>(MO_MALLOC("Filesystem", prefix_size));
        if (!prefix_copy) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
        (void)snprintf(prefix_copy, prefix_size, "%s", prefix);
    }

    auto filesystem = static_cast<MO_FilesystemAdapter*>(MO_MALLOC("Filesystem", sizeof(MO_FilesystemAdapter)));
    if (!filesystem) {
        MO_DBG_ERR("OOM");
        goto fail;
    }
    memset(filesystem, 0, sizeof(MO_FilesystemAdapter));

    filesystem->path_prefix = prefix_copy;
    filesystem->stat = EspIdfFS::stat;
    filesystem->remove = EspIdfFS::remove;
    filesystem->ftw = EspIdfFS::ftw;
    filesystem->open = EspIdfFS::open;
    filesystem->close = EspIdfFS::close;
    filesystem->read = EspIdfFS::read;
    filesystem->getc = EspIdfFS::getc;
    filesystem->write = EspIdfFS::write;
    filesystem->seek = EspIdfFS::seek;

    if (config.opt >= MO_FS_OPT_USE_MOUNT && EspIdfFS::opt < MO_FS_OPT_USE_MOUNT) { 
        char prefix [MO_MAX_PATH_SIZE];
        auto prefix_len = snprintf(prefix, MO_MAX_PATH_SIZE, "%s", prefix ? prefix : "");
        if (prefix_len < 0 || prefix_len >= sizeof(prefix)) {
            MO_DBG_ERR("path_prefix error %i", prefix_len);
            goto fail;
        } else if (prefix_len <= 2) { //shortest possible prefix: "/p/", i.e. length = 3
            MO_DBG_ERR("path_prefix cannot be root on ESP-IDF (working example: \"/mo_store/\")");
            goto fail;
        }

        // trim trailing '/'
        if (prefix[prefix_len - 1] == '/') {
            prefix[prefix_len - 1] = '\0';
        }

        esp_vfs_spiffs_conf_t conf = {
            .base_path = prefix,
            .partition_label = MO_PARTITION_LABEL,
            .max_files = 5,
            .format_if_mount_failed = (config.opt == MO_FS_OPT_USE_MOUNT_FORMAT_ON_FAIL)
        };

        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret == ESP_OK) {
            MO_DBG_DEBUG("SPIFFS mounted");
        } else {
            if (ret == ESP_FAIL) {
                MO_DBG_ERR("Failed to mount or format filesystem");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                MO_DBG_ERR("Failed to find SPIFFS partition");
            } else {
                MO_DBG_ERR("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
            }
            goto fail;
        }

        //mounted successfully

    } //if MO_FS_OPT_USE_MOUNT

    if (filesystem) {
        EspIdfFS::useCount++;
        if (config.opt > EspIdfFS::opt) {
            EspIdfFS::opt = config.opt;
        }
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = decorateIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    return filesystem;
fail:
    MO_FREE(prefix_copy);
    MO_FREE(filesystem);
    return nullptr;
}

void mo_freeDefaultFilesystemAdapter(MO_FilesystemAdapter *filesystem) {
    if (!filesystem) {
        return; //noop
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = resetIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    EspIdfFS::useCount--;
    if (EspIdfFS::useCount <= 0 && EspIdfFS::opt >= MO_FS_OPT_USE_MOUNT) {
        esp_vfs_spiffs_unregister(MO_PARTITION_LABEL);
        MO_DBG_DEBUG("SPIFFS unmounted");
        EspIdfFS::opt = MO_FS_OPT_DISABLE;
    }
    MO_FREE(filesystem->path_prefix);
    MO_FREE(filesystem);
}

#elif MO_USE_FILEAPI == MO_POSIX_FILEAPI

#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

namespace MicroOcpp {
namespace PosixFS {

int stat(const char *path, size_t *size) {
    struct ::stat st;
    auto ret = ::stat(path, &st);
    if (ret == 0) {
        *size = st.st_size;
    }
    return ret;
}

bool remove(const char *path) {
    return ::remove(path) == 0;
}

int ftw(const char *path_prefix, int(*fn)(const char *fname, void *mo_user_data), void *mo_user_data) {
    auto dir = opendir(path_prefix);
    if (!dir) {
        MO_DBG_ERR("cannot open root directory: %s", path_prefix);
        return -1;
    }

    int err = 0;
    while (auto entry = readdir(dir)) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue; //files . and .. are specific to desktop systems and rarely appear on microcontroller filesystems. Filter them
        }
        err = fn(entry->d_name, mo_user_data);
        if (err) {
            break;
        }
    }

    closedir(dir);
    return err;
}

MO_File* open(const char *path, const char *mode) {
    return reinterpret_cast<MO_File*>(fopen(path, mode));
}

bool close(MO_File *file) {
    return fclose(reinterpret_cast<FILE*>(file)) == 0;
}

size_t read(MO_File *file, char *buf, size_t len) {
    return fread(buf, 1, len, reinterpret_cast<FILE*>(file));
}

int getc(MO_File *file) {
    return fgetc(reinterpret_cast<FILE*>(file));
}

size_t write(MO_File *file, const char *buf, size_t len) {
    return fwrite(buf, 1, len, reinterpret_cast<FILE*>(file));
}

int seek(MO_File *file, size_t fpos) {
    return fseek(reinterpret_cast<FILE*>(file), fpos, SEEK_SET);
}

} //namespace PosixFS
} //namespace MicroOcpp

using namespace MicroOcpp;

MO_FilesystemAdapter *mo_makeDefaultFilesystemAdapter(MO_FilesystemConfig config) {

    if (config.opt == MO_FS_OPT_DISABLE) {
        MO_DBG_DEBUG("Access to filesystem not allowed by opt");
        return nullptr;
    }

    char *prefix_copy = nullptr;
    MO_FilesystemAdapter *filesystem = nullptr;

    const char *prefix = config.path_prefix;
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    if (prefix_len > 0) {
        size_t prefix_size = prefix_len + 1;
        prefix_copy = static_cast<char*>(MO_MALLOC("Filesystem", prefix_size));
        if (!prefix_copy) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
        (void)snprintf(prefix_copy, prefix_size, "%s", prefix);
    }

    filesystem = static_cast<MO_FilesystemAdapter*>(MO_MALLOC("Filesystem", sizeof(MO_FilesystemAdapter)));
    if (!filesystem) {
        MO_DBG_ERR("OOM");
        goto fail;
    }
    memset(filesystem, 0, sizeof(MO_FilesystemAdapter));

    filesystem->path_prefix = prefix_copy;
    filesystem->stat = PosixFS::stat;
    filesystem->remove = PosixFS::remove;
    filesystem->ftw = PosixFS::ftw;
    filesystem->open = PosixFS::open;
    filesystem->close = PosixFS::close;
    filesystem->read = PosixFS::read;
    filesystem->getc = PosixFS::getc;
    filesystem->write = PosixFS::write;
    filesystem->seek = PosixFS::seek;

    // Create MO folder if not exists
    struct ::stat st;
    int ret;
    ret = ::stat(filesystem->path_prefix, &st);
    if (ret != 0) {
        mkdir(filesystem->path_prefix, 0755);

        auto ret = ::stat(filesystem->path_prefix, &st);
        if (ret == 0) {
            MO_DBG_INFO("created MO folder at %s", filesystem->path_prefix);
        } else {
            MO_DBG_ERR("MO folder not found at %s",filesystem->path_prefix);
            goto fail;
        }
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = decorateIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    return filesystem;
fail:
    MO_FREE(prefix_copy);
    MO_FREE(filesystem);
    return nullptr;
}

void mo_freeDefaultFilesystemAdapter(MO_FilesystemAdapter *filesystem) {

    if (!filesystem) {
        return; //noop
    }

#if MO_ENABLE_FILE_INDEX
    filesystem = resetIndex(filesystem);
#endif //MO_ENABLE_FILE_INDEX

    //the default FS adapter owns the path_prefix buf and needs to free it
    char *path_prefix_buf = const_cast<char*>(filesystem->path_prefix);
    MO_FREE(path_prefix_buf);

    MO_FREE(filesystem);
}

#else //filesystem disabled

#endif //switch-case MO_USE_FILEAPI
