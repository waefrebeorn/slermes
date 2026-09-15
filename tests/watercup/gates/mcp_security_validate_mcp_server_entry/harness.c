/* gate: hermes_cli_mcp_security_validate_mcp_server_entry — traced faithful
 * copy of src/hermes_cli/mcp_security.c::hermes_cli_mcp_security_
 * validate_mcp_server_entry (constants + regexes copied verbatim), PLUS an
 * honesty check: the REAL port is linked via extra_src and both outputs must
 * agree byte-for-byte (exit 3 on drift).
 *
 * Twin: LIVE hermes_cli/mcp_security.py::validate_mcp_server_entry, same rung
 * order. Inputs are "<name>|<entry_json>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "json.h"
#include "hermes_regex.h"

json_t *hermes_cli_mcp_security_validate_mcp_server_entry(const char *name, json_t *entry);

/* ---- verbatim copies of the port's statics ---- */
static const char *SHELL_INTERPRETERS[] = {
    "bash","sh","zsh","dash","fish","cmd","cmd.exe",
    "powershell","powershell.exe","pwsh","pwsh.exe", NULL
};
static const char *IOC_SUBSTRINGS[] = {
    "AAAAC3NzaC1lZDI1NTE5AAAAICBoh1oDC4DnsO1m5mJ4yfEKrQebaFh",
    "hermes-0day",
    "60.165.167.",
    "118.182.244.156",
    "61.178.123.196",
    NULL
};
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

static bool re_search2(const char *pattern, const char *text) {
    if (!text || !text[0]) return false;
    hregex_t *re = regex_compile(pattern, 1);
    if (!re) return false;
    regex_match_t *m = regex_search(re, text);
    bool hit = (m && m->matched);
    if (m) regex_match_free(m);
    regex_free(re);
    return hit;
}

static void command_basename(const char *command, char *out, size_t outsz) {
    if (!command || !*command) { out[0] = '\0'; return; }
    while (*command == ' ' || *command == '\t') command++;
    char first[1024]; size_t fi = 0; char quote = 0;
    if (*command == '\'' || *command == '"') { quote = *command; command++; }
    while (*command && fi < sizeof first - 1) {
        if (quote) {
            if (*command == quote) break;
            if (*command == '\\' && quote == '"' && command[1]) { command++; first[fi++] = *command++; continue; }
            first[fi++] = *command++;
        } else {
            if (*command == ' ' || *command == '\t') break;
            if (*command == '\'' || *command == '"') { quote = *command++; continue; }
            if (*command == '\\' && command[1]) { command++; first[fi++] = *command++; continue; }
            first[fi++] = *command++;
        }
    }
    first[fi] = '\0';
    const char *base = strrchr(first, '/');
    base = base ? base + 1 : first;
    size_t i = 0;
    for (; base[i] && i + 1 < outsz; i++) out[i] = (char)tolower((unsigned char)base[i]);
    out[i] = '\0';
}

static char *join_args(json_t *args) {
    if (!args) return strdup("");
    if (args->type == JSON_STRING) return strdup(args->str_val ? args->str_val : "");
    if (args->type != JSON_ARRAY) return strdup("");
    size_t cap = 256, len = 0; char *out = malloc(cap); out[0] = '\0';
    for (size_t i = 0; i < args->c.count; i++) {
        json_t *it = args->c.items[i];
        char tmp[64]; const char *sv;
        if (it && it->type == JSON_STRING) sv = it->str_val ? it->str_val : "";
        else if (it && it->type == JSON_NUMBER) { snprintf(tmp, sizeof tmp, "%g", it->num_val); sv = tmp; }
        else sv = "";
        size_t need = len + strlen(sv) + 2;
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (len) out[len++] = ' ';
        strcpy(out + len, sv); len += strlen(sv);
    }
    return out;
}

static char *flatten_env(json_t *env) {
    if (!env || env->type != JSON_OBJECT) return strdup("");
    size_t cap = 256, len = 0; char *out = malloc(cap); out[0] = '\0';
    for (size_t i = 0; i < env->c.count; i++) {
        json_t *v = env->c.items[i];
        const char *sv = (v && v->type == JSON_STRING) ? v->str_val : "";
        size_t need = len + strlen(sv) + 2;
        if (need > cap) { cap = need * 2; out = realloc(out, cap); }
        if (len) out[len++] = ' ';
        strcpy(out + len, sv); len += strlen(sv);
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s validate_mcp_server_entry <name|entry_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *inp = strdup(argv[i]);
        char *bar = strchr(inp, '|');
        if (!bar) { fprintf(stderr, "bad input: %s\n", argv[i]); free(inp); return 2; }
        *bar = '\0';
        const char *name = inp;
        json_t *entry = json_parse(bar + 1, NULL);

        WCUP_BEGIN("validate_mcp_server_entry");

        /* ---- traced ladder (mirrors the port body) ---- */
        WCUP_BRANCH(entry && entry->type == JSON_OBJECT);      /* B0 */
        char vbuf[4096]; vbuf[0] = '\0';
        if (!entry || entry->type != JSON_OBJECT) {
            WCUP_VALSTR(vbuf);
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
            WCUP_END("validate_mcp_server_entry");
            free(inp);
            continue;
        }
        if (!name) name = "";

        json_t *command_j = json_obj_get(entry, "command");
        json_t *args_j = json_obj_get(entry, "args");
        json_t *env_j = json_obj_get(entry, "env");
        const char *cmd_str = (command_j && command_j->type == JSON_STRING) ? command_j->str_val : "";
        char *args_str = join_args(args_j);
        char *env_str = flatten_env(env_j);
        size_t flat_len = strlen(cmd_str) + strlen(args_str) + strlen(env_str) + 4;
        char *flat = malloc(flat_len);
        snprintf(flat, flat_len, "%s %s %s", cmd_str, args_str, env_str);

        int ioc_hit = -1;
        for (int k = 0; IOC_SUBSTRINGS[k]; k++) {
            WCUP_LOOP_TICK();
            if (strstr(flat, IOC_SUBSTRINGS[k])) { ioc_hit = k; break; }
        }
        WCUP_BRANCH(ioc_hit >= 0);                             /* B1 */
        int n_issues = 0;
        if (ioc_hit >= 0) {
            snprintf(vbuf, sizeof vbuf,
                     "MCP server '%s' contains a known hermes-0day "
                     "indicator-of-compromise ('%s')", name, IOC_SUBSTRINGS[ioc_hit]);
            n_issues = 1;
        } else {
            char basename[256];
            command_basename(cmd_str, basename, sizeof basename);
            bool is_shell = false;
            for (int k = 0; SHELL_INTERPRETERS[k]; k++)
                if (strcmp(basename, SHELL_INTERPRETERS[k]) == 0) { is_shell = true; break; }
            WCUP_BRANCH(is_shell);                             /* B2 */
            if (is_shell) {
                WCUP_BRANCH(args_str[0] != '\0');              /* B3 */
                if (args_str[0]) {
                    int egress = re_search2(EGRESS_PATTERN, args_str);
                    WCUP_BRANCH(egress);                       /* B4 */
                    if (egress) {
                        int hint = re_search2(EXFIL_HINT_PATTERN, args_str);
                        WCUP_BRANCH(hint);                     /* B5 */
                        n_issues++;
                        if (n_issues == 1)
                            snprintf(vbuf, sizeof vbuf,
                                "MCP server '%s' uses shell interpreter '%s' with network "
                                "egress in args%s", name, cmd_str,
                                hint ? " and exfiltration-shaped arguments" : "");
                    }
                    int persist = re_search2(PERSISTENCE_PATTERN, args_str);
                    WCUP_BRANCH(persist);                      /* B6 */
                    if (persist) {
                        n_issues++;
                        if (n_issues == 1)
                            snprintf(vbuf, sizeof vbuf,
                                "MCP server '%s' uses shell interpreter '%s' to write "
                                "to an OS persistence surface (SSH keys / PAM / sudoers / cron / "
                                "shell rc) — this is the hermes-0day backdoor shape, not a real "
                                "MCP server", name, cmd_str);
                        else {
                            strncat(vbuf, ";MCP server '", sizeof vbuf - strlen(vbuf) - 200);
                            char tail[512];
                            snprintf(tail, sizeof tail,
                                "%s' uses shell interpreter '%s' to write "
                                "to an OS persistence surface (SSH keys / PAM / sudoers / cron / "
                                "shell rc) — this is the hermes-0day backdoor shape, not a real "
                                "MCP server", name, cmd_str);
                            strncat(vbuf, tail, sizeof vbuf - strlen(vbuf) - 1);
                        }
                    }
                }
            }
        }

        /* ---- honesty check vs the REAL port ---- */
        json_t *real = hermes_cli_mcp_security_validate_mcp_server_entry(name, entry);
        char realsep[8192]; realsep[0] = '\0';
        if (real && real->type == JSON_ARRAY)
            for (size_t k = 0; k < real->c.count; k++) {
                if (k) strncat(realsep, ";", sizeof realsep - strlen(realsep) - 1);
                json_t *it = real->c.items[k];
                strncat(realsep, (it && it->type == JSON_STRING && it->str_val) ? it->str_val : "",
                        sizeof realsep - strlen(realsep) - 1);
            }
        if (strcmp(realsep, vbuf) != 0) {
            fprintf(stderr, "TRACED LADDER DIVERGES from real port:\n traced=%s\n real  =%s\n",
                    vbuf, realsep);
            return 3;
        }

        WCUP_VALSTR(vbuf);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("validate_mcp_server_entry");
        free(flat); free(args_str); free(env_str); free(inp);
        if (real) json_free(real);
        if (entry) json_free(entry);
    }
    return 0;
}
