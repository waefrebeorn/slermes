/*
 * mcp_tool_content.c - C home for upstream Hermes tools/mcp_tool_content.py.
 *
 * PoP: w9_mcptoolcon_4e294e_is_reserved_meta_key @ tools/mcp_tool_content.py:_is_reserved_mcp_meta_key
 * PoP: w9_mcptoolcon_4e294e_strip_reserved_meta @ tools/mcp_tool_content.py:_strip_reserved_meta_keys
 * PoP: w9_mcptoolcon_4e294e_base_mime @ tools/mcp_tool_content.py:_base_mime
 * PoP: w9_mcptoolcon_4e294e_image_extension @ tools/mcp_tool_content.py:_mcp_image_extension_for_mime_type
 * PoP: w9_mcptoolcon_4e294e_resource_filename @ tools/mcp_tool_content.py:_mcp_resource_filename
 */
#include "noc_w9_mcptoolcon_4e294e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *to_lower(const char *s) {
    char *p = dupstr(s);
    for (char *q = p; q && *q; ++q) *q = (char)tolower((unsigned char)*q);
    return p;
}

static char *strip_ascii_ws(const char *s) {
    const unsigned char *a = (const unsigned char *)s;
    while (*a && isspace(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && isspace(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

int w9_mcptoolcon_4e294e_is_reserved_meta_key(const char *key) {
    if (!key) return 0;
    const char *slash = strchr(key, '/');
    if (!slash || slash == key) return 0;
    size_t n = (size_t)(slash - key);
    char *prefix = (char *)malloc(n + 1);
    if (!prefix) return 0;
    memcpy(prefix, key, n);
    prefix[n] = '\0';
    char **labels = (char **)malloc((n + 2) * sizeof(char *));
    size_t cnt = 0;
    char *cur = prefix;
    while (1) {
        char *dot = strchr(cur, '.');
        if (dot) { *dot = '\0'; labels[cnt++] = cur; cur = dot + 1; }
        else { labels[cnt++] = cur; break; }
    }
    int found = 0;
    for (size_t i = 0; i < cnt; ++i) {
        if ((!strcmp(labels[i], "mcp") || !strcmp(labels[i], "modelcontextprotocol")) && i < cnt - 1) {
            found = 1;
            break;
        }
    }
    free(labels); free(prefix);
    return found;
}

struct json_t *w9_mcptoolcon_4e294e_strip_reserved_meta(const struct json_t *meta) {
    if (!meta || meta->type != JSON_OBJECT) return json_null();
    struct json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(meta); ++i) {
        const char *k = json_object_get_key_at(meta, i);
        struct json_t *v = json_object_get_at(meta, i);
        if (k && !w9_mcptoolcon_4e294e_is_reserved_meta_key(k)) json_set(out, k, json_copy(v));
    }
    if (json_object_size(out) == 0) { json_free(out); return json_null(); }
    return out;
}

char *w9_mcptoolcon_4e294e_base_mime(const struct json_t *mime) {
    const char *s = (mime && mime->type == JSON_STRING && mime->str_val) ? mime->str_val : "";
    const char *semi = strchr(s, ';');
    size_t n = semi ? (size_t)(semi - s) : strlen(s);
    char *piece = (char *)malloc(n + 1);
    if (!piece) return dupstr("");
    memcpy(piece, s, n); piece[n] = '\0';
    char *t = strip_ascii_ws(piece);
    char *low = to_lower(t ? t : "");
    free(piece); free(t);
    return low;
}

char *w9_mcptoolcon_4e294e_image_extension(const struct json_t *mime, const char *ext_override) {
    char *norm = w9_mcptoolcon_4e294e_base_mime(mime);
    if (norm && (!strcmp(norm, "image/jpeg") || !strcmp(norm, "image/jpg"))) {
        free(norm);
        return dupstr(".jpg");
    }
    free(norm);
    if (ext_override && *ext_override) return dupstr(ext_override);
    return dupstr(".png");
}

/* Decode %XX in-place, return new string. */
static char *unquote_pct(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '%' && i + 2 < n && isxdigit((unsigned char)s[i+1]) && isxdigit((unsigned char)s[i+2])) {
            char hex[3] = {s[i+1], s[i+2], '\0'};
            out[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

char *w9_mcptoolcon_4e294e_resource_filename(const char *uri, const struct json_t *mime, const char *fallback_ext) {
    char *name = dupstr("");
    if (uri && *uri) {
        const char *path = uri;
        const char *proto = strstr(uri, "://");
        if (proto) {
            const char *after = proto + 3;
            const char *sl = strchr(after, '/');
            path = sl ? sl : "";
        }
        /* urlparse drops query and fragment. */
        char *p0 = dupstr(path);
        char *q = strchr(p0, '?'); if (q) *q = '\0';
        char *h = strchr(p0, '#'); if (h) *h = '\0';
        char *dec = unquote_pct(p0);
        /* basename */
        size_t n = strlen(dec);
        while (n > 0 && (dec[n-1] == '/' || dec[n-1] == '\\')) dec[--n] = '\0';
        size_t start = 0;
        for (size_t i = 0; i < n; ++i)
            if (dec[i] == '/' || dec[i] == '\\') start = i + 1;
        free(name);
        name = dupstr(dec + start);
        free(dec); free(p0);
    }
    /* strip control chars then whitespace */
    size_t n = strlen(name);
    char *clean = (char *)malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 0x20 && c != 0x7f) clean[j++] = name[i];
    }
    clean[j] = '\0';
    free(name);
    char *trimmed = strip_ascii_ws(clean);
    free(clean);
    name = trimmed;
    n = strlen(name);
    if (n > 150) {
        const char *dot = NULL;
        for (size_t i = n; i > 0; --i) if (name[i-1] == '.') { dot = name + i - 1; break; }
        size_t extlen = dot ? strlen(dot + 1) : 0;
        if (dot && dot != name && extlen > 0 && extlen <= 12) {
            size_t stemlen = (size_t)(dot - name);
            size_t keep = 150 - extlen - 1;
            if (stemlen > keep) stemlen = keep;
            char *tmp = (char *)malloc(stemlen + 1 + extlen + 1);
            if (tmp) {
                memcpy(tmp, name, stemlen);
                tmp[stemlen] = '.';
                memcpy(tmp + stemlen + 1, dot + 1, extlen);
                tmp[stemlen + 1 + extlen] = '\0';
                free(name); name = tmp;
            }
        } else {
            name[150] = '\0';
        }
    }
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) {
        const char *e = (fallback_ext && *fallback_ext) ? fallback_ext : ".bin";
        size_t nn = strlen("resource") + strlen(e) + 1;
        char *tmp = (char *)malloc(nn);
        if (tmp) { snprintf(tmp, nn, "resource%s", e); free(name); name = tmp; }
    }
    (void)mime; /* mime only feeds the injected mimetypes.guess_extension seam */
    return name;
}
