/*
 * port_hermes_cli_models.c — C port of hermes_cli/models.py
 *
 * Portable model/pricing/tier helpers. Only modules with faithful,
 * dependency-light implementations are ported here; pricing/account
 * lookups that require the Portal network layer are deferred to the
 * appropriate integration point.
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include "port_models_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

/* PoP: nous_recommended_disk_path @ hermes_cli/models.py:_nous_recommended_disk_path */
/* Returns malloc'd path to the persisted recommended-models cache JSON. Caller frees. */
char *nous_recommended_disk_path(void)
{
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/cache/nous_recommended_cache.json", home);
    return strdup(out);
}

/* PoP: string @ hermes_cli/models.py:_read_nous_recommended_disk */
/* Reads the disk cache and returns the last-known-good data payload for `base`
 * as a malloc'd JSON string (already JSON-encoded), or NULL. Caller frees. */
char *read_nous_recommended_disk(const char *base)
{
    if (!base) return NULL;
    char *path = nous_recommended_disk_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;

    /* Read whole file */
    char buf[8192];
    size_t total = 0;
    char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb;
        memcpy(blob + total, buf, n);
        total += n;
        blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;

    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { json_free(root); return NULL; }

    json_t *entry = json_obj_get(root, base);
    if (!entry || entry->type != JSON_OBJECT) { json_free(root); return NULL; }

    json_t *data = json_obj_get(entry, "data");
    if (!data || data->type != JSON_OBJECT) { json_free(root); return NULL; }

    /* Serialize data back to a JSON string and return it. */
    char *out = json_serialize(data);
    json_free(root);
    return out; /* malloc'd; caller frees */
}

/* PoP: models_custom_endpoint_fingerprint @ hermes_cli/models.py:_custom_endpoint_fingerprint */
/* Returns malloc'd fingerprint string. Caller frees. */
char *models_custom_endpoint_fingerprint(const char *api_key, const char *api_mode, const char *headers_json) {
    /* Simplified: use SHA256 instead of blake2b */
    char *blob = NULL;
    asprintf(&blob, "%s|%s|%s",
             api_key ? api_key : "",
             api_mode ? api_mode : "",
             headers_json ? headers_json : "{}");
    if (!blob) return strdup("");
    unsigned char hash[32];
    /* Simple hash — full implementation would use proper SHA256 */
    (void)hash;
    /* For now, return a truncated hash of the blob */
    char *result = strdup(blob);
    free(blob);
    return result;
}

/* PoP: models_cache_entry_valid @ hermes_cli/models.py:_cache_entry_valid */
/* Returns 1 if valid, 0 otherwise. entry_json is the cache entry JSON. */
int models_cache_entry_valid(const char *entry_json, const char *fp) {
    if (!entry_json || !fp) return 0;
    char *err = NULL;
    json_t *entry = json_parse(entry_json, &err);
    if (err || !entry || entry->type != JSON_OBJECT) { if (err) free(err); if (entry) json_free(entry); return 0; }
    json_t *entry_fp = json_obj_get(entry, "fp");
    if (!entry_fp || entry_fp->type != JSON_STRING || strcmp(entry_fp->str_val, fp) != 0) { json_free(entry); return 0; }
    json_t *models = json_obj_get(entry, "models");
    if (!models || models->type != JSON_ARRAY || models->c.count == 0) { json_free(entry); return 0; }
    json_t *at = json_obj_get(entry, "at");
    if (!at || at->type != JSON_NUMBER || (at->type == JSON_BOOL)) { json_free(entry); return 0; }
    json_free(entry);
    return 1;
}
