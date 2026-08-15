/**
 * port_self_repo_guard.c — Port of Python: tools/self_repo_guard.py
 *
 * Detect Git operations that can rewrite the checkout backing this process.
 * Faithful C11 port. Reuses the shell-parsing helpers already ported in
 * port_approval.c (read_shell_word, deobfuscate_shell_word_for_detection,
 * iter_shell_command_starts) and implements the self-contained Bash option
 * grammar locally.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "self_repo_guard.h"

/* Reuse from port_approval.c (non-static, not in a shared header). */
size_t read_shell_word(const char *s, size_t start, size_t len, char *out, size_t out_cap);
char *deobfuscate_shell_word_for_detection(const char *word);
size_t iter_shell_command_starts(const char *command, size_t *out_starts, size_t max_starts);
char *strip_shell_word_syntax(const char *word);

/* forward decl */
char *srg_find_mutation(const char *command, const char *cwd, const char *root, int depth);

/* ────────────── constants ────────────── */

static const char *const srg_worktree_mutations[] = {
    "checkout", "switch", "rebase", "merge", "pull", "restore", "clean",
    "cherry-pick", "revert", "bisect", NULL};
static const char *const srg_worktree_target_actions[] = {"move", "remove", NULL};
static const char *const srg_stash_safe_actions[] = {"list", "show", "create", "store", "drop", "clear", NULL};
static const char *const srg_reset_worktree_modes[] = {"--hard", "--merge", "--keep", NULL};

static const char *const srg_known_git_builtins[] = {
    "add", "am", "apply", "blame", "branch", "bundle", "cat-file", "clean",
    "clone", "commit", "config", "describe", "diff", "fetch", "format-patch",
    "grep", "help", "init", "log", "ls-files", "ls-remote", "ls-tree",
    "maintenance", "merge-base", "mv", "notes", "push", "range-diff", "reflog",
    "remote", "repack", "replace", "reset", "restore", "rev-list", "rev-parse",
    "rm", "shortlog", "show", "show-ref", "stash", "status", "submodule", "tag",
    "worktree", NULL};

static const char *const srg_shell_executables[] = {"bash", "dash", "ksh", "sh", "zsh", NULL};

static const char *const srg_sudo_options_with_arg[] = {
    "-C", "--chdir", "-c", "--close-from", "-g", "--group", "-h", "--host",
    "-p", "--prompt", "-R", "--chroot", "-T", "--command-timeout", "-u",
    "--user", NULL};

static const char *const srg_env_options_with_arg[] = {
    "-a", "--argv0", "-C", "--chdir", "-S", "--split-string", "-u", "--unset", NULL};

static const char *const srg_bash_options_with_arg[] = {
    "-O", "+O", "-o", "+o", "--init-file", "--rcfile", NULL};

static const char *const srg_shell_short_option_letters = "ilrsDcabefhkmnptuvxBCEHPTOo";

/* exec wrapper option sets. */
static const char *const srg_exec_opts[] = {"-a", NULL};
static const char *const srg_time_opts[] = {"-f", "--format", "-o", "--output", NULL};
static const char *const srg_simple_wrappers[] = {"builtin", "exec", "nohup", "setsid", "time", NULL};

#define SRG_MAX_RECURSION 4

/* ────────────── small list helpers ────────────── */

static bool srg_str_in(const char *const *list, const char *value) {
    if (!value) return false;
    for (int i = 0; list[i]; i++)
        if (strcmp(list[i], value) == 0) return true;
    return false;
}

/* _ASSIGNMENT_RE: [A-Za-z_][A-Za-z0-9_]*=(.*) fullmatch */
static bool srg_is_assignment(const char *word) {
    if (!word || !*word) return false;
    if (!(isalpha((unsigned char)word[0]) || word[0] == '_')) return false;
    const char *eq = strchr(word, '=');
    if (!eq) return false;
    for (const char *p = word; p < eq; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) return false;
    }
    return true;
}

/* ── get_running_source_root ──
 * Return the source checkout backing this process (malloc'd) or NULL. */
char *srg_get_running_source_root(void) {
    /* Equivalent of Path(__file__).resolve().parent.parent: the repo root is
     * two levels up from src/tools/. We hardcode the build-time layout. */
    const char *candidate = SRG_REPO_ROOT;
    if (!candidate) return NULL;
    char gitpath[2048];
    snprintf(gitpath, sizeof gitpath, "%s/.git", candidate);
    if (access(gitpath, F_OK) == 0)
        return strdup(candidate);
    return NULL;
}

/* ── _resolve(path_str, base) ──
 * expanduser + (base if relative) + resolve. malloc'd path. */
char *srg_resolve(const char *path_str, const char *base) {
    if (!path_str) return NULL;
    /* expanduser ~ */
    char *expanded = NULL;
    if (path_str[0] == '~') {
        const char *home = getenv("HOME");
        if (home && path_str[1] == '/' ) {
            size_t n = strlen(home) + strlen(path_str + 1) + 1;
            expanded = malloc(n);
            if (expanded) { snprintf(expanded, n, "%s%s", home, path_str + 1); }
        } else if (home && path_str[1] == '\0') {
            expanded = strdup(home);
        }
    }
    if (!expanded) expanded = strdup(path_str);
    if (!expanded) return NULL;

    char *joined = NULL;
    if (expanded[0] != '/') {
        /* relative: base / path */
        size_t bl = base ? strlen(base) : 0;
        size_t nl = strlen(expanded);
        joined = malloc(bl + 1 + nl + 1);
        if (joined) {
            memcpy(joined, base ? base : "/", bl);
            joined[bl] = '/';
            memcpy(joined + bl + 1, expanded, nl + 1);
        }
        free(expanded);
    } else {
        joined = expanded;
    }
    if (!joined) return NULL;

    /* resolve() — best-effort realpath; on failure return joined. */
    char *resolved = realpath(joined, NULL);
    if (resolved) { free(joined); return resolved; }
    return joined;
}

/* ── _is_within(path, root) ── */
bool srg_is_within(const char *path, const char *root) {
    if (!path || !root) return false;
    if (strcmp(path, root) == 0) return true;
    size_t rl = strlen(root);
    if (strncmp(path, root, rl) != 0) return false;
    return path[rl] == '/';
}

/* ── _executable_name(value) ──
 * basename, \ -> /, strip .exe, lowercase. malloc'd. */
char *srg_executable_name(const char *value) {
    if (!value) return strdup("");
    /* replace backslashes with slashes */
    char *normalized = strdup(value);
    if (!normalized) return NULL;
    for (char *p = normalized; *p; p++) if (*p == '\\') *p = '/';
    const char *base = strrchr(normalized, '/');
    const char *name = base ? base + 1 : normalized;
    /* remove .exe suffix */
    size_t nl = strlen(name);
    size_t keep = nl;
    if (nl > 4 && strcmp(name + nl - 4, ".exe") == 0) keep = nl - 4;
    char *out = malloc(keep + 1);
    if (!out) { free(normalized); return NULL; }
    for (size_t i = 0; i < keep; i++) out[i] = (char)tolower((unsigned char)name[i]);
    out[keep] = '\0';
    free(normalized);
    return out;
}

/* ── _shell_words_at(command, start) ──
 * Read up to 64 words; stop at newline between words. Returns malloc'd
 * NULL-terminated array of malloc'd strings. Faithful to Python
 * _read_shell_word + _shell_words_at (skips whitespace, tracks quotes,
 * $(), ${}, backticks; breaks on whitespace/;&| outside quotes). */
static size_t srg_read_shell_word_pos(const char *command, size_t len, size_t pos,
                                      size_t *out_start, size_t *out_end) {
    /* skip whitespace */
    while (pos < len && (command[pos]==' '||command[pos]=='\t'||command[pos]=='\n'||command[pos]=='\r')) pos++;
    size_t start = pos;
    size_t i = start;
    char quote = 0;
    while (i < len) {
        char ch = command[i];
        if (quote) {
            if (ch == '\\' && quote == '"' && i + 1 < len) { i += 2; continue; }
            if (ch == quote) quote = 0;
            i += 1;
            continue;
        }
        if (ch == '\'' || ch == '"') { quote = ch; i += 1; continue; }
        if (ch == '\\' && i + 1 < len) { i += 2; continue; }
        if (i + 1 < len && command[i]=='$' && command[i+1]=='(') {
            /* scan $(...) — naive nesting count */
            int depth = 0;
            size_t j = i;
            for (; j < len; j++) {
                if (command[j]=='(') depth++;
                else if (command[j]==')') { depth--; if (depth==0) { j++; break; } }
            }
            i = j;
            continue;
        }
        if (i + 1 < len && command[i]=='$' && command[i+1]=='{') {
            const char *brace = strchr(command + i + 2, '}');
            if (brace) i = (size_t)(brace - command) + 1;
            else i += 2;
            continue;
        }
        if (ch == '`') {
            const char *b = strchr(command + i + 1, '`');
            if (b) i = (size_t)(b - command) + 1;
            else i += 1;
            continue;
        }
        if (ch==' '||ch=='\t'||ch=='\n'||ch=='\r'||ch==';'||ch=='&'||ch=='|') break;
        i += 1;
    }
    *out_start = start;
    *out_end = i;
    return i;
}

char **srg_shell_words_at(const char *command, size_t start) {
    char **words = calloc(65, sizeof(char *));
    if (!words) return NULL;
    size_t len = strlen(command);
    int count = 0;
    size_t cursor = start;
    for (int i = 0; i < 64; i++) {
        size_t ws, we;
        srg_read_shell_word_pos(command, len, cursor, &ws, &we);
        if (ws >= we) break;  /* no word */
        /* newline between previous word and this one -> stop (don't append) */
        if (count > 0) {
            for (size_t k = cursor; k < ws; k++)
                if (command[k] == '\n') return words;
        }
        char *raw = malloc(we - ws + 1);
        if (!raw) break;
        memcpy(raw, command + ws, we - ws); raw[we - ws] = '\0';
        char *decoded = deobfuscate_shell_word_for_detection(raw);
        char *stripped = strip_shell_word_syntax(decoded ? decoded : raw);
        words[count++] = stripped ? stripped : strdup(decoded ? decoded : raw);
        free(decoded); free(raw);
        if (stripped == NULL) words[count-1] = NULL; /* alloc failure */
        cursor = we;
    }
    return words;
}

/* ── _consume_options(words, start, options_with_arg) ── */
size_t srg_consume_options(char **words, size_t start, const char *const *options_with_arg) {
    size_t index = start;
    while (words && words[index]) {
        const char *option = words[index];
        if (strcmp(option, "--") == 0) return index + 1;
        if (option[0] != '-' || strcmp(option, "-") == 0) break;
        char *eq = strchr(option, '=');
        size_t option_name_len = eq ? (size_t)(eq - option) : strlen(option);
        char optname[256];
        size_t onl = option_name_len < sizeof(optname) - 1 ? option_name_len : sizeof(optname) - 1;
        memcpy(optname, option, onl); optname[onl] = '\0';
        if (!eq && srg_str_in(options_with_arg, optname))
            index += 2;
        else
            index += 1;
    }
    return index;
}

/* ── _command_parts(words) ──
 * Returns env (as a simple list), executable, args. We model env as a
 * malloc'd list "name\0value\0\n" packed for simplicity, plus out_executable
 * (malloc'd) and out_args (malloc'd NULL-terminated array). */
void srg_command_parts(char **words, char **out_env_packed, char **out_executable, char ***out_args) {
    char *env_packed = malloc(1); if (env_packed) env_packed[0] = '\0';
    size_t env_len = 0, env_cap = 0;
    size_t index = 0;
    *out_executable = NULL;
    *out_args = NULL;

    while (words && words[index]) {
        if (srg_is_assignment(words[index])) {
            char *eq = strchr(words[index], '=');
            *eq = '\0';
            const char *name = words[index];
            const char *value = eq + 1;
            size_t add = strlen(name) + 1 + strlen(value) + 1 + 1;
            if (env_len + add > env_cap) {
                env_cap = (env_cap ? env_cap * 2 : 256) + add;
                env_packed = realloc(env_packed, env_cap);
            }
            if (env_packed) {
                memcpy(env_packed + env_len, name, strlen(name) + 1);
                env_len += strlen(name) + 1;
                memcpy(env_packed + env_len, value, strlen(value) + 1);
                env_len += strlen(value) + 1;
                env_packed[env_len++] = '\n';
            }
            index++;
            continue;
        }

        char *exec = srg_executable_name(words[index]);
        if (exec && strcmp(exec, "sudo") == 0) {
            index = srg_consume_options(words, index + 1, srg_sudo_options_with_arg);
            free(exec);
            continue;
        }
        if (exec && strcmp(exec, "env") == 0) {
            index = srg_consume_options(words, index + 1, srg_env_options_with_arg);
            free(exec);
            continue;
        }
        if (exec && strcmp(exec, "command") == 0) {
            if (words[index + 1] && (strcmp(words[index + 1], "-v") == 0 || strcmp(words[index + 1], "-V") == 0)) {
                free(exec);
                *out_env_packed = env_packed;
                return;  /* env, None, [] */
            }
            index = srg_consume_options(words, index + 1, NULL);
            free(exec);
            continue;
        }
        if (exec && srg_str_in(srg_simple_wrappers, exec)) {
            const char *const *opts = NULL;
            if (strcmp(exec, "exec") == 0) opts = srg_exec_opts;
            else if (strcmp(exec, "time") == 0) opts = srg_time_opts;
            index = srg_consume_options(words, index + 1, opts);
            free(exec);
            continue;
        }
        /* return env, words[index], words[index+1:] */
        *out_executable = exec;  /* already normalized/lowercased */
        int n = 0; while (words[index + 1 + n]) n++;
        char **args = calloc(n + 1, sizeof(char *));
        if (args) {
            for (int i = 0; i < n; i++) args[i] = strdup(words[index + 1 + i]);
            args[n] = NULL;
        }
        *out_args = args;
        *out_env_packed = env_packed;
        return;
    }
    *out_env_packed = env_packed;
    return;  /* env, None, [] */
}

/* ── _scope_keys(command, starts) ──
 * Returns an array scopes[count] of "opener stack" for each start. We model
 * the opener stack as a small fixed array of ints. */
#define SRG_MAX_CONTEXT 32
static void srg_scope_keys(const char *command, const size_t *starts, size_t nstarts,
                           size_t (*out_scopes)[SRG_MAX_CONTEXT], size_t *out_counts) {
    /* contexts: array of {kind, opener}; kinds: 0=root,1='(',2=$(),3=` ,4=" */
    struct { int kind; int opener; char quote; } contexts[SRG_MAX_CONTEXT];
    int nctx = 1;
    contexts[0].kind = 0; contexts[0].opener = -1; contexts[0].quote = 0;
    size_t len = strlen(command);
    size_t cursor = 0;

    for (size_t si = 0; si < nstarts; si++) {
        size_t start = starts[si];
        while (cursor < start) {
            int ctx = nctx - 1;
            char quote = contexts[ctx].quote;
            char c = command[cursor];
            if (quote == '\'') {
                if (c == '\'') contexts[ctx].quote = 0;
                cursor++;
                continue;
            }
            if (quote == '"') {
                if (c == '\\' && cursor + 1 < start) { cursor += 2; continue; }
                if (c == '"') { contexts[ctx].quote = 0; cursor++; continue; }
                if (strncmp(command + cursor, "$(", 2) == 0) {
                    if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 2; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                    cursor += 2; continue;
                }
                if (c == '`') {
                    if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 3; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                }
                cursor++;
                continue;
            }
            if (c == '\'' || c == '"') { contexts[ctx].quote = c; cursor++; continue; }
            if (c == '\\' && cursor + 1 < start) { cursor += 2; continue; }
            if (strncmp(command + cursor, "$(", 2) == 0) {
                if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 2; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                cursor += 2; continue;
            }
            if (c == '(') {
                if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 1; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                cursor++; continue;
            }
            if (c == ')' && nctx > 1 && (contexts[nctx-1].kind == 1 || contexts[nctx-1].kind == 2)) {
                nctx--;
                cursor++; continue;
            }
            if (c == '`') {
                if (nctx > 1 && contexts[nctx-1].kind == 3) nctx--;
                else if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 3; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
            }
            cursor++;
        }
        /* record openers of contexts[1:] */
        size_t cnt = (size_t)(nctx - 1);
        for (size_t i = 0; i < cnt && i < SRG_MAX_CONTEXT; i++) out_scopes[si][i] = (size_t)contexts[i+1].opener;
        out_counts[si] = cnt;
    }
}

/* ── _operator_before(command, start) ──
 * Returns the operator char before `start`, or "\n"/NULL. out is a caller
 * buffer of at least 4 bytes. */
static void srg_operator_before(const char *command, size_t start, char *out) {
    out[0] = '\0';
    if (!command) return;
    long index = (long)start - 1;
    bool saw_newline = false;
    while (index >= 0 && isspace((unsigned char)command[index])) {
        if (command[index] == '\n') saw_newline = true;
        index--;
    }
    if (index < 0) {
        if (saw_newline) strcpy(out, "\n");
        return;
    }
    if (index > 0 && command[index-1] == '&' && command[index] == '&') {
        strcpy(out, "&&");
        return;
    }
    if (index > 0 && command[index-1] == '|' && command[index] == '|') {
        strcpy(out, "||");
        return;
    }
    if (command[index] == ';' || command[index] == '|' || command[index] == '&' ||
        command[index] == '(' || command[index] == '{') {
        out[0] = command[index]; out[1] = '\0';
        return;
    }
    if (saw_newline) strcpy(out, "\n");
}

/* ── _cd_target(executable, args, cwd) ──
 * For cd/pushd, return resolved target dir or NULL. */
char *srg_cd_target(const char *executable, char **args, const char *cwd) {
    char *ex = srg_executable_name(executable);
    bool is_cd = ex && (strcmp(ex, "cd") == 0 || strcmp(ex, "pushd") == 0);
    free(ex);
    if (!is_cd) return NULL;
    size_t idx = srg_consume_options(args, 0, NULL);
    if (!args || !args[idx] || strcmp(args[idx], "-") == 0) return NULL;
    char *resolved = srg_resolve(args[idx], cwd);
    if (!resolved) return NULL;
    if (access(resolved, X_OK) == 0 || access(resolved, F_OK) == 0) {
        /* Python uses target.is_dir() */
        struct stat st;
        if (stat(resolved, &st) == 0 && S_ISDIR(st.st_mode)) return resolved;
    }
    free(resolved);
    return NULL;
}

/* ── _bash_exec_payload(args) — faithful bash -c grammar.
 * Returns 1 and out_payload (malloc'd or NULL) when -c present. */
static bool srg_bash_exec_payload(char **args, char **out_payload) {
    *out_payload = NULL;
    size_t index = 0;
    while (args && args[index]) {
        const char *token = args[index];
        if (strcmp(token, "--") == 0 || (token[0] != '-' && token[0] != '+')) break;
        if (srg_str_in(srg_bash_options_with_arg, token)) { index += 2; continue; }
        if (strncmp(token, "--", 2) == 0) { index += 1; continue; }
        const char *chars = token + 1;
        bool valid = true;
        for (const char *p = chars; *p; p++)
            if (!strchr(srg_shell_short_option_letters, *p)) { valid = false; break; }
        if (!valid) { index += 1; continue; }
        bool consumed_arg = strchr(chars, 'O') != NULL || strchr(chars, 'o') != NULL;
        if (!strchr(chars, 'c')) { index += 1 + (int)consumed_arg; continue; }
        size_t payload_index = index + 1 + (int)consumed_arg;
        if (args[payload_index]) *out_payload = strdup(args[payload_index]);
        return true;
    }
    return false;
}

/* ── _shell_script_arg(args) ──
 * Return script owned by shell -c (malloc'd), or NULL. */
char *srg_shell_script_arg(char **args) {
    char *payload = NULL;
    if (srg_bash_exec_payload(args, &payload)) return payload;
    free(payload);
    for (size_t i = 0; args && args[i]; i++) {
        if (strcmp(args[i], "--") == 0) break;
        if (args[i][0] == '-' && strchr(args[i] + 1, 'c')) {
            if (args[i+1]) return strdup(args[i+1]);
            return NULL;
        }
        if (args[i][0] != '-') break;
    }
    return NULL;
}

/* ── _heredoc_specs(line) — parse << delimiters on one line.
 * Returns malloc'd array of srg_heredoc_t, NULL-terminated. */
srg_heredoc_t **srg_heredoc_specs(const char *line) {
    srg_heredoc_t **specs = calloc(8, sizeof(srg_heredoc_t *));
    if (!specs) return NULL;
    int n = 0;
    char quote = 0;
    size_t len = line ? strlen(line) : 0;
    size_t index = 0;

    while (index < len) {
        char c = line[index];
        if (quote) {
            if (c == '\\' && quote == '"' && index + 1 < len) { index += 2; continue; }
            if (c == quote) quote = 0;
            index++;
            continue;
        }
        if (c == '\'' || c == '"') { quote = c; index++; continue; }
        if (strncmp(line + index, "<<", 2) != 0 || strncmp(line + index, "<<<", 3) == 0) { index++; continue; }

        size_t operator_at = index;
        index += 2;
        bool strip_tabs = index < len && line[index] == '-';
        if (strip_tabs) index++;
        while (index < len && (line[index] == ' ' || line[index] == '\t')) index++;
        if (index >= len) break;

        char delimiter_quote = (line[index] == '\'' || line[index] == '"') ? line[index] : 0;
        char *delimiter = NULL;
        if (delimiter_quote) {
            index++;
            const char *end = strchr(line + index, delimiter_quote);
            if (!end) break;
            size_t dl = (size_t)(end - (line + index));
            delimiter = malloc(dl + 1);
            if (delimiter) { memcpy(delimiter, line + index, dl); delimiter[dl] = '\0'; }
            index = (size_t)(end - line) + 1;
        } else {
            size_t end = index;
            while (end < len && !isspace((unsigned char)line[end]) &&
                   !strchr(";|&<>", line[end])) end++;
            size_t dl = end - index;
            delimiter = malloc(dl + 1);
            if (delimiter) { memcpy(delimiter, line + index, dl); delimiter[dl] = '\0'; }
            index = end;
        }
        if (!delimiter || !*delimiter) { free(delimiter); continue; }

        /* header = line[:operator_at]; find last command start, shell exec check */
        char *header = malloc(operator_at + 1);
        if (header) { memcpy(header, line, operator_at); header[operator_at] = '\0'; }
        bool execute_as_shell = false;
        if (header) {
            size_t starts[16]; size_t nstarts = iter_shell_command_starts(header, starts, 16);
            char **words = NULL;
            if (nstarts > 0) words = srg_shell_words_at(header, starts[nstarts-1]);
            char *envp = NULL, *executable = NULL; char **args = NULL;
            if (words) srg_command_parts(words, &envp, &executable, &args);
            if (executable) {
                char *ex = srg_executable_name(executable);
                bool is_shell = ex && srg_str_in(srg_shell_executables, ex);
                if (is_shell && srg_shell_script_arg(args) == NULL) {
                    bool has_pos = false;
                    for (int i = 0; args && args[i]; i++)
                        if (args[i][0] && args[i][0] != '-') { has_pos = true; break; }
                    execute_as_shell = !has_pos;
                }
                free(ex);
            }
            free(envp);
            free(executable);
            if (args) { for (int i = 0; args[i]; i++) free(args[i]); free(args); }
            if (words) { for (int i = 0; words[i]; i++) free(words[i]); free(words); }
            free(header);
        } else free(header);

        if (n < 7) {
            srg_heredoc_t *hd = calloc(1, sizeof(srg_heredoc_t));
            if (hd) {
                hd->delimiter = delimiter; delimiter = NULL;
                hd->strip_tabs = strip_tabs;
                hd->execute_as_shell = execute_as_shell;
                specs[n++] = hd;
            }
        }
        free(delimiter);
    }
    return specs;
}

/* ── _masked_line(line) — mask all but \r \n. malloc'd. */
char *srg_masked_line(const char *line) {
    if (!line) return NULL;
    size_t len = strlen(line);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) out[i] = (line[i] == '\r' || line[i] == '\n') ? line[i] : ' ';
    out[len] = '\0';
    return out;
}

/* ── _mask_heredocs(command) — mask heredoc bodies; collect shell scripts.
 * out_masked (malloc'd), out_scripts (malloc'd NULL-terminated array). */
void srg_mask_heredocs(const char *command, char **out_masked, char ***out_scripts) {
    char *masked_buf = NULL; size_t mlen = 0, mcap = 0;
    char **scripts = calloc(8, sizeof(char *)); int nscripts = 0;
    srg_heredoc_t *pending[8]; int npending = 0;

    const char *line_start = command;
    const char *scan = command;
    while (scan) {
        const char *nl = strchr(scan, '\n');
        size_t linelen = nl ? (size_t)(nl - line_start) + 1 : strlen(line_start);
        char *line = malloc(linelen + 1);
        if (line) { memcpy(line, line_start, linelen); line[linelen] = '\0'; }

        char *masked;
        if (npending > 0) {
            /* collect body, check delimiter */
            char *candidate = strdup(line);
            size_t cl = strlen(candidate);
            if (cl > 0 && (candidate[cl-1] == '\n' || candidate[cl-1] == '\r')) candidate[cl-1] = '\0';
            if (cl > 0 && (candidate[cl-1] == '\n' || candidate[cl-1] == '\r')) candidate[cl-1] = '\0';
            if (pending[0]->strip_tabs) { /* lstrip tabs */ }
            if (strcmp(candidate, pending[0]->delimiter) == 0) {
                if (pending[0]->execute_as_shell && nscripts < 63) {
                    /* join body lines */
                    size_t bl = pending[0]->body_len;
                    char *bs = malloc(bl + 1);
                    if (bs) { memcpy(bs, pending[0]->body, bl); bs[bl] = '\0'; }
                    if (bs) { scripts[nscripts++] = bs; scripts = realloc(scripts, (size_t)(nscripts+1)*sizeof(char*)); scripts[nscripts]=NULL; }
                }
                free(pending[0]->body);
                free(pending[0]->delimiter);
                free(pending[0]);
                npending--;
            } else {
                size_t bl = strlen(line);
                pending[0]->body = realloc(pending[0]->body, pending[0]->body_len + bl + 1);
                if (pending[0]->body) { memcpy(pending[0]->body + pending[0]->body_len, line, bl); pending[0]->body_len += bl; pending[0]->body[pending[0]->body_len] = '\0'; }
            }
            masked = srg_masked_line(line);
            free(candidate);
        } else {
            masked = strdup(line);
            /* parse new heredocs from this line */
            srg_heredoc_t **newspecs = srg_heredoc_specs(line);
            for (int i = 0; newspecs && newspecs[i] && npending < 8; i++) {
                pending[npending++] = newspecs[i];
                newspecs[i] = NULL;
            }
            if (newspecs) { for (int i = 0; newspecs[i]; i++) { free(newspecs[i]->delimiter); free(newspecs[i]->body); free(newspecs[i]); } free(newspecs); }
        }

        size_t ml = strlen(masked);
        if (mlen + ml + 1 > mcap) { mcap = (mcap ? mcap*2 : 256) + ml + 1; masked_buf = realloc(masked_buf, mcap); }
        if (masked_buf) { memcpy(masked_buf + mlen, masked, ml); mlen += ml; masked_buf[mlen] = '\0'; }
        free(masked);
        free(line);

        if (!nl) break;
        line_start = nl + 1;
        scan = line_start;
    }
    /* flush pending heredoc bodies at EOF */
    for (int i = 0; i < npending; i++) {
        if (pending[i]->execute_as_shell && nscripts < 63) {
            char *bs = malloc(pending[i]->body_len + 1);
            if (bs) { memcpy(bs, pending[i]->body, pending[i]->body_len); bs[pending[i]->body_len] = '\0'; scripts[nscripts++] = bs; scripts = realloc(scripts, (size_t)(nscripts+1)*sizeof(char*)); scripts[nscripts]=NULL; }
        }
        free(pending[i]->delimiter); free(pending[i]->body); free(pending[i]);
    }
    if (!masked_buf) { masked_buf = strdup(""); }
    if (!scripts) scripts = calloc(1, sizeof(char *));
    if (scripts) scripts[nscripts] = NULL;
    *out_masked = masked_buf;
    *out_scripts = scripts;
}

/* ── _git_target_and_subcommand(args, current_dir, env) ──
 * Returns target (malloc'd), subcommand (malloc'd or NULL), sub_args
 * (malloc'd NULL-terminated), and a packed inline-aliases list. */
void srg_git_target_and_subcommand(char **args, const char *current_dir, const char *env_packed,
                                   char **out_target, char **out_subcommand, char ***out_sub_args,
                                   char **out_inline_aliases) {
    char *target = strdup(current_dir ? current_dir : "/");
    char *work_tree = NULL;
    /* inline aliases packed as name\0value\0\n */
    char *aliases = malloc(1); if (aliases) aliases[0] = '\0';
    size_t alen = 0, acap = 0;
    size_t index = 0;

    while (args && args[index]) {
        const char *arg = args[index];
        if (strcmp(arg, "--") == 0) { index++; break; }
        if (strcmp(arg, "-C") == 0 && args[index+1]) {
            char *r = srg_resolve(args[index+1], target);
            if (r) { free(target); target = r; }
            index += 2; continue;
        }
        if (strncmp(arg, "-C", 2) == 0 && strlen(arg) > 2) {
            char *r = srg_resolve(arg + 2, target);
            if (r) { free(target); target = r; }
            index += 1; continue;
        }
        if (strcmp(arg, "--work-tree") == 0 || strcmp(arg, "--git-dir") == 0 ||
            strcmp(arg, "--namespace") == 0 || strcmp(arg, "--exec-path") == 0) {
            if (strcmp(arg, "--work-tree") == 0 && args[index+1]) {
                free(work_tree); work_tree = strdup(args[index+1]);
            }
            index += 2; continue;
        }
        if (strncmp(arg, "--work-tree=", 12) == 0) {
            free(work_tree); work_tree = strdup(arg + 12);
            index += 1; continue;
        }
        if (strcmp(arg, "-c") == 0 && args[index+1]) {
            const char *config = args[index+1];
            if (strncmp(config, "alias.", 6) == 0 && strchr(config, '=')) {
                char *eq = strchr(config, '=');
                *eq = '\0';
                const char *key = config + 6;
                const char *value = eq + 1;
                size_t add = strlen(key) + 1 + strlen(value) + 1 + 1;
                if (alen + add > acap) { acap = (acap ? acap*2 : 256) + add; aliases = realloc(aliases, acap); }
                if (aliases) {
                    for (size_t i = 0; key[i]; i++) aliases[alen++] = (char)tolower((unsigned char)key[i]);
                    aliases[alen++] = '\0';
                    memcpy(aliases + alen, value, strlen(value) + 1); alen += strlen(value) + 1;
                    aliases[alen++] = '\n';
                }
            }
            index += 2; continue;
        }
        if (strncmp(arg, "-calias.", 8) == 0 && strchr(arg, '=')) {
            char *eq = strchr(arg, '=');
            *eq = '\0';
            const char *key = arg + 2 + 6;
            const char *value = eq + 1;
            size_t add = strlen(key) + 1 + strlen(value) + 1 + 1;
            if (alen + add > acap) { acap = (acap ? acap*2 : 256) + add; aliases = realloc(aliases, acap); }
            if (aliases) {
                for (size_t i = 0; key[i]; i++) aliases[alen++] = (char)tolower((unsigned char)key[i]);
                aliases[alen++] = '\0';
                memcpy(aliases + alen, value, strlen(value) + 1); alen += strlen(value) + 1;
                aliases[alen++] = '\n';
            }
            index += 1; continue;
        }
        if (arg[0] == '-') { index += 1; continue; }
        break;
    }

    /* explicit_work_tree = work_tree or env GIT_WORK_TREE */
    char *explicit = NULL;
    if (work_tree) explicit = strdup(work_tree);
    else {
        /* find GIT_WORK_TREE in env_packed */
        const char *scan = env_packed;
        while (scan && *scan) {
            const char *name = scan;
            const char *nul = strchr(scan, '\0');
            if (!nul) break;
            const char *val = nul + 1;
            const char *nl = strchr(val, '\n');
            size_t vlen = nl ? (size_t)(nl - val) : strlen(val);
            if (strcmp(name, "GIT_WORK_TREE") == 0) {
                explicit = malloc(vlen + 1);
                if (explicit) { memcpy(explicit, val, vlen); explicit[vlen] = '\0'; }
                break;
            }
            if (!nl) break;
            scan = nl + 1;
        }
    }
    if (explicit) {
        char *r = srg_resolve(explicit, target);
        if (r) { free(target); target = r; }
        free(explicit);
    }

    char *subcommand = NULL;
    if (args && args[index]) subcommand = strdup(args[index]);
    /* lower-case it */
    if (subcommand) for (char *p = subcommand; *p; p++) *p = (char)tolower((unsigned char)*p);

    int n = 0; while (args && args[index + 1 + n]) n++;
    char **sub_args = calloc(n + 1, sizeof(char *));
    if (sub_args) { for (int i = 0; i < n; i++) sub_args[i] = strdup(args[index + 1 + i]); sub_args[n] = NULL; }

    free(work_tree);
    *out_target = target;
    *out_subcommand = subcommand;
    *out_sub_args = sub_args;
    *out_inline_aliases = aliases;
}

/* ── _mutates_worktree(subcommand, args) ── */
bool srg_mutates_worktree(const char *subcommand, char **args) {
    if (!subcommand) return false;
    if (strcmp(subcommand, "reset") == 0) {
        for (int i = 0; args && args[i]; i++) {
            if (srg_str_in(srg_reset_worktree_modes, args[i])) return true;
            /* --hard abbreviated: --h(?:a(?:r(?:d)?)?)? */
            const char *a = args[i];
            if (strncmp(a, "--h", 3) == 0) {
                bool match = true;
                const char *rest = a + 3;
                if (*rest == 'a') { rest++; if (*rest == 'r') { rest++; if (*rest == 'd') rest++; } }
                if (*rest != '\0') match = false;
                if (match) return true;
            }
        }
        return false;
    }
    if (strcmp(subcommand, "stash") == 0) {
        const char *action = "push";
        for (int i = 0; args && args[i]; i++) {
            if (args[i][0] != '-') { action = args[i]; break; }
        }
        return !srg_str_in(srg_stash_safe_actions, action);
    }
    if (strcmp(subcommand, "clean") == 0) {
        bool dry_run = false;
        for (int i = 0; args && args[i]; i++) {
            if (strcmp(args[i], "--dry-run") == 0) { dry_run = true; break; }
            if (args[i][0] == '-' && args[i][1] != '-' && strchr(args[i]+1, 'n')) { dry_run = true; break; }
        }
        return !dry_run;
    }
    if (strcmp(subcommand, "restore") == 0) {
        bool staged = false, worktree = false;
        for (int i = 0; args && args[i]; i++) {
            if (strcmp(args[i], "--staged") == 0 || (args[i][0] == '-' && strchr(args[i]+1, 'S'))) staged = true;
            if (strcmp(args[i], "--worktree") == 0 || (args[i][0] == '-' && strchr(args[i]+1, 'W'))) worktree = true;
        }
        return worktree || !staged;
    }
    return srg_str_in(srg_worktree_mutations, subcommand);
}

/* ── _next_positional(args, index) ── */
size_t srg_next_positional(char **args, size_t index) {
    while (args && args[index]) {
        if (strcmp(args[index], "--") == 0) return index + 1;
        if (args[index][0] == '-' && strcmp(args[index], "-") != 0) { index++; continue; }
        return index;
    }
    return index;
}

/* ── _inspect_git_worktree(args, cwd, root) ── */
char *srg_inspect_git_worktree(char **args, const char *cwd, const char *root) {
    size_t action_index = srg_next_positional(args, 0);
    if (!args || !args[action_index]) return NULL;
    char *action = strdup(args[action_index]);
    for (char *p = action; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (!srg_str_in(srg_worktree_target_actions, action)) { free(action); return NULL; }
    size_t target_index = srg_next_positional(args, action_index + 1);
    if (!args[target_index]) { free(action); return NULL; }
    char *resolved = srg_resolve(args[target_index], cwd);
    bool hit = resolved && strcmp(resolved, root) == 0;
    free(resolved);
    free(action);
    if (hit) {
        char *out = malloc(32);
        if (out) snprintf(out, 32, "git worktree %s", args[action_index]);
        return out;
    }
    return NULL;
}

/* ── _read_git_alias(executable, target, alias) — subprocess, malloc'd. */
char *srg_read_git_alias(const char *executable, const char *target, const char *alias) {
    /* git -C <target> config --get alias.<alias> */
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s -C \"%s\" config --get alias.%s 2>/dev/null",
             executable, target, alias);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char buf[2048];
    buf[0] = '\0';
    if (fgets(buf, sizeof buf, fp) == NULL) { buf[0] = '\0'; }
    int rc = pclose(fp);
    if (rc != 0) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    if (buf[0] == '\0') return NULL;
    return strdup(buf);
}

/* ── _inspect_git(executable, args, current_dir, env, root, depth) ──
 * Returns operation string (malloc'd) or NULL. */
char *srg_inspect_git(const char *executable, char **args, const char *current_dir,
                      const char *env_packed, const char *root, int depth) {
    char *target, *subcommand; char **sub_args; char *inline_aliases;
    srg_git_target_and_subcommand(args, current_dir, env_packed, &target, &subcommand,
                                  &sub_args, &inline_aliases);
    if (!subcommand) { free(target); free(inline_aliases); free(sub_args); return NULL; }

    if (strcmp(subcommand, "worktree") == 0) {
        char *op = srg_inspect_git_worktree(sub_args, target, root);
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return op;
    }
    if (!srg_is_within(target, root)) {
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return NULL;
    }
    if (srg_mutates_worktree(subcommand, sub_args)) {
        char *out = malloc(strlen(subcommand) + 8);
        if (out) snprintf(out, strlen(subcommand) + 8, "git %s", subcommand);
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return out;
    }
    if (srg_str_in(srg_known_git_builtins, subcommand)) {
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return NULL;
    }
    if (depth >= SRG_MAX_RECURSION) {
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return NULL;
    }

    /* lookup alias in inline_aliases packed list */
    char *alias = NULL;
    {
        const char *scan = inline_aliases;
        while (scan && *scan) {
            const char *name = scan;
            const char *nul = strchr(scan, '\0');
            if (!nul) break;
            const char *val = nul + 1;
            const char *nl = strchr(val, '\n');
            size_t vlen = nl ? (size_t)(nl - val) : strlen(val);
            if (strcmp(name, subcommand) == 0) {
                alias = malloc(vlen + 1);
                if (alias) { memcpy(alias, val, vlen); alias[vlen] = '\0'; }
                break;
            }
            if (!nl) break;
            scan = nl + 1;
        }
    }
    if (!alias) alias = srg_read_git_alias(executable, target, subcommand);
    if (!alias) {
        free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return NULL;
    }
    if (alias[0] == '!') {
        char *op = srg_find_mutation(alias + 1, target, root, depth + 1);
        free(alias); free(target); free(subcommand); free(inline_aliases);
        for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
        return op;
    }
    /* shlex.split(alias, posix=True) then inspect_git([*alias_args, *sub_args]) */
    char **alias_args = NULL;
    int n_alias = 0;
    {
        /* simple whitespace tokenizer (shlex posix approximation) */
        char *copy = strdup(alias);
        char **toks = calloc(32, sizeof(char *));
        char *save = NULL;
        int t = 0;
        for (char *tok = strtok_r(copy, " \t", &save); tok && t < 31; tok = strtok_r(NULL, " \t", &save))
            toks[t++] = strdup(tok);
        free(copy);
        alias_args = toks; n_alias = t;
    }
    int n_sub = 0; while (sub_args && sub_args[n_sub]) n_sub++;
    char **merged = calloc(n_alias + n_sub + 1, sizeof(char *));
    if (merged) {
        for (int i = 0; i < n_alias; i++) merged[i] = alias_args[i];
        for (int i = 0; i < n_sub; i++) merged[n_alias + i] = strdup(sub_args[i]);
        merged[n_alias + n_sub] = NULL;
    }
    char *op = srg_inspect_git(executable, merged, target, "", root, depth + 1);
    for (int i = 0; alias_args && alias_args[i]; i++) free(alias_args[i]); free(alias_args);
    for (int i = 0; merged && merged[i]; i++) free(merged[i]); free(merged);
    free(alias); free(target); free(subcommand); free(inline_aliases);
    for (int i = 0; sub_args && sub_args[i]; i++) free(sub_args[i]); free(sub_args);
    return op;
}

/* ── _inspect_github_cli(executable, args, current_dir, root) ── */
char *srg_inspect_github_cli(const char *executable, char **args, const char *current_dir, const char *root) {
    if (!srg_is_within(current_dir, root)) return NULL;
    char *name = srg_executable_name(executable);
    static const char *const gh_opts[] = {"-R", "--repo", "--hostname", NULL};
    size_t idx = srg_consume_options(args, 0, gh_opts);
    bool is_pr_checkout = args && args[idx] && args[idx+1] &&
        strcmp(args[idx], "pr") == 0 && strcmp(args[idx+1], "checkout") == 0;
    if (is_pr_checkout) {
        char *out = malloc(strlen(name) + 16);
        if (out) snprintf(out, strlen(name) + 16, "%s pr checkout", name);
        free(name);
        return out;
    }
    free(name);
    return NULL;
}

/* ── _find_mutation(command, cwd, root, depth) ──
 * Returns operation string (malloc'd) or NULL. */
char *srg_find_mutation(const char *command, const char *cwd, const char *root, int depth) {
    if (depth > SRG_MAX_RECURSION) return NULL;

    char *masked; char **heredoc_scripts;
    srg_mask_heredocs(command, &masked, &heredoc_scripts);
    for (int i = 0; heredoc_scripts && heredoc_scripts[i]; i++) {
        char *op = srg_find_mutation(heredoc_scripts[i], cwd, root, depth + 1);
        if (op) {
            free(masked);
            for (int j = 0; heredoc_scripts[j]; j++) free(heredoc_scripts[j]); free(heredoc_scripts);
            return op;
        }
    }

    size_t starts[128];
    size_t nstarts = iter_shell_command_starts(masked, starts, 128);
    if (nstarts > 128) nstarts = 128;

    /* sort + dedupe starts */
    for (size_t i = 1; i < nstarts; i++) {
        size_t v = starts[i]; size_t j = i;
        while (j > 0 && starts[j-1] > v) { starts[j] = starts[j-1]; j--; }
        starts[j] = v;
    }
    size_t unique = 0;
    for (size_t i = 0; i < nstarts; i++) if (unique == 0 || starts[i] != starts[unique-1]) starts[unique++] = starts[i];
    nstarts = unique;

    size_t scopes[128][SRG_MAX_CONTEXT];
    size_t scounts[128];
    srg_scope_keys(masked, starts, nstarts, scopes, scounts);

    /* cwd_by_scope keyed by scope depth (0 = root). We track a flat map:
     * cwd_by_scope[0] = cwd. Since scope stack mostly nests, approximate
     * by keying on the deepest opener count. */
    char *cwd_by_scope[SRG_MAX_CONTEXT];
    for (int i = 0; i < SRG_MAX_CONTEXT; i++) cwd_by_scope[i] = NULL;
    cwd_by_scope[0] = strdup(cwd ? cwd : "/");
    char *pending_cd[SRG_MAX_CONTEXT]; /* we track pending cd per depth */
    bool pending_has[SRG_MAX_CONTEXT];
    for (int i = 0; i < SRG_MAX_CONTEXT; i++) { pending_cd[i] = NULL; pending_has[i] = false; }

    for (size_t si = 0; si < nstarts; si++) {
        size_t depth_scope = scounts[si];
        if (depth_scope >= SRG_MAX_CONTEXT) depth_scope = SRG_MAX_CONTEXT - 1;
        if (!cwd_by_scope[depth_scope]) {
            const char *parent = cwd_by_scope[depth_scope ? depth_scope - 1 : 0];
            cwd_by_scope[depth_scope] = strdup(parent ? parent : "/");
        }
        char operator[4];
        srg_operator_before(masked, starts[si], operator);
        char *current_cwd = cwd_by_scope[depth_scope];
        if (pending_has[depth_scope] &&
            (strcmp(operator, "&&") == 0 || strcmp(operator, ";") == 0 || strcmp(operator, "\n") == 0)) {
            free(current_cwd);
            cwd_by_scope[depth_scope] = strdup(pending_cd[depth_scope]);
            current_cwd = cwd_by_scope[depth_scope];
        }

        char **words = srg_shell_words_at(masked, starts[si]);
        char *envp, *executable; char **args;
        srg_command_parts(words, &envp, &executable, &args);
        if (!executable) {
            free(envp);
            for (int i = 0; words && words[i]; i++) free(words[i]); free(words);
            continue;
        }

        char *cd_target = srg_cd_target(executable, args, current_cwd);
        if (cd_target) {
            free(pending_cd[depth_scope]);
            pending_cd[depth_scope] = cd_target;
            pending_has[depth_scope] = true;
            free(envp); free(executable);
            if (args) { for (int i = 0; args[i]; i++) free(args[i]); free(args); }
            for (int i = 0; words && words[i]; i++) free(words[i]); free(words);
            continue;
        }

        char *ex_name = srg_executable_name(executable);
        char *op = NULL;
        if (ex_name && strcmp(ex_name, "git") == 0) {
            op = srg_inspect_git(executable, args, current_cwd, envp, root, depth);
        } else if (ex_name && (strcmp(ex_name, "gh") == 0 || strcmp(ex_name, "hub") == 0)) {
            op = srg_inspect_github_cli(executable, args, current_cwd, root);
        } else if (ex_name && srg_str_in(srg_shell_executables, ex_name)) {
            char *script = srg_shell_script_arg(args);
            if (script) {
                op = srg_find_mutation(script, current_cwd, root, depth + 1);
                free(script);
            }
        }
        free(ex_name);
        free(envp); free(executable);
        if (args) { for (int i = 0; args[i]; i++) free(args[i]); free(args); }
        for (int i = 0; words && words[i]; i++) free(words[i]); free(words);

        if (op) {
            free(masked);
            for (int j = 0; heredoc_scripts && heredoc_scripts[j]; j++) free(heredoc_scripts[j]);
            free(heredoc_scripts);
            for (int i = 0; i < SRG_MAX_CONTEXT; i++) { free(cwd_by_scope[i]); free(pending_cd[i]); }
            return op;
        }
    }

    free(masked);
    for (int j = 0; heredoc_scripts && heredoc_scripts[j]; j++) free(heredoc_scripts[j]);
    free(heredoc_scripts);
    for (int i = 0; i < SRG_MAX_CONTEXT; i++) { free(cwd_by_scope[i]); free(pending_cd[i]); }
    return NULL;
}

/* ── detect_self_repo_git_mutation(command, cwd, source_root) ──
 * Returns true + operation/message out-params when a mutation is found. */
bool srg_detect_self_repo_git_mutation(const char *command, const char *cwd,
                                       const char *source_root,
                                       char **out_operation, char **out_block_message) {
    *out_operation = NULL;
    *out_block_message = NULL;
    char *root = source_root ? strdup(source_root) : srg_get_running_source_root();
    if (!root || !command || !*command) { free(root); return false; }

    char *root_resolved = srg_resolve(root, "/");
    free(root);
    char *base = cwd ? srg_resolve(cwd, "/") : strdup("/");

    char *operation = srg_find_mutation(command, base, root_resolved, 0);
    free(base);
    if (!operation) { free(root_resolved); return false; }
    *out_operation = operation;
    *out_block_message = srg_block_message(operation, root_resolved);
    free(root_resolved);
    return true;
}

/* ── _block_message(operation, root) ── */
char *srg_block_message(const char *operation, const char *root) {
    const char *fmt = "Blocked: `%s` would rewrite Hermes's live source checkout "
                      "(%s) and can mix module versions in this running process. "
                      "Use a separate worktree or temporary clone. To change this "
                      "checkout, stop Hermes, run the command externally, then restart Hermes.";
    size_t n = strlen(fmt) + (operation ? strlen(operation) : 0) + (root ? strlen(root) : 0) + 8;
    char *out = malloc(n);
    if (out) snprintf(out, n, fmt, operation ? operation : "", root ? root : "");
    return out;
}
