// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/ConfigurationContainer.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ConfigurationContainer::~ConfigurationContainer() {

}

ConfigurationContainerVolatile::ConfigurationContainerVolatile(const char *filename, bool accessible) : ConfigurationContainer(filename, accessible) {

}

bool ConfigurationContainerVolatile::load() {
    return true;
}

bool ConfigurationContainerVolatile::save() {
    return true;
}

std::shared_ptr<Configuration> ConfigurationContainerVolatile::createConfiguration(TConfig type, const char *key) {
    std::shared_ptr<Configuration> res = makeConfiguration(type, key);
    if (!res) {
        //allocation failure - OOM
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    configurations.push_back(res);
    return res;
}

void ConfigurationContainerVolatile::remove(Configuration *config) {
    for (auto entry = configurations.begin(); entry != configurations.end();) {
        if (entry->get() == config) {
            entry = configurations.erase(entry);
        } else {
            entry++;
        }
    }
}

size_t ConfigurationContainerVolatile::size() {
    return configurations.size();
}

Configuration *ConfigurationContainerVolatile::getConfiguration(size_t i) {
    return configurations[i].get();
}

std::shared_ptr<Configuration> ConfigurationContainerVolatile::getConfiguration(const char *key) {
    for (auto& entry : configurations) {
        if (entry->getKey() && !strcmp(entry->getKey(), key)) {
            return entry;
        }
    }
    return nullptr;
}

void ConfigurationContainerVolatile::add(std::shared_ptr<Configuration> c) {
    configurations.push_back(std::move(c));
}

namespace MicroOcpp {

std::unique_ptr<ConfigurationContainerVolatile> makeConfigurationContainerVolatile(const char *filename, bool accessible) {
    return std::unique_ptr<ConfigurationContainerVolatile>(new ConfigurationContainerVolatile(filename, accessible));
}

} //end namespace MicroOcpp
