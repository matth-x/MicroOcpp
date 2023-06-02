// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/ConfigurationContainer.h>

namespace ArduinoOcpp {

std::shared_ptr<AbstractConfiguration> ConfigurationContainer::getConfiguration(const char *key) {
    for (std::vector<std::shared_ptr<AbstractConfiguration>>::iterator configuration = configurations.begin(); configuration != configurations.end(); configuration++) {
        if ((*configuration)->keyEquals(key)) {
            return *configuration;
        }
    }
    return NULL;
}

bool ConfigurationContainer::removeConfiguration(std::shared_ptr<AbstractConfiguration> configuration) {
    
    auto config = configurations.begin();
    auto config_rev = configurations_revision.begin();
    while (config != configurations.end()) {
        if ((*config) == configuration) {
            configurations.erase(config);
            if (config_rev != configurations_revision.end())
                configurations_revision.erase(config_rev);
            return true;
        }
        config++;
        if (config_rev != configurations_revision.end())
            config_rev++;
    }

    return false;
}

void ConfigurationContainer::addConfiguration(std::shared_ptr<AbstractConfiguration> configuration) {
    configurations.push_back(configuration);
}

bool ConfigurationContainer::configurationsUpdated() {
    bool updated = false;

    auto config = configurations.begin();
    auto config_rev = configurations_revision.begin();
    while (config != configurations.end()) {
        if (config_rev == configurations_revision.end()) {
            //vectors are not the same length -> added configurations
            updated = true;
            break;
        }

        if ((*config)->getValueRevision() != *config_rev) {
            updated = true;
            break;
        }

        config++;
        config_rev++;
    }

    if (updated) {
        configurations_revision.erase(config_rev, configurations_revision.end());

        while (config != configurations.end()) {
            configurations_revision.push_back((*config)->getValueRevision());
            config++;
        }
    }

    return updated;
}

} //end namespace ArduinoOcpp
