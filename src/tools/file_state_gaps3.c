#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/file_state.py: 1 missing functions ── */

/* ── rest_revive1 (file_state_gaps3) ────────────────────────────────────────
 * forget_task: pure state mutation under a silent lock (the single-threaded
 * harness models `with self._state_lock` as a no-op, no seam events).
 * in.self carries _reads (dict) and _last_writer (dict path -> [writer_tid,
 * ts] — the live 2-tuples). The port pops the task's read stamp, then
 * deletes every writer claim whose writer_tid == task_id. Unpacking a
 * claim value follows live py3.14 semantics: non-iterables raise
 * TypeError, wrong-length iterables raise ValueError, AFTER the reads
 * pop already happened — state order matches live. Missing self
 * attributes raise with the oracle double's SimpleNamespace type name.
 * json_t *fn(fs_io_t *io, json_t *in) — io unused (no seams). */
typedef struct fs_io {
    json_t *(*call)(struct fs_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} fs_io_t;

static json_t *fs_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *fs_self_attr_error(const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", attr);
    return fs_raise("AttributeError", msg);
}

static int fs_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type == JSON_NULL || b->type == JSON_NULL)
        return a->type == JSON_NULL && b->type == JSON_NULL;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    return 0;
}

/* PoP: forget_task @ tools/file_state.py:forget_task */
json_t *forget_task(fs_io_t *io, json_t *in) {
    (void)io;
    json_t *self = json_obj_get(in, "self");
    json_t *reads = self ? json_obj_get(self, "_reads") : NULL;
    if (!reads || reads->type != JSON_OBJECT) return fs_self_attr_error("_reads");
    json_t *lw = json_obj_get(self, "_last_writer");
    if (!lw || lw->type != JSON_OBJECT) return fs_self_attr_error("_last_writer");
    json_t *tid = json_obj_get(in, "task_id");
    /* self._reads.pop(task_id, None) */
    if (tid && tid->type == JSON_STRING) json_obj_del(reads, tid->str_val);
    /* for p in [p for p, (writer_tid, _ts) in self._last_writer.items()
     *           if writer_tid == task_id]: del self._last_writer[p]
     * The comprehension unpacks EVERY value in insertion order; the first
     * non-2-sequence raises before any deletion. */
    size_t n = lw->c.count;
    char **doomed = malloc(n * sizeof(char *));
    size_t ndoom = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = lw->c.items[i];
        /* Live unpack (writer_tid, _ts) = value: non-iterables raise
         * TypeError; iterables of length != 2 raise ValueError (py3.14
         * lists/dicts report "got N" on too-many, str does not); a
         * length-2 iterable unpacks to its first element (str: first
         * code point, dict: first key). */
        int iterable = v && (v->type == JSON_ARRAY || v->type == JSON_STRING ||
                             v->type == JSON_OBJECT);
        if (!iterable) {
            const char *tn = (!v || v->type == JSON_NULL) ? "NoneType"
                : (v->type == JSON_NUMBER ? (v->num_val == (double)(long long)v->num_val ? "int" : "float")
                : (v->type == JSON_BOOL ? "bool" : "object"));
            char msg[96];
            snprintf(msg, sizeof msg, "cannot unpack non-iterable %s object", tn);
            for (size_t j = 0; j < ndoom; j++) free(doomed[j]);
            free(doomed);
            return fs_raise("TypeError", msg);
        }
        size_t seqlen = 0;
        json_t *first = NULL;          /* array: borrowed first element */
        const char *first_key = NULL;  /* dict: first key */
        char first_cp[8];              /* str: first code point, utf-8 */
        first_cp[0] = '\0';
        int is_str = 0;
        if (v->type == JSON_ARRAY) {
            seqlen = v->c.count;
            if (seqlen == 2) first = v->c.items[0];
        } else if (v->type == JSON_OBJECT) {
            seqlen = v->c.count;
            if (seqlen == 2) first_key = v->c.keys[0];
        } else {
            is_str = 1;
            const char *s = v->str_val ? v->str_val : "";
            const char *firstp = NULL;
            for (const char *q = s; *q; q++) {
                if (((unsigned char)*q & 0xC0) != 0x80) {
                    if (!firstp) firstp = q;
                    seqlen++;
                }
            }
            if (seqlen == 2 && firstp) {
                size_t l = 1;
                while (firstp[l] && ((unsigned char)firstp[l] & 0xC0) == 0x80 && l < 4) l++;
                memcpy(first_cp, firstp, l);
                first_cp[l] = '\0';
            }
        }
        if (seqlen != 2) {
            char msg[96];
            if (seqlen < 2)
                snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", seqlen);
            else if (is_str)
                snprintf(msg, sizeof msg, "too many values to unpack (expected 2)");
            else
                snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", seqlen);
            for (size_t j = 0; j < ndoom; j++) free(doomed[j]);
            free(doomed);
            return fs_raise("ValueError", msg);
        }
        int match;
        if (first) match = fs_json_eq(first, tid);
        else if (first_key) match = tid && tid->type == JSON_STRING && strcmp(first_key, tid->str_val) == 0;
        else match = tid && tid->type == JSON_STRING && strcmp(first_cp, tid->str_val) == 0;
        if (match)
            doomed[ndoom++] = strdup(lw->c.keys[i]);
    }
    for (size_t j = 0; j < ndoom; j++) {
        json_obj_del(lw, doomed[j]);
        free(doomed[j]);
    }
    free(doomed);
    return json_null();
}
