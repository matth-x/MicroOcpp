// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/FtpMbedTLS.h>

#if MO_ENABLE_MBEDTLS

#include <string>
#include <string.h>
#include <functional>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

class FtpTransferMbedTLS : public FtpUpload, public FtpDownload {
private:
    //MbedTLS common
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
    const char *ca_cert = nullptr;
    const char *client_cert = nullptr;
    const char *client_key = nullptr;
    bool isSecure = false; //tls policy

    //control connection specific
    mbedtls_net_context ctrl_fd;
    mbedtls_ssl_context ctrl_ssl;
    bool ctrl_opened = false;
    bool ctrl_ssl_established = false;

    //data connection specific
    mbedtls_net_context data_fd;
    mbedtls_ssl_context data_ssl;
    bool data_opened = false;
    bool data_ssl_established = false;
    bool data_conn_accepted = false; //Server sent okay to upload / download data

    //FTP URL
    std::string user;
    std::string pass;
    std::string ctrl_host;
    std::string ctrl_port;
    std::string dir;
    std::string fname;

    std::string data_host;
    std::string data_port;

    bool read_url_ctrl(const char *ftp_url);
    bool read_url_data(const char *data_url);
    
    std::function<size_t(unsigned char *data, size_t len)> fileWriter;
    std::function<size_t(unsigned char *out, size_t bufsize)> fileReader;
    std::function<void(MO_FtpCloseReason)> onClose;

    enum class Method {
        Retrieve,  //download file
        Store,     //upload file
        UNDEFINED
    };
    Method method = Method::UNDEFINED;

    int setup_tls();
    int connect(mbedtls_net_context& fd, mbedtls_ssl_context& ssl, const char *server_name, const char *server_port);
    int connect_ctrl();
    int connect_data();
    void close_ctrl();
    void close_data(MO_FtpCloseReason reason);

    int handshake_tls();

    void send_cmd(const char *cmd, const char *arg = nullptr, bool disable_tls_policy = false);

    void process_ctrl();
    void process_data();

    unsigned char *data_buf = nullptr;
    size_t data_buf_size = 4096;
    size_t data_buf_avail = 0;
    size_t data_buf_offs = 0;

public:
    FtpTransferMbedTLS(bool tls_only = false, const char *client_cert = nullptr, const char *client_key = nullptr);
    ~FtpTransferMbedTLS();

    void loop() override;
    
    bool isActive() override;

    bool getFile(const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
            std::function<size_t(unsigned char *data, size_t len)> fileWriter,
            std::function<void(MO_FtpCloseReason)> onClose,
            const char *ca_cert = nullptr); // nullptr to disable cert check; will be ignored for non-TLS connections

    bool postFile(const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
            std::function<size_t(unsigned char *out, size_t buffsize)> fileReader, //write at most buffsize bytes into out-buffer. Return number of bytes written
            std::function<void(MO_FtpCloseReason)> onClose,
            const char *ca_cert = nullptr); // nullptr to disable cert check; will be ignored for non-TLS connections
};

class FtpClientMbedTLS : public FtpClient {
private:
    const char *client_cert = nullptr;
    const char *client_key = nullptr;
    bool tls_only = false; //tls policy
public:

    FtpClientMbedTLS(bool tls_only = false, const char *client_cert = nullptr, const char *client_key = nullptr);

    std::unique_ptr<FtpDownload> getFile(const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
            std::function<size_t(unsigned char *data, size_t len)> fileWriter,
            std::function<void(MO_FtpCloseReason)> onClose,
            const char *ca_cert = nullptr) override; // nullptr to disable cert check; will be ignored for non-TLS connections

    std::unique_ptr<FtpUpload> postFile(const char *ftp_url, // ftp[s]://[user[:pass]@]host[:port][/directory]/filename
            std::function<size_t(unsigned char *out, size_t buffsize)> fileReader, //write at most buffsize bytes into out-buffer. Return number of bytes written
            std::function<void(MO_FtpCloseReason)> onClose,
            const char *ca_cert = nullptr) override; // nullptr to disable cert check; will be ignored for non-TLS connections
};

std::unique_ptr<FtpClient> makeFtpClientMbedTLS(bool tls_only, const char *client_cert, const char *client_key) {
    return std::unique_ptr<FtpClient>(new FtpClientMbedTLS(tls_only, client_cert, client_key));
}

void mo_mbedtls_log(void *user, int level, const char *file, int line, const char *str) {

    /*
     * MbedTLS debug level documented in mbedtls/debug.h:
     *     - 0 No debug
     *     - 1 Error
     *     - 2 State change
     *     - 3 Informational
     *     - 4 Verbose
     * 
     * To change the debug level, use the build flag MO_DBG_LEVEL_MBEDTLS accordingly
     */
    const char *lstr = "";
    if (level <= 1) {
        lstr = "ERROR";
    } else if (level <= 3) {
        lstr = "debug";
    } else {
        lstr = "verbose";
    }

    MO_CONSOLE_PRINTF("[MO] %s (%s:%i): %s\n", lstr, file, line, str);
}

/*
 * FTP implementation
 */

FtpTransferMbedTLS::FtpTransferMbedTLS(bool tls_only, const char *client_cert, const char *client_key) 
        : client_cert(client_cert), client_key(client_key), isSecure(tls_only) {
    mbedtls_net_init(&ctrl_fd);
    mbedtls_ssl_init(&ctrl_ssl);
    mbedtls_net_init(&data_fd);
    mbedtls_ssl_init(&data_ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_x509_crt_init(&clicert);
    mbedtls_pk_init(&pkey);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
}

FtpTransferMbedTLS::~FtpTransferMbedTLS() {
    if (onClose) {
        onClose(MO_FtpCloseReason_Failure); //data connection not closed properly
        onClose = nullptr;
    }
    delete[] data_buf;
    mbedtls_x509_crt_free(&clicert);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_net_free(&ctrl_fd);
    mbedtls_ssl_free(&ctrl_ssl);
    mbedtls_net_free(&data_fd);
    mbedtls_ssl_free(&data_ssl);
}

int FtpTransferMbedTLS::setup_tls() {

    if (auto ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char*) __FILE__,
                                     strlen(__FILE__)) != 0) {
        MO_DBG_ERR("mbedtls_ctr_drbg_seed: %i", ret);
        return ret;
    }

    if (ca_cert) {
        if (auto ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *) ca_cert,
                                    strlen(ca_cert)) < 0) {
            MO_DBG_ERR("mbedtls_x509_crt_parse(ca_cert): %i", ret);
            return ret;
        }
    }

    if (client_cert) {
        if (auto ret = mbedtls_x509_crt_parse(&clicert, (const unsigned char *) client_cert,
                                    strlen(client_cert))) {
            MO_DBG_ERR("mbedtls_x509_crt_parse(client_cert): %i", ret);
            return ret;
        }
    }

    if (client_key) {
        if (auto ret = mbedtls_pk_parse_key(&pkey,
                                    (const unsigned char *) client_key,
                                    strlen(client_key),
                                    NULL,
                                    0)) {
            MO_DBG_ERR("mbedtls_pk_parse_key: %i", ret);
            return ret;
        }
    }

    if (auto ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        MO_DBG_ERR("mbedtls_ssl_config_defaults: %i", ret);
        return ret;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL); //certificate check result manually handled for now

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_dbg(&conf, mo_mbedtls_log, NULL);

    if (ca_cert) {
        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    }

    if (client_cert || client_key) {
        if (auto ret = mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey) != 0) {
            MO_DBG_ERR("mbedtls_ssl_conf_own_cert: %i", ret);
            return ret;
        }
    }

    return 0; //success
}

int FtpTransferMbedTLS::connect(mbedtls_net_context& fd, mbedtls_ssl_context& ssl, const char *server_name, const char *server_port) {

    if (auto ret = mbedtls_net_connect(&fd, server_name, server_port, MBEDTLS_NET_PROTO_TCP) != 0) {
        MO_DBG_ERR("mbedtls_net_connect: %i", ret);
        return ret;
    }

    if (auto ret = mbedtls_net_set_nonblock(&fd)) {
        MO_DBG_ERR("mbedtls_net_set_nonblock: %i", ret);
        return ret;
    }

    if (auto ret = mbedtls_ssl_setup(&ssl, &conf) != 0) {
        MO_DBG_ERR("mbedtls_ssl_setup: %i", ret);
        return ret;
    }

    if (auto ret = mbedtls_ssl_set_hostname(&ssl, server_name) != 0) {
        MO_DBG_ERR("mbedtls_ssl_set_hostname: %i", ret);
        return ret;
    }

    mbedtls_ssl_set_bio(&ssl, &fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    return 0; //success
}

int FtpTransferMbedTLS::connect_ctrl() {
    if (auto ret = connect(ctrl_fd, ctrl_ssl, ctrl_host.c_str(), ctrl_port.c_str())) {
        MO_DBG_ERR("connect: %i", ret);
        return ret;
    }

    ctrl_opened = true;

    //handshake will be done later during STARTTLS procedure

    return 0; //success
}

int FtpTransferMbedTLS::connect_data() {
    if (auto ret = connect(data_fd, data_ssl, data_host.c_str(), data_port.c_str())) {
        MO_DBG_ERR("connect: %i", ret);
        return ret;
    }

    data_opened = true;

    if (isSecure) {
        //reuse SSL session of ctrl conn

        if (auto ret = mbedtls_ssl_set_session(&data_ssl, 
                    mbedtls_ssl_get_session_pointer(&ctrl_ssl))) {
            MO_DBG_ERR("session reuse failure: %i", ret);
            return ret;
        }

        data_ssl_established = true;
    }

    if (!data_buf) {
        data_buf = new unsigned char[data_buf_size];
        if (!data_buf) {
            MO_DBG_ERR("OOM");
            return -1;
        }
    }

    return 0; //success
}

void FtpTransferMbedTLS::close_ctrl() {
    if (!ctrl_opened) {
        return;
    }

    if (ctrl_ssl_established) {
        mbedtls_ssl_close_notify(&ctrl_ssl);
        ctrl_ssl_established = false;
    }
    mbedtls_net_free(&ctrl_fd);
    ctrl_opened = false;

    if (onClose && !data_opened) {
        onClose(MO_FtpCloseReason_Failure); //data connection has never been opened --> failure
        onClose = nullptr;
    }
}

void FtpTransferMbedTLS::close_data(MO_FtpCloseReason reason) {
    if (!data_opened) {
        return;
    }

    MO_DBG_DEBUG("closing data conn");

    if (data_ssl_established) {
        MO_DBG_DEBUG("TLS shutdown");
        mbedtls_ssl_close_notify(&data_ssl);
        data_ssl_established = false;
    }
    mbedtls_net_free(&data_fd);
    data_opened = false;
    data_conn_accepted = false;

    if (onClose) {
        onClose(reason);
        onClose = nullptr;
    }
}

int FtpTransferMbedTLS::handshake_tls() {

    int ret;
    while ((ret = mbedtls_ssl_handshake(&ctrl_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != 1) {
            char buf [1024];
            mbedtls_strerror(ret, (char *) buf, 1024);
            MO_DBG_ERR("mbedtls_ssl_handshake: %i, %s", ret, buf);
            return ret;
        }
    }

    if (ca_cert) {
        //certificate validation enabled

        if ((ret = mbedtls_ssl_get_verify_result(&ctrl_ssl)) != 0) {
            char vrfy_buf[512];
            mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "   > ", ret);
            MO_DBG_ERR("mbedtls_ssl_get_verify_result: %i, %s", ret, vrfy_buf);
            return ret;
        }
    }

    ctrl_ssl_established = true;

    return 0; //success
}

void FtpTransferMbedTLS::send_cmd(const char *cmd, const char *arg, bool disable_tls_policy) {

    const size_t MSG_SIZE = 128;
    unsigned char msg [MSG_SIZE];

    auto len = snprintf((char*) msg, MSG_SIZE, "%s%s%s\r\n", 
            cmd,               //cmd mandatory (e.g. "USER")
            arg ? " " : "",    //line spacing if arg is provided
            arg ? arg : "");   //arg optional (e.g. "anonymous")
    if (len < 0 || (size_t)len >= MSG_SIZE) {
        MO_DBG_ERR("could not write cmd, send QUIT instead");
        len = sprintf((char*) msg, "QUIT\r\n");
    } else {
        //show outgoing traffic for debug, but shadow PASS
        MO_DBG_DEBUG("SEND: %s %s", 
                cmd,
                !strncmp((char*) cmd, "PASS", strlen("PASS")) ? "***" : arg ? (char*) arg : "");
    }

    int ret = -1;

    if (ctrl_ssl_established) {
        ret = mbedtls_ssl_write(&ctrl_ssl, (unsigned char*) msg, len);
    } else if (!isSecure || disable_tls_policy) {
        ret = mbedtls_net_send(&ctrl_fd, (unsigned char*) msg, len);
    } else {
        MO_DBG_ERR("TLS policy failure");
        len = strlen("QUIT\r\n");
        ret = mbedtls_net_send(&ctrl_fd, (unsigned char*) "QUIT\r\n", len);
    }

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
            ret <= 0 ||
            ret < (int) len) {
        char buf [1024];
        mbedtls_strerror(ret, (char *) buf, 1024);
        MO_DBG_ERR("fatal - message on ctrl channel lost: %i, %s", ret, buf);
        close_ctrl();
        return;
    }
}

bool FtpTransferMbedTLS::getFile(const char *ftp_url_raw, std::function<size_t(unsigned char *data, size_t len)> fileWriter, std::function<void(MO_FtpCloseReason)> onClose, const char *ca_cert) {

    if (method != Method::UNDEFINED) {
        MO_DBG_ERR("FTP Client reuse not supported");
        return false;
    }
    
    if (!ftp_url_raw || !fileWriter) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    this->ca_cert = ca_cert;
    this->method = Method::Retrieve;
    this->fileWriter = fileWriter;
    this->onClose = onClose;

    if (!read_url_ctrl(ftp_url_raw)) {
        MO_DBG_ERR("could not parse URL");
        return false;
    }

    MO_DBG_DEBUG("init download from %s: %s", ctrl_host.c_str(), fname.c_str());

    if (auto ret = setup_tls()) {
        MO_DBG_ERR("could not setup MbedTLS: %i", ret);
        return false;
    }

    if (auto ret = connect_ctrl()) {
        MO_DBG_ERR("could not establish connection to FTP server: %i", ret);
        return false;
    }

    return true;
}

bool FtpTransferMbedTLS::postFile(const char *ftp_url_raw, std::function<size_t(unsigned char *out, size_t buffsize)> fileReader, std::function<void(MO_FtpCloseReason)> onClose, const char *ca_cert) {
    
    if (method != Method::UNDEFINED) {
        MO_DBG_ERR("FTP Client reuse not supported");
        return false;
    }

    if (!ftp_url_raw || !fileReader) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    MO_DBG_DEBUG("init upload %s", ftp_url_raw);

    this->ca_cert = ca_cert;
    this->method = Method::Store;
    this->fileReader = fileReader;
    this->onClose = onClose;

    if (!read_url_ctrl(ftp_url_raw)) {
        MO_DBG_ERR("could not parse URL");
        return false;
    }

    if (auto ret = setup_tls()) {
        MO_DBG_ERR("could not setup MbedTLS: %i", ret);
        return false;
    }

    if (auto ret = connect_ctrl()) {
        MO_DBG_ERR("could not establish connection to FTP server: %i", ret);
        return false;
    }

    return true;
}

void FtpTransferMbedTLS::process_ctrl() {
    // read input (if available)

    const size_t INBUF_SIZE = 128;
    unsigned char inbuf [INBUF_SIZE];
    memset(inbuf, 0, INBUF_SIZE);

    int ret = -1;

    if (ctrl_ssl_established) {
        ret = mbedtls_ssl_read(&ctrl_ssl, inbuf, INBUF_SIZE - 1);
    } else {
        ret = mbedtls_net_recv(&ctrl_fd, inbuf, INBUF_SIZE - 1);
    }

    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        //no new input data to be processed
        return;
    } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
        MO_DBG_ERR("FTP transfer aborted");
        close_ctrl();
        return;
    } else if (ret < 0) {
        MO_DBG_ERR("mbedtls_net_recv: %i", ret);
        send_cmd("QUIT");
        close_ctrl();
        return;
    }

    size_t inbuf_len = ret;

    // read multi-line command
    char *line_next = (char*) inbuf;
    while (line_next < (char*) inbuf + inbuf_len) {

        // take current line
        char *line = line_next;

        // null-terminate current line and find begin of next line
        while (line_next + 1 < (char*) inbuf + inbuf_len && *line_next != '\n') {
            line_next++;
        }
        *line_next = '\0';
        line_next++;

        MO_DBG_DEBUG("RECV: %s", line);

        if (isSecure && !ctrl_ssl_established) { //tls not established yet, set up according to RFC 4217
            if (!strncmp("220", line, 3)) {
                MO_DBG_DEBUG("start TLS negotiation");
                send_cmd("AUTH TLS", nullptr, true);
                return;
            } else if (!strncmp("234", line, 3)) { // Proceed with TLS negotiation
                MO_DBG_DEBUG("upgrade to TLS");

                if (auto ret = handshake_tls()) {
                    MO_DBG_ERR("handshake: %i", ret);
                    send_cmd("QUIT", nullptr, true);
                    return;
                }
            } else {
                MO_DBG_ERR("cannot proceed without TLS");
                send_cmd("QUIT", nullptr, true);
                return;
            }
        }

        if (isSecure && !ctrl_ssl_established) {
            //failure to establish security policy
            MO_DBG_ERR("internal error");
            send_cmd("QUIT", nullptr, true);
            return;
        }

        //security policy met
                
        if (!strncmp("530", line, 3)            // Not logged in
                || !strncmp("220", line, 3)     // Service ready for new user
                || !strncmp("234", line, 3)) {  // Just completed AUTH TLS handshake
            MO_DBG_DEBUG("select user %s", user.empty() ? "anonymous" : user.c_str());
            send_cmd("USER", user.empty() ? "anonymous" : user.c_str());
        } else if (!strncmp("331", line, 3)) { // User name okay, need password
            MO_DBG_DEBUG("enter pass %.2s***", pass.empty() ? "-" : pass.c_str());
            send_cmd("PASS", pass.c_str());
        } else if (!strncmp("230", line, 3)) { // User logged in, proceed
            MO_DBG_DEBUG("select directory %s", dir.empty() ? "/" : dir.c_str());
            send_cmd("CWD", dir.empty() ? "/" : dir.c_str());
        } else if (!strncmp("250", line, 3)) { // Requested file action okay, completed
            MO_DBG_DEBUG("enter passive mode");
            if (isSecure) {
                send_cmd("PBSZ 0\r\n"
                         "PROT P\r\n" //RFC 4217: set FTP session Private
                         "PASV");
            } else {
                send_cmd("PASV");
            }
        } else if (!strncmp("227", line, 3)) { // Entering Passive Mode (h1,h2,h3,h4,p1,p2)

            if (!read_url_data(line + 3)) { //trim leading response code
                MO_DBG_ERR("could not process data url. Expect format: (h1,h2,h3,h4,p1,p2)");
                send_cmd("QUIT");
                return;
            }

            if (auto ret = connect_data()) {
                MO_DBG_ERR("data connection failure: %i", ret);
                send_cmd("QUIT");
                return;
            }

            if (method == Method::Retrieve) {
                MO_DBG_DEBUG("request download for %s", fname.c_str());
                send_cmd("RETR", fname.c_str());
            } else if (method == Method::Store) {
                MO_DBG_DEBUG("request upload for %s", fname.c_str());
                send_cmd("STOR", fname.c_str());
            } else {
                MO_DBG_ERR("internal error");
                send_cmd("QUIT");
                return;
            }

        } else if (!strncmp("150", line, 3)    // File status okay; about to open data connection
                || !strncmp("125", line, 3)) { // Data connection already open
            MO_DBG_DEBUG("data connection accepted");
            data_conn_accepted = true;
        } else if (!strncmp("226", line, 3)) { // Closing data connection. Requested file action successful (for example, file transfer or file abort)
            MO_DBG_INFO("FTP success: %s", line);
            send_cmd("QUIT");
            return;
        } else if (!strncmp("55", line, 2)) { // Requested action not taken / aborted
            MO_DBG_WARN("FTP failure: %s", line);
            send_cmd("QUIT");
            return;
        } else if (!strncmp("200", line, 3)) { //PBSZ -> 0 and PROT -> P accepted
            MO_DBG_INFO("PBSZ/PROT success: %s", line);
        } else if (!strncmp("221", line, 3)) { // Server Goodbye
            MO_DBG_DEBUG("closing ctrl connection");
            close_ctrl();
            return;
        } else {
            MO_DBG_WARN("unkown commad (close connection): %s", line);
            send_cmd("QUIT");
            return;
        }
    }
}

void FtpTransferMbedTLS::process_data() {
    if (!data_conn_accepted) {
        return;
    }

    if (isSecure && !data_ssl_established) {
        //failure to establish security policy
        MO_DBG_ERR("internal error");
        close_data(MO_FtpCloseReason_Failure);
        send_cmd("QUIT", nullptr, true);
        return;
    }

    if (method == Method::Retrieve) {

        if (data_buf_avail == 0) {
            //load new data from socket

            data_buf_offs = 0;

            int ret = -1;
            if (data_ssl_established) {
                ret = mbedtls_ssl_read(&data_ssl, data_buf, data_buf_size - 1);
            } else {
                ret = mbedtls_net_recv(&data_fd, data_buf, data_buf_size - 1);
            }

            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                //no new input data to be processed
                return;
            } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
                //download finished
                close_data(MO_FtpCloseReason_Success);
                return;
            } else if (ret < 0) {
                MO_DBG_ERR("mbedtls_net_recv: %i", ret);
                close_data(MO_FtpCloseReason_Failure);
                send_cmd("QUIT");
                return;
            }

            data_buf_avail = ret;
        }

        auto ret = fileWriter(data_buf + data_buf_offs, data_buf_avail);

        if (ret == 0) {
            MO_DBG_ERR("fileWriter aborted download");
            close_data(MO_FtpCloseReason_Failure);
            send_cmd("QUIT");
            return;
        } else if (ret <= data_buf_avail) {
            data_buf_avail -= ret;
            data_buf_offs += ret;
        } else {
            MO_DBG_ERR("write error");
            close_data(MO_FtpCloseReason_Failure);
            send_cmd("QUIT");
            return;
        }

        //success
    } else if (method == Method::Store) {

        if (data_buf_avail == 0) {
            //load new data from file to write on socket

            data_buf_offs = 0;

            data_buf_avail = fileReader(data_buf, data_buf_size);
        }

        if (data_buf_avail > 0) {

            int ret = -1;
            if (data_ssl_established) {
                ret = mbedtls_ssl_write(&data_ssl, data_buf + data_buf_offs, data_buf_avail);
            } else {
                ret = mbedtls_net_send(&data_fd, data_buf + data_buf_offs, data_buf_avail);
            }

            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                //no data sent, wait
                return;
            } else if (ret <= 0) {
                MO_DBG_ERR("mbedtls_ssl_write: %i", ret);
                close_data(MO_FtpCloseReason_Failure);
                send_cmd("QUIT");
                return;
            }

            //successful write
            data_buf_avail -= ret;
            data_buf_offs += ret;
        } else {
            //no data in fileReader anymore
            MO_DBG_DEBUG("finished file reading");
            close_data(MO_FtpCloseReason_Success);
        }
    }
}

void FtpTransferMbedTLS::loop() {

    if (ctrl_opened) {
        process_ctrl();
    }

    if (data_opened) {
        process_data();
    }
}

bool FtpTransferMbedTLS::isActive() {
    return ctrl_opened || data_opened;
}

bool FtpTransferMbedTLS::read_url_ctrl(const char *ftp_url_raw) {
    std::string ftp_url = ftp_url_raw; //copy input ftp_url

    //tolower protocol specifier
    for (auto c = ftp_url.begin(); *c != ':' && c != ftp_url.end(); c++) {
        *c = tolower(*c);
    }

    //parse FTP URL: protocol specifier
    std::string proto;
    if (!strncmp(ftp_url.c_str(), "ftps://", strlen("ftps://"))) {
        //FTP over TLS (RFC 4217)
        proto = "ftps://";
        isSecure = true; //TLS policy
    } else if (!strncmp(ftp_url.c_str(), "ftp://", strlen("ftp://"))) {
        //FTP without security policies (RFC 959)
        proto = "ftp://";
    } else {
        MO_DBG_ERR("protocol not supported. Please use ftps:// or ftp://");
        return false;
    }

    //parse FTP URL: dir and fname
    auto dir_pos = ftp_url.find_first_of('/', proto.length());
    if (dir_pos != std::string::npos) {
        auto fname_pos = ftp_url.find_last_of('/');
        dir = ftp_url.substr(dir_pos, fname_pos - dir_pos);
        fname = ftp_url.substr(fname_pos + 1);
    }

    if (fname.empty()) {
        MO_DBG_ERR("missing filename");
        return false;
    }

    MO_DBG_DEBUG("parsed dir: %s; fname: %s", dir.c_str(), fname.c_str());

    //parse FTP URL: user, pass, host, port

    std::string user_pass_host_port = ftp_url.substr(proto.length(), dir_pos - proto.length());
    std::string user_pass, host_port;
    auto user_pass_delim = user_pass_host_port.find_first_of('@');
    if (user_pass_delim != std::string::npos) {
        host_port = user_pass_host_port.substr(user_pass_delim + 1);
        user_pass = user_pass_host_port.substr(0, user_pass_delim);
    } else {
        host_port = user_pass_host_port;
    }

    if (!user_pass.empty()) {
        auto user_delim = user_pass.find_first_of(':');
        if (user_delim != std::string::npos) {
            user = user_pass.substr(0, user_delim);
            pass = user_pass.substr(user_delim + 1);
        } else {
            user = user_pass;
        }
    }

    MO_DBG_DEBUG("parsed user: %s; pass: %.2s***", user.c_str(), pass.empty() ? "-" : pass.c_str());

    if (host_port.empty()) {
        MO_DBG_ERR("missing hostname");
        return false;
    }

    auto host_port_delim = host_port.find(':');
    if (host_port_delim != std::string::npos) {
        ctrl_host = host_port.substr(0, host_port_delim);
        ctrl_port = host_port.substr(host_port_delim + 1);
    } else {
        //use default port number
        ctrl_host = host_port;
        ctrl_port = "21";
    }

    MO_DBG_DEBUG("parsed host: %s; port: %s", ctrl_host.c_str(), ctrl_port.c_str());

    return true;
}

bool FtpTransferMbedTLS::read_url_data(const char *data_url_raw) {

    std::string data_url = data_url_raw; //format like " Entering Passive Mode (h1,h2,h3,h4,p1,p2)"

    // parse address field. Replace all non-digits by delimiter character ' '
    for (char& c : data_url) {
        if (c < '0' || c > '9') {
            c = (unsigned char) ' ';
        }
    }

    unsigned int h1 = 0, h2 = 0, h3 = 0, h4 = 0, p1 = 0, p2 = 0;

    auto ntokens = sscanf(data_url.c_str(), "%u %u %u %u %u %u", &h1, &h2, &h3, &h4, &p1, &p2);
    if (ntokens != 6) {
        MO_DBG_ERR("could not process data url. Expect format: (h1,h2,h3,h4,p1,p2)");
        return false;
    }

    unsigned int port = 256U * p1 + p2;

    char buf [64] = {'\0'};
    auto ret = snprintf(buf, 64, "%u.%u.%u.%u", h1, h2, h3, h4);
    if (ret < 0 || ret >= 64) {
        MO_DBG_ERR("data url format failure");
        return false;
    }
    data_host = buf;

    ret = snprintf(buf, 64, "%u", port);
    if (ret < 0 || ret >= 64) {
        MO_DBG_ERR("data url format failure");
        return false;
    }
    data_port = buf;

    return true;
}

FtpClientMbedTLS::FtpClientMbedTLS(bool tls_only, const char *client_cert, const char *client_key)
        : client_cert(client_cert), client_key(client_key), tls_only(tls_only) {

}

std::unique_ptr<FtpDownload> FtpClientMbedTLS::getFile(const char *ftp_url_raw, std::function<size_t(unsigned char *data, size_t len)> fileWriter, std::function<void(MO_FtpCloseReason)> onClose, const char *ca_cert) {

    auto ftp_handle = std::unique_ptr<FtpTransferMbedTLS>(new FtpTransferMbedTLS(tls_only, client_cert, client_key));
    if (!ftp_handle) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    bool success = ftp_handle->getFile(ftp_url_raw, fileWriter, onClose, ca_cert);

    if (success) {
        return ftp_handle;
    } else {
        return nullptr;
    }
}

std::unique_ptr<FtpUpload> FtpClientMbedTLS::postFile(const char *ftp_url_raw, std::function<size_t(unsigned char *out, size_t buffsize)> fileReader, std::function<void(MO_FtpCloseReason)> onClose, const char *ca_cert) {
    
    auto ftp_handle = std::unique_ptr<FtpTransferMbedTLS>(new FtpTransferMbedTLS(tls_only, client_cert, client_key));
    if (!ftp_handle) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }

    bool success = ftp_handle->postFile(ftp_url_raw, fileReader, onClose, ca_cert);

    if (success) {
        return ftp_handle;
    } else {
        return nullptr;
    }
}

} //namespace MicroOcpp

#endif //MO_ENABLE_MBEDTLS
