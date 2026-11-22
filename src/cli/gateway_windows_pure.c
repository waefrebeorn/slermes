/* gateway_windows_pure.c — C11 ports of the pure string helpers in
 * hermes_cli/gateway_windows.py.  These are the leaf primitives shared by
 * every Windows launcher/task renderer: path normalisation, cmd/VBS quoting,
 * Windows filename sanitising, the gateway argv builder and the two schtasks
 * failure classifiers.  All return freshly allocated json_t values; raises
 * are represented as {"__raise__": <type>, "message": <str>}.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static json_t *gwp_raise(const char *kind, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", kind);
    json_set_hdr(e, "message", message);
    return e;
}

/* PoP: _normalize_windows_path @ hermes_cli/gateway_windows.py */
json_t *gwp_normalize_windows_path(const char *value) {
    if (!value) value = "";
    size_t n = strlen(value);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)value[a])) a++;
    while (b > a && isspace((unsigned char)value[b - 1])) b--;
    while (a < b && value[a] == '"') a++;
    char *out = malloc(n + 1);
    size_t k = 0;
    for (size_t i = a; i < b; i++) {
        char ch = value[i];
        out[k++] = (ch == '\\') ? '/' : (char)tolower((unsigned char)ch);
    }
    while (k > 0 && out[k - 1] == '/') k--;  /* rstrip("/") after the replace */
    out[k] = '\0';
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* Python repr() for the small string set used in ValueError messages. */
static char *gw_repr(const char *value) {
    size_t cap = strlen(value) * 4 + 3;
    char *out = malloc(cap);
    size_t k = 0;
    char quote = (strchr(value, '\'') && !strchr(value, '"')) ? '"' : '\'';
    out[k++] = quote;
    for (const char *p = value; *p; p++) {
        if (*p == '\n') { memcpy(out + k, "\\n", 2); k += 2; }
        else if (*p == '\r') { memcpy(out + k, "\\r", 2); k += 2; }
        else if (*p == '\t') { memcpy(out + k, "\\t", 2); k += 2; }
        else if (*p == '\\' || *p == quote) { out[k++] = '\\'; out[k++] = *p; }
        else out[k++] = *p;
    }
    out[k++] = quote;
    out[k] = '\0';
    return out;
}

static json_t *gw_reject_newline(const char *value, const char *label) {
    if (!value || (!strchr(value, '\n') && !strchr(value, '\r'))) return NULL;
    char *repr = gw_repr(value ? value : "");
    size_t cap = strlen(label) + strlen(repr) + 48;
    char *msg = malloc(cap);
    snprintf(msg, cap, "refusing to quote %s containing newline: %s", label, repr);
    json_t *e = gwp_raise("ValueError", msg);
    free(msg); free(repr);
    return e;
}

/* PoP: _quote_cmd_script_arg @ hermes_cli/gateway_windows.py */
json_t *gwp_quote_cmd_script_arg(const char *value) {
    json_t *bad = gw_reject_newline(value, "value");
    if (bad) return bad;
    if (!value || !*value) return json_string("\"\"");
    if (!strpbrk(value, " \t\"")) return json_string(value);
    size_t n = strlen(value);
    char *out = malloc(n * 2 + 3);
    size_t k = 0;
    out[k++] = '"';
    for (const char *p = value; *p; p++) {
        if (*p == '"') out[k++] = '"';
        out[k++] = *p;
    }
    out[k++] = '"'; out[k] = '\0';
    json_t *r = json_string(out); free(out); return r;
}

/* PoP: _quote_vbs_string @ hermes_cli/gateway_windows.py */
json_t *gwp_quote_vbs_string(const char *value) {
    json_t *bad = gw_reject_newline(value, "VBScript value");
    if (bad) return bad;
    const char *s = value ? value : "";
    size_t n = strlen(s);
    char *out = malloc(n * 2 + 3);
    size_t k = 0;
    out[k++] = '"';
    for (const char *p = s; *p; p++) {
        if (*p == '"') out[k++] = '"';
        out[k++] = *p;
    }
    out[k++] = '"'; out[k] = '\0';
    json_t *r = json_string(out); free(out); return r;
}

/* PoP: _sanitize_filename @ hermes_cli/gateway_windows.py */
json_t *gwp_sanitize_filename(const char *value) {
    const char *s = value ? value : "";
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    size_t k = 0;
    for (const char *p = s; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        out[k++] = (ch < 0x20 || strchr("<>:\"/\\|?*", *p)) ? '_' : *p;
    }
    out[k] = '\0';
    json_t *r = json_string(out); free(out); return r;
}

/* PoP: _gateway_run_argv @ hermes_cli/gateway_windows.py */
json_t *gwp_gateway_run_argv(const char *python_exe, const char *profile_arg) {
    json_t *a = json_array();
    json_append(a, json_string(python_exe ? python_exe : ""));
    json_append(a, json_string("-m"));
    json_append(a, json_string("hermes_cli.main"));
    if (profile_arg && *profile_arg) {
        /* str.split(): whitespace runs, no empty fields */
        const char *p = profile_arg;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            const char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            char *word = malloc((size_t)(p - start) + 1);
            memcpy(word, start, (size_t)(p - start));
            word[p - start] = '\0';
            json_append(a, json_string(word));
            free(word);
        }
    }
    json_append(a, json_string("gateway"));
    json_append(a, json_string("run"));
    return a;
}

/* ASCII case-insensitive substring search (Python re.IGNORECASE subset used
 * by the schtasks patterns). */
static unsigned char gw_lower_ascii(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + 32) : c;
}
static int gw_contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    while (*hay) {
        const char *h = hay, *n = needle;
        while (*h && *n && gw_lower_ascii((unsigned char)*h) == gw_lower_ascii((unsigned char)*n)) { h++; n++; }
        if (!*n) return 1;
        hay++;
    }
    return !*needle;
}

static const char *gw_denied_words[] = {
    "access is denied", "acceso denegado", "p\u0159\u00edstup byl odep\u0159en",
    "\u62d2\u7edd\u8bbf\u95ee", "\u62d2\u7d55\u5b58\u53d6", "\u30a2\u30af\u30bb\u30b9\u304c\u62d2\u5426\u3055\u308c\u307e\u3057\u305f",
    "\uc561\uc138\uc2a4\uac00 \uac70\ubd80\ub418\uc5c8\uc2b5\ub2c8\ub2e4", NULL
};

/* PoP: _is_access_denied @ hermes_cli/gateway_windows.py */
json_t *gwp_is_access_denied(const char *detail) {
    if (!detail) detail = "";
    for (int i = 0; gw_denied_words[i]; i++)
        if (gw_contains_ci(detail, gw_denied_words[i])) return json_bool(1);
    return json_bool(0);
}

/* PoP: _should_fall_back @ hermes_cli/gateway_windows.py */
json_t *gwp_should_fall_back(long code, const char *detail) {
    if (code == 124) return json_bool(1);
    if (!detail) detail = "";
    if (gw_contains_ci(detail, "schtasks timed out") ||
        gw_contains_ci(detail, "schtasks produced no output"))
        return json_bool(1);
    return gwp_is_access_denied(detail);
}
