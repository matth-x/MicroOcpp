// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CONFIGURATIONOPTIONS_H
#define CONFIGURATIONOPTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OCPP_FilesystemOpt {
    bool use;
    bool mount;
    bool formatFsOnFail;
};

#ifdef __cplusplus
}

namespace MicroOcpp {

class FilesystemOpt{
private:
    bool use = false;
    bool mount = false;
    bool formatFsOnFail = false;
public:
    enum Mode : uint8_t {Deactivate, Use, Use_Mount, Use_Mount_FormatOnFail};

    FilesystemOpt() = default;
    FilesystemOpt(Mode mode) {
        switch (mode) {
            case (FilesystemOpt::Use_Mount_FormatOnFail):
                formatFsOnFail = true;
                //fallthrough
            case (FilesystemOpt::Use_Mount):
                mount = true;
                //fallthrough
            case (FilesystemOpt::Use):
                use = true;
                break;
            default:
                break;
        }
    }
    FilesystemOpt(struct OCPP_FilesystemOpt fsopt) {
        this->use = fsopt.use;
        this->mount = fsopt.mount;
        this->formatFsOnFail = fsopt.formatFsOnFail;
    }

    bool accessAllowed() {return use;}
    bool mustMount() {return mount;}
    bool formatOnFail() {return formatFsOnFail;}
};

} //end namespace MicroOcpp

#endif //__cplusplus

#endif
