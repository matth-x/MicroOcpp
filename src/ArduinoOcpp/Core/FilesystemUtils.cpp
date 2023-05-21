// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h> //FilesystemOpt
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

std::unique_ptr<DynamicJsonDocument> FilesystemUtils::loadJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn) {
    if (!filesystem || !fn || *fn == '\0') {
        AO_DBG_ERR("Format error");
        return nullptr;
    }

    if (strnlen(fn, AO_MAX_PATH_SIZE) >= AO_MAX_PATH_SIZE) {
        AO_DBG_ERR("Fn too long: %.*s", AO_MAX_PATH_SIZE, fn);
        return nullptr;
    }
    
    size_t fsize = 0;
    if (filesystem->stat(fn, &fsize) != 0) {
        AO_DBG_DEBUG("File does not exist: %s", fn);
        return nullptr;
    }

    if (fsize < 2) {
        AO_DBG_ERR("File too small for JSON, collect %s", fn);
        filesystem->remove(fn);
        return nullptr;
    }

    auto file = filesystem->open(fn, "r");
    if (!file) {
        AO_DBG_ERR("Could not open file %s", fn);
        return nullptr;
    }

    size_t capacity_init = (3 * fsize) / 2;

    //capacity = ceil capacity_init to the next power of two; should be at least 128

    size_t capacity = 128;
    while (capacity < capacity_init && capacity < AO_MAX_JSON_CAPACITY) {
        capacity *= 2;
    }
    if (capacity > AO_MAX_JSON_CAPACITY) {
        capacity = AO_MAX_JSON_CAPACITY;
    }
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(nullptr);
    DeserializationError err = DeserializationError::NoMemory;
    ArduinoJsonFileAdapter fileReader {file.get()};

    while (err == DeserializationError::NoMemory && capacity <= AO_MAX_JSON_CAPACITY) {

        doc.reset(new DynamicJsonDocument(capacity));
        err = deserializeJson(*doc, fileReader);

        capacity *= 2;

        file->seek(0); //rewind file to beginning
    }

    if (err) {
        AO_DBG_ERR("Error deserializing file %s: %s", fn, err.c_str());
        //skip this file
        return nullptr;
    }

    AO_DBG_DEBUG("Loaded JSON file: %s", fn);

    return doc;
}

bool FilesystemUtils::storeJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn, const DynamicJsonDocument& doc) {
    if (!filesystem || !fn || *fn == '\0') {
        AO_DBG_ERR("Format error");
        return false;
    }

    if (strnlen(fn, AO_MAX_PATH_SIZE) >= AO_MAX_PATH_SIZE) {
        AO_DBG_ERR("Fn too long: %.*s", AO_MAX_PATH_SIZE, fn);
        return false;
    }

    if (doc.isNull() || doc.overflowed()) {
        AO_DBG_ERR("Invalid JSON %s", fn);
        return false;
    }

    auto file = filesystem->open(fn, "w");
    if (!file) {
        AO_DBG_ERR("Could not open file %s", fn);
        return false;
    }

    ArduinoJsonFileAdapter fileWriter {file.get()};

    size_t written = serializeJson(doc, fileWriter);

    if (written < 2) {
        AO_DBG_ERR("Error writing file %s", fn);
        size_t file_size = 0;
        if (filesystem->stat(fn, &file_size) == 0) {
            AO_DBG_DEBUG("Collect invalid file %s", fn);
            filesystem->remove(fn);
        }
        return false;
    }

    AO_DBG_DEBUG("Wrote JSON file: %s", fn);
    return true;
}

bool FilesystemUtils::remove_all(std::shared_ptr<FilesystemAdapter> filesystem, const char *fpath_prefix) {
    return filesystem->ftw_root([filesystem, fpath_prefix] (const char *fpath) {
        if (!strncmp(fpath, fpath_prefix, strlen(fpath_prefix)) &&
                fpath[0] != '.') {

            char fn [AO_MAX_PATH_SIZE] = {'\0'};
            auto ret = snprintf(fn, AO_MAX_PATH_SIZE, AO_FILENAME_PREFIX "%s", fpath);
            if (ret < 0 || ret >= AO_MAX_PATH_SIZE) {
                AO_DBG_ERR("fn error: %i", ret);
                return -1;
            }

            filesystem->remove(fn);
            //no error handling - just skip failed file
        }
        return 0;
    }) == 0;
}
