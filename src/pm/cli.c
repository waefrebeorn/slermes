/*
 * cli.c — real C11 port of pm/cli.py (gate w5_cli_d7cb5a).
 *
 * PoP: pmcli_fmt_bytes @ pm/cli.py:_fmt_bytes
 * PoP: pmcli_install_flag_error @ pm/cli.py:_install_flag_error
 * PoP: pmcli_split_without @ pm/cli.py:_split_without
 *
 * The command bodies orchestrate installs/network/processes; the deterministic
 * format, flag-validation and --without split seams are ported faithfully.
 */
#include "noc_w5_cli_d7cb5a.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PMCLI_MIB (1024.0 * 1024.0)

static const char *const PMCLI_ALL_TARGETS[] = {
    "win32-x64", "win32-arm64", "linux-x64", "linux-arm64", "linux-x64-musl",
    "linux-arm64-musl", "linux-arm64-bionic", "darwin-x64", "darwin-arm64"
};
#define PMCLI_NTARGETS ((int)(sizeof(PMCLI_ALL_TARGETS) / sizeof(PMCLI_ALL_TARGETS[0])))

char *pmcli_fmt_bytes(long long n) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f MiB", (double)n / PMCLI_MIB);
    return strdup(buf);
}

static json_t *parse_arr(const char *s) {
    json_t *a = json_parse(s ? s : "[]", NULL);
    if (!a || a->type != JSON_ARRAY) { json_free(a); return json_array(); }
    return a;
}
static int arr_len(const json_t *a) {
    return (a && a->type == JSON_ARRAY) ? (int)a->c.count : 0;
}
static const char *arr_str(const json_t *a, int i) {
    if (!a || a->type != JSON_ARRAY || i < 0 || i >= (int)a->c.count) return "";
    json_t *v = a->c.items[i];
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static int arr_has(const json_t *a, const char *s) {
    for (int i = 0; i < arr_len(a); i++) if (!strcmp(arr_str(a, i), s)) return 1;
    return 0;
}
/* ", ".join(list) */
static char *join_csv(const json_t *a) {
    size_t n = 1;
    for (int i = 0; i < arr_len(a); i++) n += strlen(arr_str(a, i)) + 2;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < arr_len(a); i++) {
        if (i) strcat(out, ", ");
        strcat(out, arr_str(a, i));
    }
    return out;
}
/* Python repr() of a str for the "unknown target" message. */
static char *py_repr_str(const char *s) {
    if (!s) s = "";
    int has_s = strchr(s, '\'') != NULL, has_d = strchr(s, '"') != NULL;
    char q = (has_s && !has_d) ? '"' : '\'';
    size_t n = 2;
    for (const char *p = s; *p; p++) n += (*p == '\\' || *p == q) ? 2 : 1;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t i = 0;
    out[i++] = q;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == q) out[i++] = '\\';
        out[i++] = *p;
    }
    out[i++] = q;
    out[i] = '\0';
    return out;
}
static const char *canon(const char *name) {
    /* pm.defaults.RETIRED_DEFAULTS = {"agent-browser": "chromium"} */
    return !strcmp(name, "agent-browser") ? "chromium" : name;
}

/* _split_without(names, defaults, droppable) -> three arrays (json_t owned by caller). */
static void split_without(const json_t *names, const json_t *defs, const json_t *drop,
                          json_t **buckets) {
    for (int b = 0; b < 3; b++) buckets[b] = json_array();
    for (int i = 0; i < arr_len(names); i++) {
        const char *name = arr_str(names, i);
        const char *package = canon(name);
        int bucket = arr_has(defs, package) ? 0 : (arr_has(drop, name) ? 1 : 2);
        json_append(buckets[bucket], json_string(bucket == 0 ? package : name));
    }
}

char *pmcli_split_without(const char *names_json, const char *default_names_json,
                          const char *droppable_json) {
    json_t *names = parse_arr(names_json);
    json_t *defs = parse_arr(default_names_json);
    json_t *drop = parse_arr(droppable_json);
    json_t *b[3];
    split_without(names, defs, drop, b);
    json_t *o = json_object();
    json_set(o, "defaults", b[0]);
    json_set(o, "extras", b[1]);
    json_set(o, "unknown", b[2]);
    char *out = json_dumps(o, 0);
    json_free(o); json_free(names); json_free(defs); json_free(drop);
    return out;
}

char *pmcli_install_flag_error(const char *cross_target, int n_names,
                               const char *extras_json, int tools_only,
                               int trust_recorded, int test_env_given,
                               const char *without_json,
                               const char *default_names_json,
                               const char *droppable_json) {
    json_t *extras = parse_arr(extras_json);
    json_t *without = parse_arr(without_json);
    json_t *defs = parse_arr(default_names_json);
    json_t *drop = parse_arr(droppable_json);
    int has_extras = arr_len(extras) > 0;
    const char *ct = (cross_target && *cross_target) ? cross_target : NULL;
    int has_without = arr_len(without) > 0;
    char buf[8192];
    buf[0] = '\0';

    if (ct) {
        int known = 0;
        for (int i = 0; i < PMCLI_NTARGETS; i++) if (!strcmp(PMCLI_ALL_TARGETS[i], ct)) { known = 1; break; }
        if (!known) {
            char *repr = py_repr_str(ct);
            char *known_csv = NULL;
            json_t *all = json_array();
            for (int i = 0; i < PMCLI_NTARGETS; i++) json_append(all, json_string(PMCLI_ALL_TARGETS[i]));
            known_csv = join_csv(all);
            snprintf(buf, sizeof(buf), "unknown target %s; known: %s",
                     repr ? repr : "''", known_csv ? known_csv : "");
            free(repr); free(known_csv); json_free(all);
            goto done;
        }
        if (!n_names) {
            snprintf(buf, sizeof(buf), "--target requires explicit package names");
            goto done;
        }
    }
    if (test_env_given && (has_extras || ct || n_names || tools_only)) {
        snprintf(buf, sizeof(buf), "--test-environment builds beside the default closure; "
                 "it does not take names, --extra, --target, or --tools-only");
        goto done;
    }
    if (tools_only && (has_extras || ct || n_names)) {
        snprintf(buf, sizeof(buf), "--tools-only installs the tool closure and then stops; "
                 "it does not take names, --extra, or --target");
        goto done;
    }
    if (trust_recorded && (has_extras || ct || n_names)) {
        snprintf(buf, sizeof(buf), "--trust-recorded installs the default closure and then stops; "
                 "it does not take names, --extra, or --target");
        goto done;
    }
    if (has_extras && ct) {
        snprintf(buf, sizeof(buf), "--extra syncs this install's venv and cannot combine with --target");
        goto done;
    }
    if (has_without && (has_extras || ct || n_names)) {
        snprintf(buf, sizeof(buf), "--without shapes the default closure; "
                 "it does not take names, --extra, or --target");
        goto done;
    }
    if (has_without) {
        json_t *b[3];
        split_without(without, defs, drop, b);
        if (arr_len(b[2]) > 0) {
            char *dcsv = join_csv(defs);
            char *ecsv = join_csv(drop);
            char *ucsv = join_csv(b[2]);
            snprintf(buf, sizeof(buf),
                     "--without accepts only optional default packages (%s) or extras outside "
                     "[all] (%s); not %s",
                     dcsv ? dcsv : "", ecsv ? ecsv : "", ucsv ? ucsv : "");
            free(dcsv); free(ecsv); free(ucsv);
            for (int k = 0; k < 3; k++) json_free(b[k]);
            goto done;
        }
        for (int k = 0; k < 3; k++) json_free(b[k]);
    }

done:
    json_free(extras); json_free(without); json_free(defs); json_free(drop);
    return buf[0] ? strdup(buf) : NULL;
}
