/*
 * provider_bedrock_sign.h — AWS SigV4 signing helpers for the Bedrock provider.
 *
 * Module-internal header (provider_bedrock.c <-> provider_bedrock_sign.c).
 */
#ifndef PROVIDER_BEDROCK_SIGN_H
#define PROVIDER_BEDROCK_SIGN_H

#include <stddef.h>
#include "provider.h"

const char * bedrock_get_region(const char *base_url);
char * sigv4_sign(const char *method, const char *host, const char *path, const char *query_string, const char *payload, const char *access_key, const char *secret_key, const char *region, const char *service, char *amz_date_out, size_t amz_date_sz);

char * bedrock_build_url(const provider_t *p, const char *base_url);

#endif /* PROVIDER_BEDROCK_SIGN_H */
