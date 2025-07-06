// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <string.h>

#include <MicroOcpp/Model/Configuration/ConfigurationContainer.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

ConfigurationContainer::~ConfigurationContainer() {

}

bool ConfigurationContainer::commit() {
    return true;
}

ConfigurationContainerNonOwning::ConfigurationContainerNonOwning() :
            ConfigurationContainer(), MemoryManaged("v16.Configuration.ConfigurationContainerNonOwning"), configurations(makeVector<Configuration*>(getMemoryTag())) {

}

size_t ConfigurationContainerNonOwning::size() {
    return configurations.size();
}

Configuration *ConfigurationContainerNonOwning::getConfiguration(size_t i) {
    return configurations[i];
}

Configuration *ConfigurationContainerNonOwning::getConfiguration(const char *key) {
    for (size_t i = 0; i < configurations.size(); i++) {
        auto& configuration = configurations[i];
        if (!strcmp(configuration->getKey(), key)) {
            return configuration;
        }
    }
    return nullptr;
}

bool ConfigurationContainerNonOwning::add(Configuration *configuration) {
    configurations.push_back(configuration);
    return true;
}

bool ConfigurationContainerOwning::checkWriteCountUpdated() {

    decltype(trackWriteCount) writeCount = 0;

    for (size_t i = 0; i < configurations.size(); i++) {
        writeCount += configurations[i]->getWriteCount();
    }

    bool updated = writeCount != trackWriteCount;

    trackWriteCount = writeCount;

    return updated;
}

ConfigurationContainerOwning::ConfigurationContainerOwning() :
            ConfigurationContainer(), MemoryManaged("v16.Configuration.ConfigurationContainerOwning"), configurations(makeVector<Configuration*>(getMemoryTag())) {

}

ConfigurationContainerOwning::~ConfigurationContainerOwning() {
    MO_FREE(filename);
    filename = nullptr;

    for (size_t i = 0; i < configurations.size(); i++) {
        delete configurations[i];
    }
}

size_t ConfigurationContainerOwning::size() {
    return configurations.size();
}

Configuration *ConfigurationContainerOwning::getConfiguration(size_t i) {
    return configurations[i];
}

Configuration *ConfigurationContainerOwning::getConfiguration(const char *key) {
    for (size_t i = 0; i < configurations.size(); i++) {
        auto& configuration = configurations[i];
        if (!strcmp(configuration->getKey(), key)) {
            return configuration;
        }
    }
    return nullptr;
}

bool ConfigurationContainerOwning::add(std::unique_ptr<Configuration> configuration) {
    configurations.push_back(configuration.release());
    return true;
}

void ConfigurationContainerOwning::setFilesystem(MO_FilesystemAdapter *filesystem) {
    this->filesystem = filesystem;
}

bool ConfigurationContainerOwning::setFilename(const char *filename) {
    MO_FREE(this->filename);
    this->filename = nullptr;

    if (filename && *filename) {
        size_t fnsize = strlen(filename) + 1;

        this->filename = static_cast<char*>(MO_MALLOC(getMemoryTag(), fnsize));
        if (!this->filename) {
            MO_DBG_ERR("OOM");
            return false;
        }

        snprintf(this->filename, fnsize, "%s", filename);
    }

    return true;
}

const char *ConfigurationContainerOwning::getFilename() {
    return filename;
}

bool ConfigurationContainerOwning::load() {
    if (loaded) {
        return true;
    }

    if (!filesystem || !filename || !strncmp(filename, MO_CONFIGURATION_VOLATILE, sizeof(MO_CONFIGURATION_VOLATILE) - 1)) {
        return true; //persistency disabled - nothing to do
    }

    JsonDoc doc (0);

    auto ret = FilesystemUtils::loadJson(filesystem, filename, doc, getMemoryTag());
    switch (ret) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_DEBUG("Populate FS: create configurations file");
            return commit();
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            return false;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", filename);
            return false;
    }

    JsonArray configurationsJson = doc["configurations"];

    for (JsonObject stored : configurationsJson) {

        const char *key = stored["key"] | "";
        if (!*key) {
            MO_DBG_ERR("corrupt config");
            continue;
        }

        if (!stored.containsKey("value")) {
            MO_DBG_ERR("corrupt config");
            continue;
        }

        auto configurationPtr = getConfiguration(key);
        if (!configurationPtr) {
            MO_DBG_ERR("loaded configuration does not exist: %s, %s", filename, key);
            continue;
        }

        auto& configuration = *configurationPtr;

        switch (configuration.getType()) {
            case Configuration::Type::Int:
                configuration.setInt(stored["value"] | 0);
                break;
            case Configuration::Type::Bool:
                configuration.setBool(stored["value"] | false);
                break;
            case Configuration::Type::String:
                if (!configuration.setString(stored["value"] | "")) {
                    MO_DBG_ERR("value error: %s, %s", filename, key);
                    continue;
                }
                break;
        }
    }

    checkWriteCountUpdated(); // update trackWriteCount after load is completed

    MO_DBG_DEBUG("Initialization finished");
    loaded = true;
    return true;
}

bool ConfigurationContainerOwning::commit() {
    if (!filesystem || !filename || !strncmp(filename, MO_CONFIGURATION_VOLATILE, sizeof(MO_CONFIGURATION_VOLATILE) - 1)) {
        //persistency disabled - nothing to do
        return true;
    }

    if (!checkWriteCountUpdated()) {
        return true; //nothing to be done
    }

    size_t jsonCapacity = JSON_ARRAY_SIZE(configurations.size()); //configurations array
    jsonCapacity += configurations.size() * JSON_OBJECT_SIZE(3); //config entries in array

    if (jsonCapacity > MO_MAX_JSON_CAPACITY) {
        MO_DBG_ERR("configs JSON exceeds maximum capacity (%s, %zu entries). Crop configs file (by FCFS)", getFilename(), configurations.size());
        jsonCapacity = MO_MAX_JSON_CAPACITY;
    }

    auto doc = initJsonDoc(getMemoryTag(), jsonCapacity);

    JsonArray configurationsJson = doc.createNestedArray("configurations");

    for (size_t i = 0; i < configurations.size(); i++) {
        auto& configuration = *configurations[i];

        auto stored = configurationsJson.createNestedObject();

        stored["key"] = configuration.getKey();

        switch (configuration.getType()) {
            case Configuration::Type::Int:
                stored["value"] = configuration.getInt();
                break;
            case Configuration::Type::Bool:
                stored["value"] = configuration.getBool();
                break;
            case Configuration::Type::String:
                stored["value"] = configuration.getString();
                break;
        }
    }

    auto ret = FilesystemUtils::storeJson(filesystem, filename, doc);
    bool success = (ret == FilesystemUtils::StoreStatus::Success);

    if (success) {
        MO_DBG_DEBUG("Saving configurations finished");
    } else {
        MO_DBG_ERR("could not save configurations file: %s %i", filename, (int)ret);
    }

    return success;
}

#endif //MO_ENABLE_V16
