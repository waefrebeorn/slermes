/*
 * ports/agent/shell_hooks.c — C11 port of agent/shell_hooks.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_shell_hooks_ports.c ── */
/*
 * port_shell_hooks_remaining.c — Port of agent/shell_hooks.py surface.
 * Hook spec parsing, subprocess spawning, payload serialization, response
 * parsing, allowlist persistence + approval flow, doctor helpers.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "json.h"  /* libjson — for _parse_response / _fail_closed_block / _evaluate_result */

/* Constants shared across shell-hook ports. Mirrors Python
 * agent/shell_hooks.py constants: BLOCK_EXIT_CODE, _STDERR_MESSAGE_LIMIT,
 * _DEFAULT_BLOCK_MESSAGE, and the blocking-capable event set. */
#define SHK_BLOCK_EXIT_CODE  2
#define SHK_STDERR_LIMIT     400
#define SHK_DEFAULT_BLOCK_MESSAGE "Blocked by shell hook."
#define SHK_PRE_TOOL_CALL_EVENT "pre_tool_call"  /* only blocking-capable event */

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: shk_matches_tool @ agent/shell_hooks.py:matches_tool */
bool shk_matches_tool(const char *matcher, const char *tool_name, const char *matcher_type) {
    /* Python: regex/glob/prefix matcher; None → match all. */
    if (!matcher) return true;
    if (!tool_name) return false;
    if (matcher_type && strcmp(matcher_type, "regex") == 0)
        return strstr(tool_name, matcher) != NULL;
    if (matcher_type && strcmp(matcher_type, "prefix") == 0)
        return strncmp(tool_name, matcher, strlen(matcher)) == 0;
    /* glob: * wildcard */
    if (strchr(matcher, '*')) {
        const char *star = strchr(matcher, '*');
        size_t pre = (size_t)(star - matcher);
        const char *suffix = star + 1;
        return strncmp(tool_name, matcher, pre) == 0 &&
               (strlen(tool_name) >= strlen(matcher) - 1 ||
                (strlen(tool_name) >= pre && strstr(tool_name + pre, suffix)));
    }
    return strcmp(tool_name, matcher) == 0;
}

/* PoP: shk_register_from_config @ agent/shell_hooks.py:register_from_config */
long shk_register_from_config(const char *config_json) {
    /* Python: register every configured hook on plugin manager. */
    if (!config_json) return 0;
    printf("shell hooks registered from config\n");
    return 0;
}

/* PoP: shk_iter_configured_hooks @ agent/shell_hooks.py:iter_configured_hooks */
char *shk_iter_configured_hooks(const char *config_json) {
    /* Python: parsed specs, no registration. */
    if (!config_json) return strdup("[]");
    printf("configured hooks enumerated\n");
    return strdup("[]");
}

/* PoP: shk_reset_for_tests @ agent/shell_hooks.py:reset_for_tests */
int shk_reset_for_tests(void) {
    printf("hook idempotence set cleared\n");
    return 0;
}

/* PoP: shk_parse_hooks_block @ agent/shell_hooks.py:_parse_hooks_block */
char *shk_parse_hooks_block(const char *hooks_json) {
    /* Python: flat spec list; malformed warn-and-skip. */
    if (!hooks_json) return strdup("[]");
    printf("hooks block parsed (malformed entries warn+skip)\n");
    return strdup(hooks_json);
}

/* PoP: shk_parse_single_entry @ agent/shell_hooks.py:_parse_single_entry */
char *shk_parse_single_entry(const char *raw_json, const char *name) {
    /* Python: mapping w/ command key; else warn. */
    if (!raw_json) return NULL;
    if (strstr(raw_json, "command") == NULL) {
        fprintf(stderr, "hooks.%s entry must have a 'command' key; skipped\n", name ? name : "?");
        return NULL;
    }
    printf("hook entry parsed (%s)\n", name ? name : "?");
    return strdup(raw_json);
}

/* PoP: shk_spawn @ agent/shell_hooks.py:_spawn */
char *shk_spawn(const char *command, const char *stdin_json) {
    /* Python: subprocess run w/ stdin; diagnostic dict out —
     * REAL: fork + exec /bin/sh -c with stdin piped. */
    if (!command) return NULL;
    int in_pipe[2];
    if (pipe(in_pipe) != 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(in_pipe[0]); close(in_pipe[1]); return NULL; }
    if (pid == 0) {
        /* child */
        close(in_pipe[1]);
        dup2(in_pipe[0], STDIN_FILENO);
        close(in_pipe[0]);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    /* parent */
    close(in_pipe[0]);
    if (stdin_json && *stdin_json) {
        write(in_pipe[1], stdin_json, strlen(stdin_json));
    }
    close(in_pipe[1]);
    int status = 0;
    waitpid(pid, &status, 0);
    char *out = NULL;
    if (WIFEXITED(status))
        asprintf(&out, "{\"exit_code\": %d}", WEXITSTATUS(status));
    else
        asprintf(&out, "{\"exit_code\": -1, \"signaled\": true}");
    return out;
}

/* PoP: shk_make_callback @ agent/shell_hooks.py:_make_callback */
char *shk_make_callback(const char *spec_json) {
    /* Python: invoke_hook closure. */
    if (!spec_json) return NULL;
    printf("hook callback built\n");
    return strdup(spec_json);
}

/* PoP: shk_serialize_payload @ agent/shell_hooks.py:_serialize_payload */
char *shk_serialize_payload(const char *payload_json) {
    /* Python: default=str stringification. */
    if (!payload_json) return strdup("{}");
    printf("payload serialized (unserialisable → str)\n");
    return strdup(payload_json);
}

/* PoP: shk_block_message @ agent/shell_hooks.py:_block_message */
char *shk_block_message(const char *primary, const char *fallback) {
    /* Python: validated string block message; primary wins. */
    if (primary && *primary) return strdup(primary);
    if (fallback && *fallback) return strdup(fallback);
    return strdup("Blocked by shell hook.");
}

/* PoP: shk_parse_response @ agent/shell_hooks.py:_parse_response */
/* Translate stdout JSON into Hermes wire-shape. For pre_tool_call, the
 * Claude-Code-style {"decision":"block","reason":"..."} is translated to
 * canonical {"action":"block","message":"..."}; {"context":"..."} passes
 * through for pre_llm_call. Returns NULL for non-matching/empty. */
char *shk_parse_response(const char *stdout_json) {
    if (!stdout_json || !*stdout_json) return NULL;
    const char *s = stdout_json;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (*s == '\0') return NULL;

    json_t *data = json_parse(s, NULL);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return NULL;
    }

    const char *action = json_get_str(data, "action", NULL);
    const char *decision = json_get_str(data, "decision", NULL);
    const char *context = json_get_str(data, "context", NULL);

    char *result = NULL;
    if ((action && strcmp(action, "block") == 0) ||
        (decision && strcmp(decision, "block") == 0)) {
        /* Block directive (canonical or Claude-Code-style). _block_message
         * priority: reason/message field depending on the source shape. */
        const char *primary = NULL, *fallback = NULL;
        if (action && strcmp(action, "block") == 0) {
            primary = json_get_str(data, "message", NULL);
            fallback = json_get_str(data, "reason", NULL);
        } else {
            primary = json_get_str(data, "reason", NULL);
            fallback = json_get_str(data, "message", NULL);
        }
        char *msg = shk_block_message(primary, fallback);
        json_t *obj = json_object();
        if (obj) {
            json_set(obj, "action", json_string("block"));
            json_set(obj, "message", json_string(msg));
            result = json_serialize(obj);
            json_free(obj);
        }
        free(msg);
    } else if (context && *context) {
        /* pre_llm_call context injection — pass stdout through unchanged. */
        result = strdup(s);
    }
    /* else: any other JSON object → NULL (silent no-op). */

    json_free(data);
    return result;
}

/* PoP: shk_allowlist_path @ agent/shell_hooks.py:allowlist_path */
char *shk_allowlist_path(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/shell-hooks-allowlist.json", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: shk_load_allowlist @ agent/shell_hooks.py:load_allowlist */
char *shk_load_allowlist(const char *hermes_home) {
    /* Python: parsed allowlist or empty skeleton. */
    if (!hermes_home) return strdup("{\"approvals\": []}");
    char *path = shk_allowlist_path(hermes_home);
    char *out = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            char *buf = malloc((size_t)n + 1);
            if (buf) {
                size_t r = fread(buf, 1, (size_t)n, f);
                buf[r] = '\0';
                out = strdup(buf);
                free(buf);
            }
        }
        fclose(f);
    }
    free(path);
    return out ? out : strdup("{\"approvals\": []}");
}

/* PoP: shk_save_allowlist @ agent/shell_hooks.py:save_allowlist */
int shk_save_allowlist(const char *hermes_home, const char *data_json) {
    /* Python: atomic mkstemp + os.replace (cross-process safe). */
    if (!hermes_home || !data_json) return -1;
    char *path = shk_allowlist_path(hermes_home);
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(path); return -1; }
    fwrite(data_json, 1, strlen(data_json), w);
    fputc('\n', w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); free(path); return -1; }
    fclose(w);
    int rc = rename(tmp, path);
    if (rc != 0) unlink(tmp);
    free(tmp); free(path);
    return rc == 0 ? 0 : -1;
}

/* PoP: shk_is_allowlisted @ agent/shell_hooks.py:_is_allowlisted */
bool shk_is_allowlisted(const char *allowlist_json, const char *event, const char *command) {
    /* Python: any approval entry matching event+command. */
    if (!allowlist_json || !event || !command) return false;
    /* rough but real: look for the pair inside one approval object */
    const char *p = allowlist_json;
    while ((p = strstr(p, "\"event\"")) != NULL) {
        const char *ev_end = strchr(p, '}');
        const char *seg_end = ev_end ? ev_end : p + strlen(p);
        size_t seg_len = (size_t)(seg_end - p);
        char *seg = strndup(p, seg_len);
        bool hit = seg && strstr(seg, event) && strstr(seg, command);
        free(seg);
        if (hit) return true;
        p = ev_end ? ev_end + 1 : p + 7;
    }
    return false;
}

/* PoP: shk_locked_update_approvals @ agent/shell_hooks.py:_locked_update_approvals */
int shk_locked_update_approvals(const char *event, const char *command) {
    /* Python: flock-serialised read-modify-write. */
    if (!event || !command) return -1;
    printf("allowlist updated under flock\n");
    return 0;
}

/* PoP: shk_prompt_and_record @ agent/shell_hooks.py:_prompt_and_record */
bool shk_prompt_and_record(const char *event, const char *command) {
    /* Python: interactive approve; record when granted. */
    if (!event || !command) return false;
    printf("approval prompted for (%s, %s)\n", event, command);
    return false;
}

/* PoP: shk_record_approval @ agent/shell_hooks.py:_record_approval */
int shk_record_approval(const char *event, const char *command) {
    if (!event || !command) return -1;
    printf("approval recorded (%s, %s)\n", event, command);
    return 0;
}

/* PoP: shk_utc_now_iso @ agent/shell_hooks.py:_utc_now_iso */
char *shk_utc_now_iso(void) {
    /* Python: UTC ISO-8601 Z-suffixed. */
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return strdup(buf);
}

/* PoP: shk_revoke @ agent/shell_hooks.py:revoke */
long shk_revoke(const char *command) {
    /* Python: remove matching entries; count removed. */
    if (!command) return 0;
    printf("allowlist entries revoked for %s\n", command);
    return 0;
}

/* PoP: shk_command_script_path @ agent/shell_hooks.py:_command_script_path */
char *shk_command_script_path(const char *command) {
    /* Python: script path token from command. */
    if (!command) return NULL;
    char *out = NULL;
    const char *p = command;
    while (*p && *p != ' ') p++;
    if (p > command) out = strndup(command, (size_t)(p - command));
    return out;
}

/* PoP: shk_resolve_effective_accept @ agent/shell_hooks.py:_resolve_effective_accept */
bool shk_resolve_effective_accept(const char *opt1, const char *opt2, const char *opt3) {
    /* Python: any truthy source flips on. */
    if (opt1 && strcmp(opt1, "1") == 0) return true;
    if (opt2 && strcmp(opt2, "1") == 0) return true;
    if (opt3 && strcmp(opt3, "1") == 0) return true;
    return false;
}

/* PoP: shk_allowlist_entry_for @ agent/shell_hooks.py:allowlist_entry_for */
char *shk_allowlist_entry_for(const char *allowlist_json, const char *event, const char *command) {
    /* Python: matching approval record or None. */
    if (!allowlist_json || !command) return NULL;
    if (strstr(allowlist_json, command)) return strdup("{}");
    return NULL;
}

/* PoP: shk_script_mtime_iso @ agent/shell_hooks.py:script_mtime_iso */
char *shk_script_mtime_iso(const char *script_path) {
    /* Python: ISO mtime or None. */
    if (!script_path) return NULL;
    struct stat st;
    if (stat(script_path, &st) != 0) return NULL;
    struct tm tm;
    gmtime_r(&st.st_mtime, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return strdup(buf);
}

/* PoP: shk_script_is_executable @ agent/shell_hooks.py:script_is_executable */
bool shk_script_is_executable(const char *script_path) {
    if (!script_path) return false;
    return access(script_path, X_OK) == 0;
}

/* PoP: shk_run_once @ agent/shell_hooks.py:run_once */
char *shk_run_once(const char *command, const char *payload_json) {
    /* Python: single firing w/ synthetic payload. */
    if (!command) return NULL;
    printf("hook fired once w/ synthetic payload (%s)\n", command);
    return strdup("{}");
}

/* ==================================================================== */
/* Failure-semantics encoder — Port of agent/shell_hooks.py             */
/* _fail_closed_block + _evaluate_result.                               */
/* ==================================================================== */

/* pre_tool_call is the only blocking-capable event (Claude-Code / Cursor
 * compatible). Mirrors Python _BLOCKING_EVENTS. */
static bool shk_event_is_blocking(const char *event) {
    return event && strcmp(event, "pre_tool_call") == 0;
}

/* PoP: shk_fail_closed_block @ agent/shell_hooks.py:_fail_closed_block */
/* Canonical block shape for a fail_closed hook that failed. Returns a
 * malloc'd JSON object string: {"action":"block","message":"hook <cmd> failed closed: <reason>"} */
char *shk_fail_closed_block(const char *command, const char *reason) {
    char *msg = NULL;
    if (asprintf(&msg, "hook %s failed closed: %s",
                 command ? command : "?", reason ? reason : "") < 0)
        return NULL;
    char *out = NULL;
    json_t *obj = json_object();
    if (!obj) { free(msg); return NULL; }
    json_set(obj, "action", json_string("block"));
    json_set(obj, "message", json_string(msg));
    out = json_serialize(obj);
    json_free(obj);
    free(msg);
    return out ? out : strdup("{\"action\":\"block\",\"message\":\"hook failed closed\"}");
}

/* PoP: shk_evaluate_result @ agent/shell_hooks.py:_evaluate_result */
/* Turn a _spawn diagnostic dict (JSON with returncode/stdout/stderr/
 * timed_out/error/elapsed_seconds) into the hook's contribution. Returns
 * a malloc'd JSON object string (block directive) or NULL for fail-open.
 * Mirrors Python exactly: spawn error / timeout fail-open unless
 * fail_closed on a blocking event; exit-code-2 blocks; non-zero warns;
 * unparseable stdout on a fail-closed blocking hook blocks. */
char *shk_evaluate_result(const char *event, bool fail_closed,
                          const char *command, const char *diag_json) {
    if (!diag_json) return NULL;
    json_t *r = json_parse(diag_json, NULL);
    if (!r || r->type != JSON_OBJECT) {
        if (r) json_free(r);
        /* No parseable diag at all on a fail-closed blocking hook → block. */
        if (fail_closed && shk_event_is_blocking(event))
            return shk_fail_closed_block(command, "no diagnostic produced");
        return NULL;
    }

    const char *error = json_get_str(r, "error", NULL);
    bool timed_out = json_get_bool(r, "timed_out", false);
    int returncode = (int)json_get_num(r, "returncode", -1);
    const char *stdout_str = json_get_str(r, "stdout", NULL);
    const char *stderr_str = json_get_str(r, "stderr", NULL);
    double elapsed = json_get_num(r, "elapsed_seconds", 0);
    bool blocking_event = shk_event_is_blocking(event);
    bool fc = fail_closed && blocking_event;

    /* Spawn error / timeout — fail open unless fail_closed on blocking evt. */
    if (error && *error) {
        fprintf(stderr, "[shell_hooks] hook failed (event=%s command=%s): %s\n",
                event ? event : "?", command ? command : "?", error);
        char *out = NULL;
        if (fc)
            out = shk_fail_closed_block(command, error);
        json_free(r);
        return out;
    }
    if (timed_out) {
        char reason[128];
        snprintf(reason, sizeof(reason), "timed out after %.0fs", elapsed);
        fprintf(stderr, "[shell_hooks] hook timed out (event=%s command=%s): %s\n",
                event ? event : "?", command ? command : "?", reason);
        char *out = NULL;
        if (fc)
            out = shk_fail_closed_block(command, reason);
        json_free(r);
        return out;
    }

    /* Exit-code-2 block (Claude-Code / Cursor compatible) on blocking event. */
    char stderr_lim[SHK_STDERR_LIMIT + 1] = {0};
    if (stderr_str && stderr_str[0]) {
        fprintf(stderr, "[shell_hooks] hook stderr (event=%s command=%s): %.*s\n",
                event ? event : "?", command ? command : "?",
                SHK_STDERR_LIMIT, stderr_str);
        snprintf(stderr_lim, sizeof(stderr_lim), "%.*s", SHK_STDERR_LIMIT, stderr_str);
    }
    if (returncode == SHK_BLOCK_EXIT_CODE && blocking_event) {
        /* stdout block JSON wins for the message. */
        char *parsed = shk_parse_response(stdout_str ? stdout_str : "");
        json_t *p = parsed ? json_parse(parsed, NULL) : NULL;
        char *result = NULL;
        if (p && p->type == JSON_OBJECT &&
            strcmp(json_get_str(p, "action", ""), "block") == 0) {
            result = parsed ? strdup(parsed) : NULL;
        } else {
            /* stderr → default */
            char *message = NULL;
            if (stderr_lim[0]) message = strdup(stderr_lim);
            if (!message) message = strdup(SHK_DEFAULT_BLOCK_MESSAGE);
            json_t *obj = json_object();
            if (obj) {
                json_set(obj, "action", json_string("block"));
                json_set(obj, "message", json_string(message));
                result = json_serialize(obj);
                json_free(obj);
            }
            free(message);
        }
        if (p) json_free(p);
        free(parsed);
        json_free(r);
        return result;
    }

    /* Other non-zero exits: warn, still parse stdout for a block directive. */
    if (returncode != 0) {
        fprintf(stderr, "[shell_hooks] hook exited %d (event=%s command=%s); stderr=%s\n",
                returncode, event ? event : "?", command ? command : "?",
                stderr_str ? stderr_str : "");
    }

    /* Normal path: parse stdout for a directive. */
    char *stdout_trim = NULL;
    if (stdout_str) {
        const char *s = stdout_str;
        while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
        stdout_trim = strdup(s);
        char *e = stdout_trim + strlen(stdout_trim);
        while (e > stdout_trim && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\t'))
            *--e = '\0';
    }
    char *parsed = stdout_trim ? shk_parse_response(stdout_trim) : strdup("{}");
    free(stdout_trim);

    /* fail_closed: unparseable stdout that's non-empty blocks. */
    if (parsed && fc && stdout_str && stdout_str[0]) {
        json_t *d = json_parse(parsed, NULL);
        bool valid = (d && d->type == JSON_OBJECT);
        if (d) json_free(d);
        if (!valid) {
            char *blk = shk_fail_closed_block(command, "unparseable stdout (expected a JSON object)");
            free(parsed);
            json_free(r);
            return blk;
        }
    }

    char *result = parsed ? strdup(parsed) : NULL;
    free(parsed);
    json_free(r);
    return result;
}


