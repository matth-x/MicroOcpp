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
        config->mo_data = this;
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

class ConfigurationContainerC : public ConfigurationContainer, public MemoryManaged {
private:
    ocpp_configuration_container *container;
public:
    ConfigurationContainerC(ocpp_configuration_container *container, const char *filename, bool accessible) :
            ConfigurationContainer(filename, accessible), MemoryManaged("v16.Configuration.ContainerC.", filename), container(container) {

    }

    bool load() override {
        return container->load(container->user_data);
    }

    bool save() override {
        return container->save(container->user_data);
    }

    std::shared_ptr<Configuration> createConfiguration(TConfig type, const char *key) override {
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
                return nullptr;
        }
        ocpp_configuration *config = container->create_configuration(container->user_data, dt, key);

        if (config) {
            return std::allocate_shared<ConfigurationC>(makeAllocator<ConfigurationC>(getMemoryTag()), config);
        } else {
            MO_DBG_ERR("could not create config: %s", key);
            return nullptr;
        }
    }

    void remove(Configuration *config) override {
        container->remove(container->user_data, config->getKey());
    }

    size_t size() override {
        return container->size(container->user_data);
    }

    Configuration *getConfiguration(size_t i) override {
        auto config = container->get_configuration(container->user_data, i);
        if (config) {
            return static_cast<Configuration*>(config->mo_data);
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<Configuration> getConfiguration(const char *key) override {
        ocpp_configuration *config = container->get_configuration_by_key(container->user_data, key);
        if (config) {
            return std::allocate_shared<ConfigurationC>(makeAllocator<ConfigurationC>(getMemoryTag()), config);
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
