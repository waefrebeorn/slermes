/*
 * port_learning_graph_render.c — rendering + orchestration layer for
 * agent/learning_graph_render.py.
 *
 * Ports the 18 functions the pure helpers compose with: the recency map, the
 * timeline bucket builder, bucket -> row/label/node extraction, category color
 * map, the trajectory sparkline, the full grid renderer, legend, axis labels,
 * peak day, run merging, and the multi-frame prerender.
 *
 * Reuses (no duplication):
 *   - port_learning_graph_render_helpers.c : to_ts, format_date, period_key,
 *     period_label, node_label, node_meta, node_score, recency_ink, rgb/hsl
 *     color helpers, derive_palette, clamp, smoothstep.
 *   - hermes_json.h for all data.
 *
 * Style-run grid format (matches Python [text, style, alpha, hex?]):
 *   run  = JSON array [string, string, number, string?]
 *   row  = JSON array of runs
 *   grid = JSON array of rows
 *
 * The json lib has no in-place array insert/remove/index-set, so sorting and
 * run-merging are done on C-side arrays of (key, node*) / run structs and
 * re-emitted as fresh JSON (with json_copy where a node is re-added).
 */

#include "port_learning_graph_render.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define LGR_LEAD_IN 0.06
#define LGR_MAX_LABELS 6

static char *lg_strdup(const char *s) { return s ? strdup(s) : strdup(""); }

/* node timestamp; returns 1 if present (value in *out). */
static int lg_ts(const json_t *node, double *out) {
    if (!node) return 0;
    json_t *tsv = json_object_get(node, "timestamp");
    if (!tsv) return 0;
    if (tsv->type == JSON_NUMBER) { *out = tsv->num_val; return 1; }
    if (tsv->type == JSON_STRING) { *out = strtod(json_string_value(tsv), NULL); return 1; }
    return 0;
}

/* node id string (caller must not free; points into node). */
static const char *lg_id(const json_t *node) {
    return node ? json_object_get_string(node, "id", "") : "";
}

/* ── node-entry sort helper ─────────────────────────────────────────────── */

typedef struct { double key; const json_t *node; } node_ent_t;

static int lg_cmp_node_key(const void *a, const void *b) {
    const node_ent_t *x = a, *y = b;
    if (x->key < y->key) return -1;
    if (x->key > y->key) return 1;
    return strcmp(lg_id(x->node), lg_id(y->node));
}
/* descending by key (then id) for category/peak tie-break */
static int lg_cmp_node_key_desc(const void *a, const void *b) {
    const node_ent_t *x = a, *y = b;
    if (x->key > y->key) return -1;
    if (x->key < y->key) return 1;
    return strcmp(lg_id(y->node), lg_id(x->node));
}

/* Build a JSON array = copy of `nodes` (a JSON array) sorted by lg_ts, stable
 * on (ts,id). Returns new array (caller frees). */
static json_t *lg_sorted_by_ts(const json_t *nodes) {
    json_t *out = json_new_array();
    if (!nodes || nodes->type != JSON_ARRAY) return out;
    size_t n = json_array_size(nodes);
    node_ent_t *es = malloc((n ? n : 1) * sizeof(node_ent_t));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *nd = json_array_get(nodes, i);
        double ts; if (!lg_ts(nd, &ts)) ts = INFINITY;
        es[m].key = ts; es[m].node = nd; m++;
    }
    qsort(es, m, sizeof(node_ent_t), lg_cmp_node_key);
    for (size_t i = 0; i < m; i++) json_array_append(out, json_copy(es[i].node));
    free(es);
    return out;
}

/* ── compute_recency ─────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_compute_recency @ agent/learning_graph_render.py:compute_recency */
char *learning_graph_render_compute_recency(const char *nodes_json) {
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    int has_min = 0, has_max = 0;
    double min_ts = 0, max_ts = 0;
    if (nodes && nodes->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(nodes); i++) {
            double ts; if (!lg_ts(json_array_get(nodes, i), &ts)) continue;
            if (!has_min || ts < min_ts) { min_ts = ts; has_min = 1; }
            if (!has_max || ts > max_ts) { max_ts = ts; has_max = 1; }
        }
    }
    int timed = has_min && has_max && max_ts > min_ts;

    json_t *ordered = lg_sorted_by_ts(nodes);
    size_t last = json_array_size(ordered) > 1 ? json_array_size(ordered) - 1 : 1;

    json_t *ord_ratio = json_new_object();
    for (size_t i = 0; i < json_array_size(ordered); i++) {
        const json_t *nd = json_array_get(ordered, i);
        double r = json_array_size(ordered) > 1 ? (double)i / (double)(json_array_size(ordered) - 1) : 0.0;
        json_object_set(ord_ratio, lg_id(nd), json_number(r));
    }

    json_t *rec = json_new_object();
    if (nodes && nodes->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(nodes); i++) {
            const json_t *nd = json_array_get(nodes, i);
            const char *nid = lg_id(nd);
            double ts; int ok = lg_ts(nd, &ts);
            double ratio;
            if (timed && ok)
                ratio = (max_ts - min_ts) > 0 ? (ts - min_ts) / (max_ts - min_ts) : 0.0;
            else
                ratio = json_object_get_number(ord_ratio, nid, 0.0);
            double v = LGR_LEAD_IN + (1.0 - LGR_LEAD_IN) * learning_graph_render_clamp(ratio, 0.0, 1.0);
            json_object_set(rec, nid, json_number(v));
        }
    }

    json_t *out = json_new_object();
    json_object_set(out, "rec", rec);
    json_object_set(out, "timed", json_bool(timed));
    json_object_set(out, "minTs", has_min ? json_number(min_ts) : json_null());
    json_object_set(out, "maxTs", has_max ? json_number(max_ts) : json_null());

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "{}");
    if (dump) free(dump);
    if (nodes) json_free(nodes);
    json_free(ordered); json_free(ord_ratio); json_free(out);
    return ret;
}

/* ── _date_at ───────────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_date_at @ agent/learning_graph_render.py:_date_at */
char *learning_graph_render_date_at(const char *rec_json, double reveal) {
    json_t *rec = json_parse(rec_json ? rec_json : "{}", NULL);
    int timed = rec ? json_object_get_bool(rec, "timed", 0) : 0;
    double lo = rec ? json_object_get_number(rec, "minTs", 0) : 0;
    double hi = rec ? json_object_get_number(rec, "maxTs", 0) : 0;
    if (!rec || !timed || (lo == 0 && hi == 0)) {
        if (rec) json_free(rec);
        return lg_strdup("unknown");
    }
    double ts = round(lo + learning_graph_render_clamp(reveal, 0.0, 1.0) * (hi - lo));
    char *d = learning_graph_render_format_date(ts);
    if (rec) json_free(rec);
    return d;
}

/* ── _ChartBucket.total ─────────────────────────────────────────────────── */

/* PoP: learning_graph_render_bucket_total @ agent/learning_graph_render.py:total */
int learning_graph_render_bucket_total(const char *bucket_json) {
    json_t *b = json_parse(bucket_json ? bucket_json : "{}", NULL);
    int s = b ? (int)json_object_get_number(b, "skills", 0) : 0;
    int m = b ? (int)json_object_get_number(b, "memories", 0) : 0;
    if (b) json_free(b);
    return s + m;
}

/* ── _build_chart_buckets ───────────────────────────────────────────────── */

static json_t *lg_make_bucket(const char *label, double ts) {
    json_t *b = json_new_object();
    json_object_set(b, "label", json_string(label ? label : ""));
    json_object_set(b, "ts", json_number(ts));
    json_object_set(b, "skills", json_int(0));
    json_object_set(b, "memories", json_int(0));
    json_object_set(b, "rec", json_number(1.0));
    json_object_set(b, "nodes", json_new_array());
    return b;
}
static void lg_bucket_add(json_t *b, const json_t *node) {
    json_t *nodes = json_object_get(b, "nodes");
    if (nodes) json_array_append(nodes, json_copy(node));
    const char *kind = json_object_get_string(node, "kind", "");
    if (strcmp(kind, "memory") == 0)
        json_object_set(b, "memories", json_int(json_object_get_number(b, "memories", 0) + 1));
    else
        json_object_set(b, "skills", json_int(json_object_get_number(b, "skills", 0) + 1));
}

/* PoP: learning_graph_render_build_chart_buckets @ agent/learning_graph_render.py:_build_chart_buckets */
char *learning_graph_render_build_chart_buckets(const char *nodes_json,
                                                 const char *rec_json, int max_rows) {
    json_t *nodes = json_parse(nodes_json ? nodes_json : "[]", NULL);
    json_t *rec = json_parse(rec_json ? rec_json : "{}", NULL);
    json_t *out = json_new_array();

    if (nodes && nodes->type == JSON_ARRAY && json_array_size(nodes) > 0) {
        int timed = rec ? json_object_get_bool(rec, "timed", 0) : 0;
        if (!timed) {
            int n_bins = max_rows > 0 ? max_rows : 1;
            if (n_bins > (int)json_array_size(nodes)) n_bins = (int)json_array_size(nodes);
            if (n_bins < 1) n_bins = 1;
            json_t **buckets = calloc(n_bins, sizeof(json_t *));
            for (int i = 0; i < n_bins; i++) {
                char lbl[16]; snprintf(lbl, sizeof(lbl), "#%d", i + 1);
                buckets[i] = lg_make_bucket(lbl, (double)i);
            }
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                const char *nid = lg_id(nd);
                double r = rec ? json_object_get_number(rec, nid, 0.0) : 0.0;
                int idx = (int)learning_graph_render_clamp(floor(r * n_bins), 0, n_bins - 1);
                lg_bucket_add(buckets[idx], nd);
            }
            for (int i = 0; i < n_bins; i++) json_array_append(out, buckets[i]);
            free(buckets);
        } else {
            const char *grans[3] = { "day", "month", "year" };
            json_t *chosen = NULL;
            for (int gi = 0; gi < 3; gi++) {
                /* group by period key */
                json_t *garr = json_new_array();
                for (size_t i = 0; i < json_array_size(nodes); i++) {
                    const json_t *nd = json_array_get(nodes, i);
                    double ts; if (!lg_ts(nd, &ts)) continue;
                    char *key = learning_graph_render_period_key(ts, grans[gi]);
                    char *label = learning_graph_render_period_label(ts, grans[gi]);
                    int found = -1;
                    for (size_t k = 0; k < json_array_size(garr); k++) {
                        if (strcmp(json_object_get_string(json_array_get(garr, k), "_key", ""), key) == 0) { found = (int)k; break; }
                    }
                    json_t *b;
                    if (found < 0) {
                        b = lg_make_bucket(label, ts);
                        json_object_set(b, "_key", json_string(key));
                        json_array_append(garr, b);
                    } else {
                        b = json_array_get(garr, found);
                    }
                    lg_bucket_add(b, nd);
                    free(key); free(label);
                }
                /* sort by ts ascending */
                {
                    size_t m = json_array_size(garr);
                    node_ent_t *es = malloc((m ? m : 1) * sizeof(node_ent_t));
                    for (size_t k = 0; k < m; k++) {
                        es[k].node = json_array_get(garr, k);
                        es[k].key = json_object_get_number(es[k].node, "ts", 0);
                    }
                    qsort(es, m, sizeof(node_ent_t), lg_cmp_node_key);
                    json_t *sorted = json_new_array();
                    for (size_t k = 0; k < m; k++) json_array_append(sorted, json_copy(es[k].node));
                    free(es);
                    json_free(garr);   /* es holds no refs into garr; safe */
                    garr = sorted;     /* sorted replaces garr */
                }
                int fits = (int)json_array_size(garr) <= max_rows ||
                           (strcmp(grans[gi], "day") == 0 && (int)json_array_size(garr) <= 32);
                if (fits) { chosen = garr; break; }
                json_free(garr);
            }
            if (!chosen) {
                double min_ts = rec ? json_object_get_number(rec, "minTs", 0) : 0;
                double max_ts = rec ? json_object_get_number(rec, "maxTs", 0) : 0;
                int n_bins = max_rows > 0 ? max_rows : 1; if (n_bins < 1) n_bins = 1;
                chosen = json_new_array();
                for (int i = 0; i < n_bins; i++) {
                    double ts = (min_ts && max_ts) ? min_ts + (i / (double)(n_bins > 1 ? n_bins - 1 : 1)) * (max_ts - min_ts) : (double)i;
                    char *d = learning_graph_render_format_date(ts);
                    json_t *b = lg_make_bucket(d, ts);
                    free(d);
                    json_array_append(chosen, b);
                }
                for (size_t i = 0; i < json_array_size(nodes); i++) {
                    const json_t *nd = json_array_get(nodes, i);
                    const char *nid = lg_id(nd);
                    double r = rec ? json_object_get_number(rec, nid, 0.0) : 0.0;
                    int idx = (int)learning_graph_render_clamp(floor(r * n_bins), 0, n_bins - 1);
                    lg_bucket_add(json_array_get(chosen, idx), nd);
                }
            }
            /* per-bucket rec from ts span */
            double min_ts = rec ? json_object_get_number(rec, "minTs", 0) : 0;
            double max_ts = rec ? json_object_get_number(rec, "maxTs", 0) : 0;
            double span = (max_ts > min_ts) ? (max_ts - min_ts) : 0;
            for (size_t i = 0; i < json_array_size(chosen); i++) {
                json_t *b = json_array_get(chosen, i);
                double ts = json_object_get_number(b, "ts", 0);
                double rv = span ? LGR_LEAD_IN + (1.0 - LGR_LEAD_IN) * ((ts - min_ts) / span) : 1.0;
                json_object_set(b, "rec", json_number(rv));
            }
            for (size_t i = 0; i < json_array_size(chosen); i++)
                json_array_append(out, json_copy(json_array_get(chosen, i)));
            json_free(chosen);
        }
    }

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (nodes) json_free(nodes);
    if (rec) json_free(rec);
    json_free(out);
    return ret;
}

/* ── _bucket_label_node ─────────────────────────────────────────────────── */

/* PoP: learning_graph_render_bucket_label_node @ agent/learning_graph_render.py:_bucket_label_node */
char *learning_graph_render_bucket_label_node(const char *bucket_json) {
    json_t *b = json_parse(bucket_json ? bucket_json : "{}", NULL);
    const json_t *best = NULL;
    double best_score = -INFINITY;
    if (b) {
        json_t *nodes = json_object_get(b, "nodes");
        if (nodes && nodes->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                double ts; if (!lg_ts(nd, &ts)) ts = json_object_get_number(b, "ts", 0);
                double sc = learning_graph_render_node_score(nd, ts);
                if (sc > best_score) { best_score = sc; best = nd; }
            }
        }
    }
    char *ret;
    if (best) {
        char *dump = json_dumps(best, 0);
        ret = lg_strdup(dump ? dump : "{}");
        if (dump) free(dump);
    } else {
        ret = lg_strdup("null");
    }
    if (b) json_free(b);
    return ret;
}

/* ── _bucket_nodes ──────────────────────────────────────────────────────── */

static const char *lg_node_glyph(const json_t *node) {
    return strcmp(json_object_get_string(node, "kind", ""), "memory") == 0 ? "◆" : "●";
}

/* PoP: learning_graph_render_bucket_nodes @ agent/learning_graph_render.py:_bucket_nodes */
char *learning_graph_render_bucket_nodes(const char *bucket_json,
                                         const char *memory_lookup_json) {
    json_t *b = json_parse(bucket_json ? bucket_json : "{}", NULL);
    json_t *lookup = json_parse(memory_lookup_json ? memory_lookup_json : "{}", NULL);
    json_t *out = json_new_array();
    if (b) {
        json_t *nodes = json_object_get(b, "nodes");
        if (nodes && nodes->type == JSON_ARRAY) {
            json_t *ordered = lg_sorted_by_ts(nodes);
            for (size_t i = 0; i < json_array_size(ordered); i++) {
                const json_t *nd = json_array_get(ordered, i);
                const char *kind = json_object_get_string(nd, "kind", "");
                const char *style = strcmp(kind, "memory") == 0 ? "memory" : "skill";
                const char *raw = json_object_get_string(nd, "label", "");
                if (!raw || !*raw) raw = json_object_get_string(nd, "id", "unknown");
                const char *nid = lg_id(nd);
                char *label = learning_graph_render_node_label(json_object_get_string(nd, "label", ""), nid);
                char *meta = learning_graph_render_node_meta(nd);
                json_t *memory = lookup ? json_object_get(lookup, nid) : NULL;
                const char *body = (memory && memory->type == JSON_OBJECT)
                    ? json_object_get_string(memory, "body", "") : "";
                json_t *o = json_new_object();
                json_object_set(o, "id", json_string(nid));
                json_object_set(o, "glyph", json_string(lg_node_glyph(nd)));
                json_object_set(o, "label", json_string(label ? label : ""));
                json_object_set(o, "fullLabel", json_string(raw ? raw : "unknown"));
                json_object_set(o, "meta", json_string(meta ? meta : ""));
                json_object_set(o, "body", json_string(body ? body : ""));
                json_object_set(o, "style", json_string(style));
                json_array_append(out, o);
                if (label) free(label);
                if (meta) free(meta);
            }
            json_free(ordered);
        }
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (b) json_free(b);
    if (lookup) json_free(lookup);
    json_free(out);
    return ret;
}

/* ── _category_counts ───────────────────────────────────────────────────── */

/* PoP: learning_graph_render_category_counts @ agent/learning_graph_render.py:_category_counts */
char *learning_graph_render_category_counts(const char *payload_json) {
    json_t *p = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *clusters = p ? json_object_get(p, "clusters") : NULL;
    json_t *out = json_new_array();
    int have_clusters = 0;
    if (clusters && clusters->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(clusters); i++) {
            json_t *c = json_array_get(clusters, i);
            const char *cat = json_object_get_string(c, "category", "");
            if (!cat || !*cat || strcmp(cat, "memory") == 0) continue;
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(cat));
            json_array_append(pair, json_number(json_object_get_number(c, "count", 0)));
            json_array_append(out, pair);
            have_clusters = 1;
        }
    }
    if (!have_clusters && p) {
        json_t *counts = json_new_object();
        json_t *nodes = json_object_get(p, "nodes");
        if (nodes && nodes->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                if (strcmp(json_object_get_string(nd, "kind", ""), "memory") == 0) continue;
                const char *cat = json_object_get_string(nd, "category", "skill");
                double cur = json_object_get_number(counts, cat, 0);
                json_object_set(counts, cat, json_number(cur + 1));
            }
        }
        size_t n = json_object_size(counts);
        node_ent_t *es = malloc((n ? n : 1) * sizeof(node_ent_t));
        int k = 0;
        for (size_t i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(counts, i);
            const json_t *v = json_object_get_at(counts, i);
            if (!key || !v) continue;
            /* sort by (-count, cat): key holds count, name in key string */
            es[k].node = (json_t *)key;            /* stash name */
            es[k].key = -json_number_value(v);     /* negative => descending */
            k++;
        }
        /* secondary ascending by name for ties */
        qsort(es, k, sizeof(node_ent_t), lg_cmp_node_key_desc);
        /* lg_cmp_node_key_desc compares key (which is -count) and then id; to
         * get (-count, cat) we instead sort by key then name via a custom cmp */
        (void)es; /* placeholder; do explicit sort below */
        free(es);
        /* explicit stable sort by (-count, name) */
        {
            size_t m = n;
            typedef struct { char name[64]; double count; } cc_t;
            cc_t *cc = malloc((m ? m : 1) * sizeof(cc_t));
            int cck = 0;
            for (size_t i = 0; i < m; i++) {
                const char *key = json_object_get_key_at(counts, i);
                const json_t *v = json_object_get_at(counts, i);
                if (!key || !v) continue;
                snprintf(cc[cck].name, sizeof(cc[cck].name), "%s", key);
                cc[cck].count = json_number_value(v);
                cck++;
            }
            for (int a = 1; a < cck; a++)
                for (int b = a; b > 0; b--) {
                    int swap = (cc[b - 1].count < cc[b].count) ||
                              (cc[b - 1].count == cc[b].count && strcmp(cc[b - 1].name, cc[b].name) > 0);
                    if (!swap) break;
                    cc_t t = cc[b - 1]; cc[b - 1] = cc[b]; cc[b] = t;
                }
            for (int a = 0; a < cck; a++) {
                json_t *pair = json_new_array();
                json_array_append(pair, json_string(cc[a].name));
                json_array_append(pair, json_number(cc[a].count));
                json_array_append(out, pair);
            }
            free(cc);
        }
        json_free(counts);
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (p) json_free(p);
    json_free(out);
    return ret;
}

static char *lg_hsl_to_hex(double h, double s, double l) {
    char *rgbs = learning_graph_render_hsl_to_rgb(h, s, l); /* "r,g,b" */
    int r = 0, g = 0, b = 0;
    if (rgbs) sscanf(rgbs, "%d,%d,%d", &r, &g, &b);
    free(rgbs);
    return learning_graph_render_rgb_to_hex(r, g, b);
}

/* PoP: learning_graph_render_category_color_map @ agent/learning_graph_render.py:category_color_map */
char *learning_graph_render_category_color_map(const char *payload_json) {
    char *counts_json = learning_graph_render_category_counts(payload_json);
    json_t *counts = json_parse(counts_json, NULL);
    json_t *out = json_new_object();
    if (counts && counts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(counts); i++) {
            json_t *pair = json_array_get(counts, i);
            const char *cat = json_string_value(json_array_get(pair, 0));
            double hue = fmod(i * 137.508, 360.0);
            char *hex = lg_hsl_to_hex(hue, 0.55, 0.62);
            json_object_set(out, cat ? cat : "", json_string(hex ? hex : ""));
            free(hex);
        }
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "{}");
    if (dump) free(dump);
    if (counts) json_free(counts);
    free(counts_json);
    json_free(out);
    return ret;
}

/* PoP: learning_graph_render_category_legend @ agent/learning_graph_render.py:category_legend */
char *learning_graph_render_category_legend(const char *payload_json, int limit) {
    char *cmap_json = learning_graph_render_category_color_map(payload_json);
    char *counts_json = learning_graph_render_category_counts(payload_json);
    json_t *cmap = json_parse(cmap_json, NULL);
    json_t *counts = json_parse(counts_json, NULL);
    json_t *out = json_new_array();
    if (counts && counts->type == JSON_ARRAY) {
        int shown = 0;
        for (size_t i = 0; i < json_array_size(counts) && shown < limit; i++) {
            json_t *pair = json_array_get(counts, i);
            const char *cat = json_string_value(json_array_get(pair, 0));
            int count = (int)json_number_value(json_array_get(pair, 1));
            const char *color = cmap ? json_object_get_string(cmap, cat ? cat : "", "") : "";
            json_t *o = json_new_object();
            json_object_set(o, "glyph", json_string("●"));
            json_object_set(o, "color", json_string(color ? color : ""));
            char lbl[64]; snprintf(lbl, sizeof(lbl), "%s (%d)", cat ? cat : "", count);
            json_object_set(o, "label", json_string(lbl));
            json_array_append(out, o);
            shown++;
        }
        int hidden = (int)json_array_size(counts) - shown;
        if (hidden > 0) {
            json_t *o = json_new_object();
            json_object_set(o, "glyph", json_string("·"));
            json_object_set(o, "color", json_string(""));
            char lbl[32]; snprintf(lbl, sizeof(lbl), "+%d", hidden);
            json_object_set(o, "label", json_string(lbl));
            json_array_append(out, o);
        }
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (cmap) json_free(cmap);
    if (counts) json_free(counts);
    free(cmap_json); free(counts_json);
    json_free(out);
    return ret;
}

/* ── _bucket_category ───────────────────────────────────────────────────── */

/* PoP: learning_graph_render_bucket_category @ agent/learning_graph_render.py:_bucket_category */
char *learning_graph_render_bucket_category(const char *bucket_json) {
    json_t *b = json_parse(bucket_json ? bucket_json : "{}", NULL);
    char best[64]; best[0] = 0; int bestc = -1;
    if (b) {
        json_t *nodes = json_object_get(b, "nodes");
        if (nodes && nodes->type == JSON_ARRAY) {
            json_t *counts = json_new_object();
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                if (strcmp(json_object_get_string(nd, "kind", ""), "memory") == 0) continue;
                const char *cat = json_object_get_string(nd, "category", "skill");
                double cur = json_object_get_number(counts, cat, 0);
                json_object_set(counts, cat, json_number(cur + 1));
            }
            for (size_t i = 0; i < json_object_size(counts); i++) {
                const char *key = json_object_get_key_at(counts, i);
                int c = (int)json_number_value(json_object_get_at(counts, i));
                if (c > bestc) { bestc = c; snprintf(best, sizeof(best), "%s", key ? key : ""); }
            }
            json_free(counts);
        }
    }
    char *ret = lg_strdup(best);
    if (b) json_free(b);
    return ret;
}

/* ── _trajectory_row ────────────────────────────────────────────────────── */

static const char *lg_run_text(const json_t *run) {
    return run ? json_string_value(json_array_get(run, 0)) : "";
}

/* bucket total from a json_t* bucket (dumps + frees internally) */
static int lg_bucket_total_json(const json_t *b) {
    char *s = json_dumps(b, 0);
    int t = learning_graph_render_bucket_total(s);
    if (s) free(s);
    return t;
}

/* PoP: learning_graph_render_trajectory_row @ agent/learning_graph_render.py:_trajectory_row */
char *learning_graph_render_trajectory_row(const char *buckets_json,
                                           int width, double reveal) {
    json_t *bs = json_parse(buckets_json ? buckets_json : "[]", NULL);
    json_t *out = json_new_array();
    if (bs && bs->type == JSON_ARRAY && json_array_size(bs) > 0) {
        int total = 0;
        for (size_t i = 0; i < json_array_size(bs); i++)
            total += lg_bucket_total_json(json_array_get(bs, i));
        if (total < 1) total = 1;
        int nb = (int)json_array_size(bs);
        int visible = (int)learning_graph_render_clamp(ceil(reveal * nb), 0, nb);
        int *points = calloc(visible > 0 ? visible : 1, sizeof(int));
        int acc = 0;
        for (int i = 0; i < visible; i++) {
            acc += lg_bucket_total_json(json_array_get(bs, i));
            points[i] = (int)round(((double)acc / (double)total) * (width - 1));
        }
        /* Per-slot mark array (0=space,1=dot,2=star); emit as a UTF-8 string
         * where dot/star glyphs occupy more bytes than a space. A fixed
         * width-byte buffer would overflow on the multi-byte glyphs. */
        char *mark = calloc(width > 0 ? width : 1, sizeof(char));
        int lastp = 0;
        for (int i = 0; i < visible; i++) {
            int p = points[i];
            int lo = lastp < p ? lastp : p;
            int hi = lastp < p ? p : lastp;
            for (int x = lo; x <= hi; x++)
                if (x >= 0 && x < width && mark[x] == 0) mark[x] = 1;
            if (p >= 0 && p < width) mark[p] = 2;
            lastp = p;
        }
        size_t cap = (size_t)(width > 0 ? width : 1) * 4 + 1;
        char *cells = malloc(cap);
        size_t len = 0;
        for (int x = 0; x < width; x++) {
            if (mark[x] == 2) { memcpy(cells + len, "\xe2\x9c\xa8", 3); len += 3; } /* ✦ */
            else if (mark[x] == 1) { memcpy(cells + len, "\xc2\xb7", 2); len += 2; } /* · */
            else { cells[len++] = ' '; }
        }
        cells[len] = '\0';
        free(mark);

        json_t *r1 = json_new_array();
        json_array_append(r1, json_string("trajectory "));
        json_array_append(r1, json_string("label"));
        json_array_append(r1, json_number(0.55));
        json_t *r2 = json_new_array();
        json_array_append(r2, json_string(cells));
        json_array_append(r2, json_string("skill"));
        json_array_append(r2, json_number(0.48));
        json_array_append(out, r1);
        json_array_append(out, r2);
        free(points); free(cells);
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (bs) json_free(bs);
    json_free(out);
    return ret;
}

/* ── render_graph ───────────────────────────────────────────────────────── */

static void lg_add_run(json_t *row, const char *text, const char *style,
                       double alpha, const char *hex) {
    json_t *run = json_new_array();
    json_array_append(run, json_string(text ? text : ""));
    json_array_append(run, json_string(style ? style : "label"));
    json_array_append(run, json_number(alpha));
    if (hex && *hex) json_array_append(run, json_string(hex));
    json_array_append(row, run);
}

/* PoP: learning_graph_render_graph @ agent/learning_graph_render.py:render_graph */
char *learning_graph_render_graph(const char *payload_json, int cols, int rows,
                                  double reveal) {
    reveal = learning_graph_render_clamp(reveal, 0.0, 1.0);
    if (cols < 44) cols = 44;
    if (rows < 14) rows = 14;
    json_t *payload = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *nodes = payload ? json_object_get(payload, "nodes") : NULL;

    json_t *grid = json_new_array();
    if (!payload || !nodes || nodes->type != JSON_ARRAY || json_array_size(nodes) == 0) {
        json_t *ph = json_new_array();
        lg_add_run(ph, "no learning yet — keep using Hermes and it maps out here", "dim", 0.7, NULL);
        json_array_append(grid, ph);
        json_t *out = json_new_object();
        json_object_set(out, "grid", grid);
        json_object_set(out, "date", json_string(""));
        json_object_set(out, "reveal", json_number(reveal));
        json_object_set(out, "visible", json_int(0));
        json_object_set(out, "labels", json_new_array());
        char *dump = json_dumps(out, 0);
        char *ret = lg_strdup(dump ? dump : "{}");
        if (dump) free(dump);
        json_free(payload);   /* grid and labels are owned by out */
        json_free(out);
        return ret;
    }

    char *nodes_json = json_dumps(nodes, 0);
    char *rec_json = learning_graph_render_compute_recency(nodes_json);
    char *cmap_json = learning_graph_render_category_color_map(payload_json);
    char *buckets_json = learning_graph_render_build_chart_buckets(
        nodes_json, rec_json, (rows - 3) > 4 ? (rows - 3) : 4);
    json_t *buckets = json_parse(buckets_json, NULL);
    json_t *rec = json_parse(rec_json, NULL);
    json_t *cmap = json_parse(cmap_json, NULL);

    int n_buckets = buckets ? (int)json_array_size(buckets) : 0;
    int vis_count = (int)learning_graph_render_clamp(ceil(reveal * n_buckets), 0, n_buckets);
    int max_total = 1;
    if (buckets) for (size_t i = 0; i < json_array_size(buckets); i++) {
        int t = lg_bucket_total_json(json_array_get(buckets, i));
        if (t > max_total) max_total = t;
    }
    int max_label = 0;
    if (buckets) for (size_t i = 0; i < json_array_size(buckets); i++) {
        int l = (int)strlen(json_object_get_string(json_array_get(buckets, i), "label", ""));
        if (l > max_label) max_label = l;
    }
    int label_w = max_label < 9 ? max_label : 9;   /* matches Python min(9, maxlen) */
    int bar_w = (cols - label_w - 16) > 14 ? (cols - label_w - 16) : 14;

    const char *label_keys = "123456789abc";
    json_t *labels = json_new_array();
    int visible = 0;

    if (buckets) for (int i = 0; i < n_buckets; i++) {
        if (i >= vis_count) { json_array_append(grid, json_new_array()); continue; }
        const json_t *bucket = json_array_get(buckets, i);
        visible += lg_bucket_total_json(bucket);
        double ink = learning_graph_render_recency_ink(json_object_get_number(bucket, "rec", 1.0));
        int total = lg_bucket_total_json(bucket);
        int bar_len = total ? (int)round(((double)total / (double)max_total) * bar_w) : 0;
        if (bar_len < 1 && total) bar_len = 1;
        int skill_len = total ? (int)round(((double)json_object_get_number(bucket, "skills", 0) / (double)total) * bar_len) : 0;
        if (json_object_get_number(bucket, "skills", 0) > 0 && skill_len == 0) skill_len = 1;
        int memory_len = bar_len - skill_len;
        if (json_object_get_number(bucket, "memories", 0) > 0 && memory_len == 0 && bar_len > 1) {
            memory_len = 1; skill_len = bar_len - 1;
        }
        char *bln_dump = json_dumps(bucket, 0);
        char *bln_json = learning_graph_render_bucket_label_node(bln_dump);
        free(bln_dump);
        json_t *node = json_parse(bln_json, NULL);
        free(bln_json);
        char marker = 0;
        const char *style = "skill";
        if (node && node->type != JSON_NULL && json_array_size(labels) < LGR_MAX_LABELS) {
            marker = label_keys[json_array_size(labels)];
            style = strcmp(json_object_get_string(node, "kind", ""), "memory") == 0 ? "memory" : "skill";
        }
        char *cat_dump = json_dumps(bucket, 0);
        char *cat = learning_graph_render_bucket_category(cat_dump);
        free(cat_dump);
        const char *cat_hex = (cmap && cat && *cat) ? json_object_get_string(cmap, cat, NULL) : NULL;

        json_t *row = json_new_array();
        char lw[32]; snprintf(lw, sizeof(lw), "%*s ", label_w, json_object_get_string(bucket, "label", ""));
        lg_add_run(row, lw, "label", ink, NULL);
        lg_add_run(row, "│ ", "dim", 0.55, NULL);
        if (marker) {
            char m[2] = { marker, 0 };
            lg_add_run(row, m, "label", 0.95, NULL);
        } else if (total) {
            int sk = (int)json_object_get_number(bucket, "skills", 0);
            const char *glyph = sk ? "✦" : "◆";
            lg_add_run(row, glyph, sk ? "skill" : "memory", ink, sk ? cat_hex : NULL);
        }
        if (skill_len) {
            char *bar = calloc(skill_len + 1, 1);
            for (int x = 0; x < skill_len; x++) bar[x] = '━';
            lg_add_run(row, bar, "skill", ink, cat_hex);
            free(bar);
        }
        if (memory_len) {
            char *mbar = calloc(memory_len + 1, 1);
            if (memory_len == 1) mbar[0] = '◆';
            else { mbar[0] = '◆'; for (int x = 1; x < memory_len - 1; x++) mbar[x] = '━'; mbar[memory_len - 1] = '◆'; }
            lg_add_run(row, mbar, "memory", ink > 0.65 ? ink : 0.65, NULL);
            free(mbar);
        }
        if (bar_len < bar_w) {
            char *sp = calloc(bar_w - bar_len + 1, 1);
            for (int x = 0; x < bar_w - bar_len; x++) sp[x] = ' ';
            lg_add_run(row, sp, "bg", 1.0, NULL);
            free(sp);
        }
        lg_add_run(row, "  ", "bg", 1.0, NULL);
        char skc[16]; snprintf(skc, sizeof(skc), "%d", (int)json_object_get_number(bucket, "skills", 0));
        lg_add_run(row, skc, "skill", ink > 0.72 ? ink : 0.72, NULL);
        if (json_object_get_number(bucket, "memories", 0) > 0) {
            lg_add_run(row, "+", "dim", 0.6, NULL);
            char mc[16]; snprintf(mc, sizeof(mc), "%d", (int)json_object_get_number(bucket, "memories", 0));
            lg_add_run(row, mc, "memory", ink > 0.72 ? ink : 0.72, NULL);
        }
        if (i == vis_count - 1) {
            lg_add_run(row, "  ◀ now", "label", 0.9, NULL);
        } else if (total == max_total && max_total > 1) {
            lg_add_run(row, "  ☄ peak", "label", 0.75, NULL);
        }
        json_array_append(grid, row);

        if (marker && node && node->type != JSON_NULL) {
            char *nlabel = learning_graph_render_node_label(
                json_object_get_string(node, "label", ""), json_object_get_string(node, "id", ""));
            char *nmeta = learning_graph_render_node_meta(node);
            json_t *lo = json_new_object();
            char m[2] = { marker, 0 };
            json_object_set(lo, "key", json_string(m));
            json_object_set(lo, "glyph", json_string(lg_node_glyph(node)));
            json_object_set(lo, "label", json_string(nlabel ? nlabel : ""));
            json_object_set(lo, "meta", json_string(nmeta ? nmeta : ""));
            json_object_set(lo, "style", json_string(style));
            json_object_set(lo, "alpha", json_number(round(ink * 1000) / 1000.0));
            json_array_append(labels, lo);
            if (nlabel) free(nlabel);
            if (nmeta) free(nmeta);
        }
        if (cat) free(cat);
        if (node) json_free(node);
    }

    /* trajectory row */
    json_t *trow = json_new_array();
    char *trows_json = learning_graph_render_trajectory_row(buckets_json,
        (cols - label_w - 13) > 12 ? (cols - label_w - 13) : 12, reveal);
    json_t *trows = json_parse(trows_json, NULL);
    free(trows_json);
    lg_add_run(trow, "  ", "bg", 1.0, NULL);
    if (trows && trows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(trows); i++) {
            const json_t *tr = json_array_get(trows, i);
            if (tr && tr->type == JSON_ARRAY && json_array_size(tr) >= 3) {
                const char *text = lg_run_text(tr);
                const char *st = json_string_value(json_array_get(tr, 1));
                double al = json_number_value(json_array_get(tr, 2));
                const char *hx = json_array_size(tr) >= 4 ? json_string_value(json_array_get(tr, 3)) : NULL;
                lg_add_run(trow, text, st, al, hx);
            }
        }
    }
    json_array_append(grid, trow);
    if (trows) json_free(trows);

    json_t *out = json_new_object();
    json_object_set(out, "grid", grid);
    char *date = learning_graph_render_date_at(rec_json, reveal);
    json_object_set(out, "date", json_string(date ? date : ""));
    free(date);
    json_object_set(out, "reveal", json_number(reveal));
    json_object_set(out, "visible", json_int(visible));
    json_object_set(out, "labels", labels);

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "{}");

    if (dump) free(dump);
    json_free(payload);
    if (rec) json_free(rec);
    if (cmap) json_free(cmap);
    if (buckets) json_free(buckets);
    free(nodes_json); free(rec_json); free(cmap_json); free(buckets_json);
    /* grid and labels are owned by `out` (json_object_set steals the ref) */
    json_free(out);
    return ret;
}

/* ── build_legend ───────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_build_legend @ agent/learning_graph_render.py:build_legend */
char *learning_graph_render_build_legend(const char *payload_json) {
    json_t *p = json_parse(payload_json ? payload_json : "{}", NULL);
    int skills = 0, memories = 0;
    if (p) {
        json_t *nodes = json_object_get(p, "nodes");
        if (nodes && nodes->type == JSON_ARRAY)
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                if (strcmp(json_object_get_string(nd, "kind", ""), "memory") == 0) memories++;
                else skills++;
            }
    }
    json_t *out = json_new_array();
    json_t *s = json_new_object();
    json_object_set(s, "glyph", json_string("●"));
    json_object_set(s, "style", json_string("skill"));
    char sl[32]; snprintf(sl, sizeof(sl), "skills (%d)", skills);
    json_object_set(s, "label", json_string(sl));
    json_array_append(out, s);
    json_t *m = json_new_object();
    json_object_set(m, "glyph", json_string("◆"));
    json_object_set(m, "style", json_string("memory"));
    char ml[32]; snprintf(ml, sizeof(ml), "memories (%d)", memories);
    json_object_set(m, "label", json_string(ml));
    json_array_append(out, m);

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    json_free(p); json_free(out);
    return ret;
}

/* ── axis_labels ────────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_axis_labels @ agent/learning_graph_render.py:axis_labels */
char *learning_graph_render_axis_labels(const char *payload_json) {
    json_t *p = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *nodes = p ? json_object_get(p, "nodes") : NULL;
    char *nodes_json = nodes ? json_dumps(nodes, 0) : NULL;
    char *rec_json = learning_graph_render_compute_recency(nodes_json ? nodes_json : "[]");
    if (nodes_json) free(nodes_json);
    json_t *rec = json_parse(rec_json, NULL);
    json_t *out = json_new_object();
    if (rec && !json_object_get_bool(rec, "timed", 0)) {
        json_object_set(out, "start", json_string("oldest"));
        json_object_set(out, "end", json_string("now"));
    } else {
        char *s = learning_graph_render_format_date(rec ? json_object_get_number(rec, "minTs", 0) : 0);
        char *e = learning_graph_render_format_date(rec ? json_object_get_number(rec, "maxTs", 0) : 0);
        json_object_set(out, "start", json_string(s ? s : "unknown"));
        json_object_set(out, "end", json_string(e ? e : "unknown"));
        free(s); free(e);
    }
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "{}");
    if (dump) free(dump);
    json_free(p);
    json_free(rec);
    free(rec_json);
    json_free(out);
    return ret;
}

/* ── _peak_day ──────────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_peak_day @ agent/learning_graph_render.py:_peak_day */
char *learning_graph_render_peak_day(const char *payload_json) {
    json_t *p = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *counts = json_new_object();
    json_t *reps = json_new_object();
    if (p) {
        json_t *nodes = json_object_get(p, "nodes");
        if (nodes && nodes->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(nodes); i++) {
                const json_t *nd = json_array_get(nodes, i);
                double ts; if (!lg_ts(nd, &ts)) continue;
                char *key = learning_graph_render_period_key(ts, "day");
                double cur = json_object_get_number(counts, key, 0);
                json_object_set(counts, key, json_number(cur + 1));
                json_object_set(reps, key, json_number(ts));
                free(key);
            }
        }
    }
    char best[64]; best[0] = 0; int bestc = -1;
    for (size_t i = 0; i < json_object_size(counts); i++) {
        const char *key = json_object_get_key_at(counts, i);
        int c = (int)json_number_value(json_object_get_at(counts, i));
        if (c > bestc) { bestc = c; snprintf(best, sizeof(best), "%s", key ? key : ""); }
    }
    char *ret;
    if (bestc < 0) {
        ret = lg_strdup("");
    } else {
        double ts = json_object_get_number(reps, best, 0);
        char *label = learning_graph_render_period_label(ts, "day");
        size_t n = strlen(label ? label : "") + 64;
        char *buf = malloc(n);
        snprintf(buf, n, "busiest day %s · %d learned", label ? label : "", bestc);
        ret = buf;
        free(label);
    }
    json_free(p); json_free(counts); json_free(reps);
    return ret;
}

/* ── _merge_runs ────────────────────────────────────────────────────────── */

typedef struct { char *text; char *style; double alpha; char *hex; } run_t;

/* PoP: learning_graph_render_merge_runs @ agent/learning_graph_render.py:_merge_runs */
char *learning_graph_render_merge_runs(const char *runs_json) {
    json_t *runs = json_parse(runs_json ? runs_json : "[]", NULL);
    run_t *acc = NULL;
    size_t nacc = 0, cap = 0;
    if (runs && runs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(runs); i++) {
            const json_t *run = json_array_get(runs, i);
            if (!run || run->type != JSON_ARRAY || json_array_size(run) < 3) continue;
            const char *text = json_string_value(json_array_get(run, 0));
            const char *style = json_string_value(json_array_get(run, 1));
            double alpha = json_number_value(json_array_get(run, 2));
            const char *hx = json_array_size(run) >= 4 ? json_string_value(json_array_get(run, 3)) : NULL;
            if (nacc > 0) {
                run_t *prev = &acc[nacc - 1];
                int same_style = prev->style && style && strcmp(prev->style, style) == 0;
                int same_alpha = fabs(prev->alpha - alpha) < 1e-6;
                int same_hex = (prev->hex == NULL && hx == NULL) ||
                               (prev->hex && hx && strcmp(prev->hex, hx) == 0);
                if (same_style && same_alpha && same_hex) {
                    char *nt = malloc(strlen(prev->text) + strlen(text ? text : "") + 1);
                    snprintf(nt, strlen(prev->text) + strlen(text ? text : "") + 1, "%s%s", prev->text, text ? text : "");
                    free(prev->text);
                    prev->text = nt;
                    continue;
                }
            }
            if (nacc >= cap) { cap = cap ? cap * 2 : 8; acc = realloc(acc, cap * sizeof(run_t)); }
            acc[nacc].text = lg_strdup(text ? text : "");
            acc[nacc].style = lg_strdup(style ? style : "");
            acc[nacc].alpha = alpha;
            acc[nacc].hex = (hx && *hx) ? lg_strdup(hx) : NULL;
            nacc++;
        }
    }
    json_t *out = json_new_array();
    for (size_t i = 0; i < nacc; i++) {
        json_t *r = json_new_array();
        json_array_append(r, json_string(acc[i].text ? acc[i].text : ""));
        json_array_append(r, json_string(acc[i].style ? acc[i].style : ""));
        json_array_append(r, json_number(acc[i].alpha));
        if (acc[i].hex) json_array_append(r, json_string(acc[i].hex));
        json_array_append(out, r);
        free(acc[i].text); free(acc[i].style); if (acc[i].hex) free(acc[i].hex);
    }
    free(acc);
    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    json_free(runs); json_free(out);
    return ret;
}

/* ── _bucket_rows ──────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_bucket_rows @ agent/learning_graph_render.py:_bucket_rows */
char *learning_graph_render_bucket_rows(const char *buckets_json,
                                        const char *payload_json) {
    json_t *bs = json_parse(buckets_json ? buckets_json : "[]", NULL);
    json_t *payload = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *out = json_new_array();

    char *cmap_json = learning_graph_render_category_color_map(payload_json ? payload_json : "{}");
    json_t *cmap = json_parse(cmap_json, NULL);

    /* memory_lookup: "memory:<source>:<idx>" -> card */
    json_t *lookup = json_new_object();
    if (payload) {
        json_t *mem = json_object_get(payload, "memory");
        if (mem && mem->type == JSON_ARRAY) {
            for (size_t idx = 0; idx < json_array_size(mem); idx++) {
                json_t *card = json_array_get(mem, idx);
                if (!card || card->type != JSON_OBJECT) continue;
                const char *src = json_object_get_string(card, "source", "");
                char key[64]; snprintf(key, sizeof(key), "memory:%s:%zu", src, idx);
                json_object_set(lookup, key, json_copy(card));
            }
        }
    }

    if (bs && bs->type == JSON_ARRAY) {
        for (size_t idx = 0; idx < json_array_size(bs); idx++) {
            const json_t *bucket = json_array_get(bs, idx);
            char *bjson = json_dumps(bucket, 0);
            char *blncat = learning_graph_render_bucket_category(bjson);
            const char *cat = (blncat && *blncat) ? blncat : NULL;
            const char *color = (cmap && cat) ? json_object_get_string(cmap, cat, NULL) : NULL;
            char *lookup_json = json_dumps(lookup, 0);
            char *bnodes_json = learning_graph_render_bucket_nodes(bjson, lookup_json);
            free(lookup_json);
            json_t *bnodes = json_parse(bnodes_json, NULL);

            json_t *row = json_new_object();
            json_object_set(row, "index", json_int((int)idx));
            json_object_set(row, "label", json_string(json_object_get_string(bucket, "label", "")));
            char *dfmt = learning_graph_render_format_date(json_object_get_number(bucket, "ts", 0));
            json_object_set(row, "date", json_string(dfmt ? dfmt : "unknown"));
            free(dfmt);
            json_object_set(row, "skills", json_int((int)json_object_get_number(bucket, "skills", 0)));
            json_object_set(row, "memories", json_int((int)json_object_get_number(bucket, "memories", 0)));
            json_object_set(row, "total", json_int(learning_graph_render_bucket_total(bjson)));
            json_object_set(row, "category", json_string(cat ? cat : ""));
            json_object_set(row, "color", json_string(color ? color : ""));
            json_object_set(row, "nodes", bnodes ? bnodes : json_new_array());

            json_array_append(out, row);
            free(bjson); free(blncat); free(bnodes_json);
        }
    }

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "[]");
    if (dump) free(dump);
    if (bs) json_free(bs);
    if (payload) json_free(payload);
    if (cmap) json_free(cmap);
    free(cmap_json);
    json_free(lookup);
    json_free(out);
    return ret;
}

/* ── render_frames ─────────────────────────────────────────────────────── */

/* PoP: learning_graph_render_frames @ agent/learning_graph_render.py:render_frames */
char *learning_graph_render_frames(const char *payload_json, int cols, int rows,
                                   int frames) {
    if (frames < 2) frames = 2;
    if (frames > 240) frames = 240;
    json_t *payload = json_parse(payload_json ? payload_json : "{}", NULL);
    json_t *nodes = payload ? json_object_get(payload, "nodes") : NULL;
    char *nodes_json = nodes ? json_dumps(nodes, 0) : NULL;
    char *rec_json = learning_graph_render_compute_recency(nodes_json ? nodes_json : "[]");
    char *buckets_json = learning_graph_render_build_chart_buckets(
        nodes_json ? nodes_json : "[]", rec_json, (rows - 3) > 4 ? (rows - 3) : 4);

    json_t *out_frames = json_new_array();
    for (int i = 0; i < frames; i++) {
        double reveal = (double)i / (double)(frames - 1);
        char *frame_json = learning_graph_render_graph(payload_json, cols, rows, reveal);
        json_t *frame = json_parse(frame_json, NULL);
        free(frame_json);
        json_t *o = json_new_object();
        json_object_set(o, "reveal", json_number(json_object_get_number(frame, "reveal", reveal)));
        char *d = learning_graph_render_date_at(rec_json, reveal);
        json_object_set(o, "date", json_string(d ? d : ""));
        free(d);
        json_object_set(o, "visible", json_number(json_object_get_number(frame, "visible", 0)));
        json_t *fg = json_object_get(frame, "grid");
        json_object_set(o, "grid", fg ? json_copy(fg) : json_new_array());
        json_t *fl = json_object_get(frame, "labels");
        json_object_set(o, "labels", fl ? json_copy(fl) : json_new_array());
        json_array_append(out_frames, o);
        if (frame) json_free(frame);
    }
    char *legend_json = learning_graph_render_build_legend(payload_json);
    char *cats_json = learning_graph_render_category_legend(payload_json, 4);
    char *brows_json = learning_graph_render_bucket_rows(buckets_json, payload_json);
    char *axis_json = learning_graph_render_axis_labels(payload_json);

    json_t *out = json_new_object();
    json_object_set(out, "frames", out_frames);
    json_t *leg = json_parse(legend_json, NULL); json_object_set(out, "legend", leg ? leg : json_new_array());
    json_t *cat = json_parse(cats_json, NULL); json_object_set(out, "categories", cat ? cat : json_new_array());
    json_t *br = json_parse(brows_json, NULL); json_object_set(out, "buckets", br ? br : json_new_array());
    json_object_set(out, "summary", json_new_array());
    json_t *ax = json_parse(axis_json, NULL); json_object_set(out, "axis", ax ? ax : json_new_object());
    json_object_set(out, "count", json_int(nodes ? (int)json_array_size(nodes) : 0));
    json_object_set(out, "cols", json_int(cols));
    json_object_set(out, "rows", json_int(rows));

    char *dump = json_dumps(out, 0);
    char *ret = lg_strdup(dump ? dump : "{}");

    if (dump) free(dump);
    json_free(payload);
    if (nodes_json) free(nodes_json);
    free(rec_json); free(buckets_json);
    free(legend_json); free(cats_json); free(brows_json); free(axis_json);
    json_free(out);
    return ret;
}
