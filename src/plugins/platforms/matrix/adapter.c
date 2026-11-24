/*
 * adapter.c - C11 port of the pure core of upstream
 * plugins/platforms/matrix/adapter.py (gates w27_adapter_e9cb6b_p1.._p6).
 *
 * PORTED (value-in / value-out, no socket, no file, no clock, no SDK):
 *   p1  bang_split, bang_candidates, reply_fallback_extract,
 *       reply_fallback_strip, reply_fallback_split, permanent_auth_error,
 *       utf8_len, max_message_length, looks_like_transport_filename,
 *       is_bare_media_filename, event_timestamp_seconds, redact_url_for_log
 *   p2  normalize_e2ee_mode, resolve_e2ee_mode, env_truthy, env_number,
 *       csv_set, thread_root, extra_truthy, configured_bool,
 *       parse_require_mention, parse_thread_require_mention,
 *       extract_server_ed25519, is_connected, session_scope,
 *       reactions_enabled
 *   p3  exec_approval_tier, approval_reaction, approval_action,
 *       flat_picker_choices, choice_picker_emoji, apply_relation_metadata,
 *       protect_mention_regions, extract_outbound_mentions,
 *       has_outbound_room_mention, inject_outbound_mention_links,
 *       is_bot_mentioned, user_localpart, strip_mention, mxc_to_http,
 *       sanitize_link_url, format_message
 *   p4  is_self_sender, is_system_or_bridge_sender, classify_inbound_media,
 *       voice_may_park, content_mentions_bot, extract_invite_dm_signal,
 *       matrix_prompt_expired, is_authorized_user, reactor_allowed,
 *       state_event_value, room_server_name, room_identity, presence_valid
 *   p5  html_escape, markdown_to_html_fallback
 *   p6  constants, model_picker_reactions, choice_picker_reactions,
 *       approval_reaction_map, yaml_bridge, image_exts, media_exts, txn_id,
 *       duplicate_events
 *
 * SEAMED (left in the caller: mautrix client, aiohttp, sqlite/nio state,
 * crypto store, subprocess media probing, i18n, config/registry readers):
 *   _run_media_tool, _matrix_voice_metadata_for_file, _resolve_matrix_bang_command
 *   (command/skill registry lookup), _normalize_matrix_bang_command (drives the
 *   registry), _content_bytes (json.dumps byte-exact), _resolve_max_message_length's
 *   registry fallback (the clamp is ported), _create_matrix_session, _check_e2ee_deps,
 *   _recovery_key_output_path/_write_.../_get_..._target (filesystem),
 *   matrix_deps_present/check_matrix_requirements/ensure_matrix_deps (package manager),
 *   every connect/sync/send/upload/download/edit/react/redact/room/user/typing/receipt
 *   coroutine on MatrixAdapter, _markdown_to_html (delegates to adapter_html's
 *   sanitizer, a separate module), _has_voice_marker/is_voice_event (voice_mention),
 *   _standalone_send/_standalone_payloads (truncate_message + HTTP),
 *   _apply_yaml_config (gateway.platforms._shared), _is_connected's env reader,
 *   interactive_setup, register.
 *
 * PoP markers sit above each ported symbol.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hermes_json.h"

/* ------------------------------------------------------------------ */
/* shared helpers: Python value semantics over json_t                  */
/* ------------------------------------------------------------------ */

/* Python truthiness. */
static int w27_truthy(const json_t *v) {
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

static const char *w27_cstr(const json_t *v) {
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return "";
}

/* Python's numeric view: int/float/bool; None and strings are not numbers. */
static int w27_is_number(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static double w27_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* --- growable byte buffer ---------------------------------------- */
typedef struct { char *p; size_t n, cap; } w27_sb;

static void w27_sb_init(w27_sb *b) { b->p = NULL; b->n = b->cap = 0; }

static void w27_sb_reserve(w27_sb *b, size_t need) {
    if (b->n + need + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->n + need + 1) cap *= 2;
    char *np = (char *)realloc(b->p, cap);
    if (!np) return;
    b->p = np; b->cap = cap;
}

static void w27_sb_putn(w27_sb *b, const char *s, size_t n) {
    if (!s || !n) return;
    w27_sb_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void w27_sb_put(w27_sb *b, const char *s) { if (s) w27_sb_putn(b, s, strlen(s)); }
static void w27_sb_putc(w27_sb *b, char c) { w27_sb_putn(b, &c, 1); }

static void w27_sb_puts(w27_sb *b, const w27_sb *src) { w27_sb_putn(b, src->p, src->n); }

static char *w27_sb_take(w27_sb *b) {
    if (!b->p) { char *z = (char *)malloc(1); if (z) z[0] = '\0'; return z; }
    char *r = b->p;
    b->p = NULL; b->n = b->cap = 0;
    return r;
}

static void w27_sb_free(w27_sb *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

static char *w27_strdup2(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    if (n) memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

/* Python str.strip() over ASCII whitespace. */
static char *w27_strip(const char *s) {
    if (!s) return w27_strdup2("", 0);
    const char *a = s, *b = s + strlen(s);
    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f') a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' ||
                     b[-1] == '\r' || b[-1] == '\v' || b[-1] == '\f')) b--;
    return w27_strdup2(a, (size_t)(b - a));
}

static int w27_isspace_ascii(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python repr()/str() of a scalar, good enough for the call sites that do
 * str(value) on config scalars.  Numbers lose int/float identity in json_t,
 * so an integral value prints without a decimal point. */
static void w27_pystr_into(w27_sb *b, const json_t *v);

static void w27_pyrepr_into(w27_sb *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { w27_sb_put(b, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:   w27_sb_put(b, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: {
        char tmp[64];
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 1e16)
            snprintf(tmp, sizeof tmp, "%lld", (long long)d);
        else
            snprintf(tmp, sizeof tmp, "%g", d);
        w27_sb_put(b, tmp);
        return;
    }
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char quote = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
        w27_sb_putc(b, quote);
        for (const char *q = s; *q; q++) {
            if (*q == '\\') w27_sb_put(b, "\\\\");
            else if (*q == quote) { w27_sb_putc(b, '\\'); w27_sb_putc(b, quote); }
            else if (*q == '\n') w27_sb_put(b, "\\n");
            else w27_sb_putc(b, *q);
        }
        w27_sb_putc(b, quote);
        return;
    }
    case JSON_ARRAY: {
        w27_sb_putc(b, '[');
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) w27_sb_put(b, ", ");
            w27_pyrepr_into(b, json_get(v, i));
        }
        w27_sb_putc(b, ']');
        return;
    }
    case JSON_OBJECT: {
        w27_sb_putc(b, '{');
        for (size_t i = 0; i < json_object_size(v); i++) {
            if (i) w27_sb_put(b, ", ");
            const char *k = json_object_get_key_at(v, i);
            w27_sb_putc(b, '\'');
            w27_sb_put(b, k ? k : "");
            w27_sb_put(b, "': ");
            w27_pyrepr_into(b, json_object_get_at(v, i));
        }
        w27_sb_putc(b, '}');
        return;
    }
    }
}

static void w27_pystr_into(w27_sb *b, const json_t *v) {
    if (v && v->type == JSON_STRING) { w27_sb_put(b, v->str_val ? v->str_val : ""); return; }
    w27_pyrepr_into(b, v);
}

static char *w27_pystr(const json_t *v) {
    w27_sb b; w27_sb_init(&b);
    w27_pystr_into(&b, v);
    return w27_sb_take(&b);
}

/* Python int(text) for the decimal-literal shapes int() accepts; returns 0 on
 * a ValueError, storing 1 in *ok.  Underscore separators are honoured. */
static long long w27_py_int(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0;
    while (w27_isspace_ascii((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    long long val = 0;
    int digits = 0, prev_digit = 0;
    for (; *s; s++) {
        if (*s == '_') { if (!prev_digit) return 0; prev_digit = 0; continue; }
        if (*s < '0' || *s > '9') break;
        if (val > (long long)1e17) return 0;
        val = val * 10 + (*s - '0');
        digits++; prev_digit = 1;
    }
    if (!digits || !prev_digit) return 0;
    while (w27_isspace_ascii((unsigned char)*s)) s++;
    if (*s) return 0;
    *ok = 1;
    return neg ? -val : val;
}

/* Python float(text) for plain decimal/exponent literals. */
static double w27_py_float(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0.0;
    while (w27_isspace_ascii((unsigned char)*s)) s++;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0.0;
    while (end && w27_isspace_ascii((unsigned char)*end)) end++;
    if (end && *end) return 0.0;
    *ok = 1;
    return v;
}

/* JSON array of strings, sorted (Python set -> sorted list in the oracle). */
static json_t *w27_sorted_array(char **items, size_t n) {
    json_t *out = json_array();
    for (size_t i = 0; i + 1 < n; i++)
        for (size_t j = 0; j + 1 < n - i; j++)
            if (strcmp(items[j], items[j + 1]) > 0) {
                char *t = items[j]; items[j] = items[j + 1]; items[j + 1] = t;
            }
    for (size_t i = 0; i < n; i++) json_append(out, json_string(items[i]));
    return out;
}

/* ------------------------------------------------------------------ */
/* p1: bang commands, reply fallbacks, limits, timestamps              */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_bang_split @ plugins/platforms/matrix/adapter.py:_MATRIX_BANG_COMMAND_RE */
json_t *w27_adapter_bang_split(const json_t *text) {
    const char *s = w27_cstr(text);
    size_t i = 0;
    if (!s[0] || s[0] != '!') return json_null();
    i = 1;
    if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) return json_null();
    size_t start = i;
    i++;
    while (s[i] && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') ||
                    (s[i] >= '0' && s[i] <= '9') || s[i] == '_' || s[i] == '-')) i++;
    /* (?=$|\s) */
    if (s[i] != '\0' && !w27_isspace_ascii((unsigned char)s[i])) return json_null();
    json_t *o = json_object();
    json_set(o, "name", json_string(w27_strdup2(s + start, i - start)));
    json_set(o, "rest", json_string(w27_strdup2(s + i, strlen(s + i))));
    return o;
}

/* PoP: w27_adapter_bang_candidates @ plugins/platforms/matrix/adapter.py:_resolve_matrix_bang_command */
json_t *w27_adapter_bang_candidates(const json_t *name) {
    json_t *arr = json_array();
    const char *s = w27_cstr(name);
    char *low = w27_strdup2(s, strlen(s));
    for (char *q = low; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    char *alt = w27_strdup2(low, strlen(low));
    for (char *q = alt; *q; q++) if (*q == '_') *q = '-';
    json_append(arr, json_string(low));
    if (strcmp(alt, low)) json_append(arr, json_string(alt));
    free(low); free(alt);
    return arr;
}

/* PoP: w27_adapter_reply_fallback_extract @ plugins/platforms/matrix/adapter.py:_extract_reply_fallback */
json_t *w27_adapter_reply_fallback_extract(const json_t *body) {
    json_t *out = json_array();
    const char *s = w27_cstr(body);
    if (!s[0] || strncmp(s, "> ", 2) != 0) {
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    w27_sb quoted; w27_sb_init(&quoted);
    char *author = NULL;
    int first = 1;
    const char *line = s;
    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? (size_t)(eol - line) : strlen(line);
        if (len < 2 || line[0] != '>' || line[1] != ' ') break;
        const char *content = line + 2;
        size_t clen = len - 2;
        if (!author) {
            /* ^>\s*<(@[^>]+)>\s*(.*)$ against the whole line */
            size_t k = 1;
            while (k < len && w27_isspace_ascii((unsigned char)line[k])) k++;
            if (k < len && line[k] == '<') {
                size_t m = k + 1;
                if (m < len && line[m] == '@') {
                    size_t q = m + 1;
                    while (q < len && line[q] != '>') q++;
                    if (q < len && q > m + 1) {
                        author = w27_strdup2(line + m, q - m);      /* includes '@' */
                        size_t r = q + 1;
                        while (r < len && w27_isspace_ascii((unsigned char)line[r])) r++;
                        content = line + r;
                        clen = len - r;
                    }
                }
            }
        }
        if (!first) w27_sb_putc(&quoted, '\n');
        w27_sb_putn(&quoted, content, clen);
        first = 0;
        if (!eol) break;
        line = eol + 1;
    }
    char *raw = w27_sb_take(&quoted);
    char *trimmed = w27_strip(raw);
    json_append(out, trimmed[0] ? json_string(trimmed) : json_null());
    json_append(out, author ? json_string(author) : json_null());
    free(raw); free(trimmed); free(author);
    return out;
}

/* PoP: w27_adapter_reply_fallback_strip @ plugins/platforms/matrix/adapter.py:_strip_reply_fallback */
json_t *w27_adapter_reply_fallback_strip(const json_t *body) {
    const char *s = w27_cstr(body);
    int is_null = !body || body->type == JSON_NULL;
    if (!s[0] || strncmp(s, "> ", 2) != 0) return is_null ? json_null() : json_string(s);
    w27_sb out; w27_sb_init(&out);
    int past = 0, any = 0;
    const char *line = s;
    for (;;) {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? (size_t)(eol - line) : strlen(line);
        int skip = 0;
        if (!past) {
            if ((len >= 2 && line[0] == '>' && line[1] == ' ') || (len == 1 && line[0] == '>')) {
                skip = 1;
            } else {
                past = 1;
                if (len == 0) skip = 1;
            }
        }
        if (!skip) {
            if (any) w27_sb_putc(&out, '\n');
            w27_sb_putn(&out, line, len);
            any = 1;
        }
        if (!eol) break;
        line = eol + 1;
    }
    char *joined = w27_sb_take(&out);
    json_t *res = any ? json_string(joined) : json_string(s);
    free(joined);
    return res;
}

/* PoP: w27_adapter_reply_fallback_split @ plugins/platforms/matrix/adapter.py:_split_reply_fallback */
json_t *w27_adapter_reply_fallback_split(const json_t *body) {
    json_t *out = json_array();
    const char *s = w27_cstr(body);
    int is_null = !body || body->type == JSON_NULL;
    if (!s[0] || strncmp(s, "> ", 2) != 0) {
        json_append(out, json_string(""));
        json_append(out, is_null ? json_null() : json_string(s));
        return out;
    }
    /* line offsets */
    size_t nlines = 1;
    for (const char *q = s; *q; q++) if (*q == '\n') nlines++;
    size_t *off = (size_t *)calloc(nlines + 1, sizeof(size_t));
    size_t *len = (size_t *)calloc(nlines + 1, sizeof(size_t));
    if (!off || !len) { free(off); free(len); json_append(out, json_string("")); json_append(out, json_string(s)); return out; }
    size_t idx = 0, line_start = 0;
    for (const char *q = s;; q++) {
        if (*q == '\n' || *q == '\0') {
            off[idx] = line_start; len[idx] = (size_t)(q - (s + line_start)); idx++;
            if (*q == '\0') break;
            line_start = (size_t)(q - s) + 1;
        }
    }
    size_t i = 0;
    while (i < idx && ((len[i] >= 2 && s[off[i]] == '>' && s[off[i] + 1] == ' ') ||
                       (len[i] == 1 && s[off[i]] == '>'))) i++;
    if (i < idx && len[i] == 0) i++;
    w27_sb head; w27_sb_init(&head);
    for (size_t k = 0; k < i; k++) {
        if (k) w27_sb_putc(&head, '\n');
        w27_sb_putn(&head, s + off[k], len[k]);
    }
    char *h = w27_sb_take(&head);
    if (i >= idx) {
        json_append(out, json_string(h));
        json_append(out, json_string(""));
    } else {
        w27_sb hh; w27_sb_init(&hh);
        w27_sb_put(&hh, h);
        w27_sb_putc(&hh, '\n');
        char *h2 = w27_sb_take(&hh);
        w27_sb tail; w27_sb_init(&tail);
        for (size_t k = i; k < idx; k++) {
            if (k > i) w27_sb_putc(&tail, '\n');
            w27_sb_putn(&tail, s + off[k], len[k]);
        }
        char *t = w27_sb_take(&tail);
        json_append(out, json_string(h2));
        json_append(out, json_string(t));
        free(h2); free(t);
    }
    free(h);
    free(off); free(len);
    return out;
}

/* PoP: w27_adapter_permanent_auth_error @ plugins/platforms/matrix/adapter.py:_is_permanent_matrix_auth_error */
json_t *w27_adapter_permanent_auth_error(const json_t *errcode, const json_t *http_status) {
    int permanent = 0;
    if (errcode && errcode->type == JSON_STRING && errcode->str_val) {
        char *t = w27_strip(errcode->str_val);
        for (char *q = t; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        if (!strcmp(t, "m_unknown_token") || !strcmp(t, "m_missing_token") || !strcmp(t, "m_forbidden"))
            permanent = 1;
        free(t);
    }
    if (!permanent && http_status && http_status->type == JSON_NUMBER) {
        double d = http_status->num_val;
        if (d == 401.0 || d == 403.0) permanent = 1;
    }
    return json_bool(permanent);
}

/* PoP: w27_adapter_utf8_len @ plugins/platforms/matrix/adapter.py:_utf8_len */
json_t *w27_adapter_utf8_len(const json_t *text) {
    return json_number((double)strlen(w27_cstr(text)));
}

/* PoP: w27_adapter_max_message_length @ plugins/platforms/matrix/adapter.py:_resolve_max_message_length */
json_t *w27_adapter_max_message_length(const json_t *raw) {
    const double ceiling = 45000.0 / 3.0;   /* MATRIX_MAX_MESSAGE_LENGTH_CEILING */
    const double dflt = ceiling;            /* DEFAULT_MAX_MESSAGE_LENGTH */
    double value;
    if (!raw || raw->type == JSON_NULL) return json_number(dflt);
    if (raw->type == JSON_STRING) {
        char *t = w27_strip(raw->str_val ? raw->str_val : "");
        if (!t[0]) { free(t); return json_number(dflt); }
        int ok = 0;
        long long iv = w27_py_int(t, &ok);
        free(t);
        if (!ok) return json_number(dflt);
        value = (double)iv;
    } else if (raw->type == JSON_NUMBER) {
        value = raw->num_val;
        value = value < 0 ? ceil(value) : floor(value);   /* int() truncates */
    } else if (raw->type == JSON_BOOL) {
        value = raw->bool_val ? 1.0 : 0.0;
    } else {
        return json_number(dflt);   /* int("[]") / int("{...}") raise */
    }
    if (value < 500.0) value = 500.0;
    if (value > ceiling) value = ceiling;
    return json_number(value);
}

/* mimetypes.guess_type's image/audio/video rows, the only prefixes both call
 * sites ask about.  Suffix -> "type/subtype". */
typedef struct { const char *ext; const char *mime; } w27_mime_row;

static const w27_mime_row W27_MIME[] = {
    {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".jpe", "image/jpeg"},
    {".png", "image/png"}, {".gif", "image/gif"}, {".webp", "image/webp"},
    {".bmp", "image/bmp"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
    {".heic", "image/heic"}, {".heif", "image/heif"}, {".avif", "image/avif"},
    {".ico", "image/vnd.microsoft.icon"}, {".tif", "image/tiff"}, {".tiff", "image/tiff"},
    {".ogg", "audio/ogg"}, {".oga", "audio/ogg"}, {".opus", "audio/ogg"},
    {".m4a", "audio/mp4"}, {".mp3", "audio/mpeg"}, {".wav", "audio/x-wav"},
    {".flac", "audio/x-flac"}, {".aac", "audio/aac"}, {".amr", "audio/AMR"},
    {".mp4", "video/mp4"}, {".webm", "video/webm"}, {".mov", "video/quicktime"},
    {".mkv", "video/x-matroska"}, {".avi", "video/x-msvideo"}, {".m4v", "video/mp4"},
};

static const char *w27_guess_type(const char *suffix) {
    for (size_t i = 0; i < sizeof(W27_MIME) / sizeof(W27_MIME[0]); i++)
        if (!strcmp(W27_MIME[i].ext, suffix)) return W27_MIME[i].mime;
    return NULL;
}

static const char *const W27_IMAGE_EXTS[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".svg", ".heic", ".heif", ".avif"};
static const char *const W27_MEDIA_EXTS[] = {
    ".ogg", ".oga", ".opus", ".m4a", ".mp3", ".wav", ".flac", ".aac", ".amr",
    ".mp4", ".webm", ".mov", ".mkv"};

static int w27_in_list(const char *v, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], v)) return 1;
    return 0;
}

/* PoP: w27_adapter_looks_like_transport_filename @ plugins/platforms/matrix/adapter.py:_looks_like_transport_filename */
json_t *w27_adapter_looks_like_transport_filename(const json_t *text, const char *kind, int reject_spaces) {
    char *candidate = w27_strip(w27_truthy(text) ? w27_pystr(text) : w27_strdup2("", 0));
    int result = 0;
    if (!candidate[0] || strchr(candidate, '\n') || candidate[strlen(candidate) - 1] == '/')
        goto done;
    if (reject_spaces) {
        for (const char *q = candidate; *q; q++)
            if (w27_isspace_ascii((unsigned char)*q)) goto done;
    }
    /* Path(candidate).name != candidate */
    if (strchr(candidate, '/') || strchr(candidate, '\\')) goto done;
    {
        const char *dot = strrchr(candidate, '.');
        if (!dot || dot == candidate + (strlen(candidate) - 1) || dot == candidate) goto done;
        char *suffix = w27_strdup2(dot, strlen(dot));
        for (char *q = suffix; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        const char *mime = w27_guess_type(suffix);
        int prefix_ok = 0;
        if (mime) {
            if (!strcmp(kind, "image")) prefix_ok = strncmp(mime, "image/", 6) == 0;
            else prefix_ok = strncmp(mime, "audio/", 6) == 0 || strncmp(mime, "video/", 6) == 0;
        }
        if (!prefix_ok) {
            if (!strcmp(kind, "image"))
                prefix_ok = w27_in_list(suffix, W27_IMAGE_EXTS, sizeof(W27_IMAGE_EXTS) / sizeof(char *));
            else
                prefix_ok = w27_in_list(suffix, W27_MEDIA_EXTS, sizeof(W27_MEDIA_EXTS) / sizeof(char *));
        }
        result = prefix_ok;
        free(suffix);
    }
done:
    free(candidate);
    return json_bool(result);
}

/* PoP: w27_adapter_is_bare_media_filename @ plugins/platforms/matrix/adapter.py:_is_bare_media_filename */
json_t *w27_adapter_is_bare_media_filename(const json_t *msgtype, const json_t *body) {
    const char *mt = w27_cstr(msgtype);
    json_t *r;
    if (!strcmp(mt, "m.image")) {
        r = w27_adapter_looks_like_transport_filename(body, "image", 0);
    } else if (!strcmp(mt, "m.audio") || !strcmp(mt, "m.file") || !strcmp(mt, "m.video")) {
        r = w27_adapter_looks_like_transport_filename(body, "media", 1);
    } else {
        return json_bool(0);
    }
    json_t *out = json_bool(w27_truthy(r));
    json_free(r);
    return out;
}

/* PoP: w27_adapter_event_timestamp_seconds @ plugins/platforms/matrix/adapter.py:_matrix_event_timestamp_seconds */
json_t *w27_adapter_event_timestamp_seconds(const json_t *ts) {
    double value = 0.0;
    if (!ts || ts->type == JSON_NULL) return json_number(0.0);
    if (ts->type == JSON_BOOL) { if (!ts->bool_val) return json_number(0.0); value = 1.0; }
    else if (ts->type == JSON_NUMBER) { if (ts->num_val == 0.0) return json_number(0.0); value = ts->num_val; }
    else if (ts->type == JSON_STRING) {
        int ok = 0;
        if (!ts->str_val || !ts->str_val[0]) return json_number(0.0);
        value = w27_py_float(ts->str_val, &ok);
        if (!ok) return json_number(0.0);
    } else return json_number(0.0);
    return json_number(value > 10000000000.0 ? value / 1000.0 : value);
}

/* PoP: w27_adapter_redact_url_for_log @ plugins/platforms/matrix/adapter.py:_redact_url_for_log */
json_t *w27_adapter_redact_url_for_log(const json_t *url) {
    char *raw = NULL;
    const char *s;
    if (url && url->type == JSON_STRING) s = url->str_val ? url->str_val : "";
    else { raw = w27_pystr(url); s = raw; }
    /* scheme://netloc/path?query#frag -> scheme://netloc/path */
    const char *sep = strstr(s, "://");
    if (sep && sep != s) {
        int scheme_ok = 1;
        for (const char *q = s; q < sep; q++) {
            int alnum = (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                        (*q >= '0' && *q <= '9') || *q == '+' || *q == '.' || *q == '-';
            if (!alnum) { scheme_ok = 0; break; }
        }
        if (scheme_ok) {
            const char *authority = sep + 3;
            const char *end = authority;
            while (*end && *end != '/' && *end != '?' && *end != '#') end++;
            const char *path_end = end;
            while (*path_end && *path_end != '?' && *path_end != '#') path_end++;
            char *out = w27_strdup2(s, (size_t)(path_end - s));
            json_t *r = json_string(out);
            free(out);
            free(raw);
            return r;
        }
    }
    const char *cut = s;
    while (*cut && *cut != '?' && *cut != '#') cut++;
    char *out = w27_strdup2(s, (size_t)(cut - s));
    json_t *r = json_string(out);
    free(out);
    free(raw);
    return r;
}

/* ------------------------------------------------------------------ */
/* p2: config / env normalisation                                      */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_normalize_e2ee_mode @ plugins/platforms/matrix/adapter.py:_normalize_e2ee_mode */
json_t *w27_adapter_normalize_e2ee_mode(const json_t *value) {
    char *raw = w27_truthy(value) ? w27_pystr(value) : w27_strdup2("", 0);
    char *t = w27_strip(raw);
    for (char *q = t; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    const char *mode = "off";
    if (!strcmp(t, "required") || !strcmp(t, "require") || !strcmp(t, "true") ||
        !strcmp(t, "1") || !strcmp(t, "yes") || !strcmp(t, "on")) mode = "required";
    else if (!strcmp(t, "optional") || !strcmp(t, "prefer") || !strcmp(t, "preferred")) mode = "optional";
    free(raw); free(t);
    return json_string(mode);
}

/* PoP: w27_adapter_resolve_e2ee_mode @ plugins/platforms/matrix/adapter.py:_resolve_e2ee_mode */
json_t *w27_adapter_resolve_e2ee_mode(const json_t *explicit_, const json_t *legacy_enabled) {
    if (w27_truthy(explicit_)) return w27_adapter_normalize_e2ee_mode(explicit_);
    return json_string(w27_truthy(legacy_enabled) ? "required" : "off");
}

/* PoP: w27_adapter_env_truthy @ plugins/platforms/matrix/adapter.py:_env_truthy */
json_t *w27_adapter_env_truthy(const json_t *value) {
    char *s = w27_pystr(value);
    for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    int t = !strcmp(s, "true") || !strcmp(s, "1") || !strcmp(s, "yes");
    free(s);
    return json_bool(t);
}

/* PoP: w27_adapter_env_number @ plugins/platforms/matrix/adapter.py:_env_number */
json_t *w27_adapter_env_number(const json_t *value, const json_t *dflt, int want_float) {
    if (value && value->type != JSON_NULL) {
        char *s = w27_pystr(value);
        int ok = 0;
        if (want_float) {
            double v = w27_py_float(s, &ok);
            free(s);
            if (ok) return json_number(v);
        } else {
            long long v = w27_py_int(s, &ok);
            free(s);
            if (ok) return json_number((double)v);
        }
    }
    return json_copy(dflt ? dflt : json_null());
}

/* PoP: w27_adapter_csv_set @ plugins/platforms/matrix/adapter.py:_csv_set */
/* Python builds a set: duplicates collapse before the port's sort. Without this
 * "a,a,b" produced ['a','a','b'] where the live module answers ['a','b']. */
static int w27_already_have(char **items, size_t n, const char *v) {
    for (size_t i = 0; i < n; i++)
        if (!strcmp(items[i], v)) return 1;
    return 0;
}

json_t *w27_adapter_csv_set(const json_t *raw) {
    char **items = NULL;
    size_t n = 0, cap = 0;
    if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            char *v = w27_pystr(json_get(raw, i));
            char *t = w27_strip(v);
            free(v);
            if (!t[0]) { free(t); continue; }
            if (w27_already_have(items, n, t)) { free(t); continue; }
            if (n == cap) { cap = cap ? cap * 2 : 8; items = (char **)realloc(items, cap * sizeof(char *)); }
            items[n++] = t;
        }
    } else {
        char *s = w27_pystr(raw);
        char *p = s;
        for (;;) {
            char *comma = strchr(p, ',');
            size_t len = comma ? (size_t)(comma - p) : strlen(p);
            char *part = w27_strdup2(p, len);
            char *t = w27_strip(part);
            free(part);
            if (t[0] && !w27_already_have(items, n, t)) {
                if (n == cap) { cap = cap ? cap * 2 : 8; items = (char **)realloc(items, cap * sizeof(char *)); }
                items[n++] = t;
            } else free(t);
            if (!comma) break;
            p = comma + 1;
        }
        free(s);
    }
    json_t *out = w27_sorted_array(items, n);
    for (size_t i = 0; i < n; i++) free(items[i]);
    free(items);
    return out;
}

/* PoP: w27_adapter_thread_root @ plugins/platforms/matrix/adapter.py:_thread_root */
json_t *w27_adapter_thread_root(const json_t *relates_to) {
    if (!relates_to || relates_to->type != JSON_OBJECT) return json_null();
    const char *rel = w27_cstr(json_obj_get(relates_to, "rel_type"));
    if (strcmp(rel, "m.thread")) return json_null();
    json_t *eid = json_obj_get(relates_to, "event_id");
    if (!eid) return json_null();
    return json_copy(eid);
}

/* PoP: w27_adapter_extra_truthy @ plugins/platforms/matrix/adapter.py:MatrixAdapter._extra_truthy */
json_t *w27_adapter_extra_truthy(const json_t *configured) {
    if (configured && configured->type == JSON_BOOL) return json_bool(configured->bool_val);
    return w27_adapter_env_truthy(configured);
}

/* PoP: w27_adapter_configured_bool @ plugins/platforms/matrix/adapter.py:MatrixAdapter._configured_bool */
json_t *w27_adapter_configured_bool(const json_t *configured) {
    if (!configured || configured->type == JSON_NULL) return json_null();
    if (configured->type == JSON_BOOL) return json_bool(configured->bool_val);
    if (configured->type == JSON_STRING) {
        char *s = w27_pystr(configured);
        for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        int off = !strcmp(s, "false") || !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "off");
        free(s);
        return json_bool(!off);
    }
    return json_bool(w27_truthy(configured));
}

/* PoP: w27_adapter_parse_require_mention @ plugins/platforms/matrix/adapter.py:MatrixAdapter._parse_require_mention */
json_t *w27_adapter_parse_require_mention(const json_t *configured) {
    if (!configured || configured->type == JSON_NULL) return json_bool(1);
    if (configured->type == JSON_BOOL) return json_bool(configured->bool_val);
    char *s = w27_pystr(configured);
    for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    int off = !strcmp(s, "false") || !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "off");
    free(s);
    return json_bool(!off);
}

/* PoP: w27_adapter_parse_thread_require_mention @ plugins/platforms/matrix/adapter.py:MatrixAdapter._parse_thread_require_mention */
json_t *w27_adapter_parse_thread_require_mention(const json_t *configured) {
    /* The value arrives from _extra_or_secret(..., False), which is `value or
     * default`: a FALSY configured value (None, "", 0, [], {}) never reaches the
     * string ladder, it becomes False. Only a truthy non-bool goes through
     * str(...).lower() not in {"false","0","no","off"}. Treating "" as present
     * answered True where Python answers False. */
    if (!configured || configured->type == JSON_NULL) return json_bool(0);
    if (configured->type == JSON_BOOL) return json_bool(configured->bool_val);
    /* Only an EMPTY STRING falls back to the False default; an empty list or
     * dict is a value, so it reaches the ladder and answers True (str([]) is
     * "[]", which is not in the off-set). Held-out cases pinned both arms. */
    if (configured->type == JSON_STRING && !configured->str_val[0]) return json_bool(0);
    return w27_adapter_parse_require_mention(configured);
}

/* PoP: w27_adapter_extract_server_ed25519 @ plugins/platforms/matrix/adapter.py:MatrixAdapter._extract_server_ed25519 */
json_t *w27_adapter_extract_server_ed25519(const json_t *keys) {
    if (!keys || keys->type != JSON_OBJECT) return json_null();
    for (size_t i = 0; i < json_object_size(keys); i++) {
        const char *kid = json_object_get_key_at(keys, i);
        if (!kid || strncmp(kid, "ed25519:", 8) != 0) continue;
        char *val = w27_pystr(json_object_get_at(keys, i));
        json_t *out = json_string(val);
        free(val);
        return out;
    }
    return json_null();
}

/* PoP: w27_adapter_is_connected @ plugins/platforms/matrix/adapter.py:_is_connected */
json_t *w27_adapter_is_connected(const json_t *homeserver, const json_t *token) {
    char *h = w27_strip(w27_truthy(homeserver) ? w27_pystr(homeserver) : w27_strdup2("", 0));
    char *t = w27_strip(w27_truthy(token) ? w27_pystr(token) : w27_strdup2("", 0));
    int ok = h[0] && t[0];
    free(h); free(t);
    return json_bool(ok);
}

/* PoP: w27_adapter_session_scope @ plugins/platforms/matrix/adapter.py:MatrixAdapter.__init__ */
json_t *w27_adapter_session_scope(const json_t *raw) {
    char *s = w27_strip(w27_pystr(raw));
    for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    const char *scope = "auto";
    if (!strcmp(s, "room")) scope = "room";
    else if (!strcmp(s, "thread")) scope = "thread";
    free(s);
    return json_string(scope);
}

/* PoP: w27_adapter_reactions_enabled @ plugins/platforms/matrix/adapter.py:MatrixAdapter.__init__ */
json_t *w27_adapter_reactions_enabled(const json_t *raw) {
    char *s = w27_pystr(raw);
    for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    int enabled = strcmp(s, "false") && strcmp(s, "0") && strcmp(s, "no");
    free(s);
    return json_bool(enabled);
}

/* ------------------------------------------------------------------ */
/* p3: outbound payload shaping                                        */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_exec_approval_tier @ plugins/platforms/matrix/adapter.py:MatrixAdapter._send_exec_approval_prompt */
json_t *w27_adapter_exec_approval_tier(const json_t *choices, const json_t *smart_denied) {
    int has_always = 0, has_session = 0;
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(choices); i++) {
            const char *c = w27_cstr(json_get(choices, i));
            if (!strcmp(c, "always")) has_always = 1;
            if (!strcmp(c, "session")) has_session = 1;
        }
    }
    if (!w27_truthy(smart_denied)) {
        if (has_always) return json_string("always");
        if (has_session) return json_string("session");
    }
    return json_string("once");
}

/* PoP: w27_adapter_approval_reaction @ plugins/platforms/matrix/adapter.py:MatrixAdapter._EA_REACTIONS */
json_t *w27_adapter_approval_reaction(const json_t *tier) {
    const char *t = w27_cstr(tier);
    if (!strcmp(t, "once")) return json_string("\xE2\x9C\x85");
    if (!strcmp(t, "session")) return json_string("\xF0\x9F\x8C\x80");
    if (!strcmp(t, "always")) return json_string("\xE2\x99\xBE\xEF\xB8\x8F");
    if (!strcmp(t, "deny")) return json_string("\xE2\x9D\x8C");
    return json_null();
}

/* PoP: w27_adapter_approval_action @ plugins/platforms/matrix/adapter.py:MatrixAdapter.__init__ */
json_t *w27_adapter_approval_action(const json_t *emoji) {
    const char *e = w27_cstr(emoji);
    if (!strcmp(e, "\xE2\x9C\x85")) return json_string("once");
    if (!strcmp(e, "\xF0\x9F\x8C\x80")) return json_string("session");
    if (!strcmp(e, "\xE2\x99\xBE\xEF\xB8\x8F") || !strcmp(e, "\xE2\x99\xBE"))
        return json_string("always");
    if (!strcmp(e, "\xE2\x9D\x8C") || !strcmp(e, "\xE2\x9D\x8E")) return json_string("deny");
    return json_null();
}

/* PoP: w27_adapter_flat_picker_choices @ plugins/platforms/matrix/adapter.py:MatrixAdapter.send_model_picker */
json_t *w27_adapter_flat_picker_choices(const json_t *providers) {
    json_t *out = json_array();
    if (!providers || providers->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(providers) && json_len(out) < 10; i++) {
        const json_t *p = json_get(providers, i);
        if (!p || p->type != JSON_OBJECT) continue;
        const json_t *models = json_obj_get(p, "models");
        if (!models || models->type != JSON_ARRAY) continue;
        const json_t *slug = json_obj_get(p, "slug");
        const json_t *name = json_obj_get(p, "name");
        for (size_t m = 0; m < json_len(models) && json_len(out) < 10; m++) {
            json_t *row = json_array();
            char *mid = w27_pystr(json_get(models, m));
            char *s = w27_truthy(slug) ? w27_pystr(slug) : w27_strdup2("", 0);
            char *nm;
            if (w27_truthy(name)) nm = w27_pystr(name);
            else if (w27_truthy(slug)) nm = w27_pystr(slug);
            else nm = w27_strdup2("", 0);
            json_append(row, json_string(mid));
            json_append(row, json_string(s));
            json_append(row, json_string(nm));
            json_append(out, row);
            free(mid); free(s); free(nm);
        }
    }
    return out;
}

/* Keycap 1-9 + 10; choice pickers add A/B. */
static const char *const W27_MODEL_REACTIONS[10] = {
    "1\xEF\xB8\x8F\xE2\x83\xA3", "2\xEF\xB8\x8F\xE2\x83\xA3", "3\xEF\xB8\x8F\xE2\x83\xA3",
    "4\xEF\xB8\x8F\xE2\x83\xA3", "5\xEF\xB8\x8F\xE2\x83\xA3", "6\xEF\xB8\x8F\xE2\x83\xA3",
    "7\xEF\xB8\x8F\xE2\x83\xA3", "8\xEF\xB8\x8F\xE2\x83\xA3", "9\xEF\xB8\x8F\xE2\x83\xA3",
    "\xF0\x9F\x94\x9F"};
static const char *const W27_CHOICE_REACTIONS[12] = {
    "1\xEF\xB8\x8F\xE2\x83\xA3", "2\xEF\xB8\x8F\xE2\x83\xA3", "3\xEF\xB8\x8F\xE2\x83\xA3",
    "4\xEF\xB8\x8F\xE2\x83\xA3", "5\xEF\xB8\x8F\xE2\x83\xA3", "6\xEF\xB8\x8F\xE2\x83\xA3",
    "7\xEF\xB8\x8F\xE2\x83\xA3", "8\xEF\xB8\x8F\xE2\x83\xA3", "9\xEF\xB8\x8F\xE2\x83\xA3",
    "\xF0\x9F\x94\x9F", "\xF0\x9F\x85\xB0\xEF\xB8\x8F", "\xF0\x9F\x85\xB1\xEF\xB8\x8F"};

/* PoP: w27_adapter_choice_picker_emoji @ plugins/platforms/matrix/adapter.py:MatrixAdapter.send_choice_picker */
json_t *w27_adapter_choice_picker_emoji(const json_t *choices) {
    json_t *out = json_object();
    if (!choices || choices->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(choices) && i < 12; i++) {
        const json_t *c = json_get(choices, i);
        const json_t *v = (c && c->type == JSON_OBJECT) ? json_obj_get(c, "value") : NULL;
        char *val = w27_truthy(v) ? w27_pystr(v) : w27_strdup2("", 0);
        json_set(out, W27_CHOICE_REACTIONS[i], json_string(val));
        free(val);
    }
    return out;
}

/* PoP: w27_adapter_apply_relation_metadata @ plugins/platforms/matrix/adapter.py:MatrixAdapter._apply_relation_metadata */
json_t *w27_adapter_apply_relation_metadata(const json_t *content, const json_t *reply_to, const json_t *thread_id) {
    json_t *out = content && content->type == JSON_OBJECT ? json_copy(content) : json_object();
    char *tid = w27_truthy(thread_id) ? w27_pystr(thread_id) : w27_strdup2("", 0);
    char *rt = NULL;
    if (w27_truthy(reply_to)) {
        rt = w27_pystr(reply_to);
        json_t *rel = json_object();
        json_t *inreply = json_object();
        json_set(inreply, "event_id", json_string(rt));
        json_set(rel, "m.in_reply_to", inreply);
        json_set(out, "m.relates_to", rel);
    }
    if (tid[0]) {
        json_t *rel = json_obj_get(out, "m.relates_to");
        json_t *work;
        if (rel && rel->type == JSON_OBJECT) work = json_copy(rel);
        else work = json_object();
        json_set(work, "rel_type", json_string("m.thread"));
        json_set(work, "event_id", json_string(tid));
        json_set(work, "is_falling_back", json_bool(1));
        if (!json_obj_get(work, "m.in_reply_to")) {
            json_t *inreply = json_object();
            json_set(inreply, "event_id", json_string(rt ? rt : tid));
            json_set(work, "m.in_reply_to", inreply);
        }
        json_set(out, "m.relates_to", work);
    }
    free(tid); free(rt);
    return out;
}

/* Protect fenced code, inline code and markdown links with sentinel tokens.
 * The sentinel is \x01 (never present in Matrix text) rather than Python's
 * \x00, which C strings cannot carry. */
static void w27_sb_token(w27_sb *b, size_t idx) {
    char tmp[48];
    snprintf(tmp, sizeof tmp, "\x01MENTION_PROTECTED%zu\x01", idx);
    w27_sb_put(b, tmp);
}

static size_t w27_token_at(const char *s, size_t pos) {
    /* returns token index when a token starts at pos, else (size_t)-1 */
    static const char prefix[] = "\x01MENTION_PROTECTED";
    size_t pl = sizeof(prefix) - 1;
    if (strncmp(s + pos, prefix, pl) != 0) return (size_t)-1;
    size_t i = pos + pl, val = 0, digits = 0;
    while (s[i] >= '0' && s[i] <= '9') { val = val * 10 + (size_t)(s[i] - '0'); i++; digits++; }
    if (!digits || s[i] != '\x01') return (size_t)-1;
    return val;
}

/* One protection pass: replace every non-overlapping match of the pattern
 * kind with a token, appending the matched text to *ph. */
static char *w27_protect_pass(const char *text, int kind, char ***ph, size_t *nph, size_t *capph) {
    w27_sb out; w27_sb_init(&out);
    size_t i = 0, n = strlen(text);
    while (i < n) {
        size_t start = i, end = 0;
        int matched = 0;
        if (kind == 0) {                     /* ```[\s\S]*?``` */
            if (i + 3 <= n && !strncmp(text + i, "```", 3)) {
                const char *close = strstr(text + i + 3, "```");
                if (close) { end = (size_t)(close - text) + 3; matched = 1; }
            }
        } else if (kind == 1) {              /* `[^`\n]+` */
            if (text[i] == '`') {
                size_t j = i + 1;
                while (j < n && text[j] != '`' && text[j] != '\n') j++;
                if (j < n && text[j] == '`' && j > i + 1) { end = j + 1; matched = 1; }
            }
        } else {                             /* \[[^\]]+\]\([^)]+\) */
            if (text[i] == '[') {
                size_t j = i + 1;
                while (j < n && text[j] != ']' && text[j] != '\n') j++;
                if (j < n && text[j] == ']' && j > i + 1 && j + 1 < n && text[j + 1] == '(') {
                    size_t k = j + 2;
                    while (k < n && text[k] != ')') k++;
                    if (k < n && text[k] == ')' && k > j + 2) { end = k + 1; matched = 1; }
                }
            }
        }
        if (!matched) { w27_sb_putc(&out, text[i]); i++; continue; }
        if (*nph == *capph) {
            *capph = *capph ? *capph * 2 : 8;
            *ph = (char **)realloc(*ph, *capph * sizeof(char *));
        }
        (*ph)[*nph] = w27_strdup2(text + start, end - start);
        w27_sb_token(&out, *nph);
        (*nph)++;
        i = end;
    }
    return w27_sb_take(&out);
}

/* PoP: w27_adapter_protect_mention_regions @ plugins/platforms/matrix/adapter.py:MatrixAdapter._protect_outbound_mention_regions */
json_t *w27_adapter_protect_mention_regions(const json_t *text) {
    char **ph = NULL;
    size_t nph = 0, capph = 0;
    char *cur = w27_strdup2(w27_cstr(text), strlen(w27_cstr(text)));
    for (int kind = 0; kind < 3; kind++) {
        char *next = w27_protect_pass(cur, kind, &ph, &nph, &capph);
        free(cur);
        cur = next;
    }
    json_t *out = json_object();
    json_set(out, "protected", json_string(cur));
    json_t *arr = json_array();
    for (size_t i = 0; i < nph; i++) json_append(arr, json_string(ph[i]));
    json_set(out, "placeholders", arr);
    free(cur);
    for (size_t i = 0; i < nph; i++) free(ph[i]);
    free(ph);
    return out;
}

static int w27_is_word(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* Case-insensitive literal match (re.IGNORECASE over ASCII). */
static int w27_ci_match_word(const char *p, const char *lp) {
    for (size_t k = 0; lp[k]; k++) {
        unsigned char a = (unsigned char)p[k], b = (unsigned char)lp[k];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (!a || a != b) return 0;
    }
    return 1;
}

/* One outbound mention starting at pos: @localpart:server[:port].
 * Returns end offset or 0. */
static size_t w27_mention_end(const char *s, size_t pos) {
    size_t i = pos;
    if (s[i] != '@') return 0;
    i++;
    size_t lp = i;
    while (s[i] && ((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') ||
                    (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '.' || s[i] == '_' ||
                    s[i] == '=' || s[i] == '/' || s[i] == '-')) i++;
    if (i == lp || s[i] != ':') return 0;
    i++;
    size_t sp = i;
    while (s[i] && ((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') ||
                    (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '.' || s[i] == '-')) i++;
    if (i == sp) return 0;
    if (s[i] == ':' && s[i + 1] >= '0' && s[i + 1] <= '9') {
        size_t d = i + 1;
        while (s[d] >= '0' && s[d] <= '9') d++;
        i = d;
    }
    return i;
}

/* PoP: w27_adapter_extract_outbound_mentions @ plugins/platforms/matrix/adapter.py:MatrixAdapter._extract_outbound_mentions */
json_t *w27_adapter_extract_outbound_mentions(const json_t *text) {
    json_t *protected_ = w27_adapter_protect_mention_regions(text);
    const char *s = w27_cstr(json_obj_get(protected_, "protected"));
    json_t *out = json_array();
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        if (s[i] != '@') continue;
        if (i > 0 && (w27_is_word((unsigned char)s[i - 1]) || s[i - 1] == '/')) continue;
        size_t end = w27_mention_end(s, i);
        if (!end) continue;
        char *m = w27_strdup2(s + i, end - i);
        int seen = 0;
        for (size_t k = 0; k < json_len(out); k++)
            if (!strcmp(w27_cstr(json_get(out, k)), m)) { seen = 1; break; }
        if (!seen) json_append(out, json_string(m));
        free(m);
        i = end - 1;
    }
    json_free(protected_);
    return out;
}

/* PoP: w27_adapter_has_outbound_room_mention @ plugins/platforms/matrix/adapter.py:MatrixAdapter._has_outbound_room_mention */
json_t *w27_adapter_has_outbound_room_mention(const json_t *text) {
    json_t *protected_ = w27_adapter_protect_mention_regions(text);
    const char *s = w27_cstr(json_obj_get(protected_, "protected"));
    int found = 0;
    for (size_t i = 0; s[i] && !found; i++) {
        if (strncmp(s + i, "@room", 5) != 0) continue;
        if (i > 0 && (w27_is_word((unsigned char)s[i - 1]) || s[i - 1] == '/')) continue;
        char c = s[i + 5];
        if (w27_is_word((unsigned char)c) || c == ':' || c == '.' || c == '-') continue;
        found = 1;
    }
    json_free(protected_);
    return json_bool(found);
}

/* PoP: w27_adapter_inject_outbound_mention_links @ plugins/platforms/matrix/adapter.py:MatrixAdapter._inject_outbound_mention_links */
json_t *w27_adapter_inject_outbound_mention_links(const json_t *text) {
    if (!text || text->type == JSON_NULL) return json_null();
    if (!w27_truthy(text)) return json_string("");
    json_t *protected_ = w27_adapter_protect_mention_regions(text);
    const char *s = w27_cstr(json_obj_get(protected_, "protected"));
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s);
    for (size_t i = 0; i < n;) {
        if (s[i] == '@' && !(i > 0 && (w27_is_word((unsigned char)s[i - 1]) || s[i - 1] == '/'))) {
            size_t end = w27_mention_end(s, i);
            if (end) {
                char *m = w27_strdup2(s + i, end - i);
                w27_sb_putc(&out, '[');
                w27_sb_put(&out, m);
                w27_sb_put(&out, "](https://matrix.to/#/");
                w27_sb_put(&out, m);
                w27_sb_putc(&out, ')');
                free(m);
                i = end;
                continue;
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    char *linked = w27_sb_take(&out);
    /* restore placeholders */
    json_t *ph = json_obj_get(protected_, "placeholders");
    for (size_t idx = 0; ph && idx < json_len(ph); idx++) {
        char token[48];
        snprintf(token, sizeof token, "\x01MENTION_PROTECTED%zu\x01", idx);
        const char *orig = w27_cstr(json_get(ph, idx));
        w27_sb rebuilt; w27_sb_init(&rebuilt);
        size_t tl = strlen(token), pos = 0;
        const char *hit;
        while ((hit = strstr(linked + pos, token)) != NULL) {
            w27_sb_putn(&rebuilt, linked + pos, (size_t)(hit - (linked + pos)));
            w27_sb_put(&rebuilt, orig);
            pos = (size_t)(hit - linked) + tl;
        }
        w27_sb_put(&rebuilt, linked + pos);
        free(linked);
        linked = w27_sb_take(&rebuilt);
    }
    json_t *res = json_string(linked);
    free(linked);
    json_free(protected_);
    return res;
}

/* PoP: w27_adapter_user_localpart @ plugins/platforms/matrix/adapter.py:MatrixAdapter._user_localpart */
static char *w27_localpart_impl(const char *user_id) {
    if (!user_id || !user_id[0] || !strchr(user_id, ':')) return w27_strdup2("", 0);
    const char *colon = strchr(user_id, ':');
    const char *start = user_id;
    while (*start == '@') start++;
    if (start > colon) start = colon;
    return w27_strdup2(start, (size_t)(colon - start));
}

json_t *w27_adapter_user_localpart(const json_t *user_id) {
    char *lp = w27_localpart_impl(w27_cstr(user_id));
    json_t *out = json_string(lp);
    free(lp);
    return out;
}

/* PoP: w27_adapter_is_bot_mentioned @ plugins/platforms/matrix/adapter.py:MatrixAdapter._is_bot_mentioned */
json_t *w27_adapter_is_bot_mentioned(const json_t *user_id, const json_t *body,
                                     const json_t *formatted_body, const json_t *mention_user_ids) {
    const char *uid = w27_cstr(user_id);
    const char *b = w27_cstr(body);
    const char *fb = w27_cstr(formatted_body);
    if (mention_user_ids && mention_user_ids->type == JSON_ARRAY && json_len(mention_user_ids) > 0 && uid[0]) {
        for (size_t i = 0; i < json_len(mention_user_ids); i++)
            if (!strcmp(w27_cstr(json_get(mention_user_ids, i)), uid)) return json_bool(1);
    } else if (mention_user_ids && mention_user_ids->type == JSON_STRING && uid[0]) {
        if (strstr(w27_cstr(mention_user_ids), uid)) return json_bool(1);
    }
    if (!b[0] && !fb[0]) return json_bool(0);
    if (uid[0] && strstr(b, uid)) return json_bool(1);
    char *lp = w27_localpart_impl(uid);
    if (lp[0]) {
        size_t ll = strlen(lp);
        for (const char *p = b; *p; p++) {
            /* \b before, case-insensitive literal, \b after (re.IGNORECASE) */
            if (p != b && w27_is_word((unsigned char)p[-1])) continue;
            if (w27_ci_match_word(p, lp) && !w27_is_word((unsigned char)p[ll])) {
                free(lp);
                return json_bool(1);
            }
        }
    }
    free(lp);
    if (fb[0] && uid[0]) {
        char *needle = (char *)malloc(strlen(uid) + 16);
        if (needle) {
            sprintf(needle, "matrix.to/#/%s", uid);
            int hit = strstr(fb, needle) != NULL;
            free(needle);
            if (hit) return json_bool(1);
        }
    }
    return json_bool(0);
}

/* PoP: w27_adapter_strip_mention @ plugins/platforms/matrix/adapter.py:MatrixAdapter._strip_mention */
json_t *w27_adapter_strip_mention(const json_t *user_id, const json_t *body) {
    const char *uid = w27_cstr(user_id);
    const char *b = w27_cstr(body);
    if (!b[0]) return json_string("");
    w27_sb work; w27_sb_init(&work);
    /* body.replace(user_id, "") */
    size_t ul = strlen(uid), pos = 0;
    if (ul) {
        const char *hit;
        while ((hit = strstr(b + pos, uid)) != NULL) {
            w27_sb_putn(&work, b + pos, (size_t)(hit - (b + pos)));
            pos = (size_t)(hit - b) + ul;
        }
    }
    w27_sb_put(&work, b + pos);
    char *stage = w27_sb_take(&work);

    /* re.sub(r'(?<![\w])@' + localpart + r'\b', '', stage, IGNORECASE) */
    char *lp = w27_localpart_impl(uid);
    if (lp[0]) {
        w27_sb res; w27_sb_init(&res);
        size_t n = strlen(stage), ll = strlen(lp);
        for (size_t i = 0; i < n;) {
            if (stage[i] == '@' && (i == 0 || !w27_is_word((unsigned char)stage[i - 1]))) {
                size_t k = 0;
                while (k < ll && stage[i + 1 + k]) {
                    char a = stage[i + 1 + k], c = lp[k];
                    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                    if (a != c) break;
                    k++;
                }
                if (k == ll && !w27_is_word((unsigned char)stage[i + 1 + ll])) {
                    i += 1 + ll;
                    continue;
                }
            }
            w27_sb_putc(&res, stage[i]);
            i++;
        }
        free(stage);
        stage = w27_sb_take(&res);
    }
    free(lp);

    /* re.sub(r'[ \t]{2,}', ' ', ...) */
    {
        w27_sb res; w27_sb_init(&res);
        for (size_t i = 0; stage[i];) {
            if (stage[i] == ' ' || stage[i] == '\t') {
                size_t j = i;
                while (stage[j] == ' ' || stage[j] == '\t') j++;
                if (j - i >= 2) w27_sb_putc(&res, ' ');
                else w27_sb_putn(&res, stage + i, j - i);
                i = j;
            } else { w27_sb_putc(&res, stage[i]); i++; }
        }
        free(stage);
        stage = w27_sb_take(&res);
    }
    /* re.sub(r'\s+([,.;:!?])', r'\1', ...) */
    {
        w27_sb res; w27_sb_init(&res);
        for (size_t i = 0; stage[i];) {
            if (w27_isspace_ascii((unsigned char)stage[i])) {
                size_t j = i;
                while (w27_isspace_ascii((unsigned char)stage[j])) j++;
                char next = stage[j];
                if (next && strchr(",.;:!?", next)) { i = j; continue; }
                w27_sb_putn(&res, stage + i, j - i);
                i = j;
            } else { w27_sb_putc(&res, stage[i]); i++; }
        }
        free(stage);
        stage = w27_sb_take(&res);
    }
    char *trimmed = w27_strip(stage);
    json_t *out = json_string(trimmed);
    free(stage); free(trimmed);
    return out;
}

/* PoP: w27_adapter_mxc_to_http @ plugins/platforms/matrix/adapter.py:MatrixAdapter._mxc_to_http */
json_t *w27_adapter_mxc_to_http(const json_t *homeserver, const json_t *mxc_url) {
    const char *mxc = w27_cstr(mxc_url);
    if (strncmp(mxc, "mxc://", 6) != 0) return json_string(mxc);
    w27_sb b; w27_sb_init(&b);
    w27_sb_put(&b, w27_cstr(homeserver));
    w27_sb_put(&b, "/_matrix/client/v1/media/download/");
    w27_sb_put(&b, mxc + 6);
    char *s = w27_sb_take(&b);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: w27_adapter_sanitize_link_url @ plugins/platforms/matrix/adapter.py:MatrixAdapter._sanitize_link_url */
json_t *w27_adapter_sanitize_link_url(const json_t *url) {
    char *stripped = w27_strip(w27_cstr(url));
    char *colon = strchr(stripped, ':');
    if (colon) {
        char *scheme = w27_strdup2(stripped, (size_t)(colon - stripped));
        char *sf = w27_strip(scheme);
        for (char *q = sf; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        int bad = !strcmp(sf, "javascript") || !strcmp(sf, "data") || !strcmp(sf, "vbscript");
        free(scheme); free(sf);
        if (bad) { free(stripped); return json_string(""); }
    }
    w27_sb b; w27_sb_init(&b);
    for (const char *q = stripped; *q; q++) {
        if (*q == '"') w27_sb_put(&b, "&quot;");
        else w27_sb_putc(&b, *q);
    }
    char *s = w27_sb_take(&b);
    json_t *out = json_string(s);
    free(s); free(stripped);
    return out;
}

/* PoP: w27_adapter_format_message @ plugins/platforms/matrix/adapter.py:MatrixAdapter.format_message */
json_t *w27_adapter_format_message(const json_t *content) {
    const char *s = w27_cstr(content);
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (s[i] == '!' && i + 1 < n && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j < n && s[j] != ']') j++;
            if (j < n && s[j] == ']' && j + 1 < n && s[j + 1] == '(') {
                size_t k = j + 2;
                while (k < n && s[k] != ')') k++;
                if (k < n && k > j + 2) {
                    w27_sb_putn(&out, s + j + 2, k - (j + 2));
                    i = k + 1;
                    continue;
                }
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    char *r = w27_sb_take(&out);
    json_t *res = json_string(r);
    free(r);
    return res;
}

/* ------------------------------------------------------------------ */
/* p4: inbound classification and policy predicates                    */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_is_self_sender @ plugins/platforms/matrix/adapter.py:MatrixAdapter._is_self_sender */
json_t *w27_adapter_is_self_sender(const json_t *user_id, const json_t *sender) {
    char *own = w27_strip(w27_cstr(user_id));
    for (char *q = own; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    char *snd = w27_strip(w27_cstr(sender));
    for (char *q = snd; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    int self = !own[0] || !strcmp(snd, own);
    free(own); free(snd);
    return json_bool(self);
}

/* PoP: w27_adapter_is_system_or_bridge_sender @ plugins/platforms/matrix/adapter.py:MatrixAdapter._is_system_or_bridge_sender */
json_t *w27_adapter_is_system_or_bridge_sender(const json_t *sender) {
    char *t = w27_strip(w27_cstr(sender));
    char *p = t;
    while (*p == '@') p++;
    char *colon = strchr(p, ':');
    if (colon) *colon = '\0';
    int bridge = !p[0] || p[0] == '_';
    free(t);
    return json_bool(bridge);
}

/* PoP: w27_adapter_classify_inbound_media @ plugins/platforms/matrix/adapter.py:MatrixAdapter._classify_inbound_media */
json_t *w27_adapter_classify_inbound_media(const json_t *msgtype, const json_t *mimetype, const json_t *content) {
    const char *mt = w27_cstr(msgtype);
    const char *mime = w27_cstr(mimetype);
    json_t *out = json_array();
    if (!strcmp(mt, "m.image")) {
        json_append(out, json_string("photo"));
        json_append(out, json_string(mime[0] ? mime : "image/png"));
        json_append(out, json_bool(0));
        return out;
    }
    if (!strcmp(mt, "m.audio")) {
        const json_t *marker = (content && content->type == JSON_OBJECT)
                                   ? json_obj_get(content, "org.matrix.msc3245.voice") : NULL;
        int voice = marker != NULL && marker->type != JSON_NULL;
        json_append(out, json_string(voice ? "voice" : "audio"));
        json_append(out, json_string(mime[0] ? mime : "audio/ogg"));
        json_append(out, json_bool(voice));
        return out;
    }
    if (!strcmp(mt, "m.video")) {
        json_append(out, json_string("video"));
        json_append(out, json_string(mime[0] ? mime : "video/mp4"));
        json_append(out, json_bool(0));
        return out;
    }
    json_append(out, json_string("document"));
    json_append(out, json_string(mime[0] ? mime : "application/octet-stream"));
    json_append(out, json_bool(0));
    return out;
}

/* PoP: w27_adapter_content_mentions_bot @ plugins/platforms/matrix/adapter.py:MatrixAdapter._content_mentions_bot */
json_t *w27_adapter_content_mentions_bot(const json_t *user_id, const json_t *body, const json_t *content) {
    const json_t *mentions = (content && content->type == JSON_OBJECT) ? json_obj_get(content, "m.mentions") : NULL;
    const json_t *user_ids = NULL;
    if (mentions && mentions->type == JSON_OBJECT) user_ids = json_obj_get(mentions, "user_ids");
    const json_t *fb = (content && content->type == JSON_OBJECT) ? json_obj_get(content, "formatted_body") : NULL;
    return w27_adapter_is_bot_mentioned(user_id, body, fb, user_ids);
}

/* PoP: w27_adapter_voice_may_park @ plugins/platforms/matrix/adapter.py:MatrixAdapter._voice_may_park */
json_t *w27_adapter_voice_may_park(const json_t *req) {
    const char *room_id = w27_cstr(json_obj_get(req, "room_id"));
    const char *body = w27_cstr(json_obj_get(req, "body"));
    const json_t *content = json_obj_get(req, "content");
    const json_t *relates_to = json_obj_get(req, "relates_to");
    int mention_claimed = w27_truthy(json_obj_get(req, "mention_claimed"));
    int require_mention = w27_truthy(json_obj_get(req, "require_mention"));
    int is_voice = 0;
    if (content && content->type == JSON_OBJECT &&
        !strcmp(w27_cstr(json_obj_get(content, "msgtype")), "m.audio")) {
        const json_t *marker = json_obj_get(content, "org.matrix.msc3245.voice");
        is_voice = marker != NULL && marker->type != JSON_NULL;
    }
    if (mention_claimed || !require_mention || !is_voice) return json_bool(0);
    const json_t *free_rooms = json_obj_get(req, "free_rooms");
    const json_t *allowed_rooms = json_obj_get(req, "allowed_rooms");
    int in_free = 0, in_allowed = 1, allowed_nonempty = 0;
    if (free_rooms && free_rooms->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(free_rooms); i++)
            if (!strcmp(w27_cstr(json_get(free_rooms, i)), room_id)) in_free = 1;
    if (allowed_rooms && allowed_rooms->type == JSON_ARRAY) {
        allowed_nonempty = json_len(allowed_rooms) > 0;
        in_allowed = 0;
        for (size_t i = 0; i < json_len(allowed_rooms); i++)
            if (!strcmp(w27_cstr(json_get(allowed_rooms, i)), room_id)) in_allowed = 1;
    }
    if (in_free || (allowed_nonempty && !in_allowed)) return json_bool(0);
    json_t *root = w27_adapter_thread_root(relates_to);
    int in_bot_thread = 0;
    const json_t *threads = json_obj_get(req, "bot_threads");
    if (root && root->type == JSON_STRING && threads && threads->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(threads); i++)
            if (!strcmp(w27_cstr(json_get(threads, i)), w27_cstr(root))) in_bot_thread = 1;
    json_free(root);
    if (in_bot_thread) return json_bool(0);
    if (body[0] == '/') return json_bool(0);
    json_t *mentioned = w27_adapter_content_mentions_bot(json_obj_get(req, "user_id"), json_obj_get(req, "body"), content);
    int m = w27_truthy(mentioned);
    json_free(mentioned);
    return json_bool(!m);
}

/* PoP: w27_adapter_extract_invite_dm_signal @ plugins/platforms/matrix/adapter.py:MatrixAdapter._extract_invite_dm_signal */
json_t *w27_adapter_extract_invite_dm_signal(const json_t *user_id, const json_t *invited_room) {
    json_t *out = json_array();
    const char *uid = w27_cstr(user_id);
    if (!uid[0] || !invited_room || invited_room->type != JSON_OBJECT) {
        json_append(out, json_bool(0));
        json_append(out, json_string(""));
        return out;
    }
    const json_t *state = json_obj_get(invited_room, "invite_state");
    if (!state || state->type != JSON_OBJECT) {
        json_append(out, json_bool(0));
        json_append(out, json_string(""));
        return out;
    }
    const json_t *events = json_obj_get(state, "events");
    if (!events || events->type != JSON_ARRAY) {
        json_append(out, json_bool(0));
        json_append(out, json_string(""));
        return out;
    }
    for (size_t i = 0; i < json_len(events); i++) {
        const json_t *ev = json_get(events, i);
        if (!ev || ev->type != JSON_OBJECT) continue;
        if (strcmp(w27_cstr(json_obj_get(ev, "type")), "m.room.member")) continue;
        if (strcmp(w27_cstr(json_obj_get(ev, "state_key")), uid)) continue;
        const json_t *content = json_obj_get(ev, "content");
        if (!content || content->type != JSON_OBJECT) continue;
        if (strcmp(w27_cstr(json_obj_get(content, "membership")), "invite")) continue;
        json_append(out, json_bool(w27_truthy(json_obj_get(content, "is_direct"))));
        json_append(out, json_string(w27_cstr(json_obj_get(ev, "sender"))));
        return out;
    }
    json_append(out, json_bool(0));
    json_append(out, json_string(""));
    return out;
}

/* PoP: w27_adapter_matrix_prompt_expired @ plugins/platforms/matrix/adapter.py:MatrixAdapter._matrix_prompt_expired */
json_t *w27_adapter_matrix_prompt_expired(const json_t *expires_at, const json_t *now) {
    if (!expires_at || expires_at->type == JSON_NULL) return json_bool(0);
    double exp = 0.0, nw = 0.0;
    if (expires_at->type == JSON_NUMBER) exp = expires_at->num_val;
    else if (expires_at->type == JSON_STRING) {
        int ok = 0;
        exp = w27_py_float(expires_at->str_val ? expires_at->str_val : "", &ok);
        if (!ok) return json_bool(0);
    } else return json_bool(0);
    nw = w27_num(now, 0.0);
    return json_bool(nw > exp);
}

/* PoP: w27_adapter_is_authorized_user @ plugins/platforms/matrix/adapter.py:MatrixAdapter._is_authorized_user */
json_t *w27_adapter_is_authorized_user(const json_t *user_id, const json_t *allow_all, const json_t *allowed_users) {
    const char *uid = w27_cstr(user_id);
    if (w27_truthy(allow_all)) return json_bool(1);
    if (allowed_users && allowed_users->type == JSON_ARRAY && uid[0])
        for (size_t i = 0; i < json_len(allowed_users); i++)
            if (!strcmp(w27_cstr(json_get(allowed_users, i)), uid)) return json_bool(1);
    return json_bool(0);
}

/* PoP: w27_adapter_reactor_allowed @ plugins/platforms/matrix/adapter.py:MatrixAdapter._validate_matrix_prompt_reactor */
json_t *w27_adapter_reactor_allowed(const json_t *sender, const json_t *requester, const json_t *require_sender) {
    int req = (!require_sender || require_sender->type == JSON_NULL) ? 1 : w27_truthy(require_sender);
    const char *rq = w27_cstr(requester);
    if (req && rq[0] && strcmp(w27_cstr(sender), rq)) return json_bool(0);
    return json_bool(1);
}

/* PoP: w27_adapter_state_event_value @ plugins/platforms/matrix/adapter.py:MatrixAdapter._state_event_value */
json_t *w27_adapter_state_event_value(const json_t *event, const json_t *key) {
    const char *k = w27_cstr(key);
    if (!event || event->type == JSON_NULL) return json_null();
    const json_t *objs[2] = {event, NULL};
    if (event->type == JSON_OBJECT) objs[1] = json_obj_get(event, "content");
    for (int i = 0; i < 2; i++) {
        const json_t *obj = objs[i];
        if (!obj || obj->type != JSON_OBJECT) continue;
        const json_t *value = json_obj_get(obj, k);
        if (w27_truthy(value)) {
            char *s = w27_pystr(value);
            json_t *out = json_string(s);
            free(s);
            return out;
        }
    }
    return json_null();
}

/* PoP: w27_adapter_room_server_name @ plugins/platforms/matrix/adapter.py:MatrixAdapter._resolve_room_identity */
json_t *w27_adapter_room_server_name(const json_t *room_id) {
    const char *rid = w27_cstr(room_id);
    const char *colon = strrchr(rid, ':');
    if (!colon) return json_null();
    char *tail = w27_strip(colon + 1);
    if (!tail[0]) { free(tail); return json_null(); }
    json_t *out = json_string(tail);
    free(tail);
    return out;
}

/* PoP: w27_adapter_room_identity @ plugins/platforms/matrix/adapter.py:MatrixAdapter._resolve_room_identity */
json_t *w27_adapter_room_identity(const json_t *req) {
    const char *room_id = w27_cstr(json_obj_get(req, "room_id"));
    const json_t *room_name = json_obj_get(req, "room_name");
    const json_t *alias = json_obj_get(req, "canonical_alias");
    const json_t *count = json_obj_get(req, "member_count");
    int is_direct = w27_truthy(json_obj_get(req, "is_direct"));
    int has_explicit_name = w27_truthy(room_name);
    int count_known = count && count->type == JSON_NUMBER;
    double member_count = count_known ? count->num_val : 0.0;
    int likely_dm = (count_known && member_count <= 2.0) || (is_direct && !has_explicit_name);
    json_t *out = json_object();
    json_t *server = w27_adapter_room_server_name(json_obj_get(req, "room_id"));
    json_set(out, "server_name", server ? server : json_null());
    if (w27_truthy(room_name)) json_set(out, "display_name", json_copy(room_name));
    else if (w27_truthy(alias)) json_set(out, "display_name", json_copy(alias));
    else json_set(out, "display_name", json_string(room_id));
    json_set(out, "has_explicit_name", json_bool(has_explicit_name));
    json_set(out, "chat_type", json_string(likely_dm ? "dm" : "room"));
    int conflict = is_direct && has_explicit_name && (!count_known || member_count > 2.0);
    json_set(out, "conflict", json_bool(conflict));
    return out;
}

/* PoP: w27_adapter_presence_valid @ plugins/platforms/matrix/adapter.py:MatrixAdapter._VALID_PRESENCE_STATES */
json_t *w27_adapter_presence_valid(const json_t *state) {
    const char *s = w27_cstr(state);
    int ok = !strcmp(s, "online") || !strcmp(s, "offline") || !strcmp(s, "unavailable");
    return json_bool(ok);
}

/* ------------------------------------------------------------------ */
/* p5: HTML rendering (html.escape + the regex Markdown fallback)      */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_html_escape @ plugins/platforms/matrix/adapter.py:_html_escape (html.escape) */
static void w27_html_escape_into(w27_sb *b, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        switch (s[i]) {
        case '&':  w27_sb_put(b, "&amp;"); break;
        case '<':  w27_sb_put(b, "&lt;"); break;
        case '>':  w27_sb_put(b, "&gt;"); break;
        case '"':  w27_sb_put(b, "&quot;"); break;
        case '\'': w27_sb_put(b, "&#x27;"); break;
        default:   w27_sb_putc(b, s[i]);
        }
    }
}

json_t *w27_adapter_html_escape(const json_t *text) {
    const char *s = w27_cstr(text);
    w27_sb b; w27_sb_init(&b);
    w27_html_escape_into(&b, s, strlen(s));
    char *r = w27_sb_take(&b);
    json_t *out = json_string(r);
    free(r);
    return out;
}

/* ---- placeholder table for the markdown renderer ---- */
typedef struct { char **items; size_t n, cap; } w27_ph;

static void w27_ph_add(w27_ph *ph, w27_sb *frag) {
    if (ph->n == ph->cap) {
        ph->cap = ph->cap ? ph->cap * 2 : 8;
        ph->items = (char **)realloc(ph->items, ph->cap * sizeof(char *));
    }
    ph->items[ph->n++] = w27_sb_take(frag);
}

static void w27_ph_token(w27_sb *b, size_t idx) {
    char tmp[40];
    snprintf(tmp, sizeof tmp, "\x01PROTECTED%zu\x01", idx);
    w27_sb_put(b, tmp);
}

static void w27_ph_free(w27_ph *ph) {
    for (size_t i = 0; i < ph->n; i++) free(ph->items[i]);
    free(ph->items);
    ph->items = NULL; ph->n = ph->cap = 0;
}

/* ```(\w*)\n(.*?)``` */
static char *w27_md_fenced(const char *s, w27_ph *ph) {
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (i + 3 <= n && !strncmp(s + i, "```", 3)) {
            size_t lp = i + 3;
            while (lp < n && ((s[lp] >= '0' && s[lp] <= '9') || (s[lp] >= 'a' && s[lp] <= 'z') ||
                              (s[lp] >= 'A' && s[lp] <= 'Z') || s[lp] == '_'))
                lp++;
            if (lp < n && s[lp] == '\n') {
                const char *close = strstr(s + lp + 1, "```");
                if (close) {
                    size_t body_start = lp + 1, body_end = (size_t)(close - s);
                    w27_sb frag; w27_sb_init(&frag);
                    if (lp > i + 3) {
                        w27_sb_put(&frag, "<pre><code class=\"language-");
                        w27_html_escape_into(&frag, s + i + 3, lp - (i + 3));
                        w27_sb_put(&frag, "\">");
                    } else {
                        w27_sb_put(&frag, "<pre><code>");
                    }
                    w27_html_escape_into(&frag, s + body_start, body_end - body_start);
                    w27_sb_put(&frag, "</code></pre>");
                    size_t idx = ph->n;
                    w27_ph_add(ph, &frag);
                    w27_ph_token(&out, idx);
                    i = body_end + 3;
                    continue;
                }
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    return w27_sb_take(&out);
}

/* `([^`\n]+)` */
static char *w27_md_inline_code(const char *s, w27_ph *ph) {
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (s[i] == '`') {
            size_t j = i + 1;
            while (j < n && s[j] != '`' && s[j] != '\n') j++;
            if (j < n && s[j] == '`' && j > i + 1) {
                w27_sb frag; w27_sb_init(&frag);
                w27_sb_put(&frag, "<code>");
                w27_html_escape_into(&frag, s + i + 1, j - (i + 1));
                w27_sb_put(&frag, "</code>");
                size_t idx = ph->n;
                w27_ph_add(ph, &frag);
                w27_ph_token(&out, idx);
                i = j + 1;
                continue;
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    return w27_sb_take(&out);
}

/* \[([^\]]+)\]\(([^)]+)\) */
static char *w27_md_links(const char *s, w27_ph *ph) {
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (s[i] == '[') {
            size_t j = i + 1;
            while (j < n && s[j] != ']') j++;
            if (j < n && s[j] == ']' && j > i + 1 && j + 1 < n && s[j + 1] == '(') {
                size_t k = j + 2;
                while (k < n && s[k] != ')') k++;
                if (k < n && k > j + 2) {
                    w27_sb frag; w27_sb_init(&frag);
                    w27_sb_put(&frag, "<a href=\"");
                    {
                        char *rawurl = w27_strdup2(s + j + 2, k - (j + 2));
                        json_t *san = w27_adapter_sanitize_link_url(json_string(rawurl));
                        w27_sb_put(&frag, w27_cstr(san));
                        json_free(san);
                        free(rawurl);
                    }
                    w27_sb_put(&frag, "\">");
                    w27_html_escape_into(&frag, s + i + 1, j - (i + 1));
                    w27_sb_put(&frag, "</a>");
                    size_t idx = ph->n;
                    w27_ph_add(ph, &frag);
                    w27_ph_token(&out, idx);
                    i = k + 1;
                    continue;
                }
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    return w27_sb_take(&out);
}

/* re.split(r"(\x01PROTECTED\d+\x01)") + html.escape over the non-token parts. */
static size_t w27_token_at2(const char *s, size_t pos) {
    static const char prefix[] = "\x01PROTECTED";
    size_t pl = sizeof(prefix) - 1;
    if (strncmp(s + pos, prefix, pl) != 0) return (size_t)-1;
    size_t i = pos + pl, val = 0, digits = 0;
    while (s[i] >= '0' && s[i] <= '9') { val = val * 10 + (size_t)(s[i] - '0'); i++; digits++; }
    if (!digits || s[i] != '\x01') return (size_t)-1;
    return val;
}

static char *w27_md_escape_non_tokens(const char *s) {
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0, seg = 0;
    while (i < n) {
        if (s[i] == '\x01') {
            size_t idx = w27_token_at2(s, i);
            if (idx != (size_t)-1) {
                w27_html_escape_into(&out, s + seg, i - seg);
                size_t start = i;
                while (s[i] != '\x01' || i == start) i++;
                i++;
                w27_sb_putn(&out, s + start, i - start);
                seg = i;
                continue;
            }
        }
        i++;
    }
    w27_html_escape_into(&out, s + seg, n - seg);
    return w27_sb_take(&out);
}

/* ^[\s]*([-*_])\s*\1\s*\1[\s\-*_]*$ */
static int w27_md_is_hr(const char *line) {
    size_t i = 0;
    while (line[i] && w27_isspace_ascii((unsigned char)line[i])) i++;
    char c = line[i];
    if (c != '-' && c != '*' && c != '_') return 0;
    i++;
    while (line[i] && w27_isspace_ascii((unsigned char)line[i])) i++;
    if (line[i] != c) return 0;
    i++;
    while (line[i] && w27_isspace_ascii((unsigned char)line[i])) i++;
    if (line[i] != c) return 0;
    i++;
    for (; line[i]; i++)
        if (!(w27_isspace_ascii((unsigned char)line[i]) || line[i] == '-' || line[i] == '*' || line[i] == '_'))
            return 0;
    return 1;
}

/* ^[\s]*[-*+]\s+(.+)$  /  ^[\s]*\d+[.)]\s+(.+)$  -> offset of the captured item, or NULL */
static const char *w27_md_list_item(const char *line, int ordered) {
    size_t i = 0;
    while (line[i] && w27_isspace_ascii((unsigned char)line[i])) i++;
    if (!ordered) {
        if (line[i] != '-' && line[i] != '*' && line[i] != '+') return NULL;
        i++;
    } else {
        size_t digits = i;
        while (line[i] >= '0' && line[i] <= '9') i++;
        if (i == digits) return NULL;
        if (line[i] != '.' && line[i] != ')') return NULL;
        i++;
    }
    if (!w27_isspace_ascii((unsigned char)line[i])) return NULL;
    while (w27_isspace_ascii((unsigned char)line[i])) i++;
    return line[i] ? line + i : NULL;
}

/* ^>(\s*) ... blockquote line detection */
static int w27_md_is_bq(const char *line) {
    return !strncmp(line, "&gt; ", 5) || !strncmp(line, "> ", 2) ||
           !strcmp(line, "&gt;") || !strcmp(line, ">");
}

static const char *w27_md_bq_content(const char *line) {
    if (!strncmp(line, "&gt; ", 5)) return line + 5;
    if (!strncmp(line, "> ", 2)) return line + 2;
    return "";
}

/* One global non-greedy `open ... close` substitution (DOTALL, leftmost). */
static char *w27_sub_pair(const char *s, const char *open, const char *close,
                          const char *pre, const char *post, int underscore_guard) {
    w27_sb out; w27_sb_init(&out);
    size_t n = strlen(s), i = 0, ol = strlen(open), cl = strlen(close);
    while (i < n) {
        if (i + ol <= n && !strncmp(s + i, open, ol)) {
            if (!underscore_guard || i == 0 || !w27_is_word((unsigned char)s[i - 1])) {
                size_t j = i + ol + 1;
                int matched = 0;
                while (j + cl <= n) {
                    if (!strncmp(s + j, close, cl)) {
                        if (underscore_guard && s[j + cl] && w27_is_word((unsigned char)s[j + cl])) {
                            j++;
                            continue;
                        }
                        matched = 1;
                        break;
                    }
                    j++;
                }
                if (matched) {
                    w27_sb_put(&out, pre);
                    w27_sb_putn(&out, s + i + ol, j - (i + ol));
                    w27_sb_put(&out, post);
                    i = j + cl;
                    continue;
                }
            }
        }
        w27_sb_putc(&out, s[i]);
        i++;
    }
    return w27_sb_take(&out);
}

static int w27_tag_prefix(const char *p, int allow_hr) {
    /* p points at '<' of a candidate tag; returns name length when it is one. */
    if (p[0] != '<') return 0;
    size_t i = 1;
    if (p[i] == '/') i++;
    static const char *const names[] = {"pre", "blockquote", "ul", "ol", "li", "hr"};
    for (size_t k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
        size_t nl = strlen(names[k]);
        if (!strncmp(p + i, names[k], nl)) {
            if (!allow_hr && !strcmp(names[k], "hr")) continue;
            return (int)(i + nl);
        }
    }
    if (p[i] == 'h' && p[i + 1] >= '1' && p[i + 1] <= '6') return (int)(i + 2);
    return 0;
}

/* PoP: w27_adapter_markdown_to_html_fallback @ plugins/platforms/matrix/adapter.py:MatrixAdapter._markdown_to_html_fallback */
json_t *w27_adapter_markdown_to_html_fallback(const json_t *text) {
    w27_ph ph; ph.items = NULL; ph.n = ph.cap = 0;
    char *stage;
    {
        char *s1 = w27_md_fenced(w27_cstr(text), &ph);
        char *s2 = w27_md_inline_code(s1, &ph);
        char *s3 = w27_md_links(s2, &ph);
        free(s1); free(s2);
        stage = s3;
    }
    {
        char *escaped = w27_md_escape_non_tokens(stage);
        free(stage);
        stage = escaped;
    }
    /* block-level pass */
    {
        w27_sb out; w27_sb_init(&out);
        size_t nlines = 1;
        for (const char *q = stage; *q; q++) if (*q == '\n') nlines++;
        char **lines = (char **)calloc(nlines, sizeof(char *));
        size_t li = 0, start = 0;
        for (size_t i = 0;; i++) {
            if (stage[i] == '\n' || stage[i] == '\0') {
                lines[li++] = w27_strdup2(stage + start, i - start);
                if (stage[i] == '\0') break;
                start = i + 1;
            }
        }
        int first_out = 1;
        for (size_t i = 0; i < li;) {
            char *line = lines[i];
            if (w27_md_is_hr(line)) {
                if (!first_out) w27_sb_putc(&out, '\n');
                w27_sb_put(&out, "<hr>");
                first_out = 0;
                i++;
                continue;
            }
            size_t hashes = 0;
            while (line[hashes] == '#') hashes++;
            if (hashes >= 1 && hashes <= 6 && w27_isspace_ascii((unsigned char)line[hashes])) {
                size_t k = hashes;
                while (w27_isspace_ascii((unsigned char)line[k])) k++;
                if (line[k]) {
                    char *body = w27_strip(line + k);
                    char tmp[16];
                    snprintf(tmp, sizeof tmp, "<h%zu>", hashes);
                    if (!first_out) w27_sb_putc(&out, '\n');
                    w27_sb_put(&out, tmp);
                    w27_sb_put(&out, body);
                    snprintf(tmp, sizeof tmp, "</h%zu>", hashes);
                    w27_sb_put(&out, tmp);
                    free(body);
                    first_out = 0;
                    i++;
                    continue;
                }
            }
            if (w27_md_is_bq(line)) {
                if (!first_out) w27_sb_putc(&out, '\n');
                w27_sb_put(&out, "<blockquote>");
                int firstb = 1;
                while (i < li && w27_md_is_bq(lines[i])) {
                    if (!firstb) w27_sb_put(&out, "<br>");
                    w27_sb_put(&out, w27_md_bq_content(lines[i]));
                    firstb = 0;
                    i++;
                }
                w27_sb_put(&out, "</blockquote>");
                first_out = 0;
                continue;
            }
            {
                int tagged = 0;
                for (int ordered = 0; ordered < 2 && !tagged; ordered++) {
                    const char *item = w27_md_list_item(line, ordered);
                    if (!item) continue;
                    if (!first_out) w27_sb_putc(&out, '\n');
                    w27_sb_put(&out, ordered ? "<ol>" : "<ul>");
                    while (i < li) {
                        const char *it = w27_md_list_item(lines[i], ordered);
                        if (!it) break;
                        w27_sb_put(&out, "<li>");
                        w27_sb_put(&out, it);
                        w27_sb_put(&out, "</li>");
                        i++;
                    }
                    w27_sb_put(&out, ordered ? "</ol>" : "</ul>");
                    first_out = 0;
                    tagged = 1;
                }
                if (tagged) continue;
            }
            if (!first_out) w27_sb_putc(&out, '\n');
            w27_sb_put(&out, line);
            first_out = 0;
            i++;
        }
        for (size_t i = 0; i < li; i++) free(lines[i]);
        free(lines);
        free(stage);
        stage = w27_sb_take(&out);
    }
    /* inline emphasis, in the artifact's order */
    {
        char *a = w27_sub_pair(stage, "**", "**", "<strong>", "</strong>", 0);
        char *b = w27_sub_pair(a, "__", "__", "<strong>", "</strong>", 0);
        char *c = w27_sub_pair(b, "*", "*", "<em>", "</em>", 0);
        char *d = w27_sub_pair(c, "_", "_", "<em>", "</em>", 1);
        char *e = w27_sub_pair(d, "~~", "~~", "<del>", "</del>", 0);
        free(a); free(b); free(c); free(d);
        free(stage);
        stage = e;
    }
    /* newline -> <br>\n, then the two block-adjacency fixups */
    {
        w27_sb out; w27_sb_init(&out);
        for (const char *q = stage; *q; q++) {
            if (*q == '\n') w27_sb_put(&out, "<br>\n");
            else w27_sb_putc(&out, *q);
        }
        char *with_br = w27_sb_take(&out);
        /* fixup 1: <br>\n</?(pre|blockquote|h[1-6]|ul|ol|li|hr) -> \n + tag */
        w27_sb f1; w27_sb_init(&f1);
        size_t n = strlen(with_br);
        for (size_t i = 0; i < n;) {
            if (i + 5 <= n && !strncmp(with_br + i, "<br>\n", 5) && w27_tag_prefix(with_br + i + 5, 1)) {
                w27_sb_putc(&f1, '\n');
                i += 5;
                continue;
            }
            w27_sb_putc(&f1, with_br[i]);
            i++;
        }
        free(with_br);
        char *fixed1 = w27_sb_take(&f1);
        /* fixup 2: (</(pre|blockquote|h[1-6]|ul|ol|li)>)<br> -> \1 */
        w27_sb f2; w27_sb_init(&f2);
        n = strlen(fixed1);
        for (size_t i = 0; i < n;) {
            if (fixed1[i] == '<' && fixed1[i + 1] == '/') {
                int tl = w27_tag_prefix(fixed1 + i, 0);
                if (tl && fixed1[i + tl] == '>' && !strncmp(fixed1 + i + tl + 1, "<br>", 4)) {
                    w27_sb_putn(&f2, fixed1 + i, (size_t)tl + 1);
                    i += (size_t)tl + 1 + 4;
                    continue;
                }
            }
            w27_sb_putc(&f2, fixed1[i]);
            i++;
        }
        free(fixed1);
        free(stage);
        stage = w27_sb_take(&f2);
    }
    /* restore placeholders */
    for (size_t idx = 0; idx < ph.n; idx++) {
        char token[40];
        snprintf(token, sizeof token, "\x01PROTECTED%zu\x01", idx);
        w27_sb rebuilt; w27_sb_init(&rebuilt);
        size_t tl = strlen(token), pos = 0;
        const char *hit;
        while ((hit = strstr(stage + pos, token)) != NULL) {
            w27_sb_putn(&rebuilt, stage + pos, (size_t)(hit - (stage + pos)));
            w27_sb_put(&rebuilt, ph.items[idx]);
            pos = (size_t)(hit - stage) + tl;
        }
        w27_sb_put(&rebuilt, stage + pos);
        free(stage);
        stage = w27_sb_take(&rebuilt);
    }
    json_t *out = json_string(stage);
    free(stage);
    w27_ph_free(&ph);
    return out;
}

/* ------------------------------------------------------------------ */
/* p6: constants and dispatch tables                                   */
/* ------------------------------------------------------------------ */

/* PoP: w27_adapter_constants @ plugins/platforms/matrix/adapter.py:<module/class constants> */
json_t *w27_adapter_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_content_bytes", json_number(45000.0));
    json_set(o, "max_message_length_ceiling", json_number(15000.0));
    json_set(o, "default_max_message_length", json_number(15000.0));
    json_set(o, "split_threshold", json_number(14900.0));
    json_set(o, "startup_grace_seconds", json_number(5.0));
    json_set(o, "voice_waveform_bins", json_number(30.0));
    json_set(o, "exec_approval_cmd_budget", json_number(2000.0));
    return o;
}

/* PoP: w27_adapter_model_picker_reactions @ plugins/platforms/matrix/adapter.py:_MATRIX_MODEL_PICKER_REACTIONS */
json_t *w27_adapter_model_picker_reactions(void) {
    json_t *a = json_array();
    for (size_t i = 0; i < 10; i++) json_append(a, json_string(W27_MODEL_REACTIONS[i]));
    return a;
}

/* PoP: w27_adapter_choice_picker_reactions @ plugins/platforms/matrix/adapter.py:_MATRIX_CHOICE_PICKER_REACTIONS */
json_t *w27_adapter_choice_picker_reactions(void) {
    json_t *a = json_array();
    for (size_t i = 0; i < 12; i++) json_append(a, json_string(W27_CHOICE_REACTIONS[i]));
    return a;
}

/* PoP: w27_adapter_approval_reaction_map @ plugins/platforms/matrix/adapter.py:MatrixAdapter._approval_reaction_map */
json_t *w27_adapter_approval_reaction_map(void) {
    json_t *o = json_object();
    json_set(o, "\xE2\x9C\x85", json_string("once"));
    json_set(o, "\xF0\x9F\x8C\x80", json_string("session"));
    json_set(o, "\xE2\x99\xBE\xEF\xB8\x8F", json_string("always"));
    json_set(o, "\xE2\x99\xBE", json_string("always"));
    json_set(o, "\xE2\x9D\x8C", json_string("deny"));
    json_set(o, "\xE2\x9D\x8E", json_string("deny"));
    return o;
}

/* PoP: w27_adapter_yaml_bridge @ plugins/platforms/matrix/adapter.py:_YAML_BRIDGE */
json_t *w27_adapter_yaml_bridge(void) {
    static const char *const rows[][3] = {
        {"require_mention", "MATRIX_REQUIRE_MENTION", "lower"},
        {"process_notices", "MATRIX_PROCESS_NOTICES", "lower"},
        {"session_scope", "MATRIX_SESSION_SCOPE", "lower"},
        {"auto_thread", "MATRIX_AUTO_THREAD", "lower"},
        {"dm_mention_threads", "MATRIX_DM_MENTION_THREADS", "lower"},
        {"allowed_users", "MATRIX_ALLOWED_USERS", "csv"},
        {"free_response_rooms", "MATRIX_FREE_RESPONSE_ROOMS", "csv"},
        {"allowed_rooms", "MATRIX_ALLOWED_ROOMS", "csv"},
        {"ignore_user_patterns", "MATRIX_IGNORE_USER_PATTERNS", "csv"},
        {"max_message_length", "MATRIX_MAX_MESSAGE_LENGTH", "str"},
    };
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        json_t *row = json_array();
        json_append(row, json_string(rows[i][0]));
        json_append(row, json_string(rows[i][1]));
        json_append(row, json_string(rows[i][2]));
        json_append(out, row);
    }
    return out;
}

/* PoP: w27_adapter_image_exts @ plugins/platforms/matrix/adapter.py:_MATRIX_IMAGE_FILENAME_EXTS */
json_t *w27_adapter_image_exts(void) {
    const char *sorted[] = {".avif", ".bmp", ".gif", ".heic", ".heif", ".jpeg",
                            ".jpg", ".png", ".svg", ".webp"};
    json_t *a = json_array();
    for (size_t i = 0; i < sizeof(sorted) / sizeof(sorted[0]); i++) json_append(a, json_string(sorted[i]));
    return a;
}

/* PoP: w27_adapter_media_exts @ plugins/platforms/matrix/adapter.py:_MATRIX_MEDIA_FILENAME_EXTS */
json_t *w27_adapter_media_exts(void) {
    const char *sorted[] = {".aac", ".amr", ".flac", ".m4a", ".mkv", ".mov", ".mp3",
                            ".mp4", ".oga", ".ogg", ".opus", ".wav", ".webm"};
    json_t *a = json_array();
    for (size_t i = 0; i < sizeof(sorted) / sizeof(sorted[0]); i++) json_append(a, json_string(sorted[i]));
    return a;
}

/* PoP: w27_adapter_txn_id @ plugins/platforms/matrix/adapter.py:_standalone_send */
json_t *w27_adapter_txn_id(const json_t *ms, const json_t *rand_hex) {
    char tmp[64];
    long long v = (long long)w27_num(ms, 0.0);
    snprintf(tmp, sizeof tmp, "hermes_%lld_", v);
    w27_sb b; w27_sb_init(&b);
    w27_sb_put(&b, tmp);
    w27_sb_put(&b, w27_cstr(rand_hex));
    char *s = w27_sb_take(&b);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: w27_adapter_duplicate_events @ plugins/platforms/matrix/adapter.py:MatrixAdapter._is_duplicate_event */
json_t *w27_adapter_duplicate_events(const json_t *ids) {
    enum { MAXLEN = 1000 };
    char **ring = (char **)calloc(MAXLEN, sizeof(char *));
    size_t count = 0, head = 0;
    json_t *out = json_array();
    if (!ring) return out;
    if (!ids || ids->type != JSON_ARRAY) { free(ring); return out; }
    for (size_t i = 0; i < json_len(ids); i++) {
        json_t *v = json_get(ids, i);
        int dup = 0;
        if (!w27_truthy(v)) {
            json_append(out, json_bool(0));
            continue;
        }
        char *id = w27_pystr(v);
        if (!id[0]) { free(id); json_append(out, json_bool(0)); continue; }
        for (size_t k = 0; k < count; k++) {
            char *slotv = ring[(head + k) % MAXLEN];
            if (slotv && !strcmp(slotv, id)) { dup = 1; break; }
        }
        if (dup) { json_append(out, json_bool(1)); free(id); continue; }
        if (count == MAXLEN) {
            free(ring[head]);
            ring[head] = NULL;
            head = (head + 1) % MAXLEN;
            count--;
        }
        ring[(head + count) % MAXLEN] = id;
        count++;
        json_append(out, json_bool(0));
    }
    for (size_t k = 0; k < count; k++) free(ring[(head + k) % MAXLEN]);
    free(ring);
    return out;
}
