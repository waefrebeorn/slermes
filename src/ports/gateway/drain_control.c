#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/gateway/drain_control.c — C11 port of gateway/drain_control.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_drain_control_helpers.c ── */
/*
 * port_drain_control_helpers.c
 *
 * Pure, portable helper functions ported from gateway/drain_control.py.
 * current_instantiation_epoch reads /proc (boot_id + /proc/1/stat field 22) —
 * these are plain file reads, no network/transaction. The marker JSON helpers
 * (staleness, notification-suppressed, drain-requested) take the already-read
 * marker body JSON plus the current epoch. File-write/remove helpers
 * (write_drain_request, clear_drain_request, read_drain_request) stay REAL_GAP.
 *
 * C name <- python name (module prefix 'drain_control_'):
 *   drain_control_current_epoch         <- current_instantiation_epoch
 *   drain_control_request_path          <- drain_request_path
 *   drain_control_marker_epoch_is_stale <- _marker_epoch_is_stale
 *   drain_control_notification_suppressed <- drain_notification_suppressed
 *   drain_control_requested             <- drain_requested
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DRAIN_REQUEST_FILENAME ".drain_request.json"

/* read an entire small text file into a malloc'd string, or NULL on failure */
static char *read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

/* trim trailing whitespace/newline */
static void trim_right(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) s[--n]='\0';
}

/*
 * Reads /proc/sys/kernel/random/boot_id and field 22 of /proc/1/stat.
 * Returns malloc'd "boot_id:pid1start" or "" when neither readable. Caller frees. */
char *drain_control_current_epoch(void)
{
    char *boot = read_file_text("/proc/sys/kernel/random/boot_id");
    if (boot) trim_right(boot);
    char *pid1 = NULL;
    char *stat = read_file_text("/proc/1/stat");
    if (stat) {
        char *tail = strrchr(stat, ')');
        if (tail) {
            /* split the tail after the last ')' on whitespace */
            char *p = tail + 1;
            while (*p == ' ' || *p == '\t') p++;
            int idx = 0;
            char *tok = strtok(p, " \t\n");
            while (tok && idx < 19) { tok = strtok(NULL, " \t\n"); idx++; }
            if (tok) { pid1 = strdup(tok); }
        }
        free(stat);
    }
    if (!boot || !boot[0]) {
        if ((!pid1 || !pid1[0])) {
            free(boot); free(pid1);
            return strdup("");
        }
    }
    size_t cap = (boot ? strlen(boot) : 0) + (pid1 ? strlen(pid1) : 0) + 2;
    char *out = malloc(cap);
    snprintf(out, cap, "%s:%s", boot ? boot : "", pid1 ? pid1 : "");
    free(boot); free(pid1);
    return out;
}

/*
 * home: HERMES_HOME string (caller resolves). Returns malloc'd path. Caller frees. */
char *drain_control_request_path(const char *home)
{
    if (!home || !home[0]) home = ".";
    size_t cap = strlen(home) + strlen(DRAIN_REQUEST_FILENAME) + 2;
    char *out = malloc(cap);
    snprintf(out, cap, "%s/%s", home, DRAIN_REQUEST_FILENAME);
    return out;
}

/*
 * body_json: marker body; current_epoch: result of drain_control_current_epoch().
 * Returns 1 iff a definite mismatch (current non-empty, marker epoch present
 * and differs), else 0 (lenient: missing/empty current epoch or marker epoch
 * is treated as NOT stale). */
/* PoP: drain_control_marker_epoch_is_stale @ gateway/drain_control.py:_marker_epoch_is_stale */
int drain_control_marker_epoch_is_stale(const char *body_json, const char *current_epoch)
{
    if (!current_epoch || !current_epoch[0]) return 0; /* can't be sure */
    if (!body_json || !body_json[0]) return 0;
    json_t *body = json_parse(body_json, NULL);
    if (!body || body->type != JSON_OBJECT) { if (body) json_free(body); return 0; }
    json_t *ep = json_object_get(body, "epoch");
    int stale = 0;
    if (ep && ep->type == JSON_STRING) {
        const char *marker = json_string_value(ep);
        if (marker && marker[0] && strcmp(marker, current_epoch) != 0) stale = 1;
    }
    json_free(body);
    return stale;
}

/*
 * body_json: marker body. Returns 1 iff body["suppress_notification"] == true. */
int drain_control_notification_suppressed(const char *body_json)
{
    if (!body_json || !body_json[0]) return 0;
    json_t *body = json_parse(body_json, NULL);
    if (!body || body->type != JSON_OBJECT) { if (body) json_free(body); return 0; }
    json_t *s = json_object_get(body, "suppress_notification");
    int r = (s && s->type == JSON_BOOL && s->bool_val) ? 1 : 0;
    json_free(body);
    return r;
}

/*
 * body_json: marker body (caller reads the file), or NULL/"" when absent.
 * current_epoch: result of drain_control_current_epoch().
 * Returns 1 iff a begin-drain marker for THIS instantiation is present
 * (body present AND epoch not stale). */
int drain_control_requested(const char *body_json, const char *current_epoch)
{
    if (!body_json || !body_json[0]) return 0; /* no marker */
    if (drain_control_marker_epoch_is_stale(body_json, current_epoch)) return 0;
    return 1;
}


/* ── from src/gateway/port_gateway_drain_control.c ── */
/* Slermes C port — gateway/drain_control.py (marker contract, pure FS helpers) */

#include <stdbool.h>
#include "hermes_gateway_core.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>


















static bool write_file_atomic(const char *path, const char *content)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return rename(tmp, path) == 0;
}

/* PoP: current_instantiation_epoch @ gateway/drain_control.py:current_instantiation_epoch */
/* Mirror current_instantiation_epoch(): boot_id + pid1 start time. */
static void current_instantiation_epoch(char *out, size_t outsz)
{
    out[0] = '\0';
    char *boot = read_file_text("/proc/sys/kernel/random/boot_id");
    char *pid1 = read_file_text("/proc/1/stat");
    char bid[64] = "", p1[64] = "";
    if (boot) { size_t i = 0; while (boot[i] && boot[i] != '\n' && i < sizeof(bid)-1) bid[i] = boot[i], i++; bid[i]='\0'; }
    if (pid1) {
        char *rparen = strrchr(pid1, ')');
        if (rparen) {
            /* fields after comm: state ppid ... starttime is the 20th */
            char *rest = rparen + 1;
            int n = 0; char *tok = strtok(rest, " ");
            while (tok) {
                n++;
                if (n == 20) { strncpy(p1, tok, sizeof(p1)-1); p1[sizeof(p1)-1]='\0'; break; }
                tok = strtok(NULL, " ");
            }
        }
    }
    if (boot) free(boot);
    if (pid1) free(pid1);
    if (!bid[0] && !p1[0]) return;  /* no /proc -> empty epoch */
    snprintf(out, outsz, "%s:%s", bid, p1);
}

/* PoP: drain_request_path @ gateway/drain_control.py:drain_request_path */
static const char *drain_request_path(char *out, size_t outsz, const char *home)
{
    const char *h = home ? home : getenv("HERMES_HOME");
    if (!h) h = ".";
    snprintf(out, outsz, "%s/.drain_request.json", h);
    return out;
}

/* PoP: string @ gateway/drain_control.py:write_drain_request */
/* Returns a malloc'd JSON payload string (caller frees). */
char *gateway_drain_control_write_drain_request(const char *principal, bool suppress_notification, const char *home)
{
    char epoch[128]; current_instantiation_epoch(epoch, sizeof(epoch));
    char path[1024]; drain_request_path(path, sizeof(path), home);
    char ts[64]; time_t now = time(NULL);
    struct tm tm; gmtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm);
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"action\": \"drain\", \"requested_at\": \"%s\", \"principal\": \"%s\", "
        "\"epoch\": \"%s\", \"suppress_notification\": %s}",
        ts, principal ? principal : "drain-control", epoch, suppress_notification ? "true" : "false");
    write_file_atomic(path, payload);
    return strdup(payload);
}

/* PoP: gateway_drain_control_clear_drain_request @ gateway/drain_control.py:clear_drain_request */
bool gateway_drain_control_clear_drain_request(const char *home)
{
    char path[1024]; drain_request_path(path, sizeof(path), home);
    if (unlink(path) == 0) return true;
    return false;  /* missing or error -> idempotent False */
}

/* Internal: marker epoch stale? (lenient) */
static bool marker_epoch_is_stale(const char *body, const char *current_epoch)
{
    if (!current_epoch || !current_epoch[0]) return false;
    /* find "epoch" : "..." */
    char pat[32]; snprintf(pat, sizeof(pat), "\"epoch\"[ \t]*:[ \t]*\"");
    regex_t re; if (regcomp(&re, pat, REG_EXTENDED) != 0) return false;
    regmatch_t m; bool stale = false;
    if (regexec(&re, body, 1, &m, 0) == 0) {
        char *s = (char *)body + m.rm_eo - 1;
        if (*s == '"') {
            char mepoch[128]; size_t o = 0;
            while (s[1] && s[1] != '"' && o < sizeof(mepoch) - 1) mepoch[o++] = s[1], s++;
            mepoch[o] = '\0';
            if (mepoch[0] && strcmp(mepoch, current_epoch) != 0) stale = true;
        }
    }
    regfree(&re);
    return stale;
}

/* PoP: body @ gateway/drain_control.py:read_drain_request */
/* Returns malloc'd JSON body (possibly "{}") or NULL if absent. Caller frees. */
char *gateway_drain_control_read_drain_request(const char *home)
{
    char path[1024]; drain_request_path(path, sizeof(path), home);
    char *raw = read_file_text(path);
    if (!raw) return NULL;
    /* try parse as JSON object; on failure return "{}" */
    if (strstr(raw, "{") != NULL) {
        /* minimal: ensure it's an object; return as-is if starts with '{' */
        if (raw[0] == '{') return raw;
    }
    free(raw);
    return strdup("{}");
}

