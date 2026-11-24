/*
 * provider.c - real C11 port of upstream Hermes plugins/web/xai/provider.py.
 * Pure helpers only: config seam, domain-filter coercion, tool spec, prompt builder and the
 * three-stage result extraction (JSON block -> url_citation annotations -> citations).
 * The live HTTP/OAuth wrappers (search, _post_responses, is_available) stay in Python.
 *
 * PoP: w19_provider_3254dd_coerce_domain_list @ plugins/web/xai/provider.py:_coerce_domain_list
 * PoP: w19_provider_3254dd_coerce @ plugins/web/xai/provider.py:_coerce
 * PoP: w19_provider_3254dd_web_search_tool @ plugins/web/xai/provider.py:_web_search_tool
 * PoP: w19_provider_3254dd_build_prompt @ plugins/web/xai/provider.py:_build_prompt
 * PoP: w19_provider_3254dd_collect_output_text @ plugins/web/xai/provider.py:_collect_output_text
 * PoP: w19_provider_3254dd_try_parse_json_results @ plugins/web/xai/provider.py:_try_parse_json_results
 * PoP: w19_provider_3254dd_results_from_annotations @ plugins/web/xai/provider.py:_results_from_annotations
 * PoP: w19_provider_3254dd_extract_results @ plugins/web/xai/provider.py:_extract_results
 * PoP: w19_provider_3254dd_web_config @ plugins/web/xai/provider.py:_load_xai_web_config
 * PoP: w19_provider_3254dd_limit_clamp @ plugins/web/xai/provider.py:search
 */
#include "hermes_json.h"
#include "noc_w19_provider_3254dd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DOMAIN_FILTERS 5

static char *dupstr(const char *s) {
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s) + 1;
    o = (char *)malloc(n);
    if (!o) return NULL;
    memcpy(o, s, n);
    return o;
}

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() copy. */
static char *py_strip_dup(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *o;
    while (*b && py_isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && py_isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, b, n);
    o[n] = '\0';
    return o;
}

static void append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        size_t nc = (*cap ? *cap * 2 : 128);
        char *nb;
        while (nc < *len + n + 1) nc *= 2;
        nb = (char *)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb;
        *cap = nc;
    }
    memcpy(*buf + *len, s, n + 1);
    *len += n;
}

/* Python repr() of a JSON node into a growing buffer (containers quote strings). */
static void repr_node(const json_t *v, char **buf, size_t *len, size_t *cap, int quoted);

/* Python str()/repr() of a number: integral values in plain form, else shortest %g. */
static void num_repr(double d, char **buf, size_t *len, size_t *cap) {
    char tmp[80];
    if (d != d) { append_str(buf, len, cap, "nan"); return; }
    if (d == (double)(long long)d && fabs(d) < 1e15) {
        snprintf(tmp, sizeof tmp, "%lld", (long long)d);
    } else if (d > 1.7976931348623157e308) {
        append_str(buf, len, cap, "inf");
        return;
    } else if (d < -1.7976931348623157e308) {
        append_str(buf, len, cap, "-inf");
        return;
    } else {
        snprintf(tmp, sizeof tmp, "%.15g", d);
        if (!strpbrk(tmp, ".eE")) {
            size_t l = strlen(tmp);
            snprintf(tmp + l, sizeof tmp - l, ".0");
        }
    }
    append_str(buf, len, cap, tmp);
}

static void repr_node(const json_t *v, char **buf, size_t *len, size_t *cap, int quoted) {
    size_t i;
    if (!v || v->type == JSON_NULL) { append_str(buf, len, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: append_str(buf, len, cap, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: num_repr(v->num_val, buf, len, cap); return;
        case JSON_STRING:
            if (quoted) append_str(buf, len, cap, "'");
            append_str(buf, len, cap, v->str_val ? v->str_val : "");
            if (quoted) append_str(buf, len, cap, "'");
            return;
        case JSON_ARRAY:
            append_str(buf, len, cap, "[");
            for (i = 0; i < json_len(v); i++) {
                if (i) append_str(buf, len, cap, ", ");
                repr_node(json_get(v, i), buf, len, cap, 1);
            }
            append_str(buf, len, cap, "]");
            return;
        case JSON_OBJECT:
            append_str(buf, len, cap, "{");
            for (i = 0; i < json_object_size(v); i++) {
                if (i) append_str(buf, len, cap, ", ");
                append_str(buf, len, cap, "'");
                append_str(buf, len, cap, json_object_get_key_at(v, i));
                append_str(buf, len, cap, "': ");
                repr_node(json_object_get_at(v, i), buf, len, cap, 1);
            }
            append_str(buf, len, cap, "}");
            return;
        default: return;
    }
}

/* Python str(v) (top-level strings unquoted); caller frees. */
static char *py_str_of(const json_t *v) {
    char *buf = NULL;
    size_t len = 0, cap = 0;
    repr_node(v, &buf, &len, &cap, 0);
    if (!buf) return dupstr("");
    return buf;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default: return 0;
    }
}

/* Python slice [:limit] length for n items. */
static size_t slice_len(size_t n, long long limit) {
    if (limit >= 0) return (size_t)limit < n ? (size_t)limit : n;
    {
        long long k = (long long)n + limit;
        return k > 0 ? (size_t)k : 0;
    }
}

/* ---- Python int(value) ---- */
static int py_int_coerce(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d || d > 9.0e18 || d < -9.0e18) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p;
        int neg = 0, digits = 0, prev_digit = 0;
        long long acc = 0;
        while (*s && py_isspace((unsigned char)*s)) s++;
        p = s;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        while (*p) {
            if (*p >= '0' && *p <= '9') {
                if (acc > (9223372036854775807LL - (*p - '0')) / 10) return 0;
                acc = acc * 10 + (*p - '0');
                digits = 1;
                prev_digit = 1;
                p++;
            } else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
                prev_digit = 0;
                p++;
            } else {
                break;
            }
        }
        if (!digits) return 0;
        while (*p && py_isspace((unsigned char)*p)) p++;
        if (*p) return 0;
        *out = neg ? -acc : acc;
        return 1;
    }
    return 0;
}

/* ---- Python float(value) ---- */
static int py_float_coerce(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double d;
        while (*s && py_isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        d = strtod(s, &end);
        if (end == s) return 0;
        while (*end && py_isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = d;
        return 1;
    }
    return 0;
}

json_t *w19_provider_3254dd_coerce_domain_list(const json_t *value) {
    json_t *out = json_array();
    size_t i;
    if (!value || value->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(value) && json_len(out) < MAX_DOMAIN_FILTERS; i++) {
        json_t *item = json_get(value, i);
        char *stripped;
        if (!item || item->type != JSON_STRING) continue;
        stripped = py_strip_dup(item->str_val);
        if (stripped && stripped[0]) json_append(out, json_string(stripped));
        free(stripped);
    }
    return out;
}

json_t *w19_provider_3254dd_coerce(const char *cast, const json_t *value, const json_t *def) {
    if (cast && !strcmp(cast, "int")) {
        long long iv;
        if (py_int_coerce(value, &iv)) return json_number((double)iv);
        return def ? json_copy((json_t *)def) : json_null();
    }
    if (cast && !strcmp(cast, "float")) {
        double dv;
        if (py_float_coerce(value, &dv)) return json_number(dv);
        return def ? json_copy((json_t *)def) : json_null();
    }
    if (cast && !strcmp(cast, "str")) {
        char *s = py_str_of(value);
        json_t *node = json_string(s ? s : "");
        free(s);
        return node;
    }
    return def ? json_copy((json_t *)def) : json_null();
}

json_t *w19_provider_3254dd_web_search_tool(const json_t *cfg) {
    const char *keys[2] = {"allowed_domains", "excluded_domains"};
    json_t *out;
    json_t *filters = json_object();
    int present = 0, i;
    for (i = 0; i < 2; i++) {
        json_t *raw = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, keys[i]) : NULL;
        json_t *list = w19_provider_3254dd_coerce_domain_list(raw);
        if (json_len(list) > 0) {
            json_set(filters, keys[i], list);
            present++;
        } else {
            json_free(list);
        }
    }
    if (present == 2) {
        json_free(filters);
        return json_null();
    }
    out = json_object();
    json_set(out, "type", json_string("web_search"));
    if (present) json_set(out, "filters", filters);
    else json_free(filters);
    return out;
}

char *w19_provider_3254dd_build_prompt(const char *query, long long limit) {
    char *buf = NULL;
    size_t len = 0, cap = 0;
    char tail[64];
    append_str(&buf, &len, &cap,
        "Use the web_search tool to find current information for the query below, then respond with ONLY a single "
        "JSON object \xE2\x80\x94 no prose, no markdown fences, no inline citation links \xE2\x80\x94 matching this "
        "exact schema:\n\n");
    append_str(&buf, &len, &cap,
        "{\"results\": [{\"title\": \"string\", \"url\": \"string\", \"description\": \"1-2 sentence summary\"}]}"
        "\n\nReturn at most ");
    snprintf(tail, sizeof tail, "%lld", limit);
    append_str(&buf, &len, &cap, tail);
    append_str(&buf, &len, &cap,
        " results, ordered by relevance, with absolute https:// URLs. If no usable results exist, return "
        "{\"results\": []}.\n\nQuery: ");
    append_str(&buf, &len, &cap, query ? query : "");
    if (!buf) return dupstr("");
    return buf;
}

json_t *w19_provider_3254dd_collect_output_text(const json_t *response_data) {
    json_t *text_blocks = json_array();
    json_t *annotations = json_array();
    json_t *out = json_array();
    json_t *output = (response_data && response_data->type == JSON_OBJECT)
                   ? json_obj_get(response_data, "output") : NULL;
    size_t i, j;
    if (output && output->type == JSON_ARRAY) {
        for (i = 0; i < json_len(output); i++) {
            json_t *item = json_get(output, i);
            json_t *content, *anns;
            if (!item || item->type != JSON_OBJECT) continue;
            {
                json_t *itype = json_obj_get(item, "type");
                if (!itype || itype->type != JSON_STRING || strcmp(itype->str_val, "message")) continue;
            }
            content = json_obj_get(item, "content");
            if (!content || content->type != JSON_ARRAY) continue;
            for (j = 0; j < json_len(content); j++) {
                json_t *chunk = json_get(content, j);
                json_t *ctype, *text;
                if (!chunk || chunk->type != JSON_OBJECT) continue;
                ctype = json_obj_get(chunk, "type");
                if (!ctype || ctype->type != JSON_STRING || strcmp(ctype->str_val, "output_text")) continue;
                text = json_obj_get(chunk, "text");
                if (text && text->type == JSON_STRING && text->str_val) {
                    char *stripped = py_strip_dup(text->str_val);
                    if (stripped && stripped[0]) json_append(text_blocks, json_string(text->str_val));
                    free(stripped);
                }
                anns = json_obj_get(chunk, "annotations");
                if (anns && anns->type == JSON_ARRAY) {
                    size_t k;
                    for (k = 0; k < json_len(anns); k++) {
                        json_t *a = json_get(anns, k);
                        if (a && a->type == JSON_OBJECT) json_append(annotations, json_copy(a));
                    }
                }
            }
        }
    }
    json_append(out, text_blocks);
    json_append(out, annotations);
    return out;
}

/* Python dict.get(key, "") then str(): missing key means "", present null means "None". */
static char *row_get_str(const json_t *row, const char *key) {
    json_t *v = (row && row->type == JSON_OBJECT) ? json_obj_get(row, key) : NULL;
    if (!v) return dupstr("");
    return py_str_of(v);
}

/* Row {"title","url","description","position"} - the shared title_hit wire shape. */
static json_t *make_row(const char *title, const char *url, const char *description, size_t position) {
    json_t *row = json_object();
    json_set(row, "title", json_string(title ? title : ""));
    json_set(row, "url", json_string(url ? url : ""));
    json_set(row, "description", json_string(description ? description : ""));
    json_set(row, "position", json_number((double)position));
    return row;
}

json_t *w19_provider_3254dd_try_parse_json_results(const char *text, long long limit) {
    const char *first_brace, *last_brace;
    char *candidates[2] = {NULL, NULL};
    int ncand = 0, ci;
    json_t *result = NULL;
    if (!text) return json_null();
    first_brace = strchr(text, '{');
    last_brace = strrchr(text, '}');
    candidates[ncand++] = dupstr(text);
    if (first_brace && last_brace && last_brace > first_brace) {
        size_t n = (size_t)(last_brace - first_brace) + 1;
        char *block = (char *)malloc(n + 1);
        if (block) {
            memcpy(block, first_brace, n);
            block[n] = '\0';
            if (strcmp(block, text)) candidates[ncand++] = block;
            else free(block);
        }
    }
    for (ci = 0; ci < ncand && !result; ci++) {
        json_t *parsed = json_parse(candidates[ci], NULL);
        json_t *results, *normalized;
        size_t i, take;
        if (!parsed) continue;
        results = (parsed->type == JSON_OBJECT) ? json_obj_get(parsed, "results") : NULL;
        if (!results || results->type != JSON_ARRAY) { json_free(parsed); continue; }
        normalized = json_array();
        take = slice_len(json_len(results), limit);
        for (i = 0; i < take; i++) {
            json_t *row = json_get(results, i);
            char *url = NULL, *title = NULL, *desc = NULL, *raw = NULL;
            if (row && row->type == JSON_OBJECT) {
                raw = row_get_str(row, "url");
                url = py_strip_dup(raw);
                free(raw);
                raw = row_get_str(row, "title");
                title = py_strip_dup(raw);
                free(raw);
                raw = row_get_str(row, "description");
                desc = py_strip_dup(raw);
                free(raw);
            } else {
                url = dupstr("");
            }
            if (url && url[0]) json_append(normalized, make_row(title, url, desc, json_len(normalized) + 1));
            free(url);
            free(title);
            free(desc);
        }
        json_free(parsed);
        if (json_len(normalized) > 0) result = normalized;
        else json_free(normalized);
    }
    {
        int k;
        for (k = 0; k < ncand; k++) free(candidates[k]);
    }
    return result ? result : json_null();
}

json_t *w19_provider_3254dd_results_from_annotations(const json_t *annotations,
                                                     const char *joined_text,
                                                     long long limit) {
    json_t *results = json_array();
    size_t i;
    size_t jt_len = joined_text ? strlen(joined_text) : 0;
    json_t **seen = NULL;
    size_t seen_n = 0, seen_cap = 0;
    if (!annotations || annotations->type != JSON_ARRAY) return results;
    for (i = 0; i < json_len(annotations); i++) {
        json_t *ann = json_get(annotations, i);
        json_t *atype, *aurl, *start, *end;
        char *url = NULL;
        char *description = NULL;
        size_t k;
        int dup = 0;
        if (!ann || ann->type != JSON_OBJECT) continue;
        atype = json_obj_get(ann, "type");
        if (!(atype && atype->type == JSON_STRING && !strcmp(atype->str_val, "url_citation"))) continue;
        aurl = json_obj_get(ann, "url");
        {
            char *raw = row_get_str(ann, "url");
            url = py_strip_dup(raw);
            free(raw);
        }
        if (!url || !url[0]) { free(url); continue; }
        for (k = 0; k < seen_n; k++) {
            if (seen[k]->type == JSON_STRING && seen[k]->str_val && !strcmp(seen[k]->str_val, url)) {
                dup = 1;
                break;
            }
        }
        if (dup) { free(url); continue; }
        if (seen_n == seen_cap) {
            size_t ncap = seen_cap ? seen_cap * 2 : 8;
            json_t **nb = (json_t **)realloc(seen, ncap * sizeof *nb);
            if (!nb) { free(url); continue; }
            seen = nb;
            seen_cap = ncap;
        }
        seen[seen_n++] = aurl;
        description = dupstr("");
        start = json_obj_get(ann, "start_index");
        end = json_obj_get(ann, "end_index");
        if (start && end && start->type == JSON_NUMBER && end->type == JSON_NUMBER) {
            double sd = start->num_val, ed = end->num_val;
            if (sd == (double)(long long)sd && ed == (double)(long long)ed) {
                long long si = (long long)sd, ei = (long long)ed;
                if (0 <= si && si < ei && (unsigned long long)ei <= jt_len) {
                    long long from = si - 200;
                    char *slice;
                    size_t m;
                    if (from < 0) from = 0;
                    m = (size_t)(si - from);
                    slice = (char *)malloc(m + 1);
                    if (slice) {
                        memcpy(slice, joined_text + (size_t)from, m);
                        slice[m] = '\0';
                    }
                    free(description);
                    description = py_strip_dup(slice ? slice : "");
                    free(slice);
                    if (!description) description = dupstr("");
                    if (strlen(description) > 200) {
                        size_t off = strlen(description) - 200;
                        char *t2;
                        memmove(description, description + off, 201);
                        t2 = py_strip_dup(description);
                        free(description);
                        description = t2 ? t2 : dupstr("");
                    }
                }
            }
        }
        json_append(results, make_row("", url, description, json_len(results) + 1));
        free(url);
        free(description);
        if ((long long)json_len(results) >= limit) break;
    }
    free(seen);
    return results;
}

json_t *w19_provider_3254dd_extract_results(const json_t *response_data, long long limit) {
    json_t *collected = w19_provider_3254dd_collect_output_text(response_data);
    json_t *text_blocks = json_get(collected, 0);
    json_t *annotations = json_get(collected, 1);
    json_t *parsed = NULL;
    json_t *out = NULL;
    size_t i;
    for (i = 0; i < json_len(text_blocks) && !parsed; i++) {
        json_t *tb = json_get(text_blocks, i);
        json_t *p = w19_provider_3254dd_try_parse_json_results(tb->str_val, limit);
        if (py_truthy(p)) parsed = p;
        else json_free(p);
    }
    if (parsed) {
        json_free(collected);
        return parsed;
    }
    if (json_len(annotations) > 0) {
        char *joined = NULL;
        size_t len = 0, cap = 0;
        append_str(&joined, &len, &cap, "");
        for (i = 0; i < json_len(text_blocks); i++) {
            if (i) append_str(&joined, &len, &cap, "\n");
            append_str(&joined, &len, &cap, json_get(text_blocks, i)->str_val);
        }
        parsed = w19_provider_3254dd_results_from_annotations(annotations, joined ? joined : "", limit);
        free(joined);
        if (py_truthy(parsed)) {
            json_free(collected);
            return parsed;
        }
        json_free(parsed);
    }
    {
        json_t *citations = (response_data && response_data->type == JSON_OBJECT)
                          ? json_obj_get(response_data, "citations") : NULL;
        out = json_array();
        if (citations && py_truthy(citations) && citations->type == JSON_ARRAY) {
            size_t take = slice_len(json_len(citations), limit);
            for (i = 0; i < take; i++) {
                json_t *u = json_get(citations, i);
                char *stripped;
                if (!u || u->type != JSON_STRING) continue;
                stripped = py_strip_dup(u->str_val);
                if (stripped && stripped[0]) json_append(out, make_row("", u->str_val, "", i + 1));
                free(stripped);
            }
        }
    }
    json_free(collected);
    return out;
}

json_t *w19_provider_3254dd_web_config(const json_t *root) {
    const json_t *cfg = root;
    const char *keys[2] = {"web", "xai"};
    int i;
    for (i = 0; i < 2; i++) {
        if (!cfg || cfg->type != JSON_OBJECT) { cfg = NULL; break; }
        cfg = json_obj_get(cfg, keys[i]);
    }
    if (!cfg || cfg->type != JSON_OBJECT) return json_object();
    return json_copy((json_t *)cfg);
}

json_t *w19_provider_3254dd_limit_clamp(const json_t *limit_value) {
    json_t *fallback = json_number(5.0);
    json_t *coerced = w19_provider_3254dd_coerce("int", limit_value, fallback);
    long long v = (long long)coerced->num_val;
    json_free(fallback);
    json_free(coerced);
    if (v > 100) v = 100;
    if (v < 1) v = 1;
    return json_number((double)v);
}
