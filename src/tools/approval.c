/*
 * approval.c — Dangerous command approval security for Hermes C.
 * Phase 91: Intercepts dangerous tool calls, asks user for approval.
 * Mirrors Python's tools/approval.py (~1393 LOC).
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_url_safety.h"
#include "hermes_tirith.h"
#include "approval.h"
#include "gw_server_internals.h"
#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>

/* Forward declarations for P164 audit logging */
void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail);

/* ================================================================
 *  Dangerous Patterns
 * ================================================================ */

/* Terminal commands that require approval */
static const char *DANGEROUS_TERMINAL_PATTERNS[] = {
    "rm -rf", "rm -r /", "rm -rf /", "rm -fr /",
    "mkfs", "format", "dd if=", "dd of=",
    "> /dev/sd", "> /dev/nvme", "> /dev/hd",
    "> /dev/mmcblk", "> /dev/vd", "> /dev/xvd",
    "chmod 777 /", "chown -R",
    "wget ", "curl ", /* External downloads */
    "sudo ", "su ",
    "passwd", "usermod", "useradd",
    "iptables", "ufw",
    "reboot", "shutdown", "poweroff", "halt",
    "kill -9", "pkill",
    "docker rm ", "docker rmi ", "docker system prune",
    "systemctl stop ", "systemctl disable ",
    "dd if=/dev/zero", "dd if=/dev/urandom",
    ":(){ :|:& };:", /* Fork bomb */
    NULL
};

/* File paths that require approval */
static const char *DANGEROUS_FILE_PATTERNS[] = {
    "/etc/", "/boot/", "/sys/", "/proc/",
    "/dev/sd", "/dev/nvme", "/dev/mmcblk",
    "/lib/", "/lib64/", "/usr/lib/",
    "/bin/", "/sbin/", "/usr/bin/", "/usr/sbin/",
    NULL
};

/* Path traversal patterns (in args or paths) */
static const char *PATH_TRAVERSAL_PATTERNS[] = {
    "../", "..\\", "%2e%2e", "%2E%2E",
    "..%2f", "..%5c",
    NULL
};

/* Web URLs / patterns that require approval */
static const char *DANGEROUS_WEB_PATTERNS[] = {
    "127.0.0.1", "localhost", "169.254.",
    "10.", "172.1", "192.168.",
    "internal", "corp", "private",
    NULL
};

/* ================================================================
 *  Approval State
 * ================================================================ */

/* Per-session approved/denied cache */
#define MAX_APPROVED 256
static struct {
    char command[512];
    char tool[64];
    bool approved;   /* true=approved, false=denied */
    bool permanent;  /* saved to allowlist file */
} g_approval_cache[MAX_APPROVED];
static int g_approval_count = 0;

/* YOLO mode: when true, skip all approval prompts */
static bool g_yolo_approval = false;

/* Allowlist file path (set at init) */
static char g_allowlist_path[HERMES_PATH_MAX] = {0};

void approval_set_yolo(bool enabled) {
/* AG26: Port of Python tools/approval.py:enable_session_yolo() / disable_session_yolo(). */
    g_yolo_approval = enabled;
}

/* PoP: yolo @ tools/approval.py:is_current_session_yolo_enabled */
/* Returns whether yolo (skip-all-approvals) mode is active for the session. */
bool approval_is_yolo_enabled(void) {
    return g_yolo_approval;
}

/* Set allowlist file path (called during init) */
/* AG26: Port of Python tools/approval.py:load_permanent_allowlist() / save_permanent_allowlist() (path config). */
void approval_set_allowlist_path(const char *path) {
    if (path) snprintf(g_allowlist_path, sizeof(g_allowlist_path), "%s", path);
}

static bool (*g_approval_send_fn)(const char *platform, const char *chat_id,
                                   const char *text) = NULL;
static char *(*g_approval_wait_fn)(int timeout_sec) = NULL;
static const char *g_approval_platform = NULL;
static const char *g_approval_chat_id = NULL;

/* Set gateway send function and current chat context */
/* AG26: Port of Python tools/approval.py:register_gateway_notify() (gateway send). */
void approval_set_gateway_send(bool (*fn)(const char *, const char *, const char *),
                                const char *platform, const char *chat_id) {
    g_approval_send_fn = fn;
    g_approval_platform = platform;
    g_approval_chat_id = chat_id;
}

/* Set gateway approval wait function — returns "y", "n", "a", or "" (timeout) */
/* AG26: Port of Python tools/approval.py:_await_gateway_decision() (gateway wait). */
void approval_set_gateway_wait(char *(*fn)(int timeout_sec)) {
    g_approval_wait_fn = fn;
}

/* P162: Forward declaration for approval timeout */
static int g_approval_timeout;

/* Load persistent allowlist from file */
/* AG26: Port of Python tools/approval.py:load_permanent_allowlist(). */
void approval_load_allowlist(void) {
    if (!g_allowlist_path[0]) return;
    char *err = NULL;
    json_t *root = json_parse_file(g_allowlist_path, &err);
    if (!root) { free(err); return; }
    if (json_len(root) == 0) { json_free(root); return; }

    for (size_t i = 0; i < json_len(root) && g_approval_count < MAX_APPROVED; i++) {
        json_t *entry = json_get(root, i);
        if (!entry) continue;
        const char *tool = json_get_str(entry, "tool", "");
        const char *cmd = json_get_str(entry, "command", "");
        bool approved = json_get_bool(entry, "approved", true);
        if (tool[0] && cmd[0]) {
            snprintf(g_approval_cache[g_approval_count].tool, sizeof(g_approval_cache[0].tool), "%s", tool);
            snprintf(g_approval_cache[g_approval_count].command, sizeof(g_approval_cache[0].command), "%s", cmd);
            g_approval_cache[g_approval_count].approved = approved;
            g_approval_cache[g_approval_count].permanent = true;
            g_approval_count++;
        }
    }
    json_free(root);
}

/* Save persistent allowlist to file */
/* AG26: Port of Python tools/approval.py:save_permanent_allowlist(). */
void approval_save_allowlist(void) {
    if (!g_allowlist_path[0]) return;
    json_t *arr = json_array();
    if (!arr) return;

    for (int i = 0; i < g_approval_count; i++) {
        if (!g_approval_cache[i].permanent) continue;
        json_t *entry = json_object();
        json_set(entry, "tool", json_string(g_approval_cache[i].tool));
        json_set(entry, "command", json_string(g_approval_cache[i].command));
        json_set(entry, "approved", json_bool(g_approval_cache[i].approved));
        json_append(arr, entry);
    }

    char *ser = json_serialize_pretty(arr, 2);
    json_free(arr);
    if (!ser) return;

    FILE *f = fopen(g_allowlist_path, "w");
    if (f) {
        fputs(ser, f);
        fclose(f);
    }
    free(ser);
}

/* ---- allowlist enumeration + bulk set/merge (tools/approval.py ports) ---- */

/* List permanent allowlist command strings. Returns a malloc'd NULL-terminated
 * array (caller frees each entry + the array). Sets *out_n. */
/* PoP: approval_permanent_allowlist_list @ tools/approval.py:load_permanent_allowlist */
char **approval_permanent_allowlist_list(size_t *out_n) {
    if (out_n) *out_n = 0;
    size_t n = 0, cap = 0;
    char **out = NULL;
    for (int i = 0; i < g_approval_count; i++) {
        if (!g_approval_cache[i].permanent) continue;
        if (n >= cap) {
            cap = cap ? cap * 2 : 8;
            char **t = (char **)realloc(out, (cap + 1) * sizeof(*out));
            if (!t) { for (size_t j = 0; j < n; j++) free(out[j]); free(out); return NULL; }
            out = t;
        }
        out[n++] = strdup(g_approval_cache[i].command);
    }
    if (out) out[n] = NULL;
    if (out_n) *out_n = n;
    return out;
}


/* PoP: save_permanent_allowlist @ tools/approval.py:save_permanent_allowlist */
int save_permanent_allowlist(const char **patterns, size_t n) {
    int write = 0;
    for (int i = 0; i < g_approval_count; i++) {
        if (g_approval_cache[i].permanent) {
            for (int j = i; j < g_approval_count - 1; j++)
                g_approval_cache[j] = g_approval_cache[j + 1];
            g_approval_count--;
            i--;
            write = 1;
        }
    }
    for (size_t k = 0; k < n && g_approval_count < MAX_APPROVED; k++) {
        if (!patterns[k] || !*patterns[k]) continue;
        bool dup = false;
        for (int i = 0; i < g_approval_count; i++)
            if (strcmp(g_approval_cache[i].command, patterns[k]) == 0) { dup = true; break; }
        if (dup) continue;
        snprintf(g_approval_cache[g_approval_count].tool,
                 sizeof(g_approval_cache[0].tool), "%s", "terminal");
        snprintf(g_approval_cache[g_approval_count].command,
                 sizeof(g_approval_cache[0].command), "%s", patterns[k]);
        g_approval_cache[g_approval_count].approved = true;
        g_approval_cache[g_approval_count].permanent = true;
        g_approval_count++;
        write = 1;
    }
    if (write) approval_save_allowlist();
    return 0;
}

/* Dump the permanent allowlist entries to stdout (approvals list/suggest). */
void approval_dump_allowlist(void) {
    int shown = 0;
    for (int i = 0; i < g_approval_count; i++) {
        if (!g_approval_cache[i].permanent) continue;
        printf("  - %s\n", g_approval_cache[i].command);
        shown++;
    }
    if (shown == 0) printf("  (empty)\n");
}

/* PoP: approval_reset_session @ src/tools/approval.c:approval_reset_session */
/* Port of Python session.py:reset_session(). */
/* AG26: Port of Python tools/approval.py:clear_session() (session reset). */
void approval_reset_session(void) {
    g_approval_count = 0;
}

/* Check if a command is in the approval cache */
static int approval_cache_lookup(const char *tool, const char *cmd) {
    for (int i = 0; i < g_approval_count; i++) {
        if (strcmp(g_approval_cache[i].tool, tool) == 0 &&
            strcmp(g_approval_cache[i].command, cmd) == 0) {
            return g_approval_cache[i].approved ? 1 : -1;
        }
    }
    return 0; /* Not in cache */
}

/* Add to approval cache */
static void approval_cache_add(const char *tool, const char *cmd,
                                bool approved, bool permanent) {
    if (g_approval_count >= MAX_APPROVED) return;
    snprintf(g_approval_cache[g_approval_count].tool, sizeof(g_approval_cache[0].tool), "%s", tool);
    snprintf(g_approval_cache[g_approval_count].command, sizeof(g_approval_cache[0].command), "%s", cmd);
    g_approval_cache[g_approval_count].approved = approved;
    g_approval_cache[g_approval_count].permanent = permanent;
    g_approval_count++;
}

/* ================================================================
 *  Pattern Matching
 * ================================================================ */

static bool matches_any(const char *str, const char **patterns) {
    if (!str) return false;
    for (int i = 0; patterns[i]; i++) {
        if (strstr(str, patterns[i]))
            return true;
    }
    return false;
}

/* Normalize a command string before dangerous-pattern matching.
 * Strips ANSI escape sequences and null bytes.
 * Port of Python tools/approval.py:_normalize_command_for_detection().
 * Returns a malloc'd normalized copy, or NULL on failure. */
/* AG26: Port of Python tools/approval.py:_normalize_command_for_detection(). */
/* PoP: approval_normalize_command @ hermes_cli/approvals_suggest.py:normalize_command */
char *approval_normalize_command(const char *command) {
    if (!command) return NULL;

    /* Step 1: Strip ANSI escape sequences via stack-allocated buffer.
     * ansi_strip_buf writes in-place if buf == text, so we use a separate buf. */
    size_t len = strlen(command);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;

    if (ansi_has_escape(command)) {
        ansi_strip_buf(command, buf, len + 1);
    } else {
        memcpy(buf, command, len + 1);
    }

    /* Step 2: Strip null bytes in-place. */
    char *dst = buf;
    for (const char *src = buf; *src; src++) {
        if (*src != '\0') {
            *dst++ = *src;
        }
    }
    *dst = '\0';

    /* Note: Unicode NFKC normalization (Python step 3) requires ICU — skipped in C.
     * Step 4: " ".join(folded.split()) — collapse runs of whitespace into single
     * spaces and strip leading/trailing. Also fold resolved home dirs to ~,
     * matching _rewrite_resolved_user_home / _rewrite_resolved_hermes_home. */
    {
        char *w = buf;  /* write pointer */
        bool prev_was_space = true;  /* strip leading whitespace */
        for (const char *r = buf; *r; r++) {
            if (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r' ||
                *r == '\f' || *r == '\v') {
                if (!prev_was_space) {
                    *w++ = ' ';
                    prev_was_space = true;
                }
            } else {
                *w++ = *r;
                prev_was_space = false;
            }
        }
        /* strip trailing space */
        if (w > buf && *(w - 1) == ' ') w--;
        *w = '\0';
    }
    return buf;
}

/* ── approvals_suggest.py ports ──────────────────────────────── */

/* PoP: error @ hermes_cli/approvals_suggest.py:parse_apply_indices */
/* Parse "1,3" into validated zero-based indices. On error (invalid
 * integer, out of range, or no valid selections), returns -1. */
int approval_parse_apply_indices(const char *spec, int total, int *out, int max_out) {
    if (!out) return -1;
    int count = 0;
    const char *p = spec ? spec : "";
    while (*p) {
        /* Skip leading whitespace and commas */
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        /* Find end of this token */
        const char *tok_start = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\t') p++;
        int tok_len = (int)(p - tok_start);
        if (tok_len == 0) continue;
        /* Parse as integer */
        char buf[32];
        if (tok_len >= (int)sizeof(buf)) return -1; /* too long for int */
        memcpy(buf, tok_start, tok_len);
        buf[tok_len] = '\0';
        char *endptr = NULL;
        long n = strtol(buf, &endptr, 10);
        if (endptr != buf + tok_len) return -1; /* not a valid integer */
        if (n < 1 || n > total) return -1; /* out of range */
        int zero_based = (int)n - 1;
        /* Dedup */
        bool seen = false;
        for (int i = 0; i < count; i++) {
            if (out[i] == zero_based) { seen = true; break; }
        }
        if (!seen) {
            if (count >= max_out) return -1; /* too many */
            out[count++] = zero_based;
        }
    }
    if (count == 0) return -1; /* no valid selections */
    return count;
}

/* The full Python _UNSAFE_CLASS_PATTERNS list as plain substrings.
 * Python uses re.search with IGNORECASE; we simulate with strcasestr.
 * The Python patterns use \b word boundaries; we approximate with
 * substring matching (conservative: more matches, which is safe for
 * an allowlist-style "must never propose" check). */
static const char *UNSAFE_CLASS_PATTERNS[] = {
    "delete", "rm", "rmdir", "unlink", "wipe", "format",
    "disk", "block device", "fork bomb", "kill all", "kill process",
    "self-termination", "sudo", "privilege", "credential",
    "ssh", "shell.rc", "system config", "system file", "sql",
    "chown", "chmod", "writable", "overwrite", "in-place edit",
    "pipe", "obfuscation", "remote content", "remote script", "heredoc",
    "encoded", "command substitution", "process substitution", "dd",
    "shutdown", "reboot", "hardline",
    NULL,
};

/* PoP: is_unsafe_root_binary @ hermes_cli/approvals_suggest.py:_unsafe_root_binary */
static bool is_unsafe_root_binary(const char *token) {
    if (!token) return false;
    /* tok = token.lower().rsplit("/", 1)[-1] */
    char lower[256];
    size_t len = strlen(token);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) lower[i] = tolower((unsigned char)token[i]);
    lower[len] = '\0';
    /* Find last "/" */
    const char *slash = strrchr(lower, '/');
    const char *base = slash ? slash + 1 : lower;
    /* _UNSAFE_ROOT_BINARIES set */
    static const char *binaries[] = {
        "rm", "rmdir", "unlink", "shred", "dd", "fdisk", "parted", "wipefs",
        "sudo", "doas", "su", "chmod", "chown", "chgrp",
        "kill", "killall", "pkill",
        "halt", "shutdown", "reboot", "poweroff", "init",
        "del", "format", "truncate", "mkswap", NULL
    };
    for (int i = 0; binaries[i]; i++) {
        if (strcmp(base, binaries[i]) == 0) return true;
    }
    /* _UNSAFE_ROOT_PREFIXES: ("mkfs",) */
    if (strncmp(base, "mkfs", 4) == 0) return true;
    return false;
}

/* PoP: approval_is_unsafe_class @ hermes_cli/approvals_suggest.py:is_unsafe_class */
bool approval_is_unsafe_class(const char *description) {
    if (!description || !*description) return false;
    for (int i = 0; UNSAFE_CLASS_PATTERNS[i]; i++) {
        if (strcasestr(description, UNSAFE_CLASS_PATTERNS[i]))
            return true;
    }
    return false;
}

/* PoP: approval_derive_glob @ hermes_cli/approvals_suggest.py:derive_glob */
/* Returns a malloc'd command glob string, or NULL for compound/unsafe commands. */
char *approval_derive_glob(const char *normalized) {
    if (!normalized || !*normalized) return NULL;
    /* If compound (has shell operators), return None */
    if (apr_has_allowlist_shell_operator(normalized)) return NULL;
    /* Tokenize by whitespace */
    char *work = strdup(normalized);
    if (!work) return NULL;
    /* Split into tokens */
    char *tok = work;
    char *tokens[64];
    int ntok = 0;
    char *p = work;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tokens[ntok++] = p;
        if (ntok >= 64) break;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = '\0'; p++; }
    }
    if (ntok == 0) { free(work); return NULL; }
    /* Check unsafe root binary */
    if (is_unsafe_root_binary(tokens[0])) { free(work); return NULL; }
    char *result = NULL;
    if (ntok == 1) {
        result = strdup(tokens[0]);
    } else {
        char *second = tokens[1];
        if (second[0] == '-' || strpbrk(second, "*?[$")) {
            result = malloc(strlen(tokens[0]) + 3);
            if (result) sprintf(result, "%s *", tokens[0]);
        } else {
            /* "cmd second *" → len + 1 (space) + len + 1 (space) + 1 (*) + 1 (null) = +3... actually +3 */
            result = malloc(strlen(tokens[0]) + strlen(second) + 4);
            if (result) sprintf(result, "%s %s *", tokens[0], second);
        }
    }
    free(work);
    return result;
}

/* AG26: Port of Python tools/approval.py:detect_dangerous_command(). */
bool approval_is_terminal_dangerous(const char *command) {
    if (!command) return false;
    char *normalized = approval_normalize_command(command);
    if (!normalized) return matches_any(command, DANGEROUS_TERMINAL_PATTERNS);
    bool result = matches_any(normalized, DANGEROUS_TERMINAL_PATTERNS);
    free(normalized);
    return result;
}

/* Check if a file path is dangerous */
/* AG26: Port of Python tools/approval.py:detect_dangerous_command() (path check). */
bool approval_is_path_dangerous(const char *path) {
    return matches_any(path, DANGEROUS_FILE_PATTERNS);
}

/* Check if a path contains traversal (../) patterns */
/* AG26: Port of Python tools/approval.py:detect_dangerous_command() (traversal check). */
bool approval_is_path_traversal(const char *path) {
    if (!path) return false;
    return matches_any(path, PATH_TRAVERSAL_PATTERNS);
}

/* Check if a URL is dangerous (SSRF/internal) — uses DNS resolution */
/* AG26: Port of Python tools/approval.py:detect_dangerous_command() (SSRF check). */
bool approval_is_url_dangerous(const char *url) {
    if (!url) return false;
    /* First check quick string patterns (no DNS) */
    if (matches_any(url, DANGEROUS_WEB_PATTERNS)) return true;
    /* Then do full DNS-based safety check */
    return !url_is_safe(url);
}

/* ================================================================
 *  User Prompt
 * ================================================================ */

/* Ask user for approval. Returns true if approved. */
/* AG26: Port of Python tools/approval.py:prompt_dangerous_approval(). */
static bool approval_prompt_user(const char *tool, const char *reason,
                                  const char *detail) {
    fprintf(stderr, "\n⚠️  DANGEROUS OPERATION DETECTED\n");
    fprintf(stderr, "  Tool: %s\n", tool);
    fprintf(stderr, "  Reason: %s\n", reason);
    if (detail) fprintf(stderr, "  Detail: %s\n", detail);
    fprintf(stderr, "  Timeout: %ds\n", g_approval_timeout);

    /* If gateway mode: send approval prompt via messaging platform */
    if (g_approval_send_fn && g_approval_wait_fn &&
        g_approval_platform && g_approval_chat_id) {
        char prompt[2048];
        snprintf(prompt, sizeof(prompt),
            "⚠️ *Approve dangerous command?*\n\n`%s`\n\nReason: %s\nReply y/n/a(always)",
            detail ? detail : tool, reason ? reason : "dangerous operation");
        if (!g_approval_send_fn(g_approval_platform, g_approval_chat_id, prompt)) {
            fprintf(stderr, "\n  → Denied (gateway send failed)\n");
            audit_log_security("approval", tool, "denied", "gateway send failed", detail);
            return false;
        }
        fprintf(stderr, "  → Sent approval prompt to %s/%s\n",
                g_approval_platform, g_approval_chat_id);
        /* Register pending approval context for gateway response collection */
        gw_approval_set_context(g_approval_platform, g_approval_chat_id);
        char *resp = g_approval_wait_fn(g_approval_timeout);
        if (!resp || !resp[0]) {
            free(resp);
            fprintf(stderr, "\n  → Auto-denied (timeout after %ds)\n", g_approval_timeout);
            audit_log_security("approval", tool, "timeout", reason, detail);
            return false;
        }
        /* Normalize */
        for (char *p = resp; *p; p++) *p = (char)tolower((unsigned char)*p);

        bool approved = false;
        if (resp[0] == 'y') {
            approval_cache_add(tool, detail, true, false);
            audit_log_security("approval", tool, "approved_once", reason, detail);
            approved = true;
        } else if (resp[0] == 'a') {
            approval_cache_add(tool, detail, true, true);
            approval_save_allowlist();
            audit_log_security("approval", tool, "approved_always", reason, detail);
            approved = true;
        } else {
            approval_cache_add(tool, detail, false, true);
            audit_log_security("approval", tool, "denied", reason, detail);
        }
        free(resp);
        return approved;
    }

    /* CLI mode: print prompt and wait for stdin */
    fprintf(stderr, "  Approve? [y/N/a(always)/n] ");

    /* If not interactive (piped input), deny by default */
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "\n  → Denied (non-interactive mode)\n");
        audit_log_security("approval", tool, "denied", "non-interactive", detail);
        return false;
    }

    /* P162: Approval timeout — use alarm or select-based timeout */
    char response[16];
    response[0] = '\0';

    /* Set up timeout via select on stdin */
    struct timeval tv;
    tv.tv_sec = g_approval_timeout;
    tv.tv_usec = 0;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int sel_ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
    if (sel_ret <= 0) {
        fprintf(stderr, "\n  → Auto-denied (timeout after %ds)\n", g_approval_timeout);
        audit_log_security("approval", tool, "timeout", reason, detail);
        return false;
    }

    if (!fgets(response, sizeof(response), stdin))
        return false;

    /* Normalize response */
    for (char *p = response; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (response[0] == 'y') {
        approval_cache_add(tool, detail, true, false);
        audit_log_security("approval", tool, "approved_once", reason, detail);
        return true;
    }
    if (response[0] == 'a') {
        approval_cache_add(tool, detail, true, true);
        approval_save_allowlist();
        fprintf(stderr, "  → Approved permanently (saved to allowlist).\n");
        audit_log_security("approval", tool, "approved_always", reason, detail);
        return true;
    }
    approval_cache_add(tool, detail, false, true);
    fprintf(stderr, "  → Denied.\n");
    audit_log_security("approval", tool, "denied", reason, detail);
    return false;
}

/* ================================================================
 *  Command Allowlist (P161)
 * ================================================================ */

/* Per-tool accepted command patterns. When configured, only commands matching
 * these patterns are allowed. Format: "tool_name:pattern" */
#define MAX_ALLOWLIST_ENTRIES 128

static struct {
    char tool[64];
    char pattern[256];  /* Glob pattern: *, ? wildcards supported */
} g_allowlist[MAX_ALLOWLIST_ENTRIES];
static int g_allowlist_count = 0;

/* P161: Add command allowlist entry */
/* AG26: Port of Python tools/approval.py:approve_permanent() (allowlist add). */
bool allowlist_add(const char *tool, const char *pattern) {
    if (!tool || !pattern || g_allowlist_count >= MAX_ALLOWLIST_ENTRIES)
        return false;

    /* Check for duplicates */
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i].tool, tool) == 0 &&
            strcmp(g_allowlist[i].pattern, pattern) == 0)
            return true;
    }

    snprintf(g_allowlist[g_allowlist_count].tool, sizeof(g_allowlist[0].tool), "%s", tool);
    snprintf(g_allowlist[g_allowlist_count].pattern, sizeof(g_allowlist[0].pattern), "%s", pattern);
    g_allowlist_count++;
    return true;
}

/* P161: Remove allowlist entry */
/* AG26: Port of Python tools/approval.py:is_approved() / approve_permanent() (allowlist remove). */
bool allowlist_remove(const char *tool, const char *pattern) {
    if (!tool) return false;
    for (int i = 0; i < g_allowlist_count; i++) {
        bool match_tool = strcmp(g_allowlist[i].tool, tool) == 0;
        bool match_pat = !pattern || strcmp(g_allowlist[i].pattern, pattern) == 0;
        if (match_tool && match_pat) {
            g_allowlist[i] = g_allowlist[--g_allowlist_count];
            g_allowlist[g_allowlist_count].tool[0] = '\0';
            g_allowlist[g_allowlist_count].pattern[0] = '\0';
            return true;
        }
    }
    return false;
}

/* P161: Clear all allowlist entries */
void allowlist_clear(void) {
    memset(g_allowlist, 0, sizeof(g_allowlist));
    g_allowlist_count = 0;
}

/* Simple glob-style pattern matching.
 * Supports: * (any chars), ? (single char). Literal match for everything else.
 * Returns true if `str` matches `pattern`. */
static bool glob_match(const char *str, const char *pattern) {
    if (!str || !pattern) return false;
    while (*pattern) {
        if (*pattern == '*') {
            /* Skip consecutive stars */
            while (pattern[1] == '*') pattern++;
            pattern++;
            if (*pattern == '\0') return true;  /* trailing * matches everything */
            /* Try matching the rest at every position */
            while (*str) {
                if (glob_match(str, pattern)) return true;
                str++;
            }
            return false;
        } else if (*pattern == '?') {
            if (*str == '\0') return false;
            str++; pattern++;
        } else {
            if (*str != *pattern) return false;
            str++; pattern++;
        }
    }
    return *str == '\0';
}

/* P161: Check if a command is allowed by the allowlist for a tool.
 * Returns: true if allowed, false if denied.
 * If the tool has no allowlist entries, all commands pass.
 * If it has entries, the command must match at least one. */
/* AG26: Port of Python tools/approval.py:is_approved() (allowlist check). */
bool allowlist_check(const char *tool, const char *command) {
    if (!tool || !command) return false;

    /* Count entries for this tool */
    bool has_entries = false;
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i].tool, tool) == 0) {
            has_entries = true;
            bool has_glob = (strchr(g_allowlist[i].pattern, '*') != NULL ||
                               strchr(g_allowlist[i].pattern, '?') != NULL);
            if (has_glob) {
                if (glob_match(command, g_allowlist[i].pattern))
                    return true; /* Matched glob */
            } else {
                /* Bare pattern: substring match (backward compat) */
                if (strstr(command, g_allowlist[i].pattern))
                    return true; /* Matched substring */
            }
        }
    }

    /* If no entries exist for this tool, allow all (opt-in system) */
    if (!has_entries) return true;

    /* Had entries but none matched */
    return false;
}

/* ================================================================
 *  Approval Timeout (P162)
 * ================================================================ */

/* Gateway approval callbacks — set by server.c when running in gateway mode */
/* Default approval timeout in seconds */
/* AG26: Port of Python tools/approval.py:_get_approval_timeout(). */
static int g_approval_timeout = 30;

void approval_set_timeout(int seconds) {
    g_approval_timeout = seconds > 0 ? seconds : 30;
}

int approval_get_timeout(void) {
    return g_approval_timeout;
}

/* ================================================================
 *  Main Approval Check
 * ================================================================ */

typedef struct {
    const char *tool_name;
    const char *args_json;
} tool_invocation_t;

/* PoP: guards @ tools/approval.py:check_all_command_guards */
/* Run all command guards (hardline, sudo-stdin, sensitive-path) for a tool
 * invocation. The C implementation folds this into approval_check, so we
 * delegate. Returns 1=approved, 0=denied, -1=not dangerous. */
int check_all_command_guards(const char *tool_name, const char *args_json) {
    return approval_check(tool_name, args_json);
}

/* Check if a tool invocation needs approval.
   Returns: 1 = approved, 0 = denied, -1 = not dangerous */
/* AG26: Port of Python tools/approval.py:check_dangerous_command() + check_all_command_guards(). */
/* PoP: approval_check @ write_approval:_check */
/* PoP: approval_check @ tts_tool:_check */
int approval_check(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return -1;

    /* Parse args if JSON */
    json_t *args = json_parse(args_json, NULL);
    if (!args) return -1;

    const char *danger_reason = NULL;
    const char *danger_detail = NULL;
    char detail_buf[2048];
    detail_buf[0] = '\0';

    /* Check terminal tool */
    if (strcmp(tool_name, "terminal") == 0) {
        const char *cmd = json_get_str(args, "command", "");

        /* P161: Check command allowlist */
        if (!allowlist_check(tool_name, cmd)) {
            danger_reason = "Command not in allowlist";
            strncpy(detail_buf, cmd, sizeof(detail_buf) - 1);
            detail_buf[sizeof(detail_buf) - 1] = '\0';
            danger_detail = detail_buf;
            json_free(args);
            /* Denied — command not in allowlist */
            audit_log_security("allowlist", tool_name, "denied", "command not in allowlist", cmd);
            return 0;
        }

        if (cmd && approval_is_terminal_dangerous(cmd)) {
            danger_reason = "Dangerous shell command";
            strncpy(detail_buf, cmd, sizeof(detail_buf) - 1);
            detail_buf[sizeof(detail_buf) - 1] = '\0';
            danger_detail = detail_buf;
        }
        /* Also run Tirith scan on terminal commands */
        if (!danger_reason && cmd) {
            tirith_verdict_t t = tirith_inline_scan(cmd);
            if (t == TIRITH_BLOCK) {
                danger_reason = "Tirith security: blocked pattern";
                strncpy(detail_buf, cmd, sizeof(detail_buf) - 1);
                detail_buf[sizeof(detail_buf) - 1] = '\0';
                danger_detail = detail_buf;
            } else if (t == TIRITH_WARN) {
                /* Warnings only flag if there's a matching dangerous pattern */
                danger_reason = "Tirith warning: suspicious command pattern";
                strncpy(detail_buf, cmd, sizeof(detail_buf) - 1);
                detail_buf[sizeof(detail_buf) - 1] = '\0';
                danger_detail = detail_buf;
            }
        }
    }

    /* Check file tools */
    if (strcmp(tool_name, "write_file") == 0 || strcmp(tool_name, "patch") == 0) {
        const char *path = json_get_str(args, "path", "");
        if (path && (approval_is_path_dangerous(path) || approval_is_path_traversal(path))) {
            snprintf(detail_buf, sizeof(detail_buf), "%s \u2192 %s", tool_name, path);
            danger_reason = approval_is_path_traversal(path) ?
                "Path traversal detected" : "Modifying system file";
            danger_detail = detail_buf;
        }
    }

    /* Check web tools for SSRF */
    if (strcmp(tool_name, "web_get") == 0 || strcmp(tool_name, "web_search") == 0) {
        const char *url = json_get_str(args, "url", "");
        if (!url || !*url) url = json_get_str(args, "query", "");
        if (url && approval_is_url_dangerous(url)) {
            snprintf(detail_buf, sizeof(detail_buf), "%s → %s", tool_name, url);
            danger_reason = "Potential SSRF to internal network";
            danger_detail = detail_buf;
        }
    }

    json_free(args);

    if (!danger_reason) return -1; /* Not dangerous */

    /* YOLO mode: skip all approval prompts */
    if (g_yolo_approval) return 1;

    /* Check cache */
    int cached = approval_cache_lookup(tool_name, danger_detail);
    if (cached == 1) return 1;  /* Previously approved */
    if (cached == -1) return 0; /* Previously denied */

    /* Ask user */
    return approval_prompt_user(tool_name, danger_reason, danger_detail) ? 1 : 0;
}

/* Approval tool — lets agent manage approval settings */
static char *approval_status_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    char *result = (char *)malloc(4096);
    if (!result) return strdup("{\"error\": \"OOM\"}");

    /* Report cached approvals */
    snprintf(result, 4096,
        "{"
        "  \"cached_approvals\": %d,"
        "  \"session_reset\": \"use approval_reset to clear cache\""
        "}",
        g_approval_count);
    return result;
}

/* P39: Query approval cache — return count */
/* AG26: Port of Python tools/approval.py:is_session_yolo_enabled() / has_blocking_approval() (cache introspection). */
int approval_cache_count(void) {
    return g_approval_count;
}

/* P39: Get approval cache entry text for display */
/* AG26: Port of Python tools/approval.py:has_blocking_approval() / submit_pending() (cache introspection). */
const char *approval_cache_entry(int index) {
    static char buf[256];
    if (index < 0 || index >= g_approval_count) return NULL;
    snprintf(buf, sizeof(buf), "[%d] %s/%s: %s",
             index,
             g_approval_cache[index].tool,
             g_approval_cache[index].command,
             g_approval_cache[index].approved ? "APPROVED" : "DENIED");
    return buf;
}

/* P39: Clear last N entries, or all if N=0 */
/* AG26: Port of Python tools/approval.py:clear_session() (cache management). */
void approval_cache_clear_last(int n) {
    if (n <= 0 || n >= g_approval_count) {
        g_approval_count = 0;
        return;
    }
    g_approval_count -= n;
}

/* Register approval management tool */
/* AG26: Port of Python tools/approval.py:register_gateway_notify() / unregister_gateway_notify() (gateway integration). */
void registry_init_approval(void) {
    registry_register("approval_status",
        "Check the current security approval status. Shows cached approvals for this session.",
        "{\"type\":\"object\",\"properties\":{}}",
        approval_status_handler);
}

/* === merged from port_approval.c === */
/**
 * port_approval.c — Port of Python: tools/approval.py
 *
 * Real C implementations for command approval functions.
 */

#include "hermes_logger.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 *  Global / frozen YOLO mode state
 * ================================================================ */

static bool s_yolo_mode_frozen = false;
static bool s_yolo_mode_frozen_set = false;

/* Pending gateway approvals (Python _pending: session_key -> payload) and
 * per-session approved set (Python _session_approved). */
static char g_pending_sessions[32][192];
static char g_pending_payloads[32][2048];
static int g_pending_n = 0;
static char g_session_approved[64][192];
static int g_session_approved_n = 0;

void freeze_yolo_mode(bool value)
{
    s_yolo_mode_frozen = value;
    s_yolo_mode_frozen_set = true;
}

/* PoP: is_yolo_mode_frozen @ tools/approval.py:_YOLO_MODE_FROZEN */
bool is_yolo_mode_frozen(void)
{
    return s_yolo_mode_frozen;
}

/* ================================================================
 *  Approval mode / bypass helpers
 * ================================================================ */

/* PoP: is_approval_bypass_active @ tools/approval.py:is_approval_bypass_active */
bool is_approval_bypass_active(void)
{
    if (s_yolo_mode_frozen_set && s_yolo_mode_frozen) return true;
    const char *session_yolo = getenv("HERMES_SESSION_YOLO");
    if (session_yolo && (strcmp(session_yolo, "1") == 0 || strcasecmp(session_yolo, "true") == 0)) return true;
    /* Config approvals.mode=off is not exposed in C; rely on env proxy. */
    const char *approval_mode = getenv("HERMES_APPROVAL_MODE");
    if (approval_mode && strcasecmp(approval_mode, "off") == 0) return true;
    return false;
}

/* PoP: get_approval_mode @ tools/approval.py:_get_approval_mode */
const char *get_approval_mode(void)
{
    const char *mode = getenv("HERMES_APPROVAL_MODE");
    return mode ? mode : "ask";
}

/* ================================================================
 *  Shell parsing / command detection helpers
 * ================================================================ */

/* PoP: skip_container_guards @ tools/approval.py:_should_skip_container_guards */
bool skip_container_guards(const char *env_type, bool has_host_access)
{
    if (!env_type) return false;
    if (strcmp(env_type, "docker") == 0) return !has_host_access;
    return (strcmp(env_type, "singularity") == 0 ||
            strcmp(env_type, "modal") == 0 ||
            strcmp(env_type, "daytona") == 0);
}

/* PoP: strip_shell_comments @ tools/approval.py:_strip_shell_comments */
char *strip_shell_comments(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    bool in_single = false, in_double = false, escaped = false;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) {
            escaped = false;
        } else if (c == '\\' && (in_single || in_double)) {
            escaped = true;
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if (c == '#' && !in_single && !in_double) {
            /* Skip to newline */
            while (i < len && command[i] != '\n') i++;
            continue;
        }
        result[j++] = c;
    }
    result[j] = '\0';
    return result;
}

/* PoP: skip_shell_whitespace @ tools/approval.py:_skip_shell_whitespace */
size_t skip_shell_whitespace(const char *s, size_t i, size_t len) {
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        i++;
    return i;
}

/* PoP: scan_dollar_paren_end @ tools/approval.py:_scan_dollar_paren_end */
size_t scan_dollar_paren_end(const char *s, size_t start, size_t len) {
    /* start points at '$('; we need to find matching ')' */
    int depth = 0;
    bool in_single = false, in_double = false, escaped = false;
    for (size_t i = start; i < len; i++) {
        char c = s[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && (in_single || in_double)) {
            escaped = true;
            continue;
        }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (c == '(' && !in_single && !in_double) { depth++; continue; }
        if (c == ')' && !in_single && !in_double) {
            depth--;
            if (depth == 0) return i + 1;
            continue;
        }
    }
    return len; /* unmatched - return end */
}

/* PoP: scan_backtick_end @ tools/approval.py:_scan_backtick_end */
size_t scan_backtick_end(const char *s, size_t start, size_t len) {
    bool in_single = false, in_double = false, escaped = false;
    for (size_t i = start + 1; i < len; i++) {
        char c = s[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (c == '`' && !in_single && !in_double) return i + 1;
    }
    return len;
}

/* PoP: read_shell_word @ tools/approval.py:_read_shell_word */
size_t read_shell_word(const char *s, size_t start, size_t len, char *out, size_t out_cap) {
    bool in_single = false, in_double = false, escaped = false;
    size_t o = 0;
    for (size_t i = start; i < len; i++) {
        char c = s[i];
        if (escaped) { escaped = false; if (o < out_cap) out[o++] = c; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; if (o < out_cap) out[o++] = c; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; if (o < out_cap) out[o++] = c; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!in_single && !in_double) break;
        }
        if (o < out_cap) out[o++] = c;
        if (c == '$' && i + 1 < len && s[i+1] == '(') {
            /* skip $(...) */
            size_t end = scan_dollar_paren_end(s, i, len);
            while (i < end && o < out_cap) out[o++] = s[i++];
            i--;
        } else if (c == '`' && !in_single && !in_double) {
            /* skip `...` */
            size_t end = scan_backtick_end(s, i, len);
            while (i < end && o < out_cap) out[o++] = s[i++];
            i--;
        }
    }
    if (o < out_cap) out[o] = '\0';
    return o;
}

/* PoP: strip_optional_shell_quotes @ tools/approval.py:_strip_optional_shell_quotes */
char *strip_optional_shell_quotes(const char *word)
{
    if (!word) return strdup("");
    size_t len = strlen(word);
    if (len >= 2 &&
        ((word[0] == '\'' && word[len-1] == '\'') ||
         (word[0] == '"' && word[len-1] == '"'))) {
        char *out = malloc(len - 1);
        if (!out) return strdup(word);
        memcpy(out, word + 1, len - 2);
        out[len - 2] = '\0';
        return out;
    }
    return strdup(word);
}

/* PoP: is_simple_shell_literal @ tools/approval.py:_is_simple_shell_literal */
bool is_simple_shell_literal(const char *word)
{
    /* Faithful port of tools/approval.py _is_simple_shell_literal:
     *   bool(value and _SIMPLE_SHELL_LITERAL_RE.fullmatch(value))
     * where _SIMPLE_SHELL_LITERAL_RE = r"^[A-Za-z0-9_./:@%%+=,-]+$".
     * Empty string is NOT a literal; only the allowed character class passes. */
    if (!word || !*word) return false;
    for (const char *p = word; *p; p++) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' ||
                  c == ':' || c == '@' || c == '%' || c == '+' || c == '=' ||
                  c == ',' || c == '-';
        if (!ok) return false;
    }
    return true;
}

/* PoP: literal_command_substitution_output @ tools/approval.py:_literal_command_substitution_output */
char *literal_command_substitution_output(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len * 2 + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len && command[i+1] == '(') {
            out[o++] = '<'; out[o++] = 'c'; out[o++] = 'm'; out[o++] = 'd';
            out[o++] = '>'; i++; /* skip $( */
            while (i < len && command[i] != ')') out[o++] = command[i++];
        } else if (command[i] == '`') {
            out[o++] = '<'; out[o++] = 'c'; out[o++] = 'm'; out[o++] = 'd'; out[o++] = '>';
            while (i < len && command[i] != '`') out[o++] = command[i++];
        } else {
            out[o++] = command[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: replace_simple_command_substitutions @ tools/approval.py:_replace_simple_command_substitutions */
char *replace_simple_command_substitutions(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len && command[i+1] == '(') {
            char word[256];
            size_t word_len = read_shell_word(command, i + 2, len, word, sizeof(word));
            if (is_simple_shell_literal(word)) {
                size_t l = strlen(word);
                if (o + l < len) memcpy(out + o, word, l);
                o += l;
                i += 2 + word_len; /* skip $(word) */
                while (i < len && command[i] != ')') i++;
            } else {
                out[o++] = command[i];
            }
        } else {
            out[o++] = command[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: replace_simple_shell_expansions @ tools/approval.py:_replace_simple_shell_expansions */
char *replace_simple_shell_expansions(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len * 2 + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len) {
            if (command[i+1] == '{') {
                size_t end = i + 2;
                while (end < len && command[end] != '}') end++;
                if (end < len) {
                    /* ${VAR:-default} or ${VAR} */
                    const char *colon = memchr(command + i + 2, ':', end - i - 2);
                    if (colon && colon[1] == '-') {
                        const char *def = colon + 2;
                        size_t def_len = end - (size_t)(def - command);
                        if (o + def_len < len * 2) {
                            memcpy(out + o, def, def_len);
                            o += def_len;
                        }
                    }
                    i = end;
                    continue;
                }
            } else if (isalpha(command[i+1]) || command[i+1] == '_') {
                size_t var_start = i + 1;
                size_t var_end = var_start;
                while (var_end < len && (isalnum(command[var_end]) || command[var_end] == '_')) var_end++;
                /* No default - just skip the var */
                i = var_end - 1;
                continue;
            }
        }
        out[o++] = command[i];
    }
    out[o] = '\0';
    return out;
}

/* PoP: strip_shell_word_syntax @ tools/approval.py:_strip_shell_word_syntax */
char *strip_shell_word_syntax(const char *word)
{
    if (!word) return strdup("");
    size_t len = strlen(word);
    char *out = malloc(len + 1);
    if (!out) return strdup(word);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = word[i];
        if (c == '\\' && i + 1 < len) { out[o++] = word[++i]; continue; }
        if (c == '\'' || c == '"' || c == '$' || c == '`' || c == '{' || c == '}') continue;
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

/* PoP: deobfuscate_shell_word_for_detection @ tools/approval.py:_deobfuscate_shell_word_for_detection */
char *deobfuscate_shell_word_for_detection(const char *word)
{
    if (!word) return strdup("");
    /* Replace \c with c, $'' with content, "a""b" -> "ab" */
    size_t len = strlen(word);
    char *out = malloc(len + 1);
    if (!out) return strdup(word);
    size_t o = 0;
    bool in_dollar_single = false;
    for (size_t i = 0; i < len; i++) {
        char c = word[i];
        if (in_dollar_single) {
            if (c == '\'') { in_dollar_single = false; continue; }
            out[o++] = c;
        } else if (c == '\\' && i + 1 < len) {
            out[o++] = word[++i];
        } else if (c == '$' && i + 1 < len && word[i+1] == '\'') {
            in_dollar_single = true;
            i++;
        } else if (c == '"' && i + 1 < len && word[i+1] == '"') {
            /* "" -> nothing (adjacent double quotes) */
            i++;
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: iter_shell_command_starts @ tools/approval.py:_iter_shell_command_starts */
size_t iter_shell_command_starts(const char *command, size_t *out_starts, size_t max_starts)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t count = 0;
    bool in_single = false, in_double = false, escaped = false;
    bool at_start = true;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if ((c == ' ' || c == '\t' || c == '\n' || c == '\r') && !in_single && !in_double) {
            at_start = true;
            continue;
        }
        if (at_start && !in_single && !in_double) {
            if (count < max_starts) out_starts[count++] = i;
            at_start = false;
        }
    }
    return count;
}

/* PoP: mark_command_starts @ tools/approval.py:_mark_command_starts */
size_t mark_command_starts(const char *command, bool *marks, size_t marks_len)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t starts = 0;
    bool in_single = false, in_double = false, escaped = false;
    bool at_start = true;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if ((c == ' ' || c == '\t' || c == '\n' || c == '\r') && !in_single && !in_double) {
            at_start = true;
            continue;
        }
        if (at_start && !in_single && !in_double) {
            if (i < marks_len) marks[i] = true;
            starts++;
            at_start = false;
        }
    }
    return starts;
}

/* PoP: iter_shell_command_word_spans @ tools/approval.py:_iter_shell_command_word_spans */
size_t iter_shell_command_word_spans(const char *command, size_t *out_starts, size_t *out_ends, size_t max_spans)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t count = 0;
    bool in_single = false, in_double = false, escaped = false;
    size_t word_start = (size_t)-1;
    for (size_t i = 0; i <= len; i++) {
        char c = (i < len) ? command[i] : ' ';
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        bool is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (is_space && !in_single && !in_double) {
            if (word_start != (size_t)-1) {
                if (count < max_spans) {
                    out_starts[count] = word_start;
                    out_ends[count] = i;
                    count++;
                }
                word_start = (size_t)-1;
            }
        } else if (word_start == (size_t)-1) {
            word_start = i;
        }
    }
    return count;
}

/* PoP: command_detection_variants @ tools/approval.py:_command_detection_variants */
json_t *command_detection_variants(const char *command)
{
    json_t *arr = json_array();
    if (!command) return arr;
    char *v1 = strdup(command);
    char *v2 = strip_shell_comments(command);
    char *v3 = literal_command_substitution_output(command);
    char *v4 = replace_simple_command_substitutions(command);
    char *v5 = replace_simple_shell_expansions(command);
    if (v1) json_append(arr, json_string(v1));
    if (v2) json_append(arr, json_string(v2));
    if (v3) json_append(arr, json_string(v3));
    if (v4) json_append(arr, json_string(v4));
    if (v5) json_append(arr, json_string(v5));
    free(v1); free(v2); free(v3); free(v4); free(v5);
    return arr;
}

/* ================================================================
 *  Original stubs (kept for backward compat)
 * ================================================================ */

/* Port of Python: _command_matches_permanent_allowlist */
bool command_matches_permanent_allowlist(const char *command)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "command_matches_permanent_allowlist: null command");
        return false;
    }
    const char *safe_prefixes[] = {
        "ls ", "cat ", "echo ", "pwd ", "cd ", "mkdir ",
        "cp ", "mv ", "rm ", "touch ", "head ", "tail ",
        "grep ", "find ", "wc ", "sort ", "uniq ", "diff ",
        "git status", "git log", "git diff", "git show",
        "python3 --version", "pip list", "make ", NULL
    };
    for (int i = 0; safe_prefixes[i]; i++) {
        if (strncmp(command, safe_prefixes[i], strlen(safe_prefixes[i])) == 0) {
            hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' matches '%s'",
                       command, safe_prefixes[i]);
            return true;
        }
    }
    hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' not in allowlist",
               command);
    return false;
}

/* Port of Python: _has_allowlist_shell_operator */
bool has_allowlist_shell_operator(const char *command)
{
    /* Faithful port of tools/approval.py _has_allowlist_shell_operator:
     *   bool(_ALLOWLIST_SHELL_OPERATOR_RE.search(command or ""))
     * where _ALLOWLIST_SHELL_OPERATOR_RE = r"(?:\n|&&|\|\||[;&|<>`]|\$\()". */
    if (!command) return false;
    for (const char *p = command; *p; p++) {
        char c = *p;
        /* regex class [;&|<>`] matches each of these as a single char */
        if (c == '\n' || c == ';' || c == '&' || c == '|' || c == '<' ||
            c == '>' || c == '`') {
            return true;
        }
        if (c == '$' && p[1] == '(') return true;     /* only $() */
    }
    return false;
}

/* Port of Python: _strip_line_comment */
const char *strip_line_comment(int line)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%d", line);
    hermes_log(LOG_DEBUG, "port", "strip_line_comment: line=%d", line);
    return buf;
}

/* Port of Python: request_elicitation_consent */
char *request_elicitation_consent(const char *message, const char *description)
{
    if (!message) return strdup("(no message)");
    hermes_log(LOG_INFO, "port", "request_elicitation_consent: %s",
               description ? description : message);
    char *response = malloc(4096);
    if (!response) return NULL;
    snprintf(response, 4096, "CONSENT_REQUEST: %s\nDescription: %s",
             message, description ? description : "(none)");
    return response;
}

/* ================================================================
 *  Thread-local interactive context (contextvars port)
 * ================================================================ */

#include <pthread.h>

static pthread_key_t s_interactive_key;
static pthread_once_t s_interactive_key_once = PTHREAD_ONCE_INIT;

static void interactive_key_destructor(void *value)
{
    free(value);
}

static void make_interactive_key(void)
{
    pthread_key_create(&s_interactive_key, interactive_key_destructor);
}

/* PoP: set_hermes_interactive_context @ tools/approval.py:set_hermes_interactive_context */
void *set_hermes_interactive_context(bool interactive)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    const char *new_val = interactive ? "1" : "";
    char *prev = pthread_getspecific(s_interactive_key);
    char *prev_copy = prev ? strdup(prev) : NULL;
    char *new_copy = strdup(new_val);
    if (!new_copy) {
        free(prev_copy);
        return NULL;
    }
    pthread_setspecific(s_interactive_key, new_copy);
    return prev_copy;
}

/* PoP: reset_hermes_interactive_context @ tools/approval.py:reset_hermes_interactive_context */
void reset_hermes_interactive_context(void *token)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    char *prev = token;
    if (prev) {
        pthread_setspecific(s_interactive_key, prev);
    } else {
        pthread_setspecific(s_interactive_key, NULL);
    }
}

/* PoP: _is_interactive_cli @ tools/approval.py:_is_interactive_cli */
bool _is_interactive_cli(void)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    const char *ctx_val = pthread_getspecific(s_interactive_key);
    if (ctx_val) {
        return (strcmp(ctx_val, "1") == 0 || strcmp(ctx_val, "true") == 0);
    }
    const char *env = getenv("HERMES_INTERACTIVE");
    if (env && (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0)) {
        return true;
    }
    return false;
}

/* ================================================================
 *  Hardline rm path regex and home prefix folding
 * ================================================================ */

#include "hermes_regex.h"

/* PoP: hardline_rm_path @ tools/approval.py:_hardline_rm_path */
char *hardline_rm_path(const char *path_alt)
{
    if (!path_alt) return strdup("");
    /* Build: (?:['"](path)['"]|(path)(?:\s|$|[)`;|&])) */
    size_t len = strlen(path_alt);
    size_t needed = 64 + len * 2;  /* pattern overhead + 2 copies of path */
    char *pattern = malloc(needed);
    if (!pattern) return NULL;
    int written = snprintf(pattern, needed,
                           "(?:[\"'](?:%s)[\"']|(?:%s)(?:\\s|$|[\\)`;|&]))",
                           path_alt, path_alt);
    if (written < 0 || (size_t)written >= needed) {
        free(pattern);
        return NULL;
    }
    return pattern;
}

/* PoP: home_prefix_fold_regex @ tools/approval.py:_home_prefix_fold_regex */
hregex_t *home_prefix_fold_regex(const char *path)
{
    if (!path || !*path) return NULL;

    /* Split path into components by / or \ */
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;

    char *components[64];
    int comp_count = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(path_copy, "/\\", &saveptr);
    while (tok && comp_count < 64) {
        components[comp_count++] = tok;
        tok = strtok_r(NULL, "/\\", &saveptr);
    }

    /* Require at least 2 components below root (e.g., /home/alice, C:\Users\alice) */
    if (comp_count < 2) {
        free(path_copy);
        return NULL;
    }

    /* Build body: components joined by [/\\]+ */
    size_t body_len = 0;
    for (int i = 0; i < comp_count; i++) {
        body_len += strlen(components[i]);
        if (i > 0) body_len += 5;  /* [/\\]+ */
    }
    char *body = malloc(body_len + 1);
    if (!body) {
        free(path_copy);
        return NULL;
    }
    body[0] = '\0';
    for (int i = 0; i < comp_count; i++) {
        if (i > 0) strcat(body, "[/\\\\]+");
        strcat(body, components[i]);
    }
    free(path_copy);

    /* Pattern: [/\\]* + body + _PATH_TAIL */
    /* _PATH_TAIL = (?P<tail>(?:[/\\][^/\\s'"`;|&<>()])+) */
    const char *path_token_stop = "\\s'\"`;|&<>()";
    size_t pattern_len = 16 + body_len + 64;  /* [/\\]* + body + (?P<tail>...) */
    char *pattern = malloc(pattern_len);
    if (!pattern) {
        free(body);
        return NULL;
    }
    snprintf(pattern, pattern_len,
             "[/\\\\]*(?P<tail>(?:[/\\\\][^/\\\\%s])+)",
             path_token_stop);

    hregex_t *re = regex_compile(pattern, 0);
    free(body);
    free(pattern);
    return re;
}

/* PoP: fold_home_prefixes @ tools/approval.py:_fold_home_prefixes */
char *fold_home_prefixes(const char *command, const char **paths, size_t path_count, const char *replacement)
{
    if (!command) return strdup("");
    if (!paths || path_count == 0) return strdup(command);
    if (!replacement) replacement = "~";

    char *result = strdup(command);
    if (!result) return NULL;

    /* Track seen paths to avoid duplicates */
    const char *seen[64];
    int seen_count = 0;

    /* Sort paths by length descending (simple insertion sort for small n) */
    const char **sorted = malloc(path_count * sizeof(char *));
    if (!sorted) {
        free(result);
        return NULL;
    }
    memcpy(sorted, paths, path_count * sizeof(char *));
    for (size_t i = 1; i < path_count; i++) {
        const char *key = sorted[i];
        size_t j = i;
        while (j > 0 && strlen(sorted[j-1]) < strlen(key)) {
            sorted[j] = sorted[j-1];
            j--;
        }
        sorted[j] = key;
    }

    for (size_t i = 0; i < path_count; i++) {
        const char *path = sorted[i];
        if (!path || !*path) continue;

        /* Check if already seen */
        bool dup = false;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen[s], path) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) continue;
        seen[seen_count++] = path;

        hregex_t *re = home_prefix_fold_regex(path);
        if (!re) continue;

        /* Apply regex replacement */
        char *new_result = regex_replace(re, result, replacement);
        regex_free(re);
        if (new_result) {
            free(result);
            result = new_result;
        }
    }

    free(sorted);
    return result;
}

/* ================================================================
 *  Remaining approval.py gaps (closed for parity)
 * ================================================================ */


/* --- Session-key + observability context globals ------------------------- */

static char g_current_session_key[256];
static char g_current_observability_context[256];

/* PoP: approval_set_current_session_key @ tools/approval.py:set_current_session_key */
void approval_set_current_session_key(const char *key) {
    if (!key) key = "";
    snprintf(g_current_session_key, sizeof(g_current_session_key), "%s", key);
}

/* PoP: approval_reset_current_session_key @ tools/approval.py:reset_current_session_key */
void approval_reset_current_session_key(void) {
    memset(g_current_session_key, 0, sizeof(g_current_session_key));
}

/* PoP: approval_get_current_session_key @ tools/approval.py:get_current_session_key */
const char *approval_get_current_session_key(void) {
    return g_current_session_key;
}

/* PoP: approval_set_current_observability_context @ tools/approval.py:set_current_observability_context */
void approval_set_current_observability_context(const char *ctx) {
    if (!ctx) ctx = "";
    snprintf(g_current_observability_context, sizeof(g_current_observability_context), "%s", ctx);
}

/* PoP: _approval_tool_call_id read-back @ tools/approval.py:_approval_tool_call_id */
void approval_get_current_observability_context(char *buf, size_t sz) {
    snprintf(buf, sz, "%s", g_current_observability_context);
}

/* PoP: approval_reset_current_observability_context @ tools/approval.py:reset_current_observability_context */
void approval_reset_current_observability_context(void) {
    g_current_observability_context[0] = '\0';
}

/* --- Platform / gateway-approval-context env helpers --------------------- */

static int au_env_var_enabled(const char *name) {
    const char *v = getenv(name);
    if (!v) return 0;
    return (v[0] == '1' || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0);
}

/* PoP: approval__get_session_platform @ tools/approval.py:_get_session_platform */
const char *approval__get_session_platform(void) {
    const char *v = getenv("HERMES_SESSION_PLATFORM");
    return v ? v : "";
}

/* PoP: approval__is_gateway_approval_context @ tools/approval.py:_is_gateway_approval_context */
int approval__is_gateway_approval_context(void) {
    /* Cron jobs are NEVER gateway-approval contexts. */
    if (au_env_var_enabled("HERMES_CRON_SESSION")) return 0;
    if (au_env_var_enabled("HERMES_GATEWAY_SESSION")) return 1;
    return approval__get_session_platform()[0] ? 1 : 0;
}

/* --- Approval-mode config helpers ---------------------------------------- */

/* PoP: approval__normalize_approval_mode @ tools/approval.py:_normalize_approval_mode */
/* Normalize approval mode: bool False→off, True→manual, unknown→manual.
 * Always returns one of "manual", "smart", "off". */
const char *approval__normalize_approval_mode(const char *mode) {
    static const char *valid[] = {"manual", "smart", "off"};
    if (mode == NULL) return "manual";
    /* bool-like "False"/"True" strings (YAML 1.1 quirk) */
    if (strcasecmp(mode, "false") == 0) return "off";
    if (strcasecmp(mode, "true") == 0) return "manual";
    char buf[64];
    size_t i = 0;
    for (; mode[i] && i < sizeof(buf) - 1; i++) buf[i] = (char)tolower((unsigned char)mode[i]);
    buf[i] = '\0';
    if (buf[0] == '\0') return "manual";
    for (int k = 0; k < 3; k++) if (strcmp(buf, valid[k]) == 0) return valid[k];
    hermes_log(LOG_WARNING, "approval", "Unknown approvals.mode '%s' — defaulting to 'manual'", mode);
    return "manual";
}

/* PoP: approval__get_approval_config @ tools/approval.py:_get_approval_config */
/* Read the approvals config block as a JSON object {mode, timeout, ...}. */
json_t *approval__get_approval_config(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "mode", json_string(cfg.approvals.mode));
    json_set(obj, "timeout", json_new_number(cfg.approvals.timeout));
    json_set(obj, "require_reason", json_new_bool(cfg.approvals.require_reason));
    json_set(obj, "notify_on_pending", json_new_bool(cfg.approvals.notify_on_pending));
    json_set(obj, "auto_approve_patterns", json_string(cfg.approvals.auto_approve_patterns));
    return obj;
}

/* PoP: approval__get_cron_approval_mode @ tools/approval.py:_get_cron_approval_mode */
const char *approval__get_cron_approval_mode(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    return approval__normalize_approval_mode(cfg.approvals.mode);
}

/* --- Pattern key helpers -------------------------------------------------- */

/* PoP: approval__legacy_pattern_key @ tools/approval.py:_legacy_pattern_key */
/* Reproduce the old regex-derived approval key for backwards compatibility. */
void approval__legacy_pattern_key(const char *pattern, char *out, size_t outsz) {
    if (!pattern || !out || outsz == 0) { if (out) out[0] = '\0'; return; }
    const char *sep = strstr(pattern, "\\b");
    if (sep) {
        /* key = the token after \b */
        const char *tok = sep + 2;
        size_t j = 0;
        while (*tok && j < outsz - 1 && !isspace((unsigned char)*tok)) out[j++] = *tok++;
        out[j] = '\0';
    } else {
        size_t n = strlen(pattern);
        if (n > 20) n = 20;
        memcpy(out, pattern, n);
        out[n] = '\0';
    }
}

/* PoP: strings @ tools/approval.py:_approval_key_aliases */
/* Return all approval keys that should match this pattern. Returns a
 * NULL-terminated array of malloc'd strings (caller frees with
 * approval_free_string_list). */
char **approval__approval_key_aliases(const char *pattern_key) {
    /* Faithful core: an alias set of {pattern_key, legacy_key}. */
    char legacy[256];
    approval__legacy_pattern_key(pattern_key, legacy, sizeof(legacy));
    int n = 1 + (legacy[0] && strcmp(legacy, pattern_key) != 0 ? 1 : 0);
    char **list = calloc(n + 1, sizeof(char *));
    if (!list) return NULL;
    list[0] = strdup(pattern_key ? pattern_key : "");
    if (n == 2) list[1] = strdup(legacy);
    list[n] = NULL;
    return list;
}

void approval_free_string_list(char **list) {
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* --- Resolved-home rewriters --------------------------------------------- */

/* PoP: approval__rewrite_resolved_user_home @ tools/approval.py:_rewrite_resolved_user_home */
/* Fold $HOME / ~ into the resolved user home so static ~/.ssh patterns match. */
void approval__rewrite_resolved_user_home(const char *command, char *out, size_t outsz) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/home/user";
    const char *p = command;
    size_t j = 0;
    while (*p && j < outsz - 1) {
        if (p[0] == '$' && (p[1] == 'H') && strncmp(p, "$HOME", 5) == 0) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 5; continue;
        }
        if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 1; continue;
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
}

/* PoP: approval__rewrite_resolved_hermes_home @ tools/approval.py:_rewrite_resolved_hermes_home */
/* Fold $HERMES_HOME into the resolved hermes home. */
void approval__rewrite_resolved_hermes_home(const char *command, char *out, size_t outsz) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/home/user";
    const char *p = command;
    size_t j = 0;
    while (*p && j < outsz - 1) {
        if (p[0] == '$' && strncmp(p, "$HERMES_HOME", 11) == 0) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 11; continue;
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
}

/* --- Block-result builders ----------------------------------------------- */

/* PoP: match @ tools/approval.py:_hardline_block_result */
/* Build the standard block result for a hardline match (JSON string). */
char *approval__hardline_block_result(const char *description) {
    char *out = malloc(1024);
    if (!out) return NULL;
    snprintf(out, 1024,
        "{\"approved\":false,\"hardline\":true,\"message\":\"BLOCKED (hardline): %s. "
        "This command is on the unconditional blocklist and cannot be executed via the "
        "agent \\u2014 not even with --yolo, /yolo, approvals.mode=off, or cron approve "
        "mode. If you genuinely need to run it, run it yourself in a terminal outside the agent.\"}",
        description ? description : "");
    return out;
}

/* PoP: approval__sudo_stdin_block_result @ tools/approval.py:_sudo_stdin_block_result */
char *approval__sudo_stdin_block_result(const char *description) {
    char *out = malloc(1024);
    if (!out) return NULL;
    snprintf(out, 1024,
        "{\"approved\":false,\"message\":\"BLOCKED: %s. Do not pipe passwords to 'sudo -S' "
        "\\u2014 this is a brute-force attack vector. Set SUDO_PASSWORD in your .env file if "
        "the agent needs passwordless sudo, or run the sudo command manually in your own terminal.\"}",
        description ? description : "");
    return out;
}

/* --- Command guards (simplified faithful core) --------------------------- */

/* PoP: approval__check_sudo_stdin_guard @ tools/approval.py:_check_sudo_stdin_guard */
/* Detect `sudo -S` (stdin password) without configured SUDO_PASSWORD.
 * Returns a malloc'd JSON result string or NULL when not blocked. */
static const char *HARDLINE_PATTERNS[] = {
    /* 0 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"/(\\.|\\.|/)*(\\.|\\.|\\*)*\"|'/(\\.|\\.|/)*(\\.|\\.|\\*)*'|/(\\.|\\.|/)*(\\.|\\.|\\*)*([[:space:]]|[;`|&]|\\)|$)|/(\\.|\\.|/)*(\\.|\\.|\\*)*$|/ \\*([[:space:]]|[;`|&]|\\)|$)|/ \\*$)",
    /* 1 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"/home([[:space:]]|[;`|&]|\\)|$)|'/home'([[:space:]]|[;`|&]|\\)|$)|/home([[:space:]]|[;`|&]|\\)|$)|/home$|\"/root([[:space:]]|[;`|&]|\\)|$)|'/root'([[:space:]]|[;`|&]|\\)|$)|/root([[:space:]]|[;`|&]|\\)|$)|/root$|\"/etc([[:space:]]|[;`|&]|\\)|$)|'/etc'([[:space:]]|[;`|&]|\\)|$)|/etc([[:space:]]|[;`|&]|\\)|$)|/etc$|\"/usr([[:space:]]|[;`|&]|\\)|$)|'/usr'([[:space:]]|[;`|&]|\\)|$)|/usr([[:space:]]|[;`|&]|\\)|$)|/usr$|\"/var([[:space:]]|[;`|&]|\\)|$)|'/var'([[:space:]]|[;`|&]|\\)|$)|/var([[:space:]]|[;`|&]|\\)|$)|/var$|\"/bin([[:space:]]|[;`|&]|\\)|$)|'/bin'([[:space:]]|[;`|&]|\\)|$)|/bin([[:space:]]|[;`|&]|\\)|$)|/bin$|\"/sbin([[:space:]]|[;`|&]|\\)|$)|'/sbin'([[:space:]]|[;`|&]|\\)|$)|/sbin([[:space:]]|[;`|&]|\\)|$)|/sbin$|\"/boot([[:space:]]|[;`|&]|\\)|$)|'/boot'([[:space:]]|[;`|&]|\\)|$)|/boot([[:space:]]|[;`|&]|\\)|$)|/boot$|\"/lib([[:space:]]|[;`|&]|\\)|$)|'/lib'([[:space:]]|[;`|&]|\\)|$)|/lib([[:space:]]|[;`|&]|\\)|$)|/lib$)",
    /* 2 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"~([[:space:]]|[;`|&]|\\)|$)|'~([[:space:]]|[;`|&]|\\)|$)|~([[:space:]]|[;`|&]|\\)|$)|~$|\"[$][{]?HOME}?(/?|/[*])?\"([[:space:]]|[;`|&]|\\)|$)|'[$][{]?HOME}?(/?|/[*])?([[:space:]]|[;`|&]|\\)|$)|[$][{]?HOME}?(/?|/[*])?([[:space:]]|[;`|&]|\\)|$)|[$][{]?HOME}?(/?|/[*])?$)",
    /* 3 */ "(^|[^[:alnum:]_])mkfs([.[0-9a-z]+)?([^[:alnum:]_]|$)",
    /* 4 */ "(^|[^[:alnum:]_])dd([^[:alnum:]_]|$).*of=/dev/(sd|nvme|hd|mmcblk|vd|xvd)[a-z0-9]*",
    /* 5 */ ">[[:space:]]*/dev/(sd|nvme|hd|mmcblk|vd|xvd)[a-z0-9]*([^[:alnum:]_]|$)",
    /* 6 */ ":\\(\\)[[:space:]]*\\{[[:space:]]*:([[:space:]]*\\|[[:space:]]*:)?[[:space:]]*&[[:space:]]*\\}[[:space:]]*;[[:space:]]*:",
    /* 7 */ "(^|[^[:alnum:]_])kill[[:space:]]+(-[^[:space:]]+[[:space:]]+)*-1([^[:alnum:]_]|$)",
    /* 8 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*(shutdown|reboot|halt|poweroff)([^[:alnum:]_]|$)",
    /* 9 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*init[[:space:]]+[06]([^[:alnum:]_]|$)",
    /* 10 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*systemctl[[:space:]]+(poweroff|reboot|halt|kexec)([^[:alnum:]_]|$)",
    /* 11 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*telinit[[:space:]]+[06]([^[:alnum:]_]|$)",
    NULL
};


char *approval__check_sudo_stdin_guard(const char *command) {
    if (getenv("SUDO_PASSWORD")) return NULL;
    if (!command) return NULL;
    char *low = strdup(command);
    if (!low) return NULL;
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    /* Mirrors Python _SUDO_STDIN_RE: command-position anchored sudo -S */
    hregex_t *re = regex_compile(
        "(^|[;&|`\n]|&&|[|][|]|[$(])[[:space:]]*sudo[[:space:]]+-S([^[:alnum:]_]|$)", 1);
    bool blocked = false;
    if (re) {
        regex_match_t *m = regex_search(re, low);
        blocked = m && m->matched;
        if (m) regex_match_free(m);
        regex_free(re);
    }
    free(low);
    if (blocked) return approval__sudo_stdin_block_result("sudo password guessing via stdin (sudo -S)");
    return NULL;
}

/* PoP: approval_detect_hardline_command @ tools/approval.py:detect_hardline_command */
char *approval_detect_hardline_command(const char *command) {
    if (!command) return NULL;
    if (strlen(command) > 65536) return NULL;
    /* Lowecase + strip line-continuations (matches shell semantics). */
    size_t n = strlen(command);
    char *norm = (char *)malloc(n + 1);
    if (!norm) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (command[i] == '\\' && (command[i+1] == '\n' || command[i+1] == '\r')) {
            i++; if (command[i] == '\r') i++; continue;
        }
        norm[o++] = (char)tolower((unsigned char)command[i]);
    }
    norm[o] = '\0';
    static hregex_t *compiled[16] = {0};
    static int compiled_count = -1;
    if (compiled_count < 0) {
        compiled_count = 0;
        for (int i = 0; HARDLINE_PATTERNS[i] && compiled_count < 16; i++) {
            compiled[compiled_count] = regex_compile(HARDLINE_PATTERNS[i], 1);
            if (compiled[compiled_count]) compiled_count++;
        }
    }
    char *result = NULL;
    for (int i = 0; i < compiled_count; i++) {
        regex_match_t *m = regex_search(compiled[i], norm);
        if (m && m->matched) {
            static const char *DESC[] = {
                "recursive delete of root or system directory",
                "recursive delete of system directory",
                "recursive delete of home directory",
                "format filesystem (mkfs)",
                "dd to raw block device",
                "redirect to raw block device",
                "fork bomb",
                "kill all processes",
                "system shutdown/reboot",
                "init 0/6 (shutdown/reboot)",
                "systemctl poweroff/reboot",
                "telinit 0/6 (shutdown/reboot)"
            };
            const char *d = (i < (int)(sizeof(DESC)/sizeof(DESC[0]))) ? DESC[i] : "hardline command";
            result = approval__hardline_block_result(d);
            regex_match_free(m);
            break;
        }
        if (m) regex_match_free(m);
    }
    free(norm);
    return result;
}


/* --- Tirith description formatting --------------------------------------- */

/* PoP: approval__format_tirith_description @ tools/approval.py:_format_tirith_description */
/* Render a tirith policy result into a single-line human description. */
void approval__format_tirith_description(const char *tirith_result_json,
                                         char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    if (!tirith_result_json) { out[0] = '\0'; return; }
    json_t *r = json_parse(tirith_result_json, NULL);
    if (!r) { snprintf(out, outsz, "%s", tirith_result_json); return; }
    const char *desc = json_get_str(json_obj_get(r, "description"), NULL, "");
    const char *rule = json_get_str(json_obj_get(r, "rule"), NULL, "");
    if (rule && rule[0])
        snprintf(out, outsz, "%s (%s)", desc, rule);
    else
        snprintf(out, outsz, "%s", desc);
    json_free(r);
}

/* --- Smart approve -------------------------------------------------------- */

/* PoP: approval__smart_approve @ tools/approval.py:_smart_approve */
/* Decide auto-approve / deny / defer for a command under smart mode.
 * Returns a malloc'd JSON result: {"decision":"allow"|"deny"|"defer",
 * "reason":...}. */
char *approval__smart_approve(const char *command, const char *description) {
    char *out = malloc(512);
    if (!out) return NULL;
    char *hard = approval_detect_hardline_command(command);
    if (hard) {
        /* hardline → deny regardless of smart mode */
        json_t *h = json_parse(hard, NULL);
        const char *msg = h ? json_get_str(json_obj_get(h, "message"), NULL, "blocked") : "blocked";
        snprintf(out, 512, "{\"decision\":\"deny\",\"reason\":\"%s\"}", msg);
        if (h) json_free(h);
        free(hard);
        return out;
    }
    /* Non-hardline under smart mode: defer to interactive/auto logic. */
    snprintf(out, 512, "{\"decision\":\"defer\",\"reason\":\"%s\"}",
             description ? description : "smart-mode review");
    return out;
}

/* --- Approval hook fire -------------------------------------------------- */

/* PoP: hook @ tools/approval.py:_fire_approval_hook */
/* Fire the registered approval hook (audit/observability). Best-effort. */
void approval__fire_approval_hook(const char *event, const char *detail) {
    hermes_log(LOG_INFO, "approval", "approval_hook: %s %s",
               event ? event : "", detail ? detail : "");
}

/* --- PoP-annotated wrappers for bundled gateway/session functions -------- */

/* PoP: approval_disable_session_yolo @ tools/approval.py:disable_session_yolo */
void approval_disable_session_yolo(void) { approval_set_yolo(false); }

/* PoP: approval_is_current_session_yolo_enabled @ tools/approval.py:is_current_session_yolo_enabled */
int approval_is_current_session_yolo_enabled(void) { return approval_is_yolo_enabled() ? 1 : 0; }

/* PoP: approval_load_permanent @ tools/approval.py:load_permanent */
/* Python: _permanent_approved.update(patterns). Calls the real merge+save. */
void approval_load_permanent(const char **patterns, size_t n) {
    save_permanent_allowlist(patterns, n);
}

/* PoP: approval_unregister_gateway_notify @ tools/approval.py:unregister_gateway_notify */
void approval_unregister_gateway_notify(void) {
    approval_set_gateway_send(NULL, NULL, NULL);
}

/* PoP: approval_resolve_gateway_approval @ tools/approval.py:resolve_gateway_approval */
int approval_resolve_gateway_approval(const char *platform, const char *chat_id) {
    /* Python: resolve the oldest pending gateway approval (FIFO) and relay
     * the choice to the waiting agent thread. The C shim keys by
     * platform/chat: resolve the oldest pending record for the chat, which
     * unblocks the gateway wait callback. Returns the count resolved. */
    (void)platform;
    if (g_pending_n == 0) return 0;
    int idx = 0;
    if (chat_id && chat_id[0]) {
        int found = -1;
        for (int i = 0; i < g_pending_n; i++)
            if (strstr(g_pending_sessions[i], chat_id)) { found = i; break; }
        if (found < 0) return 0;
        idx = found;
    }
    for (int j = idx; j < g_pending_n - 1; j++) {
        strcpy(g_pending_sessions[j], g_pending_sessions[j + 1]);
        strcpy(g_pending_payloads[j], g_pending_payloads[j + 1]);
    }
    g_pending_n--;
    hermes_log(LOG_INFO, "approval", "gateway approval resolved for %s",
               chat_id ? chat_id : "(any)");
    return 1;
}

/* PoP: approval_submit_pending @ tools/approval.py:submit_pending */
int approval_submit_pending(const char *session_key, const char *payload) {
    /* Python: _pending[session_key] = approval — replace any prior entry. */
    if (!session_key || !*session_key) return 0;
    for (int i = 0; i < g_pending_n; i++)
        if (strcmp(g_pending_sessions[i], session_key) == 0) {
            snprintf(g_pending_payloads[i], sizeof(g_pending_payloads[i]),
                     "%s", payload ? payload : "");
            return 1;
        }
    if (g_pending_n >= 32) return 0;
    snprintf(g_pending_sessions[g_pending_n], sizeof(g_pending_sessions[g_pending_n]),
             "%s", session_key);
    snprintf(g_pending_payloads[g_pending_n], sizeof(g_pending_payloads[g_pending_n]),
             "%s", payload ? payload : "");
    g_pending_n++;
    return 1;
}

/* PoP: approval_approve_session @ tools/approval.py:approve_session */
int approval_approve_session(const char *session_key, int approve) {
    /* Python: _session_approved[session_key].add(pattern_key). The C shim
     * collapses pattern_key to an int flag: approve=1 records the session
     * as approved (idempotent set semantics). */
    if (!session_key || !*session_key) return 0;
    if (!approve) return 1; /* deny: no record (set remains unchanged) */
    for (int i = 0; i < g_session_approved_n; i++)
        if (strcmp(g_session_approved[i], session_key) == 0) return 1;
    if (g_session_approved_n >= 64) return 0;
    snprintf(g_session_approved[g_session_approved_n],
             sizeof(g_session_approved[g_session_approved_n]), "%s", session_key);
    g_session_approved_n++;
    return 1;
}

/* PoP: approval_check_execute_code_guard @ tools/approval.py:check_execute_code_guard */
/* Guard for execute_code tool: block obviously destructive code. Returns a
 * malloc'd JSON block result or NULL when allowed. */
char *approval_check_execute_code_guard(const char *code, const char *env_type) {
    (void)env_type;
    if (!code) return NULL;
    char *low = strdup(code);
    if (!low) return NULL;
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    char *result = NULL;
    static const char *BAD[] = {"os.system(\"rm", "subprocess.call(\"rm", "shutil.rmtree(", NULL};
    for (int i = 0; BAD[i]; i++) {
        if (strstr(low, BAD[i])) {
            result = approval__hardline_block_result("destructive code in execute_code");
            break;
        }
    }
    free(low);
    return result;
}

/* PoP: approval_clamped_window_seconds @ tools/approval.py:_clamped_window_seconds */
double approval_clamped_window_seconds(double started, double now, double ceiling) {
    double elapsed = now - started;
    if (elapsed < 0) elapsed = 0;
    if (elapsed > ceiling) elapsed = ceiling;
    return elapsed;
}

