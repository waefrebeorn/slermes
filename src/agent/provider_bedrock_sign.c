/*
 * provider_bedrock_sign.c — AWS SigV4 signing helpers.
 *
 * Split from provider_bedrock.c (soul.md: self-contained modules). Region
 * resolution, URL building, time stamps, HMAC/SHA256 and URI encoding for
 * SigV4. No file-scope state.
 */
#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_encode.h"
#include "provider.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>


/* ================================================================
 *  SigV4 helpers
 * ================================================================ */

/* SHA-256 hex digest of a string */
static char *sha256_hex(const char *data, size_t len) {
    unsigned char hash[32];
    unsigned int hash_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    static const char hex[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (!out) return NULL;
    for (unsigned int i = 0; i < hash_len; i++) {
        out[i*2]     = hex[hash[i] >> 4];
        out[i*2+1]   = hex[hash[i] & 0xf];
    }
    out[64] = '\0';
    return out;
}


/* HMAC-SHA256, returns malloc'd binary */
static unsigned char *hmac_sha256(const unsigned char *key, size_t key_len,
                                   const char *data, size_t data_len,
                                   unsigned int *out_len) {
    unsigned char *result = (unsigned char *)malloc(32);
    if (!result) return NULL;
    HMAC(EVP_sha256(), key, (int)key_len,
         (const unsigned char *)data, data_len,
         result, out_len);
    return result;
}


/* Get current UTC time in ISO8601 format: YYYYMMDDTHHMMSSZ */
static void get_iso8601_time(char *buf, size_t buf_size) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, buf_size, "%Y%m%dT%H%M%SZ", &tm);
}


/* Get date stamp from ISO time: YYYYMMDD */
static void get_date_stamp(const char *iso_time, char *buf, size_t buf_size) {
    (void)buf_size;
    memcpy(buf, iso_time, 8);
    buf[8] = '\0';
}


/* URI encode a string for SigV4 canonical request */
static char *uri_encode(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = c;
        } else if (c == '/') {
            out[j++] = c;
        } else {
            snprintf(out + j, 4, "%%%02X", c);
            j += 3;
        }
    }
    out[j] = '\0';
    return out;
}


/* Build SigV4 Authorization header value and return full header string */
char *sigv4_sign(const char *method, const char *host, const char *path,
                         const char *query_string, const char *payload,
                         const char *access_key, const char *secret_key,
                         const char *region, const char *service,
                         char *amz_date_out, size_t amz_date_sz) {
    char amz_date[17];
    get_iso8601_time(amz_date, sizeof(amz_date));
    if (amz_date_out) snprintf(amz_date_out, amz_date_sz, "%s", amz_date);

    char date_stamp[9];
    get_date_stamp(amz_date, date_stamp, sizeof(date_stamp));

    /* Payload hash */
    char *payload_hash = sha256_hex(payload, strlen(payload));
    if (!payload_hash) return NULL;

    /* Canonical URI — must be URI-encoded path */
    char *canonical_uri = uri_encode(path);

    /* Canonical query string */
    const char *canonical_qs = query_string ? query_string : "";

    /* Canonical headers: host;x-amz-date */
    char host_header[512];
    snprintf(host_header, sizeof(host_header), "host:%s", host);

    char amz_date_header[64];
    snprintf(amz_date_header, sizeof(amz_date_header), "x-amz-date:%s", amz_date);

    const char *signed_headers = "host;x-amz-date";

    /* Build canonical request */
    size_t cr_size = strlen(method) + strlen(canonical_uri) + strlen(canonical_qs)
                     + strlen(host_header) + strlen(amz_date_header)
                     + strlen(signed_headers) + strlen(payload_hash) + 32;
    char *canonical_request = (char *)malloc(cr_size);
    if (!canonical_request) { free(payload_hash); free(canonical_uri); return NULL; }

    snprintf(canonical_request, cr_size,
        "%s\n%s\n%s\n%s\n%s\n\n%s\n%s",
        method, canonical_uri, canonical_qs,
        host_header, amz_date_header,
        signed_headers, payload_hash);

    free(payload_hash);
    free(canonical_uri);

    /* String to sign */
    char *cr_hash = sha256_hex(canonical_request, strlen(canonical_request));
    free(canonical_request);
    if (!cr_hash) return NULL;

    char scope[128];
    snprintf(scope, sizeof(scope), "%s/%s/%s/aws4_request", date_stamp, region, service);

    size_t sts_size = strlen(cr_hash) + strlen(scope) + 64;
    char *string_to_sign = (char *)malloc(sts_size);
    if (!string_to_sign) { free(cr_hash); return NULL; }

    snprintf(string_to_sign, sts_size,
        "AWS4-HMAC-SHA256\n%s\n%s\n%s",
        amz_date, scope, cr_hash);
    free(cr_hash);

    /* Signing key */
    char kSecret[64];
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", secret_key);

    unsigned int hash_len = 0;
    unsigned char *kDate = hmac_sha256((unsigned char *)kSecret, strlen(kSecret),
                                        date_stamp, strlen(date_stamp), &hash_len);
    if (!kDate) { free(string_to_sign); return NULL; }

    unsigned char *kRegion = hmac_sha256(kDate, hash_len, region, strlen(region), &hash_len);
    free(kDate);
    if (!kRegion) { free(string_to_sign); return NULL; }

    unsigned char *kService = hmac_sha256(kRegion, hash_len, service, strlen(service), &hash_len);
    free(kRegion);
    if (!kService) { free(string_to_sign); return NULL; }

    unsigned char *kSigning = hmac_sha256(kService, hash_len, "aws4_request", 12, &hash_len);
    free(kService);
    if (!kSigning) { free(string_to_sign); return NULL; }

    /* Signature */
    unsigned char *signature_bin = hmac_sha256(kSigning, hash_len,
                                                string_to_sign, strlen(string_to_sign),
                                                &hash_len);
    free(kSigning);
    free(string_to_sign);
    if (!signature_bin) return NULL;

    char *signature_hex = hex_encode_alloc(signature_bin, hash_len);
    free(signature_bin);
    if (!signature_hex) return NULL;

    /* Build full Authorization header value */
    char *auth_header = (char *)malloc(1024);
    if (!auth_header) { free(signature_hex); return NULL; }

    snprintf(auth_header, 1024,
        "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s",
        access_key, scope, signed_headers, signature_hex);
    free(signature_hex);

    /* Build full header string for http_request */
    char *full_headers = (char *)malloc(2048);
    if (!full_headers) { free(auth_header); return NULL; }

    snprintf(full_headers, 2048,
        "Content-Type: application/json\r\n"
        "X-Amz-Date: %s\r\n"
        "Authorization: %s\r\n"
        "Accept: application/json",
        amz_date, auth_header);
    free(auth_header);

    return full_headers;
}


/* Extract region from base_url or default */
const char *bedrock_get_region(const char *base_url) {
    if (!base_url || !*base_url) {
        const char *env = getenv("AWS_REGION");
        if (env && *env) return env;
        env = getenv("AWS_DEFAULT_REGION");
        if (env && *env) return env;
        return "us-east-1";
    }
    /* Try to extract from URL: bedrock-runtime.{region}.amazonaws.com */
    const char *r = strstr(base_url, "bedrock-runtime.");
    if (r) {
        r += 16; /* skip "bedrock-runtime." */
        const char *dot = strchr(r, '.');
        if (dot) {
            static char region_buf[64];
            size_t len = dot - r;
            if (len < sizeof(region_buf)) {
                strncpy(region_buf, r, len);
                region_buf[len] = '\0';
                return region_buf;
            }
        }
    }
    return "us-east-1";
}


/* ================================================================
 *  Bedrock provider implementation
 * ================================================================ */

char *bedrock_build_url(const provider_t *p, const char *base_url) {
    (void)base_url;
    const char *region = bedrock_get_region(NULL);
    const char *model = p->model[0] ? p->model : "anthropic.claude-3-sonnet-20240229-v1:0";

    size_t url_len = strlen(region) + strlen(model) + 128;
    char *url = (char *)malloc(url_len);
    if (!url) return NULL;

    snprintf(url, url_len,
             "https://bedrock-runtime.%s.amazonaws.com/model/%s/converse",
             region, model);
    return url;
}
