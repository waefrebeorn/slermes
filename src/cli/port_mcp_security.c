/* Slermes C port — hermes_cli/mcp_security.py (pure IOC-scan helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/* PoP: hermes_cli_mcp_security__entry_text @ hermes_cli/mcp_security.py:_entry_text
 * entry is passed as "command\x1fargs\x1fenv1\x1fenv2..." (unit sep between fields). */
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
