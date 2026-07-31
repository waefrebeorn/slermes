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

/* PoP: _nous_recommended_disk_path @ hermes_cli/models.py:_nous_recommended_disk_path */
/* Returns malloc'd path to the persisted recommended-models cache JSON. Caller frees. */
char *nous_recommended_disk_path(void)
{
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/cache/nous_recommended_cache.json", home);
    return strdup(out);
}

/* PoP: _read_nous_recommended_disk @ hermes_cli/models.py:_read_nous_recommended_disk */
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
