# Migrating to v1.1

As a new minor version, all features should work the same as in v1.0 and existing integrations are mostly backwards compatible. However, some fixes / cleanup steps in MicroOcpp require syntactic changes or special consideration when upgrading from v1.0 to v1.1. The known pitfalls are as follows:

- The default branch has been renamed from `master` into `main`
- Need to include extra headers: the transitive includes have been cleaned a bit. Probably it's necessary to add more includes next to `#include <MicroOcpp.h>`. E.g.<br/>`#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>`<br/>`#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>`
- `ocppPermitsCharge()` does not consider failures reported by the charger anymore. Before v1.1 it was possible to report failures to MicroOcpp using ErrorCodeInputs and then to rely on `ocppPermitsCharge()` becoming false when a failure occurs. For backwards compatibility, complement any occurence to `ocppPermitsCharge() && !isFaulted()`
- `setEnergyMeterInput` changed the expected return type of the callback function from `float` to `int` (see [#301](https://github.com/matth-x/MicroOcpp/pull/301))
- The return type of the UnlockConnector handler also changed from `PollResult<bool>` to enum `UnlockConnectorResult` (see [#271](https://github.com/matth-x/MicroOcpp/pull/271))

If upgrading MicroOcppMongoose at the same time, then the following changes are very important to consider:

- Certificates are no longer copied into heap memory, but the MO-Mongoose class takes the passed certificate pointer as a zero-copy parameter. The string behind the passed pointer must outlive the MO-Mongoose class (see [#10](https://github.com/matth-x/MicroOcppMongoose/pull/10))
- WebSocket authorization keys are no longer stored as c-strings, but as `unsigned char` buffers. For backwards compatibility, a null-byte is still appended and the buffer can be accessed as c-string, but this should be tested in existing deployments. Furtermore, MicroOcpp only accepts hex-encoded keys coming via ChangeConfiguration which is mandated by the standard. This also may break existing deployments (see [#4](https://github.com/matth-x/MicroOcppMongoose/pull/4)).

If accessing the MicroOcpp modules directly (i.e. not over `MicroOcpp.h` or `MicroOcpp_c.h`) then there are likely some more modifications to be done. See the history of pull requests where each change to the code is documented. However, if the existing integration compiles under the new MO version, then there shouldn't be too many unexpected incompatibilities.

# Migrating to v1.0

The API has been continously improved to best suit the common use cases for MicroOcpp. Moreover, the project has been given a new name to prevent confusion with the relation to the Arduino platform and to reflect the project goals properly. With the new project name, the API has been frozen for the v1.0 release.

## Adopting the new project name in existing projects

Find and replace the keywords in the following.

If using the C-facade (skip if you don't use anything from *ArduinoOcpp_c.h*):

- `AO_Connection` to `OCPP_Connection`
- `AO_Transaction` to `OCPP_Transaction`
- `AO_FilesystemOpt` to `OCPP_FilesystemOpt`
- `AO_TxNotification` to `OCPP_TxNotification`
- `ao_set_console_out_c` to `ocpp_set_console_out_c`

Change this in any case:

- `ArduinoOcpp` to `MicroOcpp`
- `"AO_` to `"Cst_` (define build flag `MO_CONFIG_EXT_PREFIX="AO_"` to keep old config keys)
- `AO_` to `MO_`
- `ocpp_` to `mocpp_`

Change this if used anywhere:

- `ao_set_console_out` to `mocpp_set_console_out`
- `ao_tick_ms` to `mocpp_tick_ms`

If using the C-facade, change this as the final step:

- `ao_` to `ocpp_`

## Further API changes to consider

In addition to the new project name, the API has also been reworked for more consistency. After renaming the existing project as described above, also take a look at the [changelogs](https://github.com/matth-x/MicroOcpp/blob/1.0.x/CHANGELOG.md) (see Section Changed for v1.0.0).

**If something is missing in this guide, please share the issue here:** [https://github.com/matth-x/MicroOcpp/issues/176](https://github.com/matth-x/MicroOcpp/issues/176)
