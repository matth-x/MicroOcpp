// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Core/ConfigurationOptions.h> //FilesystemOpt
#include <ArduinoOcpp/Debug.h>

#define MAX_JSON_CAPACITY 4096

using namespace ArduinoOcpp;

std::unique_ptr<DynamicJsonDocument> FilesystemUtils::loadJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn) {
    if (!filesystem || !fn || *fn == '\0') {
        AO_DBG_ERR("Format error");
        return nullptr;
    }

    if (strnlen(fn, MAX_PATH_SIZE) >= MAX_PATH_SIZE) {
        AO_DBG_ERR("Fn too long: %.*s", MAX_PATH_SIZE, fn);
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

    size_t capacity = (3 * fsize) / 2;
    if (capacity < 32) {
        capacity = 32;
    }
    if (capacity > MAX_JSON_CAPACITY) {
        capacity = MAX_JSON_CAPACITY;
    }
    
    auto doc = std::unique_ptr<DynamicJsonDocument>(nullptr);
    DeserializationError err = DeserializationError::NoMemory;
    ArduinoJsonFileAdapter fileReader {file.get()};

    while (err == DeserializationError::NoMemory && capacity <= MAX_JSON_CAPACITY) {

        doc.reset(new DynamicJsonDocument(capacity));
        err = deserializeJson(*doc, fileReader);

        capacity *= 3;
        capacity /= 2;

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

    if (strnlen(fn, MAX_PATH_SIZE) >= MAX_PATH_SIZE) {
        AO_DBG_ERR("Fn too long: %.*s", MAX_PATH_SIZE, fn);
        return false;
    }

    if (doc.isNull() || doc.overflowed()) {
        AO_DBG_ERR("Invalid JSON %s", fn);
        return false;
    }

    size_t file_size = 0;
    if (filesystem->stat(fn, &file_size) == 0) {
        filesystem->remove(fn);
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
        if (filesystem->stat(fn, &file_size) == 0) {
            AO_DBG_DEBUG("Collect invalid file %s", fn);
            filesystem->remove(fn);
        }
        return false;
    }

    AO_DBG_DEBUG("Wrote JSON file: %s", fn);
    return true;
}
