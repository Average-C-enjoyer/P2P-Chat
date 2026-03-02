#pragma once

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/sha.h>

static inline int init_openssl()
{
   SSL_library_init();
   SSL_load_error_strings();
   OpenSSL_add_all_algorithms();
   return 1;
}

static inline SSL_CTX *create_ctx()
{
   SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
   if (!ctx) return NULL;

   if (!SSL_CTX_load_verify_locations(ctx, "server.crt", NULL)) {
      printf("Failed to load server.crt\n");
      return NULL;
   }

   SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
   SSL_CTX_set_default_verify_paths(ctx);

   return ctx;
}

static inline int verify_certificate(SSL *ssl)
{
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) return 0;

    long res = SSL_get_verify_result(ssl);
    X509_free(cert);

    return res == X509_V_OK;
}
