/*
 * model_setup_flows_bedrock.c - real C11 port of the pure Bedrock policy helpers in upstream
 * Hermes hermes_cli/model_setup_flows_bedrock.py. The interactive wizard flows (prompting,
 * config writes, boto3 discovery) stay Python-side; every module-level decision function is here.
 * PoP: w19_modelsetup_405db7_region_geo_prefix @ hermes_cli/model_setup_flows_bedrock.py:bedrock_region_geo_prefix
 * PoP: w19_modelsetup_405db7_model_routable @ hermes_cli/model_setup_flows_bedrock.py:bedrock_model_routable_from_region
 * PoP: w19_modelsetup_405db7_text_model_ids @ hermes_cli/model_setup_flows_bedrock.py:_bedrock_text_model_ids
 */
#include "noc_w19_modelsetup_405db7.h"
#include <stdlib.h>
#include <string.h>

/* AWS cross-region inference-profile geo prefixes, in the upstream tuple order. */
static const char *const GEO_PREFIXES[] = {
    "us.", "eu.", "ap.", "apac.", "jp.", "ca.", "sa.", "me.", "af.", NULL
};

/* _BEDROCK_PROFILE_PREFIXES = BEDROCK_GEO_PREFIXES + ("global.",) */
static const char *const PROFILE_PREFIXES[] = {
    "us.", "eu.", "ap.", "apac.", "jp.", "ca.", "sa.", "me.", "af.", "global.", NULL
};

static const char *const EXCLUDE_PREFIXES[] = {
    "stability.", "cohere.embed", "twelvelabs.", "us.stability.", "us.cohere.embed",
    "us.twelvelabs.", "global.cohere.embed", "global.twelvelabs.", NULL
};

static const char *const EXCLUDE_SUBSTRINGS[] = {
    "safeguard", "voxtral", "palmyra-vision", NULL
};

static const char *const RECOMMENDED_BASES[] = {
    "anthropic.claude-sonnet-4-6", "anthropic.claude-opus-4-6", "anthropic.claude-haiku-4-5",
    "amazon.nova-pro", "amazon.nova-lite", "amazon.nova-micro", "deepseek.v3",
    "meta.llama4-maverick", "meta.llama4-scout", NULL
};

/* static dupstr - no strdup (not C11) anywhere in this port. */
static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/* Python str.lower() over ASCII, which is all AWS region ids and model ids use. */
static char *lower_dup(const char *s) {
    char *p = dupstr(s ? s : "");
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 'A' && c <= 'Z') p[i] = (char)(c - 'A' + 'a');
    }
    return p;
}

static int starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* next((p for p in PROFILE_PREFIXES if mid.startswith(p)), None) */
static const char *matched_profile_prefix(const char *mid) {
    int i;
    for (i = 0; PROFILE_PREFIXES[i]; i++)
        if (starts_with(mid, PROFILE_PREFIXES[i])) return PROFILE_PREFIXES[i];
    return NULL;
}

/* _base_id(mid): strip the first matching inference-profile prefix. */
static const char *base_id(const char *mid) {
    const char *p = matched_profile_prefix(mid);
    return p ? mid + strlen(p) : mid;
}

const char *w19_modelsetup_405db7_region_geo_prefix(const char *region_name) {
    char *r = lower_dup(region_name);
    const char *geo = "";
    if (!r) return "";
    if (starts_with(r, "us-") || starts_with(r, "us_gov")) geo = "us.";
    else if (starts_with(r, "eu-")) geo = "eu.";
    else if (starts_with(r, "ap-")) geo = "ap.";
    else if (starts_with(r, "ca-")) geo = "ca.";
    else if (starts_with(r, "sa-")) geo = "sa.";
    else if (starts_with(r, "me-")) geo = "me.";
    else if (starts_with(r, "af-")) geo = "af.";
    free(r);
    return geo; /* static literal, never an interior pointer */
}

int w19_modelsetup_405db7_model_routable(const char *model_id, const char *region_name) {
    char *mid = lower_dup(model_id);
    const char *matched = NULL;
    const char *geo;
    int ok, i;
    if (!mid) return 1;
    for (i = 0; GEO_PREFIXES[i]; i++)
        if (starts_with(mid, GEO_PREFIXES[i])) { matched = GEO_PREFIXES[i]; break; }
    if (!matched || starts_with(mid, "global.")) { free(mid); return 1; }
    geo = w19_modelsetup_405db7_region_geo_prefix(region_name);
    if (!geo[0]) { free(mid); return 1; }
    if (!strcmp(geo, "ap.")) {
        /* Asia-Pacific regions can carry ap./apac./jp. profile spellings. */
        ok = (!strcmp(matched, "ap.") || !strcmp(matched, "apac.") || !strcmp(matched, "jp."));
    } else {
        ok = (strcmp(matched, geo) == 0);
    }
    free(mid);
    return ok;
}

/* One surviving discovery record. `id`/`base` point into the caller's JSON tree. */
typedef struct {
    const char *id;
    const char *base;
    int group;      /* 0 recommended, 1 global.*, 2 the rest */
    int rec_index;  /* position in RECOMMENDED_BASES when group == 0 */
    int is_global;
} model_item;

/* Python tuple order: (group, rec_index, global flag, id). */
static int model_item_cmp(const void *a, const void *b) {
    const model_item *x = (const model_item *)a;
    const model_item *y = (const model_item *)b;
    if (x->group != y->group) return x->group < y->group ? -1 : 1;
    if (x->rec_index != y->rec_index) return x->rec_index < y->rec_index ? -1 : 1;
    if (x->is_global != y->is_global) return x->is_global < y->is_global ? -1 : 1;
    return strcmp(x->id, y->id);
}

static int excluded(const char *id, const char *lower_id) {
    int i;
    for (i = 0; EXCLUDE_PREFIXES[i]; i++)
        if (starts_with(id, EXCLUDE_PREFIXES[i])) return 1;
    for (i = 0; EXCLUDE_SUBSTRINGS[i]; i++)
        if (lower_id && strstr(lower_id, EXCLUDE_SUBSTRINGS[i])) return 1;
    return 0;
}

json_t *w19_modelsetup_405db7_text_model_ids(const json_t *live_models, const char *region) {
    json_t *out = json_array();
    model_item *filtered = NULL;
    size_t n_filtered = 0, i;
    const char **profile_bases = NULL;
    size_t n_profile_bases = 0;
    size_t cap = 0;
    model_item *keep;
    size_t n_keep = 0;

    if (!out) return json_array();

    /* filtered = [m for m in live_models if routable and not excluded] */
    if (live_models && live_models->type == JSON_ARRAY) {
        for (i = 0; i < json_len(live_models); i++) {
            json_t *m = json_get(live_models, i);
            json_t *idn = (m && m->type == JSON_OBJECT) ? json_obj_get(m, "id") : NULL;
            const char *id = (idn && idn->type == JSON_STRING && idn->str_val) ? idn->str_val : "";
            char *lower = lower_dup(id);
            int keep_it = !excluded(id, lower);
            free(lower);
            if (keep_it && !w19_modelsetup_405db7_model_routable(id, region)) keep_it = 0;
            if (!keep_it) continue;
            if (n_filtered == cap) {
                size_t ncap = cap ? cap * 2 : 16;
                model_item *grown = (model_item *)realloc(filtered, ncap * sizeof(*grown));
                if (!grown) break;
                filtered = grown;
                cap = ncap;
            }
            filtered[n_filtered].id = id;
            filtered[n_filtered].base = base_id(id);
            filtered[n_filtered].group = 2;
            filtered[n_filtered].rec_index = 0;
            filtered[n_filtered].is_global = 0;
            n_filtered++;
        }
    }

    /* profile_base_ids = {_base_id(m["id"]) for m in filtered if profile-prefixed} */
    if (n_filtered) profile_bases = (const char **)calloc(n_filtered, sizeof(*profile_bases));
    for (i = 0; profile_bases && i < n_filtered; i++)
        if (matched_profile_prefix(filtered[i].id)) profile_bases[n_profile_bases++] = filtered[i].base;

    /* Deduplicate: prefer inference profiles over bare foundation-model ids. */
    keep = (model_item *)calloc(n_filtered ? n_filtered : 1, sizeof(*keep));
    if (!keep) {
        free(profile_bases);
        free(filtered);
        return out;
    }
    for (i = 0; i < n_filtered; i++) {
        int has_profile = matched_profile_prefix(filtered[i].id) != NULL;
        int seen = 0;
        size_t j;
        if (!has_profile) {
            for (j = 0; j < n_profile_bases; j++)
                if (!strcmp(profile_bases[j], filtered[i].base)) { seen = 1; break; }
        }
        if (has_profile || !seen) keep[n_keep++] = filtered[i];
    }

    /* _sort_key */
    for (i = 0; i < n_keep; i++) {
        int r, hit = 0;
        for (r = 0; RECOMMENDED_BASES[r]; r++) {
            if (starts_with(keep[i].base, RECOMMENDED_BASES[r])) {
                keep[i].group = 0;
                keep[i].rec_index = r;
                keep[i].is_global = starts_with(keep[i].id, "global.") ? 1 : 0;
                hit = 1;
                break;
            }
        }
        if (!hit) {
            keep[i].group = starts_with(keep[i].id, "global.") ? 1 : 2;
            keep[i].rec_index = 0;
            keep[i].is_global = 0;
        }
    }
    if (n_keep > 1) qsort(keep, n_keep, sizeof(*keep), model_item_cmp);
    for (i = 0; i < n_keep; i++) json_append(out, json_string(keep[i].id));

    free(keep);
    free(profile_bases);
    free(filtered);
    return out;
}
