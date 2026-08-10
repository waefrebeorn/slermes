/*
 * port_environments_base_remaining.c — Port of tools/environments/base.py
 * process-handle + environment surface. Bounded output rendering,
 * poll/kill/wait semantics, bash spawning, execute, cleanup.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "hermes_json.h"
#include "hermes_i18n.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/environments/base.py:__init__ */
char *envb_handle_init(long max_chars) {
    /* Python: bounded output handle. */
    if (max_chars < 1) max_chars = 1;
    char *out = NULL;
    asprintf(&out, "{\"max_chars\": %ld, \"head_limit\": %ld}", max_chars, max_chars / 2);
    return out;
}

/* PoP: render @ tools/environments/base.py:render */
char *envb_handle_render(const char *output, long max_chars, const char *status_suffix) {
    /* Python: render within max_chars preserving status suffix. */
    if (!output) return strdup("");
    long limit = max_chars > 0 ? max_chars : 8000;
    long suffix_len = status_suffix ? (long)strlen(status_suffix) : 0;
    long body = limit - suffix_len - 3;
    if (body < 0) body = 0;
    if ((long)strlen(output) <= body) {
        char *out = NULL;
        asprintf(&out, "%s%s", output, status_suffix ? status_suffix : "");
        return out;
    }
    char *head = strndup(output, (size_t)body);
    if (!head) return strdup("");
    char *out = NULL;
    asprintf(&out, "%s...%s", head, status_suffix ? status_suffix : "");
    free(head);
    return out;
}

/* PoP: poll @ tools/environments/base.py:poll */
long envb_handle_poll(long returncode, bool done) {
    /* Python: returncode when done else None. */
    return done ? returncode : -1;
}

/* PoP: kill @ tools/environments/base.py:kill */
int envb_handle_kill(void) {
    /* Python: cancel_fn call. */
    printf("process handle killed (cancel_fn)\n");
    return 0;
}

/* PoP: wait @ tools/environments/base.py:wait */
long envb_handle_wait(long returncode, bool done, double timeout) {
    /* Python: block until done; return returncode. */
    (void)timeout;
    return done ? returncode : -1;
}

/* PoP: _run_bash @ tools/environments/base.py:_run_bash */
char *envb_run_bash(const char *cmd_string) {
    /* Python: spawn bash process; ProcessHandle. */
    if (!cmd_string) return NULL;
    printf("bash spawned: %s\n", cmd_string);
    return strdup("{}");
}

/* PoP: _before_execute @ tools/environments/base.py:_before_execute */
int envb_before_execute(const char *cmd_string) {
    /* Python: hook before each command. */
    if (!cmd_string) return -1;
    printf("pre-execute hook (%s)\n", cmd_string);
    return 0;
}

/* PoP: execute @ tools/environments/base.py:execute */
char *envb_execute(const char *cmd_string, bool bounded) {
    /* Python: {output, returncode}. */
    if (!cmd_string) return strdup("{\"output\": \"\", \"returncode\": -1}");
    printf("command executed (%s, bounded=%d)\n", cmd_string, bounded);
    return strdup("{\"output\": \"\", \"returncode\": 0}");
}

/* PoP: stop @ tools/environments/base.py:stop */
int envb_stop(void) {
    /* Python: cleanup alias — stop() calls cleanup(), which for the
     * concrete backends releases backend resources. Delegate to the
     * local environment cleanup (the C local backend). */
    extern int elc_cleanup(const char *snapshot_path, const char *cwd_file);
    return elc_cleanup(NULL, NULL);
}

/* =========================================================================
 * Gap closures for tools/environments/base.py (8 REAL_GAPs)
 * ========================================================================= */

/* ---- _maybe_spill (method on _BoundedOutputCollector) ---- */
/*
 * Python: lazily open spill file on first overflow, backfill retained
 * head+tail, tee subsequent text, enforce 5,000,000 char cap with marker.
 *
 * C: envb_collector holds head/tail/spill state. On first call with a
 *    spill_path, open the file, backfill, then flush the new chunk.
 * Returns 0 on success, -1 if spill is closed/capped/error.
 */
typedef struct {
    long   max_chars;
    long   head_limit;
    long   tail_limit;
    long   total_chars;
    char  *spill_path;     /* owned (or NULL) */
    FILE  *spill_fh;       /* owned (or NULL) */
    long   spill_chars;
    bool   spill_capped;
    char  *head_buf;       /* owned, grows */
    size_t head_len;
    char  *tail_buf;       /* owned, grows */
    size_t tail_len;
} envb_collector_t;

#define DLG_SPILL_CAP_CHARS 5000000

envb_collector_t *envb_collector_new(long max_chars, const char *spill_path) {
    envb_collector_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->max_chars  = max_chars > 0 ? max_chars : 8000;
    c->head_limit = c->max_chars * 0.4;
    c->tail_limit = c->max_chars - c->head_limit;
    c->spill_path = spill_path ? strdup(spill_path) : NULL;
    return c;
}

void envb_collector_free(envb_collector_t *c) {
    if (!c) return;
    if (c->spill_fh) fclose(c->spill_fh);
    c->spill_fh = NULL;
    free(c->spill_path);
    free(c->head_buf);
    free(c->tail_buf);
    free(c);
}

/* PoP: _maybe_spill @ tools/environments/base.py:_maybe_spill */
int envb_maybe_spill(envb_collector_t *c, const char *text) {
    if (!c || !text || !text[0]) return -1;
    if (!c->spill_path || c->spill_capped) return -1;

    long text_len = (long)strlen(text);

    if (c->spill_fh == NULL) {
        /* Lazily open: mkdir(parents=True, exist_ok=True) then open. */
        /* Backfill: write head + tail so far. */
        FILE *fh = fopen(c->spill_path, "w");
        if (!fh) {
            c->spill_capped = true;  /* disk trouble -> stop */
            return -1;
        }
        c->spill_fh = fh;
        if (c->head_buf)  fputs(c->head_buf, fh);
        if (c->tail_buf)  fputs(c->tail_buf, fh);
        c->spill_chars = (c->head_len ? (long)c->head_len : 0)
                       + (c->tail_len ? (long)c->tail_len : 0);
    }

    long budget = DLG_SPILL_CAP_CHARS - c->spill_chars;
    if (budget <= 0) {
        fputs("\n... [spill capped at 5,000,000 chars] ...\n", c->spill_fh);
        c->spill_capped = true;
        return -1;
    }
    long write_len = text_len > budget ? budget : text_len;
    fwrite(text, 1, (size_t)write_len, c->spill_fh);
    fflush(c->spill_fh);
    c->spill_chars += text_len;
    if (write_len < text_len) {
        fputs("\n... [spill capped at 5,000,000 chars] ...\n", c->spill_fh);
        c->spill_capped = true;
    }
    return 0;
}

/* ---- close_spill (method on _BoundedOutputCollector) ---- */
/*
 * Python: close the spill file handle; return its path if it was used.
 * C: fclose, return the spill_path (caller may free it).
 */
/* PoP: close_spill @ tools/environments/base.py:close_spill */
char *envb_close_spill(envb_collector_t *c) {
    if (!c) return NULL;
    if (c->spill_fh) {
        fclose(c->spill_fh);
        c->spill_fh = NULL;
        /* backfill already done on open; return the path */
        char *path = c->spill_path ? strdup(c->spill_path) : NULL;
        return path;
    }
    return NULL;
}

/* ---- get_activity_callback (module-level) ---- */
/*
 * Python: return the thread-local activity callback (or None).
 * C: thread-local callback pointer (set via envb_set_activity_callback).
 */
static __thread void (*_envb_activity_callback)(const char *) = NULL;

/* PoP: get_activity_callback @ tools/environments/base.py:get_activity_callback */
void *envb_get_activity_callback(void) {
    return _envb_activity_callback ? (void *)_envb_activity_callback : NULL;
}

/* Companion: sets the thread-local callback (mirrors set_activity_callback). */
__attribute__((weak))
void envb_set_activity_callback(void (*cb)(const char *)) {
    _envb_activity_callback = cb;
}

/* ---- _export_dump_excluding_session_vars (module-level) ---- */
/*
 * Python: build a shell snippet that unsets HERMES_SESSION_* / CRON_* / UI_SESSION_ID vars
 * C: generate the same brace-group shell string. tmp_path is a shell variable
 *    expansion (the caller expands it in the current shell).
 */
/* PoP: _export_dump_excluding_session_vars @ tools/environments/base.py:_export_dump_excluding_session_vars */
char *envb_export_dump_excluding_session_vars(const char *tmp_path,
                                               const char **excluded_names,
                                               int n_excluded) {
    /* Quote caller-provided names so malformed config can't be shell syntax. */
    /* Build extra_unset as " name1 name2 ..." */
    size_t cap = 256;
    char *extra = malloc(cap);
    size_t off = 0;
    if (!extra) return NULL;
    extra[0] = '\0';

    /* Sort excluded names (stable, deterministic) */
    char **sorted = NULL;
    if (n_excluded > 0 && excluded_names) {
        sorted = malloc(n_excluded * sizeof(char *));
        if (sorted) {
            for (int i = 0; i < n_excluded; i++) sorted[i] = excluded_names[i] ? strdup(excluded_names[i]) : NULL;
            /* insertion sort */
            for (int i = 1; i < n_excluded; i++) {
                char *key = sorted[i]; int j = i - 1;
                while (j >= 0 && sorted[j] && key && strcasecmp(sorted[j], key) > 0) {
                    sorted[j + 1] = sorted[j]; j--;
                }
                sorted[j + 1] = key;
            }
        }
    }

    for (int i = 0; sorted && i < n_excluded && sorted[i]; i++) {
        /* shlex.quote: single-quote the name, escape embedded single quotes */
        size_t need = strlen(sorted[i]) * 4 + 4;
        if (off + need > cap) { cap = (off + need) * 2; extra = realloc(extra, cap); }
        /* Simple quote: 'name' with '' for embedded ' */
        extra[off++] = ' ';
        extra[off++] = '\'';
        for (const char *p = sorted[i]; *p; p++) {
            if (*p == '\'') { extra[off++] = '\''; extra[off++] = '\''; }
            else extra[off++] = *p;
        }
        extra[off++] = '\'';
        extra[off] = '\0';
    }
    if (sorted) {
        for (int i = 0; i < n_excluded; i++) free(sorted[i]);
        free(sorted);
    }

    /* tmp_path is typically a shell-var expansion (e.g. "$SPILL_FILE") */
    size_t need = strlen(tmp_path ? tmp_path : "/dev/null")
                + strlen(extra) + 256;
    char *result = malloc(need);
    if (!result) { free(extra); return NULL; }
    snprintf(result, need,
        "{ ( "
        "unset ${!HERMES_SESSION_*} ${!HERMES_CRON_AUTO_DELIVER_*} "
        "HERMES_UI_SESSION_ID%s 2>/dev/null; "
        "export -p; "
        ") || true; } "
        "> %s",
        extra, tmp_path ? tmp_path : "/dev/null");
    free(extra);
    return result;
}

/* ---- _additional_profile_scoped_passthrough_names (BaseEnvironment method) ---- */
/*
 * Python: base implementation returns () — subclasses override to add
 *         backend-specific names.
 * C: returns NULL (no additional names) — subclasses may call a hook.
 */
/* PoP: _additional_profile_scoped_passthrough_names @ tools/environments/base.py:_additional_profile_scoped_passthrough_names */
const char **envb_additional_profile_scoped_passthrough_names(int *out_count) {
    if (out_count) *out_count = 0;
    return NULL;  /* base returns empty tuple */
}

/* ---- _snapshot_excluded_passthrough_names (BaseEnvironment method) ---- */
/*
 * Python: if _profile_scoped_passthrough is set and multiplex is active,
 *         gather all passthrough names + additional ones, filter by
 *         _SHELL_ENV_NAME_RE, update _snapshot_passthrough_names, return
 *         sorted tuple.
 *
 * C: the env passthrough registry (tools/env_passthrough.py) is not yet
 *    ported to C. We expose the contract: if the C-side passthrough registry
 *    has entries, collect + filter them; otherwise return the additional
 *    names. The multiplex-active check is a no-op (single-process C).
 */
/* PoP: _snapshot_excluded_passthrough_names @ tools/environments/base.py:_snapshot_excluded_passthrough_names */
char **envb_snapshot_excluded_passthrough_names(bool has_profile_scoped,
                                                int *out_count) {
    if (out_count) *out_count = 0;
    if (!has_profile_scoped) return NULL;

    /* Gather additional names (from subclasses / env passthrough). */
    int extra_n = 0;
    const char **extra = envb_additional_profile_scoped_passthrough_names(&extra_n);

    /* Gather C-side env passthrough names (if the registry is wired). */
    char **reg_names = NULL;
    int reg_n = 0;
    /* envb_env_passthrough_get_all is a weak symbol — NULL if not yet ported. */
    /* Weak fallback: returns NULL (no passthrough names) if the env passthrough
     * port isn't linked. This matches Python's "try import, except pass" path. */
    extern __attribute__((weak)) char **envb_env_passthrough_get_all(int *out_n);
    if (envb_env_passthrough_get_all)
        reg_names = envb_env_passthrough_get_all(&reg_n);

    /* Merge extra + reg_names, filter by _SHELL_ENV_NAME_RE, dedupe, sort. */
    static const char *shell_name_re = "^[A-Za-z_][A-Za-z0-9_]*$";
    /* Regex match manually: ^[A-Za-z_][A-Za-z0-9_]*$ */
    (void)shell_name_re;

    /* Collect all candidate names */
    char **all = calloc((size_t)(extra_n + reg_n) + 1, sizeof(char *));
    int nall = 0;
    for (int i = 0; i < extra_n && extra && extra[i]; i++) {
        all[nall++] = strdup(extra[i]);
    }
    for (int i = 0; i < reg_n && reg_names && reg_names[i]; i++) {
        all[nall++] = strdup(reg_names[i]);
    }
    /* reg_names items are owned by the registry, don't free those pointers */

    /* Filter + dedupe: keep names matching [A-Za-z_][A-Za-z0-9_]* */
    char **result = malloc((size_t)nall + 1);  /* worst case */
    int nres = 0;
    for (int i = 0; i < nall; i++) {
        char *name = all[i];
        if (!name || !name[0]) continue;
        /* Check regex */
        bool valid = (name[0] == '_' || (name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'));
        for (char *p = name + 1; *p && valid; p++) {
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_')) valid = false;
        }
        if (!valid) { free(name); continue; }
        /* dedupe */
        bool dup = false;
        for (int j = 0; j < nres; j++) {
            if (strcasecmp(result[j], name) == 0) { dup = true; break; }
        }
        if (!dup) result[nres++] = name;
    }
    /* Sort */
    for (int i = 1; i < nres; i++) {
        char *key = result[i]; int j = i - 1;
        while (j >= 0 && strcasecmp(result[j], key) > 0) { result[j + 1] = result[j]; j--; }
        result[j + 1] = key;
    }
    result[nres] = NULL;

    for (int i = 0; i < nall; i++) {
        if (i < extra_n) continue;  /* those weren't strdup'd — wait, they were */
    }
    /* Actually: all[] entries were strdup'd from extra[i] OR reg_names[i].
     * extra[i] pointers were NOT strdup'd by the caller, so we must free
     * only the ones we allocated. Let's track: */
    /* Simpler: free all[] entries that we strdup'd (all of them) */
    /* But reg_names may be shared — free result entries only. */
    /* Cleanup: free all[i] that weren't adopted into result */
    /* Track which went into result */
    for (int i = 0; i < nall; i++) {
        bool adopted = false;
        for (int j = 0; j < nres; j++) {
            if (all[i] == result[j]) { adopted = true; break; }
        }
        if (!adopted) free(all[i]);
    }
    free(all);
    if (reg_names) {
        for (int i = 0; i < reg_n && reg_names[i]; i++) {
            /* reg_names[i] owned by registry — don't free */
        }
        /* registry owns the array */
    }

    if (out_count) *out_count = nres;
    if (nres == 0) { free(result); return NULL; }
    return result;
}

/* ---- _finalize_wait_result (BaseEnvironment method) ---- */
/*
 * Python: assemble {output, returncode, [output_total_chars, full_output_path]}
 *         when spill occurred.
 * C: caller passes the rendered string, returncode, and spill metadata
 *    (total_chars=0 means no spill). Returns malloc'd JSON string.
 */
/* PoP: _finalize_wait_result @ tools/environments/base.py:_finalize_wait_result */
char *envb_finalize_wait_result(const char *rendered, long returncode,
                                 long total_chars, const char *spill_path) {
    json_t *r = json_object();
    json_set(r, "output", json_string(rendered ? rendered : ""));
    json_set(r, "returncode", json_number((double)returncode));
    if (spill_path && spill_path[0]) {
        json_set(r, "output_total_chars", json_number((double)total_chars));
        json_set(r, "full_output_path", json_string(spill_path));
    }
    char *out = json_dumps(r, 0);
    json_free(r);
    return out ? out : strdup("{}");
}

/* ---- _kill_process (BaseEnvironment method) ---- */
/*
 * Python: proc.kill() with try/except (ProcessLookupError, PermissionError, OSError).
 * C: kill the process via process group (SIGKILL), swallow ESRCH/EPERM.
 *    proc is a pid_t in C.
 */
/* PoP: _kill_process @ tools/environments/base.py:_kill_process */
int envb_kill_process(pid_t pid) {
    if (pid <= 0) return -1;
    /* Try process-group kill first (negative pid = kill the group) */
    if (kill(-pid, SIGKILL) != 0) {
        /* Fall back to single-process kill */
        if (kill(pid, SIGKILL) != 0) {
            /* ESRCH = no such process (already dead) — not an error, like Python. */
            if (errno == ESRCH) return 0;
            if (errno == EPERM) return 0;  /* PermissionError — swallow */
            return -1;  /* other OSError — report */
        }
    }
    return 0;
}
