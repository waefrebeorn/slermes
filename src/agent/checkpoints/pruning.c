/*
 * pruning.c - real C11 port of upstream Hermes agent/checkpoints/pruning.py.
 * Everything git-shaped here is pure decision logic over already-captured git output (the
 * run_git / dir_size collaborators are injected on the Python side and become plain arguments
 * here); the file lock and subprocess plumbing stay in Python.
 *
 * PoP: w19_pruning_c083bd_store_lock_path @ agent/checkpoints/pruning.py:store_lock_path
 * PoP: w19_pruning_c083bd_parse_metadata @ agent/checkpoints/pruning.py:Pruner._rewrite
 * PoP: w19_pruning_c083bd_commits @ agent/checkpoints/pruning.py:Pruner._commits
 * PoP: w19_pruning_c083bd_trim @ agent/checkpoints/pruning.py:Pruner.trim
 * PoP: w19_pruning_c083bd_drop_one_round @ agent/checkpoints/pruning.py:Pruner.drop_one_round
 * PoP: w19_pruning_c083bd_enforce_size @ agent/checkpoints/pruning.py:Pruner.enforce_size
 */
#include "hermes_json.h"
#include "noc_w19_pruning_c083bd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROUNDS 20

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

static char *format_dup(const char *fmt, const char *a) {
    int n = snprintf(NULL, 0, fmt, a);
    char *buf;
    if (n < 0) return dupstr("");
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) return dupstr("");
    snprintf(buf, (size_t)n + 1, fmt, a);
    return buf;
}

/* Path.resolve() collapses separators; Path.with_name() then swaps the leaf. Windows form. */
char *w19_pruning_c083bd_store_lock_path(const char *base) {
    char *norm, *out;
    size_t len, i, cut = 0;
    int have_cut = 0;
    if (!base) return dupstr("");
    len = strlen(base);
    norm = (char *)malloc(len + 2);
    if (!norm) return dupstr("");
    for (i = 0; i < len; i++) norm[i] = (base[i] == '/') ? '\\' : base[i];
    norm[len] = '\0';
    /* Trim trailing separators, but never a drive root like "C:\". */
    while (len > 0 && norm[len - 1] == '\\' && !(len == 3 && norm[1] == ':')) {
        norm[--len] = '\0';
    }
    for (i = 0; i < len; i++) {
        if (norm[i] == '\\') { cut = i; have_cut = 1; }
    }
    {
        const char *name = have_cut ? norm + cut + 1 : norm;
        size_t parent_len = have_cut ? cut + 1 : 0; /* keep the separator */
        size_t name_len = strlen(name);
        out = (char *)malloc(parent_len + 1 + name_len + 6);
        if (!out) { free(norm); return dupstr(""); }
        memcpy(out, norm, parent_len);
        out[parent_len] = '.';
        memcpy(out + parent_len + 1, name, name_len);
        memcpy(out + parent_len + 1 + name_len, ".lock", 5);
        out[parent_len + 1 + name_len + 5] = '\0';
    }
    free(norm);
    return out;
}

/* metadata.split(sep, 3) then the len(fields) != 4 guard. */
json_t *w19_pruning_c083bd_parse_metadata(const char *text, const char *sep, const char *sha) {
    const char *src = text ? text : "";
    const char *sepp = (sep && *sep) ? sep : "\x00";
    size_t sl = strlen(sepp);
    const char *starts[4];
    size_t lens[4];
    int count = 0;
    const char *p = src;
    json_t *out;
    while (count < 3) {
        const char *hit = strstr(p, sepp);
        if (!hit) break;
        starts[count] = p;
        lens[count] = (size_t)(hit - p);
        count++;
        p = hit + sl;
    }
    starts[count] = p;
    lens[count] = strlen(p);
    count++;
    if (count != 4) {
        char *msg = format_dup("invalid checkpoint metadata: %s", sha ? sha : "");
        out = json_object();
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(msg));
        free(msg);
        return out;
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    {
        const char *keys[4] = {"tree", "author_date", "committer_date", "message"};
        int i;
        for (i = 0; i < 4; i++) {
            char *v = (char *)malloc(lens[i] + 1);
            if (!v) continue;
            memcpy(v, starts[i], lens[i]);
            v[lens[i]] = '\0';
            json_set(out, keys[i], json_string(v));
            free(v);
        }
    }
    return out;
}

/* Python str.splitlines() for the newline family (no other line breaks in git output). */
static json_t *splitlines(const char *s) {
    json_t *arr = json_array();
    const char *p = s;
    if (!s || !*s) return arr;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        size_t m = n;
        char *seg;
        if (m > 0 && p[m - 1] == '\r') m--;
        seg = (char *)malloc(m + 1);
        if (seg) {
            memcpy(seg, p, m);
            seg[m] = '\0';
            json_append(arr, json_string(seg));
            free(seg);
        }
        if (!nl) break;
        p = nl + 1;
        if (!*p) break;
    }
    return arr;
}

json_t *w19_pruning_c083bd_commits(const char *output, const char *ref, const char *git_error) {
    json_t *out;
    if (git_error) {
        out = json_object();
        json_set(out, "ok", json_bool(0));
        {
            char *msg = format_dup("checkpoint rev-list failed: %s", git_error);
            json_set(out, "error", json_string(msg));
            free(msg);
        }
        return out;
    }
    {
        json_t *lines = splitlines(output ? output : "");
        if (json_len(lines) == 0) {
            char *msg = format_dup("checkpoint ref has no readable history: %s", ref);
            json_free(lines);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string(msg));
            free(msg);
            return out;
        }
        out = json_object();
        json_set(out, "ok", json_bool(1));
        json_set(out, "commits", lines);
        return out;
    }
}

static size_t json_array_len(const json_t *v) {
    return (v && v->type == JSON_ARRAY) ? json_len(v) : 0;
}

static json_t *tail_slice(const json_t *arr, size_t start) {
    json_t *out = json_array();
    size_t i, n = json_array_len(arr);
    for (i = start; i < n; i++) json_append(out, json_copy(json_get(arr, i)));
    return out;
}

json_t *w19_pruning_c083bd_trim(const json_t *commits, long long keep_count, const char *ref) {
    json_t *out = json_object();
    size_t n = json_array_len(commits);
    if (n == 0) {
        char *msg = format_dup("checkpoint ref has no readable history: %s", ref ? ref : "");
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string(msg));
        free(msg);
        return out;
    }
    json_set(out, "ok", json_bool(1));
    if (keep_count < 1) keep_count = 1;
    if (n > (size_t)keep_count) {
        json_set(out, "action", json_string("rewrite"));
        json_set(out, "keep", tail_slice(commits, n - (size_t)keep_count));
    } else {
        json_set(out, "action", json_string("none"));
        json_set(out, "keep", json_array());
    }
    return out;
}

/* A refs entry is [name, [commits...]]; a non-list value means "no readable history". */
static const json_t *ref_commits(const json_t *entry) {
    if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 2) return NULL;
    return json_get(entry, 1);
}

static json_t *ref_name(const json_t *entry) {
    if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 1) return NULL;
    return json_get(entry, 0);
}

json_t *w19_pruning_c083bd_drop_one_round(const json_t *cap_bytes, const json_t *size,
                                          const json_t *refs) {
    json_t *out = json_object();
    json_t *dropped = json_array();
    double cap = (cap_bytes && cap_bytes->type == JSON_NUMBER) ? cap_bytes->num_val : 0.0;
    double cur = (size && size->type == JSON_NUMBER) ? size->num_val : 0.0;
    int changed = 0;
    size_t i;
    if (!(cap <= 0 || cur <= cap)) {
        for (i = 0; i < json_array_len(refs); i++) {
            json_t *entry = json_get(refs, i);
            const json_t *commits = ref_commits(entry);
            json_t *name = ref_name(entry);
            if (json_array_len(commits) > 1) {
                changed = 1;
                if (name && name->type == JSON_STRING) json_append(dropped, json_string(name->str_val));
            }
        }
    }
    json_set(out, "changed", json_bool(changed != 0));
    json_set(out, "dropped", dropped);
    return out;
}

json_t *w19_pruning_c083bd_enforce_size(const json_t *cap_bytes, const json_t *size,
                                        const json_t *refs, const json_t *sizes) {
    json_t *out = json_object();
    json_t *kept = json_array();
    double cap = (cap_bytes && cap_bytes->type == JSON_NUMBER) ? cap_bytes->num_val : 0.0;
    double cur = (size && size->type == JSON_NUMBER) ? size->num_val : 0.0;
    size_t script_i = 0;
    size_t script_n = json_array_len(sizes);
    int done = 0, rewrites = 0, round;
    if (cap <= 0 || cur <= cap) {
        json_set(out, "result", json_bool(1));
        json_set(out, "rewrites", json_number(0));
        json_set(out, "kept", kept);
        return out;
    }
    /* reclaim(), then the first remeasure. */
    if (script_i < script_n) {
        cur = json_get(sizes, script_i)->num_val;
        script_i++;
    }
    if (cur <= cap) done = 1;
    for (round = 0; round < MAX_ROUNDS && !done; round++) {
        int changed = 0;
        size_t i;
        for (i = 0; i < json_array_len(refs); i++) {
            json_t *entry = json_get(refs, i);
            const json_t *commits = ref_commits(entry);
            size_t n = json_array_len(commits);
            size_t k;
            if (n <= 1) continue;
            for (k = 1; k < n; k++) json_append(kept, json_copy(json_get(commits, k)));
            rewrites++;
            changed = 1;
            if (script_i < script_n) {
                cur = json_get(sizes, script_i)->num_val;
                script_i++;
            }
            if (cur <= cap) { done = 1; break; }
        }
        if (!changed) break;
    }
    json_set(out, "result", json_bool(done));
    json_set(out, "rewrites", json_number((double)rewrites));
    json_set(out, "kept", kept);
    return out;
}
