/*
 * path_utils.c — C port of upstream Hermes tools/environments/path_utils.py.
 * PoP: sanitizeid_sanitize_task_id_for_path @ tools/environments/path_utils.py:sanitize_task_id_for_path
 */
#include "noc_sanitizeid.h"
#include <openssl/sha.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Decode one UTF-8 code point. Python string length/slicing/regex work on
 * code points, so the port must too (a multi-byte char -> one "_"). */
static void cp_decode(const unsigned char *s, size_t n, size_t *i, uint32_t *cp) {
    unsigned char c = s[*i];
    if (c < 0x80) { *cp = c; (*i) += 1; return; }
    if ((c & 0xE0) == 0xC0 && *i + 1 < n) {
        *cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[*i + 1] & 0x3F);
        *i += 2; return;
    }
    if ((c & 0xF0) == 0xE0 && *i + 2 < n) {
        *cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[*i + 1] & 0x3F) << 6)
            | (uint32_t)(s[*i + 2] & 0x3F);
        *i += 3; return;
    }
    if ((c & 0xF8) == 0xF0 && *i + 3 < n) {
        *cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[*i + 1] & 0x3F) << 12)
            | ((uint32_t)(s[*i + 2] & 0x3F) << 6) | (uint32_t)(s[*i + 3] & 0x3F);
        *i += 4; return;
    }
    *cp = c; (*i) += 1; /* malformed byte: treat as its own code point */
}

static size_t cp_encode(uint32_t cp, unsigned char *out) {
    if (cp < 0x80) { out[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F)); return 2;
    }
    if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F)); return 3;
    }
    out[0] = (unsigned char)(0xF0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (cp & 0x3F)); return 4;
}

/* _SANDBOX_DIR_UNSAFE_RE = [^A-Za-z0-9._-] */
static int cp_safe(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')
        || (cp >= '0' && cp <= '9') || cp == '.' || cp == '_' || cp == '-';
}

#define SANITIZEID_MAX_LEN 128
#define SANITIZEID_HASH_LEN 12
#define CLEAN(j) (cp_safe(cps[j]) ? cps[j] : (uint32_t)'_')

char *sanitizeid_sanitize_task_id_for_path(const char *task_id) {
    const char *value = task_id ? task_id : "";
    if (!*value) return strdup("default");

    size_t blen = strlen(value);
    uint32_t *cps = (uint32_t *)malloc(blen * sizeof(uint32_t));
    if (!cps) return strdup("default");
    size_t n = 0;
    for (size_t i = 0; i < blen;) {
        uint32_t cp; cp_decode((const unsigned char *)value, blen, &i, &cp);
        cps[n++] = cp;
    }

    int all_safe = 1;
    for (size_t j = 0; j < n; j++) {
        if (!cp_safe(cps[j])) { all_safe = 0; break; }
    }
    if (all_safe && n <= SANITIZEID_MAX_LEN
            && !(n == 1 && cps[0] == '.')
            && !(n == 2 && cps[0] == '.' && cps[1] == '.')
            && !(n > 0 && (cps[n - 1] == '.' || cps[n - 1] == ' '))) {
        free(cps);
        return strdup(value);
    }

    /* digest = sha256(value.encode("utf-8")).hexdigest()[:12] */
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)value, blen, md);
    static const char hex[] = "0123456789abcdef";
    char digest[SANITIZEID_HASH_LEN + 1];
    for (int i = 0; i < SANITIZEID_HASH_LEN; i++) {
        unsigned char byte = md[i / 2];
        digest[i] = hex[(byte >> (4 * (1 - (i & 1)))) & 0x0F];
    }
    digest[SANITIZEID_HASH_LEN] = 0;

    /* stem = cleaned[:128-12-1].strip("._") */
    size_t lim = n < (SANITIZEID_MAX_LEN - SANITIZEID_HASH_LEN - 1)
        ? n : (SANITIZEID_MAX_LEN - SANITIZEID_HASH_LEN - 1);
    size_t a = 0, b = lim;
    while (a < b && (CLEAN(a) == '.' || CLEAN(a) == '_')) a++;
    while (b > a && (CLEAN(b - 1) == '.' || CLEAN(b - 1) == '_')) b--;

    char *out = (char *)malloc(lim * 4 + SANITIZEID_HASH_LEN + 8);
    size_t k = 0;
    if (a == b) {
        memcpy(out, "task", 4); k = 4;
    } else {
        for (size_t j = a; j < b; j++)
            k += cp_encode(CLEAN(j), (unsigned char *)out + k);
    }
    out[k++] = '-';
    memcpy(out + k, digest, SANITIZEID_HASH_LEN);
    k += SANITIZEID_HASH_LEN;
    out[k] = 0;
    free(cps);
    return out;
}
