/*
 * web_whatsapp_session.c -- C11 port of upstream Hermes
 *   hermes_cli/web_whatsapp_session.py
 *
 * PoP: w21_webwhatsap_8973fb_bridge_check @ hermes_cli/web_whatsapp_session.py:_assert_whatsapp_bridge_stopped
 * PoP: w21_webwhatsap_8973fb_backup @ hermes_cli/web_whatsapp_session.py:_backup_whatsapp_session
 *
 * Both guards are ported in full: the pid file parse (Python int() semantics,
 * utf-8-sig read, splitlines), the liveness + start-time fingerprint, the
 * symlink refusal, and the rename/mkdir(0700) backup with its 500 detail. The
 * filesystem, the process table and the clock are the injected seams.
 */
#include "hermes_json.h"
#include "noc_w21_webwhatsap_8973fb.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEBWHATSAP_UNKNOWN \
    "Cannot verify the WhatsApp bridge is stopped. Check its bridge.pid before re-pairing."
#define WEBWHATSAP_RUNNING \
    "The WhatsApp bridge is still running. Stop it before re-pairing."
#define WEBWHATSAP_SYMLINK \
    "The WhatsApp session is a symbolic link. Back it up manually before re-pairing."

/* --- tiny helpers --------------------------------------------------------- */

static const char *webwhatsap_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = json_obj_get(obj, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

static json_t *webwhatsap_result(int raised, int status, const char *detail) {
    json_t *out = json_object();
    json_set(out, "raised", json_bool(raised));
    json_set(out, "status", status ? json_number((double)status) : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

/*
 * Python int(s) for base 10: optional surrounding whitespace, optional sign,
 * digits with single underscores between them. Returns 0 where int() raises
 * ValueError. Values outside long long saturate (Python ints are unbounded;
 * no fixture process start time comes close).
 */
static int webwhatsap_py_int(const char *s, long long *out) {
    const char *p = s;
    int negative = 0, digits = 0, overflow = 0;
    unsigned long long acc = 0;

    if (!s) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }
    while (*p) {
        if (*p == '_') {
            /* an underscore must sit between two digits */
            if (digits == 0 || !isdigit((unsigned char)p[1]) || p[-1] == '_') return 0;
            p++;
            continue;
        }
        if (!isdigit((unsigned char)*p)) break;
        if (acc > (unsigned long long)LLONG_MAX / 10ull + 1ull) overflow = 1;
        acc = acc * 10ull + (unsigned long long)(*p - '0');
        digits++;
        p++;
    }
    if (digits == 0) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return 0;                       /* trailing junk -> ValueError */
    if (overflow) {
        *out = negative ? LLONG_MIN : LLONG_MAX;
        return 1;
    }
    if (negative) {
        if (acc > (unsigned long long)LLONG_MAX + 1ull) *out = LLONG_MIN;
        else *out = -(long long)acc;
    } else {
        *out = (acc > (unsigned long long)LLONG_MAX) ? LLONG_MAX : (long long)acc;
    }
    return 1;
}

/* text.splitlines()[want]; 0 when that line does not exist. */
static int webwhatsap_line(const char *text, int want, char *out, size_t cap) {
    size_t n, pos = 0, end;
    int idx = 0, term;

    if (!text) return 0;
    n = strlen(text);
    if (n == 0) return 0;
    for (;;) {
        end = pos;
        term = 0;
        while (end < n) {
            unsigned char c = (unsigned char)text[end];
            if (c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == 0x85) {
                term = 1;
                break;
            }
            end++;
        }
        if (idx == want) {
            size_t len = end - pos;
            if (len >= cap) len = cap - 1;
            memcpy(out, text + pos, len);
            out[len] = '\0';
            return 1;
        }
        if (!term) return 0;
        pos = end + ((text[end] == '\r' && end + 1 < n && text[end + 1] == '\n') ? 2 : 1);
        idx++;
        if (pos >= n) return 0;             /* trailing terminator adds no line */
    }
}

/* read_text(encoding="utf-8-sig"): a leading BOM is not part of the payload. */
static const char *webwhatsap_after_bom(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (p && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) return (const char *)(p + 3);
    return text;
}

/* --- _assert_whatsapp_bridge_stopped ------------------------------------- */

/* 0 = the bridge is verifiably stopped, else 409 with *detail set. */
static int webwhatsap_bridge_status(const json_t *state, const char **detail) {
    char line[256];
    long long pid, recorded_start;
    json_t *current = json_obj_get(state, "current_start");
    const char *text;

    *detail = NULL;
    if (!json_get_bool(state, "pid_file_exists", 0)) return 0;
    if (json_get_bool(state, "read_raises", 0)) {
        *detail = WEBWHATSAP_UNKNOWN;
        return 409;
    }
    text = webwhatsap_after_bom(webwhatsap_str(state, "pid_text", ""));
    if (!webwhatsap_line(text, 0, line, sizeof(line)) || !webwhatsap_py_int(line, &pid) ||
        pid <= 0) {
        *detail = WEBWHATSAP_UNKNOWN;
        return 409;
    }
    if (!json_get_bool(state, "pid_exists", 0)) return 0;
    if (!webwhatsap_line(text, 1, line, sizeof(line)) ||
        !webwhatsap_py_int(line, &recorded_start) || recorded_start <= 0) {
        *detail = WEBWHATSAP_UNKNOWN;
        return 409;
    }
    if (!current || current->type != JSON_NUMBER) {      /* None from the process table */
        *detail = WEBWHATSAP_UNKNOWN;
        return 409;
    }
    if ((long long)current->num_val == recorded_start) {
        *detail = WEBWHATSAP_RUNNING;
        return 409;
    }
    /* Both fingerprints are available and differ: a recycled PID, not the
     * bridge that owns the session. */
    return 0;
}

json_t *w21_webwhatsap_8973fb_bridge_check(const json_t *state) {
    const char *detail = NULL;
    int status = webwhatsap_bridge_status(state, &detail);
    return webwhatsap_result(status != 0, status, detail);
}

/* --- _backup_whatsapp_session -------------------------------------------- */

json_t *w21_webwhatsap_8973fb_backup(const json_t *state) {
    const char *detail = NULL;
    int status = webwhatsap_bridge_status(state, &detail);
    json_t *out = json_object();
    json_t *actions = json_array();
    const char *rename_error = NULL, *mkdir_error = NULL, *backup_dir;

    json_set(out, "raised", json_bool(0));
    json_set(out, "status", json_null());
    json_set(out, "detail", json_null());
    json_set(out, "actions", actions);
    json_set(out, "mkdir_mode", json_null());

    if (status != 0) {
        json_set(out, "raised", json_bool(1));
        json_set(out, "status", json_number((double)status));
        json_set(out, "detail", json_string(detail ? detail : ""));
        return out;
    }
    if (json_get_bool(state, "is_symlink", 0)) {
        json_set(out, "raised", json_bool(1));
        json_set(out, "status", json_number(409.0));
        json_set(out, "detail", json_string(WEBWHATSAP_SYMLINK));
        return out;
    }
    if (!json_get_bool(state, "exists", 0)) return out;   /* nothing to move */

    rename_error = webwhatsap_str(state, "rename_error", NULL);
    if (rename_error) goto backup_failed;
    json_append(actions, json_string("rename"));

    mkdir_error = webwhatsap_str(state, "mkdir_error", NULL);
    if (mkdir_error) goto backup_failed;
    json_append(actions, json_string("mkdir"));
    json_set(out, "mkdir_mode", json_string("0700"));
    return out;

backup_failed:
    backup_dir = webwhatsap_str(state, "backup_dir", "");
    {
        const char *msg = rename_error ? rename_error : (mkdir_error ? mkdir_error : "");
        size_t need = strlen("Could not back up the WhatsApp session: . Pairing was not started.")
                      + strlen(msg) + strlen(backup_dir) + 64;
        char *text = (char *)malloc(need);
        if (!text) {
            json_set(out, "raised", json_bool(1));
            json_set(out, "status", json_number(500.0));
            return out;
        }
        snprintf(text, need,
                 "Could not back up the WhatsApp session: %s. Pairing was not started.",
                 msg);
        json_set(out, "raised", json_bool(1));
        json_set(out, "status", json_number(500.0));
        if (mkdir_error) {
            /* rename succeeded, so backup.exists() is true: say where it went */
            size_t need2 = strlen(text) + strlen(backup_dir) + 64;
            char *text2 = (char *)malloc(need2);
            if (text2) {
                snprintf(text2, need2,
                         "%s The previous session is preserved at %s/session.", text,
                         backup_dir);
                json_set(out, "detail", json_string(text2));
                free(text2);
            }
        } else {
            json_set(out, "detail", json_string(text));
        }
        free(text);
    }
    return out;
}
