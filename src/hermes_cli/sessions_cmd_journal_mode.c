/*
 * sessions_cmd_journal_mode.c -- real C11 port of the console command
 * `hermes sessions set-journal-mode delete|wal` of upstream Hermes
 *   hermes_cli/sessions_cmd_journal_mode.py.
 *
 * PoP: w21_sessionscm_34a9f9_run_set_journal_mode @ hermes_cli/sessions_cmd_journal_mode.py:cmd_set_journal_mode
 *
 * All four seams of the command -- the profile's store location, the store
 * admin (open_store_admin + its set_journal_mode), the config's configured
 * journal mode, and the filesystem existence test -- arrive in the request
 * object. Every message and every return code is reproduced byte for byte,
 * including the check/cross/arrow/dash glyphs (the source stays ASCII: the
 * glyphs are written as UTF-8 byte escapes).
 */
#include "hermes_json.h"
#include "noc_w21_sessionscm_34a9f9.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UTF-8 glyphs used by the command's messages. */
#define SJM_CHECK "\xE2\x9C\x93"     /* U+2713 */
#define SJM_CROSS "\xE2\x9C\x97"     /* U+2717 */
#define SJM_ARROW "\xE2\x86\x92"     /* U+2192 */
#define SJM_DASH  "\xE2\x80\x94"     /* U+2014 */

static char *sjm_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int sjm_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* `f"{value}"` for the scalars a journal mode can be. */
static char *sjm_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return sjm_dup("None");
    switch (v->type) {
    case JSON_STRING: return sjm_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return sjm_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (sjm_is_int(v)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return sjm_dup(buf);
    default:
        return sjm_dup("?");
    }
}

/* `a == b` for two decoded scalars (no containers reach these fields). */
static int sjm_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type != b->type) {
        /* Python 1 == True and 1 == 1.0; keep the numeric/bool bridge. */
        if ((a->type == JSON_NUMBER && b->type == JSON_BOOL) ||
            (a->type == JSON_BOOL && b->type == JSON_NUMBER)) {
            double x = a->type == JSON_NUMBER ? a->num_val : (a->bool_val ? 1.0 : 0.0);
            double y = b->type == JSON_NUMBER ? b->num_val : (b->bool_val ? 1.0 : 0.0);
            return x == y;
        }
        return 0;
    }
    switch (a->type) {
    case JSON_BOOL:   return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    default:          return 0;
    }
}

/* The trailing config hint; appended only when the config would switch back. */
static void sjm_append_note(json_t *lines, const json_t *target, const char *configured) {
    char *tgt, *buf;
    size_t n;

    tgt = sjm_str(target);
    if (!tgt) return;
    n = strlen(configured ? configured : "") + strlen(tgt) + 320;
    buf = (char *)malloc(n);
    if (!buf) { free(tgt); return; }
    snprintf(buf, n,
             "  Note: config.yaml has database.journal_mode: %s " SJM_DASH
             " the next open would switch the file back. Set `database.journal_mode: %s` "
             "in config.yaml to keep it.",
             configured ? configured : "", tgt);
    json_append(lines, json_string(buf));
    free(buf);
    free(tgt);
}

json_t *w21_sessionscm_34a9f9_run_set_journal_mode(const json_t *req) {
    json_t *lines = json_array();
    json_t *out = json_object();
    const json_t *target;
    const char *db, *fallback, *admin_kind, *error;
    char *db_path = NULL;
    int code = 0;

    if (!req || req->type != JSON_OBJECT) {
        json_set(out, "code", json_number(1));
        json_set(out, "lines", lines);
        return out;
    }
    target = json_obj_get(req, "mode");

    /* `getattr(args, "db", None) or store_location(profile_scope())` */
    db = json_get_str(req, "db", NULL);
    fallback = json_get_str(req, "store_location", NULL);
    if (!db || !db[0]) db = fallback;
    if (!db || !db[0]) {
        json_append(lines, json_string(SJM_CROSS " Setting the journal mode needs a "
                                              "file-backed session store; this profile's "
                                              "store has no file."));
        code = 1;
        json_set(out, "code", json_number(code));
        json_set(out, "lines", lines);
        return out;
    }
    db_path = sjm_dup(db);
    if (!db_path) {
        json_set(out, "code", json_number(1));
        json_set(out, "lines", lines);
        return out;
    }

    /* `open_store_admin(...) if db_path.exists() else None`, then the
     * AdminUnsupported isinstance: both collapse to the same message. */
    admin_kind = json_get_str(req, "admin", "none");
    if (!json_get_bool(req, "db_exists", 0) || !strcmp(admin_kind, "none") ||
        !strcmp(admin_kind, "unsupported")) {
        size_t n = strlen(db_path) + 48;
        char *buf = (char *)malloc(n);
        if (buf) {
            snprintf(buf, n, "No database at %s (nothing to convert).", db_path);
            json_append(lines, json_string(buf));
            free(buf);
        }
        free(db_path);
        json_set(out, "code", json_number(1));
        json_set(out, "lines", lines);
        return out;
    }

    /* `except PersistenceError as exc: print(f"<cross> {exc}"); return 1` */
    error = json_get_str(req, "error", NULL);
    if (error && error[0]) {
        size_t n = strlen(error) + 8;
        char *buf = (char *)malloc(n);
        if (buf) {
            snprintf(buf, n, SJM_CROSS " %s", error);
            json_append(lines, json_string(buf));
            free(buf);
        }
        free(db_path);
        json_set(out, "code", json_number(1));
        json_set(out, "lines", lines);
        return out;
    }

    {
        const json_t *result = json_obj_get(req, "result");
        const json_t *current = json_obj_get(result, "current");
        const json_t *after = json_obj_get(result, "after");
        char *cur_s = sjm_str(current), *aft_s = sjm_str(after);
        char *tgt_s = sjm_str(target);
        size_t n = strlen(db_path) + strlen(cur_s ? cur_s : "") +
                   strlen(aft_s ? aft_s : "") + strlen(tgt_s ? tgt_s : "") + 96;

        if (sjm_eq(current, after)) {
            char *buf = (char *)malloc(n);
            if (buf) {
                snprintf(buf, n, SJM_CHECK " %s is already journal_mode=%s.",
                         db_path, tgt_s ? tgt_s : "");
                json_append(lines, json_string(buf));
                free(buf);
            }
            code = 0;
        } else {
            char *buf = (char *)malloc(n);
            if (buf) {
                snprintf(buf, n, SJM_CHECK " %s: journal_mode %s " SJM_ARROW " %s "
                                           "(header verified).",
                         db_path, cur_s ? cur_s : "", aft_s ? aft_s : "");
                json_append(lines, json_string(buf));
                free(buf);
            }
            /* `configured = resolve_journal_mode(); if configured != target:` */
            {
                const json_t *configured = json_obj_get(req, "configured");
                if (!sjm_eq(configured, target)) {
                    char *conf_s = sjm_str(configured);
                    sjm_append_note(lines, target, conf_s);
                    free(conf_s);
                }
            }
            code = 0;
        }
        free(cur_s);
        free(aft_s);
        free(tgt_s);
    }
    free(db_path);
    json_set(out, "code", json_number(code));
    json_set(out, "lines", lines);
    return out;
}
