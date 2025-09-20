// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FILESYSTEMUTILS_H
#define MO_FILESYSTEMUTILS_H

#ifdef __cplusplus
#include <ArduinoJson.h>
#endif //__cplusplus

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>

#ifdef __cplusplus

namespace MicroOcpp {

class ArduinoJsonFileAdapter {
private:
    MO_FilesystemAdapter *filesystem = nullptr;
    MO_File *file = nullptr;
public:
    ArduinoJsonFileAdapter(MO_FilesystemAdapter *filesystem, MO_File *file) : filesystem(filesystem), file(file) { }

    size_t readBytes(char *buf, size_t len) {
        return filesystem->read(file, buf, len);
    }

    int read() {
        return filesystem->getc(file);
    }

    size_t write(const uint8_t *buf, size_t len) {
        return filesystem->write(file, (const char*) buf, len);
    }

    size_t write(uint8_t c) {
        return filesystem->write(file, (const char*) &c, 1);
    }
};

namespace FilesystemUtils {

/* Determines path of a file given the filename and the path_prefix defined in the filesystem adapter.
 * `path`: array which should have at least MO_MAX_PATH_SIZE bytes (including the terminating zero)
 * `size`: number of bytes of `path` */
bool printPath(MO_FilesystemAdapter *filesystem, char *path, size_t size, const char *fname, ...);

enum class LoadStatus : uint8_t {
    Success, //file operation successful
    FileNotFound,
    ErrOOM,
    ErrFileCorruption,
    ErrOther, //file operation is not possible with the given parameters
};
LoadStatus loadJson(MO_FilesystemAdapter *filesystem, const char *fname, JsonDoc& doc, const char *memoryTag);

enum class StoreStatus : uint8_t {
    Success, //file operation successful
    ErrFileWrite,
    ErrJsonCorruption,
    ErrOther, //file operation is not possible with the given parameters
};
StoreStatus storeJson(MO_FilesystemAdapter *filesystem, const char *fname, const JsonDoc& doc);

/*
 * Removes files in the MO folder whose file names start with `fnamePrefix`.
 */
bool removeByPrefix(MO_FilesystemAdapter *filesystem, const char *fnamePrefix);

/*
 * Loads index for data queue on flash which is organized as a ring buffer. For example, Security
 * events are stored on flash like this:
 *     sloc-4.jsn
 *     sloc-5.jsn
 *     sloc-6.jsn
 * Then the `indexBegin` is 4 and `indexEnd` is 7 (one after last element). The size of the queue is
 * `indexEnd - indexBegin = 7 - 4 = 3`. If the queue is empty, then `indexEnd - indexBegin = 0`.
 * Parameters (and example values):
 * `filesystem`: filesystem adapter
 * `fnamePrefix`: common prefix of the filenames (e.g. "sloc-")
 * `MAX_INDEX`: range end of the index (e.g. if the index ranges from 0 - 999, then `MAX_INDEX = 1000`)
 * `*indexBegin`: output parameter for the first file in the queue (e.g. 4)
 * `*indexEnd`: output parameter for the end of the queue (e.g. 7)
 */
bool loadRingIndex(MO_FilesystemAdapter *filesystem, const char *fnamePrefix, unsigned int MAX_INDEX, unsigned int *indexBegin, unsigned int *indexEnd);

} //namespace FilesystemUtils
} //namespace MicroOcpp
#endif //__cplusplus
#endif
