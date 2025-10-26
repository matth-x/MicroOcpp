// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/Certificate.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

#include <string.h>
#include <stdio.h>

bool mo_cert_equals(const mo_cert_hash *h1, const mo_cert_hash *h2) {
    return h1->hashAlgorithm == h2->hashAlgorithm &&
            h1->serialNumberLen == h2->serialNumberLen && !memcmp(h1->serialNumber, h2->serialNumber, h1->serialNumberLen) &&
            !memcmp(h1->issuerNameHash, h2->issuerNameHash, HashAlgorithmSize(h1->hashAlgorithm)) &&
            !memcmp(h1->issuerKeyHash, h2->issuerKeyHash, HashAlgorithmSize(h1->hashAlgorithm));
}

int mo_cert_bytes_to_hex(char *dst, size_t dst_size, const unsigned char *src, size_t src_len) {
    if (!dst || !dst_size || !src) {
        return -1;
    }

    dst[0] = '\0';

    size_t hexLen = 2 * src_len; // hex-encoding needs two characters per byte

    if (dst_size < hexLen + 1) { // buf will hold hex-encoding + terminating null
        return -1;
    }

    for (size_t i = 0; i < src_len; i++) {
        snprintf(dst, 3, "%02X", src[i]);
        dst += 2;
    }

    return (int)hexLen;
}

int mo_cert_print_issuerNameHash(const mo_cert_hash *src, char *buf, size_t size) {
    return mo_cert_bytes_to_hex(buf, size, src->issuerNameHash, HashAlgorithmSize(src->hashAlgorithm));
}

int mo_cert_print_issuerKeyHash(const mo_cert_hash *src, char *buf, size_t size) {
    return mo_cert_bytes_to_hex(buf, size, src->issuerKeyHash, HashAlgorithmSize(src->hashAlgorithm));
}

int mo_cert_print_serialNumber(const mo_cert_hash *src, char *buf, size_t size) {

    if (!buf || !size) {
        return -1;
    }

    buf[0] = '\0';

    if (!src->serialNumberLen) {
        return 0;
    }

    int hexLen = snprintf(buf, size, "%X", src->serialNumber[0]);
    if (hexLen < 0 || (size_t)hexLen >= size) {
        return -1;
    }

    if (src->serialNumberLen > 1) {
        auto ret = mo_cert_bytes_to_hex(buf + (size_t)hexLen, size - (size_t)hexLen, src->serialNumber + 1, src->serialNumberLen - 1);
        if (ret < 0) {
            return -1;
        }
        hexLen += ret;
    }

    return hexLen;
}

int mo_cert_hex_to_bytes(unsigned char *dst, size_t dst_size, const char *hex_src) {
    if (!dst || !dst_size || !hex_src) {
        return -1;
    }

    dst[0] = '\0';

    size_t hex_len = strlen(hex_src);

    size_t write_len = (hex_len + 1) / 2;

    if (dst_size < write_len) {
        return -1;
    }

    for (size_t i = 0; i < write_len; i++) {
        char octet [2];

        if (i == 0 && hex_len % 2) {
            octet[0] = '0';
            octet[1] = hex_src[2*i];
        } else {
            octet[0] = hex_src[2*i];
            octet[1] = hex_src[2*i + 1];
        }

        unsigned char val = 0;

        for (size_t j = 0; j < 2; j++) {
            char c = octet[j];
            if (c >= '0' && c <= '9') {
                val += c - '0';
            } else if (c >= 'A' && c <= 'F') {
                val += (c - 'A') + 0xA;
            } else if (c >= 'a' && c <= 'f') {
                val += (c - 'a') + 0xA;
            } else {
                return -1;
            }

            if (j == 0) {
                val *= 0x10;
            }
        }

        dst[i] = val;
    }

    return (int)write_len;
}

int mo_cert_set_issuerNameHash(mo_cert_hash *dst, const char *hex_src, HashAlgorithmType hash_algorithm) {
    auto ret = mo_cert_hex_to_bytes(dst->issuerNameHash, sizeof(dst->issuerNameHash), hex_src);

    if (ret < 0) {
        return ret;
    }

    if (ret != HashAlgorithmSize(hash_algorithm)) {
        return -1;
    }

    return ret;
}

int mo_cert_set_issuerKeyHash(mo_cert_hash *dst, const char *hex_src, HashAlgorithmType hash_algorithm) {
    auto ret = mo_cert_hex_to_bytes(dst->issuerKeyHash, sizeof(dst->issuerNameHash), hex_src);

    if (ret < 0) {
        return ret;
    }

    if (ret != HashAlgorithmSize(hash_algorithm)) {
        return -1;
    }

    return ret;
}

int mo_cert_set_serialNumber(mo_cert_hash *dst, const char *hex_src) {
    auto ret = mo_cert_hex_to_bytes(dst->serialNumber, sizeof(dst->serialNumber), hex_src);

    if (ret < 0) {
        return ret;
    }

    dst->serialNumberLen = (size_t)ret;

    return ret;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
