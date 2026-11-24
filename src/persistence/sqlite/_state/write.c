/*
 * write.c - C11 port of upstream persistence/sqlite/_state/write.py
 * (gate w24_write_5b6032).
 *
 * PoP: w24_write_5b6032_describe_code @ persistence/sqlite/_state/write.py:_describe_code
 * PoP: w24_write_5b6032_describe_site @ persistence/sqlite/_state/write.py:describe_site
 * PoP: w24_write_5b6032_maintenance @ persistence/sqlite/_state/write.py:maintain_after_commit
 * PoP: w24_write_5b6032_constants @ persistence/sqlite/_state/write.py:WriterMutexTimeout/_OWNER_LOG_INTERVAL_S/_WRAPPER_NAMES
 *
 * Upstream reads live code objects (sys._getframe), holds a threading.Lock and
 * logs through a rate-limited handler.  Those are the seams: a code object is
 * the value triple (filename, name, qualname), the frame is never entered, and
 * the counter/period decision is a pure modulo question.  The refusal text and
 * SQLITE_BUSY code are the literals WriterMutexTimeout carries.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_write_5b6032.h"

/* Python truthiness of a JSON value; NULL is an absent attribute (None). */
static int wr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str() of the scalars a code object's attributes hold. */
static const char *wr_text(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) return "None";
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? "True" : "False";
    case JSON_STRING: return v->str_val ? v->str_val : "";
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val && fabs(v->num_val) < 1e16)
            snprintf(buf, cap, "%.0f", v->num_val);
        else
            snprintf(buf, cap, "%.15g", v->num_val);
        return buf;
    default:
        return "";
    }
}

/* One code location, rendered into buf. */
static void wr_describe(const json_t *code, char *buf, size_t cap) {
    char nbuf[64];
    const json_t *fn = json_obj_get(code, "co_filename");
    const json_t *qualname = json_obj_get(code, "co_qualname");
    const json_t *name = json_obj_get(code, "co_name");
    /* (getattr(code, "co_filename", "") or "?").rsplit("/", 1)[-1] */
    const char *file = (fn && fn->type == JSON_STRING && fn->str_val && fn->str_val[0])
                           ? fn->str_val
                           : "?";
    const char *slash = strrchr(file, '/');
    const char *base = slash ? slash + 1 : file;
    size_t blen = strlen(base);
    const char *qual;
    char module[512];

    if (blen >= 3 && !strcmp(base + blen - 3, ".py")) blen -= 3;
    if (blen >= sizeof module) blen = sizeof module - 1;
    memcpy(module, base, blen);
    module[blen] = '\0';

    /* getattr(code, "co_qualname", None) or getattr(code, "co_name", "?") */
    if (wr_truthy(qualname)) qual = wr_text(qualname, nbuf, sizeof nbuf);
    else if (name) qual = wr_text(name, nbuf, sizeof nbuf);
    else qual = "?";

    snprintf(buf, cap, "%s:%s", module, qual);
}

json_t *w24_write_5b6032_describe_code(const json_t *code) {
    char buf[640];
    wr_describe(code, buf, sizeof buf);
    return json_string(buf);
}

json_t *w24_write_5b6032_describe_site(const json_t *site) {
    char first[640], second[640], out[1300];
    size_t n, used = 0;
    json_t *joined;
    char err[96];

    if (!site || site->type != JSON_ARRAY) return json_string("?");
    n = json_len(site);
    if (n != 2) {
        /* caller, callback = site  -- a 2-tuple unpack, ValueError otherwise */
        snprintf(err, sizeof err,
                 n < 2 ? "not enough values to unpack (expected 2, got %d)"
                       : "too many values to unpack (expected 2, got %d)",
                 (int)n);
        joined = json_object();
        json_set(joined, "error", json_string(err));
        return joined;
    }
    first[0] = second[0] = '\0';
    if (json_get(site, 0) && json_get(site, 0)->type != JSON_NULL)
        wr_describe(json_get(site, 0), first, sizeof first);
    if (json_get(site, 1) && json_get(site, 1)->type != JSON_NULL)
        wr_describe(json_get(site, 1), second, sizeof second);
    out[0] = '\0';
    if (first[0]) {
        used = (size_t)snprintf(out, sizeof out, "%s", first);
        if (second[0] && used < sizeof out)
            snprintf(out + used, sizeof out - used, " -> %s", second);
    } else if (second[0]) {
        snprintf(out, sizeof out, "%s", second);
    }
    /* " -> ".join(parts) or "?" */
    return json_string(out[0] ? out : "?");
}

json_t *w24_write_5b6032_maintenance(const json_t *req) {
    double count = json_get_num(req, "write_count", 0.0);
    double every_cp = json_get_num(req, "checkpoint_every", 0.0);
    double every_fts = json_get_num(req, "fts_every", 0.0);
    json_t *out;

    /* db._write_count += 1 happens before the checks upstream; the sample is
     * the post-increment count, so the caller passes what the counter holds. */
    if (every_cp == 0.0 || every_fts == 0.0) {
        out = json_object();
        json_set(out, "error", json_string("division by zero"));
        return out;
    }
    out = json_object();
    json_set(out, "checkpoint", json_bool(fmod(count, every_cp) == 0.0));
    json_set(out, "fts_merge", json_bool(fmod(count, every_fts) == 0.0));
    return out;
}

json_t *w24_write_5b6032_constants(void) {
    json_t *o = json_object();
    json_t *names = json_array();
    json_set(o, "owner_log_interval_s", json_number(W24_WRITE_5B6032_OWNER_LOG_INTERVAL_S));
    json_append(names, json_string("_write_rowcount"));
    json_append(names, json_string("_write_sql"));
    json_set(o, "wrapper_names", names);
    json_set(o, "mutex_message",
             json_string("database is locked (in-process state.db writer mutex exceeded "
                         "write patience)"));
    json_set(o, "sqlite_busy", json_number(5.0));
    json_set(o, "sqlite_busy_name", json_string("SQLITE_BUSY"));
    return o;
}
