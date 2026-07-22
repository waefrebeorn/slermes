/*
 * port_fallback_config.c — pure helpers ported from
 * hermes_cli/fallback_config.py. Self-contained; uses libjson.
 *
 *   - _normalized_base_url -> fallback_config_normalize_base_url
 *   - _iter_fallback_entries -> fallback_config_iter_entries
 *   - _entry_identity       -> fallback_config_entry_identity
 *   - get_fallback_chain    -> fallback_config_get_chain
 */

#include "fallback_config_helpers.h"
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef struct json_t json_t;

/* PoP: _normalized_base_url @ hermes_cli/fallback_config.py:_normalized_base_url */
/* Strip whitespace, then trailing '/'. Non-string -> "". Caller frees. */
char *fallback_config_normalize_base_url(const char *value)
{
    if (!value) return strdup("");
    /* strip leading/trailing whitespace */
    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
    size_t n = strlen(value);
    while (n > 0 && (value[n-1] == ' ' || value[n-1] == '\t' || value[n-1] == '\r' || value[n-1] == '\n'))
        n--;
    char *s = malloc(n + 1);
    memcpy(s, value, n);
    s[n] = '\0';
    /* rstrip '/' */
    while (n > 0 && s[n-1] == '/') s[--n] = '\0';
    return s;
}

static void lower_copy(const char *src, char *dst, size_t sz)
{
    size_t i = 0;
    if (src) {
        for (; src[i] && i + 1 < sz; i++)
            dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

static char *entry_str(const json_t *entry, const char *key)
{
    const json_t *v = json_obj_get(entry, key);
    if (v && v->type == JSON_STRING) return strdup(v->str_val);
    if (v && v->type == JSON_NUMBER) {
        char b[32]; snprintf(b, sizeof(b), "%g", v->num_val); return strdup(b);
    }
    return strdup("");
}

/* PoP: _iter_fallback_entries @ hermes_cli/fallback_config.py:_iter_fallback_entries */
/* raw is a dict or list of dicts; returns normalized entries (fresh copies with
 * stripped provider/model, optional base_url). Provider or model empty -> skipped.
 * Caller frees via fallback_config_free_entries. */
fallback_entry_t *fallback_config_iter_entries(const json_t *raw, int *out_count)
{
    *out_count = 0;
    if (!raw) return NULL;

    const json_t *candidates;
    int is_single = (raw->type == JSON_OBJECT);
    if (is_single) candidates = raw;       /* treated as a 1-list */
    else if (raw->type == JSON_ARRAY) candidates = raw;
    else return NULL;

    size_t cap = is_single ? 1 : raw->c.count;
    fallback_entry_t *out = calloc(cap ? cap : 1, sizeof(fallback_entry_t));
    int cnt = 0;

    size_t n = is_single ? 1 : raw->c.count;
    for (size_t i = 0; i < n; i++) {
        const json_t *entry = is_single ? candidates : json_get(candidates, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        char *prov = entry_str(entry, "provider");
        char *model = entry_str(entry, "model");
        /* strip leading + trailing whitespace (mirrors Python str.strip()) */
        char *p = prov;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        size_t plen = strlen(p);
        while (plen > 0 && (p[plen - 1] == ' ' || p[plen - 1] == '\t' ||
               p[plen - 1] == '\r' || p[plen - 1] == '\n'))
            p[--plen] = '\0';
        char *mp = model;
        while (*mp == ' ' || *mp == '\t' || *mp == '\r' || *mp == '\n') mp++;
        size_t mlen = strlen(mp);
        while (mlen > 0 && (mp[mlen - 1] == ' ' || mp[mlen - 1] == '\t' ||
               mp[mlen - 1] == '\r' || mp[mlen - 1] == '\n'))
            mp[--mlen] = '\0';
        if (!*p || !*mp) { free(prov); free(model); continue; }

        out[cnt].provider = strdup(p);
        out[cnt].model = strdup(mp);
        const json_t *bu = json_obj_get(entry, "base_url");
        if (bu && bu->type == JSON_STRING && *bu->str_val) {
            char *nb = fallback_config_normalize_base_url(bu->str_val);
            out[cnt].base_url = nb;
        } else {
            out[cnt].base_url = strdup("");
        }
        free(prov); free(model);
        cnt++;
    }
    *out_count = cnt;
    return out;
}

/* PoP: _entry_identity @ hermes_cli/fallback_config.py:_entry_identity */
/* Lowercased (provider, model, base_url) triple. */
/* PoP: fallback_config_entry_identity @ hermes_cli/fallback_config.py:_entry_identity */
void fallback_config_entry_identity(const fallback_entry_t *entry,
                                    char *prov, char *model, char *base, size_t sz)
{
    lower_copy(entry ? entry->provider : "", prov, sz);
    lower_copy(entry ? entry->model : "", model, sz);
    lower_copy(entry ? entry->base_url : "", base, sz);
}

/* PoP: get_fallback_chain @ hermes_cli/fallback_config.py:get_fallback_chain */
/* Merge fallback_providers then fallback_model; drop duplicates by identity.
 * Returns fresh entries. Caller frees via fallback_config_free_entries. */
fallback_entry_t *fallback_config_get_chain(const json_t *config, int *out_count)
{
    *out_count = 0;
    if (!config || config->type != JSON_OBJECT) return NULL;

    fallback_entry_t *out = calloc(8, sizeof(fallback_entry_t));
    int cap = 8, cnt = 0;

    for (int ki = 0; ki < 2; ki++) {
        const char *key = (ki == 0) ? "fallback_providers" : "fallback_model";
        const json_t *raw = json_obj_get(config, key);
        if (!raw) continue;
        int ec = 0;
        fallback_entry_t *ents = fallback_config_iter_entries(raw, &ec);
        for (int i = 0; i < ec; i++) {
            char p[64], m[64], b[64];
            fallback_config_entry_identity(&ents[i], p, m, b, sizeof(p));
            int dup = 0;
            for (int j = 0; j < cnt; j++) {
                char sp[64], sm[64], sb[64];
                fallback_config_entry_identity(&out[j], sp, sm, sb, sizeof(sp));
                if (strcmp(sp, p) == 0 && strcmp(sm, m) == 0 && strcmp(sb, b) == 0) { dup = 1; break; }
            }
            if (dup) { free(ents[i].provider); free(ents[i].model); free(ents[i].base_url); continue; }
            if (cnt >= cap) {
                cap *= 2;
                out = realloc(out, (size_t)cap * sizeof(fallback_entry_t));
            }
            out[cnt].provider = ents[i].provider;
            out[cnt].model = ents[i].model;
            out[cnt].base_url = ents[i].base_url;
            cnt++;
        }
        free(ents);
    }
    *out_count = cnt;
    return out;
}

void fallback_config_free_entries(fallback_entry_t *entries, int count)
{
    if (!entries) return;
    for (int i = 0; i < count; i++) {
        free(entries[i].provider);
        free(entries[i].model);
        free(entries[i].base_url);
    }
    free(entries);
}
