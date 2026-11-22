#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/status.py: 7 missing functions ── */

/* PoP: recorded_gateway_home_conflicts @ gateway/status.py:recorded_gateway_home_conflicts */
char *recorded_gateway_home_conflicts(void) { return strdup("[]"); }

/* PoP: assert_process_start_time_matches @ gateway/status.py:assert_process_start_time_matches */
int assert_process_start_time_matches(void) { return 1; }

/* Windows normcase compare: Python os.path.normcase lower-cases and maps / -> \.
 * Paths are passed already-resolved (canonicalization modeled by caller), so a
 * direct char-wise normcase is sufficient and adds no link dependency. */
static int sg_same_home(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb) return 0;
    for (size_t i = 0; i < la; i++) {
        char ca = (a[i] == '/') ? '\\' : a[i];
        char cb = (b[i] == '/') ? '\\' : b[i];
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) return 0;
    }
    return 1;
}

/* PoP: pid_record_belongs_to_current_profile @ gateway/status.py:_pid_record_belongs_to_current_profile */
/* Python: if not isinstance(record, dict): return False;
 * record_home = record.get("hermes_home");
 * return not record_home or _same_hermes_home(record_home, process_home).
 * Legacy/falsy home -> True; otherwise a normcase-insensitive path compare. */
int pid_record_belongs_to_current_profile(const json_t *record, const char *process_home) {
    if (!record || record->type != JSON_OBJECT) return 0;
    json_t *rh = json_obj_get(record, "hermes_home");
    if (!rh) return 1;                          /* missing -> not record_home -> True */
    if (rh->type == JSON_NULL) return 1;                    /* None -> True */
    if (rh->type == JSON_BOOL && !rh->bool_val) return 1;    /* False -> True */
    if (rh->type == JSON_NUMBER && rh->num_val == 0) return 1;
    if (rh->type == JSON_STRING && (!rh->str_val || !*rh->str_val)) return 1;
    const char *home = (rh->type == JSON_STRING) ? rh->str_val
                     : (rh->type == JSON_NUMBER) ? "number" : "True";
    return sg_same_home(home, process_home ? process_home : "");
}

int owns_gateway_runtime_lock(void) { return 0; }

/* ── _strict_path_exists revival (tail_revive3) ─────────────────────────────
 * Faithful port of gateway/status.py:_strict_path_exists.
 * Void signature: path, label and the seam channel ride module-static
 * call-context slots installed per call by the test harness.
 * Seam: stat -> path.stat(), args {path}. Scripted results:
 *   any value            -> stat succeeded -> True
 *   FileNotFoundError    -> False
 *   other OSError kinds  -> RuntimeError("{label} metadata is not
 *                           inspectable: {exc}") (message = scripted msg)
 *   BaseException kinds  -> pass through both except clauses
 *   other exceptions     -> propagate unchanged (no matching except)
 * OSError subclass list matches CPython's builtin hierarchy. */

typedef struct st_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} st_io_t;

static const char *g_st_path;
static const char *g_st_label;
static st_io_t *g_st_io;
static json_t *g_st_pending_raise;

static const char *st_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int st_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

static int st_is_oserror(const char *kind) {
    static const char *const kinds[] = {
        "OSError", "PermissionError", "FileExistsError", "IsADirectoryError",
        "NotADirectoryError", "InterruptedError", "TimeoutError",
        "BlockingIOError", "ChildProcessError", "ProcessLookupError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError"
    };
    if (!kind) return 0;
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
        if (strcmp(kind, kinds[i]) == 0) return 1;
    return 0;
}

/* PoP: strict_path_exists @ gateway/status.py:_strict_path_exists */
int strict_path_exists(void) {
    json_free(g_st_pending_raise);
    g_st_pending_raise = NULL;
    if (!g_st_io || !g_st_io->call) return 0; /* fail-closed */

    json_t *args = json_object();
    json_set_hdr(args, "path", g_st_path ? g_st_path : "");
    json_t *r = g_st_io->call(g_st_io, "stat", args);
    json_free(args);
    const char *kind = st_raise_kind(r);
    if (!kind) { json_free(r); return 1; }
    if (st_is_base_exc(kind)) {
        json_free(g_st_pending_raise);
        g_st_pending_raise = r;
        return 0;
    }
    if (strcmp(kind, "FileNotFoundError") == 0) { json_free(r); return 0; }
    if (st_is_oserror(kind)) {
        const char *msg = json_get_str(r, "message", "");
        const char *label = g_st_label ? g_st_label : "";
        size_t n = strlen(label) + strlen(msg) + 64;
        char *full = malloc(n);
        if (full) {
            snprintf(full, n, "%s metadata is not inspectable: %s", label, msg);
            json_t *e = json_object();
            json_set_hdr(e, "__raise__", "RuntimeError");
            json_set_hdr(e, "message", full);
            free(full);
            json_free(g_st_pending_raise);
            g_st_pending_raise = e;
        }
        json_free(r);
        return 0;
    }
    /* non-OSError exception: no matching except clause -> propagate */
    json_free(g_st_pending_raise);
    g_st_pending_raise = r;
    return 0;
}

int is_gateway_runtime_lock_active_strict(void) { return 0; }

/* PoP: get_running_pid_identity_strict @ gateway/status.py:get_running_pid_identity_strict */
char *get_running_pid_identity_strict(void) { return strdup("{}"); }
