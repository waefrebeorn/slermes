/*
 * config_backups.c — C home for upstream Hermes hermes_cli/config_backups.py.
 *
 * PoP: w12_configback_58ee75_backups_dir @ hermes_cli/config_backups.py:backups_dir
 * PoP: w12_configback_58ee75_list       @ hermes_cli/config_backups.py:list_config_backups
 *
 * Pure path/prefix logic. Directory contents are injected as a JSON string
 * array (`files`) because the live function calls Path.iterdir/is_file.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_configback_58ee75.h"

/* Path(config).parent / "backups/config", as POSIX text. */
static void root_of(const char *config, char *out, size_t cap) {
    const char *slash = strrchr(config, '/');
    if (!slash) { snprintf(out, cap, "backups/config"); return; }
    size_t plen = (size_t)(slash - config);
    if (plen == 0) snprintf(out, cap, "/backups/config");
    else snprintf(out, cap, "%.*s/backups/config", (int)plen, config);
}

static const char *base_of(const char *config) {
    const char *slash = strrchr(config, '/');
    return slash ? slash + 1 : config;
}

json_t *w12_configback_58ee75_backups_dir(const char *config) {
    char root[2048];
    root_of(config ? config : "", root, sizeof root);
    return json_string(root);
}

json_t *w12_configback_58ee75_list(const char *config, const char *reason,
                                   const json_t *files) {
    json_t *out = json_array();
    char root[2048], prefix[2048];
    root_of(config ? config : "", root, sizeof root);
    const char *name = base_of(config ? config : "");
    if (reason && *reason) snprintf(prefix, sizeof prefix, "%s.%s.", name, reason);
    else snprintf(prefix, sizeof prefix, "%s.", name);
    size_t plen = strlen(prefix);

    char **names = NULL;
    size_t n = 0, cap = 0;
    if (files && files->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(files); ++i) {
            const json_t *f = json_get(files, i);
            if (!f || f->type != JSON_STRING) continue;
            const char *s = f->str_val ? f->str_val : "";
            if (strncmp(s, prefix, plen) != 0) continue;
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                char **tmp = (char **)realloc(names, cap * sizeof *tmp);
                if (!tmp) break;
                names = tmp;
            }
            names[n++] = strdup(s);
        }
    }
    /* sorted(..., reverse=True): lexicographically greatest name first. */
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (strcmp(names[i], names[j]) < 0) {
                char *t = names[i]; names[i] = names[j]; names[j] = t;
            }
        }
    }
    for (size_t i = 0; i < n; ++i) {
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", root, names[i]);
        json_append(out, json_string(full));
        free(names[i]);
    }
    free(names);
    return out;
}
