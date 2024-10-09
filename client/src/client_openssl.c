#include "client_openssl.h"

#include <openssl/ssl.h>

#include "log.h"

int init_ssl(client_t* client)
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    client->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!client->ssl_ctx)
    {
        log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Failed to create SSL context");
        finish_logging();
        return OPENSSL_SSL_CTX_CREATION_FAILURE;
    }
    client->ssl = SSL_new(client->ssl_ctx);
    if (!client->ssl)
    {
        log_message(T_LOG_ERROR, CLIENT_LOG, __FILE__, "Failed to create SSL object");
        finish_logging();
        destroy_ssl(client);
        return OPENSSL_SSL_OBJECT_FAILURE;
    }
    SSL_set_fd(client->ssl, client->socket);
    log_message(T_LOG_INFO, CLIENT_LOG, __FILE__, "SSL initialization successful");
    return OPENSSL_INIT_SUCCESS;
}

int destroy_ssl(client_t* client)
{
    if (client->ssl)
        SSL_free(client->ssl);
    if (client->ssl_ctx)
        SSL_CTX_free(client->ssl_ctx);
    return 0;
}
