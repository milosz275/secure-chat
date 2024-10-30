#ifndef __CLIENT_OPENSSL_H
#define __CLIENT_OPENSSL_H

#include "client.h"

#define OPENSSL_INIT_SUCCESS 9000
#define OPENSSL_SSL_CTX_CREATION_FAILURE 9001
#define OPENSSL_SSL_OBJECT_FAILURE 9002

/**
 * Initialize SSL. This function is used to initialize OpenSSL, its context and all structures.
 *
 * @param cl The client structure.
 * @return The exit code.
 */
int init_ssl(struct client* cl);

/**
 * Destroy SSL. This function is used to deinitialize OpenSSL, its context and all structures.
 *
 * @param cl The client structure.
 * @return The exit code.
 */
int destroy_ssl(struct client* cl);

#endif
