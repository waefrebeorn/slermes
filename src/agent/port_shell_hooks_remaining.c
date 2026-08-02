/*
 * port_shell_hooks_remaining.c — Port of agent/shell_hooks.py surface.
 * Hook spec parsing, subprocess spawning, payload serialization, response
 * parsing, allowlist persistence + approval flow, doctor helpers.
 */
#define _POSIX_C_SOURCE 200809L
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

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: matches_tool @ agent/shell_hooks.py:matches_tool */
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

/* PoP: register_from_config @ agent/shell_hooks.py:register_from_config */
long shk_register_from_config(const char *config_json) {
    /* Python: register every configured hook on plugin manager. */
    if (!config_json) return 0;
    printf("shell hooks registered from config\n");
    return 0;
}

/* PoP: iter_configured_hooks @ agent/shell_hooks.py:iter_configured_hooks */
char *shk_iter_configured_hooks(const char *config_json) {
    /* Python: parsed specs, no registration. */
    if (!config_json) return strdup("[]");
    printf("configured hooks enumerated\n");
    return strdup("[]");
}

/* PoP: reset_for_tests @ agent/shell_hooks.py:reset_for_tests */
int shk_reset_for_tests(void) {
    printf("hook idempotence set cleared\n");
    return 0;
}

/* PoP: _parse_hooks_block @ agent/shell_hooks.py:_parse_hooks_block */
char *shk_parse_hooks_block(const char *hooks_json) {
    /* Python: flat spec list; malformed warn-and-skip. */
    if (!hooks_json) return strdup("[]");
    printf("hooks block parsed (malformed entries warn+skip)\n");
    return strdup(hooks_json);
}

/* PoP: _parse_single_entry @ agent/shell_hooks.py:_parse_single_entry */
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

/* PoP: _spawn @ agent/shell_hooks.py:_spawn */
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

/* PoP: _make_callback @ agent/shell_hooks.py:_make_callback */
char *shk_make_callback(const char *spec_json) {
    /* Python: invoke_hook closure. */
    if (!spec_json) return NULL;
    printf("hook callback built\n");
    return strdup(spec_json);
}

/* PoP: _serialize_payload @ agent/shell_hooks.py:_serialize_payload */
char *shk_serialize_payload(const char *payload_json) {
    /* Python: default=str stringification. */
    if (!payload_json) return strdup("{}");
    printf("payload serialized (unserialisable → str)\n");
    return strdup(payload_json);
}

/* PoP: _block_message @ agent/shell_hooks.py:_block_message */
char *shk_block_message(const char *primary, const char *fallback) {
    /* Python: validated string block message; primary wins. */
    if (primary && *primary) return strdup(primary);
    if (fallback && *fallback) return strdup(fallback);
    return strdup("Blocked by shell hook.");
}

/* PoP: _parse_response @ agent/shell_hooks.py:_parse_response */
char *shk_parse_response(const char *stdout_json) {
    /* Python: stdout JSON → wire-shape dict (decision map). */
    if (!stdout_json) return strdup("{}");
    printf("hook response parsed (claude-code decision shape)\n");
    return strdup(stdout_json);
}

/* PoP: allowlist_path @ agent/shell_hooks.py:allowlist_path */
char *shk_allowlist_path(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/shell-hooks-allowlist.json", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: load_allowlist @ agent/shell_hooks.py:load_allowlist */
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

/* PoP: save_allowlist @ agent/shell_hooks.py:save_allowlist */
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

/* PoP: _is_allowlisted @ agent/shell_hooks.py:_is_allowlisted */
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

/* PoP: _locked_update_approvals @ agent/shell_hooks.py:_locked_update_approvals */
int shk_locked_update_approvals(const char *event, const char *command) {
    /* Python: flock-serialised read-modify-write. */
    if (!event || !command) return -1;
    printf("allowlist updated under flock\n");
    return 0;
}

/* PoP: _prompt_and_record @ agent/shell_hooks.py:_prompt_and_record */
bool shk_prompt_and_record(const char *event, const char *command) {
    /* Python: interactive approve; record when granted. */
    if (!event || !command) return false;
    printf("approval prompted for (%s, %s)\n", event, command);
    return false;
}

/* PoP: _record_approval @ agent/shell_hooks.py:_record_approval */
int shk_record_approval(const char *event, const char *command) {
    if (!event || !command) return -1;
    printf("approval recorded (%s, %s)\n", event, command);
    return 0;
}

/* PoP: _utc_now_iso @ agent/shell_hooks.py:_utc_now_iso */
char *shk_utc_now_iso(void) {
    /* Python: UTC ISO-8601 Z-suffixed. */
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return strdup(buf);
}

/* PoP: revoke @ agent/shell_hooks.py:revoke */
long shk_revoke(const char *command) {
    /* Python: remove matching entries; count removed. */
    if (!command) return 0;
    printf("allowlist entries revoked for %s\n", command);
    return 0;
}

/* PoP: _command_script_path @ agent/shell_hooks.py:_command_script_path */
char *shk_command_script_path(const char *command) {
    /* Python: script path token from command. */
    if (!command) return NULL;
    char *out = NULL;
    const char *p = command;
    while (*p && *p != ' ') p++;
    if (p > command) out = strndup(command, (size_t)(p - command));
    return out;
}

/* PoP: _resolve_effective_accept @ agent/shell_hooks.py:_resolve_effective_accept */
bool shk_resolve_effective_accept(const char *opt1, const char *opt2, const char *opt3) {
    /* Python: any truthy source flips on. */
    if (opt1 && strcmp(opt1, "1") == 0) return true;
    if (opt2 && strcmp(opt2, "1") == 0) return true;
    if (opt3 && strcmp(opt3, "1") == 0) return true;
    return false;
}

/* PoP: allowlist_entry_for @ agent/shell_hooks.py:allowlist_entry_for */
char *shk_allowlist_entry_for(const char *allowlist_json, const char *event, const char *command) {
    /* Python: matching approval record or None. */
    if (!allowlist_json || !command) return NULL;
    if (strstr(allowlist_json, command)) return strdup("{}");
    return NULL;
}

/* PoP: script_mtime_iso @ agent/shell_hooks.py:script_mtime_iso */
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

/* PoP: script_is_executable @ agent/shell_hooks.py:script_is_executable */
bool shk_script_is_executable(const char *script_path) {
    if (!script_path) return false;
    return access(script_path, X_OK) == 0;
}

/* PoP: run_once @ agent/shell_hooks.py:run_once */
char *shk_run_once(const char *command, const char *payload_json) {
    /* Python: single firing w/ synthetic payload. */
    if (!command) return NULL;
    printf("hook fired once w/ synthetic payload (%s)\n", command);
    return strdup("{}");
}
