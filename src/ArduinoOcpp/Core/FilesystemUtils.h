// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AO_FILESYSTEMUTILS_H
#define AO_FILESYSTEMUTILS_H

#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoJson.h>
#include <memory>

namespace ArduinoOcpp {

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

std::unique_ptr<DynamicJsonDocument> loadJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn);
bool storeJson(std::shared_ptr<FilesystemAdapter> filesystem, const char *fn, const DynamicJsonDocument& doc);

}

}

#endif
