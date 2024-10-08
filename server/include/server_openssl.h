#ifndef __SERVER_OPENSSL_H
#define __SERVER_OPENSSL_H

#define SERVER_KEY_FILE "server.key"
#define SERVER_CERT_FILE "server.crt"

// OpenSSL result codes
#define OPENSSL_INIT_SUCCESS 4000
#define OPENSSL_SSL_CTX_CREATION_FAILURE 4001
#define OPENSSL_CERTIFICATE_LOAD_FAILURE 4002
#define OPENSSL_PRIVATE_KEY_LOAD_FAILURE 4003
#define OPENSSL_SSL_OBJECT_FAILURE 4004

#include "server.h"

typedef struct server_t server_t;

/**
 * Initialize SSL. This function is used to initialize OpenSSL, its context and all structures.
 *
 * @param server The server structure.
 * @return The exit code.
 */
int init_ssl(server_t* server);

/**
 * Destroy SSL. This function is used to deinitialize OpenSSL, its context and all structures.
 *
 * @param server The server structure.
 * @return The exit code.
 */
int destroy_ssl(server_t* server);

int file_exists(const char* filename);

int generate_rsa_key(const char* key_file);

int generate_self_signed_certificate(const char* cert_file, const char* key_file);

void check_and_generate_key_cert();

#endif
