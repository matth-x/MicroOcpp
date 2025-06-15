// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <stdarg.h>

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

bool FilesystemUtils::printPath(MO_FilesystemAdapter *filesystem, char *path, size_t size, const char *fname, ...) {

    size_t written = 0;
    int ret;
    bool success = true;

    // write path_prefix
    ret = snprintf(path, size, "%s", filesystem->path_prefix ? filesystem->path_prefix : "");
    if (ret < 0 || written + (size_t)ret >= size) {
        MO_DBG_ERR("path buffer insufficient");
        return false;
    }
    written += (size_t)ret;

    // write fname
    va_list args;
    va_start(args, fname);
    ret = vsnprintf(path + written, size - written, fname, args);
    va_end(args);
    if (ret < 0 || written + (size_t)ret >= size) {
        MO_DBG_ERR("path buffer insufficient, fname too long or fname error");
        return false;
    }
    written += (size_t)ret;

    return true;
}

FilesystemUtils::LoadStatus FilesystemUtils::loadJson(MO_FilesystemAdapter *filesystem, const char *fname, JsonDoc& doc, const char *memoryTag) {

    char path [MO_MAX_PATH_SIZE];
    if (!printPath(filesystem, path, sizeof(path), "%s", fname)) {
        MO_DBG_ERR("fname too long: %s", fname);
        return FilesystemUtils::LoadStatus::ErrOther;
    }

    size_t fsize = 0;
    if (filesystem->stat(path, &fsize) != 0) {
        return FilesystemUtils::LoadStatus::FileNotFound;
    }

    auto file = filesystem->open(path, "r");
    if (!file) {
        MO_DBG_ERR("Could not open file %s", fname);
        return FilesystemUtils::LoadStatus::ErrFileCorruption;
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

    DeserializationError err = DeserializationError::NoMemory;
    ArduinoJsonFileAdapter fileReader {filesystem, file};

    while (err == DeserializationError::NoMemory && capacity <= MO_MAX_JSON_CAPACITY) {

        doc = std::move(initJsonDoc(memoryTag, capacity));
        if (doc.capacity() < capacity) {
            MO_DBG_ERR("OOM");
            (void)filesystem->close(file);
            return FilesystemUtils::LoadStatus::ErrOOM;
        }

        err = deserializeJson(doc, fileReader);

        capacity *= 2;

        filesystem->seek(file, 0); //rewind file to beginning
    }

    (void)filesystem->close(file);

    if (err) {
        MO_DBG_ERR("Error deserializing file %s: %s", fname, err.c_str());
        //skip this file
        return FilesystemUtils::LoadStatus::ErrFileCorruption;
    }

    MO_DBG_DEBUG("Loaded JSON file: %s", fname);

    return FilesystemUtils::LoadStatus::Success;;
}

FilesystemUtils::StoreStatus FilesystemUtils::storeJson(MO_FilesystemAdapter *filesystem, const char *fname, const JsonDoc& doc) {

    char path [MO_MAX_PATH_SIZE];
    if (!printPath(filesystem, path, sizeof(path), "%s", fname)) {
        MO_DBG_ERR("fname too long: %s", fname);
        return FilesystemUtils::StoreStatus::ErrOther;
    }

    if (doc.isNull() || doc.overflowed()) {
        MO_DBG_ERR("Invalid JSON %s", fname);
        return FilesystemUtils::StoreStatus::ErrOther;
    }

    auto file = filesystem->open(path, "w");
    if (!file) {
        MO_DBG_ERR("Could not open file %s", fname);
        return FilesystemUtils::StoreStatus::ErrFileWrite;
    }

    ArduinoJsonFileAdapter fileWriter (filesystem, file);

    size_t written;
    if (MO_PLATFORM == MO_PLATFORM_UNIX) {
        //when debugging on Linux, add line breaks and white spaces for better readibility
        written = serializeJsonPretty(doc, fileWriter);
    } else {
        written = serializeJson(doc, fileWriter);
    }

    if (written < 2) {
        MO_DBG_ERR("Error writing file %s", fname);
        (void)filesystem->close(file);
        return FilesystemUtils::StoreStatus::ErrFileWrite;
    }

    bool ret = filesystem->close(file);
    if (!ret) {
        MO_DBG_ERR("Error writing file %s", fname);
        return FilesystemUtils::StoreStatus::ErrFileWrite;
    }

    MO_DBG_DEBUG("Wrote JSON file: %s", fname);

    return FilesystemUtils::StoreStatus::Success;
}

namespace MicroOcpp {
namespace FilesystemUtils {

struct RemoveByPrefixData {
    MO_FilesystemAdapter *filesystem;
    const char *fnamePrefix;
};

} //namespace FilesystemUtils
} //namespace MicroOcpp

bool FilesystemUtils::removeByPrefix(MO_FilesystemAdapter *filesystem, const char *fnamePrefix) {

    RemoveByPrefixData data;
    data.filesystem = filesystem;
    data.fnamePrefix = fnamePrefix;

    auto ret = filesystem->ftw(filesystem->path_prefix, [] (const char *fname, void* user_data) -> int {
        auto data = reinterpret_cast<RemoveByPrefixData*>(user_data);
        auto filesystem = data->filesystem;
        auto fnamePrefix = data->fnamePrefix;

        //check if fname starts with fnamePrefix
        for (size_t i = 0; fnamePrefix[i] != '\0'; i++) {
            if (fname[i] != fnamePrefix[i]) {
                //not a prefix of fname - don't do anything on this file
                return 0;
            }
        }

        char path [MO_MAX_PATH_SIZE];
        if (!printPath(filesystem, path, sizeof(path), "%s", fname)) {
            MO_DBG_ERR("fname too long: %s", fname);
            return -1;
        }

        (void)filesystem->remove(path); //no error handling - just skip failed file
        return 0;
    }, reinterpret_cast<void*>(&data));

    return ret == 0;
}

namespace MicroOcpp {
namespace FilesystemUtils {

struct RingIndexData {
    const char *fnamePrefix;
    unsigned int MAX_INDEX;
    unsigned int *indexBegin;
    unsigned int *indexEnd;

    size_t fnamePrefixLen;
    unsigned int indexPivot;
};

int updateRingIndex(const char *fname, void* user_data) {
    FilesystemUtils::RingIndexData& data = *reinterpret_cast<FilesystemUtils::RingIndexData*>(user_data);
    const char *fnamePrefix = data.fnamePrefix;
    unsigned int MAX_INDEX = data.MAX_INDEX;
    unsigned int& indexBegin = *data.indexBegin;
    unsigned int& indexEnd = *data.indexEnd;
    size_t fnamePrefixLen = data.fnamePrefixLen;
    unsigned int& indexPivot = data.indexPivot;

    if (!strncmp(fname, fnamePrefix, fnamePrefixLen)) {
        unsigned int parsedIndex = 0;
        for (size_t i = fnamePrefixLen; fname[i] >= '0' && fname[i] <= '9'; i++) {
            parsedIndex *= 10;
            parsedIndex += fname[i] - '0';
        }

        if (indexPivot == MAX_INDEX) {
            indexPivot = parsedIndex;
            indexBegin = parsedIndex;
            indexEnd = (parsedIndex + 1) % MAX_INDEX;
            MO_DBG_DEBUG("found %s%u.jsn - Internal range from %u to %u (exclusive)", fnamePrefix, parsedIndex, indexBegin, indexEnd);
            return 0;
        }

        if ((parsedIndex + MAX_INDEX - indexPivot) % MAX_INDEX < MAX_INDEX / 2) {
            //parsedIndex is after pivot point
            if ((parsedIndex + 1 + MAX_INDEX - indexPivot) % MAX_INDEX > (indexEnd + MAX_INDEX - indexPivot) % MAX_INDEX) {
                indexEnd = (parsedIndex + 1) % MAX_INDEX;
            }
        } else if ((indexPivot + MAX_INDEX - parsedIndex) % MAX_INDEX < MAX_INDEX / 2) {
            //parsedIndex is before pivot point
            if ((indexPivot + MAX_INDEX - parsedIndex) % MAX_INDEX > (indexPivot + MAX_INDEX - indexBegin) % MAX_INDEX) {
                indexBegin = parsedIndex;
            }
        }

        MO_DBG_DEBUG("found %s%u.jsn - Internal range from %u to %u (exclusive)", fnamePrefix, parsedIndex, indexBegin, indexEnd);
    }
    return 0;
}

} //namespace FilesystemUtils
} //namespace MicroOcpp

bool FilesystemUtils::loadRingIndex(MO_FilesystemAdapter *filesystem, const char *fnamePrefix, unsigned int MAX_INDEX, unsigned int *indexBegin, unsigned int *indexEnd) {

    FilesystemUtils::RingIndexData data;
    data.fnamePrefix = fnamePrefix;
    data.MAX_INDEX = MAX_INDEX;
    data.indexBegin = indexBegin;
    data.indexEnd = indexEnd;
    data.fnamePrefixLen = strlen(fnamePrefix);
    data.indexPivot = MAX_INDEX;

    auto ret = filesystem->ftw(filesystem->path_prefix, updateRingIndex, reinterpret_cast<void*>(&data));

    return ret == 0;
}
