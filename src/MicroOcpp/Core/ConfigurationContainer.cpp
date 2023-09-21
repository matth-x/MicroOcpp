// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/ConfigurationContainer.h>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

ConfigurationContainer::~ConfigurationContainer() {

}

class ConfigurationContainerVolatile : public ConfigurationContainer {
private:
    std::vector<std::shared_ptr<Configuration>> configurations;
public:
    ConfigurationContainerVolatile(const char *filename, bool accessible) : ConfigurationContainer(filename, accessible) { }

    bool load() override {return true;}

    bool save() override {return true;}

    std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) override {
        std::shared_ptr<Configuration> res = makeConfiguration(type, key);
        if (!res) {
            //allocation failure - OOM
            MOCPP_DBG_ERR("OOM");
            return nullptr;
        }
        configurations.push_back(res);
        return res;
    }

    void removeConfiguration(Configuration *config) override {
        for (auto entry = configurations.begin(); entry != configurations.end();) {
            if (entry->get() == config) {
                entry = configurations.erase(entry);
            } else {
                entry++;
            }
        }
    }

    size_t getConfigurationCount() override {
        return configurations.size();
    }

    Configuration *getConfiguration(size_t i) override {
        return configurations[i].get();
    }

    std::shared_ptr<Configuration> getConfiguration(const char *key) override {
        for (auto& entry : configurations) {
            if (entry->getKey() && !strcmp(entry->getKey(), key)) {
                return entry;
            }
        }
        return nullptr;
    }
};

std::unique_ptr<ConfigurationContainer> makeConfigurationContainerVolatile(const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainer>(new ConfigurationContainerVolatile(filename, accessible));
}

} //end namespace MicroOcpp
