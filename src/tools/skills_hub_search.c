/*
 * skills_hub_search.c — C home for tools/skills_hub_search.py.
 * PoP: skillsearch_provider_filter @ tools/skills_hub_search.py:_provider_filter_of
 * PoP: skillsearch_select_active @ tools/skills_hub_search.py:_select_active_sources
 * PoP: skillsearch_index_miss_fallback @ tools/skills_hub_search.py:_index_miss_fallback_sources
 *
 * Deterministic source-routing logic of the hub search: provider-filter normalization,
 * active-source selection (index skip / official injection) and index-miss fallback.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* tools/skills_hub_github.py:GITHUB_TAP_PROVIDERS values, lowercased. */
static const char *const PROVIDERS[] = {
    "openai", "anthropic", "huggingface", "nvidia", "voltagent", "gstack",
    "minimax", "weights & biases", "qdrant", "pinecone", "notion", "airtable", "shopify",
};
static const char *const API_IDS[] = {"github", "skills-sh", "clawhub", "lobehub", "well-known"};
static const char *const FALLBACK_IDS[] = {"skills-sh", "clawhub", "lobehub", "well-known"};

static int in_list(const char *const *list, size_t n, const char *s) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], s)) return 1;
    return 0;
}

/* Python: value = source_filter.strip().lower(); return value if value in PROVIDERS else "" */
static char *provider_filter(const char *sf) {
    size_t n = sf ? strlen(sf) : 0;
    char *out = (char *)calloc(n + 1, 1);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)sf[a])) a++;
    while (b > a && isspace((unsigned char)sf[b - 1])) b--;
    size_t j = 0;
    for (size_t i = a; i < b; i++) out[j++] = (char)tolower((unsigned char)sf[i]);
    out[j] = 0;
    if (!in_list(PROVIDERS, sizeof(PROVIDERS) / sizeof(PROVIDERS[0]), out)) { out[0] = 0; }
    return out;
}

json_t *skillsearch_provider_filter(const char *source_filter) {
    char *v = provider_filter(source_filter ? source_filter : "");
    json_t *r = json_string(v);
    free(v);
    return r;
}

/* str.strip() truth for the whitespace set Python treats as whitespace. */
static int blank(const char *s) {
    if (!s) return 1;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!isspace(*p)) return 0;
    return 1;
}

json_t *skillsearch_select_active(const json_t *sources, const char *source_filter) {
    json_t *active = json_array();
    if (!sources || sources->type != JSON_ARRAY) return active;
    char *pf = provider_filter(source_filter ? source_filter : "");
    const char *effective = pf[0] ? "all" : (source_filter ? source_filter : "");
    int effective_all = !strcmp(effective, "all");

    int index_available = 0;
    if (effective_all) {
        for (size_t i = 0; i < json_len(sources); i++) {
            json_t *s = json_get(sources, i);
            const char *sid = json_get_str(s, "id", "");
            if (!strcmp(sid, "hermes-index") && json_get_bool(s, "available", false)) { index_available = 1; break; }
        }
    }
    for (size_t i = 0; i < json_len(sources); i++) {
        json_t *s = json_get(sources, i);
        const char *sid = json_get_str(s, "id", "");
        if (!effective_all && strcmp(sid, effective) && strcmp(sid, "official")) continue;
        if (index_available && in_list(API_IDS, sizeof(API_IDS) / sizeof(API_IDS[0]), sid)) continue;
        json_append(active, json_string(sid));
    }
    free(pf);
    return active;
}

json_t *skillsearch_index_miss_fallback(const json_t *all_ids, const json_t *active_ids,
                                        const char *query, const json_t *source_counts,
                                        const char *provider_filter_str) {
    json_t *out = json_array();
    if (blank(query) || (provider_filter_str && *provider_filter_str)) return out;
    if (!active_ids || active_ids->type != JSON_ARRAY) return out;
    int has_index = 0;
    for (size_t i = 0; i < json_len(active_ids); i++) {
        json_t *v = json_get(active_ids, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, "hermes-index")) { has_index = 1; break; }
    }
    if (!has_index) return out;
    json_t *cnt = source_counts ? json_obj_get(source_counts, "hermes-index") : NULL;
    if (!cnt || cnt->type != JSON_NUMBER || cnt->num_val != 0.0) return out;
    if (!all_ids || all_ids->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(all_ids); i++) {
        json_t *v = json_get(all_ids, i);
        if (!v || v->type != JSON_STRING) continue;
        const char *sid = v->str_val;
        if (!in_list(FALLBACK_IDS, sizeof(FALLBACK_IDS) / sizeof(FALLBACK_IDS[0]), sid)) continue;
        int in_active = 0;
        for (size_t j = 0; j < json_len(active_ids); j++) {
            json_t *a = json_get(active_ids, j);
            if (a && a->type == JSON_STRING && !strcmp(a->str_val, sid)) { in_active = 1; break; }
        }
        if (!in_active) json_append(out, json_copy(v));
    }
    return out;
}
