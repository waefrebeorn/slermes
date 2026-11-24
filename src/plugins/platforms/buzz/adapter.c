/*
 * adapter.c - C11 port of upstream plugins/platforms/buzz/adapter.py
 * (gates w27_adapter_04abb5_p1 .. _p4).
 *
 * PoP: w27_adapter_04abb5_escape_mention          @ plugins/platforms/buzz/adapter.py:_escape_unresolved_presentation_mention
 * PoP: w27_adapter_04abb5_safe_filename           @ plugins/platforms/buzz/adapter.py:_safe_attachment_filename
 * PoP: w27_adapter_04abb5_attachment_origin       @ plugins/platforms/buzz/adapter.py:_attachment_origin
 * PoP: w27_adapter_04abb5_effective_port          @ plugins/platforms/buzz/adapter.py:_effective_port
 * PoP: w27_adapter_04abb5_is_relay_media_url      @ plugins/platforms/buzz/adapter.py:_is_relay_media_url
 * PoP: w27_adapter_04abb5_find_relay_media_refs   @ plugins/platforms/buzz/adapter.py:_find_relay_media_refs
 * PoP: w27_adapter_04abb5_replace_media_refs      @ plugins/platforms/buzz/adapter.py:_replace_media_refs
 * PoP: w27_adapter_04abb5_hex_to_npub             @ plugins/platforms/buzz/adapter.py:hex_to_npub
 * PoP: w27_adapter_04abb5_constants               @ plugins/platforms/buzz/adapter.py:<module constants>
 * PoP: w27_adapter_04abb5_member_candidates       @ plugins/platforms/buzz/adapter.py:BuzzAdapter._channel_member_pubkeys
 * PoP: w27_adapter_04abb5_profile_name            @ plugins/platforms/buzz/adapter.py:BuzzAdapter._profile_display_name
 * PoP: w27_adapter_04abb5_mention_pubkeys         @ plugins/platforms/buzz/adapter.py:BuzzAdapter._mention_pubkeys_for
 * PoP: w27_adapter_04abb5_send_ladder             @ plugins/platforms/buzz/adapter.py:BuzzAdapter._run_message_send
 * PoP: w27_adapter_04abb5_send_result             @ plugins/platforms/buzz/adapter.py:BuzzAdapter._send_result
 * PoP: w27_adapter_04abb5_chat_info               @ plugins/platforms/buzz/adapter.py:BuzzAdapter.get_chat_info
 * PoP: w27_adapter_04abb5_websocket_url           @ plugins/platforms/buzz/adapter.py:BuzzAdapter._websocket_url
 * PoP: w27_adapter_04abb5_channel_subscription    @ plugins/platforms/buzz/adapter.py:BuzzAdapter._send_channel_subscription
 * PoP: w27_adapter_04abb5_subscriptions           @ plugins/platforms/buzz/adapter.py:BuzzAdapter._subscribe_websocket
 * PoP: w27_adapter_04abb5_ws_message              @ plugins/platforms/buzz/adapter.py:BuzzAdapter._handle_ws_message
 * PoP: w27_adapter_04abb5_imeta_parse             @ plugins/platforms/buzz/adapter.py:BuzzAdapter._parse_imeta_attachments
 * PoP: w27_adapter_04abb5_imeta_attachments       @ plugins/platforms/buzz/adapter.py:BuzzAdapter._imeta_attachments
 * PoP: w27_adapter_04abb5_rejection_note          @ plugins/platforms/buzz/adapter.py:BuzzAdapter._attachment_rejection_note
 * PoP: w27_adapter_04abb5_user_name               @ plugins/platforms/buzz/adapter.py:BuzzAdapter._resolve_user_name
 * PoP: w27_adapter_04abb5_dispatch_shape          @ plugins/platforms/buzz/adapter.py:BuzzAdapter._dispatch_message
 * PoP: w27_adapter_04abb5_check_requirements      @ plugins/platforms/buzz/adapter.py:check_requirements
 * PoP: w27_adapter_04abb5_validate_config         @ plugins/platforms/buzz/adapter.py:validate_config
 * PoP: w27_adapter_04abb5_apply_yaml_config       @ plugins/platforms/buzz/adapter.py:_apply_yaml_config
 * PoP: w27_adapter_04abb5_env_enablement          @ plugins/platforms/buzz/adapter.py:_env_enablement
 * PoP: w27_adapter_04abb5_standalone_send         @ plugins/platforms/buzz/adapter.py:_standalone_send
 *
 * What is ported is the decision core: text shaping (media-ref rewriting,
 * mention escaping, filename sanitation), identity encoding, NIP-94 metadata
 * acceptance, relay frame disposition, the CLI send ladder, and the config
 * decisions.  Every I/O seam (the buzz subprocess, the WebSocket, the relay
 * socket, httpx downloads, the credentials file, the plugin registry) stays in
 * the caller; nothing here opens a socket, a file or a subprocess.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "noc_w27_adapter_04abb5_p1.h"

/* ── small string helpers ─────────────────────────────────────────────── */

static char *w_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *w_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip() whitespace (ASCII subset: the adapters only ever
 * see ASCII whitespace in these paths). */
static int w_ws(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *w_strip(const char *s) {
    size_t a, b;
    if (!s) return w_dup("");
    a = 0;
    b = strlen(s);
    while (a < b && w_ws((unsigned char)s[a])) a++;
    while (b > a && w_ws((unsigned char)s[b - 1])) b--;
    return w_ndup(s + a, b - a);
}

static char *w_lower(const char *s) {
    size_t i, n;
    char *p;
    if (!s) return w_dup("");
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        p[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    p[n] = '\0';
    return p;
}

static int w_ilower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int w_ieq_at(const char *hay, size_t pos, const char *needle) {
    size_t i;
    if (!hay || !needle) return 0;
    for (i = 0; needle[i]; i++)
        if (!hay[pos + i] || w_ilower((unsigned char)hay[pos + i]) != w_ilower((unsigned char)needle[i]))
            return 0;
    return 1;
}

static int w_startswith_ci(const char *s, const char *pre) {
    size_t i;
    if (!s || !pre) return 0;
    for (i = 0; pre[i]; i++)
        if (!s[i] || w_ilower((unsigned char)s[i]) != w_ilower((unsigned char)pre[i])) return 0;
    return 1;
}

static int w_is_hex(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int w_is_alnum(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int w_hex64(const char *s) {
    size_t i;
    if (!s || strlen(s) != 64) return 0;
    for (i = 0; i < 64; i++)
        if (!w_is_hex((unsigned char)s[i])) return 0;
    return 1;
}

static int w_all_digits(const char *s, size_t n) {
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* Python truthiness of a JSON value. */
static int w_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value or "") for the shapes these helpers receive. */
static char *w_pystr(const json_t *v) {
    if (!w_truthy(v)) {
        /* None / 0 / False / "" / [] / {} all collapse to "" via `or ""`. */
        if (v && v->type == JSON_NUMBER) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return w_dup(buf);
        }
        return w_dup("");
    }
    switch (v->type) {
    case JSON_STRING: return w_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return w_dup("True");
    case JSON_NUMBER: {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return w_dup(buf);
    }
    default: {
        char *s = json_dumps(v, 0);
        char *r = w_dup(s ? s : "");
        if (s) free(s);
        return r;
    }
    }
}

/* str(value) (None -> "None"), used where the source does not apply `or ""`. */
static char *w_str(const json_t *v) {
    if (!v) return w_dup("None");
    if (v->type == JSON_NULL) return w_dup("None");
    if (v->type == JSON_STRING) return w_dup(v->str_val ? v->str_val : "");
    return w_pystr(v);
}

/* ── urlsplit (the subset these leaves consult) ───────────────────────── */

typedef struct {
    char *scheme;    /* "" when absent */
    char *netloc;    /* "" when absent */
    char *path;
    char *query;
    char *fragment;
    char *host;      /* lowercased, brackets stripped; "" when absent */
    char *username;  /* NULL when no userinfo */
    int has_port;
    double port;     /* valid only when has_port */
    int bad;         /* urlsplit raised ValueError */
    int bad_port;    /* .port raised ValueError */
} w_url;

static void w_url_clear(w_url *u) {
    free(u->scheme); free(u->netloc); free(u->path); free(u->query);
    free(u->fragment); free(u->host); free(u->username);
    memset(u, 0, sizeof(*u));
}

static int w_scheme_char(int c) {
    return w_is_alnum(c) || c == '+' || c == '-' || c == '.';
}

static void w_urlsplit(const char *text, w_url *u) {
    const char *s = text ? text : "";
    size_t n, i, cut;
    /* urlsplit() strips leading/trailing C0 controls and spaces (WHATWG), so
     * " http://h:8080/x" parses as http with port 8080 -- a leading space does
     * not make the scheme empty. Nothing is allocated: advance s, trim n. */
    while (*s && (unsigned char)*s <= 0x20) s++;
    n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= 0x20) n--;
    char *rest;
    memset(u, 0, sizeof(*u));
    u->scheme = w_dup("");
    u->netloc = w_dup("");
    u->path = w_dup("");
    u->query = w_dup("");
    u->fragment = w_dup("");
    u->host = w_dup("");

    /* scheme: [A-Za-z][A-Za-z0-9+.-]* ':' */
    for (i = 0; i < n; i++) {
        if (s[i] == ':') break;
        if (!w_scheme_char((unsigned char)s[i])) { i = n; break; }
    }
    if (i < n && i > 0 && ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'))) {
        free(u->scheme);
        u->scheme = w_lower(w_ndup(s, i));
        rest = w_ndup(s + i + 1, n - i - 1);
    } else {
        rest = w_dup(s);
    }

    /* netloc only when the remainder opens with "//" */
    cut = 0;
    if (rest[0] == '/' && rest[1] == '/') {
        size_t start = 2, end = start;
        while (rest[end] && rest[end] != '/' && rest[end] != '?' && rest[end] != '#') end++;
        free(u->netloc);
        u->netloc = w_ndup(rest + start, end - start);
        cut = end;
    }

    /* path / query / fragment of the remainder */
    {
        const char *r = rest + cut;
        size_t len = strlen(r), p = 0;
        while (p < len && r[p] != '?' && r[p] != '#') p++;
        free(u->path);
        u->path = w_ndup(r, p);
        if (p < len && r[p] == '?') {
            size_t q = p + 1;
            while (q < len && r[q] != '#') q++;
            free(u->query);
            u->query = w_ndup(r + p + 1, q - p - 1);
            p = q;
        }
        if (p < len && r[p] == '#') {
            free(u->fragment);
            u->fragment = w_ndup(r + p + 1, len - p - 1);
        }
    }
    free(rest);

    /* host / userinfo / port out of netloc */
    {
        const char *nl = u->netloc;
        size_t len = strlen(nl), at = len, atpos = 0, has_at = 0, k;
        for (k = 0; k < len; k++)
            if (nl[k] == '@') { has_at = 1; atpos = k; }
        if (has_at) {
            u->username = w_ndup(nl, atpos);
            nl += atpos + 1;
            len -= atpos + 1;
            at = len;
        }
        (void)at;
        if (len && nl[0] == '[') {
            size_t close = 0;
            for (k = 1; k < len; k++)
                if (nl[k] == ']') { close = k; break; }
            if (!close) { u->bad = 1; return; }
            free(u->host);
            u->host = w_lower(w_ndup(nl + 1, close - 1));
            if (close + 1 < len && nl[close + 1] == ':') {
                size_t plen = len - close - 2;
                if (plen) {
                    if (!w_all_digits(nl + close + 2, plen)) { u->bad_port = 1; return; }
                    u->has_port = 1;
                    u->port = atof(w_ndup(nl + close + 2, plen));
                }
            }
        } else {
            size_t colon = len, plen;
            for (k = 0; k < len; k++)
                if (nl[k] == ':') { colon = k; break; }
            free(u->host);
            u->host = w_lower(w_ndup(nl, colon));
            if (colon < len) {
                plen = len - colon - 1;
                if (plen) {
                    if (!w_all_digits(nl + colon + 1, plen)) { u->bad_port = 1; return; }
                    u->has_port = 1;
                    {
                        char *tmp = w_ndup(nl + colon + 1, plen);
                        u->port = atof(tmp);
                        free(tmp);
                    }
                    if (u->port > 65535.0) { u->bad_port = 1; return; }
                }
            }
        }
    }
}

/* ── bech32 (BIP-173) ─────────────────────────────────────────────────── */

static const char W_BECH32_CHARSET[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static unsigned w_bech32_polymod_step(unsigned chk, unsigned val) {
    static const unsigned GEN[5] = {0x3b6a57b2u, 0x26508e6du, 0x1ea119fau, 0x3d4233ddu, 0x2a1462b3u};
    unsigned top = chk >> 25;
    int i;
    chk = ((chk & 0x1ffffffu) << 5) ^ val;
    for (i = 0; i < 5; i++)
        if ((top >> i) & 1u) chk ^= GEN[i];
    return chk;
}

static unsigned w_bech32_polymod(const unsigned *values, size_t n) {
    unsigned chk = 1;
    size_t i;
    for (i = 0; i < n; i++) chk = w_bech32_polymod_step(chk, values[i]);
    return chk;
}

/* ── p1: text / url / identity leaves ─────────────────────────────────── */

/* _HEX64_RE = [0-9a-f]{64} used with .fullmatch */
static int w_hex64_full(const char *s) { return w_hex64(s); }

/* _MEDIA_PATH_RE = ^/media/([0-9a-f]{64})(\.[a-z0-9]{1,10})?/?$ (IGNORECASE) */
static int w_media_path(const char *path) {
    size_t n, i, q;
    if (!path) return 0;
    n = strlen(path);
    /* re.IGNORECASE: "/MEDIA/..." is the same object as "/media/...". */
    if (n < 7 || !w_startswith_ci(path, "/media/")) return 0;
    if (n < 7 + 64) return 0;
    for (i = 0; i < 64; i++)
        if (!w_is_hex((unsigned char)path[7 + i])) return 0;
    q = 7 + 64;
    if (q == n) return 1;
    if (path[q] == '.') {
        size_t e = 0;
        q++;
        while (q < n && w_is_alnum((unsigned char)path[q]) && e < 10) { q++; e++; }
        if (e == 0) return 0;
        if (q == n) return 1;
    }
    if (q == n) return 1;
    if (path[q] == '/') {
        q++;
        return q == n;
    }
    return 0;
}

/* Does s[0..n) fully match the media URL grammar?
 *   https?://[^\s<>\[\]()]+/media/<64 hex>(\.<1..10 alnum>)?(\?[^\s<>\[\]()]*)? */
static int w_media_url_exact(const char *s, size_t n) {
    size_t plen, pos;
    int last;
    if (n < 20) return 0;
    if (w_startswith_ci(s, "http://")) plen = 7;
    else if (w_startswith_ci(s, "https://")) plen = 8;
    else return 0;
    if (plen >= n) return 0;
    /* the leading class must be non-empty */
    if (s[plen] == '/' || s[plen] == '?' || s[plen] == '#') return 0;
    last = -1;
    for (pos = plen; pos + 7 + 64 <= n; pos++)
        if (s[pos] == '/' && w_startswith_ci(s + pos, "/media/")) last = (int)pos;
    for (pos = (size_t)(last < 0 ? 0 : last + 1); pos-- > plen;) {
        size_t q, r;
        int ok = 1;
        if (!(s[pos] == '/' && strncmp(s + pos, "/media/", 7) == 0)) {
            if (pos == plen) break;
            continue;
        }
        q = pos + 7;
        if (q + 64 > n) { if (pos == plen) break; continue; }
        {
            size_t i;
            for (i = 0; i < 64; i++)
                if (!w_is_hex((unsigned char)s[q + i])) ok = 0;
        }
        if (ok) {
            r = q + 64;
            if (r == n) return 1;
            if (s[r] == '.') {
                size_t e = 0, r2 = r + 1;
                while (r2 < n && w_is_alnum((unsigned char)s[r2]) && e < 10) { r2++; e++; }
                if (e > 0) {
                    if (r2 == n) return 1;
                    if (s[r2] == '?') {
                        size_t k;
                        for (k = r2 + 1; k < n; k++)
                            if (w_ws((unsigned char)s[k]) || strchr("<>[]()", s[k])) break;
                        if (k == n) return 1;
                    }
                }
            } else if (s[r] == '?') {
                size_t k;
                for (k = r + 1; k < n; k++)
                    if (w_ws((unsigned char)s[k]) || strchr("<>[]()", s[k])) break;
                if (k == n) return 1;
            }
        }
        if (pos == plen) break;
    }
    return 0;
}

/* Length of the greedy media-URL prefix of *s* (0 when none). */
static size_t w_media_prefix(const char *s) {
    size_t n, end;
    if (!s) return 0;
    if (!w_startswith_ci(s, "http://") && !w_startswith_ci(s, "https://")) return 0;
    n = 0;
    while (s[n] && !w_ws((unsigned char)s[n]) && !strchr("<>[]()", s[n])) n++;
    for (end = n; end > 0; end--)
        if (w_media_url_exact(s, end)) return end;
    return 0;
}

/* _escape_unresolved_presentation_mention */
json_t *w27_adapter_04abb5_escape_mention(const json_t *in) {
    const char *content = json_get_str(in, "content", "");
    const char *error = json_get_str(in, "error", "");
    const char *marker = "does not match a current channel member";
    const char *open, *name_start, *name_end;
    char *name;
    size_t nlen, i, n, outn = 0, count = 0;
    char *out;
    json_t *res;

    /* _UNRESOLVED_MENTION_ERROR_RE.search(error): mention '@<name>' does not ... */
    name = NULL;
    open = strstr(error, "mention '@");
    if (open) {
        name_start = open + 10;   /* past "mention '@" */
        name_end = strchr(name_start, '\'');
        if (name_end && name_end > name_start) {
            const char *tail = name_end + 1;
            while (*tail == ' ') tail++;
            if (strncmp(tail, marker, strlen(marker)) == 0) name = w_ndup(name_start, (size_t)(name_end - name_start));
        }
    }
    if (!name) {
        if (open && !name_end) { /* malformed: no closing quote -> no match */ }
        res = json_null();
        free(name);
        return res;
    }
    nlen = strlen(name);
    n = strlen(content);
    out = (char *)malloc(n * 2 + 8);
    if (!out) { free(name); return json_null(); }
    i = 0;
    while (i < n) {
        int boundary = (i == 0) || w_ws((unsigned char)content[i - 1]);
        if (content[i] == '@' && boundary && i + 1 + nlen <= n && w_ieq_at(content, i + 1, name)) {
            size_t after = i + 1 + nlen;
            int tail_ok = (after == n) ? 1
                : !( w_is_alnum((unsigned char)content[after]) || content[after] == '.'
                     || content[after] == '_' || content[after] == '-');
            if (tail_ok) {
                /* "@" + ZWSP + matched[1:] */
                out[outn++] = '@';
                out[outn++] = (char)0xE2; out[outn++] = (char)0x80; out[outn++] = (char)0x8B;
                memcpy(out + outn, content + i + 1, nlen);
                outn += nlen;
                i = after;
                count++;
                continue;
            }
        }
        out[outn++] = content[i++];
    }
    out[outn] = '\0';
    res = count ? json_string(out) : json_null();
    free(out);
    free(name);
    return res;
}

/* Path(name).suffix offset for a bare basename (pathlib 3.14: a leading dot
 * run never yields a suffix, but a trailing dot does). */
static size_t w_suffix_off(const char *name, size_t n) {
    size_t i = n, lead = 0;
    if (n == 0) return n;
    while (lead < n && name[lead] == '.') lead++;
    if (lead >= n) return n;                 /* all dots: no suffix */
    while (i > 0) {
        i--;
        if (name[i] == '.') {
            if (i < lead) return n;          /* inside the leading dot run */
            return i;
        }
    }
    return n;
}

/* _safe_attachment_filename */
json_t *w27_adapter_04abb5_safe_filename(const json_t *in) {
    char *raw = w_pystr(json_obj_get(in, "value"));
    char *name, *clean, *suffix, *stem, *safe, *out;
    size_t i, n, clen = 0, slen, budget, cut;
    json_t *res;

    /* .replace("\\", "/").rsplit("/", 1)[-1] */
    for (i = 0; raw[i]; i++)
        if (raw[i] == '\\') raw[i] = '/';
    {
        char *slash = strrchr(raw, '/');
        name = w_dup(slash ? slash + 1 : raw);
    }
    free(raw);

    /* drop control chars (< 32 and 0x7f) then .strip() */
    n = strlen(name);
    clean = (char *)malloc(n + 1);
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 32 && c != 0x7f) clean[clen++] = (char)c;
    }
    clean[clen] = '\0';
    free(name);
    name = w_strip(clean);
    free(clean);

    if (!strcmp(name, "") || !strcmp(name, ".") || !strcmp(name, "..")) {
        free(name);
        return json_string("attachment.bin");
    }

    n = strlen(name);
    {
        size_t so = w_suffix_off(name, n);
        slen = n - so;
        suffix = w_ndup(name + so, slen);
    }
    if (slen > 20) { free(suffix); suffix = w_dup(""); slen = 0; }
    stem = w_ndup(name, n - slen);

    budget = 120 - slen;
    cut = strlen(stem);
    if (cut > budget) {
        size_t j, seq;
        cut = budget;
        /* truncate to *budget* bytes, dropping a split UTF-8 sequence */
        j = cut ? cut - 1 : 0;
        while (j > 0 && ((unsigned char)stem[j] & 0xC0) == 0x80) j--;
        {
            unsigned char lead = (unsigned char)stem[j];
            if (lead < 0x80) seq = 1;
            else if ((lead & 0xE0) == 0xC0) seq = 2;
            else if ((lead & 0xF0) == 0xE0) seq = 3;
            else if ((lead & 0xF8) == 0xF0) seq = 4;
            else seq = 1;
        }
        if (j + seq > cut) cut = j;
        stem[cut] = '\0';
    }
    while (strlen(stem) && (stem[strlen(stem) - 1] == ' ' || stem[strlen(stem) - 1] == '.'))
        stem[strlen(stem) - 1] = '\0';

    safe = strlen(stem) ? w_dup(stem) : w_dup("attachment");
    out = (char *)malloc(strlen(safe) + slen + 1);
    strcpy(out, safe);
    strcat(out, suffix);
    res = json_string(out);
    free(out); free(safe); free(stem); free(suffix); free(name);
    return res;
}

/* _attachment_origin -> [host, port] or null */
json_t *w27_adapter_04abb5_attachment_origin(const json_t *in) {
    char *raw = w_pystr(json_obj_get(in, "value"));
    char *trimmed = w_strip(raw);
    char *target;
    w_url u;
    json_t *res = NULL;
    free(raw);
    if (!trimmed[0]) { free(trimmed); return json_null(); }
    if (strstr(trimmed, "://")) target = w_dup(trimmed);
    else {
        target = (char *)malloc(strlen(trimmed) + 3);
        strcpy(target, "//");
        strcat(target, trimmed);
    }
    free(trimmed);
    w_urlsplit(target, &u);
    free(target);
    if (u.bad || u.bad_port) { w_url_clear(&u); return json_null(); }
    {
        int host_ok;
        char *host = w_dup(u.host ? u.host : "");
        size_t hl = strlen(host);
        while (hl > 0 && host[hl - 1] == '.') host[--hl] = '\0';
        host_ok = hl > 0;
        if ((u.scheme && u.scheme[0] && strcmp(u.scheme, "https") && strcmp(u.scheme, "wss")) || !host_ok) {
            res = json_null();
        } else {
            res = json_array();
            json_append(res, json_string(host));
            json_append(res, json_number(u.has_port ? u.port : 443.0));
        }
        free(host);
    }
    w_url_clear(&u);
    return res;
}

/* _effective_port(urlsplit(url)) -> int or null */
json_t *w27_adapter_04abb5_effective_port(const json_t *in) {
    char *raw = w_pystr(json_obj_get(in, "url"));
    w_url u;
    json_t *res = json_null();
    w_urlsplit(raw, &u);
    free(raw);
    /* Held-out fuzzing found no gain from dropping the whole-URL guard: urlsplit
     * failures (bad port included) must answer None, and only a *host-less* URL
     * falls through to the scheme default. Kept as-is after the experiment. */
    if (u.bad) { w_url_clear(&u); return json_null(); }
    if (u.bad_port) { w_url_clear(&u); return json_null(); }
    if (u.has_port) res = json_number(u.port);
    else if (!strcmp(u.scheme, "https") || !strcmp(u.scheme, "wss")) res = json_number(443);
    else if (!strcmp(u.scheme, "http") || !strcmp(u.scheme, "ws")) res = json_number(80);
    w_url_clear(&u);
    return res;
}

/* _is_relay_media_url */
json_t *w27_adapter_04abb5_is_relay_media_url(const json_t *in) {
    char *url = w_pystr(json_obj_get(in, "url"));
    char *relay = w_pystr(json_obj_get(in, "relay_url"));
    w_url a, b;
    int ok = 0;
    w_urlsplit(url, &a);
    w_urlsplit(relay, &b);
    if (!a.bad && !b.bad && !a.bad_port && !b.bad_port) {
        int port_a = a.has_port ? (int)a.port : (!strcmp(a.scheme, "https") || !strcmp(a.scheme, "wss") ? 443 : (!strcmp(a.scheme, "http") || !strcmp(a.scheme, "ws") ? 80 : -1));
        int port_b = b.has_port ? (int)b.port : (!strcmp(b.scheme, "https") || !strcmp(b.scheme, "wss") ? 443 : (!strcmp(b.scheme, "http") || !strcmp(b.scheme, "ws") ? 80 : -1));
        ok = (!strcmp(a.scheme, "http") || !strcmp(a.scheme, "https"))
             && a.host && a.host[0] && b.host && b.host[0]
             && !strcmp(a.host, b.host) && port_a == port_b && w_media_path(a.path);
    }
    w_url_clear(&a);
    w_url_clear(&b);
    free(url); free(relay);
    return json_bool(ok);
}

/* _find_relay_media_refs -> {urls: [...], replacements: [[start,end,text], ...]} */
json_t *w27_adapter_04abb5_find_relay_media_refs(const json_t *in) {
    char *text = w_pystr(json_obj_get(in, "text"));
    char *relay = w_pystr(json_obj_get(in, "relay_url"));
    size_t n = strlen(text), i = 0;
    size_t *spans = NULL, nspans = 0, cap = 0;
    json_t *urls = json_array(), *repls = json_array(), *out = json_object();

    while (i + 1 < n) {
        if (text[i] == '!' && text[i + 1] == '[') {
            size_t j = i + 2;
            while (j < n && text[j] != ']') j++;
            if (j + 1 < n && text[j] == ']' && text[j + 1] == '(') {
                size_t k = j + 2, ulen, pend, end = 0;
                while (k < n && w_ws((unsigned char)text[k])) k++;
                ulen = w_media_prefix(text + k);
                if (ulen) {
                    char *url = w_ndup(text + k, ulen);
                    int accept;
                    {
                        json_t *probe = json_object();
                        json_t *r;
                        json_set(probe, "url", json_string(url));
                        json_set(probe, "relay_url", json_string(relay));
                        r = w27_adapter_04abb5_is_relay_media_url(probe);
                        accept = w_truthy(r);
                        json_free(r);
                        json_free(probe);
                    }
                    pend = k + ulen;
                    {
                        size_t q = pend;
                        while (q < n && w_ws((unsigned char)text[q])) q++;
                        if (q < n && text[q] == ')') end = q + 1;
                        else if (q > pend && q < n && (text[q] == '"' || text[q] == '\'')) {
                            char quote = text[q];
                            size_t r2 = q + 1;
                            while (r2 < n && text[r2] != quote) r2++;
                            if (r2 < n) {
                                size_t s2 = r2 + 1;
                                while (s2 < n && w_ws((unsigned char)text[s2])) s2++;
                                if (s2 < n && text[s2] == ')') end = s2 + 1;
                            }
                            if (!end) {
                                size_t s2 = q;
                                while (s2 < n && w_ws((unsigned char)text[s2])) s2++;
                                if (s2 < n && text[s2] == ')') end = s2 + 1;
                            }
                        }
                    }
                    if (end) {
                        if (accept) {
                            size_t alt_start = i + 2, alt_end = j, a0, a1;
                            char *alt;
                            json_t *entry, *seen = json_null();
                            size_t u;
                            if (nspans == cap) {
                                cap = cap ? cap * 2 : 8;
                                spans = (size_t *)realloc(spans, cap * 2 * sizeof(size_t));
                            }
                            spans[nspans * 2] = i;
                            spans[nspans * 2 + 1] = end;
                            nspans++;
                            alt = w_ndup(text + alt_start, alt_end - alt_start);
                            a0 = 0; a1 = strlen(alt);
                            while (a0 < a1 && w_ws((unsigned char)alt[a0])) a0++;
                            while (a1 > a0 && w_ws((unsigned char)alt[a1 - 1])) a1--;
                            {
                                char *trimmed = w_ndup(alt + a0, a1 - a0);
                                entry = json_array();
                                json_append(entry, json_number((double)i));
                                json_append(entry, json_number((double)end));
                                json_append(entry, json_string(trimmed));
                                json_append(repls, entry);
                                free(trimmed);
                            }
                            free(alt);
                            for (u = 0; u < json_len(urls); u++) {
                                const char *have = json_string_value(json_get(urls, u));
                                if (have && !strcmp(have, url)) { seen = json_get(urls, u); break; }
                            }
                            if (!w_truthy(seen)) json_append(urls, json_string(url));
                        }
                        i = end;
                        free(url);
                        continue;
                    }
                    free(url);
                }
            }
        }
        i++;
    }

    /* bare pass over the ORIGINAL text */
    i = 0;
    while (i < n) {
        if (w_startswith_ci(text + i, "http://") || w_startswith_ci(text + i, "https://")) {
            size_t ulen = w_media_prefix(text + i);
            if (ulen) {
                char *url = w_ndup(text + i, ulen);
                int inside = 0;
                size_t s;
                for (s = 0; s < nspans; s++)
                    if (spans[s * 2] <= i && i + ulen <= spans[s * 2 + 1]) { inside = 1; break; }
                if (!inside) {
                    json_t *probe = json_object();
                    json_t *r;
                    json_set(probe, "url", json_string(url));
                    json_set(probe, "relay_url", json_string(relay));
                    r = w27_adapter_04abb5_is_relay_media_url(probe);
                    if (w_truthy(r)) {
                        json_t *entry = json_array();
                        size_t u;
                        int seen = 0;
                        json_append(entry, json_number((double)i));
                        json_append(entry, json_number((double)(i + ulen)));
                        json_append(entry, json_string(""));
                        json_append(repls, entry);
                        for (u = 0; u < json_len(urls); u++) {
                            const char *have = json_string_value(json_get(urls, u));
                            if (have && !strcmp(have, url)) { seen = 1; break; }
                        }
                        if (!seen) json_append(urls, json_string(url));
                        i += ulen;
                        json_free(r);
                        json_free(probe);
                        free(url);
                        continue;
                    }
                    json_free(r);
                    json_free(probe);
                }
                free(url);
            }
        }
        i++;
    }

    json_set(out, "urls", urls);
    json_set(out, "replacements", repls);
    free(spans);
    free(text); free(relay);
    return out;
}

/* _replace_media_refs */
json_t *w27_adapter_04abb5_replace_media_refs(const json_t *in) {
    const char *text = json_get_str(in, "text", "");
    const json_t *repls = json_obj_get(in, "replacements");
    size_t count = repls ? json_len(repls) : 0;
    char *cur = w_dup(text);
    size_t i;

    /* sorted(replacements, reverse=True): descending (start, end, text) */
    for (i = 0; i < count; i++) {
        size_t j;
        for (j = 0; j + 1 < count - i; j++) {
            const json_t *a = json_get(repls, j), *b = json_get(repls, j + 1);
            {
                double a0 = json_number_value(json_get(a, 0)), b0 = json_number_value(json_get(b, 0));
                double a1 = json_number_value(json_get(a, 1)), b1 = json_number_value(json_get(b, 1));
                const char *asx = json_string_value(json_get(a, 2));
                const char *bsx = json_string_value(json_get(b, 2));
                int swap = 0;
                if (a0 < b0) swap = 1;
                else if (a0 == b0) {
                    if (a1 < b1) swap = 1;
                    else if (a1 == b1 && asx && bsx && strcmp(asx, bsx) < 0) swap = 1;
                }
                if (swap) {
                    /* swap positions in place */
                    json_t *tmp = json_copy(a);
                    json_free(((json_t *)repls)->c.items[j]);
                    ((json_t *)repls)->c.items[j] = json_copy(b);
                    json_free(((json_t *)repls)->c.items[j + 1]);
                    ((json_t *)repls)->c.items[j + 1] = tmp;
                }
            }
        }
    }

    for (i = 0; i < count; i++) {
        const json_t *e = json_get(repls, i);
        long start = (long)json_number_value(json_get(e, 0));
        long end = (long)json_number_value(json_get(e, 1));
        const char *mid = json_string_value(json_get(e, 2));
        size_t cl = strlen(cur), ml = mid ? strlen(mid) : 0, pre, post;
        char *next;
        if (start < 0 || end < start || (size_t)end > cl) continue;
        pre = (size_t)start;
        post = cl - (size_t)end;
        next = (char *)malloc(pre + ml + post + 1);
        memcpy(next, cur, pre);
        if (ml) memcpy(next + pre, mid, ml);
        memcpy(next + pre + ml, cur + end, post);
        next[pre + ml + post] = '\0';
        free(cur);
        cur = next;
    }

    /* re.sub(r"[ \t]+\n", "\n", text) then r"\n{3,}" -> "\n\n", then .strip() */
    {
        char *stage = (char *)malloc(strlen(cur) + 1);
        size_t o = 0, p = 0, len = strlen(cur);
        while (p < len) {
            if (cur[p] == ' ' || cur[p] == '\t') {
                size_t q = p;
                while (q < len && (cur[q] == ' ' || cur[q] == '\t')) q++;
                if (q < len && cur[q] == '\n') { stage[o++] = '\n'; p = q + 1; continue; }
                while (p < q) stage[o++] = cur[p++];
                continue;
            }
            stage[o++] = cur[p++];
        }
        stage[o] = '\0';
        free(cur);
        cur = (char *)malloc(o * 2 + 1);
        {
            size_t s = 0, t = 0;
            while (s < o) {
                if (stage[s] == '\n') {
                    size_t q = s;
                    while (q < o && stage[q] == '\n') q++;
                    if (q - s >= 3) { cur[t++] = '\n'; cur[t++] = '\n'; }
                    else { size_t z; for (z = s; z < q; z++) cur[t++] = '\n'; }
                    s = q;
                    continue;
                }
                cur[t++] = stage[s++];
            }
            cur[t] = '\0';
        }
        free(stage);
    }
    {
        char *trimmed = w_strip(cur);
        json_t *res = json_string(trimmed);
        free(trimmed);
        free(cur);
        return res;
    }
}

/* hex_to_npub */
json_t *w27_adapter_04abb5_hex_to_npub(const json_t *in) {
    const char *hex = json_get_str(in, "hex", "");
    unsigned char raw[64];
    size_t nraw = 0, i = 0, nh = strlen(hex);
    int half = 0;
    unsigned acc = 0;
    unsigned data5[128];
    size_t ndata = 0, k;
    unsigned values[160];
    size_t nv = 0;
    unsigned polymod;
    char out[256];
    size_t o = 0;

    while (i < nh) {
        int c = (unsigned char)hex[i++];
        int v;
        if (w_ws(c)) continue;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return json_null();
        if (!half) { acc = (unsigned)v << 4; half = 1; }
        else {
            acc |= (unsigned)v;
            if (nraw >= sizeof(raw)) return json_null();
            raw[nraw++] = (unsigned char)acc;
            half = 0;
        }
    }
    if (half) return json_null();
    if (nraw != 32) return json_null();

    /* convertbits(raw, 8, 5) */
    {
        unsigned acc2 = 0, bits2 = 0;
        for (i = 0; i < nraw; i++) {
            acc2 = ((acc2 << 8) | raw[i]) & 0xfff;
            bits2 += 8;
            while (bits2 >= 5) {
                bits2 -= 5;
                data5[ndata++] = (acc2 >> bits2) & 31u;
            }
        }
        if (bits2) data5[ndata++] = (acc2 << (5 - bits2)) & 31u;
    }

    /* _bech32_hrp_expand("npub") + data + [0]*6 */
    {
        const char *hrp = "npub";
        for (k = 0; hrp[k]; k++) values[nv++] = (unsigned char)hrp[k] >> 5;
        values[nv++] = 0;
        for (k = 0; hrp[k]; k++) values[nv++] = (unsigned char)hrp[k] & 31u;
    }
    for (k = 0; k < ndata; k++) values[nv++] = data5[k];
    for (k = 0; k < 6; k++) values[nv++] = 0;
    polymod = w_bech32_polymod(values, nv) ^ 1u;

    memcpy(out, "npub1", 5);
    o = 5;
    for (k = 0; k < ndata; k++) out[o++] = W_BECH32_CHARSET[data5[k]];
    for (k = 0; k < 6; k++) {
        unsigned v = (polymod >> (5 * (5 - k))) & 31u;
        out[o++] = W_BECH32_CHARSET[v];
    }
    out[o] = '\0';
    return json_string(out);
}

/* module/class tuning constants */
json_t *w27_adapter_04abb5_constants(void) {
    json_t *o = json_object();
    json_t *kinds = json_array();
    json_append(kinds, json_number(9));
    json_append(kinds, json_number(45001));
    json_append(kinds, json_number(45003));
    json_set(o, "chat_kind", json_number(9));
    json_set(o, "dispatch_kinds", kinds);
    json_set(o, "fetch_limit", json_number(50));
    json_set(o, "dm_discovery_every", json_number(5));
    json_set(o, "default_poll_interval", json_number(4.0));
    json_set(o, "min_poll_interval", json_number(1.0));
    json_set(o, "cli_timeout", json_number(30.0));
    json_set(o, "member_cache_ttl", json_number(60.0));
    json_set(o, "profile_name_ttl", json_number(300.0));
    json_set(o, "max_inbound_attachments", json_number(4));
    json_set(o, "max_inbound_attachment_bytes", json_number(20.0 * 1024 * 1024));
    json_set(o, "attachment_download_timeout", json_number(30.0));
    json_set(o, "max_attachment_filename_bytes", json_number(120));
    json_set(o, "ws_auth_timeout", json_number(20.0));
    json_set(o, "ws_read_idle_timeout", json_number(300.0));
    json_set(o, "ws_max_message_bytes", json_number(2000000));
    json_set(o, "ws_membership_kind", json_number(44100));
    json_set(o, "event_meta_content_cap", json_number(500));
    json_set(o, "thread_root_cache", json_number(512));
    json_set(o, "max_cli_message_chars", json_number(900));
    json_set(o, "seen_cap", json_number(500));
    json_set(o, "presentation_mention_separator", json_string("\xe2\x80\x8b"));
    json_set(o, "ws_membership_sub_id", json_string("hermes-buzz-membership"));
    return o;
}

/* ── p2: CLI plumbing / send shaping ───────────────────────────────────── */

/* _add_pubkey: append the lowercased value once, skipping empties. */
static void w_add_pubkey(json_t *bucket, const json_t *raw) {
    char *py = w_pystr(raw);
    char *pk = w_lower(py);
    size_t i;
    free(py);
    if (!pk || !pk[0]) { free(pk); return; }
    for (i = 0; i < json_len(bucket); i++) {
        const char *have = json_string_value(json_get(bucket, i));
        if (have && !strcmp(have, pk)) { free(pk); return; }
    }
    json_append(bucket, json_string(pk));
    free(pk);
}

/* _channel_member_pubkeys: members first, recent traffic as the fallback. */
json_t *w27_adapter_04abb5_member_candidates(const json_t *in) {
    const json_t *members = json_obj_get(in, "members");
    const json_t *messages = json_obj_get(in, "messages");
    json_t *pks = json_array();
    size_t i, j;

    if (members && members->type == JSON_ARRAY) {
        for (i = 0; i < json_len(members); i++) {
            const json_t *row = json_get(members, i);
            if (row && row->type == JSON_OBJECT) w_add_pubkey(pks, json_obj_get(row, "pubkey"));
            else w_add_pubkey(pks, row);
        }
    }
    if (json_len(pks)) return pks;

    if (messages && messages->type == JSON_ARRAY) {
        for (i = 0; i < json_len(messages); i++) {
            const json_t *msg = json_get(messages, i);
            const json_t *tags;
            if (!msg || msg->type != JSON_OBJECT) continue;
            w_add_pubkey(pks, json_obj_get(msg, "pubkey"));
            tags = json_obj_get(msg, "tags");
            if (tags && tags->type == JSON_ARRAY) {
                for (j = 0; j < json_len(tags); j++) {
                    const json_t *t = json_get(tags, j);
                    const char *head;
                    if (!t || t->type != JSON_ARRAY || json_len(t) <= 1) continue;
                    head = json_string_value(json_get(t, 0));
                    if (head && !strcmp(head, "p")) {
                        char *py = w_str(json_get(t, 1));
                        json_t *tmp = json_string(py);
                        w_add_pubkey(pks, tmp);
                        json_free(tmp);
                        free(py);
                    }
                }
            }
        }
    }
    return pks;
}

/* _profile_display_name -> the resolved display name ("" when none). */
json_t *w27_adapter_04abb5_profile_name(const json_t *in) {
    const json_t *profiles = json_obj_get(in, "profiles");
    json_t *res = json_string("");
    if (!profiles || profiles->type != JSON_ARRAY || json_len(profiles) == 0) return res;
    {
        const json_t *p0 = json_get(profiles, 0);
        const json_t *dn, *nm, *content;
        char *name = NULL;
        if (!p0 || p0->type != JSON_OBJECT) return res;
        dn = json_obj_get(p0, "display_name");
        nm = json_obj_get(p0, "name");
        {
            char *a = w_pystr(dn), *b = w_pystr(nm);
            const char *pick = (w_truthy(dn) && a[0]) ? a : b;
            name = w_strip(pick);
            free(a); free(b);
        }
        content = json_obj_get(p0, "content");
        if (!name[0] && w_truthy(content) && content->type == JSON_STRING) {
            json_t *prof = json_parse(content->str_val, NULL);
            if (prof && prof->type == JSON_OBJECT) {
                const json_t *pdn = json_obj_get(prof, "display_name");
                const json_t *pnm = json_obj_get(prof, "name");
                char *a = w_pystr(pdn), *b = w_pystr(pnm);
                const char *pick = (w_truthy(pdn) && a[0]) ? a : b;
                free(name);
                name = w_strip(pick);
                free(a); free(b);
            }
            if (prof) json_free(prof);
        }
        json_free(res);
        res = json_string(name);
        free(name);
    }
    return res;
}

/* (?<![\w@])@<name>(?!\w) IGNORECASE -> (matches, text with matches -> \x00) */
static int w_ws_word(int c) { return w_is_alnum(c) || c == '_'; }

static size_t w_mention_scan(const char *text, const char *name, size_t nlen, char **replaced) {
    size_t n = strlen(text), i = 0, count = 0, o = 0;
    char *out = (char *)malloc(n + 1);
    while (i < n) {
        if (text[i] == '@' && (i == 0 || !(w_ws_word((unsigned char)text[i - 1]) || text[i - 1] == '@'))
            && i + 1 + nlen <= n && w_ieq_at(text, i + 1, name)) {
            size_t after = i + 1 + nlen;
            if (after == n || !w_ws_word((unsigned char)text[after])) {
                out[o++] = 0x01;   /* the span is consumed (Python uses "\x00") */
                count++;
                i = after;
                continue;
            }
        }
        out[o++] = text[i++];
    }
    out[o] = '\0';
    *replaced = out;
    return count;
}

/* _mention_pubkeys_for: word-bounded @Name -> member pubkeys. */
json_t *w27_adapter_04abb5_mention_pubkeys(const json_t *in) {
    const char *content = json_get_str(in, "content", "");
    const char *self_pk = json_get_str(in, "self_pubkey", "");
    const json_t *members = json_obj_get(in, "members");
    json_t *found = json_array();
    char *text = w_dup(content);
    char **keys = NULL, **names = NULL, **pklists = NULL;
    size_t *pkn = NULL, nkeys = 0, cap = 0, i;

    if (!strchr(content, '@')) {
        free(text);
        return found;
    }
    if (members && members->type == JSON_ARRAY) {
        for (i = 0; i < json_len(members); i++) {
            const json_t *m = json_get(members, i);
            const char *pk, *name;
            char *lkey;
            size_t k;
            if (!m || m->type != JSON_OBJECT) continue;
            pk = json_get_str(m, "pubkey", "");
            name = json_get_str(m, "name", "");
            /* A member whose pubkey is null still contributes a CANDIDATE: the
             * live code appends it to by_name[name], so two members sharing a
             * display name are ambiguous and tag nobody. Dropping the null here
             * left one candidate and woke a name Python deliberately refuses. */
            if (!name[0]) continue;
            if (self_pk[0] && pk[0] && !strcmp(pk, self_pk)) continue;
            lkey = w_lower(name);
            for (k = 0; k < nkeys; k++)
                if (!strcmp(keys[k], lkey)) break;
            if (k == nkeys) {
                if (nkeys == cap) {
                    cap = cap ? cap * 2 : 8;
                    keys = (char **)realloc(keys, cap * sizeof(char *));
                    names = (char **)realloc(names, cap * sizeof(char *));
                    pklists = (char **)realloc(pklists, cap * sizeof(char *));
                    pkn = (size_t *)realloc(pkn, cap * sizeof(size_t));
                }
                keys[nkeys] = lkey;
                names[nkeys] = w_dup(name);
                pklists[nkeys] = NULL;
                pkn[nkeys] = 0;
                nkeys++;
                k = nkeys - 1;
            } else {
                free(lkey);
            }
            {
                size_t b;
                int dup = 0;
                for (b = 0; b < pkn[k]; b++) {
                    size_t off = b * 65;
                    if (pklists[k] && !strcmp(pklists[k] + off, pk)) { dup = 1; break; }
                }
                if (!dup) {
                    pklists[k] = (char *)realloc(pklists[k], (pkn[k] + 1) * 65 + 1);
                    memset(pklists[k] + pkn[k] * 65, 0, 65);
                    strncpy(pklists[k] + pkn[k] * 65, pk, 64);
                    pkn[k]++;
                }
            }
        }
    }

    /* sorted(by_name, key=len, reverse=True): longest first, insertion-stable */
    {
        size_t a, b;
        for (a = 0; a < nkeys; a++)
            for (b = 0; b + 1 < nkeys - a; b++)
                if (strlen(keys[b]) < strlen(keys[b + 1])) {
                    char *tk = keys[b]; char *tn = names[b]; char *tp = pklists[b]; size_t tpn = pkn[b];
                    keys[b] = keys[b + 1]; names[b] = names[b + 1]; pklists[b] = pklists[b + 1]; pkn[b] = pkn[b + 1];
                    keys[b + 1] = tk; names[b + 1] = tn; pklists[b + 1] = tp; pkn[b + 1] = tpn;
                }
    }

    for (i = 0; i < nkeys; i++) {
        char *next = NULL;
        size_t hits = w_mention_scan(text, names[i], strlen(names[i]), &next);
        if (hits) {
            if (pkn[i] == 1) {
                const char *pk = pklists[i];
                size_t c;
                int have = 0;
                for (c = 0; c < json_len(found); c++) {
                    const json_t *f = json_get(found, c);
                    if (!pk[0]) {
                        if (f && f->type == JSON_NULL) { have = 1; break; }
                    } else {
                        const char *fv = json_string_value(f);
                        if (fv && !strcmp(fv, pk)) { have = 1; break; }
                    }
                }
                /* A single candidate that is a null pubkey still reaches found
                 * upstream (the live code appends whatever the member list held),
                 * so it must be emitted as JSON null rather than skipped. */
                if (!have) json_append(found, pk[0] ? json_string(pk) : json_null());
            }
            free(text);
            text = next;
        } else {
            free(next);
        }
    }

    for (i = 0; i < nkeys; i++) { free(keys[i]); free(names[i]); free(pklists[i]); }
    free(keys); free(names); free(pklists); free(pkn);
    free(text);
    return found;
}

/* _bounded_cli_message */
static char *w_bounded(const char *message, const char *redact_path) {
    char *work;
    if (!message) return w_dup("");
    work = redact_path && redact_path[0] ? NULL : w_dup(message);
    if (redact_path && redact_path[0]) {
        /* message.replace(str(redact_path), redact_path.name) */
        const char *base = strrchr(redact_path, '/');
        const char *base2 = strrchr(redact_path, '\\');
        const char *name = base ? base + 1 : redact_path;
        size_t plen = strlen(redact_path), nl = strlen(name), ml = strlen(message), o = 0, i = 0;
        char *out = (char *)malloc(ml + 1);
        if (base2 && (!base || base2 > base)) name = base2 + 1;
        nl = strlen(name);
        while (i < ml) {
            if (plen && i + plen <= ml && !strncmp(message + i, redact_path, plen)) {
                memcpy(out + o, name, nl);
                o += nl;
                i += plen;
                continue;
            }
            out[o++] = message[i++];
        }
        out[o] = '\0';
        work = out;
    }
    if (strlen(work) <= 900) return work;
    {
        char *cut = w_ndup(work, 897);
        char *out = (char *)malloc(901);
        strcpy(out, cut);
        strcat(out, "...");
        free(cut); free(work);
        return out;
    }
}

/* _json_or(text, default) where default is a parse of json.dumps(default) */
static json_t *w_json_or(const char *text, const json_t *deflt) {
    json_t *parsed;
    if (!text || !text[0]) return deflt ? json_copy(deflt) : json_null();
    parsed = json_parse(text, NULL);
    if (!parsed) return deflt ? json_copy(deflt) : json_null();
    return parsed;
}

/* _cli_error_message */
static char *w_cli_error(const char *stderr_text, double returncode, const char *redact_path) {
    char *text = w_strip(stderr_text ? stderr_text : "");
    json_t *data = w_json_or(text, NULL);
    if (data && data->type == JSON_OBJECT) {
        const json_t *detail = json_obj_get(data, "message");
        const json_t *category = json_obj_get(data, "error");
        if (detail && detail->type == JSON_STRING && w_truthy(detail)) {
            char *label = (category && category->type == JSON_STRING && w_truthy(category))
                ? w_strip(category->str_val) : w_dup("error");
            char *d = w_strip(detail->str_val);
            char *joined = (char *)malloc(strlen(label) + strlen(d) + 64);
            char *bounded;
            char num[64];
            snprintf(num, sizeof(num), "%g", returncode);
            sprintf(joined, "%s: %s (exit %s)", label, d, num);
            bounded = w_bounded(joined, redact_path);
            free(joined); free(d); free(label);
            json_free(data); free(text);
            return bounded;
        }
    }
    if (data) json_free(data);
    if (text[0]) { char *b = w_bounded(text, redact_path); free(text); return b; }
    {
        char *fallback = (char *)malloc(96);
        char num[64];
        snprintf(num, sizeof(num), "%g", returncode);
        sprintf(fallback, "buzz CLI failed with exit code %s", num);
        {
            char *b = w_bounded(fallback, redact_path);
            free(fallback); free(text);
            return b;
        }
    }
}

/* _parse_send_receipt -> (event_id|NULL, error|NULL) */
static void w_parse_receipt(const char *stdout_text, char **event_id, char **error) {
    json_t *data = w_json_or(stdout_text, NULL);
    *event_id = NULL;
    *error = NULL;
    if (!data || data->type != JSON_OBJECT) {
        *error = w_dup("invalid CLI response");
        if (data) json_free(data);
        return;
    }
    {
        const json_t *accepted = json_obj_get(data, "accepted");
        if (accepted && accepted->type == JSON_BOOL && accepted->bool_val == 0) {
            const json_t *detail = json_obj_get(data, "message");
            if (!detail || detail->type != JSON_STRING || !w_truthy(detail)) *error = w_dup("message was not accepted");
            else { char *d = w_strip(detail->str_val); *error = w_bounded(d, NULL); free(d); }
            json_free(data);
            return;
        }
        {
            const json_t *eid = json_obj_get(data, "event_id");
            if (!accepted || accepted->type != JSON_BOOL || accepted->bool_val != 1
                || !eid || eid->type != JSON_STRING || !w_truthy(eid)) {
                *error = w_dup("invalid CLI response");
            } else {
                char *t = w_strip(eid->str_val);
                *event_id = t;
            }
        }
    }
    json_free(data);
}

static json_t *w_send_result_obj(int success, const char *message_id, const char *error, int retryable) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(success));
    json_set(o, "message_id", message_id ? json_string(message_id) : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "retryable", json_bool(retryable));
    return o;
}

/* _run_message_send: the bounded recovery ladder, over a scripted CLI.
 * Rungs (each at most once): explicit --mentions -> without them on a
 * membership-drift rejection -> escaped presentation mention -> --mention self. */
typedef struct {
    json_t *calls;
    json_t *inputs;
    const json_t *script;
    size_t step;
    double rc;
    char *out;
    char *err;
    int exhausted;
} w_ladder;

static int w_ladder_call(w_ladder *L, const json_t *args, const json_t *extra, const char *input) {
    const json_t *entry;
    json_t *argv = json_array();
    size_t i;
    if (args && args->type == JSON_ARRAY)
        for (i = 0; i < json_len(args); i++) json_append(argv, json_copy(json_get(args, i)));
    if (extra && extra->type == JSON_ARRAY)
        for (i = 0; i < json_len(extra); i++) json_append(argv, json_copy(json_get(extra, i)));
    json_append(L->calls, argv);
    json_append(L->inputs, json_string(input ? input : ""));
    if (!L->script || L->script->type != JSON_ARRAY || L->step >= json_len(L->script)) {
        L->exhausted = 1;
        return 0;
    }
    entry = json_get(L->script, L->step++);
    L->rc = json_number_value(json_get(entry, 0));
    free(L->out);
    L->out = w_dup(json_string_value(json_get(entry, 1)));
    free(L->err);
    L->err = w_dup(json_string_value(json_get(entry, 2)));
    return 1;
}

json_t *w27_adapter_04abb5_send_ladder(const json_t *in) {
    const json_t *args = json_obj_get(in, "args");
    const json_t *mentions = json_obj_get(in, "mention_pubkeys");
    const char *self_pk = json_get_str(in, "self_pubkey", "");
    const char *content = json_get_str(in, "content", "");
    w_ladder L;
    json_t *margs = json_array();
    json_t *out = json_object();
    size_t i;

    memset(&L, 0, sizeof(L));
    L.calls = json_array();
    L.inputs = json_array();
    L.script = json_obj_get(in, "script");
    L.out = w_dup("");
    L.err = w_dup("");
    if (mentions && mentions->type == JSON_ARRAY)
        for (i = 0; i < json_len(mentions); i++) {
            const char *pk = json_string_value(json_get(mentions, i));
            json_append(margs, json_string("--mention"));
            json_append(margs, json_string(pk ? pk : ""));
        }

    if (!w_ladder_call(&L, args, margs, content)) goto done;
    if (L.rc == 0) goto done;

    if (json_len(margs) > 0 && strstr(L.err, "not channel members")) {
        if (!w_ladder_call(&L, args, NULL, content)) goto done;
        if (L.rc == 0) goto done;
    }

    {
        json_t *probe = json_object();
        json_t *esc;
        json_set(probe, "content", json_string(content));
        json_set(probe, "error", json_string(L.err));
        esc = w27_adapter_04abb5_escape_mention(probe);
        json_free(probe);
        if (w_truthy(esc)) {
            const char *escaped = json_string_value(esc);
            int ok = w_ladder_call(&L, args, NULL, escaped ? escaped : "");
            json_free(esc);
            if (!ok) goto done;
            if (L.rc == 0) goto done;
        } else {
            json_free(esc);
        }
    }

    if (strstr(L.err, "does not match a current channel member") && self_pk[0]) {
        json_t *extra = json_array();
        json_append(extra, json_string("--mention"));
        json_append(extra, json_string(self_pk));
        if (!w_ladder_call(&L, args, extra, content)) { json_free(extra); goto done; }
        json_free(extra);
    }

done:
    json_set(out, "calls", L.calls);
    json_set(out, "inputs", L.inputs);
    if (L.exhausted) {
        json_set(out, "error", json_string("SCRIPT_EXHAUSTED"));
    } else {
        json_set(out, "rc", json_number(L.rc));
        json_set(out, "out", json_string(L.out));
        json_set(out, "err", json_string(L.err));
    }
    free(L.out);
    free(L.err);
    json_free(margs);
    return out;
}

/* _send_result -> SendResult fields */
json_t *w27_adapter_04abb5_send_result(const json_t *in) {
    double code = json_get_num(in, "code", 0);
    const char *out = json_get_str(in, "stdout", "");
    const char *err = json_get_str(in, "stderr", "");
    const char *redact = json_get_str(in, "redact", "");
    json_t *res;
    if (code != 0) {
        char *message = w_cli_error(err, code, redact);
        res = w_send_result_obj(0, NULL, message, (int)code == 2);
        free(message);
        return res;
    }
    {
        char *event_id = NULL, *receipt_error = NULL;
        w_parse_receipt(out, &event_id, &receipt_error);
        if (receipt_error) {
            res = w_send_result_obj(0, NULL, receipt_error, 0);
            free(receipt_error); free(event_id);
            return res;
        }
        res = w_send_result_obj(1, event_id, NULL, 0);
        free(event_id);
        return res;
    }
}

/* get_chat_info */
json_t *w27_adapter_04abb5_chat_info(const json_t *in) {
    char *chat_id = w_str(json_obj_get(in, "chat_id"));
    const json_t *names = json_obj_get(in, "names");
    const json_t *state = json_obj_get(in, "state");
    const json_t *reply = json_obj_get(in, "channel_reply");
    const char *cli_path = json_get_str(in, "cli_path", "");
    const json_t *name = NULL;
    json_t *out = json_object();
    char *type;

    if (names && names->type == JSON_OBJECT) name = json_obj_get(names, chat_id);
    if (!name && cli_path[0] && reply && reply->type == JSON_OBJECT) name = json_obj_get(reply, "name");
    type = w_dup("group");
    if (state && state->type == JSON_OBJECT) {
        /* self._channel_state.get(chat_id) -- keyed by chat id, no flat fallback.
         * A state map that does not carry THIS chat answers None upstream, which
         * is the "group" default, not the map's own chat_type. */
        const json_t *st = json_obj_get(state, chat_id);
        if (st && st->type == JSON_OBJECT) {
            const json_t *ct = json_obj_get(st, "chat_type");
            if (ct && ct->type == JSON_STRING && ct->str_val[0]) { free(type); type = w_dup(ct->str_val); }
        }
    }
    json_set(out, "name", (name && w_truthy(name)) ? json_copy(name) : json_string(chat_id));
    json_set(out, "type", json_string(type));
    json_set(out, "chat_id", json_string(chat_id));
    free(type);
    free(chat_id);
    return out;
}

/* _websocket_url: relay URL -> ws(s) URL, or the ValueError detail. */
json_t *w27_adapter_04abb5_websocket_url(const json_t *in) {
    char *raw = w_strip(json_get_str(in, "relay_url", ""));
    w_url u;
    json_t *out = json_object();
    const char *scheme;
    w_urlsplit(raw, &u);
    free(raw);
    scheme = u.scheme;
    if (!strcmp(scheme, "http")) scheme = "ws";
    else if (!strcmp(scheme, "https")) scheme = "wss";
    if ((strcmp(scheme, "ws") && strcmp(scheme, "wss")) || !u.netloc[0]) {
        json_set(out, "error", json_string("Buzz relay URL must use http(s) or ws(s)"));
    } else {
        char *url;
        size_t need = strlen(scheme) + strlen(u.netloc) + strlen(u.path) + strlen(u.query) + 8;
        url = (char *)malloc(need);
        url[0] = '\0';
        strcat(url, scheme);
        strcat(url, "://");
        strcat(url, u.netloc);
        if (u.path[0]) {
            if (u.path[0] != '/') strcat(url, "/");
            strcat(url, u.path);
        }
        if (u.query[0]) { strcat(url, "?"); strcat(url, u.query); }
        json_set(out, "ok", json_string(url));
        free(url);
    }
    w_url_clear(&u);
    return out;
}

/* sorted(_DISPATCH_KINDS) + the since/limit choice for a channel subscription */
static json_t *w_subscription_filter(const char *channel_id, double last_ts) {
    json_t *f = json_object(), *kinds = json_array();
    json_append(kinds, json_number(9));
    json_append(kinds, json_number(45001));
    json_append(kinds, json_number(45003));
    json_set(f, "kinds", kinds);
    {
        json_t *h = json_array();
        json_append(h, json_string(channel_id ? channel_id : ""));
        json_set(f, "#h", h);
    }
    if ((long long)last_ts) {
        double since = last_ts - 1;
        if (since < 0) since = 0;
        json_set(f, "since", json_number(since));
    } else {
        json_set(f, "limit", json_number(50));
    }
    return f;
}

json_t *w27_adapter_04abb5_channel_subscription(const json_t *in) {
    const char *channel_id = json_get_str(in, "channel_id", "");
    const json_t *state = json_obj_get(in, "state");
    double last_ts = 0;
    if (state && state->type == JSON_OBJECT) {
        const json_t *lt = json_obj_get(state, "last_ts");
        if (lt && lt->type == JSON_NUMBER) last_ts = (double)(long long)lt->num_val;
    }
    return w_subscription_filter(channel_id, last_ts);
}

/* _subscribe_websocket: the subscription map and the REQ sends it issues. */
json_t *w27_adapter_04abb5_subscriptions(const json_t *in) {
    const json_t *states = json_obj_get(in, "channel_state");
    const json_t *restricted = json_obj_get(in, "restricted");
    const char *self_pk = json_get_str(in, "self_pubkey", "");
    double membership_since = json_get_num(in, "membership_since", 0);
    json_t *out = json_object(), *subs = json_array(), *sends = json_array();
    size_t i, index = 0;

    if (states && states->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(states); i++) {
            const char *ch = json_object_get_key_at(states, i);
            int skip = 0;
            size_t r;
            if (!ch) continue;
            if (restricted && restricted->type == JSON_ARRAY)
                for (r = 0; r < json_len(restricted); r++) {
                    const char *x = json_string_value(json_get(restricted, r));
                    if (x && !strcmp(x, ch)) { skip = 1; break; }
                }
            {
                char sub_id[64];
                json_t *pair = json_array();
                double last_ts = 0;
                const json_t *st = json_object_get_at(states, i);
                if (skip) { index++; continue; }
                if (st && st->type == JSON_OBJECT) {
                    const json_t *lt = json_obj_get(st, "last_ts");
                    if (lt && lt->type == JSON_NUMBER) last_ts = (double)(long long)lt->num_val;
                }
                snprintf(sub_id, sizeof(sub_id), "hermes-buzz-%llu", (unsigned long long)index);
                index++;
                json_append(pair, json_string(sub_id));
                json_append(pair, json_string(ch));
                json_append(subs, pair);
                {
                    json_t *send = json_array();
                    json_append(send, json_string(sub_id));
                    json_append(send, w_subscription_filter(ch, last_ts));
                    json_append(sends, send);
                }
            }
        }
    }
    if (self_pk[0]) {
        json_t *pair = json_array(), *send = json_array(), *f = json_object(), *kinds = json_array(), *p = json_array();
        double since = membership_since - 1;
        if (since < 0) since = 0;
        json_append(kinds, json_number(44100));
        json_append(p, json_string(self_pk));
        json_set(f, "kinds", kinds);
        json_set(f, "#p", p);
        json_set(f, "since", json_number(since));
        json_append(pair, json_string("hermes-buzz-membership"));
        json_append(pair, json_null());
        json_append(subs, pair);
        json_append(send, json_string("hermes-buzz-membership"));
        json_append(send, f);
        json_append(sends, send);
    }
    json_set(out, "subs", subs);
    json_set(out, "sends", sends);
    return out;
}

/* ── p3: inbound shaping (frames, NIP-94 metadata, dispatch) ───────────── */

/* _handle_ws_message: route one parsed relay frame. */
json_t *w27_adapter_04abb5_ws_message(const json_t *in) {
    const json_t *message = json_obj_get(in, "message");
    const json_t *subs = json_obj_get(in, "subscriptions");
    const json_t *states = json_obj_get(in, "channel_state");
    double membership_since = json_get_num(in, "membership_since", 0);
    json_t *out = json_object();
    const char *head;
    size_t len = message ? json_len(message) : 0;

    if (!message || message->type != JSON_ARRAY || !len) {
        json_set(out, "action", json_string("ignore"));
        return out;
    }
    head = json_string_value(json_get(message, 0));
    if (head && !strcmp(head, "EVENT") && len >= 3) {
        char *sub_id = w_str(json_get(message, 1));
        const json_t *event = json_get(message, 2);
        if (!event || event->type != JSON_OBJECT) {
            json_set(out, "action", json_string("ignore"));
            free(sub_id);
            return out;
        }
        if (!strcmp(sub_id, "hermes-buzz-membership")) {
            const json_t *created = json_obj_get(event, "created_at");
            double ts = 0;
            if (created && created->type == JSON_NUMBER) ts = (double)(long long)created->num_val;
            if (ts > membership_since) membership_since = ts;
            json_set(out, "action", json_string("rediscover"));
            json_set(out, "membership_since", json_number(membership_since));
            free(sub_id);
            return out;
        }
        {
            const json_t *ch = (subs && subs->type == JSON_OBJECT) ? json_obj_get(subs, sub_id) : NULL;
            const char *channel_id = (ch && ch->type == JSON_STRING) ? ch->str_val : NULL;
            const json_t *state = (channel_id && states && states->type == JSON_OBJECT) ? json_obj_get(states, channel_id) : NULL;
            if (channel_id && state && state->type == JSON_OBJECT) {
                json_set(out, "action", json_string("dispatch"));
                json_set(out, "channel", json_string(channel_id));
            } else {
                json_set(out, "action", json_string("ignore"));
            }
        }
        free(sub_id);
        return out;
    }
    if (head && !strcmp(head, "CLOSED")) {
        const json_t *detail_node = len > 2 ? json_get(message, len - 1) : NULL;
        char *detail = detail_node ? w_str(detail_node) : w_dup("subscription closed");
        char *sub_id = len > 1 ? w_str(json_get(message, 1)) : w_dup("");
        /* subscriptions.get(sub_id) runs for ANY sub_id, including "" -- a dict may
         * carry an empty key. The channel is then used for its TRUTHINESS upstream:
         * `if not (rejected and closed_channel): raise`, so an empty channel id is
         * falsy and the frame raises instead of dropping the channel. */
        const json_t *ch = (subs && subs->type == JSON_OBJECT) ? json_obj_get(subs, sub_id) : NULL;
        const char *closed_channel = (ch && ch->type == JSON_STRING) ? ch->str_val : NULL;
        int has_channel = closed_channel && closed_channel[0];
        char *lower = w_lower(detail);
        int rejected = strstr(lower, "restricted") || strstr(lower, "not a channel member") || strstr(lower, "auth-required");
        free(lower);
        if (!(rejected && has_channel)) {
            json_set(out, "action", json_string("raise"));
            json_set(out, "detail", json_string(detail));
            free(detail); free(sub_id);
            return out;
        }
        json_set(out, "action", json_string("drop"));
        json_set(out, "detail", json_string(detail));
        json_set(out, "channel", json_string(closed_channel));
        {
            json_t *restricted = json_array();
            json_t *left = json_array();
            size_t i;
            json_append(restricted, json_string(closed_channel));
            json_set(out, "restricted", restricted);
            if (states && states->type == JSON_OBJECT)
                for (i = 0; i < json_object_size(states); i++) {
                    const char *k = json_object_get_key_at(states, i);
                    if (k && strcmp(k, closed_channel)) json_append(left, json_string(k));
                }
            json_set(out, "state_left", left);
            json_set(out, "subs_removed", json_bool(1));
        }
        free(detail); free(sub_id);
        return out;
    }
    if (head && !strcmp(head, "NOTICE")) {
        json_set(out, "action", json_string("notice"));
        return out;
    }
    json_set(out, "action", json_string("ignore"));
    return out;
}

/* Python int(str): strips whitespace, accepts an optional sign and digits. */
static int w_py_int(const char *s, long long *out) {
    const char *p = s;
    int neg = 0;
    long long v = 0;
    int any = 0;
    while (*p && w_ws((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
    if (*p == '_') return 0;
    while (*p && w_ws((unsigned char)*p)) p++;
    if (*p || !any) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* imeta fields: "<key> <value>" pairs, first key wins. */
static json_t *w_imeta_fields(const json_t *tag) {
    json_t *fields = json_object();
    size_t i;
    for (i = 1; i < json_len(tag); i++) {
        const json_t *f = json_get(tag, i);
        const char *s;
        const char *sp;
        if (!f || f->type != JSON_STRING) continue;
        s = f->str_val ? f->str_val : "";
        sp = strchr(s, ' ');
        if (!sp) continue;                       /* partition found no separator */
        {
            char *key = w_ndup(s, (size_t)(sp - s));
            char *val = w_strip(sp + 1);
            if (!json_has(fields, key)) json_set(fields, key, json_string(val));
            free(key);
            free(val);
        }
    }
    return fields;
}

/* _parse_imeta_attachments -> {attachments, rejected} */
static json_t *w_imeta_scan(const json_t *in, json_t **attachments_out) {
    const json_t *event = json_obj_get(in, "event");
    const json_t *tags = (event && event->type == JSON_OBJECT) ? json_obj_get(event, "tags") : NULL;
    json_t *attachments = json_array();
    long long rejected = 0, total = 0;
    size_t i;
    json_t *out = json_object();

    if (tags && tags->type == JSON_ARRAY) {
        for (i = 0; i < json_len(tags); i++) {
            const json_t *tag = json_get(tags, i);
            const char *head;
            json_t *fields;
            const char *url, *digest_field, *size_field;
            char *digest;
            long long size = 0;
            w_url u;
            int ok;

            if (!tag || tag->type != JSON_ARRAY || json_len(tag) == 0) continue;
            head = json_string_value(json_get(tag, 0));
            if (!head || strcmp(head, "imeta")) continue;
            if (json_len(attachments) >= 4) { rejected++; continue; }

            fields = w_imeta_fields(tag);
            url = json_get_str(fields, "url", "");
            digest_field = json_get_str(fields, "x", "");
            digest = w_lower(digest_field);
            size_field = json_get_str(fields, "size", "");

            ok = w_py_int(size_field, &size);
            if (!ok) {
                rejected++;
                free(digest);
                json_free(fields);
                continue;
            }
            w_urlsplit(url, &u);
            if (u.bad || u.bad_port) {
                rejected++;
                free(digest);
                json_free(fields);
                w_url_clear(&u);
                continue;
            }
            {
                int has_host = u.host && u.host[0];
                int has_user = u.username && u.username[0];
                int has_frag = u.fragment && u.fragment[0];
                if (strcmp(u.scheme, "https") || !has_host || has_user || has_frag
                    || !w_hex64_full(digest) || !(size > 0) || size > 20LL * 1024 * 1024
                    || total + size > 20LL * 1024 * 1024) {
                    rejected++;
                    free(digest);
                    json_free(fields);
                    w_url_clear(&u);
                    continue;
                }
            }
            total += size;
            {
                json_t *entry = json_object();
                json_t *name_in = json_object();
                json_t *fname;
                const char *mime = json_get_str(fields, "m", "");
                char mime_cut[256];
                size_t ml = strlen(mime);
                if (ml > 255) ml = 255;
                memcpy(mime_cut, mime, ml);
                mime_cut[ml] = '\0';
                json_set(name_in, "value", json_string(json_get_str(fields, "filename", "")));
                fname = w27_adapter_04abb5_safe_filename(name_in);
                json_free(name_in);
                json_set(entry, "url", json_string(url));
                json_set(entry, "sha256", json_string(digest));
                json_set(entry, "size", json_number((double)size));
                json_set(entry, "filename", fname);
                json_set(entry, "mime_type", json_string(mime_cut));
                json_append(attachments, entry);
            }
            free(digest);
            json_free(fields);
            w_url_clear(&u);
        }
    }
    json_set(out, "rejected", json_number((double)rejected));
    if (attachments_out) *attachments_out = attachments;
    else json_free(attachments);
    return out;
}

json_t *w27_adapter_04abb5_imeta_parse(const json_t *in) {
    json_t *attachments = NULL;
    json_t *out = w_imeta_scan(in, &attachments);
    json_set(out, "attachments", attachments);
    return out;
}

json_t *w27_adapter_04abb5_imeta_attachments(const json_t *in) {
    json_t *attachments = NULL;
    json_t *scratch = w_imeta_scan(in, &attachments);
    json_free(scratch);
    return attachments;
}

/* _attachment_rejection_note */
json_t *w27_adapter_04abb5_rejection_note(const json_t *in) {
    double rejected = json_get_num(in, "rejected", 0);
    char buf[128];
    if (rejected <= 999) snprintf(buf, sizeof(buf), "[%g Buzz attachment(s) rejected as malformed or over limits.]", rejected);
    else snprintf(buf, sizeof(buf), "[999+ Buzz attachment(s) rejected as malformed or over limits.]");
    return json_string(buf);
}

/* _resolve_user_name: cached, else the profile CLI, else an npub/hex prefix. */
json_t *w27_adapter_04abb5_user_name(const json_t *in) {
    const char *pubkey = json_get_str(in, "pubkey", "");
    const json_t *cached = json_obj_get(in, "cached");
    double code = json_get_num(in, "code", 0);

    if (cached && cached->type == JSON_OBJECT) {
        const json_t *hit = json_obj_get(cached, pubkey);
        if (hit && hit->type != JSON_NULL) return json_copy(hit);
    }
    {
        char *name = w_dup("");
        if (code == 0) {
            const json_t *out_text = json_obj_get(in, "stdout");
            char *text = w_pystr(out_text);
            json_t *data = w_json_or(text, NULL);
            free(text);
            if (data && data->type == JSON_ARRAY && json_len(data) > 0) {
                const json_t *p0 = NULL;
                size_t k;
                for (k = 0; k < json_len(data); k++) {
                    const json_t *cand = json_get(data, k);
                    if (cand && cand->type == JSON_OBJECT) { p0 = cand; break; }
                }
                if (p0) {
                    const json_t *dn = json_obj_get(p0, "display_name");
                    char *raw = w_pystr(dn);
                    free(name);
                    name = w_strip(raw);
                    free(raw);
                }
            }
            if (data) json_free(data);
        }
        if (!name[0]) {
            json_t *probe = json_object();
            json_t *npub;
            char *fallback = NULL;
            free(name);
            json_set(probe, "hex", json_string(pubkey));
            npub = w27_adapter_04abb5_hex_to_npub(probe);
            json_free(probe);
            if (w_truthy(npub)) fallback = w_dup(json_string_value(npub));
            else fallback = w_dup(pubkey);
            if (npub) json_free(npub);
            name = w_ndup(fallback, strlen(fallback) > 16 ? 16 : strlen(fallback));
            free(fallback);
        }
        {
            json_t *res = json_string(name);
            free(name);
            return res;
        }
    }
}

/* _dispatch_message: the shaping half (text, media merge, message type). */
json_t *w27_adapter_04abb5_dispatch_shape(const json_t *in) {
    const json_t *localized = json_obj_get(in, "localized");
    const char *message_type = json_get_str(in, "message_type", "text");
    const char *chat_id = json_get_str(in, "chat_id", "");
    const json_t *names = json_obj_get(in, "channel_names");
    const json_t *urls_in = json_obj_get(in, "media_urls");
    const json_t *types_in = json_obj_get(in, "media_types");
    char *text;
    json_t *urls = json_array(), *types = json_array(), *out = json_object();
    json_t *lurls = localized ? json_obj_get(localized, "urls") : NULL;
    json_t *ltypes = localized ? json_obj_get(localized, "types") : NULL;
    const char *ltype = localized ? json_get_str(localized, "message_type", "text") : "text";
    size_t i;

    text = localized ? w_dup(json_get_str(localized, "text", "")) : w_dup(json_get_str(in, "text", ""));
    if (urls_in && urls_in->type == JSON_ARRAY)
        for (i = 0; i < json_len(urls_in); i++) json_append(urls, json_copy(json_get(urls_in, i)));
    if (types_in && types_in->type == JSON_ARRAY)
        for (i = 0; i < json_len(types_in); i++) json_append(types, json_copy(json_get(types_in, i)));

    if (lurls && lurls->type == JSON_ARRAY) {
        for (i = 0; i < json_len(lurls); i++) {
            const char *path = json_string_value(json_get(lurls, i));
            const char *mime = (ltypes && ltypes->type == JSON_ARRAY && i < json_len(ltypes))
                ? json_string_value(json_get(ltypes, i)) : NULL;
            int present = 0;
            size_t k;
            for (k = 0; k < json_len(urls); k++) {
                const char *have = json_string_value(json_get(urls, k));
                if (have && path && !strcmp(have, path)) { present = 1; break; }
            }
            if (!present && path) {
                json_append(urls, json_string(path));
                json_append(types, json_string(mime ? mime : ""));
            }
        }
    }

    if (!strcmp(message_type, "text")) {
        message_type = ltype;
    } else if (lurls && json_len(lurls) && strcmp(ltype, message_type) && strcmp(ltype, "text")) {
        message_type = "document";
    }

    json_set(out, "text", json_string(text));
    json_set(out, "message_type", json_string(message_type));
    json_set(out, "media_urls", urls);
    json_set(out, "media_types", types);
    {
        json_t *inlined = json_array();
        for (i = 0; i < json_len(urls); i++) json_append(inlined, json_bool(0));
        json_set(out, "media_text_inlined", inlined);
    }
    {
        const json_t *chat_name = (names && names->type == JSON_OBJECT) ? json_obj_get(names, chat_id) : NULL;
        /* self._channel_names.get(chat_id, chat_id): the default applies only when
         * the KEY is absent. A present-but-empty name is the answer, not a miss. */
        json_set(out, "chat_name", chat_name ? json_copy(chat_name) : json_string(chat_id));
    }
    free(text);
    return out;
}

/* ── p4: configuration decisions + standalone send ─────────────────────── */

/* check_requirements (unscoped path): a relay URL and a resolvable key. */
json_t *w27_adapter_04abb5_check_requirements(const json_t *in) {
    char *relay = w_strip(json_get_str(in, "relay", ""));
    char *key = w_strip(json_get_str(in, "key", ""));
    int ok = relay[0] && key[0];
    free(relay); free(key);
    return json_bool(ok);
}

/* validate_config: relay (env wins over extra) and a resolvable key. */
json_t *w27_adapter_04abb5_validate_config(const json_t *in) {
    const char *env_relay = json_get_str(in, "relay_env", "");
    const json_t *extra = json_obj_get(in, "extra");
    const char *extra_relay = (extra && extra->type == JSON_OBJECT) ? json_get_str(extra, "relay_url", "") : "";
    char *key = w_strip(json_get_str(in, "key", ""));
    const char *relay = env_relay[0] ? env_relay : extra_relay;
    int ok = relay && relay[0] && key[0];
    free(key);
    return json_bool(ok);
}

/* _YAML_BRIDGE: (yaml key, env var, kind) */
typedef struct { const char *key; const char *env; const char *kind; } w_bridge_row;
static const w_bridge_row W_BRIDGE[] = {
    { "relay_url", "BUZZ_RELAY_URL", "str" },
    { "cli_path", "BUZZ_CLI_PATH", "str" },
    { "home_channel", "BUZZ_HOME_CHANNEL", "str" },
    { "transport", "BUZZ_TRANSPORT", "str" },
    { "poll_interval", "BUZZ_POLL_INTERVAL", "str" },
    { "channels", "BUZZ_CHANNELS", "csv" },
    { "allowed_users", "BUZZ_ALLOWED_USERS", "csv" },
    { "reaction_only_users", "BUZZ_REACTION_ONLY_USERS", "csv" },
    { "allow_all_users", "BUZZ_ALLOW_ALL_USERS", "lower" },
    { "require_mention", "BUZZ_REQUIRE_MENTION", "lower" },
    { "reply_in_thread", "BUZZ_REPLY_IN_THREAD", "lower" },
    { "reply_to_mode", "BUZZ_REPLY_TO_MODE", "lower" },
};

/* _apply_yaml_config: buzz.extra -> the seeded extra (or null). */
json_t *w27_adapter_04abb5_apply_yaml_config(const json_t *in) {
    const json_t *buzz = json_obj_get(in, "buzz_cfg");
    const json_t *extra = NULL;
    json_t *seeded = json_object();
    size_t i;

    if (!buzz || buzz->type != JSON_OBJECT) return json_null();
    extra = json_obj_get(buzz, "extra");
    if (!extra || !w_truthy(extra)) extra = buzz;
    if (!extra || extra->type != JSON_OBJECT) { json_free(seeded); return json_null(); }

    for (i = 0; i < sizeof(W_BRIDGE) / sizeof(W_BRIDGE[0]); i++) {
        const json_t *v = json_obj_get(extra, W_BRIDGE[i].key);
        int applies = 0;
        if (!strcmp(W_BRIDGE[i].kind, "str")) {
            applies = v && !(v->type == JSON_NULL) && !(v->type == JSON_STRING && !v->str_val[0]);
        } else if (!strcmp(W_BRIDGE[i].kind, "csv")) {
            applies = v && v->type != JSON_NULL;
        } else {
            applies = json_has(extra, W_BRIDGE[i].key);
        }
        if (applies) json_set(seeded, W_BRIDGE[i].key, json_copy(v));
    }
    if (!json_object_size(seeded)) { json_free(seeded); return json_null(); }
    return seeded;
}

/* _env_enablement: env -> the seeded extra, or null when unconfigured. */
json_t *w27_adapter_04abb5_env_enablement(const json_t *in) {
    const json_t *env = json_obj_get(in, "env");
    char *relay, *key, *channels_raw, *poll_raw, *cli_raw, *home_raw, *home_name;
    json_t *out;

    if (!env || env->type != JSON_OBJECT) env = NULL;
    relay = w_strip(env ? json_get_str(env, "BUZZ_RELAY_URL", "") : "");
    key = w_strip(env ? json_get_str(env, "BUZZ_PRIVATE_KEY", "") : "");
    if (!relay[0] || !key[0]) {
        free(relay); free(key);
        return json_null();
    }
    out = json_object();
    json_set(out, "relay_url", json_string(relay));

    channels_raw = w_strip(env ? json_get_str(env, "BUZZ_CHANNELS", "") : "");
    if (channels_raw[0]) {
        json_t *channels = json_array();
        char *p = channels_raw;
        while (p && *p) {
            char *comma = strchr(p, ',');
            char *piece = comma ? w_ndup(p, (size_t)(comma - p)) : w_dup(p);
            char *trimmed = w_strip(piece);
            free(piece);
            if (trimmed[0]) json_append(channels, json_string(trimmed));
            free(trimmed);
            p = comma ? comma + 1 : NULL;
        }
        json_set(out, "channels", channels);
    }
    free(channels_raw);

    poll_raw = w_strip(env ? json_get_str(env, "BUZZ_POLL_INTERVAL", "") : "");
    if (poll_raw[0]) {
        char *endp = NULL;
        double val = strtod(poll_raw, &endp);
        int ok = endp && endp != poll_raw;
        while (endp && *endp && w_ws((unsigned char)*endp)) endp++;
        if (endp && *endp) ok = 0;
        if (ok) json_set(out, "poll_interval", json_number(val));
    }
    free(poll_raw);

    cli_raw = w_strip(env ? json_get_str(env, "BUZZ_CLI_PATH", "") : "");
    if (cli_raw[0]) json_set(out, "cli_path", json_string(cli_raw));
    free(cli_raw);

    home_raw = w_strip(env ? json_get_str(env, "BUZZ_HOME_CHANNEL", "") : "");
    if (!home_raw[0]) {
        const json_t *chans = json_obj_get(out, "channels");
        if (chans && chans->type == JSON_ARRAY && json_len(chans) > 0) {
            const char *first = json_string_value(json_get(chans, 0));
            free(home_raw);
            home_raw = w_dup(first ? first : "");
        }
    }
    if (home_raw[0]) {
        json_t *hc = json_object();
        home_name = env ? w_dup(json_get_str(env, "BUZZ_HOME_CHANNEL_NAME", "Home")) : w_dup("Home");
        if (!home_name[0]) { free(home_name); home_name = w_dup("Home"); }
        json_set(hc, "chat_id", json_string(home_raw));
        json_set(hc, "name", json_string(home_name));
        json_set(out, "home_channel", hc);
        free(home_name);
    }
    free(home_raw);
    free(relay); free(key);
    return out;
}

/* _standalone_send: target choice, reply/media argv, escape retry, receipt. */
static json_t *w_send_error_obj(const char *message) {
    json_t *o = json_object();
    json_set(o, "error", json_string(message ? message : ""));
    return o;
}

typedef struct {
    json_t *calls;
    const json_t *script;
    size_t step;
    double rc;
    char *out;
    char *err;
    int exhausted;
} w_send_script;

static void w_send_call(w_send_script *S, const json_t *args, const char *input, const char *auth_tag) {
    json_t *call = json_object();
    json_t *argv = json_array();
    size_t i;
    for (i = 0; i < json_len(args); i++) json_append(argv, json_copy(json_get(args, i)));
    json_set(call, "args", argv);
    json_set(call, "input", json_string(input ? input : ""));
    json_set(call, "auth_tag", json_string(auth_tag ? auth_tag : ""));
    json_append(S->calls, call);
    if (!S->script || S->script->type != JSON_ARRAY || S->step >= json_len(S->script)) {
        S->exhausted = 1;
        return;
    }
    {
        const json_t *entry = json_get(S->script, S->step++);
        S->rc = json_number_value(json_get(entry, 0));
        free(S->out);
        S->out = w_dup(json_string_value(json_get(entry, 1)));
        free(S->err);
        S->err = w_dup(json_string_value(json_get(entry, 2)));
    }
}

json_t *w27_adapter_04abb5_standalone_send(const json_t *in) {
    const json_t *media = json_obj_get(in, "media_files");
    const char *relay = json_get_str(in, "relay", "");
    const char *key = json_get_str(in, "key", "");
    const char *cli = json_get_str(in, "cli_path", "");
    const char *chat_id = json_get_str(in, "chat_id", "");
    const char *home = json_get_str(in, "home_channel", "");
    const char *thread_id = json_get_str(in, "thread_id", "");
    const char *reply_mode = json_get_str(in, "reply_to_mode", "first");
    const char *message = json_get_str(in, "message", "");
    const char *auth_tag = json_get_str(in, "auth_tag", "");
    json_t *out = json_object(), *args;
    w_send_script S;
    char *target = NULL;
    size_t i;

    memset(&S, 0, sizeof(S));
    S.calls = json_array();
    S.script = json_obj_get(in, "script");
    S.out = w_dup("");
    S.err = w_dup("");

    if (!relay[0] || !key[0]) {
        json_set(out, "calls", S.calls);
        json_set(out, "result", w_send_error_obj("Buzz standalone send: BUZZ_RELAY_URL and BUZZ_PRIVATE_KEY must be configured"));
        free(S.out); free(S.err);
        return out;
    }
    if (!cli[0]) {
        json_set(out, "calls", S.calls);
        json_set(out, "result", w_send_error_obj("Buzz standalone send: buzz CLI binary not found"));
        free(S.out); free(S.err);
        return out;
    }
    {
        char *trimmed = w_strip(chat_id);
        if (trimmed[0]) {
            target = trimmed;
        } else {
            free(trimmed);
            target = w_strip(home);
        }
    }
    if (!target[0]) {
        json_set(out, "calls", S.calls);
        json_set(out, "result", w_send_error_obj("Buzz standalone send: no target channel (set BUZZ_HOME_CHANNEL)"));
        free(target); free(S.out); free(S.err);
        return out;
    }

    args = json_array();
    json_append(args, json_string("messages"));
    json_append(args, json_string("send"));
    json_append(args, json_string("--channel"));
    json_append(args, json_string(target));
    json_append(args, json_string("--content"));
    json_append(args, json_string("-"));
    {
        /* _reply_to_mode() normalises with str(...).strip().lower() before the
         * != "off" test, so "OFF"/" Off " post flat exactly like "off". */
        char *rs = w_strip(reply_mode);
        char *rl = w_lower(rs);
        free(rs);
        if (thread_id[0] && strcmp(rl, "off")) {
            json_append(args, json_string("--reply-to"));
            json_append(args, json_string(thread_id));
        }
        free(rl);
    }
    if (media && media->type == JSON_ARRAY) {
        for (i = 0; i < json_len(media); i++) {
            const json_t *m = json_get(media, i);
            char *file;
            if (m && m->type == JSON_ARRAY && json_len(m) > 0) file = w_pystr(json_get(m, 0));
            else file = w_pystr(m);
            json_append(args, json_string("--file"));
            json_append(args, json_string(file));
            free(file);
        }
    }

    w_send_call(&S, args, message, auth_tag);
    if (!S.exhausted && S.rc != 0) {
        json_t *probe = json_object();
        json_t *esc;
        json_set(probe, "content", json_string(message));
        json_set(probe, "error", json_string(S.err));
        esc = w27_adapter_04abb5_escape_mention(probe);
        json_free(probe);
        if (w_truthy(esc)) {
            const char *escaped = json_string_value(esc);
            /* the retry intentionally omits the auth tag */
            w_send_call(&S, args, escaped ? escaped : "", "");
            json_free(esc);
        } else {
            json_free(esc);
        }
    }

    if (S.exhausted) {
        json_set(out, "error", json_string("SCRIPT_EXHAUSTED"));
        json_set(out, "calls", S.calls);
        free(target); free(S.out); free(S.err);
        json_free(args);
        return out;
    }
    json_set(out, "calls", S.calls);
    if (S.rc != 0) {
        char *detail = w_cli_error(S.err, S.rc, NULL);
        char *msg = (char *)malloc(strlen(detail) + 64);
        sprintf(msg, "Buzz standalone send failed: %s", detail);
        json_set(out, "result", w_send_error_obj(msg));
        free(msg); free(detail);
    } else {
        char *event_id = NULL, *receipt_error = NULL;
        w_parse_receipt(S.out, &event_id, &receipt_error);
        if (receipt_error) {
            char *msg = (char *)malloc(strlen(receipt_error) + 64);
            sprintf(msg, "Buzz standalone send failed: %s", receipt_error);
            json_set(out, "result", w_send_error_obj(msg));
            free(msg); free(receipt_error);
        } else {
            json_t *result = json_object();
            json_set(result, "success", json_bool(1));
            json_set(result, "message_id", json_string(event_id ? event_id : ""));
            if (media && media->type == JSON_ARRAY && json_len(media) > 0)
                json_set(result, "media_delivered", json_bool(1));
            json_set(out, "result", result);
        }
        free(event_id);
    }
    free(target); free(S.out); free(S.err);
    json_free(args);
    return out;
}
