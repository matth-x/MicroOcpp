#ifndef MO_CONFIGURATION_C_H
#define MO_CONFIGURATION_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ocpp_config_datatype {
    ENUM_CDT_INT,
    ENUM_CDT_BOOL,
    ENUM_CDT_STRING
} ocpp_config_datatype;

typedef struct ocpp_configuration {
    void *user_data; // Set this at your choice. MO passes it back to the functions below

    bool (*set_key) (void *user_data, const char *key); // Optional. MO may provide a static key value which you can use to replace a possibly malloc'd key buffer
    const char* (*get_key) (void *user_data); // Return Configuration key

    ocpp_config_datatype (*get_type) (void *user_data); // Return internal data type of config (determines which of the following getX()/setX() pairs are valid)

    // Set value of Config
    union {
        void (*set_int) (void *user_data, int val);
        void (*set_bool) (void *user_data, bool val);
        bool (*set_string) (void *user_data, const char *val);
    };

    // Get value of Config
    union {
        int (*get_int) (void *user_data);
        bool (*get_bool) (void *user_data);
        const char* (*get_string) (void *user_data);
    };

    uint16_t (*get_write_count) (void *user_data); // Return number of changes of the value. MO uses this to detect if the firmware has updated the config

    void *mo_data; // Reserved for MO
} ocpp_configuration;

typedef struct ocpp_configuration_container {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    bool (*load) (void *user_data); // Called after declaring Configurations, to load them with their values
    bool (*save) (void *user_data); // Commit all Configurations to memory

    ocpp_configuration* (*create_configuration) (void *user_data, ocpp_config_datatype dt, const char *key); // Called to get a reference to a Configuration managed by this container (create new or return existing)
    void (*remove) (void *user_data, const char *key); // Remove this config from the container. Do not free the config here, the config must outlive the MO lifecycle

    size_t (*size) (void *user_data); // Number of Configurations currently managed by this container
    ocpp_configuration* (*get_configuration) (void *user_data, size_t i); // Return config at container position i
    ocpp_configuration* (*get_configuration_by_key) (void *user_data, const char *key); // Return config for given key

    void (*load_static_key) (void *user_data, const char *key); // Optional. MO may provide a static key value which you can use to replace a possibly malloc'd key buffer
} ocpp_configuration_container;

// Add custom Configuration container. Add one container per container_path before mocpp_initialize(...)
void ocpp_configuration_container_add(ocpp_configuration_container *container, const char *container_path, bool accessible);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
