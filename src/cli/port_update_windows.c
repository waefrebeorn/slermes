/*
 * port_update_windows.c
 *
 * Pure helpers ported from hermes_cli/update_cmd_windows.py (66 defs, previously
 * 0 ported) and gateway/status.py.
 *
 *   uw_lower_dir_prefix     <- _lower_dir_prefix
 *   uw_cwd_prefix           <- _cwd_prefix
 *   uw_holder_subcommand    <- _hermes_holder_subcommand
 *   uw_holder_kind          <- _venv_holder_kind
 *   uw_is_pausable_gateway  <- _is_pausable_gateway
 *   uw_gateway_subcommand   <- gateway/status.py:_gateway_command_subcommand
 *
 * The process scan / psutil / updater half stays in Python.
 */

#include "port_update_windows.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Windows os.sep */
#define UW_SEP '\\'

/* ===========================================================================
 *  _lower_dir_prefix  (update_cmd_windows.py:_lower_dir_prefix)
 *
 *      try: raw = str(path.resolve())
 *      except OSError: raw = str(path)
 *      return raw.lower().rstrip(os.sep) + os.sep
 *
 *  `resolved` is the resolve() answer, or NULL to use `path` verbatim (the
 *  OSError path). The trailing separator is what makes this a PREFIX match:
 *  "C:/hermes" matches "C:/hermes/bin" but not "C:/hermes2". Caller frees.
 * =========================================================================== */

/* PoP: _lower_dir_prefix @ hermes_cli/update_cmd_windows.py:_lower_dir_prefix */
char *uw_lower_dir_prefix(const char *path, const char *resolved)
{
    const char *raw = (resolved && resolved[0]) ? resolved : (path ? path : "");

    size_t n = strlen(raw);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;

    size_t k = 0;
    for (size_t i = 0; i < n; i++) out[k++] = (char)tolower((unsigned char)raw[i]);
    while (k > 0 && (out[k - 1] == UW_SEP || out[k - 1] == '/')) k--;
    out[k++] = (char)UW_SEP;
    out[k] = '\0';
    return out;
}

/* ===========================================================================
 *  _cwd_prefix  (update_cmd_windows.py:_cwd_prefix)
 *
 *      try: return str(proc.cwd() or "").lower().rstrip(os.sep) + os.sep
 *      except Exception: return os.sep
 *
 *  `cwd` is the process cwd, or NULL when it could not be read (the except
 *  path, which yields a bare separator). Caller frees.
 * =========================================================================== */

/* PoP: _cwd_prefix @ hermes_cli/update_cmd_windows.py:_cwd_prefix */
char *uw_cwd_prefix(const char *cwd)
{
    if (!cwd) return strdup("\\");   /* the except path */

    size_t n = strlen(cwd);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;

    size_t k = 0;
    for (size_t i = 0; i < n; i++) out[k++] = (char)tolower((unsigned char)cwd[i]);
    while (k > 0 && (out[k - 1] == UW_SEP || out[k - 1] == '/')) k--;
    out[k++] = (char)UW_SEP;
    out[k] = '\0';
    return out;
}

/* ===========================================================================
 *  _gateway_command_subcommand  (gateway/status.py:_gateway_command_subcommand)
 *
 *  The canonical gateway matcher. No loose substring matches: it needs a Hermes
 *  entrypoint plus the `gateway` subcommand, or a gateway-dedicated entrypoint.
 *
 *      - shlex.split(posix=False) tokenisation, falling back to a plain split
 *      - strip surrounding quotes, normalise slashes, lower-case per token
 *      - desktop-gateway.py / hermes-gateway[.exe] basename => "run"
 *      - else require hermes_cli.main / hermes_cli/main.py / hermes[.exe]
 *      - drop --profile X / -p X / --profile=X / -p=X  (they can consume the
 *        value "gateway" too)
 *      - the first "gateway" token's successor is the subcommand; a bare
 *        `hermes gateway` defaults to "run"
 *
 *  Returns a malloc'd string, or NULL for "not a gateway command".
 * =========================================================================== */

/* PoP: _gateway_command_subcommand @ gateway/status.py:_gateway_command_subcommand */
char *uw_gateway_subcommand(const char *command)
{
    if (!command || !command[0]) return NULL;

    /* shlex.split(posix=False): split on whitespace, keep quotes as characters.
     * A plain whitespace split is the documented ValueError fallback and is
     * close enough for the token tests that follow. */
    size_t cap = strlen(command) + 1;
    char **tokens = (char **)calloc(cap / 2 + 8, sizeof(char *));
    if (!tokens) return NULL;
    int nt = 0;

    const char *p = command;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        int quoted = 0;
        char q = 0;
        while (*p) {
            if (quoted) {
                if (*p == q) { quoted = 0; p++; }
                else p++;
            } else if (*p == '"' || *p == '\'') { quoted = 1; q = *p; p++; }
            else if (isspace((unsigned char)*p)) break;
            else p++;
        }
        size_t len = (size_t)(p - start);
        char *tok = (char *)malloc(len + 1);
        if (tok) {
            memcpy(tok, start, len);
            tok[len] = '\0';
            /* strip surrounding quotes, normalise slashes */
            size_t a = 0, b = len;
            while (a < b && (tok[a] == '"' || tok[a] == '\'')) a++;
            while (b > a && (tok[b - 1] == '"' || tok[b - 1] == '\'')) b--;
            size_t k = 0;
            for (size_t i = a; i < b; i++) tok[k++] = (tok[i] == '\\') ? '/' : tok[i];
            tok[k] = '\0';
            if (k) tokens[nt++] = tok; else free(tok);
        }
        if (!*p) break;
    }

    if (nt == 0) { free(tokens); return NULL; }

    /* basenames */
    char **base = (char **)calloc((size_t)nt, sizeof(char *));
    if (!base) { for (int i = 0; i < nt; i++) free(tokens[i]); free(tokens); return NULL; }
    for (int i = 0; i < nt; i++) {
        char *s = tokens[i];
        for (size_t j = 0; s[j]; j++) s[j] = (char)tolower((unsigned char)s[j]);
        const char *sl = strrchr(s, '/');
        base[i] = sl ? (char *)(sl + 1) : s;
    }

    char *result = NULL;

    /* gateway-dedicated entrypoints */
    for (int i = 0; i < nt && !result; i++) {
        if (strcmp(base[i], "desktop-gateway.py") == 0 ||
            strcmp(base[i], "hermes-gateway") == 0 ||
            strcmp(base[i], "hermes-gateway.exe") == 0)
            result = strdup("run");
    }

    if (!result) {
        /* need a Hermes entrypoint */
        int has_entry = 0;
        for (int i = 0; i < nt && !has_entry; i++) {
            if (strstr(tokens[i], "hermes_cli.main") ||
                strstr(tokens[i], "hermes_cli/main.py") ||
                strcmp(base[i], "hermes") == 0 ||
                strcmp(base[i], "hermes.exe") == 0)
                has_entry = 1;
        }
        if (has_entry) {
            /* drop --profile X / -p X / --profile=X / -p=X */
            char **filtered = (char **)calloc((size_t)nt, sizeof(char *));
            if (filtered) {
                int nf = 0, skip_next = 0;
                for (int i = 0; i < nt; i++) {
                    if (skip_next) { skip_next = 0; continue; }
                    if (strcmp(tokens[i], "--profile") == 0 || strcmp(tokens[i], "-p") == 0) {
                        skip_next = 1;
                        continue;
                    }
                    if (strncmp(tokens[i], "--profile=", 10) == 0 ||
                        strncmp(tokens[i], "-p=", 3) == 0)
                        continue;
                    filtered[nf++] = tokens[i];
                }
                for (int i = 0; i < nf && !result; i++) {
                    if (strcmp(filtered[i], "gateway") == 0)
                        result = strdup((i + 1 < nf) ? filtered[i + 1] : "run");
                }
                free(filtered);
            }
        }
    }

    for (int i = 0; i < nt; i++) free(tokens[i]);
    free(base);
    free(tokens);
    return result;
}

/* ===========================================================================
 *  _is_pausable_gateway  (hermes_cli/_scan_venv_blockers.py)
 *
 *      try: from gateway.status import looks_like_gateway_command_line
 *      except Exception: return False
 *      return looks_like_gateway_command_line(cmdline)
 *
 *  which is `_gateway_command_subcommand(command) == "run"`. An unavailable
 *  matcher must fail CLOSED (False) - a false positive would make the updater
 *  refuse to stop a process it does not own.
 * =========================================================================== */

/* PoP: _is_pausable_gateway @ hermes_cli/_scan_venv_blockers.py:_is_pausable_gateway */
int uw_is_pausable_gateway(const char *cmdline)
{
    char *sub = uw_gateway_subcommand(cmdline);
    if (!sub) return 0;
    int hit = (strcmp(sub, "run") == 0);
    free(sub);
    return hit;
}

/* ===========================================================================
 *  _hermes_holder_subcommand  (update_cmd_windows.py:_hermes_holder_subcommand)
 *
 *  Token-based, never substring: find the hermes_cli.main / hermes(.exe) entry
 *  token, then return the first following token that is not a flag or a flag's
 *  value. Profile selectors (--profile X, -p X) are skipped.
 *
 *      def _is_entry(i, token):
 *          low = token.lower().strip("\"'").replace("\\", "/")
 *          return (low.endswith("hermes_cli.main") and i > 0 and tokens[i-1] == "-m") or (
 *              low.rsplit("/", 1)[-1] in ("hermes", "hermes.exe")) or low.endswith("hermes_cli/main.py")
 *
 *  The value-flag set is the fallback table (the real one comes from the CLI
 *  parser); `--flag value` consumes two tokens, `--flag=value` one.
 * =========================================================================== */

/* PoP: _hermes_holder_subcommand @ hermes_cli/update_cmd_windows.py:_hermes_holder_subcommand */
char *uw_holder_subcommand(const char *cmdline)
{
    if (!cmdline || !cmdline[0]) return NULL;

    static const char *const VALUE_FLAGS[] = {
        "--profile", "-p", "--config", "--model", "-m", "--provider", "--reasoning",
        "--toolsets", "-t", "--skills", "-s", "--continue", "-c", "--resume", "-r",
        "--oneshot", "-z", "--in", "--usage-file",
    };
#define UW_VALUE_FLAGS_N (sizeof(VALUE_FLAGS) / sizeof(VALUE_FLAGS[0]))

    /* tokenise like uw_gateway_subcommand */
    size_t cap = strlen(cmdline) + 1;
    char **tokens = (char **)calloc(cap / 2 + 8, sizeof(char *));
    if (!tokens) return NULL;
    int nt = 0;

    const char *p = cmdline;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        int quoted = 0; char q = 0;
        while (*p) {
            if (quoted) { if (*p == q) { quoted = 0; p++; } else p++; }
            else if (*p == '"' || *p == '\'') { quoted = 1; q = *p; p++; }
            else if (isspace((unsigned char)*p)) break;
            else p++;
        }
        size_t len = (size_t)(p - start);
        char *tok = (char *)malloc(len + 1);
        if (tok) {
            memcpy(tok, start, len);
            tok[len] = '\0';
            size_t a = 0, b = len;
            while (a < b && (tok[a] == '"' || tok[a] == '\'')) a++;
            while (b > a && (tok[b - 1] == '"' || tok[b - 1] == '\'')) b--;
            size_t k = 0;
            for (size_t i = a; i < b; i++) tok[k++] = (tok[i] == '\\') ? '/' : tok[i];
            tok[k] = '\0';
            for (size_t i = 0; tok[i]; i++) tok[i] = (char)tolower((unsigned char)tok[i]);
            if (k) tokens[nt++] = tok; else free(tok);
        }
        if (!*p) break;
    }

    char *result = NULL;
    int entry = -1;
    for (int i = 0; i < nt && entry < 0; i++) {
        const char *low = tokens[i];
        const char *sl = strrchr(low, '/');
        const char *base = sl ? sl + 1 : low;
        if ((strstr(low, "hermes_cli.main") && i > 0 && strcmp(tokens[i - 1], "-m") == 0) ||
            strcmp(base, "hermes") == 0 || strcmp(base, "hermes.exe") == 0 ||
            strstr(low, "hermes_cli/main.py"))
            entry = i;
    }
    if (entry >= 0) {
        int i = entry + 1;
        while (i < nt && !result) {
            const char *tok = tokens[i];
            int is_value_flag = 0;
            for (size_t v = 0; v < UW_VALUE_FLAGS_N; v++) {
                if (strcmp(tok, VALUE_FLAGS[v]) == 0 ||
                    strncmp(tok, VALUE_FLAGS[v], strlen(VALUE_FLAGS[v])) == 0) {
                    /* `--flag=value` consumes ONE token; `--flag value` TWO */
                    size_t vl = strlen(VALUE_FLAGS[v]);
                    if (tok[vl] == '=') { is_value_flag = 1; }
                    else if (tok[vl] == '\0') { is_value_flag = 1; i++; }
                    break;
                }
            }
            if (is_value_flag) { i++; continue; }
            if (tok[0] == '-') { i++; continue; }
            result = strdup(tok);
            break;
        }
    }

    for (int i = 0; i < nt; i++) free(tokens[i]);
    free(tokens);
    return result;
}

/* ===========================================================================
 *  _venv_holder_kind  (update_cmd_windows.py:_venv_holder_kind)
 *
 *      if _is_pausable_gateway(cmdline): return "gateway"
 *      subcommand = _hermes_holder_subcommand(cmdline)
 *      if subcommand in ("serve", "dashboard"): return "backend"
 *      if subcommand: return f"hermes:{subcommand}"
 *      return "python"
 *
 *  Derived from the same classifiers the refusal path uses, so automation stops
 *  exactly what the guard would refuse on. Caller frees.
 * =========================================================================== */

/* PoP: _venv_holder_kind @ hermes_cli/update_cmd_windows.py:_venv_holder_kind */
char *uw_holder_kind(const char *cmdline)
{
    if (uw_is_pausable_gateway(cmdline)) return strdup("gateway");

    char *sub = uw_holder_subcommand(cmdline);
    if (!sub) return strdup("python");

    char *out;
    if (strcmp(sub, "serve") == 0 || strcmp(sub, "dashboard") == 0) {
        out = strdup("backend");
    } else {
        size_t n = strlen(sub) + 8;
        out = (char *)malloc(n);
        if (out) snprintf(out, n, "hermes:%s", sub);
    }
    free(sub);
    return out;
}
