// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef CONFIGURATIONOPTIONS_H
#define CONFIGURATIONOPTIONS_H

namespace ArduinoOcpp {

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

    bool accessAllowed() {return use;}
    bool mustMount() {return mount;}
    bool formatOnFail() {return formatFsOnFail;}
};

} //end namespace ArduinoOcpp

#endif
