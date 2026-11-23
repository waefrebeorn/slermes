/*
 * toolset_distributions.c — faithful C11 port of upstream Hermes
 * toolset_distributions.py (gate w5_toolsetdist_3d9ebc).
 *
 * PoP: w5_toolsetdist_3d9ebc_get_distribution @ toolset_distributions.py:get_distribution
 * PoP: w5_toolsetdist_3d9ebc_validate_distribution @ toolset_distributions.py:validate_distribution
 * PoP: w5_toolsetdist_3d9ebc_entry_members @ toolset_distributions.py:_entry_members
 * PoP: w5_toolsetdist_3d9ebc_sample @ toolset_distributions.py:sample_toolsets_from_distribution
 *
 * The static DISTRIBUTIONS table and the sampling decision are ported verbatim.
 * random.random() and toolsets.validate_toolset() are fixture-driven seams so
 * the port stays deterministic and diffable.
 */
#include "noc_w5_toolsetdist_3d9ebc.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct w5t_kv { const char *key; double prob; };
struct w5t_dist { const char *name; const char *desc; const struct w5t_kv *items; size_t n; };

#define W5T_N(arr) (sizeof(arr)/sizeof((arr)[0]))

static const struct w5t_kv KV_DEFAULT[] = {
    {"web",100},{"vision",100},{"image_gen",100},{"terminal",100},{"file",100},{"browser",100}};
static const struct w5t_kv KV_IMAGE_GEN[] = {
    {"image_gen",90},{"vision",90},{"web",55},{"terminal",45}};
static const struct w5t_kv KV_RESEARCH[] = {
    {"web",90},{"browser",70},{"vision",50},{"terminal",10}};
static const struct w5t_kv KV_SCIENCE[] = {
    {"web",94},{"terminal",94},{"file",94},{"vision",65},{"browser",50},{"image_gen",15}};
static const struct w5t_kv KV_DEVELOPMENT[] = {
    {"terminal",80},{"file",80},{"web",30},{"vision",10}};
static const struct w5t_kv KV_SAFE[] = {
    {"web",80},{"browser",70},{"vision",60},{"image_gen",60}};
static const struct w5t_kv KV_BALANCED[] = {
    {"web",50},{"vision",50},{"image_gen",50},{"terminal",50},{"file",50},{"browser",50}};
static const struct w5t_kv KV_MINIMAL[] = { {"web",100} };
static const struct w5t_kv KV_TERMINAL_ONLY[] = { {"terminal",100},{"file",100} };
static const struct w5t_kv KV_TERMINAL_WEB[] = { {"terminal",100},{"file",100},{"web",100} };
static const struct w5t_kv KV_CREATIVE[] = { {"image_gen",90},{"vision",90},{"web",30} };
static const struct w5t_kv KV_REASONING[] = { {"web",90},{"file",60},{"terminal",20} };
static const struct w5t_kv KV_BROWSER_USE[] = { {"browser",100},{"web",80},{"vision",70} };
static const struct w5t_kv KV_BROWSER_ONLY[] = { {"browser",100} };
static const struct w5t_kv KV_BROWSER_TASKS[] = {
    {"browser+search",97},{"vision",12},{"terminal",15}};
static const struct w5t_kv KV_TERMINAL_TASKS[] = {
    {"terminal",97},{"file",97},{"web",97},{"browser",75},{"vision",50},{"image_gen",10}};
static const struct w5t_kv KV_MIXED_TASKS[] = {
    {"browser",92},{"terminal",92},{"file",92},{"web",35},{"vision",15},{"image_gen",15}};

static const struct w5t_dist DISTS[] = {
    {"default", "All available tools, all the time", KV_DEFAULT, W5T_N(KV_DEFAULT)},
    {"image_gen", "Heavy focus on image generation with vision and web support", KV_IMAGE_GEN, W5T_N(KV_IMAGE_GEN)},
    {"research", "Web research with vision analysis and reasoning", KV_RESEARCH, W5T_N(KV_RESEARCH)},
    {"science", "Scientific research with web, terminal, file, and browser capabilities", KV_SCIENCE, W5T_N(KV_SCIENCE)},
    {"development", "Terminal, file tools, and reasoning with occasional web lookup", KV_DEVELOPMENT, W5T_N(KV_DEVELOPMENT)},
    {"safe", "All tools except terminal for safety", KV_SAFE, W5T_N(KV_SAFE)},
    {"balanced", "Equal probability of all toolsets", KV_BALANCED, W5T_N(KV_BALANCED)},
    {"minimal", "Only web tools for basic research", KV_MINIMAL, W5T_N(KV_MINIMAL)},
    {"terminal_only", "Terminal and file tools for code execution tasks", KV_TERMINAL_ONLY, W5T_N(KV_TERMINAL_ONLY)},
    {"terminal_web", "Terminal and file tools with web search for documentation lookup", KV_TERMINAL_WEB, W5T_N(KV_TERMINAL_WEB)},
    {"creative", "Image generation and vision analysis focus", KV_CREATIVE, W5T_N(KV_CREATIVE)},
    {"reasoning", "Heavy research/reasoning distribution with minimal other tools", KV_REASONING, W5T_N(KV_REASONING)},
    {"browser_use", "Full browser-based web interaction with search, vision, and page control", KV_BROWSER_USE, W5T_N(KV_BROWSER_USE)},
    {"browser_only", "Only browser automation tools for pure web interaction tasks", KV_BROWSER_ONLY, W5T_N(KV_BROWSER_ONLY)},
    {"browser_tasks", "Browser-focused distribution with web_search for finding URLs (Google blocks direct browser searches)", KV_BROWSER_TASKS, W5T_N(KV_BROWSER_TASKS)},
    {"terminal_tasks", "Terminal-focused distribution with high terminal/file availability, occasional other tools", KV_TERMINAL_TASKS, W5T_N(KV_TERMINAL_TASKS)},
    {"mixed_tasks", "Mixed distribution with high browser, terminal, and file availability for complex tasks", KV_MIXED_TASKS, W5T_N(KV_MIXED_TASKS)},
};

static const struct w5t_dist *w5t_find(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(DISTS)/sizeof(DISTS[0]); i++)
        if (!strcmp(DISTS[i].name, name)) return &DISTS[i];
    return NULL;
}

struct json_t *w5_toolsetdist_3d9ebc_get_distribution(const char *name) {
    const struct w5t_dist *d = w5t_find(name);
    struct json_t *toolsets, *out;
    if (!d) return json_null();
    toolsets = json_object();
    for (size_t i = 0; i < d->n; i++) json_set(toolsets, d->items[i].key, json_number(d->items[i].prob));
    out = json_object();
    json_set(out, "description", json_string(d->desc));
    json_set(out, "toolsets", toolsets);
    return out;
}

struct json_t *w5_toolsetdist_3d9ebc_validate_distribution(const char *name) {
    return json_bool(w5t_find(name) ? 1 : 0);
}

/* Python str.strip() over ASCII whitespace (fixtures are ASCII). */
static char *w5t_strip(const char *s) {
    size_t n, a, b;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    a = 0; b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* [name.strip() for name in entry.split("+")] */
struct json_t *w5_toolsetdist_3d9ebc_entry_members(const char *entry) {
    struct json_t *out = json_array();
    const char *p;
    if (!entry) return out;
    p = entry;
    for (;;) {
        const char *plus = strchr(p, '+');
        size_t n = plus ? (size_t)(plus - p) : strlen(p);
        char *piece = (char *)malloc(n + 1);
        char *stripped;
        if (!piece) break;
        memcpy(piece, p, n);
        piece[n] = '\0';
        stripped = w5t_strip(piece);
        json_append(out, json_string(stripped ? stripped : ""));
        free(piece);
        free(stripped);
        if (!plus) break;
        p = plus + 1;
    }
    return out;
}

static int w5t_valid_name(const struct json_t *valid, const char *name) {
    if (!valid || valid->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(valid); i++) {
        json_t *v = json_get(valid, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

static int w5t_all_valid(const struct json_t *valid, const struct json_t *members) {
    for (size_t i = 0; i < json_len(members); i++) {
        json_t *m = json_get(members, i);
        if (!m || m->type != JSON_STRING || !w5t_valid_name(valid, m->str_val)) return 0;
    }
    return 1;
}

static double w5t_roll(const struct json_t *rolls, size_t idx) {
    if (rolls && rolls->type == JSON_ARRAY && idx < json_len(rolls)) {
        json_t *v = json_get(rolls, idx);
        if (v && v->type == JSON_NUMBER) return v->num_val;
    }
    return 0.0;
}

struct json_t *w5_toolsetdist_3d9ebc_sample(const char *name,
                                            const struct json_t *rolls,
                                            const struct json_t *valid) {
    const struct w5t_dist *d = w5t_find(name);
    struct json_t *selected;
    size_t roll_idx = 0;
    if (!d) return json_null();
    selected = json_array();
    for (size_t i = 0; i < d->n; i++) {
        struct json_t *members = w5_toolsetdist_3d9ebc_entry_members(d->items[i].key);
        if (!w5t_all_valid(valid, members)) {
            json_free(members);
            continue; /* invalid entries consume no random roll */
        }
        if (w5t_roll(rolls, roll_idx++) * 100.0 < d->items[i].prob) {
            for (size_t j = 0; j < json_len(members); j++)
                json_append(selected, json_copy(json_get(members, j)));
        }
        json_free(members);
    }
    if (json_len(selected) == 0 && d->n > 0) {
        /* max(items, key=lambda x: x[1])[0]: first maximum on ties. */
        size_t best = 0;
        for (size_t i = 1; i < d->n; i++)
            if (d->items[i].prob > d->items[best].prob) best = i;
        {
            struct json_t *members = w5_toolsetdist_3d9ebc_entry_members(d->items[best].key);
            if (w5t_all_valid(valid, members))
                for (size_t j = 0; j < json_len(members); j++)
                    json_append(selected, json_copy(json_get(members, j)));
            json_free(members);
        }
    }
    return selected;
}
