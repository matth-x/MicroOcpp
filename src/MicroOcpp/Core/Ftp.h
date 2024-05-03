// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FTP_H
#define MO_FTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ocpp_ftp_download {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    void (*loop)(void *user_data);
} ocpp_ftp_download;

typedef struct ocpp_ftp_upload {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    void (*loop)(void *user_data);
} ocpp_ftp_upload;

typedef struct ocpp_ftp_client {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    void (*loop)(void *user_data);

    ocpp_ftp_download* (*get_file)(void *user_data,
        const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
        const char *ca_cert, // nullptr to disable cert check; will be ignored for non-TLS connections
        size_t (*file_writer)(void *mo_data, unsigned char *data, size_t len),
        void (*on_close)(void *mo_data),
        void *mo_data);

    void (*get_file_free)(void *user_data, ocpp_ftp_download*);

    ocpp_ftp_upload* (*post_file)(void *user_data,
        const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
        const char *ca_cert, // nullptr to disable cert check; will be ignored for non-TLS connections
        size_t (*file_reader)(void *mo_data, unsigned char *buf, size_t bufsize),
        void (*on_close)(void *mo_data),
        void *mo_data);

    void (*post_file_free)(void *user_data, ocpp_ftp_upload*);
} ocpp_ftp_client;

#ifdef __cplusplus
} //extern "C"

#include <memory>
#include <functional>

class FtpDownload {
public:
    virtual void loop() = 0;
};

class FtpUpload {
public:
    virtual void loop() = 0;
};

class FtpClient {
public:

    virtual std::unique_ptr<FtpDownload> getFile(
                const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
                const char *ca_cert, // nullptr to disable cert check; will be ignored for non-TLS connections
                std::function<size_t(unsigned char *data, size_t len)> fileWriter,
                std::function<void()> onClose) = 0;

    virtual std::unique_ptr<FtpUpload> postFile(
                const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
                const char *ca_cert, // nullptr to disable cert check; will be ignored for non-TLS connections
                std::function<size_t(unsigned char *out, size_t buffsize)> fileReader, //write at most buffsize bytes into out-buffer. Return number of bytes written
                std::function<void()> onClose) = 0;
};

#endif //def __cplusplus
#endif
