/*
 * browser_session_cdp.c — C home for upstream Hermes tools/browser_session_cdp.py.
 * PoP: w12_browserses_e75381_resolve_cdp_override @ tools/browser_session_cdp.py:resolve_cdp_override
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w12_browserses_e75381.h"

static char *w12_strip(const char *s) {
    char *out; size_t n;
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *w12_rstrip_char(const char *s, char c) {
    size_t n = strlen(s);
    char *out;
    while (n && s[n - 1] == c) n--;
    out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *w12_lower_dup(const char *s) {
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

char *w12_browserses_e75381_resolve_cdp_override(const char *cdp_url,
                                                  const struct json_t *discovery_payload) {
    char *raw = w12_strip(cdp_url ? cdp_url : "");
    char *low;
    char *result;
    if (!raw) return strdup("");
    if (!*raw) { free(raw); return strdup(""); }

    low = w12_lower_dup(raw);
    /* Full browser endpoints pass straight through. */
    if (strstr(low, "/devtools/browser/")) { free(low); return raw; }

    /* Bare ws://host:port endpoints resolve via /json/version; anything else is
     * not host:port (or has a path) and passes through untouched. */
    if (!strncmp(low, "ws://", 5) || !strncmp(low, "wss://", 6)) {
        int colons = 0, port_ok = 0, no_path = 0;
        const char *p;
        for (p = raw; *p; p++) if (*p == ':') colons++;
        {
            char *rs = w12_rstrip_char(raw, '/');
            const char *last = strrchr(rs, ':');
            if (last && last[1]) {
                const char *q;
                port_ok = 1;
                for (q = last + 1; *q; q++) {
                    if (!isdigit((unsigned char)*q)) { port_ok = 0; break; }
                }
            }
            free(rs);
        }
        {
            const char *c1 = strchr(raw, ':');
            const char *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
            no_path = (c2 && !strchr(c2 + 1, '/')) ? 1 : 0;
        }
        if (!(colons == 2 && port_ok && no_path)) { free(low); return raw; }
    }
    free(low);

    /* Build the discovery root and version URL (the payload is injected, so the
     * version URL is computed only to mirror the live function's control flow). */
    {
        const char *scheme = (!strncmp(raw, "ws://", 5) || !strncmp(raw, "WSS://", 5) ||
                              !strncmp(raw, "Wss://", 5))
                             ? "http://" : "https://";
        /* scheme decided from the already-lowercased original: recompute safely */
        char *rawlow = w12_lower_dup(raw);
        const char *after = strstr(raw, "://");
        size_t need;
        char *discovery_url, *dlow, *version_url;
        if (!strncmp(rawlow, "ws://", 5)) scheme = "http://";
        else if (!strncmp(rawlow, "wss://", 6)) scheme = "https://";
        free(rawlow);
        if (!after) after = raw; else after += 3;
        need = strlen(scheme) + strlen(after) + 1;
        discovery_url = (char *)malloc(need);
        if (discovery_url) snprintf(discovery_url, need, "%s%s", scheme, after);
        else discovery_url = strdup("");
        dlow = w12_lower_dup(discovery_url);
        if (strlen(dlow) >= 13 && !strcmp(dlow + strlen(dlow) - 13, "/json/version")) {
            version_url = strdup(discovery_url);
        } else {
            char *rs = w12_rstrip_char(discovery_url, '/');
            size_t vneed = strlen(rs) + 14;
            version_url = (char *)malloc(vneed);
            if (version_url) snprintf(version_url, vneed, "%s/json/version", rs);
            else version_url = strdup("");
            free(rs);
        }
        (void)version_url;
        free(version_url);
        free(dlow);
        free(discovery_url);
    }

    /* Discovery response payload (null => request failed): fall back to raw. */
    if (discovery_payload && discovery_payload->type == JSON_OBJECT) {
        json_t *w = json_obj_get(discovery_payload, "webSocketDebuggerUrl");
        if (w && w->type == JSON_STRING) {
            char *ws = w12_strip(w->str_val ? w->str_val : "");
            if (ws && *ws) { free(raw); return ws; }
            free(ws);
        }
    }
    result = raw;
    return result;
}
