/*
 * standalone.c - C11 port of upstream plugins/platforms/telegram/standalone.py
 * (gate w33_standalone_9f791b).
 *
 * PoP: w33_standalone_9f791b_caption_split @ plugins/platforms/telegram/standalone.py:_media_caption_split
 * PoP: w33_standalone_9f791b_media_kind @ plugins/platforms/telegram/standalone.py:_telegram_send_media
 * PoP: w33_standalone_9f791b_album_shape @ plugins/platforms/telegram/standalone.py:_telegram_album_paths
 * PoP: w33_standalone_9f791b_thread_kwargs @ plugins/platforms/telegram/standalone.py:_telegram_thread_kwargs
 * PoP: w33_standalone_9f791b_thread_is_name @ plugins/platforms/telegram/standalone.py:_send_telegram
 * PoP: w33_standalone_9f791b_thread_not_found @ plugins/platforms/telegram/standalone.py:_is_telegram_thread_not_found
 * PoP: w33_standalone_9f791b_html_tag_present @ plugins/platforms/telegram/standalone.py:_telegram_format
 * PoP: w33_standalone_9f791b_escape_markup @ plugins/platforms/telegram/standalone.py:_escape_non_telegram_markup
 * PoP: w33_standalone_9f791b_fallback_ips @ plugins/platforms/telegram/standalone.py:_telegram_fallback_ips
 * PoP: w33_standalone_9f791b_resolve_token @ plugins/platforms/telegram/standalone.py:standalone_send
 *
 * The one-shot Bot API client is a seam: no bot, no request stack, no upload,
 * no filesystem, no network.  What is ported is what the values decide --
 * which text rides as a caption, which Bot API method an extension routes to,
 * whether a batch is an album, how a topic id becomes message_thread_id, when a
 * topic id is a name no direct send can resolve, when an error is a missing
 * topic, whether text carries Telegram HTML and how the rest of it is escaped,
 * which fallback IPs a send may try, and which token it uses.
 *
 * Scope note: whitespace trimming and the tag scanner's \s follow Python's
 * unicode str.strip()/re sets, and lengths count code points as Python does.
 * Non-ASCII digits are not digits here (C's isdigit), which only matters for
 * str.isdigit() on ids no Bot API caller sends.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_standalone_9f791b.h"

/* _IMAGE_EXTS / _VIDEO_EXTS / _VOICE_EXTS / _TELEGRAM_SEND_AUDIO_EXTS */
static const char *const TG_IMAGE_EXTS[] = { ".jpg", ".jpeg", ".png", ".webp", ".gif" };
static const char *const TG_VIDEO_EXTS[] = { ".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp" };
static const char *const TG_VOICE_EXTS[] = { ".ogg", ".opus" };
static const char *const TG_AUDIO_EXTS[] = { ".mp3", ".m4a" };
/* _CAPTIONABLE_EXTS = _IMAGE_EXTS | _VIDEO_EXTS | {documents} */
static const char *const TG_CAPTIONABLE_EXTS[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif",
    ".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp",
    ".pdf", ".doc", ".docx", ".txt", ".md", ".csv", ".xlsx", ".zip",
};
/* SEED_FALLBACK_IPS: no DoH discovery on a one-shot send. */
static const char *const TG_SEED_FALLBACK_IPS[] = { "149.154.166.110", "149.154.167.220" };
/* _TELEGRAM_HTML_TAGS, longest first, exactly as the upstream alternation is
 * built (sorted(_TELEGRAM_HTML_TAGS, key=len, reverse=True) is stable). */
static const char *const TG_HTML_TAGS[] = {
    "blockquote", "tg-spoiler", "tg-emoji", "strong", "strike", "code",
    "span", "ins", "del", "pre", "em", "b", "i", "u", "s", "a",
};
#define TG_NTAGS (sizeof(TG_HTML_TAGS) / sizeof(*TG_HTML_TAGS))

/* _TELEGRAM_CAPTION_LIMIT (photo/video/document captions) */
#define TG_CAPTION_LIMIT 1024

/* ---------------------------------------------------------------- helpers */

/* Python truthiness of a JSON value. */
static int tg_truthy(const json_t *v) {
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

/* str.strip()'s default set: ASCII whitespace plus the non-ASCII code points
 * str.isspace() reports (NBSP, NEL, the U+2000 block, U+3000). */

/* One UTF-8 sequence at s: its byte length, with the code point in *cp
 * (a stray continuation/lead byte decodes as itself, length 1). */
static size_t tg_utf8_at(const char *s, unsigned *cp) {
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) { *cp = u[0]; return 1; }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x1F) << 6) | (unsigned)(u[1] & 0x3F);
        return 2;
    }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) |
              (unsigned)(u[2] & 0x3F);
        return 3;
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 &&
        (u[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
              ((unsigned)(u[2] & 0x3F) << 6) | (unsigned)(u[3] & 0x3F);
        return 4;
    }
    *cp = u[0];
    return 1;
}

static int tg_py_ws_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D)) return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F) return 1;
    return cp == 0x3000;
}

/* Byte length of the whitespace character at p, or 0. */
static size_t tg_ws_len(const char *p) {
    unsigned cp;
    size_t n = tg_utf8_at(p, &cp);
    return tg_py_ws_cp(cp) ? n : 0;
}

/* Advance over leading Python whitespace. */
static void tg_skip_ws(const char **p) {
    size_t n;
    while (**p && (n = tg_ws_len(*p)) != 0) *p += n;
}

static char *tg_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* (s or "").strip() as a fresh buffer. */
static char *tg_trim_dup(const char *s) {
    size_t a = 0, b;
    char *p;
    if (!s) s = "";
    b = strlen(s);
    {
        size_t n;
        while (a < b && (n = tg_ws_len(s + a)) != 0) a += n;
    }
    while (b > a) {
        size_t k = b - 1;
        while (k > a && ((unsigned char)s[k] & 0xC0) == 0x80) k--;
        if (!tg_ws_len(s + k)) break;
        b = k;
    }
    p = (char *)malloc(b - a + 1);
    if (!p) return NULL;
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}

static char *tg_lower_dup(const char *s) {
    char *p = tg_dup(s ? s : "");
    char *q;
    if (!p) return NULL;
    for (q = p; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    return p;
}

/* Python len(): code points, not bytes. */
static size_t tg_char_len(const char *s) {
    size_t n = 0;
    for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

/* str(value) for the scalar shapes a thread id / env value can hold. */
static char *tg_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tg_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return tg_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return tg_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return tg_dup(buf);
    default:          return tg_dup("");
    }
}

/* int(s) for a Python-shaped integer literal: surrounding whitespace, one
 * optional sign, digits with Python's '_' separators.  0 when not one. */
static int tg_parse_int(const char *s, long long *out) {
    long long v = 0;
    int neg = 0, digits = 0, prev_digit = 0;
    tg_skip_ws(&s);
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    for (; *s; s++) {
        if (*s == '_') {
            if (!prev_digit) return 0;
            prev_digit = 0;
            continue;
        }
        if (!isdigit((unsigned char)*s)) break;
        if (v > (9223372036854775807LL - 9) / 10) return 0;
        v = v * 10 + (*s - '0');
        digits++;
        prev_digit = 1;
    }
    tg_skip_ws(&s);
    if (!digits || *s != '\0' || !prev_digit) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* os.path.splitext(path)[1] (ntpath: both separators, leading dots are not an
 * extension).  A pointer at the '.' or at the terminating NUL when there is
 * no extension. */
static const char *tg_ext(const char *path) {
    size_t i, sep = (size_t)-1, dot = (size_t)-1, k;
    if (!path) return "";
    for (i = 0; path[i]; i++) {
        if (path[i] == '/' || path[i] == '\\') sep = i;
        else if (path[i] == '.') dot = i;
    }
    if (dot == (size_t)-1) return path + strlen(path);
    if (sep != (size_t)-1 && dot < sep) return path + strlen(path);
    for (k = (sep == (size_t)-1 ? 0 : sep + 1); k < dot; k++)
        if (path[k] != '.') return path + dot;
    return path + strlen(path);
}

static int tg_in_set(const char *ext, const char *const *set, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) if (!strcmp(ext, set[i])) return 1;
    return 0;
}

#define TG_IN(ext, set) tg_in_set((ext), (set), sizeof(set) / sizeof(*(set)))

static int tg_ncasecmp(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y - 'A' + 'a');
        if (x != y || !x) return 1;
    }
    return 0;
}

/* --------------------------------------------------------- string builder */

typedef struct {
    char *p;
    size_t n, cap;
} tg_buf;

static void tg_buf_add(tg_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 128;
        char *p;
        while (cap < b->n + n + 1) cap *= 2;
        p = (char *)realloc(b->p, cap);
        if (!p) return;
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void tg_buf_puts(tg_buf *b, const char *s) { tg_buf_add(b, s, strlen(s)); }
static void tg_buf_putc(tg_buf *b, char c) { tg_buf_add(b, &c, 1); }

/* ------------------------------------------- Telegram HTML tag scanning */

/* End offset (exclusive) of the tag starting at text[i] == '<', or 0.
 * Mirrors the upstream regex
 *   </?(?:blockquote|tg-spoiler|tg-emoji|strong|strike|code|span|ins|del|pre|em|b|i|u|s|a)
 *      (?:\s(?:[^>"']++|"[^"]*+"|'[^']*+')*+)?>
 * under re.IGNORECASE: the name must end at '>' or whitespace, a quoted
 * attribute value may hold '>', and a failed alternative backtracks. */
static size_t tg_tag_end(const char *text, size_t i) {
    size_t k;
    for (k = 0; k < TG_NTAGS; k++) {
        size_t len = strlen(TG_HTML_TAGS[k]);
        size_t m = i + 1;
        if (text[m] == '/') m++;
        if (tg_ncasecmp(text + m, TG_HTML_TAGS[k], len)) continue;
        m += len;
        if (text[m] == '>') return m + 1;
        {
            size_t w = tg_ws_len(text + m);   /* re's \s is unicode-aware */
            if (!w) continue;                 /* the name ran into prose */
            m += w;
        }
        for (;;) {
            char c = text[m];
            if (c == '>') return m + 1;
            if (c == '\0') return 0;
            if (c == '"' || c == '\'') {
                const char *close = strchr(text + m + 1, c);
                if (!close) return 0;       /* unterminated quote: no tag here */
                m = (size_t)(close - text) + 1;
                continue;
            }
            m++;
        }
    }
    return 0;
}

/* Length of `&(?:#\d+|#x[0-9a-fA-F]+|[a-zA-Z]+);` at p, or 0. */
static size_t tg_entity_len(const char *p) {
    size_t i = 1, j;
    if (p[0] != '&') return 0;
    if (p[i] == '#') {
        if (p[i + 1] == 'x') {
            j = i + 2;
            while (isxdigit((unsigned char)p[j])) j++;
            return (j > i + 2 && p[j] == ';') ? j + 1 : 0;
        }
        j = i + 1;
        while (isdigit((unsigned char)p[j])) j++;
        return (j > i + 1 && p[j] == ';') ? j + 1 : 0;
    }
    j = i;
    while (isalpha((unsigned char)p[j])) j++;
    return (j > i && p[j] == ';') ? j + 1 : 0;
}

/* Everything but an existing entity: & -> &amp;, < -> &lt;, > -> &gt;. */
static void tg_escape_into(tg_buf *b, const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        char c = s[i];
        if (c == '&') {
            size_t e = tg_entity_len(s + i);
            if (e && i + e <= n) { tg_buf_add(b, s + i, e); i += e; continue; }
            tg_buf_puts(b, "&amp;");
        } else if (c == '<') {
            tg_buf_puts(b, "&lt;");
        } else if (c == '>') {
            tg_buf_puts(b, "&gt;");
        } else {
            tg_buf_add(b, s + i, 1);
        }
        i++;
    }
}

static void tg_escape_text(tg_buf *b, const char *text) {
    size_t i = 0, pos = 0;
    for (i = 0; text[i];) {
        size_t end;
        if (text[i] != '<') { i++; continue; }
        end = tg_tag_end(text, i);
        if (!end) { i++; continue; }
        tg_escape_into(b, text + pos, i - pos);
        tg_buf_add(b, text + i, end - i);   /* the tag itself stays verbatim */
        i = end;
        pos = end;
    }
    tg_escape_into(b, text + pos, strlen(text) - pos);
}

/* --------------------------------------------------- IPv4 normalization */

/* Strict dotted quad: four decimal parts, no leading zeros (ipaddress's rule). */
static int tg_ipv4(const char *s, unsigned char oct[4]) {
    int i;
    for (i = 0; i < 4; i++) {
        const char *p = s;
        int v = 0, digits = 0;
        if (!isdigit((unsigned char)*p)) return 0;
        if (*p == '0' && isdigit((unsigned char)p[1])) return 0;
        while (isdigit((unsigned char)*p)) {
            v = v * 10 + (*p - '0');
            if (++digits > 3 || v > 255) return 0;
            p++;
        }
        oct[i] = (unsigned char)v;
        if (i == 3) return *p == '\0';
        if (*p != '.') return 0;
        s = p + 1;
    }
    return 0;
}

/* ipaddress's is_private / is_loopback / is_link_local / is_unspecified. */
static int tg_ipv4_internal(const unsigned char o[4]) {
    if (o[0] == 0) return 1;                                  /* 0.0.0.0/8 */
    if (o[0] == 10) return 1;                                 /* 10/8 */
    if (o[0] == 127) return 1;                                /* loopback */
    if (o[0] == 169 && o[1] == 254) return 1;                 /* link-local */
    if (o[0] == 172 && (o[1] & 0xF0) == 16) return 1;         /* 172.16/12 */
    if (o[0] == 192 && o[1] == 0 && o[2] == 0) return 1;      /* 192.0.0.0/24 */
    if (o[0] == 192 && o[1] == 0 && o[2] == 2) return 1;      /* TEST-NET-1 */
    if (o[0] == 192 && o[1] == 168) return 1;                 /* 192.168/16 */
    if (o[0] == 198 && (o[1] == 18 || o[1] == 19)) return 1;  /* 198.18/15 */
    if (o[0] == 198 && o[1] == 51 && o[2] == 100) return 1;   /* TEST-NET-2 */
    if (o[0] == 203 && o[1] == 0 && o[2] == 113) return 1;    /* TEST-NET-3 */
    if (o[0] >= 240) return 1;                                /* 240/4, broadcast */
    return 0;
}

/* _normalize_fallback_ips over one comma-separated string. */
static void tg_parse_ips(const char *value, json_t *out) {
    const char *p = value;
    while (p && *p) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char *part = (char *)malloc(n + 1);
        if (part) {
            unsigned char o[4];
            char canon[24];
            char *t;
            memcpy(part, p, n);
            part[n] = '\0';
            t = tg_trim_dup(part);
            if (t && t[0] && tg_ipv4(t, o) && !tg_ipv4_internal(o)) {
                snprintf(canon, sizeof(canon), "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
                json_append(out, json_string(canon));
            }
            free(t);
            free(part);
        }
        if (!comma) break;
        p = comma + 1;
    }
}

/* ================================================================= symbols */

json_t *w33_standalone_9f791b_caption_split(const json_t *in) {
    const json_t *textv = json_obj_get(in, "text");
    const json_t *media = json_obj_get(in, "media");
    const char *text = (textv && textv->type == JSON_STRING) ? textv->str_val : NULL;
    size_t nmedia = (media && media->type == JSON_ARRAY) ? json_len(media) : 0;
    char *stripped = tg_trim_dup(text ? text : "");
    json_t *out = json_object();
    int caption = 0;

    if (stripped && stripped[0] && nmedia == 1) {
        const json_t *entry = json_get(media, 0);
        if (entry && entry->type == JSON_ARRAY && json_len(entry) >= 1) {
            const json_t *pathv = json_get(entry, 0);
            const char *path = (pathv && pathv->type == JSON_STRING) ? pathv->str_val : "";
            int voice = tg_truthy(json_len(entry) >= 2 ? json_get(entry, 1) : NULL);
            char *ext = tg_lower_dup(tg_ext(path));
            /* exactly one captionable file, not a voice/audio note, and the
             * (unformatted) text inside the caption cap */
            if (ext && !voice && TG_IN(ext, TG_CAPTIONABLE_EXTS) &&
                tg_char_len(stripped) <= TG_CAPTION_LIMIT)
                caption = 1;
            free(ext);
        }
    }
    if (caption) {
        json_set(out, "caption", json_string(stripped));
        json_set(out, "text", json_string(""));
    } else {
        /* (None, text): multi-file caption association is ambiguous, and the
         * caller re-checks the *formatted* caption in UTF-16 units. */
        json_set(out, "caption", json_null());
        /* Python returns `text` itself -- an empty list, 0 or {} included. */
        json_set(out, "text", textv ? json_copy(textv) : json_null());
    }
    free(stripped);
    return out;
}

json_t *w33_standalone_9f791b_media_kind(const json_t *in) {
    const char *path = json_get_str(in, "path", "");
    int is_voice = tg_truthy(json_obj_get(in, "is_voice"));
    int force_document = tg_truthy(json_obj_get(in, "force_document"));
    char *ext = tg_lower_dup(tg_ext(path));
    const char *kind = "document";

    if (!ext) return json_string("document");
    if (!force_document && TG_IN(ext, TG_IMAGE_EXTS)) kind = "photo";
    else if (TG_IN(ext, TG_VIDEO_EXTS)) kind = "video";
    else if (is_voice && TG_IN(ext, TG_VOICE_EXTS)) kind = "voice";
    else if (TG_IN(ext, TG_AUDIO_EXTS)) kind = "audio";
    free(ext);
    return json_string(kind);
}

json_t *w33_standalone_9f791b_album_shape(const json_t *in) {
    const json_t *media = json_obj_get(in, "media");
    const json_t *caption = json_obj_get(in, "caption");
    int force_document = tg_truthy(json_obj_get(in, "force_document"));
    size_t n = (media && media->type == JSON_ARRAY) ? json_len(media) : 0;
    size_t i;

    /* A forced document, any caption or a single file keeps the per-file path. */
    if (force_document || (caption && caption->type != JSON_NULL) || n < 2)
        return json_bool(0);
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(media, i);
        const char *path = "";
        char *ext;
        if (!entry || entry->type != JSON_ARRAY || json_len(entry) < 2)
            return json_bool(0);                       /* not a (path, voice) pair */
        if (tg_truthy(json_get(entry, 1)))
            return json_bool(0);                       /* a voice note */
        if (json_get(entry, 0) && json_get(entry, 0)->type == JSON_STRING)
            path = json_get(entry, 0)->str_val;
        ext = tg_lower_dup(tg_ext(path));
        /* still photos only: a GIF animates and is not album-able here */
        if (!ext || !TG_IN(ext, TG_IMAGE_EXTS) || !strcmp(ext, ".gif")) {
            free(ext);
            return json_bool(0);
        }
        free(ext);
        /* os.path.exists(path) / os.path.abspath() are the caller's seams */
    }
    return json_bool(1);
}

json_t *w33_standalone_9f791b_thread_kwargs(const json_t *in) {
    const json_t *tid = json_obj_get(in, "thread_id");
    json_t *out = json_object();
    char *s;
    long long v;

    if (!tid || tid->type == JSON_NULL) return out;   /* thread_id is None -> {} */
    s = tg_pystr(tid);
    if (!s) return out;
    /* adapter._message_thread_id_for_send: falsy or "1" (forum General, which
     * the Bot API rejects) maps to no thread at all. */
    if (!s[0] || !strcmp(s, "1")) { free(s); return out; }
    if (!tg_parse_int(s, &v)) {
        /* int(thread_id) raises ValueError in Python: no send is attempted. */
        free(s);
        json_set(out, "error", json_string("ValueError"));
        return out;
    }
    free(s);
    json_set(out, "message_thread_id", json_number((double)v));
    return out;
}

json_t *w33_standalone_9f791b_thread_is_name(const json_t *in) {
    const json_t *tid = json_obj_get(in, "thread_id");
    char *raw = tg_truthy(tid) ? tg_pystr(tid) : tg_dup("");
    char *s = tg_trim_dup(raw ? raw : "");
    const char *p;
    int digits;
    json_t *out;

    free(raw);
    if (!s) return json_bool(0);
    p = s;
    while (*p == '-') p++;            /* str.lstrip("-") drops every leading dash */
    digits = (*p != '\0');
    for (; *p; p++) if (!isdigit((unsigned char)*p)) { digits = 0; break; }
    /* A name ("general", "12a", "-") parses for every caller, but only the live
     * gateway's cron router can resolve it: the Bot API cannot (#80483). */
    out = json_bool(s[0] != '\0' && !digits);
    free(s);
    return out;
}

json_t *w33_standalone_9f791b_thread_not_found(const json_t *in) {
    const char *msg = json_get_str(in, "error", "");
    char *low = tg_lower_dup(msg);
    int found = low && strstr(low, "thread not found") != NULL;
    free(low);
    return json_bool(found);
}

json_t *w33_standalone_9f791b_html_tag_present(const json_t *in) {
    const char *text = json_get_str(in, "text", "");
    size_t i;
    for (i = 0; text[i]; i++)
        if (text[i] == '<' && tg_tag_end(text, i)) return json_bool(1);
    return json_bool(0);
}

json_t *w33_standalone_9f791b_escape_markup(const json_t *in) {
    const char *text = json_get_str(in, "text", "");
    tg_buf b = { NULL, 0, 0 };
    json_t *out;

    tg_escape_text(&b, text);
    out = json_string(b.p ? b.p : "");
    free(b.p);
    return out;
}

json_t *w33_standalone_9f791b_fallback_ips(const json_t *in) {
    const json_t *env = json_obj_get(in, "env");
    const json_t *configured = json_obj_get(in, "configured");
    const char *disable = "";
    json_t *out = json_array();
    size_t i;

    if (env && env->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(env, "HERMES_TELEGRAM_DISABLE_FALLBACK_IPS");
        if (v && v->type == JSON_STRING) disable = v->str_val;
    }
    {
        char *t = tg_trim_dup(disable);
        int off = 0;
        if (t) {
            char *q;
            for (q = t; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
            off = !strcmp(t, "1") || !strcmp(t, "true") || !strcmp(t, "yes") ||
                  !strcmp(t, "on");
        }
        free(t);
        if (off) return out;   /* opted out: no fallback transport at all */
    }
    if (configured && configured->type == JSON_STRING) {
        if (configured->str_val && configured->str_val[0])
            tg_parse_ips(configured->str_val, out);
    } else if (configured && configured->type == JSON_ARRAY && json_len(configured) > 0) {
        /* ",".join(str(ip) for ip in configured) */
        tg_buf joined = { NULL, 0, 0 };
        for (i = 0; i < json_len(configured); i++) {
            char *s = tg_pystr(json_get(configured, i));
            if (i) tg_buf_putc(&joined, ',');
            if (s) tg_buf_puts(&joined, s);
            free(s);
        }
        if (joined.p) tg_parse_ips(joined.p, out);
        free(joined.p);
    }
    /* parse_fallback_ip_env(configured) or list(SEED_FALLBACK_IPS) */
    if (json_len(out) == 0) {
        for (i = 0; i < sizeof(TG_SEED_FALLBACK_IPS) / sizeof(*TG_SEED_FALLBACK_IPS); i++)
            json_append(out, json_string(TG_SEED_FALLBACK_IPS[i]));
    }
    return out;
}

json_t *w33_standalone_9f791b_resolve_token(const json_t *in) {
    const json_t *cfg = json_obj_get(in, "config_token");
    const json_t *scoped = json_obj_get(in, "scoped_token");

    /* pconfig.token, else the profile-scoped TELEGRAM_BOT_TOKEN (never another
     * profile's), else "". The value is passed to the sender UNCHANGED -- a
     * non-string token stays a number/object, so str()-ing it here made
     * resolve_token answer '7' where Python answers 7. */
    if (tg_truthy(cfg)) return json_copy(cfg);
    if (tg_truthy(scoped)) return json_copy(scoped);
    return json_string("");
}
