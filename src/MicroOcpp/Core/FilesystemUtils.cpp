// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/ConfigurationOptions.h> //FilesystemOpt
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

std::unique_ptr<DynamicJsonDocument> FilesystemUtils::loadJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn) {
    if (!filesystem || !fn || *fn == '\0') {
        MO_DBG_ERR("Format error");
        return nullptr;
    }

    if (strnlen(fn, MO_MAX_PATH_SIZE) >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("Fn too long: %.*s", MO_MAX_PATH_SIZE, fn);
        return nullptr;
    }
    
    size_t fsize = 0;
    if (filesystem->stat(fn, &fsize) != 0) {
        MO_DBG_DEBUG("File does not exist: %s", fn);
        return nullptr;
    }

    if (fsize < 2) {
        MO_DBG_ERR("File too small for JSON, collect %s", fn);
        filesystem->remove(fn);
        return nullptr;
    }

    auto file = filesystem->open(fn, "r");
    if (!file) {
        MO_DBG_ERR("Could not open file %s", fn);
        return nullptr;
    }

    size_t capacity_init = (3 * fsize) / 2;

    //capacity = ceil capacity_init to the next power of two; should be at least 128

    size_t capacity = 128;
    while (capacity < capacity_init && capacity < MO_MAX_JSON_CAPACITY) {
        capacity *= 2;
    }
    if (capacity > MO_MAX_JSON_CAPACITY) {
        capacity = MO_MAX_JSON_CAPACITY;
    }
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(nullptr);
    DeserializationError err = DeserializationError::NoMemory;
    ArduinoJsonFileAdapter fileReader {file.get()};

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc.reset(new DynamicJsonDocument(capacity));
        err = deserializeJson(*doc, fileReader);

        capacity *= 2;

        file->seek(0); //rewind file to beginning
    }

    if (err) {
        MO_DBG_ERR("Error deserializing file %s: %s", fn, err.c_str());
        //skip this file
        return nullptr;
    }

    MO_DBG_DEBUG("Loaded JSON file: %s", fn);

    return doc;
}

bool FilesystemUtils::storeJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn, const DynamicJsonDocument& doc) {
    if (!filesystem || !fn || *fn == '\0') {
        MO_DBG_ERR("Format error");
        return false;
    }

    if (strnlen(fn, MO_MAX_PATH_SIZE) >= MO_MAX_PATH_SIZE) {
        MO_DBG_ERR("Fn too long: %.*s", MO_MAX_PATH_SIZE, fn);
        return false;
    }

    if (doc.isNull() || doc.overflowed()) {
        MO_DBG_ERR("Invalid JSON %s", fn);
        return false;
    }

    auto file = filesystem->open(fn, "w");
    if (!file) {
        MO_DBG_ERR("Could not open file %s", fn);
        return false;
    }

    ArduinoJsonFileAdapter fileWriter {file.get()};

    size_t written = serializeJson(doc, fileWriter);

    if (written < 2) {
        MO_DBG_ERR("Error writing file %s", fn);
        size_t file_size = 0;
        if (filesystem->stat(fn, &file_size) == 0) {
            MO_DBG_DEBUG("Collect invalid file %s", fn);
            filesystem->remove(fn);
        }
        return false;
    }

    MO_DBG_DEBUG("Wrote JSON file: %s", fn);
    return true;
}

bool FilesystemUtils::remove_if(std::shared_ptr<FilesystemAdapter> filesystem, std::function<bool(const char*)> pred) {
    return filesystem->ftw_root([filesystem, pred] (const char *fpath) {
        if (pred(fpath) && fpath[0] != '.') {

            char fn [MO_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, MO_MAX_PATH_SIZE, MO_FILENAME_PREFIX "%s", fpath);
            if (ret < 0 || ret >= MO_MAX_PATH_SIZE) {
                MO_DBG_ERR("fn error: %i", ret);
                return -1;
            }

            filesystem->remove(fn);
            //no error handling - just skip failed file
        }
        return 0;
    }) == 0;
}
