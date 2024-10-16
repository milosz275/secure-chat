#include "server_openssl.h"

#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <sys/stat.h>

#include "protocol.h"
#include "log.h"
#include "server.h"

int init_ssl(server_t* server)
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    server->ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!server->ssl_ctx)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to create SSL context");
        finish_logging();
        return OPENSSL_SSL_CTX_CREATION_FAILURE;
    }
    check_and_generate_key_cert();
    if (SSL_CTX_use_certificate_file(server->ssl_ctx, SERVER_CERT_FILE, SSL_FILETYPE_PEM) <= 0)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to load server certificate");
        finish_logging();
        destroy_ssl(server);
        return OPENSSL_CERTIFICATE_LOAD_FAILURE;
    }
    if (SSL_CTX_use_PrivateKey_file(server->ssl_ctx, SERVER_KEY_FILE, SSL_FILETYPE_PEM) <= 0)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to load server private key");
        finish_logging();
        destroy_ssl(server);
        return OPENSSL_PRIVATE_KEY_LOAD_FAILURE;
    }
    server->ssl = SSL_new(server->ssl_ctx);
    if (!server->ssl)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to create SSL object");
        finish_logging();
        destroy_ssl(server);
        return OPENSSL_SSL_OBJECT_FAILURE;
    }
    SSL_set_fd(server->ssl, server->socket);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "SSL initialization successful");
    return OPENSSL_INIT_SUCCESS;
}

int destroy_ssl(server_t* server)
{
    if (server->ssl)
        SSL_free(server->ssl);
    if (server->ssl_ctx)
        SSL_CTX_free(server->ssl_ctx);
    return 1;
}

int file_exists(const char* filename)
{
    struct stat buffer;
    return (!stat(filename, &buffer));
}

int generate_rsa_key(const char* key_file)
{
    EVP_PKEY_CTX* ctx;
    EVP_PKEY* pkey = NULL;
    FILE* file;

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error creating context.");
        return -1;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error initializing keygen.");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, RSA_KEY_LENGTH) <= 0)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error setting RSA key length.");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error generating RSA key.");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    file = fopen(key_file, "wb");
    if (!file)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error opening key file.");
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (!PEM_write_PrivateKey(file, pkey, NULL, NULL, 0, NULL, NULL))
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error writing key to file.");
        fclose(file);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    fclose(file);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "RSA key generation successful");
    return 0;
}

int generate_self_signed_certificate(const char* cert_file, const char* key_file)
{
    EVP_PKEY* pkey = EVP_PKEY_new();
    X509* x509 = X509_new();
    FILE* file;

    FILE* key_fp = fopen(key_file, "rb");
    if (!key_fp)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Unable to open key file for reading");
        return -1;
    }

    pkey = PEM_read_PrivateKey(key_fp, NULL, NULL, NULL);
    fclose(key_fp);

    if (!pkey)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error reading private key from file");
        return -1;
    }

    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), CERT_VALIDITY_PERIOD);

    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (const unsigned char*)"My Organization", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)"localhost", -1, -1, 0);

    X509_set_issuer_name(x509, name);

    if (!X509_sign(x509, pkey, EVP_sha256()))
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Error signing certificate.");
        EVP_PKEY_free(pkey);
        X509_free(x509);
        return -1;
    }

    file = fopen(cert_file, "wb");
    if (!file)
    {
        log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Unable to open cert file for writing");
        EVP_PKEY_free(pkey);
        X509_free(x509);
        return -1;
    }
    PEM_write_X509(file, x509);
    fclose(file);

    EVP_PKEY_free(pkey);
    X509_free(x509);

    log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Self-signed certificate generated and saved to %s", cert_file);
    return 0;
}

void check_and_generate_key_cert()
{
    if (!file_exists(SERVER_KEY_FILE))
    {
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Key file not found. Generating new RSA key...");
        if (generate_rsa_key(SERVER_KEY_FILE) != 0)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to generate RSA key.");
            exit(EXIT_FAILURE);
        }
    }
    else
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Key file exists: %s", SERVER_KEY_FILE);

    if (!file_exists(SERVER_CERT_FILE))
    {
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Certificate file not found. Generating new self-signed certificate...");
        if (generate_self_signed_certificate(SERVER_CERT_FILE, SERVER_KEY_FILE) != 0)
        {
            log_message(T_LOG_ERROR, SERVER_LOG, __FILE__, "Failed to generate self-signed certificate.");
            exit(EXIT_FAILURE);
        }
    }
    else
        log_message(T_LOG_INFO, SERVER_LOG, __FILE__, "Certificate file exists: %s", SERVER_CERT_FILE);
}
