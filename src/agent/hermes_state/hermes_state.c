/*
 * port_hermes_state.c — C11 port of pure helpers from hermes_state.py.
 *
 * Faithful translations of the deterministic helpers:
 * - _system_prompt_hash: SHA-256 hex digest
 * - is_disk_full_error: disk-full / ENOSPC classifier
 * - resolve_journal_mode: config.yaml journal mode reader
 *
 * Reuses libjson (lib/libjson/json.h) for config.yaml parsing
 * and OpenSSL (lib/libcrypto) for SHA-256.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_hermes_state.h"
#include "libjson/json.h"
#include "slermes_home.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* OpenSSL SHA-256 */
#include <openssl/evp.h>

/* ------------------------------------------------------------------ */
/* _system_prompt_hash                                                 */
/* ------------------------------------------------------------------ */

/* PoP: hs_system_prompt_hash @ hermes_state.py:_system_prompt_hash */
char *hs_system_prompt_hash(const char *system_prompt)
{
    if (!system_prompt) return NULL;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return NULL;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    if (EVP_DigestUpdate(ctx, system_prompt, strlen(system_prompt)) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);

    char *hex = malloc(hash_len * 2 + 1);
    if (!hex) return NULL;
    for (unsigned int i = 0; i < hash_len; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return hex;
}

/* ------------------------------------------------------------------ */
/* is_disk_full_error                                                  */
/* ------------------------------------------------------------------ */

/* _DISK_FULL_MARKERS from hermes_state.py */
static const char *_disk_full_markers[] = {
    "no space left on device",
    "not enough space",
    "database or disk is full",
    "disk full",
    "full disk",
    "enospc",
    NULL,
};

/* PoP: hs_is_disk_full_error @ hermes_state.py:is_disk_full_error */
bool hs_is_disk_full_error(const char *exc)
{
    if (!exc) return false;
    /* Check for ENOSPC in OSError-style strings */
    if (strstr(exc, "ENOSPC") != NULL) return true;
    /* Check for errno number in string */
    char *end;
    long errno_val = strtol(exc, &end, 10);
    if (end != exc && errno_val == 28) return true; /* ENOSPC = 28 */

    /* Lowercase copy for marker matching */
    size_t len = strlen(exc);
    char *lower = malloc(len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)exc[i]);
    lower[len] = '\0';

    bool found = false;
    for (int i = 0; _disk_full_markers[i] && !found; i++) {
        if (strstr(lower, _disk_full_markers[i]))
            found = true;
    }
    free(lower);
    return found;
}

/* ------------------------------------------------------------------ */
/* resolve_journal_mode                                                */
/* ------------------------------------------------------------------ */

char *hs_resolve_journal_mode(void)
{
    /* database.journal_mode in config.yaml is the canonical operator setting.
     * wal remains the default; use delete when the backing filesystem cannot
     * provide WAL-safe durability (macOS virtiofs, NFS, SMB). Invalid values
     * fail safely to the default. Config resolution follows the active
     * profile via get_slermes_home()/slermes_home(), matching cli/config.c. */
    char home[HERMES_PATH_MAX];
    {
        const char *envh = getenv("HERMES_HOME");
        if (envh && *envh) snprintf(home, sizeof home, "%s", envh);
        else {
            const char *sh = slermes_home();
            snprintf(home, sizeof home, "%s", (sh && *sh) ? sh :
                     (getenv("HOME") ? getenv("HOME") : "/tmp"));
        }
    }
    char cfg_path[HERMES_PATH_MAX + 32];
    snprintf(cfg_path, sizeof cfg_path, "%s/config.yaml", home);

    const char *mode = "wal"; /* safe default */
    FILE *f = fopen(cfg_path, "r");
    if (!f) {
        char *fallback = strdup(mode);
        return fallback;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize > 0 && fsize < 10 * 1024 * 1024) {
        char *buf = malloc((size_t)fsize + 1);
        if (buf) {
            size_t rd = fread(buf, 1, (size_t)fsize, f);
            buf[rd] = '\0';
            json_t *config = json_parse(buf, NULL);
            if (config && config->type == JSON_OBJECT) {
                json_t *database = json_obj_get(config, "database");
                if (database && database->type == JSON_OBJECT) {
                    json_t *jm = json_obj_get(database, "journal_mode");
                    if (jm && jm->type == JSON_STRING) {
                        const char *raw = jm->str_val;
                        /* strip + lowercase */
                        char lowered[64];
                        size_t rl = strlen(raw);
                        if (rl >= sizeof lowered) rl = sizeof lowered - 1;
                        size_t st = 0;
                        while (st < rl && isspace((unsigned char)raw[st])) st++;
                        size_t di = 0;
                        for (size_t k = st; k < rl && di < sizeof lowered - 1; k++)
                            lowered[di++] = (char)tolower((unsigned char)raw[k]);
                        lowered[di] = '\0';
                        while (di > 0 && isspace((unsigned char)lowered[di-1])) lowered[--di]='\0';
                        if (strcmp(lowered, "wal")==0 || strcmp(lowered,"delete")==0)
                            mode = (lowered[0]=='d') ? "delete" : "wal";
                    }
                }
            }
            if (config) json_free(config);
            free(buf);
        }
    }
    fclose(f);
    return strdup(mode);
}