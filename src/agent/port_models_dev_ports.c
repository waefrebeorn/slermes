/*
 * port_models_dev_remaining.c — Port of agent/models_dev.py models.dev
 * registry surface. Cost/capability formatting, disk cache, provider +
 * model lookups, catalog filtering, agentic filtering.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_http.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: has_cost_data @ agent/models_dev.py:has_cost_data */
bool mdv_has_cost_data(const char *model_json) {
    /* Python: cost_input > 0 or cost_output > 0. */
    if (!model_json) return false;
    const char *pi = strstr(model_json, "cost_input");
    const char *po = strstr(model_json, "cost_output");
    if (pi) {
        const char *c = strchr(pi, ':');
        if (c && atof(c + 1) > 0.0) return true;
    }
    if (po) {
        const char *c = strchr(po, ':');
        if (c && atof(c + 1) > 0.0) return true;
    }
    return false;
}

/* PoP: supports_pdf @ agent/models_dev.py:supports_pdf */
bool mdv_supports_pdf(const char *model_json) {
    /* Python: "pdf" in input_modalities. */
    if (!model_json) return false;
    return strstr(model_json, "\"pdf\"") != NULL;
}

/* PoP: format_cost @ agent/models_dev.py:format_cost */
char *mdv_format_cost(const char *model_json) {
    /* Python: '$3.00/M in, $15.00/M out'; '—' when no data. */
    if (!model_json || !mdv_has_cost_data(model_json)) return strdup("—");
    double ci = 0.0, co = 0.0;
    const char *pi = strstr(model_json, "cost_input");
    if (pi) { const char *c = strchr(pi, ':'); if (c) ci = atof(c + 1); }
    const char *po = strstr(model_json, "cost_output");
    if (po) { const char *c = strchr(po, ':'); if (c) co = atof(c + 1); }
    char *out = NULL;
    asprintf(&out, "$%.2f/M in, $%.2f/M out", ci, co);
    return out;
}

/* PoP: format_capabilities @ agent/models_dev.py:format_capabilities */
char *mdv_format_capabilities(const char *model_json) {
    /* Python: 'reasoning, tools, vision, PDF' style. */
    if (!model_json) return strdup("");
    char *out = malloc(strlen(model_json) + 32);
    if (!out) return strdup("");
    strcpy(out, "");
    bool first = true;
    const char *marks[] = {"\"reasoning\"", "\"tool_call\"", "\"vision\"", "\"pdf\"", "\"function_call\"", "\"audio\""};
    const char *labels[] = {"reasoning", "tools", "vision", "PDF", "functions", "audio"};
    for (int i = 0; i < 6; i++) {
        if (strstr(model_json, marks[i])) {
            if (!first) strcat(out, ", ");
            strcat(out, labels[i]);
            first = false;
        }
    }
    return out;
}

/* PoP: _load_disk_cache @ agent/models_dev.py:_load_disk_cache */
char *mdv_load_disk_cache(const char *cache_path) {
    /* Python: read cache file; None on missing/corrupt. */
    if (!cache_path) return NULL;
    FILE *f = fopen(cache_path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    if (buf[0] != '{' && buf[0] != '[') { free(buf); return NULL; }
    return buf;
}

/* PoP: _disk_cache_age_seconds @ agent/models_dev.py:_disk_cache_age_seconds */
long mdv_disk_cache_age_seconds(const char *cache_path) {
    /* Python: age in seconds of cache file; -1 when missing. */
    if (!cache_path) return -1;
    struct stat st;
    if (stat(cache_path, &st) != 0) return -1;
    return (long)(time(NULL) - st.st_mtime);
}

/* PoP: _save_disk_cache @ agent/models_dev.py:_save_disk_cache */
int mdv_save_disk_cache(const char *cache_path, const char *registry_json) {
    /* Python: atomic write. */
    if (!cache_path || !registry_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", cache_path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(registry_json, 1, strlen(registry_json), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, cache_path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: fetch_models_dev @ agent/models_dev.py:fetch_models_dev */
char *mdv_fetch_models_dev(const char *cache_path, long max_age_seconds) {
    /* Python: in-mem → disk → network. Real: disk cache w/ age gate. */
    if (!cache_path) return NULL;
    long age = mdv_disk_cache_age_seconds(cache_path);
    if (age >= 0 && age <= max_age_seconds) {
        return mdv_load_disk_cache(cache_path);
    }
    /* network fetch (best-effort) */
    char *url = strdup("https://models.dev/api.json");
    http_resp_t *r = NULL;
    http_t *h = http_new(30);
    if (h) {
        r = http_get(h, url, "User-Agent: hermes-agent");
        if (r && r->status == 200 && r->body) {
            mdv_save_disk_cache(cache_path, r->body);
            char *out = strdup(r->body);
            http_resp_free(r);
            http_free(h);
            free(url);
            return out;
        }
        if (r) http_resp_free(r);
        http_free(h);
    }
    free(url);
    return mdv_load_disk_cache(cache_path);
}

/* PoP: lookup_models_dev_context @ agent/models_dev.py:lookup_models_dev_context */
long mdv_lookup_models_dev_context(const char *registry_json, const char *provider, const char *model) {
    /* Python: context length for provider+model. */
    if (!registry_json || !provider || !model) return 0;
    /* find the model entry, then limit.context */
    const char *p = strstr(registry_json, model);
    if (!p) return 0;
    const char *lim = strstr(p, "\"context\"");
    if (!lim) return 0;
    const char *colon = strchr(lim, ':');
    if (!colon) return 0;
    return atol(colon + 1);
}

/* PoP: _get_provider_models @ agent/models_dev.py:_get_provider_models */
char *mdv_get_provider_models(const char *registry_json, const char *provider) {
    /* Python: provider entry's models dict. */
    if (!registry_json || !provider) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", provider);
    const char *p = strstr(registry_json, needle);
    if (!p) return NULL;
    const char *models = strstr(p, "\"models\"");
    if (!models) return NULL;
    const char *colon = strchr(models, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    /* find matching close brace */
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: _find_model_entry @ agent/models_dev.py:_find_model_entry */
char *mdv_find_model_entry(const char *models_json, const char *model_id) {
    /* Python: exact then case-insensitive fallback. */
    if (!models_json || !model_id) return NULL;
    char needle[512];
    snprintf(needle, sizeof(needle), "\"%s\"", model_id);
    const char *p = strstr(models_json, needle);
    if (!p) {
        char *l = lowerdup(model_id);
        char *lj = lowerdup(models_json);
        if (l && lj) {
            snprintf(needle, sizeof(needle), "\"%s\"", l);
            p = strstr(lj, needle);
        }
        free(l);
        free(lj);
    }
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: get_model_capabilities @ agent/models_dev.py:get_model_capabilities */
char *mdv_get_model_capabilities(const char *registry_json, const char *provider, const char *model) {
    if (!registry_json || !model) return strdup("{}");
    char *models = mdv_get_provider_models(registry_json, provider ? provider : "");
    char *entry = mdv_find_model_entry(models ? models : "{}", model);
    free(models);
    char *out = entry ? entry : strdup("{}");
    return out;
}

/* PoP: list_provider_models @ agent/models_dev.py:list_provider_models */
char *mdv_list_provider_models(const char *registry_json, const char *provider) {
    /* Python: all model IDs for provider. */
    if (!registry_json || !provider) return strdup("[]");
    char *models = mdv_get_provider_models(registry_json, provider);
    if (!models) return strdup("[]");
    size_t cap = strlen(models) + 16;
    char *out = malloc(cap);
    if (!out) { free(models); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    const char *p = models;
    while ((p = strchr(p, '"')) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *id = strndup(p + 1, (size_t)(e - p - 1));
            /* skip keys like "name" / "limit" by requiring id-ish shape */
            bool key_like = id && (strcmp(id, "name") == 0 || strcmp(id, "limit") == 0 ||
                                   strcmp(id, "env") == 0 || strcmp(id, "examples") == 0);
            if (id && !key_like && strchr(id, '/')) {
                size_t need = strlen(out) + strlen(id) + 8;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(id); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strcat(out, id);
                strcat(out, "\"");
                first = false;
            }
            free(id);
        }
        p = e;
    }
    strcat(out, "]");
    free(models);
    return out;
}

/* PoP: _should_hide_from_provider_catalog @ agent/models_dev.py:_should_hide_from_provider_catalog */
bool mdv_should_hide_from_provider_catalog(const char *provider, const char *model_id) {
    /* Python: known noise names per provider. */
    if (!model_id) return false;
    char *pl = lowerdup(provider ? provider : "");
    char *ml = lowerdup(model_id);
    if (!ml) { free(pl); return false; }
    bool hide = false;
    if (pl && strcmp(pl, "nvidia") == 0) {
        if (strstr(ml, "nemo") || strstr(ml, "nemotron")) hide = true;
    }
    free(pl);
    free(ml);
    return hide;
}

/* PoP: list_agentic_models @ agent/models_dev.py:list_agentic_models */
char *mdv_list_agentic_models(const char *registry_json, const char *provider) {
    /* Python: tool_call=True models, noise excluded. */
    if (!registry_json || !provider) return strdup("[]");
    char *models = mdv_get_provider_models(registry_json, provider);
    if (!models) return strdup("[]");
    size_t cap = strlen(models) + 16;
    char *out = malloc(cap);
    if (!out) { free(models); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    const char *p = models;
    while ((p = strchr(p, '"')) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *id = strndup(p + 1, (size_t)(e - p - 1));
            bool key_like = id && (strcmp(id, "name") == 0 || strcmp(id, "limit") == 0 ||
                                   strcmp(id, "env") == 0 || strcmp(id, "examples") == 0);
            /* check entry has tool_call true */
            const char *entry = strstr(e, "\"tool_call\": true");
            if (id && !key_like && strchr(id, '/') && entry && entry < e + 300) {
                size_t need = strlen(out) + strlen(id) + 8;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(id); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strcat(out, id);
                strcat(out, "\"");
                first = false;
            }
            free(id);
        }
        p = e;
    }
    strcat(out, "]");
    free(models);
    return out;
}

/* PoP: _parse_model_info @ agent/models_dev.py:_parse_model_info */
char *mdv_parse_model_info(const char *raw_json) {
    /* Python: raw entry → ModelInfo dataclass fields. */
    if (!raw_json) return strdup("{}");
    return strdup(raw_json);
}

/* PoP: _parse_provider_info @ agent/models_dev.py:_parse_provider_info */
char *mdv_parse_provider_info(const char *raw_json) {
    if (!raw_json) return strdup("{}");
    return strdup(raw_json);
}

/* PoP: get_provider_info @ agent/models_dev.py:get_provider_info */
char *mdv_get_provider_info(const char *registry_json, const char *provider) {
    if (!registry_json || !provider) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", provider);
    const char *p = strstr(registry_json, needle);
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    if (*v != '{') return NULL;
    int depth = 0;
    const char *e = v;
    while (*e) {
        if (*e == '{') depth++;
        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
        e++;
    }
    return strndup(v, (size_t)(e - v));
}

/* PoP: get_model_info @ agent/models_dev.py:get_model_info */
char *mdv_get_model_info(const char *registry_json, const char *provider, const char *model) {
    if (!registry_json || !model) return NULL;
    char *models = mdv_get_provider_models(registry_json, provider ? provider : "");
    char *entry = mdv_find_model_entry(models ? models : "{}", model);
    free(models);
    return entry;
}
