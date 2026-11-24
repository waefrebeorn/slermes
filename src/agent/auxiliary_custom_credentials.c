/*
 * auxiliary_custom_credentials.c - C11 port of upstream
 * agent/auxiliary_custom_credentials.py (gate w25_auxiliaryc_9372f1).
 *
 * PoP: w25_auxiliaryc_9372f1_normalize_api_key @ agent/auxiliary_custom_credentials.py:_normalize_api_key
 * PoP: w25_auxiliaryc_9372f1_identity @ agent/auxiliary_custom_credentials.py:declared_custom_credential_identity
 * PoP: w25_auxiliaryc_9372f1_fast_provider @ agent/auxiliary_custom_credentials.py:fast_catalog_provider
 * PoP: w25_auxiliaryc_9372f1_declares_credential @ agent/custom_entry_pool.py:entry_declares_credential
 * PoP: w25_auxiliaryc_9372f1_constants @ agent/custom_entry_pool.py:_DECLARED_CREDENTIAL_KEYS
 *
 * The config tree, the profile secret scope and the key_cmd mint are seams: the
 * lookup that finds the owning entry, the env read behind a ``key_env`` and the
 * helper token all arrive as fixed inputs.  Ported here is the precedence the
 * module exists for -- which key wins, what a cache may key on, and which
 * catalog a ``custom`` runtime may read.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_auxiliaryc_9372f1.h"

/* _DECLARED_CREDENTIAL_KEYS */
static const char *const ACC_DECLARED_KEYS[W25_AUXILIARYC_9372F1_DECLARED_KEYS] = {
    "api_key", "key_env", "api_key_env", "key_cmd",
};

static char *acc_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int acc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(v). */
static char *acc_pystr(const json_t *v) {
    char buf[64];
    if (!v) return acc_dup("None");
    switch (v->type) {
    case JSON_NULL:
        return acc_dup("None");
    case JSON_BOOL:
        return acc_dup(v->bool_val ? "True" : "False");
    case JSON_STRING:
        return acc_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%g", d);
        return acc_dup(buf);
    }
    default:
        return acc_dup("");
    }
}

/* str(v or "").strip(). */
static char *acc_clean(const json_t *v) {
    char *raw;
    char *out;
    const char *b;
    const char *e;
    size_t n;
    if (!acc_truthy(v)) return acc_dup("");
    raw = acc_pystr(v);
    b = raw;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (out) {
        memcpy(out, b, n);
        out[n] = '\0';
    }
    free(raw);
    return out;
}

/* entry_declares_credential(entry): a provenance verdict first, else any
 * declared key non-blank. */
static int acc_declares(const json_t *entry) {
    size_t i;
    if (!entry || entry->type != JSON_OBJECT) return 0;
    if (json_obj_get(entry, "declares_credential"))
        return acc_truthy(json_obj_get(entry, "declares_credential"));
    for (i = 0; i < W25_AUXILIARYC_9372F1_DECLARED_KEYS; i++) {
        char *s = acc_clean(json_obj_get(entry, ACC_DECLARED_KEYS[i]));
        int hit = s && s[0];
        free(s);
        if (hit) return 1;
    }
    return 0;
}

json_t *w25_auxiliaryc_9372f1_declares_credential(const json_t *entry) {
    return json_bool(acc_declares(entry) ? true : false);
}

json_t *w25_auxiliaryc_9372f1_normalize_api_key(const json_t *raw) {
    json_t *out = json_object();
    int is_callable = raw && raw->type == JSON_OBJECT && acc_truthy(json_obj_get(raw, "__callable__"));
    int is_str = raw && raw->type == JSON_STRING;

    if (is_callable && !is_str) {
        json_set(out, "callable", json_bool(true));
        json_set(out, "value", json_null());
        return out;
    }
    json_set(out, "callable", json_bool(false));
    if (is_str) {
        char *s = acc_clean(raw);
        json_set(out, "value", json_string(s ? s : ""));
        free(s);
    } else {
        json_set(out, "value", json_string(""));
    }
    return out;
}

json_t *w25_auxiliaryc_9372f1_identity(const json_t *request) {
    const json_t *entry = json_obj_get(request, "entry");
    int found = acc_truthy(json_obj_get(request, "found"));
    json_t *out = json_object();
    json_t *parts = json_array();

    if (!found || !acc_declares(entry)) {
        json_set(out, "found", json_bool(false));
        json_set(out, "parts", parts);
        return out;
    }
    {
        char *key_env = acc_clean(json_obj_get(entry, "key_env"));
        char *api_key_env = acc_clean(json_obj_get(entry, "api_key_env"));
        char *api_key = acc_clean(json_obj_get(entry, "api_key"));
        char *key_cmd = acc_clean(json_obj_get(entry, "key_cmd"));
        char *cmd = (char *)malloc(strlen(key_cmd ? key_cmd : "") + 5);
        const json_t *scoped = json_obj_get(request, "scoped_key_env");

        if (!key_env[0] && api_key_env[0]) {
            free(key_env);
            key_env = acc_dup(api_key_env);
        }
        json_append(parts, json_string(api_key));
        json_append(parts, json_string(key_env[0] ? (scoped && scoped->type == JSON_STRING
                                                    ? scoped->str_val : "") : ""));
        if (cmd) {
            snprintf(cmd, strlen(key_cmd ? key_cmd : "") + 5, "cmd:%s", key_cmd ? key_cmd : "");
            json_append(parts, json_string(cmd));
            free(cmd);
        } else {
            json_append(parts, json_string("cmd:"));
        }
        free(key_env);
        free(api_key_env);
        free(api_key);
        free(key_cmd);
    }
    json_set(out, "found", json_bool(true));
    json_set(out, "parts", parts);
    return out;
}

json_t *w25_auxiliaryc_9372f1_fast_provider(const json_t *request) {
    const json_t *runtime = json_obj_get(request, "runtime");
    const json_t *main_provider = json_obj_get(request, "main_provider");
    const json_t *entry = json_obj_get(request, "entry");
    int owns = acc_truthy(json_obj_get(request, "owns"));
    char *requested = acc_clean(json_obj_get(runtime, "requested_provider"));
    json_t *out;

    /* main_provider != "custom" or not requested or requested == "custom" */
    if (!(main_provider && main_provider->type == JSON_STRING && main_provider->str_val &&
          !strcmp(main_provider->str_val, "custom")) ||
        !requested[0] || !strcmp(requested, "custom")) {
        out = main_provider ? json_copy(main_provider) : json_null();
        free(requested);
        return out;
    }
    {
        char *key = acc_dup("");
        if (owns && entry) {
            const json_t *pk = json_obj_get(entry, "provider_key");
            free(key);
            /* str(entry.get("provider_key") or "") -- no strip in the live code. */
            key = acc_truthy(pk) ? acc_pystr(pk) : acc_dup("");
        }
        if (key[0]) {
            const json_t *known = json_obj_get(request, "known");
            int is_known = 0;
            size_t i;
            if (known && known->type == JSON_ARRAY) {
                for (i = 0; i < json_len(known); i++) {
                    const json_t *k = json_get(known, i);
                    if (k && k->type == JSON_STRING && k->str_val && !strcmp(k->str_val, key)) {
                        is_known = 1;
                        break;
                    }
                }
            }
            if (!is_known) {
                out = json_string(key);
                free(key);
                free(requested);
                return out;
            }
        }
        free(key);
    }
    out = json_copy(main_provider);
    free(requested);
    return out;
}

json_t *w25_auxiliaryc_9372f1_constants(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < W25_AUXILIARYC_9372F1_DECLARED_KEYS; i++)
        json_append(out, json_string(ACC_DECLARED_KEYS[i]));
    return out;
}
