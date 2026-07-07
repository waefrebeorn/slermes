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
 * PoP: current_instantiation_epoch @ gateway/drain_control.py:current_instantiation_epoch
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
 * PoP: drain_request_path @ gateway/drain_control.py:drain_request_path
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
 * PoP: _marker_epoch_is_stale @ gateway/drain_control.py:_marker_epoch_is_stale
 * body_json: marker body; current_epoch: result of drain_control_current_epoch().
 * Returns 1 iff a definite mismatch (current non-empty, marker epoch present
 * and differs), else 0 (lenient: missing/empty current epoch or marker epoch
 * is treated as NOT stale). */
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
 * PoP: drain_notification_suppressed @ gateway/drain_control.py:drain_notification_suppressed
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
 * PoP: drain_requested @ gateway/drain_control.py:drain_requested
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
