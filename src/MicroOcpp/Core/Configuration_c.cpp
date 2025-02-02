// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Configuration_c.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

class ConfigurationC : public Configuration, public MemoryManaged {
private:
    ocpp_configuration *config;
public:
    ConfigurationC(ocpp_configuration *config) :
            config(config) {
        if (config->read_only) {
            setReadOnly();
        }
        if (config->write_only) {
            setWriteOnly();
        }
        if (config->reboot_required) {
            setRebootRequired();
        }
    }

    bool setKey(const char *key) override {
        updateMemoryTag("v16.Configuration.", key);
        return config->set_key(config->user_data, key);
    }

    const char *getKey() override {
        return config->get_key(config->user_data);
    }

    void setInt(int val) override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_INT) {
            MO_DBG_ERR("type err");
            return;
        }
        #endif
        config->set_int(config->user_data, val);
    }

    void setBool(bool val) override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_BOOL) {
            MO_DBG_ERR("type err");
            return;
        }
        #endif
        config->set_bool(config->user_data, val);
    }

    bool setString(const char *val) override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_STRING) {
            MO_DBG_ERR("type err");
            return false;
        }
        #endif
        return config->set_string(config->user_data, val);
    }

    int getInt() override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_INT) {
            MO_DBG_ERR("type err");
            return 0;
        }
        #endif
        return config->get_int(config->user_data);
    }

    bool getBool() override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_BOOL) {
            MO_DBG_ERR("type err");
            return false;
        }
        #endif
        return config->get_bool(config->user_data);
    }

    const char *getString() override {
        #if MO_CONFIG_TYPECHECK
        if (config->get_type(config->user_data) != ENUM_CDT_STRING) {
            MO_DBG_ERR("type err");
            return "";
        }
        #endif
        return config->get_string(config->user_data);
    }

    TConfig getType() override {
        TConfig res = TConfig::Int;
        switch (config->get_type(config->user_data)) {
            case ENUM_CDT_INT:
                res = TConfig::Int;
                break;
            case ENUM_CDT_BOOL:
                res = TConfig::Bool;
                break;
            case ENUM_CDT_STRING:
                res = TConfig::String;
                break;
            default:
                MO_DBG_ERR("type conversion");
                break;
        }

        return res;
    }

    uint16_t getValueRevision() override {
        return config->get_write_count(config->user_data);
    }

    ocpp_configuration *getConfiguration() {
        return config;
    }
};

namespace MicroOcpp {

ConfigurationC *getConfigurationC(ocpp_configuration *config) {
    if (!config->mo_data) {
        return nullptr;
    }
    return reinterpret_cast<std::shared_ptr<ConfigurationC>*>(config->mo_data)->get();
}

}

using namespace MicroOcpp;


void ocpp_setRebootRequired(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        c->setRebootRequired();
    }
    config->reboot_required = true;
}
bool ocpp_isRebootRequired(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        return c->isRebootRequired();
    }
    return config->reboot_required;
}

void ocpp_setReadOnly(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        c->setReadOnly();
    }
    config->read_only = true;
}
bool ocpp_isReadOnly(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        return c->isReadOnly();
    }
    return config->read_only;
}
bool ocpp_isReadable(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        return c->isReadable();
    }
    return !config->write_only;
}

void ocpp_setWriteOnly(ocpp_configuration *config) {
    if (auto c = getConfigurationC(config)) {
        c->setWriteOnly();
    }
    config->write_only = true;
}

class ConfigurationContainerC : public ConfigurationContainer, public MemoryManaged {
private:
    ocpp_configuration_container *container;
public:
    ConfigurationContainerC(ocpp_configuration_container *container, const char *filename, bool accessible) :
            ConfigurationContainer(filename, accessible), MemoryManaged("v16.Configuration.ContainerC.", filename), container(container) {

    }

    ~ConfigurationContainerC() {
        for (size_t i = 0; i < container->size(container->user_data); i++) {
            if (auto config = container->get_configuration(container->user_data, i)) {
                if (config->mo_data) {
                    delete reinterpret_cast<std::shared_ptr<ConfigurationC>*>(config->mo_data);
                    config->mo_data = nullptr;
                }
            }
        }
    }

    bool load() override {
        if (container->load) {
            return container->load(container->user_data);
        } else {
            return true;
        }
    }

    bool save() override {
        if (container->save) {
            return container->save(container->user_data);
        } else {
            return true;
        }
    }

    std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) override {

        auto result = std::shared_ptr<ConfigurationC>(nullptr, std::default_delete<ConfigurationC>(), makeAllocator<ConfigurationC>(getMemoryTag()));

        if (!container->create_configuration) {
            return result;
        }

        ocpp_config_datatype dt;
        switch (type) {
            case TConfig::Int:
                dt = ENUM_CDT_INT;
                break;
            case TConfig::Bool:
                dt = ENUM_CDT_BOOL;
                break;
            case TConfig::String:
                dt = ENUM_CDT_STRING;
                break;
            default:
                MO_DBG_ERR("internal error");
                return result;
        }
        ocpp_configuration *config = container->create_configuration(container->user_data, dt, key);
        if (!config) {
            return result;
        }
        
        result.reset(new ConfigurationC(config));

        if (result) {
            auto captureConfigC = new std::shared_ptr<ConfigurationC>(result);
            config->mo_data = reinterpret_cast<void*>(captureConfigC);
        } else {
            MO_DBG_ERR("could not create config: %s", key);
            if (container->remove) {
                container->remove(container->user_data, key);
            }
        }

        return result;
    }

    void remove(Configuration *config) override {
        if (!container->remove) {
            return;
        }

        if (auto c = container->get_configuration_by_key(container->user_data, config->getKey())) {
            delete reinterpret_cast<std::shared_ptr<ConfigurationC>*>(c->mo_data);
            c->mo_data = nullptr;
        }

        container->remove(container->user_data, config->getKey());
    }

    size_t size() override {
        return container->size(container->user_data);
    }

    Configuration *getConfiguration(size_t i) override {
        auto config = container->get_configuration(container->user_data, i);
        if (config) {
            if (!config->mo_data) {
                auto c = new ConfigurationC(config);
                if (c) {
                    config->mo_data = reinterpret_cast<void*>(new std::shared_ptr<ConfigurationC>(c, std::default_delete<ConfigurationC>(), makeAllocator<ConfigurationC>(getMemoryTag())));
                }
            }
            return static_cast<Configuration*>(config->mo_data ? reinterpret_cast<std::shared_ptr<ConfigurationC>*>(config->mo_data)->get() : nullptr);
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<Configuration> getConfiguration(const char *key) override {
        auto config = container->get_configuration_by_key(container->user_data, key);
        if (config) {
            if (!config->mo_data) {
                auto c = new ConfigurationC(config);
                if (c) {
                    config->mo_data = reinterpret_cast<void*>(new std::shared_ptr<ConfigurationC>(c, std::default_delete<ConfigurationC>(), makeAllocator<ConfigurationC>(getMemoryTag())));
                }
            }
            return config->mo_data ? *reinterpret_cast<std::shared_ptr<ConfigurationC>*>(config->mo_data) : nullptr;
        } else {
            return nullptr;
        }
    }

    void loadStaticKey(Configuration& config, const char *key) override {
        if (container->load_static_key) {
            container->load_static_key(container->user_data, key);
        }
    }
};

void ocpp_configuration_container_add(ocpp_configuration_container *container, const char *container_path, bool accessible) {
    addConfigurationContainer(std::allocate_shared<ConfigurationContainerC>(makeAllocator<ConfigurationContainerC>("v16.Configuration.ContainerC.", container_path), container, container_path, accessible));
}
