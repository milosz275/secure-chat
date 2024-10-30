#include "client_openssl.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "log.h"

int init_ssl(struct client* cl)
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    cl->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!cl->ssl_ctx)
        return OPENSSL_SSL_CTX_CREATION_FAILURE;
    cl->ssl = SSL_new(cl->ssl_ctx);
    if (!cl->ssl)
    {
        destroy_ssl(cl);
        return OPENSSL_SSL_OBJECT_FAILURE;
    }
    SSL_set_fd(cl->ssl, cl->sock);
    return OPENSSL_INIT_SUCCESS;
}

int destroy_ssl(struct client* cl)
{
    SSL_shutdown(cl->ssl);
    if (cl->ssl)
        SSL_free(cl->ssl);
    if (cl->ssl_ctx)
        SSL_CTX_free(cl->ssl_ctx);
    close(cl->sock);
    return 0;
}
