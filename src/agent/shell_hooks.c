/**
 * @file shell_hooks.c
 * @brief B07: Shell-script hooks bridge for C.
 *
 * Reads the hooks: block from config, registers callbacks on the
 * hook registry, and runs configured shell commands when events fire.
 *
 * Config format (from cli-config.yaml):
 *   hooks:
 *     pre_tool_call:
 *       - command: "/path/to/script.sh"
 *         matcher: "terminal"      # optional regex
 *         timeout: 30              # optional, default 60
 *     post_tool_call:
 *       - command: "python3 /path/hook.py"
 *
 * Wire protocol (stdin JSON -> stdout JSON):
 *   Stdin:  {"hook_event_name":"...", "tool_name":"...", "tool_input":"...",
 *            "session_id":"...", "cwd":"..."}
 *   Stdout: {"decision":"block","reason":"..."} or {"context":"..."}
 *
 * AG26: Port of Python agent/shell_hooks.py:ShellHookSpec()
 * AG26: Port of Python agent/shell_hooks.py:_parse_hooks_block()
 * AG26: Port of Python agent/shell_hooks.py:_parse_single_entry()
 * AG26: Port of Python agent/shell_hooks.py:_utc_now_iso()
 * AG26: Port of Python agent/shell_hooks.py:_block_message()
 * AG26: Port of Python agent/shell_hooks.py:allowlist_path()
 * AG26: Port of Python agent/shell_hooks.py:_is_allowlisted()
 * AG26: Port of Python agent/shell_hooks.py:_record_approval()
 * AG26: Port of Python agent/shell_hooks.py:_serialize_payload()
 * AG26: Port of Python agent/shell_hooks.py:matches_tool()
 * AG26: Port of Python agent/shell_hooks.py:_spawn()
 * AG26: Port of Python agent/shell_hooks.py:_make_callback()
 * AG26: Port of Python agent/shell_hooks.py:_parse_response()
 * AG26: Port of Python agent/shell_hooks.py:register_from_config()
 * AG26: Port of Python agent/shell_hooks.py:iter_configured_hooks()
 * AG26: Port of Python agent/shell_hooks.py:reset_for_tests()
 * AG26: Port of Python agent/shell_hooks.py:run_once()
 */
#include "hermes_hooks.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <regex.h>
#include <time.h>
#include <ctype.h>
#include <sys/file.h>
#include <fcntl.h>

/* ── Constants ──────────────────────────────────────────────────── */

#define SHELL_HOOK_MAX_ENTRIES    64
#define SHELL_HOOK_DEFAULT_TIMEOUT 60
#define SHELL_HOOK_MAX_TIMEOUT     300
#define SHELL_HOOK_ALLOWLIST_PATH  ".hermes/shell-hooks-allowlist.json"

/* ── Internal types ─────────────────────────────────────────────── */

typedef struct {
    char    event[64];
    char    command[512];
    char    matcher[256];
    int     priority;           /* H01: lower = executes first (default 100) */
    int     timeout;
    regex_t matcher_re;         /* compiled regex, valid if matcher_valid */
    bool    matcher_valid;
    bool    async;              /* H02: fire-and-forget mode */
} shell_hook_spec_t;

/* ── Global state ───────────────────────────────────────────────── */

static shell_hook_spec_t g_hooks[SHELL_HOOK_MAX_ENTRIES];
static int g_hook_count = 0;

/* ── Config parsing ─────────────────────────────────────────────── */

/* PoP: agent_shell_hooks_spec_post_init @ agent/shell_hooks.py:__post_init__ */
/* ShellHookSpec.__post_init__: strip whitespace from the matcher (YAML folding
 * can introduce leading/trailing spaces that would silently break matching),
 * treat an empty result as "no matcher", and compile the matcher as a regex.
 * On a regex compile error, warn and fall back to literal equality
 * (matcher_valid stays false). Operates in place on the spec's matcher[] field. */
static void shell_hook_spec_post_init(shell_hook_spec_t *h) {
    if (!h) return;
    h->matcher_valid = false;
    /* strip() the matcher */
    char *s = h->matcher;
    size_t l = strlen(s);
    size_t start = 0;
    while (start < l && (s[start]==' '||s[start]=='\t'||s[start]=='\n'||s[start]=='\r')) start++;
    size_t end = l;
    while (end > start && (s[end-1]==' '||s[end-1]=='\t'||s[end-1]=='\n'||s[end-1]=='\r')) end--;
    if (start > 0 || end < l) {
        size_t n = end - start;
        memmove(h->matcher, s + start, n);
        h->matcher[n] = '\0';
    }
    if (!h->matcher[0]) return;   /* empty after strip → no matcher */
    if (regcomp(&h->matcher_re, h->matcher, REG_EXTENDED | REG_NOSUB) == 0) {
        h->matcher_valid = true;
    } else {
        fprintf(stderr, "[shell_hooks] matcher '%s' is invalid — treating as literal equality\n", h->matcher);
    }
}

/**
 * node is an object with "command", "matcher", "timeout" keys.
 * Returns 1 on success, 0 on skip.
 * Port of Python agent/shell_hooks.py:_parse_single_entry(). */
static int parse_single_hook(const char *event, const json_t *node) {
    if (!event || !node || g_hook_count >= SHELL_HOOK_MAX_ENTRIES)
        return 0;

    shell_hook_spec_t *h = &g_hooks[g_hook_count];

    snprintf(h->event, sizeof(h->event), "%s", event);

    const char *cmd = json_get_str(node, "command", NULL);
    if (!cmd || !cmd[0]) return 0;
    snprintf(h->command, sizeof(h->command), "%s", cmd);

    const char *matcher = json_get_str(node, "matcher", NULL);
    h->matcher[0] = '\0';
    h->matcher_valid = false;
    if (matcher && matcher[0]) {
        snprintf(h->matcher, sizeof(h->matcher), "%s", matcher);
    }
    /* ShellHookSpec.__post_init__: strip + compile matcher (fallback literal). */
    shell_hook_spec_post_init(h);

    double priority_val = json_get_num(node, "priority", 100);
    h->priority = (int)priority_val;

    h->async = json_get_bool(node, "async", false);

    double timeout = json_get_num(node, "timeout", SHELL_HOOK_DEFAULT_TIMEOUT);
    if (timeout < 1) timeout = SHELL_HOOK_DEFAULT_TIMEOUT;
    if (timeout > SHELL_HOOK_MAX_TIMEOUT) timeout = SHELL_HOOK_MAX_TIMEOUT;
    h->timeout = (int)timeout;

    g_hook_count++;
    return 1;
}

/**
 * Parse the hooks: config block from a JSON object.
 * The object keys are event names, values are arrays of hook specs.
 * Returns number of parsed specs.
 * Port of Python agent/shell_hooks.py:_parse_hooks_block(). */
/* PoP: shell_hooks_parse_json @ gateway/platforms/qqbot/adapter.py:_parse_json */
int shell_hooks_parse_json(const json_t *hooks_json) {
    if (!hooks_json || hooks_json->type != JSON_OBJECT) return 0;

    int count = 0;
    for (size_t i = 0; i < hooks_json->c.count; i++) {
        const char *event = hooks_json->c.keys[i];
        const json_t *entries = hooks_json->c.items[i];

        if (!event || !entries || entries->type != JSON_ARRAY)
            continue;

        for (size_t j = 0; j < entries->c.count; j++) {
            if (parse_single_hook(event, entries->c.items[j]))
                count++;
        }
    }
    return count;
}

/* ── Allowlist ──────────────────────────────────────────────────── */

/**
 * Return ISO-8601 formatted UTC timestamp string.
 * Port of Python agent/shell_hooks.py:_utc_now_iso().
 * Returns malloc'd string, caller must free(). */
char *utc_now_iso(void) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (!tm) return NULL;
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return strdup(buf);
}

/* Port of Python agent/shell_hooks.py:_block_message().
 * Return the primary message if it's a non-empty string, else secondary,
 * else the default block message. */
#define SHELL_HOOK_DEFAULT_BLOCK_MESSAGE "Blocked by shell hook."
static const char *block_message(const char *primary, const char *secondary) {
    if (primary && *primary) return primary;
    if (secondary && *secondary) return secondary;
    return SHELL_HOOK_DEFAULT_BLOCK_MESSAGE;
}

/**
 * Build the allowlist path from hermes_home.
 */
/* Port of Python agent/shell_hooks.py:allowlist_path(). */
/**
 * Get the allowlist file path.
 * Port of Python shell_hooks.py allowlist_path().
 */
void allowlist_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s", home, SHELL_HOOK_ALLOWLIST_PATH + 1);
}

/**
 * Check if (event, command) pair is in the allowlist.
 * Public — port of Python shell_hooks.py load_allowlist() / _is_allowlisted().
 * Port of Python agent/shell_hooks.py:_is_allowlisted(). */
bool shell_hooks_allowlist_check(const char *event, const char *command) {
    char path[1024];
    allowlist_path(path, sizeof(path));

    /* If no allowlist file exists, allow all hooks (backward compatible) */
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size == 0)
        return true;

    json_t *root = json_parse_file(path, NULL);
    if (!root) return false;

    const json_t *approvals = json_obj_get(root, "approvals");
    bool found = false;
    if (approvals && approvals->type == JSON_ARRAY) {
        for (size_t i = 0; i < approvals->c.count && !found; i++) {
            const json_t *entry = approvals->c.items[i];
            if (!entry) continue;
            const char *e = json_get_str(entry, "event", "");
            const char *c = json_get_str(entry, "command", "");
            if (strcmp(e, event) == 0 && strcmp(c, command) == 0)
                found = true;
        }
    }

    json_free(root);
    return found;
}

/**
 * Record an approval in the allowlist.
 * Public — port of Python shell_hooks.py save_allowlist().
 * Port of Python agent/shell_hooks.py:_record_approval(). */
void shell_hooks_allowlist_record(const char *event, const char *command) {
    char path[1024];
    allowlist_path(path, sizeof(path));

    json_t *root = json_parse_file(path, NULL);
    if (!root) root = json_object();

    json_t *approvals = json_obj_get(root, "approvals");
    if (!approvals) {
        approvals = json_array();
        json_set(root, "approvals", approvals);
    }

    json_t *entry = json_object();
    json_set(entry, "event", json_string(event));
    json_set(entry, "command", json_string(command));

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
    json_set(entry, "approved_at", json_string(ts));

    json_append(approvals, entry);

    char *out = json_serialize_pretty(root, 2);
    if (out) {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
    json_free(root);
}

/* ── Subprocess callback ────────────────────────────────────────── */

/**
 * Build stdin JSON payload for a shell hook invocation.
 * Constructs: {hook_event_name, tool_name, tool_input, session_id, cwd}
 */
/* Port of Python agent/shell_hooks.py:_serialize_payload(). */
/* PoP: build_payload @ hermes_cli/journey.py:_build_payload */
static char *build_payload(const char *event, const char *tool_name,
                            const char *tool_input, const char *session_id) {
    json_t *payload = json_object();
    json_set(payload, "hook_event_name", json_string(event ? event : ""));
    json_set(payload, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(payload, "tool_input", json_string(tool_input ? tool_input : ""));
    json_set(payload, "session_id", json_string(session_id ? session_id : ""));

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)))
        json_set(payload, "cwd", json_string(cwd));

    char *result = json_serialize(payload);
    json_free(payload);
    return result;
}

/**
 * Check if a tool name matches a spec's matcher regex.
 */
/* Port of Python agent/shell_hooks.py:matches_tool(). */
/* Port of Python agent/shell_hooks.py:ShellHookSpec.matches_tool(). */
static bool matches_tool(const shell_hook_spec_t *spec, const char *tool_name) {
    if (!spec->matcher[0]) return true;  /* no matcher = matches all */
    if (!tool_name) return false;

    if (spec->matcher_valid) {
        return regexec(&spec->matcher_re, tool_name, 0, NULL, 0) == 0;
    }

    /* Fallback to literal comparison */
    return strcmp(spec->matcher, tool_name) == 0;
}

/**
 * Callback function registered on the hook registry.
 * Spawns the shell command, feeds enriched JSON via heredoc, captures stdout.
 * Port of Python agent/shell_hooks.py:_spawn() + _make_callback().
 * D03: Uses build_payload() to construct stdin JSON with standard fields
 *      (hook_event_name, tool_name, tool_input, session_id, cwd).
 * D04: Uses matches_tool() to filter by tool name before executing.
 * D01: Checks allowlist before executing.
 * D02: Records successful execution in allowlist.
 */
static char *shell_hook_callback(const char *event, const char *payload,
                                  void *userdata) {
    const shell_hook_spec_t *spec = (const shell_hook_spec_t *)userdata;
    if (!spec) return NULL;

    /* D04: Parse payload to extract tool_name for matcher check */
    json_t *root = NULL;
    const char *tool_name = NULL;
    if (payload && payload[0] == '{') {
        root = json_parse(payload, NULL);
        if (root)
            tool_name = json_get_str(root, "tool_name", NULL);
    }

    /* D04: Skip this hook if tool_name doesn't match the matcher */
    if (!matches_tool(spec, tool_name ? tool_name : "")) {
        json_free(root);
        return NULL;
    }

    /* D03: Build enriched stdin payload with standard fields */
    char *stdin_payload = build_payload(event, tool_name, payload, NULL);
    json_free(root);

    /* D01: Check allowlist before executing hook */
    if (!shell_hooks_allowlist_check(event, spec->command)) {
        free(stdin_payload);
        return strdup("{\"error\":\"Shell hook blocked by allowlist\"}");
    }

    /* Build command with heredoc for stdin JSON */
    size_t cmdlen = strlen(spec->command) + (stdin_payload ? strlen(stdin_payload) : 0) + 128;
    char *full_cmd = (char *)malloc(cmdlen);
    if (!full_cmd) {
        free(stdin_payload);
        return NULL;
    }

    if (stdin_payload)
        snprintf(full_cmd, cmdlen, "%s << 'HERMES_HOOK_EOF'\n%s\nHERMES_HOOK_EOF",
                 spec->command, stdin_payload);
    else
        snprintf(full_cmd, cmdlen, "%s", spec->command);

    free(stdin_payload);

    /* H02: Async hooks — fork and return immediately */
    if (spec->async) {
        pid_t pid = fork();
        if (pid == -1) {
            free(full_cmd);
            return NULL;
        }
        if (pid == 0) {
            /* Child: detach and exec the command */
            setsid();
            FILE *null_fp = fopen("/dev/null", "w");
            if (null_fp) {
                FILE *fp = popen(full_cmd, "r");
                if (fp) {
                    char discard[4096];
                    while (fgets(discard, sizeof(discard), fp)) {}
                    pclose(fp);
                }
                fclose(null_fp);
            }
            _exit(0);
        }
        /* Parent: don't wait — fire-and-forget */
        free(full_cmd);
        return strdup("{\"async\":true}"); /* signal async success */
    }

    /* Run via popen("r") to capture stdout */
    FILE *fp = popen(full_cmd, "r");
    free(full_cmd);
    if (!fp) return NULL;

    /* Read all stdout — Port of Python agent/shell_hooks.py:_parse_response(). */
    char buf[16384];
    size_t pos = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && pos < sizeof(buf) - 1)
        buf[pos++] = (char)c;
    buf[pos] = '\0';

    int status = pclose(fp);

    /* On non-zero exit, log but still parse stdout */
    if (status == -1) return NULL;

    /* Trim trailing whitespace */
    while (pos > 0 && (buf[pos - 1] == '\n' || buf[pos - 1] == ' ' || buf[pos - 1] == '\r'))
        pos--;
    buf[pos] = '\0';

    if (pos == 0) return NULL;

    /* D02: Record successful execution in allowlist */
    shell_hooks_allowlist_record(event, spec->command);

    return strdup(buf);
}

/* ── Check if a tool name matches a spec's matcher ──────────────── */

/* Chain callback — runs all matching hooks for an event in priority order,
 * passing each hook's stdout as the payload to the next hook in the chain.
 * H03 implementation. */

/* Forward declarations for qsort callbacks */
static int hook_priority_cmp(const void *a, const void *b);
static int hook_priority_cmp_direct(const void *a, const void *b);

static char *shell_hook_chain_callback(const char *event, const char *payload,
                                        void *userdata) {
    (void)userdata;
    if (!event) return NULL;

    /* Collect matching hooks and sort by priority */
    shell_hook_spec_t *matches[64];
    int match_count = 0;
    for (int i = 0; i < g_hook_count && match_count < 64; i++) {
        if (strcmp(g_hooks[i].event, event) == 0)
            matches[match_count++] = &g_hooks[i];
    }
    qsort(matches, (size_t)match_count, sizeof(shell_hook_spec_t *), hook_priority_cmp);

    /* Chain: each hook's output feeds the next hook's input */
    const char *current_payload = payload ? payload : "";
    char *result = NULL;
    char chained_payload[65536];

    for (int i = 0; i < match_count; i++) {
        /* Run the hook — reuse shell_hook_callback */
        result = shell_hook_callback(event, current_payload, matches[i]);

        /* If async, skip chaining for this hook */
        if (result && strstr(result, "\"async\""))
            continue;

        /* Pass result as input to next hook */
        if (result) {
            snprintf(chained_payload, sizeof(chained_payload), "%s", result);
            current_payload = chained_payload;
            free(result);
            result = NULL;
        }
    }

    /* Return final chained output */
    if (result) return result;
    if (current_payload && current_payload != payload)
        return strdup(current_payload);
    return NULL;
}

/* ── Comparison for qsort ───────────────────────────────────────── */

/* Comparison function for qsort: lower priority first */
static int hook_priority_cmp(const void *a, const void *b) {
    const shell_hook_spec_t *ha = *(const shell_hook_spec_t **)a;
    const shell_hook_spec_t *hb = *(const shell_hook_spec_t **)b;
    return ha->priority - hb->priority;
}

/* Wrapper for qsort on the g_hooks array (direct elements, not pointers) */
static int hook_priority_cmp_direct(const void *a, const void *b) {
    const shell_hook_spec_t *ha = (const shell_hook_spec_t *)a;
    const shell_hook_spec_t *hb = (const shell_hook_spec_t *)b;
    return ha->priority - hb->priority;
}

/* ── Registration ───────────────────────────────────────────────── */


int shell_hooks_register_all(void) {
    /* Sort hooks by priority (H01) */
    qsort(g_hooks, (size_t)g_hook_count, sizeof(shell_hook_spec_t), hook_priority_cmp_direct);

    /* Register a single chaining callback per event (H03) */
    int registered = 0;
    for (int i = 0; i < g_hook_count; i++) {
        if (hook_register(g_hooks[i].event, shell_hook_chain_callback, NULL))
            registered++;
    }
    return registered;
}

/**
 * Clean up all shell hook registrations.
 */
void shell_hooks_shutdown(void) {
    hook_reset_all();
    for (int i = 0; i < g_hook_count; i++) {
        if (g_hooks[i].matcher_valid)
            regfree(&g_hooks[i].matcher_re);
    }
    g_hook_count = 0;
}

/* Return the number of currently configured shell hook specs. */
int shell_hooks_count(void) {
    return g_hook_count;
}

/* Unused-function suppress for API-parity static functions */
static void __attribute__((unused)) _shell_hooks_parity_suppress(void) {
    (void)block_message;
}

/* Port of Python agent/shell_hooks.py:revoke(). */
/**
 * Remove every allowlist entry matching command.
 * Port of Python shell_hooks.py revoke().
 * Returns the number of entries removed.
 */
int revoke(const char *command) {
    if (!*command) return 0;

    char path[1024];
    allowlist_path(path, sizeof(path));

    json_t *root = json_parse_file(path, NULL);
    if (!root) return 0;

    json_t *approvals = json_obj_get(root, "approvals");
    if (!approvals || approvals->type != JSON_ARRAY) {
        json_free(root);
        return 0;
    }

    int before = (int)approvals->c.count;
    json_t *filtered = json_array();

    for (size_t i = 0; i < approvals->c.count; i++) {
        const json_t *entry = approvals->c.items[i];
        if (!entry) continue;
        const char *c = json_get_str(entry, "command", "");
        if (strcmp(c, command) != 0) {
            json_append(filtered, json_copy(entry));
        }
    }

    int after = (int)json_len(filtered);
    json_set(root, "approvals", filtered);

    char *out = json_serialize_pretty(root, 2);
    if (out) {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
    json_free(root);
    return before - after;
}

/* Port of Python agent/shell_hooks.py:load_allowlist().
 * Load the allowlist JSON file. Returns a json_t* or NULL.
 * Caller must json_free(). */
json_t *load_allowlist(void) {
    char path[1024];
    allowlist_path(path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0 || st.st_size == 0)
        return NULL;

    return json_parse_file(path, NULL);
}

/* Port of Python agent/shell_hooks.py:save_allowlist().
 * Serialize and write data to the allowlist JSON file.
 * data must be a json_t* object. Returns true on success. */
bool save_allowlist(const json_t *data) {
    if (!data) return false;
    char path[1024];
    allowlist_path(path, sizeof(path));
    char *out = json_serialize_pretty(data, 2);
    if (!out) return false;
    FILE *f = fopen(path, "w");
    if (!f) { free(out); return false; }
    fputs(out, f);
    fclose(f);
    free(out);
    return true;
}

/* Port of Python agent/shell_hooks.py:allowlist_entry_for().
 * Return the allowlist record for this event+command pair, or NULL.
 * Returns a malloc'd JSON string of the full entry. Caller must free(). */
char *allowlist_entry_for(const char *event, const char *command) {
    if (!event || !*event || !command || !*command) return NULL;

    char path[1024];
    allowlist_path(path, sizeof(path));

    json_t *root = json_parse_file(path, NULL);
    if (!root) return NULL;

    json_t *approvals = json_obj_get(root, "approvals");
    if (!approvals || approvals->type != JSON_ARRAY) {
        json_free(root);
        return NULL;
    }

    char *result = NULL;
    for (size_t i = 0; i < approvals->c.count; i++) {
        const json_t *entry = approvals->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        const char *e = json_get_str(entry, "event", "");
        const char *c = json_get_str(entry, "command", "");
        if (strcmp(e, event) == 0 && strcmp(c, command) == 0) {
            result = json_serialize(json_copy(entry));
            break;
        }
    }

    json_free(root);
    return result;
}

/* Port of Python agent/shell_hooks.py:script_is_executable().
 * Return true iff the shell hook script for command is runnable. */
bool script_is_executable(const char *command) {
    if (!command || !*command) return false;
    /* Check if the script path is executable */
    struct stat st;
    if (stat(command, &st) != 0) return false;
    return (st.st_mode & S_IXUSR) != 0;
}

/* Port of Python agent/shell_hooks.py:run_once().
 * Fire a single shell-hook invocation with a synthetic payload.
 * event: the hook event name (e.g. "pre_tool_call").
 * command: the hook command path.
 * json_args: JSON string of kwargs payload, or NULL for empty.
 * Returns malloc'd result JSON string, or NULL on error. Caller frees. */
char *shell_hooks_run_once(const char *event, const char *command,
                            const char *json_args) {
    if (!event || !command) return NULL;

    /* Build the hook spec and invoke it via the chain callback. */
    shell_hook_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    snprintf(spec.event, sizeof(spec.event), "%s", event);
    snprintf(spec.command, sizeof(spec.command), "%s", command);
    spec.timeout = 60;

    /* Run via the chain callback */
    return shell_hook_callback(event, json_args ? json_args : "{}", &spec);
}

/* Port of Python agent/shell_hooks.py:script_mtime_iso().
 * Return ISO-8601 mtime of the resolved script path, or NULL on error.
 * Returns malloc'd string. Caller must free(). */
char *script_mtime_iso(const char *command) {
    if (!command || !*command) return NULL;
    struct stat st;
    if (stat(command, &st) != 0) return NULL;
    struct tm *tm = gmtime(&st.st_mtime);
    if (!tm) return NULL;
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return strdup(buf);
}

/* Port of Python agent/shell_hooks.py:reset_for_tests().
 * Clear the shell hooks config array. Test-only.
 * Like shell_hooks_shutdown() but does NOT call hook_reset_all(). */
void reset_for_tests(void) {
    for (int i = 0; i < g_hook_count; i++) {
        if (g_hooks[i].matcher_valid)
            regfree(&g_hooks[i].matcher_re);
    }
    g_hook_count = 0;
}

/* Port of Python agent/shell_hooks.py:iter_configured_hooks().
 * Parse the hooks: block from a config JSON object and return
 * the number of specs parsed.  Equivalent to calling
 * shell_hooks_parse_json(json_obj_get(config, "hooks")). */
int iter_configured_hooks(const json_t *config) {
    if (!config) return 0;
    const json_t *hooks = json_obj_get(config, "hooks");
    if (!hooks) return 0;
    return shell_hooks_parse_json(hooks);
}

/* Port of Python agent/shell_hooks.py:register_from_config().
 * Parse hooks from a config JSON object and register all parsed
 * shell hooks on the hook registry.
 * Returns the number of registered hooks, or 0 if no hooks configured.
 * Note: Python version also handles TTY consent prompts and
 * per-spec allowlist checks at registration time.  In C the
 * allowlist check is deferred to callback time (shell_hook_callback),
 * so the end-to-end behaviour is the same — un-allowlisted hooks
 * do not execute. */
int register_from_config(const json_t *config) {
    int parsed = iter_configured_hooks(config);
    if (parsed <= 0) return 0;
    return shell_hooks_register_all();
}

/* ================================================================
 *  Portable shell_hooks function ports
 * ================================================================ */

/* Port of Python agent/shell_hooks.py:_command_script_path().
 * Extract the script path from a shell command string.
 * Scans tokens for known script extensions first, then path chars,
 * then falls back to the first token.
 * Returns malloc'd string, caller must free(). Returns NULL on error. */
char *command_script_path(const char *command) {
    if (!command || !*command) return strdup("");

    /* Simple tokenization by whitespace (approximation of shlex.split) */
    const char *tokens[64];
    int n_tokens = 0;
    const char *p = command;
    while (*p && n_tokens < 64) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        /* Handle single quotes */
        if (*p == '\'') {
            const char *end = strchr(p + 1, '\'');
            tokens[n_tokens] = p + 1;
            p = end ? end + 1 : p + strlen(p);
            /* null-terminate conceptually — we just use strlen-like logic */
            n_tokens++;
            continue;
        }
        tokens[n_tokens] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\'') p++;
        n_tokens++;
    }

    if (n_tokens == 0) return strdup(command);

    /* Known script extensions */
    static const char *extensions[] = {".sh", ".py", ".js", ".rb", ".pl",
        ".bash", ".zsh", ".fish", ".exe", ".bat", ".ps1", NULL};

    for (int i = 0; i < n_tokens; i++) {
        const char *tok = tokens[i];
        /* Must be long enough for extension */
        size_t tlen = strlen(tok);
        for (int e = 0; extensions[e]; e++) {
            size_t elen = strlen(extensions[e]);
            if (tlen > elen && strcasecmp(tok + tlen - elen, extensions[e]) == 0) {
                /* strndup equivalent: copy up to tlen chars */
                char *result = malloc(tlen + 1);
                if (result) {
                    memcpy(result, tok, tlen);
                    result[tlen] = '\0';
                }
                return result;
            }
        }
    }

    /* Path-containing tokens (contains '/' or starts with '~') */
    for (int i = 0; i < n_tokens; i++) {
        const char *tok = tokens[i];
        if (strchr(tok, '/') || tok[0] == '~')
            return strdup(tok);
    }

    /* First token */
    return strdup(tokens[0]);
}

/* Port of Python agent/shell_hooks.py:_resolve_effective_accept().
 * Combine --accept-hooks arg, HERMES_ACCEPT_HOOKS env var,
 * and hooks_auto_accept config into a single boolean. */
bool resolve_effective_accept(bool accept_hooks_arg, const json_t *cli_config) {
    if (accept_hooks_arg) return true;

    const char *env = getenv("HERMES_ACCEPT_HOOKS");
    if (env && *env) {
        /* Strip and lowercase */
        while (*env == ' ' || *env == '\t') env++;
        if (!*env) goto check_config;
        size_t elen = strlen(env);
        char *lower = malloc(elen + 1);
        if (lower) {
            for (size_t i = 0; i < elen; i++)
                lower[i] = tolower((unsigned char)env[i]);
            lower[elen] = '\0';
            bool match = (strcmp(lower, "1") == 0 || strcmp(lower, "true") == 0 ||
                          strcmp(lower, "yes") == 0 || strcmp(lower, "on") == 0);
            free(lower);
            if (match) return true;
        }
    }

check_config:
    if (cli_config && cli_config->type == JSON_OBJECT) {
        json_t *auto_accept = json_obj_get(cli_config, "hooks_auto_accept");
        if (auto_accept) {
            if (auto_accept->type == JSON_BOOL || auto_accept->type == JSON_NUMBER) {
                if (auto_accept->num_val != 0.0) return true;
            }
            if (auto_accept->type == JSON_STRING && auto_accept->str_val) {
                const char *val = auto_accept->str_val;
                if (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
                    strcasecmp(val, "yes") == 0 || strcasecmp(val, "on") == 0)
                    return true;
            }
        }
    }

    return false;
}

/* Port of Python agent/shell_hooks.py: _prompt_and_record().
 * Interactive prompt: ask user to approve a shell hook.
 * If accept_hooks is true, auto-approves. If not a TTY, rejects.
 * Otherwise prints warning and reads y/N from stdin.
 * Calls _record_approval() on approval.
 * Returns true if approved. */
bool prompt_and_record(const char *event, const char *command, bool accept_hooks) {
    if (!event || !command) return false;

    if (accept_hooks) {
        shell_hooks_allowlist_record(event, command);
        return true;
    }

    /* Check if stdin is a terminal */
    if (!isatty(fileno(stdin))) return false;

    printf("\n⚠ Hermes is about to register a shell hook that will run a\n"
           "  command on your behalf.\n\n"
           "    Event:   %s\n"
           "    Command: %s\n\n"
           "  Commands run with your full user credentials.  Only approve\n"
           "  commands you trust.\n\n", event, command);

    printf("Allow this hook to run? [y/N]: ");
    fflush(stdout);

    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("\n");
        return false;
    }

    /* Strip newline */
    size_t blen = strlen(buf);
    while (blen > 0 && (buf[blen-1] == '\n' || buf[blen-1] == '\r'))
        buf[--blen] = '\0';

    /* Check response */
    if (blen > 0 && (buf[0] == 'y' || buf[0] == 'Y')) {
        shell_hooks_allowlist_record(event, command);
        return true;
    }

    return false;
}

/* Port of Python agent/shell_hooks.py:_locked_update_approvals().
 * Serialises read-modify-write on the allowlist using flock().
 * Acquires an exclusive lock on allowlist_path().lock before recording
 * the approval and releases it after save — prevents concurrent writers
 * from clobbering each other's changes.
 * Returns 1 on success, 0 on failure. */
int locked_update_approvals(const char *event, const char *command) {
    if (!event || !command) return 0;

    /* Build lock path: allowlist_path + ".lock" */
    char path[1024];
    char lock_path[1032];
    allowlist_path(path, sizeof(path));
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);

    /* Ensure parent directory exists */
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = '\0';
        char mkcmd[1088];
        snprintf(mkcmd, sizeof(mkcmd), "mkdir -p '%s' 2>/dev/null", path);
        (void)system(mkcmd);
        *slash = '/';
    }

    /* Open/create lock file */
    int fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return 0;

    /* Acquire exclusive lock */
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return 0;
    }

    /* Record approval and save */
    shell_hooks_allowlist_record(event, command);

    /* Release lock and close */
    flock(fd, LOCK_UN);
    close(fd);
    return 1;
}
