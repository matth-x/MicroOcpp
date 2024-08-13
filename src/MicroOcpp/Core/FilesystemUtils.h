// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FILESYSTEMUTILS_H
#define MO_FILESYSTEMUTILS_H

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <ArduinoJson.h>
#include <memory>

namespace MicroOcpp {

class ArduinoJsonFileAdapter {
private:
    FileAdapter *file;
public:
    ArduinoJsonFileAdapter(FileAdapter *file) : file(file) { }

    size_t readBytes(char *buf, size_t len) {
        return file->read(buf, len);
    }

    int read() {
        return file->read();
    }

    size_t write(const uint8_t *buf, size_t len) {
        return file->write((const char*) buf, len);
    }

    size_t write(uint8_t c) {
        return file->write((const char*) &c, 1);
    }
};

namespace FilesystemUtils {

std::unique_ptr<JsonDoc> loadJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn, const char *memoryTag = nullptr);
bool storeJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn, const JsonDoc& doc);

bool remove_if(std::shared_ptr<FilesystemAdapter> filesystem, std::function<bool(const char*)> pred);

}

}

#endif
