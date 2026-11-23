/*
 * skills_hub_github.c -- faithful C11 port of upstream Hermes
 * tools/skills_hub_github.py (gate w10_skillshub_742005).
 *
 * PoP: w10_skillshub_742005_github_provider_for     @ tools/skills_hub_github.py:github_provider_for
 * PoP: w10_skillshub_742005_provider_filter_of      @ tools/skills_hub_github.py:_provider_filter_of
 * PoP: w10_skillshub_742005_tap_cache_key           @ tools/skills_hub_github.py:_tap_cache_key
 * PoP: w10_skillshub_742005_split_repo_id           @ tools/skills_hub_github.py:_split_repo_id
 * PoP: w10_skillshub_742005_skill_file_path         @ tools/skills_hub_github.py:_skill_file_path
 * PoP: w10_skillshub_742005_skip_bundle_file        @ tools/skills_hub_github.py:_skip_bundle_file
 * PoP: w10_skillshub_742005_is_rate_limit_response  @ tools/skills_hub_github.py:_is_rate_limit_response
 * PoP: w10_skillshub_742005_tree_members            @ tools/skills_hub_github.py:_tree_members
 * PoP: w10_skillshub_742005_parse_skillsh_groupings @ tools/skills_hub_github.py:GitHubSource._parse_skillsh_groupings
 *
 * Network/auth/transport surface is not portable; the nine deterministic
 * helpers above are ported verbatim (including literal tables and string
 * normalization).
 */
#include "noc_w10_skillshub_742005.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static char *str_replace(const char *s, char from, char to) {
    if (!s) s = "";
    char *out = (char *)malloc(strlen(s) + 1);
    if (!out) return NULL;
    strcpy(out, s);
    for (char *p = out; *p; p++) if (*p == from) *p = to;
    return out;
}

static char *str_concat3(const char *a, const char *b, const char *c) {
    if (!a) a = ""; if (!b) b = ""; if (!c) c = "";
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s%s%s", a, b, c);
    return out;
}

/* GITHUB_TAP_PROVIDERS -- order matches the Python literal. */
static const char *const TAP_REPOS[] = {
    "openai/skills", "anthropics/skills", "huggingface/skills", "nvidia/skills",
    "voltagent/awesome-agent-skills", "garrytan/gstack", "minimax-ai/cli", "wandb/skills",
    "qdrant/skills", "pinecone-io/skills", "makenotion/skills", "airtable/skills",
    "shopify/shopify-ai-toolkit", NULL
};
static const char *const TAP_PROVIDERS[] = {
    "OpenAI", "Anthropic", "HuggingFace", "NVIDIA", "VoltAgent", "gstack", "MiniMax",
    "Weights & Biases", "Qdrant", "Pinecone", "Notion", "Airtable", "Shopify", NULL
};

static char *ascii_lower(const char *s) {
    if (!s) s = "";
    char *out = (char *)malloc(strlen(s) + 1);
    if (!out) return NULL;
    size_t i;
    for (i = 0; s[i]; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
    return out;
}

json_t *w10_skillshub_742005_github_provider_for(const char *repo) {
    if (!repo || !*repo) return json_null();
    char *key = ascii_lower(repo);
    /* .strip().lower() -- trim ASCII whitespace around the key. */
    char *e = key + strlen(key);
    while (e > key && isspace((unsigned char)e[-1])) *--e = '\0';
    char *p = key;
    while (*p && isspace((unsigned char)*p)) p++;
    for (int i = 0; TAP_REPOS[i]; i++) {
        if (!strcmp(p, TAP_REPOS[i])) { free(key); return json_string(TAP_PROVIDERS[i]); }
    }
    free(key);
    return json_null();
}

json_t *w10_skillshub_742005_provider_filter_of(const char *source_filter) {
    char *v = ascii_lower(source_filter ? source_filter : "");
    char *e = v + strlen(v);
    while (e > v && isspace((unsigned char)e[-1])) *--e = '\0';
    char *p = v;
    while (*p && isspace((unsigned char)*p)) p++;
    int ok = 0;
    for (int i = 0; TAP_PROVIDERS[i]; i++) {
        char *lp = ascii_lower(TAP_PROVIDERS[i]);
        if (!strcmp(p, lp)) { ok = 1; free(lp); break; }
        free(lp);
    }
    json_t *out = ok ? json_string(p) : json_string("");
    free(v);
    return out;
}

json_t *w10_skillshub_742005_tap_cache_key(const char *repo, const char *path, const char *bucket) {
    char *raw = str_concat3(repo, "_", path ? path : "");
    char *raw2 = str_concat3(raw, "_", (bucket && *bucket) ? bucket : "");
    free(raw);
    char *a = str_replace(raw2, '/', '_');
    char *b = str_replace(a, ' ', '_');
    json_t *out = json_string(b);
    free(raw2); free(a); free(b);
    return out;
}

json_t *w10_skillshub_742005_split_repo_id(const char *identifier) {
    if (!identifier) return json_null();
    const char *s1 = strchr(identifier, '/');
    if (!s1) return json_null();
    const char *s2 = strchr(s1 + 1, '/');
    if (!s2) return json_null();
    size_t repo_n = (size_t)(s2 - identifier);
    char *repo = (char *)malloc(repo_n + 1);
    if (!repo) return json_null();
    memcpy(repo, identifier, repo_n); repo[repo_n] = '\0';
    json_t *out = json_array();
    json_append(out, json_string(repo));
    json_append(out, json_string(s2 + 1));
    free(repo);
    return out;
}

json_t *w10_skillshub_742005_skill_file_path(const char *skill_path, const char *filename) {
    if (!filename) filename = "";
    if (skill_path && *skill_path) {
        char *s = str_concat3(skill_path, "/", filename);
        json_t *out = json_string(s);
        free(s);
        return out;
    }
    return json_string(filename);
}

json_t *w10_skillshub_742005_skip_bundle_file(const char *rel_path) {
    if (!rel_path) rel_path = "";
    const char *base = strrchr(rel_path, '/');
    base = base ? base + 1 : rel_path;
    int skip = base[0] == '.';
    size_t bl = strlen(base);
    if (bl >= 4 && !strcmp(base + bl - 4, ".pyc")) skip = 1;
    /* "__pycache__" in rel_path.split("/") -- exact segment match. */
    const char *p = rel_path;
    while (*p) {
        const char *q = strchr(p, '/');
        size_t n = q ? (size_t)(q - p) : strlen(p);
        if (n == 11 && !strncmp(p, "__pycache__", 11)) skip = 1;
        if (!q) break;
        p = q + 1;
    }
    return json_bool(skip);
}

json_t *w10_skillshub_742005_is_rate_limit_response(int status_code, const json_t *headers) {
    int limited = 0;
    if (status_code == 429) limited = 1;
    else if (status_code == 403) {
        const char *rem = headers ? json_get_str(headers, "X-RateLimit-Remaining", "") : "";
        if (rem && !strcmp(rem, "0")) limited = 1;
    }
    return json_bool(limited);
}

json_t *w10_skillshub_742005_tree_members(const json_t *entries, const char *prefix) {
    json_t *out = json_array();
    if (!prefix) prefix = "";
    size_t pl = strlen(prefix);
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(entries, i);
        const char *item_path = jstr(json_obj_get(item, "path"));
        if (strncmp(item_path, prefix, pl)) continue;
        const char *typ = jstr(json_obj_get(item, "type"));
        const char *mode = jstr(json_obj_get(item, "mode"));
        int regular = !strcmp(typ, "blob") && strcmp(mode, "120000");
        json_t *tri = json_array();
        json_append(tri, json_string(item_path + pl));
        json_append(tri, json_string(item_path));
        json_append(tri, json_bool(regular));
        json_append(out, tri);
    }
    return out;
}

json_t *w10_skillshub_742005_parse_skillsh_groupings(const char *content) {
    if (!content) return json_null();
    json_t *data = json_parse(content, NULL);
    if (!data || data->type != JSON_OBJECT) { if (data) json_free(data); return json_null(); }
    json_t *groupings = json_obj_get(data, "groupings");
    if (!groupings || groupings->type != JSON_ARRAY) { json_free(data); return json_null(); }
    json_t *mapping = json_object();
    size_t n = json_len(groupings);
    for (size_t i = 0; i < n; i++) {
        json_t *group = json_get(groupings, i);
        if (!group || group->type != JSON_OBJECT) continue;
        json_t *tv = json_obj_get(group, "title");
        json_t *mv = json_obj_get(group, "skills");
        if (!tv || tv->type != JSON_STRING || !mv || mv->type != JSON_ARRAY) continue;
        const char *title = tv->str_val;
        size_t m = json_len(mv);
        for (size_t j = 0; j < m; j++) {
            json_t *member = json_get(mv, j);
            if (!member || member->type != JSON_STRING || !member->str_val || !*member->str_val) continue;
            if (!json_obj_get(mapping, member->str_val))
                json_set(mapping, member->str_val, json_string(title));
        }
    }
    json_free(data);
    return mapping;
}
