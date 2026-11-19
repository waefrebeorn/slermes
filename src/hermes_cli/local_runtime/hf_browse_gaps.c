#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <strings.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/hf_browse.py: 5 missing functions ── */

/* ── rest_revive3 seam infrastructure (hb) ───────────────────────────────
 * fn(hb_io_t *io, json_t *in); hb_seam records {ev,args,ret} (args
 * adopted), raise-specs model Python exceptions, NULL return == None.
 * Integral JSON numbers model ints. Pure ports take void *io. */
typedef struct hb_io {
    json_t *(*call)(struct hb_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} hb_io_t;

static int hb_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int hb_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static json_t *hb_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *hb_seam(hb_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

#define HB_HF "https://huggingface.co"
#define HB_ROUGH_KV_AND_OVERHEAD (4LL << 30)

/* str(node) for the scalar shapes the live code coerces (str/number/bool/None). */
static char *hb_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

/* strndup is absent from the winlibs UCRT runtime; copy at most n bytes. */
static char *hb_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* int(node or 0): truthy gate then int() (number truncates toward zero). */
static long long hb_int_or0(const json_t *v) {
    if (!hb_truthy(v)) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) {
        const char *p = v->str_val;
        while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        long long n = 0; int digits = 0;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; digits++; }
        while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
        if (digits && *p == '\0') return neg ? -n : n;
    }
    return 0;
}

/* urllib.parse.quote(s, safe="/"): UTF-8 bytes, unreserved + '/' literal. */
static char *hb_url_quote(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = malloc(n * 3 + 1);
    if (!out) return NULL;
    size_t o = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '~';
        if (unreserved || c == '/') out[o++] = (char)c;
        else { out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15]; }
    }
    out[o] = '\0';
    return out;
}

static void hb_lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

/* _QUANT_RE = (?:IQ|Q)\d[_A-Z0-9]*|F16|BF16|F32 (IGNORECASE), search + upper.
 * Returns malloc'd match, or NULL when no match. */
static char *hb_quant_match(const char *s) {
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        char c = (char)tolower((unsigned char)s[i]);
        size_t start = i, j = i;
        int matched = 0;
        /* (IQ|Q)\d */
        if (c == 'i' && i + 2 < n && tolower((unsigned char)s[i+1]) == 'q' &&
            isdigit((unsigned char)s[i+2])) { j = i + 2; matched = 1; }
        else if (c == 'q' && i + 1 < n && isdigit((unsigned char)s[i+1])) { j = i + 1; matched = 1; }
        if (matched) {
            j++; /* first digit */
            while (j < n) {
                char d = (char)tolower((unsigned char)s[j]);
                if ((d >= 'a' && d <= 'z') || (d >= '0' && d <= '9') || d == '_') j++;
                else break;
            }
        } else if (i + 3 <= n && strncasecmp(s + i, "F16", 3) == 0) { j = i + 3; matched = 1; }
        else if (i + 4 <= n && strncasecmp(s + i, "BF16", 4) == 0) { j = i + 4; matched = 1; }
        else if (i + 3 <= n && strncasecmp(s + i, "F32", 3) == 0) { j = i + 3; matched = 1; }
        if (matched) {
            char *out = malloc(j - start + 1);
            if (!out) return NULL;
            for (size_t k = start; k < j; k++) out[k - start] = (char)toupper((unsigned char)s[k]);
            out[j - start] = '\0';
            return out;
        }
    }
    return NULL;
}

/* PoP: quant_label @ hermes_cli/local_runtime/hf_browse.py:_quant_label */
json_t *quant_label(void *io, json_t *in) {
    (void)io;
    const char *fn = json_get_str(in, "filename", "");
    char *m = hb_quant_match(fn);
    json_t *r = json_string(m ? m : fn);
    free(m);
    return r;
}

/* _SPLIT_RE = -(\d{5})-of-(\d{5})\.gguf$ (IGNORECASE). On match returns the
 * stem length (m.start()) and part number (group 1); else 0. */
static size_t hb_split_match(const char *path, long long *part) {
    size_t n = strlen(path);
    /* suffix "-NNNNN-of-NNNNN.gguf" length = 1+5+4+5+5 = 20 */
    if (n < 20) return 0;
    size_t s = n - 20;
    if (path[s] != '-') return 0;
    for (int k = 0; k < 5; k++) if (!isdigit((unsigned char)path[s+1+k])) return 0;
    if (strncasecmp(path + s + 6, "-of-", 4) != 0) return 0;
    for (int k = 0; k < 5; k++) if (!isdigit((unsigned char)path[s+10+k])) return 0;
    if (strncasecmp(path + s + 15, ".gguf", 5) != 0) return 0;
    long long p = 0;
    for (int k = 0; k < 5; k++) p = p * 10 + (path[s+1+k] - '0');
    *part = p;
    return s;
}

static json_t *hb_get_json(hb_io_t *io, const char *url) {
    json_t *args = json_object();
    json_set_hdr(args, "url", url);
    return hb_seam(io, "_get_json", args);
}

/* PoP: rough_fit @ hermes_cli/local_runtime/hf_browse.py:rough_fit */
json_t *rough_fit(void *io, json_t *in) {
    (void)io;
    long long total = (long long)json_get_num(in, "total_bytes", 0);
    const json_t *budget = json_obj_get(in, "budget");
    long long vram = (long long)json_get_num(budget, "usable_vram_bytes", 0);
    long long ram = (long long)json_get_num(budget, "ram_available_bytes", 0);
    long long need = total + HB_ROUGH_KV_AND_OVERHEAD;
    if (need <= vram) return json_string("fits-gpu");
    if (need <= vram + ram) return json_string("needs-ram");
    return json_string("too-big");
}

/* split-part record */
typedef struct { long long part; char *path; long long size; } hb_part_t;
typedef struct { char *stem; hb_part_t *parts; size_t n, cap; } hb_split_t;

static int hb_part_cmp(const void *a, const void *b) {
    const hb_part_t *x = a, *y = b;
    if (x->part != y->part) return x->part < y->part ? -1 : 1;
    int c = strcmp(x->path, y->path);
    if (c) return c;
    return x->size < y->size ? -1 : (x->size > y->size ? 1 : 0);
}

static json_t *hb_group(const char *label, json_t *paths, long long total) {
    json_t *g = json_object();
    json_set_hdr(g, "label", label);
    json_set(g, "paths", paths);
    json_set(g, "total_bytes", json_number((double)total));
    json_set_hdr(g, "fit", "unknown");
    return g;
}

/* PoP: repo_files @ hermes_cli/local_runtime/hf_browse.py:repo_files */
json_t *repo_files(hb_io_t *io, json_t *in) {
    const char *repo = json_get_str(in, "repo", "");
    char *qrepo = hb_url_quote(repo);
    size_t ulen = strlen(HB_HF) + strlen(qrepo) + 64;
    char *url = malloc(ulen);
    snprintf(url, ulen, "%s/api/models/%s/tree/main?recursive=true", HB_HF, qrepo);
    free(qrepo);
    json_t *files = hb_get_json(io, url);
    free(url);
    if (hb_is_raise(files)) return files;
    if (!files || files->type != JSON_ARRAY) {
        json_free(files);
        return hb_raise("AttributeError", "'str' object has no attribute 'get'");
    }
    /* singles */
    char **single_path = NULL; long long *single_size = NULL; size_t ns = 0, cs = 0;
    /* splits (insertion ordered) */
    hb_split_t *splits = NULL; size_t nsp = 0, csp = 0;
    for (size_t i = 0; i < json_len(files); i++) {
        const json_t *f = json_get(files, i);
        const json_t *pnode = json_obj_get(f, "path");
        char *path = pnode ? hb_str_of(pnode) : strdup("");
        size_t plen = strlen(path);
        int is_gguf = plen >= 5 && strncasecmp(path + plen - 5, ".gguf", 5) == 0;
        if (!is_gguf) { free(path); continue; }
        /* name = after last '/', lowercased */
        const char *slash = strrchr(path, '/');
        char *name = strdup(slash ? slash + 1 : path);
        hb_lower(name);
        int excluded = strncmp(name, "mmproj", 6) == 0 || strncmp(name, "dspark", 6) == 0 ||
                       strncmp(name, "mtp-", 4) == 0 || strstr(name, "draft") != NULL;
        free(name);
        if (excluded) { free(path); continue; }
        long long size = hb_int_or0(json_obj_get(f, "size"));
        long long part = 0;
        size_t stemlen = hb_split_match(path, &part);
        if (stemlen) {
            char *stem = hb_strndup(path, stemlen);
            hb_split_t *sp = NULL;
            for (size_t k = 0; k < nsp; k++) if (strcmp(splits[k].stem, stem) == 0) { sp = &splits[k]; break; }
            if (!sp) {
                if (nsp == csp) { csp = csp ? csp * 2 : 8; splits = realloc(splits, csp * sizeof *splits); }
                sp = &splits[nsp++];
                sp->stem = stem; sp->parts = NULL; sp->n = 0; sp->cap = 0;
            } else free(stem);
            if (sp->n == sp->cap) { sp->cap = sp->cap ? sp->cap * 2 : 8; sp->parts = realloc(sp->parts, sp->cap * sizeof *sp->parts); }
            sp->parts[sp->n].part = part;
            sp->parts[sp->n].path = path;  /* adopts path */
            sp->parts[sp->n].size = size;
            sp->n++;
        } else {
            if (ns == cs) { cs = cs ? cs * 2 : 8; single_path = realloc(single_path, cs * sizeof *single_path); single_size = realloc(single_size, cs * sizeof *single_size); }
            single_path[ns] = path; single_size[ns] = size; ns++;
        }
    }
    json_t *groups = json_array();
    for (size_t i = 0; i < ns; i++) {
        char *lab = hb_quant_match(single_path[i]);
        json_t *paths = json_array();
        json_append(paths, json_string(single_path[i]));
        json_append(groups, hb_group(lab ? lab : single_path[i], paths, single_size[i]));
        free(lab);
    }
    for (size_t i = 0; i < nsp; i++) {
        qsort(splits[i].parts, splits[i].n, sizeof(hb_part_t), hb_part_cmp);
        char *lab = hb_quant_match(splits[i].stem);
        json_t *paths = json_array();
        long long total = 0;
        for (size_t k = 0; k < splits[i].n; k++) {
            json_append(paths, json_string(splits[i].parts[k].path));
            total += splits[i].parts[k].size;
        }
        json_append(groups, hb_group(lab ? lab : splits[i].stem, paths, total));
        free(lab);
    }
    /* stable sort by total_bytes descending (insertion sort) */
    size_t ng = json_len(groups);
    for (size_t i = 1; i < ng; i++) {
        json_t *g = groups->c.items[i];
        long long key = (long long)json_get_num(g, "total_bytes", 0);
        size_t j = i;
        while (j > 0 && (long long)json_get_num(groups->c.items[j-1], "total_bytes", 0) < key) {
            groups->c.items[j] = groups->c.items[j-1];
            j--;
        }
        groups->c.items[j] = g;
    }
    /* cleanup */
    for (size_t i = 0; i < ns; i++) free(single_path[i]);
    free(single_path); free(single_size);
    for (size_t i = 0; i < nsp; i++) {
        for (size_t k = 0; k < splits[i].n; k++) free(splits[i].parts[k].path);
        free(splits[i].parts); free(splits[i].stem);
    }
    free(splits);
    json_free(files);
    return groups;
}

/* PoP: search_models @ hermes_cli/local_runtime/hf_browse.py:search_models */
json_t *search_models(hb_io_t *io, json_t *in) {
    const char *query = json_get_str(in, "query", "");
    /* query.strip() */
    while (*query == ' ' || (*query >= '\t' && *query <= '\r')) query++;
    size_t qlen = strlen(query);
    while (qlen > 0 && (query[qlen-1] == ' ' || (query[qlen-1] >= '\t' && query[qlen-1] <= '\r'))) qlen--;
    char *qstrip = hb_strndup(query, qlen);
    char *q = hb_url_quote(qstrip);
    free(qstrip);
    long long limit = (long long)json_get_num(in, "limit", 20);
    if (limit > 50) limit = 50;
    if (limit < 1) limit = 1;
    size_t ulen = strlen(HB_HF) + strlen(q) + 128;
    char *url = malloc(ulen);
    snprintf(url, ulen, "%s/api/models?search=%s&filter=gguf&sort=downloads&direction=-1&limit=%lld", HB_HF, q, limit);
    free(q);
    json_t *files = hb_get_json(io, url);
    free(url);
    if (hb_is_raise(files)) return files;
    if (!files || files->type != JSON_ARRAY) {
        json_free(files);
        return hb_raise("TypeError", "'object' is not iterable");
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(files); i++) {
        const json_t *m = json_get(files, i);
        json_t *hit = json_object();
        const json_t *idnode = json_obj_get(m, "id");
        char *repo = idnode ? hb_str_of(idnode) : strdup("");
        json_set_hdr(hit, "repo", repo); free(repo);
        json_set(hit, "downloads", json_number((double)hb_int_or0(json_obj_get(m, "downloads"))));
        json_set(hit, "likes", json_number((double)hb_int_or0(json_obj_get(m, "likes"))));
        const json_t *lm = json_obj_get(m, "lastModified");
        char *upd = hb_truthy(lm) ? hb_str_of(lm) : strdup("");
        json_set_hdr(hit, "updated", upd); free(upd);
        json_set(hit, "gated", json_bool(hb_truthy(json_obj_get(m, "gated")) ? true : false));
        json_append(out, hit);
    }
    json_free(files);
    return out;
}

/* PoP: priced_repo_files @ hermes_cli/local_runtime/hf_browse.py:priced_repo_files */
json_t *priced_repo_files(hb_io_t *io, json_t *in) {
    json_t *groups = repo_files(io, in);
    if (hb_is_raise(groups)) return groups;
    const json_t *budget = json_obj_get(in, "budget");
    for (size_t i = 0; i < json_len(groups); i++) {
        json_t *g = json_get(groups, i);
        json_t *fit_in = json_object();
        json_set(fit_in, "total_bytes", json_copy(json_obj_get(g, "total_bytes")));
        json_set(fit_in, "budget", budget ? json_copy(budget) : json_null());
        json_t *fit = rough_fit(NULL, fit_in);
        json_free(fit_in);
        json_set(g, "fit", fit);
    }
    return groups;
}
