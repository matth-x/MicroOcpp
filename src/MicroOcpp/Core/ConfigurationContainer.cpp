// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/ConfigurationContainer.h>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

std::shared_ptr<Configuration> ConfigurationPool::getConfiguration_typesafe(const char *key, TConfig type) {
    for (auto entry = configurations.begin(); entry != configurations.end();entry++) {
        if ((*entry)->getKey() && !strcmp((*entry)->getKey(), key)) {
            //found entry
            if ((*entry)->getType() == type) {
                return *entry;
            } else {
                MOCPP_DBG_ERR("conflicting types for %s - discard old config", key);
                configurations.erase(entry);
                break;
            }
        }
    }
    return nullptr;
}

template<class T>
std::shared_ptr<Configuration> ConfigurationPool::declareConfiguration(const char *key, T factoryDef, bool readonly = false, bool rebootRequired = false) {
    
    std::shared_ptr<Configuration> res = getConfiguration_typesafe(key, convertType<T>());

    if (!res) {
        //config doesn't exist yet
        res = makeConfigInt(key, factoryDef);
        if (!res) {
            //allocation failure - OOM
            MOCPP_DBG_ERR("OOM");
            return nullptr;
        }
        configurations.push_back(res);
    }

    if (readonly) {
        res->setReadOnly();
    }

    if (rebootRequired) {
        res->setRebootRequired();
    }

    return res;
}

size_t ConfigurationPool::getConfigurationCount() const {
        return configurations.size();
    }

Configuration *ConfigurationPool::getConfiguration(size_t i) const {
    return configurations[i].get();
}

Configuration *ConfigurationPool::getConfiguration(const char *key) const {
    for (auto& entry : configurations) {
        if (!strcmp(entry->getKey(), key)) {
            return entry.get();
        }
    }
}

class ConfigurationContainerVolatile : public ConfigurationContainer {
private:
    ConfigurationPool container;
public:
    ConfigurationContainerVolatile(const char *filename, bool accessible) : ConfigurationContainer(filename, accessible) { }

    bool load() override {return true;}

    bool save() override {return true;}

    std::shared_ptr<Configuration> declareConfigurationInt(const char *key, int factoryDef, bool readonly = false, bool rebootRequired = false) override {
        return container.declareConfiguration<int>(key, factoryDef, readonly, rebootRequired);
    }

    std::shared_ptr<Configuration> declareConfigurationBool(const char *key, bool factoryDef, bool readonly = false, bool rebootRequired = false) override {
        return container.declareConfiguration<bool>(key, factoryDef, readonly, rebootRequired);
    }

    std::shared_ptr<Configuration> declareConfigurationString(const char *key, const char*factoryDef, bool readonly = false, bool rebootRequired = false) override {
        return container.declareConfiguration<const char*>(key, factoryDef, readonly, rebootRequired);
    }

    size_t getConfigurationCount() override {
        return container.getConfigurationCount();
    }

    Configuration *getConfiguration(size_t i) override {
        return container.getConfiguration(i);
    }

    Configuration *getConfiguration(const char *key) override {
        return container.getConfiguration(key);
    }
};

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerVolatile(const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerVolatile(filename, accessible));
}

} //end namespace MicroOcpp
