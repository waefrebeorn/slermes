/* Slermes C port — hermes_cli/mcp_security.py (pure IOC-scan helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_regex.h"
#include "json.h"

/* PoP: hermes_cli_mcp_security__command_basename @ hermes_cli/mcp_security.py:_command_basename */
void hermes_cli_mcp_security_command_basename(const char *command, char *out, size_t outsz)
{
    if (!command || !*command) { out[0] = '\0'; return; }
    while (*command == ' ' || *command == '\t') command++;
    /* extract first token (handle surrounding quotes) */
    char first[1024]; size_t fi = 0;
    char quote = 0;
    if (*command == '\'' || *command == '"') { quote = *command; command++; }
    while (*command && fi < sizeof(first) - 1) {
        if (quote) {
            if (*command == quote) break;
            if (*command == '\\' && quote == '"' && command[1]) { command++; first[fi++] = *command; command++; continue; }
            first[fi++] = *command; command++;
        } else {
            if (*command == ' ' || *command == '\t') break;
            if (*command == '\'' || *command == '"') { quote = *command; command++; continue; }
            if (*command == '\\' && command[1]) { command++; first[fi++] = *command; command++; continue; }
            first[fi++] = *command; command++;
        }
    }
    first[fi] = '\0';
    const char *base = strrchr(first, '/');
    base = base ? base + 1 : first;
    size_t i = 0;
    for (; base[i] && i + 1 < outsz; i++) out[i] = (char)tolower((unsigned char)base[i]);
    out[i] = '\0';
}

/* PoP: hermes_cli_mcp_security__inline_script @ hermes_cli/mcp_security.py:_inline_script */
void hermes_cli_mcp_security_inline_script(const char *args_json, char *out, size_t outsz)
{
    /* args may be a JSON array (we accept a pre-joined string or a list serialized
     * by the caller). Faithful to Python: list/tuple -> " ".join(str(item));
     * else str(args). Here the harness passes the already-joined representation. */
    if (!args_json) { out[0] = '\0'; return; }
    snprintf(out, outsz, "%s", args_json);
}

/* PoP: hermes_cli_mcp_security__entry_text @ hermes_cli/mcp_security.py:_entry_text */
/* entry is passed as "command\x1fargs\x1fenv1\x1fenv2..." (unit sep between fields). */
void hermes_cli_mcp_security_entry_text(const char *entry, char *out, size_t outsz)
{
    if (!entry) { if (outsz) out[0] = '\0'; return; }
    if (outsz == 0) return;
    char *parts[64]; int n = 0;
    char *buf = strdup(entry);
    char *sp = buf;
    char *tok = strsep(&sp, "\x1f");
    while (tok && n < 64) { parts[n++] = tok; tok = strsep(&sp, "\x1f"); }
    size_t o = 0;
    for (int i = 0; i < n; i++) {
        if (i) {
            if (o + 1 < outsz) out[o++] = ' ';
        }
        size_t L = strlen(parts[i]);
        size_t avail = outsz - o - 1;
        if (L > avail) L = avail;
        if (L > 0) { memcpy(out + o, parts[i], L); o += L; }
    }
    out[o] = '\0';
    free(buf);
}

/* ── Constants (faithful to mcp_security.py module globals) ─────────────── */

static const char *SHELL_INTERPRETERS[] = {
    "bash","sh","zsh","dash","fish","cmd","cmd.exe",
    "powershell","powershell.exe","pwsh","pwsh.exe", NULL
};

/* Attacker artifacts — June 2026 hermes-0day campaign (mcp_security._IOC_SUBSTRINGS) */
static const char *IOC_SUBSTRINGS[] = {
    "AAAAC3NzaC1lZDI1NTE5AAAAICBoh1oDC4DnsO1m5mJ4yfEKrQebaFh",
    "hermes-0day",
    "60.165.167.",
    "118.182.244.156",
    "61.178.123.196",
    NULL
};

/* POSIX-ERE equivalents of the Python regexes (case-insensitive via flag 1). */
static const char *EGRESS_PATTERN =
    "(^|[^[:alnum:]._-])(curl|wget|nc|ncat|socat)([^[:alnum:]._-]|$)"
    "|/dev/tcp/"
    "|Invoke-WebRequest"
    "|Invoke-RestMethod"
    "|System\\.Net\\.WebClient";

static const char *EXFIL_HINT_PATTERN =
    "\\.env|--data-binary|--data-raw|-X[[:space:]]+POST|POST|<[[:space:]]*[^[:space:]]+";

static const char *PERSISTENCE_PATTERN =
    "authorized_keys"
    "|\\.ssh/"
    "|/etc/ssh"
    "|/etc/pam\\.d|pam_[[:alnum:]_-]+\\.so"
    "|/etc/sudoers"
    "|/etc/cron|crontab"
    "|/etc/rc\\.local|/etc/systemd"
    "|\\.bashrc|\\.bash_profile|\\.profile|\\.zshrc";

static bool re_search(const char *pattern, const char *text)
{
    if (!text || !text[0]) return false;
    hregex_t *re = regex_compile(pattern, 1 /* ICASE */);
    if (!re) return false;
    regex_match_t *m = regex_search(re, text);
    bool hit = (m && m->matched);
    if (m) regex_match_free(m);
    regex_free(re);
    return hit;
}

static char *flatten_env(json_t *env)
{
    if (!env || env->type != JSON_OBJECT) return strdup("");
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (size_t i = 0; i < env->c.count; i++) {
        json_t *v = env->c.items[i];
        const char *sv = (v && v->type == JSON_STRING) ? v->str_val : "";
        size_t need = len + strlen(sv) + 2;
        if (need > cap) { cap = need * 2; char *n = realloc(out, cap); if (!n) { free(out); return NULL; } out = n; }
        if (len) out[len++] = ' ';
        strcpy(out + len, sv);
        len += strlen(sv);
    }
    return out;
}

static char *join_args(json_t *args)
{
    if (!args) return strdup("");
    if (args->type == JSON_STRING) return strdup(args->str_val ? args->str_val : "");
    if (args->type != JSON_ARRAY) return strdup("");
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (size_t i = 0; i < args->c.count; i++) {
        json_t *it = args->c.items[i];
        char tmp[64];
        const char *sv;
        if (it && it->type == JSON_STRING) sv = it->str_val ? it->str_val : "";
        else if (it && it->type == JSON_NUMBER) { snprintf(tmp, sizeof(tmp), "%g", it->num_val); sv = tmp; }
        else sv = "";
        size_t need = len + strlen(sv) + 2;
        if (need > cap) { cap = need * 2; char *n = realloc(out, cap); if (!n) { free(out); return NULL; } out = n; }
        if (len) out[len++] = ' ';
        strcpy(out + len, sv);
        len += strlen(sv);
    }
    return out;
}

/* PoP: hermes_cli_mcp_security_validate_mcp_server_entry @ hermes_cli/mcp_security.py:validate_mcp_server_entry */
/* Returns a JSON array of warning strings. Empty array = not suspicious. */
json_t *hermes_cli_mcp_security_validate_mcp_server_entry(const char *name, json_t *entry)
{
    json_t *issues = json_array();
    if (!issues) return NULL;
    if (!entry || entry->type != JSON_OBJECT) return issues;
    if (!name) name = "";

    /* 1. Hardcoded IOC blocklist — regardless of command shape. */
    json_t *command = json_obj_get(entry, "command");
    json_t *args    = json_obj_get(entry, "args");
    json_t *env     = json_obj_get(entry, "env");

    const char *cmd_str = (command && command->type == JSON_STRING) ? command->str_val : "";
    char *args_str = join_args(args);
    char *env_str  = flatten_env(env);

    size_t flat_len = strlen(cmd_str) + strlen(args_str ? args_str : "") +
                      strlen(env_str ? env_str : "") + 4;
    char *flat = malloc(flat_len);
    if (flat) snprintf(flat, flat_len, "%s %s %s", cmd_str,
                       args_str ? args_str : "", env_str ? env_str : "");

    for (int i = 0; IOC_SUBSTRINGS[i]; i++) {
        if (flat && strstr(flat, IOC_SUBSTRINGS[i])) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "MCP server '%s' contains a known hermes-0day "
                     "indicator-of-compromise ('%s')", name, IOC_SUBSTRINGS[i]);
            json_append(issues, json_string(msg));
            free(flat); free(args_str); free(env_str);
            return issues;   /* one IOC is enough */
        }
    }
    free(flat);

    /* 2/3. Only shell interpreters with an inline script are further scrutinized. */
    char basename[256];
    hermes_cli_mcp_security_command_basename(cmd_str, basename, sizeof(basename));
    bool is_shell = false;
    for (int i = 0; SHELL_INTERPRETERS[i]; i++)
        if (strcmp(basename, SHELL_INTERPRETERS[i]) == 0) { is_shell = true; break; }
    if (!is_shell) { free(args_str); free(env_str); return issues; }

    const char *script = args_str ? args_str : "";
    if (!script[0]) { free(args_str); free(env_str); return issues; }

    /* 2. Network exfiltration shape. */
    if (re_search(EGRESS_PATTERN, script)) {
        char msg[512];
        if (re_search(EXFIL_HINT_PATTERN, script))
            snprintf(msg, sizeof(msg),
                     "MCP server '%s' uses shell interpreter '%s' with network "
                     "egress in args and exfiltration-shaped arguments", name, cmd_str);
        else
            snprintf(msg, sizeof(msg),
                     "MCP server '%s' uses shell interpreter '%s' with network "
                     "egress in args", name, cmd_str);
        json_append(issues, json_string(msg));
    }

    /* 3. OS persistence shape. */
    if (re_search(PERSISTENCE_PATTERN, script)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "MCP server '%s' uses shell interpreter '%s' to write to an OS "
                 "persistence surface (SSH keys / PAM / sudoers / cron / shell rc) "
                 "— this is the hermes-0day backdoor shape, not a real MCP server",
                 name, cmd_str);
        json_append(issues, json_string(msg));
    }

    free(args_str); free(env_str);
    return issues;
}

/* PoP: hermes_cli_mcp_security_is_mcp_server_entry_suspicious @ hermes_cli/mcp_security.py:is_mcp_server_entry_suspicious */
bool hermes_cli_mcp_security_is_mcp_server_entry_suspicious(const char *name, json_t *entry)
{
    json_t *issues = hermes_cli_mcp_security_validate_mcp_server_entry(name, entry);
    bool suspicious = issues && issues->type == JSON_ARRAY && issues->c.count > 0;
    if (issues) json_free(issues);
    return suspicious;
}
