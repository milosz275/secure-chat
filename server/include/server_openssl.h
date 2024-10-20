#ifndef __SERVER_OPENSSL_H
#define __SERVER_OPENSSL_H

#define SERVER_KEY_FILE "server.key"
#define SERVER_CERT_FILE "server.crt"

#define RSA_KEY_LENGTH 4096
#define CERT_VALIDITY_PERIOD 31536000L // 1 year in seconds

// OpenSSL result codes
#define OPENSSL_INIT_SUCCESS 4000
#define OPENSSL_SSL_CTX_CREATION_FAILURE 4001
#define OPENSSL_CERTIFICATE_LOAD_FAILURE 4002
#define OPENSSL_PRIVATE_KEY_LOAD_FAILURE 4003
#define OPENSSL_SSL_OBJECT_FAILURE 4004

#include "server.h"

/**
 * Initialize SSL. This function is used to initialize OpenSSL, its context and all structures.
 *
 * @param srv The server structure.
 * @return The exit code.
 */
int init_ssl(struct server* srv);

/**
 * Destroy SSL. This function is used to deinitialize OpenSSL, its context and all structures.
 *
 * @param srv The server structure.
 * @return The exit code.
 */
int destroy_ssl(struct server* srv);

/**
 * Check if file exists. This function is used to check if a file exists.
 *
 * @param filename The file name.
 * @return 1 if the file exists, 0 otherwise.
 */
int file_exists(const char* filename);

/**
 * Generate RSA key. This function is used to generate an RSA key.
 *
 * @param key_file The key file name.
 * @return The exit code.
 */
int generate_rsa_key(const char* key_file);

/**
 * Generate self-signed certificate. This function is used to generate a self-signed certificate.
 *
 * @param cert_file The certificate file name.
 * @param key_file The key file name.
 * @return The exit code.
 */
int generate_self_signed_certificate(const char* cert_file, const char* key_file);

/**
 * Check and generate key and certificate. This function is used to check if the key and certificate exist and generate them if they do not.
 */
void check_and_generate_key_cert();

#endif
