# Migrating to v1.0

The API has been continously improved to best suit the common use cases for MicroOcpp. Moreover, the project has been given a new name to prevent confusion with the relation to the Arduino platform and to reflect the project goals properly. With the new project name, the API has been frozen for the v1.0 release. To migrate existing projects, find and replace the keywords in the following.

If using the C-facade (skip if you don't use anything from *ArduinoOcpp_c.h*):

- `AO_Connection` to `OCPP_Connection`
- `AO_Transaction` to `OCPP_Transaction`
- `AO_FilesystemOpt` to `OCPP_FilesystemOpt`
- `AO_TxNotification` to `OCPP_TxNotification`
- `ao_set_console_out_c` to `ocpp_set_console_out_c`

Change this in any case:

- `ArduinoOcpp` to `MicroOcpp`
- `"AO_` to `"Cst_` (define build flag `MOCPP_CONFIG_EXT_PREFIX="AO_"` to keep old config keys)
- `AO_` to `MOCPP_`
- `ocpp_` to `mocpp_`

Change this if used anywhere:

- `ao_set_console_out` to `mocpp_set_console_out`
- `ao_tick_ms` to `mocpp_tick_ms`

If using the C-facade, change this as the final step:

- `ao_` to `ocpp_`

**If something is missing in this guide, please share the issue here:** [https://github.com/matth-x/MicroOcpp/issues/176](https://github.com/matth-x/MicroOcpp/issues/176)
