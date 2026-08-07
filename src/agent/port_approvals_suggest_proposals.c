/*
 * port_approvals_suggest_proposals.c — Faithful C11 port of the pure
 * aggregation/ranking helpers from hermes_cli/approvals_suggest.py:
 *
 *   - class Proposal (opaque struct + methods)
 *   - Proposal.add_example()
 *   - build_proposals()
 *
 * The DB-scanning layer (scan_approval_history, _iter_terminal_calls,
 * _blocked_tool_call_ids) and I/O dispatch (suggest_command, approvals_command)
 * live elsewhere; this file is the in-memory normalization → ranking pipeline.
 *
 * Reuses the existing approval primitives:
 *   approval_normalize_command(), approval_is_unsafe_class(),
 *   approval_derive_glob() — all PoP-annotated to this same Python module.
 */

#define _GNU_SOURCE
#include "port_approvals_suggest_proposals.h"
#include "approval.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Proposal: opaque struct (header exposes only the handle) ── */

struct proposal {
    char  *pattern;          /* command glob ("git push *") or class key */
    char  *kind;             /* "glob" | "class"              */
    int    count;
    char **classes;          /* dynamic array of class descriptions  */
    size_t classes_len;
    size_t classes_cap;
    char **examples;         /* dynamic array of short commands      */
    size_t examples_len;
    size_t examples_cap;
};

static void _ensure_cap_classes(proposal_t *p)
{
    if (p->classes_len < p->classes_cap) return;
    size_t new_cap = p->classes_cap ? p->classes_cap * 2 : 8;
    char **tmp = (char **)realloc(p->classes, new_cap * sizeof(char *));
    if (!tmp) return;
    p->classes = tmp;
    p->classes_cap = new_cap;
}

static void _ensure_cap_examples(proposal_t *p)
{
    if (p->examples_len < p->examples_cap) return;
    size_t new_cap = p->examples_cap ? p->examples_cap * 2 : 8;
    char **tmp = (char **)realloc(p->examples, new_cap * sizeof(char *));
    if (!tmp) return;
    p->examples = tmp;
    p->examples_cap = new_cap;
}

static int _str_in_array(const char **arr, size_t len, const char *key)
{
    for (size_t i = 0; i < len; i++)
        if (strcmp(arr[i], key) == 0) return 1;
    return 0;
}

/* ── Constructor / destructor ── */

proposal_t *proposal_create(const char *pattern, const char *kind)
{
    if (!pattern || !kind) return NULL;
    proposal_t *p = (proposal_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->pattern = strdup(pattern);
    p->kind    = strdup(kind);
    if (!p->pattern || !p->kind) {
        free(p->pattern);
        free(p->kind);
        free(p);
        return NULL;
    }
    return p;
}

void proposal_free(proposal_t *p)
{
    if (!p) return;
    free(p->pattern);
    free(p->kind);
    for (size_t i = 0; i < p->classes_len; i++) free(p->classes[i]);
    for (size_t i = 0; i < p->examples_len; i++) free(p->examples[i]);
    free(p->classes);
    free(p->examples);
    free(p);
}

/* ── Methods (PoP: Proposal.add_example) ── */

/* PoP: add_example @ hermes_cli/approvals_suggest.py:Proposal.add_example */
int proposal_add_example(proposal_t *p, const char *command)
{
    if (!p || !command) return -1;
    /* short = command.strip(); if len > 100: short = short[:97] + "..." */
    char shortbuf[128];
    const char *src = command;
    /* skip leading whitespace */
    while (*src == ' ' || *src == '\t' || *src == '\n') src++;
    size_t slen = strlen(src);
    const char *end = src + slen;
    while (end > src && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    slen = (size_t)(end - src);
    if (slen > 100) {
        /* short[:97] + "..." */
        memcpy(shortbuf, src, 97);
        memcpy(shortbuf + 97, "...", 3);
        shortbuf[100] = '\0';
    } else {
        memcpy(shortbuf, src, slen);
        shortbuf[slen] = '\0';
    }

    /* if short not in self.examples and len(self.examples) < 3: append */
    if (p->examples_len >= 3) return 0;
    for (size_t i = 0; i < p->examples_len; i++)
        if (strcmp(p->examples[i], shortbuf) == 0) return 0;

    _ensure_cap_examples(p);
    if (p->examples_len >= p->examples_cap) return -1;
    p->examples[p->examples_len++] = strdup(shortbuf);
    return 0;
}

/* Helper: add a class description if not already present */
static void _add_class_uniq(proposal_t *p, const char *cls)
{
    if (!cls) return;
    if (_str_in_array((const char **)p->classes, p->classes_len, cls)) return;
    _ensure_cap_classes(p);
    if (p->classes_len >= p->classes_cap) return;
    p->classes[p->classes_len++] = strdup(cls);
}

/* ── build_proposals ─────────────────────────────────────────── */

/* PoP: build_proposals @ hermes_cli/approvals_suggest.py:build_proposals */
/* Records: array of (command, description) pairs, each a 2-element char* array.
 * existing: set of pattern strings to skip (or NULL).
 * Returns a malloc'd array of proposal_t* (len_results output param),
 * sorted by (-count, pattern).  Caller frees each proposal + the array.
 */
proposal_t **build_proposals(
    const char ***commands,   /* records[i][0] = command, [i][1] = description */
    size_t n_records,
    const char **existing,    /* exclusion set of pattern strings */
    size_t n_existing,
    int min_count,
    int limit,
    size_t *out_len
)
{
    if (!out_len) return NULL;
    *out_len = 0;
    if (!commands || n_records == 0) return NULL;

    /* by_pattern: dict[tuple[str, str], Proposal]
     * Key: (pattern, kind).  We store a parallel array of keys. */
    typedef struct {
        char    *pattern;
        char    *kind;
        proposal_t *prop;
    } entry_t;

    entry_t  *entries   = NULL;
    size_t    n_entries = 0;
    size_t    cap       = 0;

    /* Helper: find or create entry by (pattern, kind) */
    /* (inline since we need closure over entries)            */

    for (size_t i = 0; i < n_records; i++) {
        const char *command     = commands[i][0];
        const char *description = commands[i][1];
        if (!command || !description) continue;

        /* if is_unsafe_class(description): continue */
        if (approval_is_unsafe_class(description)) continue;

        /* normalized = normalize_command(command) */
        char *normalized = approval_normalize_command(command);
        if (!normalized) continue;

        /* glob = derive_glob(normalized) */
        char *glob = approval_derive_glob(normalized);
        free(normalized);

        const char *pattern, *kind;
        if (glob) {
            pattern = glob;
            kind    = "glob";
        } else {
            pattern = description;
            kind    = "class";
        }

        /* if pattern in existing: skip */
        if (_str_in_array(existing, n_existing, pattern)) {
            free(glob);
            continue;
        }

        /* find or create entry */
        entry_t *e = NULL;
        for (size_t j = 0; j < n_entries; j++)
            if (strcmp(entries[j].pattern, pattern) == 0 &&
                strcmp(entries[j].kind, kind) == 0) {
                e = &entries[j];
                break;
            }

        if (!e) {
            if (n_entries >= cap) {
                cap = cap ? cap * 2 : 8;
                entry_t *tmp = (entry_t *)realloc(entries, cap * sizeof(entry_t));
                if (!tmp) { free(glob); break; }
                entries = tmp;
            }
            entries[n_entries].pattern = strdup(pattern);
            entries[n_entries].kind    = strdup(kind);
            entries[n_entries].prop    = proposal_create(pattern, kind);
            e = &entries[n_entries];
            n_entries++;
        }
        free(glob);

        e->prop->count++;
        _add_class_uniq(e->prop, description);

        /* normalize for example */
        char *norm2 = approval_normalize_command(command);
        if (norm2) {
            proposal_add_example(e->prop, norm2);
            free(norm2);
        }
    }

    /* Filter: count >= max(min_count, 1) */
    /* Rank: sort by (-count, pattern)     */
    /* We do a simple insertion sort (small N expected). */
    for (size_t i = 1; i < n_entries; i++) {
        entry_t key = entries[i];
        size_t j = i;
        int kc = key.prop ? key.prop->count : 0;
        while (j > 0) {
            int jc = entries[j - 1].prop ? entries[j - 1].prop->count : 0;
            int cmp;
            /* Python: sort(key=lambda p: (-p.count, p.pattern))
             * Ascending by (-count, pattern): higher count first, then pattern. */
            if (kc != jc)
                cmp = jc - kc;  /* if kc > jc (key has higher count), cmp < 0 -> key goes before */
            else
                cmp = strcmp(key.pattern, entries[j - 1].pattern);
            if (cmp < 0) {
                entries[j] = entries[j - 1];
                j--;
            } else break;
        }
        entries[j] = key;
    }

    /* Collect results up to limit */
    size_t result_cap = (n_entries < (size_t)limit) ? n_entries : (size_t)limit;
    if (result_cap < 1) result_cap = 1;
    proposal_t **results = (proposal_t **)calloc(result_cap, sizeof(proposal_t *));
    if (!results) {
        for (size_t i = 0; i < n_entries; i++) {
            free(entries[i].pattern);
            free(entries[i].kind);
            proposal_free(entries[i].prop);
        }
        free(entries);
        return NULL;
    }

    int mc = min_count;
    if (mc < 1) mc = 1;
    size_t ri = 0;
    for (size_t i = 0; i < n_entries && ri < result_cap; i++) {
        if (entries[i].prop && entries[i].prop->count >= mc) {
            results[ri++] = entries[i].prop;
            entries[i].prop = NULL; /* ownership transferred */
        }
    }
    *out_len = ri;

    /* Free leftover entries (failed to meet min_count or over limit) */
    for (size_t i = 0; i < n_entries; i++) {
        free(entries[i].pattern);
        free(entries[i].kind);
        if (entries[i].prop) proposal_free(entries[i].prop);
    }
    free(entries);
    return results;
}

/* ── Accessors ── */

const char *proposal_pattern(const proposal_t *p) { return p ? p->pattern : NULL; }
const char *proposal_kind(const proposal_t *p)    { return p ? p->kind    : NULL; }
int proposal_count(const proposal_t *p)           { return p ? p->count   : 0;   }
const char **proposal_classes(const proposal_t *p, size_t *len)
{
    if (!p) { if (len) *len = 0; return NULL; }
    if (len) *len = p->classes_len;
    return (const char **)p->classes;
}
const char **proposal_examples(const proposal_t *p, size_t *len)
{
    if (!p) { if (len) *len = 0; return NULL; }
    if (len) *len = p->examples_len;
    return (const char **)p->examples;
}
