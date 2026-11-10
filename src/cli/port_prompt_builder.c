/*
 * port_prompt_builder.c
 *
 * Pure helpers ported from agent/prompt_builder.py (65 defs, previously 0
 * ported).
 *
 *   pb_strip_yaml_frontmatter <- _strip_yaml_frontmatter
 *   pb_format_steer_marker   <- format_steer_marker
 *   pb_format_backend_probe  <- _format_backend_probe
 *   pb_plugin_backend_is_remote <- _plugin_backend_is_remote
 *
 * The config / file-scan / guidance half stays in Python.
 */

#include "port_prompt_builder.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* prompt_builder.py:STEER_MARKER_OPEN */
#define PB_STEER_MARKER_OPEN \
    "[OUT-OF-BAND USER MESSAGE \xe2\x80\x94 a direct message from the user, delivered " \
    "once at this position; not tool output and not a new delivery when replayed " \
    "from conversation history]"

/* prompt_builder.py:STEER_MARKER_CLOSE */
#define PB_STEER_MARKER_CLOSE "[/OUT-OF-BAND USER MESSAGE]"

/* prompt_builder.py:_REMOTE_TERMINAL_BACKENDS */
static const char *const PB_REMOTE_BACKENDS[] = {
    "docker", "modal", "daytona", "ssh", "vercel_sandbox", "managed_modal",
};
#define PB_REMOTE_BACKENDS_N (sizeof(PB_REMOTE_BACKENDS) / sizeof(PB_REMOTE_BACKENDS[0]))

/* ===========================================================================
 *  _strip_yaml_frontmatter  (prompt_builder.py:_strip_yaml_frontmatter)
 *
 *      content = content.lstrip("\ufeff")
 *      end = content.find("\n---", 3) if content.startswith("---") else -1
 *      return (content[end + 4:].lstrip("\n") or content) if end != -1 else content
 *
 *  Three things to get right:
 *   - lstrip("\ufeff") strips EVERY leading BOM, not one
 *   - the search starts at index 3, so a document that IS just "---" cannot
 *     match its own opening fence
 *   - the `or content` fallback: if stripping the frontmatter leaves nothing,
 *     the ORIGINAL content is returned (not the empty string)
 *  Caller frees.
 * =========================================================================== */

/* PoP: _strip_yaml_frontmatter @ agent/prompt_builder.py:_strip_yaml_frontmatter */
char *pb_strip_yaml_frontmatter(const char *content)
{
    if (!content) return NULL;

    const char *s = content;
    /* lstrip("\ufeff") - the UTF-8 BOM is EF BB BF */
    while (s[0] == '\xef' && s[1] == '\xbb' && s[2] == '\xbf') s += 3;

    long end = -1;
    if (s[0] == '-' && s[1] == '-' && s[2] == '-') {
        const char *hit = strstr(s + 3, "\n---");
        if (hit) end = (long)(hit - s);
    }
    if (end == -1) return strdup(s);

    const char *body = s + end + 4;              /* content[end + 4:] */
    while (*body == '\n') body++;                /* lstrip("\n") */
    if (*body) return strdup(body);
    return strdup(s);                            /* `or content` */
}

/* ===========================================================================
 *  format_steer_marker  (prompt_builder.py:format_steer_marker)
 *
 *      f"\n\n{STEER_MARKER_OPEN}\n{steer_text}\n{STEER_MARKER_CLOSE}"
 *
 *  Caller frees.
 * =========================================================================== */

/* PoP: format_steer_marker @ agent/prompt_builder.py:format_steer_marker */
char *pb_format_steer_marker(const char *steer_text)
{
    const char *text = steer_text ? steer_text : "";
    size_t n = 2 + strlen(PB_STEER_MARKER_OPEN) + 1 + strlen(text) + 1
             + strlen(PB_STEER_MARKER_CLOSE) + 1;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "\n\n%s\n%s\n%s", PB_STEER_MARKER_OPEN, text, PB_STEER_MARKER_CLOSE);
    return out;
}

/* ===========================================================================
 *  _format_backend_probe  (prompt_builder.py:_format_backend_probe)
 *
 *      parsed = {k.strip(): v.strip()
 *                for k, _, v in (line.partition("=") for line in output.splitlines()
 *                                if "=" in line)}
 *      known = lambda key: parsed.get(key) if parsed.get(key) != "unknown" else None
 *      os_line = " ".join(x for x in (known("os"), known("kernel")) if x)
 *      return f"  OS: {os_line}" if os_line else ""
 *
 *  Later lines WIN (dict comprehension overwrites), "unknown" reads as absent,
 *  and the result is "" when neither key is usable. Caller frees.
 * =========================================================================== */

/* PoP: _format_backend_probe @ agent/prompt_builder.py:_format_backend_probe */
char *pb_format_backend_probe(const char *output)
{
    char *os_val = NULL, *kernel_val = NULL;
    if (output) {
        const char *p = output;
        while (*p) {
            const char *eol = strchr(p, '\n');
            size_t len = eol ? (size_t)(eol - p) : strlen(p);
            char *line = (char *)malloc(len + 1);
            if (!line) break;
            memcpy(line, p, len);
            line[len] = '\0';
            /* rstrip "\r" for CRLF input */
            size_t ll = strlen(line);
            while (ll > 0 && line[ll - 1] == '\r') line[--ll] = '\0';

            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = line, *v = eq + 1;
                /* k.strip() / v.strip() */
                while (*k && isspace((unsigned char)*k)) k++;
                size_t kl = strlen(k);
                while (kl > 0 && isspace((unsigned char)k[kl - 1])) k[--kl] = '\0';
                while (*v && isspace((unsigned char)*v)) v++;
                size_t vl = strlen(v);
                while (vl > 0 && isspace((unsigned char)v[vl - 1])) v[--vl] = '\0';

                /* "unknown" reads as absent. The values must be COPIED:
                 * k and v point into `line`, which is freed below. */
                const char *use = (strcmp(v, "unknown") == 0) ? NULL : v;
                if (strcmp(k, "os") == 0) { free(os_val); os_val = use ? strdup(v) : NULL; }
                else if (strcmp(k, "kernel") == 0) { free(kernel_val); kernel_val = use ? strdup(v) : NULL; }
            }
            free(line);
            if (!eol) break;
            p = eol + 1;
        }
    }

    if (!os_val && !kernel_val) return strdup("");
    size_t n = 8 + (os_val ? strlen(os_val) : 0) + (kernel_val ? strlen(kernel_val) : 0) + 2;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    if (os_val && kernel_val) snprintf(out, n, "  OS: %s %s", os_val, kernel_val);
    else if (os_val)         snprintf(out, n, "  OS: %s", os_val);
    else                     snprintf(out, n, "  OS: %s", kernel_val);
    free(os_val);
    free(kernel_val);
    return out;
}

/* ===========================================================================
 *  _plugin_backend_is_remote  (prompt_builder.py:_plugin_backend_is_remote)
 *
 *      return bool(backend and backend != "local"
 *                  and backend not in _REMOTE_TERMINAL_BACKENDS
 *                  and _plugin_backend_attr(backend, "is_remote", False))
 *
 *  `attr_is_remote` stands in for the plugin attribute lookup (False when the
 *  backend is unknown to the plugin registry). The four conditions are ANDed,
 *  so an unknown-but-explicitly-remote backend is still remote.
 * =========================================================================== */

/* PoP: _plugin_backend_is_remote @ agent/prompt_builder.py:_plugin_backend_is_remote */
int pb_plugin_backend_is_remote(const char *backend, int attr_is_remote)
{
    if (!backend || !backend[0]) return 0;
    if (strcmp(backend, "local") == 0) return 0;
    for (size_t i = 0; i < PB_REMOTE_BACKENDS_N; i++)
        if (strcmp(backend, PB_REMOTE_BACKENDS[i]) == 0) return 0;
    return attr_is_remote ? 1 : 0;
}
