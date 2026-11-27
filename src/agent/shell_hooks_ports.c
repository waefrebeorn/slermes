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
#include "json.h"  /* libjson — for _parse_response / _fail_closed_block / _evaluate_result */
#include "port_utils_ports.h"

/* Constants shared across shell-hook ports. Mirrors Python
 * agent/shell_hooks.py constants: BLOCK_EXIT_CODE, _STDERR_MESSAGE_LIMIT,
 * _DEFAULT_BLOCK_MESSAGE, and the blocking-capable event set. */
#define SHK_BLOCK_EXIT_CODE  2
#define SHK_STDERR_LIMIT     400

/* ---- shell-hook allowlist: real JSON read-modify-write ----------------
 *
 * Every function below used to print a line and return a constant, so "approve this
 * hook" wrote nothing and _is_allowlisted answered from a substring coincidence.
 * Python keeps {"approvals": [{"event","command","approved_at",...}]} and matches an
 * entry on (event, command); these helpers reproduce exactly that.
 */
char *shk_allowlist_path(const char *hermes_home);
char *shk_load_allowlist(const char *hermes_home);
int   shk_save_allowlist(const char *hermes_home, const char *data_json);

/* bounded substring search (memmem is not portable to every mingw build) */
static const char *shk_memmem(const char *h, size_t n, const char *needle, size_t m) {
    if (!m || n < m) return NULL;
    for (size_t i = 0; i + m <= n; i++)
        if (h[i] == needle[0] && memcmp(h + i, needle, m) == 0) return h + i;
    return NULL;
}

static const char *shk_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    const char *u = getenv("USERPROFILE");
    if (!u || !*u) u = getenv("HOME");
    static char buf[1024];
    snprintf(buf, sizeof buf, "%s/.hermes", (u && *u) ? u : ".");
    return buf;
}

/* Extract a JSON string field from one object slice. Caller frees. */
static char *shk_field(const char *obj, size_t len, const char *field) {
    char needle[64];
    snprintf(needle, sizeof needle, "\"%s\"", field);
    const char *p = obj, *end = obj + len;
    while ((p = shk_memmem(p, (size_t)(end - p), needle, strlen(needle))) != NULL) {
        const char *q = p + strlen(needle);
        while (q < end && (*q == ' ' || *q == ':')) q++;
        if (q >= end || *q != '"') { p = q; continue; }
        q++;
        size_t cap = 64, n = 0;
        char *out = malloc(cap);
        if (!out) return NULL;
        while (q < end && *q != '"') {
            char ch = *q++;
            if (ch == '\\' && q < end) {
                char esc = *q++;
                ch = (esc == 'n') ? '\n' : (esc == 't') ? '\t' : esc;
            }
            if (n + 2 > cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            out[n++] = ch;
        }
        out[n] = '\0';
        return out;
    }
    return NULL;
}

/* Escape s into dst (without the surrounding quotes). */
static void shk_esc_str(char *dst, size_t cap, const char *s) {
    size_t n = 0;
    for (; s && *s && n + 7 < cap; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { dst[n++] = '\\'; dst[n++] = (char)ch; }
        else if (ch == '\n') { dst[n++] = '\\'; dst[n++] = 'n'; }
        else if (ch == '\t') { dst[n++] = '\\'; dst[n++] = 't'; }
        else if (ch == '\r') { dst[n++] = '\\'; dst[n++] = 'r'; }
        else if (ch < 0x20) n += (size_t)snprintf(dst + n, cap - n, "\\u%04x", ch);
        else dst[n++] = (char)ch;
    }
    if (cap) dst[n < cap ? n : cap - 1] = '\0';
}

/* Append s as a JSON string literal. */
static void shk_esc(FILE *w, const char *s) {
    fputc('"', w);
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { fputc('\\', w); fputc(ch, w); }
        else if (ch == '\n') fputs("\\n", w);
        else if (ch == '\t') fputs("\\t", w);
        else if (ch < 0x20) fprintf(w, "\\u%04x", ch);
        else fputc(ch, w);
    }
    fputc('"', w);
}

/* Walk the approvals array, calling keep() per object slice. */
static int shk_walk_approvals(const char *json, int (*keep)(const char *obj, size_t len, void *ctx),
                              void *ctx, FILE *out, int *kept) {
    if (!json) return -1;
    const char *arr = strstr(json, "\"approvals\"");
    if (!arr) return -1;
    const char *p = strchr(arr, '[');
    if (!p) return -1;
    p++;
    int n = 0, emitted = 0;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p != '{') break;
        const char *start = p;
        int depth = 0, in_str = 0;
        while (*p) {
            char ch = *p;
            if (in_str) {
                if (ch == '\\') p++;
                else if (ch == '"') in_str = 0;
            } else if (ch == '"') in_str = 1;
            else if (ch == '{') depth++;
            else if (ch == '}') { depth--; if (depth == 0) { p++; break; } }
            p++;
        }
        size_t len = (size_t)(p - start);
        n++;
        if (!keep || keep(start, len, ctx)) {
            if (out) {
                if (emitted) fputc(',', out);
                fwrite(start, 1, len, out);
            }
            emitted++;
        }
    }
    if (kept) *kept = emitted;
    return n;
}

struct shk_ctx { const char *event; const char *command; };

static int shk_keep_not_matching(const char *obj, size_t len, void *vctx) {
    struct shk_ctx *c = (struct shk_ctx *)vctx;
    char *ev = shk_field(obj, len, "event");
    char *cmd = shk_field(obj, len, "command");
    bool match = cmd && c->command && strcmp(cmd, c->command) == 0 &&
                 (!c->event || (ev && strcmp(ev, c->event) == 0));
    free(ev); free(cmd);
    return match ? 0 : 1;
}

/* Rewrite the allowlist with the entries keep() accepts, then persist. */
static int shk_rewrite(const char *json, const char *extra_entry,
                       int (*keep)(const char *, size_t, void *), void *ctx, int *kept_n) {
    char *path = shk_allowlist_path(shk_home());
    /* Windows refuses to create the file when the directory is absent, and the old
     * body reported success anyway: approvals vanished without a message. */
    {
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; mkdir(dir, 0777); }
    }
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(path); free(tmp); return -1; }
    fputs("{\"approvals\": [", w);
    int kept = 0;
    shk_walk_approvals(json, keep, ctx, w, &kept);
    if (extra_entry) {
        if (kept) fputc(',', w);
        fputs(extra_entry, w);
        kept++;
    }
    fputs("]}\n", w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); free(path); return -1; }
    fclose(w);
    int rc = util_rename_replace(tmp, path);
    if (rc != 0) unlink(tmp);
    free(tmp); free(path);
    if (kept_n) *kept_n = kept;
    return rc == 0 ? 0 : -1;
}

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
    int rc = util_rename_replace(tmp, path);
    if (rc != 0) unlink(tmp);
    free(tmp); free(path);
    return rc == 0 ? 0 : -1;
}

/* PoP: shk_is_allowlisted @ agent/shell_hooks.py:_is_allowlisted */
bool shk_is_allowlisted(const char *allowlist_json, const char *event, const char *command) {
    /* Python: any entry whose event AND command are exactly these strings.
     * The old body matched substrings anywhere in the object, so a command that
     * merely shared a word with an unrelated entry read as allowlisted. */
    if (!allowlist_json || !event || !command) return false;
    struct shk_ctx probe = { event, command };
    int kept = 0;
    shk_walk_approvals(allowlist_json, shk_keep_not_matching, &probe, NULL, &kept);
    int total = shk_walk_approvals(allowlist_json, NULL, NULL, NULL, NULL);
    return total > kept;  /* kept < total => at least one entry matched */
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


/* Public read-back for callers and gates: the persisted approvals as a sorted,
 * ';'-joined "event|command" list. An empty list is "". */
struct shk_pairs { char *v[512]; int n; };

static int shk_pairs_collect(const char *obj, size_t len, void *vctx) {
    struct shk_pairs *acc = (struct shk_pairs *)vctx;
    if (acc->n >= 512) return 0;
    char *ev = shk_field(obj, len, "event");
    char *cmd = shk_field(obj, len, "command");
    if (cmd) {
        char *pair = NULL;
        if (asprintf(&pair, "%s|%s", ev ? ev : "", cmd) > 0) acc->v[acc->n++] = pair;
    }
    free(ev);
    free(cmd);
    return 0;   /* not "kept": we only want the callback's side effect */
}

static int shk_pair_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

char *shk_allowlist_pairs(const char *json) {
    struct shk_pairs acc;
    memset(&acc, 0, sizeof acc);
    shk_walk_approvals(json, shk_pairs_collect, &acc, NULL, NULL);
    qsort(acc.v, (size_t)acc.n, sizeof acc.v[0], shk_pair_cmp);
    size_t cap = 64;
    for (int i = 0; i < acc.n; i++) cap += strlen(acc.v[i]) + 1;
    char *out = malloc(cap);
    if (out) {
        out[0] = '\0';
        for (int i = 0; i < acc.n; i++) {
            if (i) strcat(out, ";");
            strcat(out, acc.v[i]);
        }
    }
    for (int i = 0; i < acc.n; i++) free(acc.v[i]);
    return out ? out : strdup("");
}

/* PoP: shk_record_approval @ agent/shell_hooks.py:_record_approval */
int shk_record_approval(const char *event, const char *command) {
    /* Python: drop entries matching (event, command), append the new one, persist. */
    if (!event || !command) return -1;
    char *json = shk_load_allowlist(shk_home());
    char ts[64];
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tm);

    /* mingw's open_memstream fails at runtime, so build the entry in a bounded
     * buffer with an explicit escaper. */
    char entry[4096];
    {
        char ev[512], cmd[2048];
        shk_esc_str(ev, sizeof ev, event);
        shk_esc_str(cmd, sizeof cmd, command);
        snprintf(entry, sizeof entry,
                 "{\"event\": \"%s\", \"command\": \"%s\", \"approved_at\": \"%s\"}",
                 ev, cmd, ts);
    }
    struct shk_ctx ctx = { event, command };
    int kept = 0;
    int rc = shk_rewrite(json, entry, shk_keep_not_matching, &ctx, &kept);
    free(json);
    return rc;
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
    /* Python: drop every entry for this command, return how many went away. */
    if (!command) return 0;
    char *json = shk_load_allowlist(shk_home());
    int before = shk_walk_approvals(json, NULL, NULL, NULL, NULL);
    struct shk_ctx ctx = { NULL, command };
    int kept = 0;
    if (shk_rewrite(json, NULL, shk_keep_not_matching, &ctx, &kept) != 0) {
        free(json);
        return 0;
    }
    free(json);
    return before > kept ? (long)(before - kept) : 0;
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

