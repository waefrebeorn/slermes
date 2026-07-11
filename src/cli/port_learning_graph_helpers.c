/*
 * port_learning_graph_helpers.c
 *
 * Pure, portable helpers ported from agent/learning_graph.py.
 *
 * These six are self-contained (no filesystem / network / SkillNode dataclass):
 *   - _hermes_meta(fm)        -> metadata.hermes dict (tolerant of string fm)
 *   - _related(fm)            -> list of related skill names
 *   - _category(fm, path)     -> category or path-derived fallback
 *   - _to_int_ts(value)       -> int epoch (from int/float/numeric/ISO string)
 *   - _usage_timestamp(rec)   -> first non-null activity timestamp field
 *   - _tokenize(text)         -> set of tokens len>=3 (returned as JSON array)
 *
 * Pure data-transform ports (operate on JSON representations of the SkillNode
 * dataclass — a node is {"name","category","source","use_count","created_by",
 * "related":[...]}), added per the "rewrite in C is the point" doctrine:
 *   - build_edges(nodes)               -> undirected related_skills edges
 *   - density_stats(nodes, edges)      -> graph density metrics
 *   - _memory_skill_edges(cards,skills)-> lexical memory↔skill edges
 *
 * Filesystem-coupled functions (build_skill_nodes, _iter_skill_files,
 * _load_usage, _memory_cards, _skill_roots, build_learning_graph, _frontmatter)
 * remain REAL_GAP pending fixture-driven ports; they compose the pure ones
 * below with parse_frontmatter / skill_usage_* (both already in the C tree).
 *
 * Module prefix used by the scanner for agent/learning_graph.py is
 * "learning_graph_".
 *
 * C name <- python name (learning_graph_ prefix):
 *   hermes_meta, related, category, to_int_ts, usage_timestamp, tokenize
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"

/* --- ISO-8601 -> time_t (UTC) ----------------------------------------- */
static int parse_iso_to_ts(const char *value, time_t *out)
{
    if (!value || !*value) return 0;
    int Y, M, D, h = 0, m = 0, s = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 3)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = s;
    tmv.tm_isdst = 0;
#ifdef _WIN32
    *out = _mkgmtime(&tmv);
#else
    *out = timegm(&tmv);
#endif
    return (*out != (time_t)-1) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: _hermes_meta @ agent/learning_graph.py:_hermes_meta */
/* fm_json: JSON object. Returns malloc'd JSON of metadata.hermes (or "{}"). */
char *learning_graph_hermes_meta(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("{}"); }
    json_t *meta = json_object_get(fm, "metadata");
    json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
    char *out;
    if (hermes && hermes->type == JSON_OBJECT) {
        char *dump = json_dumps(hermes, 0);
        out = strdup(dump ? dump : "{}");
        free(dump);
    } else {
        out = strdup("{}");
    }
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _related @ agent/learning_graph.py:_related */
/* fm_json: JSON object. Returns malloc'd JSON array of related skill names. */
char *learning_graph_related(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("[]"); }

    json_t *raw = json_object_get(fm, "related_skills");
    if (!raw) {
        /* fall back to metadata.hermes.related_skills */
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT)
            raw = json_object_get(hermes, "related_skills");
    }

    json_t *arr = json_new_array();
    if (raw && raw->type == JSON_ARRAY) {
        size_t n = json_array_size(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(raw, i);
            if (e && e->type == JSON_STRING) {
                const char *sv = json_string_value(e);
                char *cp = strdup(sv ? sv : "");
                char *p = cp;
                while (*p) { *p = (char)tolower((unsigned char)*p); if (*p == ' ') *p = '\0'; p++; }
                if (cp[0]) json_array_append(arr, json_string(cp));
                free(cp);
            }
        }
    } else if (raw && raw->type == JSON_STRING) {
        /* "a, b, c" or "[a, b, c]" -> split on comma */
        const char *sv = json_string_value(raw);
        char *buf = strdup(sv ? sv : "");
        /* strip surrounding brackets */
        char *b = buf;
        if (*b == '[') b++;
        size_t bl = strlen(b);
        if (bl > 0 && b[bl-1] == ']') b[bl-1] = '\0';
        char *tok = strtok(b, ",");
        while (tok) {
            /* trim */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && end[-1] == ' ') end--;
            *end = '\0';
            if (*tok) json_array_append(arr, json_string(tok));
            tok = strtok(NULL, ",");
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    json_free(fm);
    return ret;
}

/* ---------------------------------------------------------------------- */
/* PoP: _category @ agent/learning_graph.py:_category */
/* fm_json: JSON object; fallback: caller-computed path-derived category
 * (mirrors skill_md.parts[-3]). Returns malloc'd category string. */
char *learning_graph_category(const char *fm_json, const char *fallback)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup(fallback ? fallback : "general"); }
    json_t *cat = json_object_get(fm, "category");
    if (!cat || cat->type != JSON_STRING || !json_string_value(cat)[0]) {
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT) {
            json_t *hc = json_object_get(hermes, "category");
            if (hc && hc->type == JSON_STRING && json_string_value(hc)[0]) cat = hc;
        }
    }
    char *out;
    if (cat && cat->type == JSON_STRING && json_string_value(cat)[0])
        out = strdup(json_string_value(cat));
    else
        out = strdup(fallback ? fallback : "general");
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _to_int_ts @ agent/learning_graph.py:_to_int_ts */
/* value_json: a JSON scalar (number or string). *found set to 1 on success.
 * Returns epoch seconds (or 0 with *found=0 on failure). */
long long learning_graph_to_int_ts(const char *value_json, int *found)
{
    *found = 0;
    json_t *v = json_parse(value_json ? value_json : "null", NULL);
    if (!v) return 0;
    long long result = 0;
    if (v->type == JSON_NUMBER) {
        result = (long long)json_number_value(v);
        *found = 1;
    } else if (v->type == JSON_STRING) {
        const char *s = json_string_value(v);
        if (s && *s) {
            char *end = NULL;
            double d = strtod(s, &end);
            if (end != s && *end == '\0') {
                result = (long long)d;
                *found = 1;
            } else {
                /* ISO string */
                time_t t;
                if (parse_iso_to_ts(s, &t)) { result = (long long)t; *found = 1; }
            }
        }
    }
    json_free(v);
    return result;
}

/* ---------------------------------------------------------------------- */
/* PoP: _usage_timestamp @ agent/learning_graph.py:_usage_timestamp */
/* rec_json: JSON object. Returns first valid ts among the priority keys,
 * or 0 with *found=0 if none. */
long long learning_graph_usage_timestamp(const char *rec_json, int *found)
{
    static const char *keys[] = {"last_activity_at", "last_used_at", "last_viewed_at",
                                 "last_patched_at", "created_at"};
    *found = 0;
    json_t *rec = json_parse(rec_json ? rec_json : "{}", NULL);
    if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); return 0; }
    long long best = 0;
    for (int i = 0; i < 5; i++) {
        json_t *kv = json_object_get(rec, keys[i]);
        if (!kv) continue;
        char *sv = json_dumps(kv, 0);
        int f = 0;
        long long ts = learning_graph_to_int_ts(sv, &f);
        free(sv);
        if (f) { best = ts; *found = 1; break; }
    }
    json_free(rec);
    return best;
}

/* ---------------------------------------------------------------------- */
/* PoP: _tokenize @ agent/learning_graph.py:_tokenize */
/* Returns malloc'd JSON array of tokens (len>=3) from text. */
char *learning_graph_tokenize(const char *text)
{
    json_t *arr = json_new_array();
    if (text && *text) {
        char *buf = strdup(text);
        for (char *p = buf; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }
        /* split on runs of non [a-z0-9] */
        char *start = buf;
        char *p = buf;
        while (1) {
            int isword = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9');
            if (!isword || *p == '\0') {
                if (p > start) {
                    char saved = *p;
                    *p = '\0';
                    size_t len = strlen(start);
                    if (len >= 3) json_array_append(arr, json_string(start));
                    *p = saved;
                }
                start = p + 1;
                if (*p == '\0') break;
            }
            p++;
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    return ret;
}

/* ====================================================================== */
/* Pure data-transform ports (JSON-in / JSON-out).                        */
/* ====================================================================== */

/* internal: build a sorted, de-duplicated token set from text (len>=3). */
static char **lg_token_set(const char *text, int *out_n)
{
    *out_n = 0;
    char *js = learning_graph_tokenize(text);
    json_t *arr = json_parse(js ? js : "[]", NULL);
    free(js);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return NULL; }
    size_t n = json_array_size(arr);
    char **toks = (char **)calloc(n ? n : 1, sizeof(char *));
    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_array_get(arr, i);
        if (!e || e->type != JSON_STRING) continue;
        const char *sv = json_string_value(e);
        if (!sv) continue;
        int dup = 0;
        for (int j = 0; j < cnt; j++) if (strcmp(toks[j], sv) == 0) { dup = 1; break; }
        if (!dup) toks[cnt++] = strdup(sv);
    }
    json_free(arr);
    *out_n = cnt;
    return toks;
}

static void lg_free_set(char **toks, int n)
{
    for (int i = 0; i < n; i++) free(toks[i]);
    free(toks);
}

static int lg_set_intersect_count(char **a, int an, char **b, int bn)
{
    int c = 0;
    for (int i = 0; i < an; i++)
        for (int j = 0; j < bn; j++)
            if (strcmp(a[i], b[j]) == 0) { c++; break; }
    return c;
}

/* PoP: learning_graph_build_edges @ agent/learning_graph.py:build_edges */
/* nodes_json: JSON array of {"name":str,"related":[str,...]}. Returns a
 * malloc'd JSON array of [a,b] sorted, deduped undirected edges (endpoints
 * both present, no self-loops). Preserves node/related iteration order. */
char *learning_graph_build_edges(const char *nodes_json)
{
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    json_t *out = json_new_array();
    if (!nodes || nodes->type != JSON_ARRAY) { if (nodes) json_free(nodes); goto done; }

    size_t nn = json_array_size(nodes);
    /* collect names for membership */
    char **names = (char **)calloc(nn ? nn : 1, sizeof(char *));
    int ncnt = 0;
    for (size_t i = 0; i < nn; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        json_t *nm = json_object_get(nd, "name");
        if (nm && nm->type == JSON_STRING && json_string_value(nm))
            names[ncnt++] = strdup(json_string_value(nm));
    }
    /* dedup set of "a\x01b" edge keys */
    char **seen = (char **)calloc(64, sizeof(char *));
    int seen_cap = 64, seen_n = 0;

    for (size_t i = 0; i < nn; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        json_t *nm = json_object_get(nd, "name");
        if (!nm || nm->type != JSON_STRING || !json_string_value(nm)) continue;
        const char *self = json_string_value(nm);
        json_t *rel = json_object_get(nd, "related");
        if (!rel || rel->type != JSON_ARRAY) continue;
        size_t rn = json_array_size(rel);
        for (size_t r = 0; r < rn; r++) {
            json_t *te = json_array_get(rel, r);
            if (!te || te->type != JSON_STRING) continue;
            const char *tgt = json_string_value(te);
            if (!tgt || strcmp(tgt, self) == 0) continue;
            /* target must exist among names */
            int found = 0;
            for (int k = 0; k < ncnt; k++) if (strcmp(names[k], tgt) == 0) { found = 1; break; }
            if (!found) continue;
            /* sorted (a,b) */
            const char *a = self, *b = tgt;
            if (strcmp(a, b) > 0) { const char *t = a; a = b; b = t; }
            /* dedup key */
            size_t kl = strlen(a) + strlen(b) + 2;
            char *key = (char *)malloc(kl);
            snprintf(key, kl, "%s\x01%s", a, b);
            int dup = 0;
            for (int s = 0; s < seen_n; s++) if (strcmp(seen[s], key) == 0) { dup = 1; break; }
            if (dup) { free(key); continue; }
            if (seen_n >= seen_cap) { seen_cap *= 2; seen = (char **)realloc(seen, seen_cap * sizeof(char *)); }
            seen[seen_n++] = key;
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(a));
            json_array_append(pair, json_string(b));
            json_array_append(out, pair);
        }
    }
    for (int k = 0; k < ncnt; k++) free(names[k]);
    free(names);
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(seen);
    json_free(nodes);
done:;
    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(out);
    return ret;
}

/* PoP: learning_graph_density_stats @ agent/learning_graph.py:density_stats */
/* nodes_json: JSON array of node objects (name,category,use_count,created_by).
 * edges_json: JSON array of [a,b] pairs. Returns malloc'd JSON object of graph
 * density metrics matching the Python dict (edges_per_node round-3,
 * isolated_pct round-1, top_categories top-8 by count desc / first-seen). */
char *learning_graph_density_stats(const char *nodes_json, const char *edges_json)
{
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    json_t *edges = json_parse(edges_json ? edges_json : "[]", NULL);
    json_t *res = json_new_object();

    int node_count = (nodes && nodes->type == JSON_ARRAY) ? (int)json_array_size(nodes) : 0;
    int edge_count = (edges && edges->type == JSON_ARRAY) ? (int)json_array_size(edges) : 0;

    /* linked node set */
    char **linked = (char **)calloc(edge_count * 2 + 1, sizeof(char *));
    int linked_n = 0;
    for (int i = 0; i < edge_count; i++) {
        json_t *e = json_array_get(edges, i);
        if (!e || e->type != JSON_ARRAY || json_array_size(e) < 2) continue;
        for (int p = 0; p < 2; p++) {
            json_t *ep = json_array_get(e, p);
            if (!ep || ep->type != JSON_STRING) continue;
            const char *sv = json_string_value(ep);
            int dup = 0;
            for (int j = 0; j < linked_n; j++) if (strcmp(linked[j], sv) == 0) { dup = 1; break; }
            if (!dup) linked[linked_n++] = strdup(sv);
        }
    }

    /* categories in first-seen order; agent_created; used */
    char **cats = (char **)calloc(node_count ? node_count : 1, sizeof(char *));
    int *cat_counts = (int *)calloc(node_count ? node_count : 1, sizeof(int));
    int cat_n = 0, agent_created = 0, used = 0;
    for (int i = 0; i < node_count; i++) {
        json_t *nd = json_array_get(nodes, i);
        if (!nd || nd->type != JSON_OBJECT) continue;
        const char *cat = json_object_get_string(nd, "category", "general");
        if (!cat) cat = "general";
        int idx = -1;
        for (int j = 0; j < cat_n; j++) if (strcmp(cats[j], cat) == 0) { idx = j; break; }
        if (idx < 0) { cats[cat_n] = strdup(cat); cat_counts[cat_n] = 1; cat_n++; }
        else cat_counts[idx]++;
        const char *cb = json_object_get_string(nd, "created_by", NULL);
        if (cb && strcmp(cb, "agent") == 0) agent_created++;
        double uc = json_object_get_number(nd, "use_count", 0);
        if (uc > 0) used++;
    }

    int denom = node_count ? node_count : 1;
    double epn = (double)edge_count / denom;
    double iso = 100.0 * (denom - linked_n) / denom;

    json_object_set(res, "nodes", json_int(node_count));
    json_object_set(res, "related_edges", json_int(edge_count));
    json_object_set(res, "edges_per_node", json_number(epn));
    json_object_set(res, "linked_nodes", json_int(linked_n));
    json_object_set(res, "isolated_pct", json_number(iso));
    json_object_set(res, "categories", json_int(cat_n));
    json_object_set(res, "agent_created", json_int(agent_created));
    json_object_set(res, "used", json_int(used));

    /* top_categories: stable sort by count desc (first-seen tie-break), top 8 */
    int *order = (int *)calloc(cat_n ? cat_n : 1, sizeof(int));
    for (int i = 0; i < cat_n; i++) order[i] = i;
    for (int i = 0; i < cat_n; i++)
        for (int j = i + 1; j < cat_n; j++)
            if (cat_counts[order[j]] > cat_counts[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    json_t *top = json_new_array();
    int limit = cat_n < 8 ? cat_n : 8;
    for (int i = 0; i < limit; i++) {
        json_t *pair = json_new_array();
        json_array_append(pair, json_string(cats[order[i]]));
        json_array_append(pair, json_int(cat_counts[order[i]]));
        json_array_append(top, pair);
    }
    json_object_set(res, "top_categories", top);
    free(order);

    for (int i = 0; i < linked_n; i++) free(linked[i]);
    free(linked);
    for (int i = 0; i < cat_n; i++) free(cats[i]);
    free(cats); free(cat_counts);
    if (nodes) json_free(nodes);
    if (edges) json_free(edges);

    char *dump = json_dumps(res, 0);
    char *ret = strdup(dump ? dump : "{}");
    free(dump);
    json_free(res);
    return ret;
}

/* PoP: learning_graph_memory_skill_edges @ agent/learning_graph.py:_memory_skill_edges */
/* cards_json: array of {"source","title","body"}; skills_json: array of {"name"}.
 * Returns malloc'd JSON array of [mem_id, skill_name] lexical-overlap edges
 * (score = 6 if skill name substring in text, + |name_tokens ∩ text_tokens|;
 * top 4 per card, sorted by score desc then name asc). */
char *learning_graph_memory_skill_edges(const char *cards_json, const char *skills_json)
{
    json_t *cards = json_parse(cards_json ? cards_json : "[]", NULL);
    json_t *skills = json_parse(skills_json ? skills_json : "[]", NULL);
    json_t *out = json_new_array();

    int scount = (skills && skills->type == JSON_ARRAY) ? (int)json_array_size(skills) : 0;
    /* precompute per-skill: name, name_lower, name_token_set */
    char **snames = (char **)calloc(scount ? scount : 1, sizeof(char *));
    char **slower = (char **)calloc(scount ? scount : 1, sizeof(char *));
    char ***stoks = (char ***)calloc(scount ? scount : 1, sizeof(char **));
    int *stok_n = (int *)calloc(scount ? scount : 1, sizeof(int));
    int sn = 0;
    for (int i = 0; i < scount; i++) {
        json_t *sk = json_array_get(skills, i);
        if (!sk || sk->type != JSON_OBJECT) continue;
        const char *nm = json_object_get_string(sk, "name", NULL);
        if (!nm) continue;
        snames[sn] = strdup(nm);
        char *lo = strdup(nm);
        for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
        slower[sn] = lo;
        stoks[sn] = lg_token_set(nm, &stok_n[sn]);
        sn++;
    }

    int ccount = (cards && cards->type == JSON_ARRAY) ? (int)json_array_size(cards) : 0;
    for (int ci = 0; ci < ccount; ci++) {
        json_t *card = json_array_get(cards, ci);
        if (!card || card->type != JSON_OBJECT) continue;
        const char *src = json_object_get_string(card, "source", "");
        const char *title = json_object_get_string(card, "title", "");
        const char *body = json_object_get_string(card, "body", "");
        /* mem_id = "memory:<source>:<idx>" */
        char mem_id[512];
        snprintf(mem_id, sizeof(mem_id), "memory:%s:%d", src ? src : "", ci);
        /* text = lower("title\nbody") */
        size_t tl = strlen(title ? title : "") + strlen(body ? body : "") + 2;
        char *text = (char *)malloc(tl);
        snprintf(text, tl, "%s\n%s", title ? title : "", body ? body : "");
        for (char *p = text; *p; p++) *p = (char)tolower((unsigned char)*p);
        int ttn = 0;
        char **ttoks = lg_token_set(text, &ttn);

        /* score each skill */
        int *scores = (int *)calloc(sn ? sn : 1, sizeof(int));
        int *idxs = (int *)calloc(sn ? sn : 1, sizeof(int));
        int scored_n = 0;
        for (int s = 0; s < sn; s++) {
            int score = 0;
            if (slower[s][0] && strstr(text, slower[s])) score += 6;
            score += lg_set_intersect_count(stoks[s], stok_n[s], ttoks, ttn);
            if (score > 0) { scores[scored_n] = score; idxs[scored_n] = s; scored_n++; }
        }
        /* stable sort by (-score, name asc) */
        for (int i = 0; i < scored_n; i++)
            for (int j = i + 1; j < scored_n; j++) {
                int swap = 0;
                if (scores[idxs[j]] > scores[idxs[i]]) swap = 1;
                else if (scores[idxs[j]] == scores[idxs[i]] &&
                         strcmp(snames[idxs[j]], snames[idxs[i]]) < 0) swap = 1;
                if (swap) { int t = idxs[i]; idxs[i] = idxs[j]; idxs[j] = t; }
            }
        int lim = scored_n < 4 ? scored_n : 4;
        for (int i = 0; i < lim; i++) {
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(mem_id));
            json_array_append(pair, json_string(snames[idxs[i]]));
            json_array_append(out, pair);
        }
        free(scores); free(idxs);
        lg_free_set(ttoks, ttn);
        free(text);
    }

    for (int i = 0; i < sn; i++) { free(snames[i]); free(slower[i]); lg_free_set(stoks[i], stok_n[i]); }
    free(snames); free(slower); free(stoks); free(stok_n);
    if (cards) json_free(cards);
    if (skills) json_free(skills);

    char *dump = json_dumps(out, 0);
    char *ret = strdup(dump ? dump : "[]");
    free(dump);
    json_free(out);
    return ret;
}
