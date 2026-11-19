/*
 * base_flow.c — platform-adapter base FLOW cores of gateway/platforms/base.py.
 *
 * Ports the EXECUTION FLOW of BasePlatformAdapter: busy-message routing,
 * send/receive orchestration, lifecycle lock/plugin wiring, inbound text
 * batching, session-task ownership handoff, ledgered final delivery,
 * warning/media-fallback presentation, approval/clarify prompts and source
 * building — decision sequences, call order, branch conditions, state
 * transitions and error paths, not just leaf outputs.
 *
 * All live I/O enters through the universal seam (gwb_seams_t.call); the
 * cores below perform no I/O. Adapter state is injected json_t (st) and
 * mutated in place; asyncio.Event/Task identity is modeled by "id" fields.
 * See include/gateway_base_flow.h for the per-flow seam name tables.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gateway_base_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* ── internal helpers (no Python counterpart; semantics documented) ── */

/* Python truthiness for a JSON value (NULL pointer == Python None). */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python str() for scalars; containers serialize as JSON (fixtures keep
 * str()-converted fields scalar, matching the live call sites). */
static char *jpystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Number of Unicode code points in a UTF-8 string (Python len(str)). */
static size_t cp_len(const char *s) {
    if (!s) return 0;
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset just past the first n code points (Python s[:n] boundary). */
static size_t cp_byte_offset(const char *s, size_t n) {
    if (!s) return 0;
    size_t bytes = 0, cps = 0;
    while (s[bytes] && cps < n) {
        unsigned char c = (unsigned char)s[bytes];
        bytes++;
        if ((c & 0xC0) != 0x80) cps++;
    }
    return bytes;
}

/* First n code points of s (Python s[:n]). malloc'd. */
static char *cp_slice(const char *s, size_t n) {
    if (!s) return strdup("");
    size_t bytes = cp_byte_offset(s, n);
    char *out = malloc(bytes + 1);
    if (!out) return NULL;
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

static bool str_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

static bool str_endswith(const char *s, const char *p) {
    if (!s || !p) return false;
    size_t ls = strlen(s), lp = strlen(p);
    return ls >= lp && strcmp(s + ls - lp, p) == 0;
}

static bool str_contains(const char *s, const char *p) {
    return s && p && strstr(s, p) != NULL;
}

/* ASCII lowercase (live error text / platform names are ASCII at the call
 * sites; non-ASCII passes through unchanged like Python for these inputs). */
static char *str_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static bool is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() (ASCII whitespace subset; fixtures are ASCII). */
static char *str_strip(const char *s) {
    if (!s) return strdup("");
    while (*s && is_py_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && is_py_ws(s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python str.rstrip() (no argument). */
static char *str_rstrip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && is_py_ws(s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* getattr(obj, name, None) on a JSON model: absent key == None. */
static const json_t *jgetattr(const json_t *obj, const char *name) {
    return json_obj_get(obj, name);
}

/* _platform_name: getattr(platform, "value", platform); str(value or "").lower(). */
static char *platform_name(const json_t *platform) {
    const json_t *v = platform;
    if (platform && platform->type == JSON_OBJECT) {
        const json_t *val = json_obj_get(platform, "value");
        if (val) v = val;
    }
    char *s = jtruth(v) ? jpystr(v) : strdup("");
    char *low = str_lower(s);
    free(s);
    return low;
}

static char *platform_name_str(const char *platform) {
    json_t *p = platform ? json_string(platform) : json_null();
    char *r = platform_name(p);
    json_free(p);
    return r;
}

/* ── trace recorder ── */

void gwb_trace_add(gwb_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void gwb_trace_free(gwb_trace_t *t) {
    if (!t) return;
    json_free(t->events);
    t->events = NULL;
}

/* ── universal seam helpers ── */

json_t *gwb_call(const gwb_seams_t *s, const char *name, const json_t *args) {
    if (!s || !s->call) return NULL;
    return s->call(s->ctx, name, args);
}

const char *gwb_raise_class(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    return jstr(json_obj_get(r, "__raise__"));
}

bool gwb_caught(const json_t *r, const char *cls) {
    const char *rc = gwb_raise_class(r);
    return rc && strcmp(rc, cls) == 0;
}

/* Convenience: build an args object from key/value pairs (values borrowed,
 * copied into the args). NULL value == json null. */
static json_t *args0(void) { return json_object(); }
static void aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* json_copy that maps NULL (absent/None) to an explicit JSON null — json_set
 * silently drops NULL values, which would lose Python None fields. */
static json_t *jdup(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* log seam: {"level": ..., "msg": ...} — stands in for logger.<level>(msg). */
static void seam_log(const gwb_seams_t *s, const char *level, const char *msg) {
    json_t *a = args0();
    asets(a, "level", level);
    asets(a, "msg", msg);
    json_t *r = gwb_call(s, "log", a);
    json_free(a);
    json_free(r);
}

/* ── _or_default ── */

/* PoP: gwb_or_default @ gateway/platforms/base.py:_or_default */
json_t *gwb_or_default(gwb_thunk_fn thunk, void *ctx, const json_t *defval,
                       int exc_mask, int *out_exc) {
    if (out_exc) *out_exc = 0;
    int exc = 0;
    json_t *r = thunk ? thunk(ctx, &exc) : NULL;
    if (exc == 0) return r;
    json_free(r);
    /* exc kinds: 1 TypeError (bit0), 2 ValueError (bit1), 3 KeyError (bit2). */
    bool caught = (exc == 1 && (exc_mask & 1)) || (exc == 2 && (exc_mask & 2))
                  || (exc == 3 && (exc_mask & 4));
    if (caught) return defval ? json_copy(defval) : json_null();
    if (out_exc) *out_exc = exc;
    return NULL;
}

/* ── thread metadata ── */

/* PoP: gwb_thread_metadata_for_source @ gateway/platforms/base.py:_thread_metadata_for_source */
json_t *gwb_thread_metadata_for_source(const json_t *source, const char *reply_to_message_id) {
    const json_t *thread_id = jgetattr(source, "thread_id");
    char *platform = platform_name(jgetattr(source, "platform"));
    json_t *metadata = json_object();
    if (thread_id && thread_id->type != JSON_NULL)
        json_set(metadata, "thread_id", json_copy(thread_id));
    const json_t *scope_id = NULL;
    if (strcmp(platform, "slack") == 0) scope_id = jgetattr(source, "scope_id");
    if (jtruth(scope_id)) {
        char *sid = jpystr(scope_id);
        json_set(metadata, "slack_team_id", json_string(sid));
        free(sid);
    }
    if (metadata->c.count == 0) {
        json_free(metadata);
        free(platform);
        return NULL;
    }
    const json_t *chat_type = jgetattr(source, "chat_type");
    if (strcmp(platform, "telegram") == 0 && jstr(chat_type)
            && strcmp(jstr(chat_type), "dm") == 0) {
        json_set(metadata, "telegram_dm_topic_reply_fallback", json_bool(true));
        char *tid = thread_id ? jpystr(thread_id) : strdup("None");
        if (strcmp(tid, "") != 0 && strcmp(tid, "1") != 0)
            json_set(metadata, "direct_messages_topic_id", json_string(tid));
        /* anchor = reply_to_message_id or getattr(source, "message_id", None) */
        const json_t *anchor = NULL;
        json_t *anchor_tmp = NULL;
        if (reply_to_message_id && reply_to_message_id[0] != '\0') {
            anchor_tmp = json_string(reply_to_message_id);
            anchor = anchor_tmp;
        } else if (jtruth(jgetattr(source, "message_id"))) {
            anchor = jgetattr(source, "message_id");
        }
        if (anchor && anchor->type != JSON_NULL) {
            char *as = jpystr(anchor);
            json_set(metadata, "telegram_reply_to_message_id", json_string(as));
            free(as);
        }
        json_free(anchor_tmp);
        free(tid);
    }
    const json_t *profile_v = jgetattr(source, "profile");
    char *profile_raw = jtruth(profile_v) ? jpystr(profile_v) : strdup("");
    char *profile = str_strip(profile_raw);
    free(profile_raw);
    if (profile[0] != '\0')
        json_set(metadata, "hermes_profile", json_string(profile));
    free(profile);
    free(platform);
    return metadata;
}

/* PoP: gwb_thread_metadata_for_event @ gateway/platforms/base.py:_thread_metadata_for_event */
json_t *gwb_thread_metadata_for_event(const json_t *event, const gwb_seams_t *s) {
    json_t *a = args0();
    aset(a, "event", event);
    json_t *anchor = gwb_call(s, "reply_anchor", a);
    json_free(a);
    const char *anchor_s = jstr(anchor);
    json_t *r = gwb_thread_metadata_for_source(jgetattr(event, "source"), anchor_s);
    json_free(anchor);
    return r;
}

/* PoP: gwb_mark_notify_metadata @ gateway/platforms/base.py:_mark_notify_metadata */
json_t *gwb_mark_notify_metadata(const json_t *metadata) {
    json_t *out = jtruth(metadata) ? json_copy(metadata) : json_object();
    json_set(out, "notify", json_bool(true));
    return out;
}

/* ── media failure text / audio routing ── */

static char *seam_t(const gwb_seams_t *s, const char *key, const json_t *kwargs) {
    json_t *a = args0();
    asets(a, "key", key);
    aset(a, "kwargs", kwargs);
    json_t *r = gwb_call(s, "t", a);
    json_free(a);
    char *out = jstr(r) ? strdup(jstr(r)) : strdup("");
    json_free(r);
    return out;
}

/* PoP: gwb_media_failure_text @ gateway/platforms/base.py:_media_failure_text */
char *gwb_media_failure_text(const char *kind, const char *file_name, const gwb_seams_t *s) {
    static const char *const kinds[] = {"audio", "video", "file", "image"};
    bool known = false;
    for (size_t i = 0; i < 4; i++)
        if (kind && strcmp(kind, kinds[i]) == 0) { known = true; break; }
    char *kind_label;
    if (known) {
        char key[128];
        snprintf(key, sizeof(key), "gateway.notify.media_kind.%s", kind);
        kind_label = seam_t(s, key, NULL);
    } else {
        kind_label = strdup(kind ? kind : "");
    }
    json_t *kw = json_object();
    json_set(kw, "kind", json_string(kind_label));
    char *out;
    if (file_name && file_name[0] != '\0') {
        json_set(kw, "file_name", json_string(file_name));
        out = seam_t(s, "gateway.notify.media_delivery_failed_with_name", kw);
    } else {
        out = seam_t(s, "gateway.notify.media_delivery_failed", kw);
    }
    json_free(kw);
    free(kind_label);
    return out;
}

/* PoP: gwb_should_send_media_as_audio @ gateway/platforms/base.py:should_send_media_as_audio */
bool gwb_should_send_media_as_audio(const char *platform, const char *ext, bool is_voice) {
    static const char *const audio_exts[] = {".ogg", ".opus", ".mp3", ".m2a", ".wav", ".m4a", ".flac"};
    char *norm = str_lower(ext ? ext : "");
    bool is_audio = false;
    for (size_t i = 0; i < sizeof(audio_exts) / sizeof(audio_exts[0]); i++)
        if (strcmp(norm, audio_exts[i]) == 0) { is_audio = true; break; }
    if (!is_audio) { free(norm); return false; }
    char *pname = platform_name_str(platform);
    bool telegram = strcmp(pname, "telegram") == 0;
    free(pname);
    if (!telegram) { free(norm); return true; }
    bool r = is_voice || strcmp(norm, ".mp3") == 0 || strcmp(norm, ".m4a") == 0;
    free(norm);
    return r;
}

/* ── prefix fitting ── */

/* PoP: gwb_custom_unit_to_cp @ gateway/platforms/base.py:_custom_unit_to_cp */
int gwb_custom_unit_to_cp(const char *s, int budget, gwb_len_fn len_fn, void *ctx) {
    const char *str = s ? s : "";
    int total = (int)cp_len(str);
    if (len_fn(ctx, str) <= budget) return total;
    int lo = 0, hi = total;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        char *pref = cp_slice(str, (size_t)mid);
        int units = len_fn(ctx, pref);
        free(pref);
        if (units <= budget) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

/* PoP: gwb_fit_prefix @ gateway/platforms/base.py:fit_prefix */
char *gwb_fit_prefix(const char *text, int budget, gwb_len_fn measure, void *ctx,
                     const char *suffix) {
    const char *t = text ? text : "";
    const char *suf = suffix ? suffix : "...";
    int n = gwb_custom_unit_to_cp(t, budget, measure, ctx);
    if (n == (int)cp_len(t)) return strdup(t);
    char *pref = cp_slice(t, (size_t)n);
    size_t cap = strlen(pref) + strlen(suf) + 1;
    char *out = malloc(cap);
    if (out) snprintf(out, cap, "%s%s", pref, suf);
    free(pref);
    return out;
}

/* UTF-16 code unit count (static helper; utf16_len itself is claimed in
 * gateway_escape.c — not re-annotated here). */
static int utf16_units(const char *s) {
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        unsigned char c = *p;
        size_t adv;
        unsigned int cp;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; adv = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; adv = 3; }
        else { cp = c & 0x07; adv = 4; }
        for (size_t i = 1; i < adv && p[i]; i++)
            cp = (cp << 6) | (p[i] & 0x3F);
        n += (cp > 0xFFFF) ? 2 : 1;
        p += adv;
    }
    return n;
}

static int utf16_len_cb(void *ctx, const char *s) { (void)ctx; return utf16_units(s); }

/* PoP: gwb_prefix_within_utf16_limit @ gateway/platforms/base.py:_prefix_within_utf16_limit */
char *gwb_prefix_within_utf16_limit(const char *s, int limit) {
    int n = gwb_custom_unit_to_cp(s ? s : "", limit, utf16_len_cb, NULL);
    return cp_slice(s ? s : "", (size_t)n);
}

/* ── media directive scanners ── */

/* PoP: gwb_has_media_directives @ gateway/platforms/base.py:_has_media_directives */
bool gwb_has_media_directives(const char *text) {
    if (!text) return false;
    return str_contains(text, "MEDIA:") || str_contains(text, "[[audio_as_voice]]")
           || str_contains(text, "[[as_document]]");
}

/* PoP: gwb_terminal_sentinel_start @ gateway/platforms/base.py:_terminal_sentinel_start */
int gwb_terminal_sentinel_start(const char *text) {
    static const char SENT[] = "<|eos|>";
    const size_t SL = sizeof(SENT) - 1;
    char *rs = str_rstrip(text ? text : "");
    size_t end = strlen(rs);
    size_t start = end;
    while (start >= SL && strncmp(rs + start - SL, SENT, SL) == 0)
        start -= SL;
    free(rs);
    return start < end ? (int)start : -1;
}

/* Codepoint-indexed span list helpers. Spans are [start, end) codepoint
 * offsets; the scanners below walk code points so offsets match Python. */

/* PoP: gwb_code_spans @ gateway/platforms/base.py:_code_spans */
json_t *gwb_code_spans(const char *content) {
    const char *s = content ? content : "";
    /* Build a codepoint -> byte offset index for offset reporting. */
    size_t ncp = cp_len(s);
    size_t *byte_of = malloc((ncp + 1) * sizeof(size_t));
    size_t *cp_of_byte = malloc((strlen(s) + 1) * sizeof(size_t));
    size_t b = 0, c = 0;
    while (s[b]) {
        cp_of_byte[b] = c;
        byte_of[c] = b;
        unsigned char ch = (unsigned char)s[b];
        do { b++; } while (s[b] && ((unsigned char)s[b] & 0xC0) == 0x80);
        c++;
    }
    byte_of[ncp] = b;
    cp_of_byte[b] = c;
    json_t *spans = json_array();
    /* _FENCED_CODE_RE: ```[^\n]*\n.*?``` (DOTALL, non-greedy, non-overlapping). */
    size_t pos = 0;
    while (pos + 3 <= b) {
        if (strncmp(s + pos, "```", 3) != 0) { pos++; continue; }
        /* [^\n]* then \n */
        size_t j = pos + 3;
        while (j < b && s[j] != '\n') j++;
        if (j >= b) { pos++; continue; }  /* no newline: pattern cannot match here */
        j++;  /* past \n */
        /* .*?``` — first closing ``` */
        size_t k = j;
        while (k + 3 <= b && strncmp(s + k, "```", 3) != 0) k++;
        if (k + 3 > b) { pos++; continue; }  /* unclosed: no match */
        json_t *sp = json_array();
        json_append(sp, json_number((double)cp_of_byte[pos]));
        json_append(sp, json_number((double)cp_of_byte[k + 3]));
        json_append(spans, sp);
        pos = k + 3;
    }
    /* _INLINE_CODE_RE: `[^`\n]+` (non-overlapping). */
    pos = 0;
    while (pos < b) {
        if (s[pos] != '`') { pos++; continue; }
        size_t j = pos + 1;
        while (j < b && s[j] != '`' && s[j] != '\n') j++;
        if (j < b && s[j] == '`' && j > pos + 1) {
            json_t *sp = json_array();
            json_append(sp, json_number((double)cp_of_byte[pos]));
            json_append(sp, json_number((double)cp_of_byte[j + 1]));
            json_append(spans, sp);
            pos = j + 1;
        } else {
            pos++;
        }
    }
    free(byte_of);
    free(cp_of_byte);
    return spans;
}

/* PoP: gwb_blank_spans @ gateway/platforms/base.py:_blank_spans */
char *gwb_blank_spans(const char *text, const json_t *spans) {
    const char *s = text ? text : "";
    size_t ncp = cp_len(s);
    char *out = malloc(strlen(s) + 1);
    if (!out) return NULL;
    strcpy(out, s);
    /* Mark codepoints to blank, preserving newlines. */
    bool *blank = calloc(ncp + 1, sizeof(bool));
    size_t nsp = spans ? json_len(spans) : 0;
    for (size_t i = 0; i < nsp; i++) {
        const json_t *sp = json_get(spans, i);
        if (!sp || json_len(sp) < 2) continue;
        long long st = (long long)json_get(sp, 0)->num_val;
        long long en = (long long)json_get(sp, 1)->num_val;
        if (st < 0) st = 0;
        if (en > (long long)ncp) en = (long long)ncp;
        for (long long k = st; k < en; k++) blank[k] = true;
    }
    size_t bi = 0, ci = 0;
    while (out[bi]) {
        unsigned char ch = (unsigned char)out[bi];
        size_t adv = 1;
        while (out[bi + adv] && ((unsigned char)out[bi + adv] & 0xC0) == 0x80) adv++;
        if (blank[ci] && ch != '\n')
            for (size_t k = 0; k < adv; k++) out[bi + k] = ' ';
        bi += adv;
        ci++;
    }
    free(blank);
    return out;
}

/* PoP: gwb_delete_spans @ gateway/platforms/base.py:_delete_spans */
char *gwb_delete_spans(const char *text, const json_t *spans) {
    const char *s = text ? text : "";
    size_t nsp = spans ? json_len(spans) : 0;
    if (nsp == 0) return strdup(s);
    /* sorted(spans) — lexicographic on (start, end). */
    long long (*sp)[2] = malloc(nsp * sizeof(*sp));
    for (size_t i = 0; i < nsp; i++) {
        const json_t *e = json_get(spans, i);
        sp[i][0] = (long long)json_get(e, 0)->num_val;
        sp[i][1] = (long long)json_get(e, 1)->num_val;
    }
    for (size_t i = 1; i < nsp; i++) {
        long long a = sp[i][0], bb = sp[i][1];
        size_t j = i;
        while (j > 0 && (sp[j - 1][0] > a || (sp[j - 1][0] == a && sp[j - 1][1] > bb))) {
            sp[j][0] = sp[j - 1][0]; sp[j][1] = sp[j - 1][1];
            j--;
        }
        sp[j][0] = a; sp[j][1] = bb;
    }
    /* Merge overlapping/nested (s <= merged[-1][1]). */
    long long (*merged)[2] = malloc(nsp * sizeof(*merged));
    size_t nm = 0;
    for (size_t i = 0; i < nsp; i++) {
        if (nm > 0 && sp[i][0] <= merged[nm - 1][1]) {
            if (sp[i][1] > merged[nm - 1][1]) merged[nm - 1][1] = sp[i][1];
        } else {
            merged[nm][0] = sp[i][0]; merged[nm][1] = sp[i][1];
            nm++;
        }
    }
    size_t ncp = cp_len(s);
    bool *del = calloc(ncp + 1, sizeof(bool));
    for (size_t i = 0; i < nm; i++) {
        long long st = merged[i][0] < 0 ? 0 : merged[i][0];
        long long en = merged[i][1] > (long long)ncp ? (long long)ncp : merged[i][1];
        for (long long k = st; k < en; k++) del[k] = true;
    }
    char *out = malloc(strlen(s) + 1);
    size_t bi = 0, ci = 0, oi = 0;
    while (s[bi]) {
        unsigned char ch = (unsigned char)s[bi];
        size_t adv = 1;
        while (s[bi + adv] && ((unsigned char)s[bi + adv] & 0xC0) == 0x80) adv++;
        if (!del[ci]) { memcpy(out + oi, s + bi, adv); oi += adv; }
        bi += adv;
        ci++;
    }
    out[oi] = '\0';
    free(sp); free(merged); free(del);
    return out;
}

/* PoP: gwb_append_text @ gateway/platforms/base.py:_append_text */
char *gwb_append_text(const char *existing, const char *new_text) {
    if (existing && existing[0] != '\0') {
        /* f"{existing}\n{new}" — None stringifies to "None". */
        const char *nv = new_text ? new_text : "None";
        size_t cap = strlen(existing) + 1 + strlen(nv) + 1;
        char *out = malloc(cap);
        if (out) snprintf(out, cap, "%s\n%s", existing, nv);
        return out;
    }
    return new_text ? strdup(new_text) : NULL;
}

/* ── send-error classification ── */

/* PoP: gwb_any_in @ gateway/platforms/base.py:_any_in */
bool gwb_any_in(const char *blob, const char *const *needles, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (str_contains(blob, needles[i])) return true;
    return false;
}

/* _error_blob is claimed elsewhere (base_platform_error_blob); static
 * re-derivation here keeps this port self-contained (no symbol duplication). */
static char *error_blob(const json_t *exc, const char *error_text) {
    char *parts[3];
    size_t np = 0;
    if (error_text && error_text[0] != '\0') parts[np++] = strdup(error_text);
    if (exc && exc->type != JSON_NULL) {
        const json_t *es = json_obj_get(exc, "str");
        const json_t *ec = json_obj_get(exc, "class");
        char *s1 = es ? jpystr(es) : strdup("");
        char *s2 = ec ? jpystr(ec) : strdup("");
        if (s1[0]) parts[np++] = s1; else free(s1);
        if (s2[0]) parts[np++] = s2; else free(s2);
    }
    size_t cap = 1;
    for (size_t i = 0; i < np; i++) cap += strlen(parts[i]) + 1;
    char *blob = malloc(cap);
    blob[0] = '\0';
    for (size_t i = 0; i < np; i++) {
        if (i) strcat(blob, " ");
        strcat(blob, parts[i]);
        free(parts[i]);
    }
    char *low = str_lower(blob);
    free(blob);
    return low;
}

/* PoP: gwb_classify_send_error @ gateway/platforms/base.py:classify_send_error */
const char *gwb_classify_send_error(const json_t *exc, const char *error_text) {
    char *b = error_blob(exc, error_text);
    static const char *const too_long[] = {"message_too_long", "too long", "message is too long"};
    static const char *const bad_fmt[] = {"can't parse entities", "cant parse entities",
                                          "can't find end", "unsupported start tag"};
    static const char *const forbidden[] = {"forbidden", "bot was blocked", "blocked by the user",
                                            "user is deactivated", "not enough rights",
                                            "have no rights", "not a member"};
    static const char *const not_found[] = {"chat not found", "message to edit not found",
                                            "message to reply not found", "thread not found",
                                            "topic_deleted", "message_id_invalid"};
    static const char *const rate[] = {"flood", "too many requests", "retry after", "rate limit"};
    static const char *const transient[] = {"connecterror", "connectionerror", "connectionreset",
                                            "connectionrefused", "connecttimeout", "network",
                                            "broken pipe", "remotedisconnected", "eoferror",
                                            "connecttimeout"};
    const char *kind = "unknown";
    if (gwb_any_in(b, too_long, 3)) kind = "too_long";
    else if (gwb_any_in(b, bad_fmt, 4)
             || (str_contains(b, "entity") && str_contains(b, "parse"))
             || (str_contains(b, "bad request") && str_contains(b, "entit"))) kind = "bad_format";
    else if (gwb_any_in(b, forbidden, 7)) kind = "forbidden";
    else if (gwb_any_in(b, not_found, 6)) kind = "not_found";
    else if (gwb_any_in(b, rate, 4)) kind = "rate_limited";
    else if (gwb_any_in(b, transient, 10)) kind = "transient";
    free(b);
    return kind;
}

/* PoP: gwb_split_post_delivery_entry @ gateway/platforms/base.py:_split_post_delivery_entry */
json_t *gwb_split_post_delivery_entry(const json_t *entry) {
    json_t *out = json_array();
    if (entry && entry->type == JSON_ARRAY && entry->c.count == 2) {
        json_append(out, json_copy(json_get(entry, 0)));
        json_append(out, json_copy(json_get(entry, 1)));
    } else {
        json_append(out, json_null());
        json_append(out, entry ? json_copy(entry) : json_null());
    }
    return out;
}
/* ── extract_images ── */

/* _IMAGE_URL_MARKERS (module constant). */
static const char *const IMAGE_URL_MARKERS[] = {
    ".png", ".jpg", ".jpeg", ".gif", ".webp", "fal.media", "fal-cdn", "replicate.delivery"
};

typedef struct { size_t start, end; char *url; char *alt; } img_match_t;

/* MD_IMAGE_TAG_PATTERN: !\[([^\]]*)\]\((https?://[^\s\)]+)\) */
static bool md_image_match(const char *s, size_t b, size_t pos, img_match_t *m) {
    if (strncmp(s + pos, "![", 2) != 0) return false;
    size_t j = pos + 2;
    while (j < b && s[j] != ']') j++;
    if (j >= b) return false;
    size_t alt_end = j;
    if (j + 1 >= b || s[j + 1] != '(') return false;
    j += 2;
    size_t url_start = j;
    if (!(strncmp(s + j, "http://", 7) == 0 || strncmp(s + j, "https://", 8) == 0))
        return false;
    while (j < b && !is_py_ws(s[j]) && s[j] != ')') j++;
    if (j == url_start || j >= b || s[j] != ')') return false;
    m->start = pos; m->end = j + 1;
    m->url = strndup(s + url_start, j - url_start);
    m->alt = strndup(s + pos + 2, alt_end - (pos + 2));
    return true;
}

/* HTML_IMAGE_TAG_PATTERN: <img, \s+, src=, optional quote, https?:// URL
 * (no whitespace/quote/angle), optional quote, \s*, optional slash, >,
 * \s*, optional </img>. */
static bool html_image_match(const char *s, size_t b, size_t pos, img_match_t *m) {
    if (strncmp(s + pos, "<img", 4) != 0) return false;
    size_t j = pos + 4;
    size_t ws_start = j;
    while (j < b && is_py_ws(s[j])) j++;
    if (j == ws_start) return false;  /* \s+ */
    if (strncmp(s + j, "src=", 4) != 0) return false;
    j += 4;
    char quote = 0;
    if (j < b && (s[j] == '"' || s[j] == '\'')) { quote = s[j]; j++; }
    size_t url_start = j;
    if (!(strncmp(s + j, "http://", 7) == 0 || strncmp(s + j, "https://", 8) == 0))
        return false;
    while (j < b && !is_py_ws(s[j]) && s[j] != '"' && s[j] != '\'' && s[j] != '<' && s[j] != '>')
        j++;
    if (j == url_start) return false;
    size_t url_end = j;
    if (quote && j < b && s[j] == quote) j++;
    while (j < b && is_py_ws(s[j])) j++;
    if (j < b && s[j] == '/') j++;
    if (j >= b || s[j] != '>') return false;
    j++;
    while (j < b && is_py_ws(s[j])) j++;
    if (strncmp(s + j, "</img>", 6) == 0) j += 6;
    m->start = pos; m->end = j;
    m->url = strndup(s + url_start, url_end - url_start);
    m->alt = strdup("");
    return true;
}

static bool url_looks_like_image(const char *url) {
    char *low = str_lower(url);
    bool hit = false;
    for (size_t i = 0; i < sizeof(IMAGE_URL_MARKERS) / sizeof(IMAGE_URL_MARKERS[0]); i++) {
        if (str_endswith(low, IMAGE_URL_MARKERS[i]) || str_contains(low, IMAGE_URL_MARKERS[i])) {
            hit = true; break;
        }
    }
    free(low);
    return hit;
}

/* Collapse \n{3,} -> \n\n (codepoint-safe: \n is ASCII). */
static char *collapse_blank_lines(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    size_t oi = 0, i = 0;
    while (i < n) {
        if (s[i] == '\n') {
            size_t j = i;
            while (j < n && s[j] == '\n') j++;
            size_t run = j - i;
            size_t keep = run >= 3 ? 2 : run;
            for (size_t k = 0; k < keep; k++) out[oi++] = '\n';
            i = j;
        } else {
            out[oi++] = s[i++];
        }
    }
    out[oi] = '\0';
    return out;
}

/* PoP: gwb_extract_images @ gateway/platforms/base.py:extract_images */
json_t *gwb_extract_images(const char *content) {
    const char *s = content ? content : "";
    size_t b = strlen(s);
    img_match_t *md = NULL, *html = NULL;
    size_t nmd = 0, nhtml = 0, cmd = 0, chml = 0;
    /* finditer: non-overlapping, left to right. */
    for (size_t pos = 0; pos < b;) {
        img_match_t m;
        if (md_image_match(s, b, pos, &m)) {
            if (nmd == cmd) { cmd = cmd ? cmd * 2 : 8; md = realloc(md, cmd * sizeof(*md)); }
            md[nmd++] = m;
            pos = m.end;
        } else pos++;
    }
    for (size_t pos = 0; pos < b;) {
        img_match_t m;
        if (html_image_match(s, b, pos, &m)) {
            if (nhtml == chml) { chml = chml ? chml * 2 : 8; html = realloc(html, chml * sizeof(*html)); }
            html[nhtml++] = m;
            pos = m.end;
        } else pos++;
    }
    json_t *images = json_array();
    for (size_t i = 0; i < nmd; i++) {
        if (!url_looks_like_image(md[i].url)) continue;
        json_t *pair = json_array();
        json_append(pair, json_string(md[i].url));
        json_append(pair, json_string(md[i].alt));
        json_append(images, pair);
    }
    for (size_t i = 0; i < nhtml; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(html[i].url));
        json_append(pair, json_string(""));
        json_append(images, pair);
    }
    json_t *out = json_object();
    if (json_len(images) == 0) {
        json_set(out, "images", images);
        json_set(out, "cleaned", json_string(s));
        for (size_t i = 0; i < nmd; i++) { free(md[i].url); free(md[i].alt); }
        for (size_t i = 0; i < nhtml; i++) { free(html[i].url); free(html[i].alt); }
        free(md); free(html);
        return out;
    }
    /* extracted_urls set: remove only tags whose url was extracted. */
    char *cleaned = strdup(s);
    for (int pass = 0; pass < 2; pass++) {
        img_match_t *ms = pass == 0 ? md : html;
        size_t n = pass == 0 ? nmd : nhtml;
        /* Re-find matches on the CURRENT cleaned text (re.sub semantics). */
        size_t cb = strlen(cleaned);
        char *next = malloc(cb + 1);
        size_t oi = 0, pos = 0;
        while (pos < cb) {
            img_match_t m;
            bool ok = pass == 0 ? md_image_match(cleaned, cb, pos, &m)
                                : html_image_match(cleaned, cb, pos, &m);
            if (ok) {
                bool extracted = false;
                for (size_t i = 0; i < json_len(images); i++) {
                    const json_t *pair = json_get(images, i);
                    if (strcmp(jstr(json_get(pair, 0)), m.url) == 0) { extracted = true; break; }
                }
                if (!extracted) {
                    memcpy(next + oi, cleaned + m.start, m.end - m.start);
                    oi += m.end - m.start;
                }
                pos = m.end;
                free(m.url); free(m.alt);
            } else {
                next[oi++] = cleaned[pos++];
            }
        }
        next[oi] = '\0';
        free(cleaned);
        cleaned = next;
        (void)ms; (void)n;
    }
    char *collapsed = collapse_blank_lines(cleaned);
    free(cleaned);
    char *stripped = str_strip(collapsed);
    free(collapsed);
    json_set(out, "images", images);
    json_set(out, "cleaned", json_string(stripped));
    free(stripped);
    for (size_t i = 0; i < nmd; i++) { free(md[i].url); free(md[i].alt); }
    for (size_t i = 0; i < nhtml; i++) { free(html[i].url); free(html[i].alt); }
    free(md); free(html);
    return out;
}

/* ── float coercion / text batch config ── */

/* Python float(x) for the JSON scalar subset. Returns 0 ok / 1 TypeError /
 * 2 ValueError via *exc. bool is an int subclass: float(True) == 1.0. */
static double py_float(const json_t *v, int *exc) {
    *exc = 0;
    if (!v || v->type == JSON_NULL) { *exc = 1; return 0.0; }
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type != JSON_STRING) { *exc = 1; return 0.0; }
    char *t = str_strip(v->str_val);
    if (t[0] == '\0') { free(t); *exc = 2; return 0.0; }
    /* Python float() rejects hex floats; strtod accepts them. */
    const char *p = t;
    if (*p == '+' || *p == '-') p++;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { free(t); *exc = 2; return 0.0; }
    char *endp = NULL;
    double d = strtod(t, &endp);
    if (endp == t || *endp != '\0') { free(t); *exc = 2; return 0.0; }
    free(t);
    return d;
}

/* Python int(x) truncating toward zero; exc kinds as py_float. */
static long long py_int(const json_t *v, int *exc) {
    *exc = 0;
    if (!v || v->type == JSON_NULL) { *exc = 1; return 0; }
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type != JSON_STRING) { *exc = 1; return 0; }
    char *t = str_strip(v->str_val);
    if (t[0] == '\0') { free(t); *exc = 2; return 0; }
    const char *p = t;
    if (*p == '+' || *p == '-') p++;
    if (*p == '\0') { free(t); *exc = 2; return 0; }
    for (const char *q = p; *q; q++)
        if (!isdigit((unsigned char)*q)) { free(t); *exc = 2; return 0; }
    long long r = strtoll(t, NULL, 10);
    free(t);
    return r;
}

/* PoP: gwb_coerce_float_extra @ gateway/platforms/base.py:_coerce_float_extra */
double gwb_coerce_float_extra(const json_t *value, double defval, double min_value,
                              bool has_max, double max_value, const char *key,
                              const gwb_seams_t *s) {
    int exc = 0;
    double parsed = py_float(value, &exc);
    if (exc) parsed = defval;
    if (!isfinite(parsed) || parsed < 0) parsed = defval;
    if (parsed < min_value) parsed = min_value;
    if (has_max && parsed > max_value) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s=%.17g exceeds the %.17g ceiling; clamped",
                 key ? key : "", parsed, max_value);
        seam_log(s, "warning", msg);
        parsed = max_value;
    }
    return parsed;
}

/* PoP: gwb_configure_text_batch_delays @ gateway/platforms/base.py:_configure_text_batch_delays */
void gwb_configure_text_batch_delays(json_t *st, const gwb_seams_t *s) {
    const json_t *config = jgetattr(st, "config");
    const json_t *extra = jgetattr(config, "extra");
    if (!jtruth(extra)) extra = NULL;
    double delay = gwb_coerce_float_extra(json_obj_get(extra, "text_batch_delay_seconds"),
                                          0.3, 0.0, true, 2.0, "text_batch_delay_seconds", s);
    double split = gwb_coerce_float_extra(json_obj_get(extra, "text_batch_split_delay_seconds"),
                                          1.0, delay, true, 4.0, "text_batch_split_delay_seconds", s);
    json_set(st, "_text_batch_delay_seconds", json_number(delay));
    json_set(st, "_text_batch_split_delay_seconds", json_number(split));
}

/* PoP: gwb_text_batch_delay_for @ gateway/platforms/base.py:_text_batch_delay_for */
double gwb_text_batch_delay_for(const json_t *st, const json_t *pending) {
    double last_len = 0;
    if (pending) {
        const json_t *lc = jgetattr(pending, "_last_chunk_len");
        if (lc && lc->type == JSON_NUMBER) last_len = lc->num_val;
    }
    double threshold = json_get_num(st, "_SPLIT_THRESHOLD", 4000);
    if (last_len >= threshold)
        return json_get_num(st, "_text_batch_split_delay_seconds", 0.0);
    return json_get_num(st, "_text_batch_delay_seconds", 0.0);
}

/* PoP: gwb_owner_transport_profile @ gateway/platforms/base.py:_owner_transport_profile */
const char *gwb_owner_transport_profile(const json_t *st) {
    const json_t *owner = jgetattr(st, "_owner_profile");
    if (!owner || owner->type != JSON_STRING) return NULL;
    char *t = str_strip(owner->str_val);
    bool ok = t[0] != '\0';
    free(t);
    return ok ? owner->str_val : NULL;
}

/* ── send-result predicates ── */

/* PoP: gwb_is_rate_limited_error @ gateway/platforms/base.py:_is_rate_limited_error */
bool gwb_is_rate_limited_error(const char *error) {
    return strcmp(gwb_classify_send_error(NULL, error ? error : ""), "rate_limited") == 0;
}

/* PoP: gwb_is_timeout_error @ gateway/platforms/base.py:_is_timeout_error */
bool gwb_is_timeout_error(const char *error) {
    char *low = str_lower(error ? error : "");
    bool r = str_contains(low, "timed out") || str_contains(low, "readtimeout")
             || str_contains(low, "writetimeout");
    free(low);
    return r;
}

/* PoP: gwb_send_retry_is_final @ gateway/platforms/base.py:_send_retry_is_final */
bool gwb_send_retry_is_final(const json_t *result) {
    (void)result;
    return false;  /* live default: never final */
}

/* PoP: gwb_is_partial_delivery @ gateway/platforms/base.py:_is_partial_delivery */
bool gwb_is_partial_delivery(const json_t *result) {
    const json_t *raw = jgetattr(result, "raw_response");
    if (!raw || raw->type != JSON_OBJECT) return false;
    return jtruth(json_obj_get(raw, "partial_overflow"));
}

/* PoP: gwb_merge_caption @ gateway/platforms/base.py:_merge_caption */
char *gwb_merge_caption(const char *existing_text, const char *new_text) {
    const char *nt = new_text ? new_text : "";
    if (!existing_text || existing_text[0] == '\0') return strdup(nt);
    char *new_strip = str_strip(nt);
    /* existing.split("\n\n") — each caption stripped, exact match. */
    bool found = false;
    const char *p = existing_text;
    while (p && !found) {
        const char *sep = strstr(p, "\n\n");
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        char *cap = strndup(p, len);
        char *caps = str_strip(cap);
        if (strcmp(caps, new_strip) == 0) found = true;
        free(cap); free(caps);
        p = sep ? sep + 2 : NULL;
    }
    free(new_strip);
    if (found) return strdup(existing_text);
    size_t cap = strlen(existing_text) + 2 + strlen(nt) + 1;
    char *joined = malloc(cap);
    snprintf(joined, cap, "%s\n\n%s", existing_text, nt);
    char *out = str_strip(joined);
    free(joined);
    return out;
}

/* Python str.title() for ASCII names (uppercase after any non-cased char). */
static char *str_title(const char *s) {
    char *out = strdup(s ? s : "");
    bool prev_cased = false;
    for (char *p = out; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool cased = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (cased) {
            if (!prev_cased && c >= 'a' && c <= 'z') *p = (char)(c - 32);
            else if (prev_cased && c >= 'A' && c <= 'Z') *p = (char)(c + 32);
        }
        prev_cased = cased;
    }
    return out;
}

/* adapter name property: platform.value.title(). */
static char *adapter_name(const json_t *st) {
    char *pn = platform_name(jgetattr(st, "platform"));
    char *t = str_title(pn);
    free(pn);
    return t;
}

/* PoP: gwb_requeue_backoff_delay @ gateway/platforms/base.py:_requeue_backoff_delay */
double gwb_requeue_backoff_delay(json_t *st, const char *session_key,
                                 const json_t *pending_event, const json_t *dispatched_event,
                                 const gwb_seams_t *s) {
    const json_t *pid = jgetattr(pending_event, "message_id");
    const json_t *did = jgetattr(dispatched_event, "message_id");
    /* message_id equality (None == None); JSON null/absent both model None. */
    bool id_eq = (!pid || pid->type == JSON_NULL) ? (!did || did->type == JSON_NULL)
                 : (did && pid->type == did->type
                    && ((pid->type == JSON_STRING && strcmp(pid->str_val, did->str_val) == 0)
                        || (pid->type == JSON_NUMBER && pid->num_val == did->num_val)));
    bool same = id_eq && (jtruth(pid)
                          || json_get_num(pending_event, "timestamp", 0)
                             == json_get_num(dispatched_event, "timestamp", 0));
    json_t *counts = json_obj_get(st, "_requeue_counts");
    if (!counts) { counts = json_object(); json_set(st, "_requeue_counts", counts); }
    if (!same) {
        json_obj_del(counts, session_key);
        return 0.0;
    }
    long long attempts = (long long)json_get_num(counts, session_key, 0);
    json_set(counts, session_key, json_number((double)(attempts + 1)));
    if (attempts == 0) return 0.0;
    /* jittered_backoff(attempts, base_delay=0.25, max_delay=1.0, jitter_ratio=0):
     * min(0.25 * 2^(attempts-1), 1.0); uniform(0, 0) == 0 exactly. */
    double delay;
    if (attempts - 1 >= 63) delay = 1.0;
    else {
        delay = 0.25 * pow(2.0, (double)(attempts - 1));
        if (delay > 1.0) delay = 1.0;
    }
    char *name = adapter_name(st);
    char msg[512];
    snprintf(msg, sizeof(msg),
             "[%s] Handler re-queued a pending event for %s again (session busy elsewhere); "
             "backing off %.2fs", name, session_key, delay);
    seam_log(s, attempts == 1 ? "info" : "debug", msg);
    free(name);
    return delay;
}

/* PoP: gwb_get_human_delay @ gateway/platforms/base.py:_get_human_delay */
double gwb_get_human_delay(const json_t *st, gwb_uniform_fn uniform, void *ctx) {
    const json_t *bounds = jgetattr(st, "_human_delay_range_ms");
    if (!jtruth(bounds)) return 0.0;
    double lo = json_get(bounds, 0)->num_val;
    double hi = json_get(bounds, 1)->num_val;
    return uniform(ctx, lo / 1000.0, hi / 1000.0);
}

/* PoP: gwb_wants_auto_tts @ gateway/platforms/base.py:_wants_auto_tts */
bool gwb_wants_auto_tts(const json_t *st, const json_t *event, const char *session_key,
                        const json_t *interrupt_event, const char *text_content,
                        const json_t *media_files, const gwb_seams_t *s) {
    const json_t *generation = jgetattr(interrupt_event, "_hermes_run_generation");
    const json_t *source = jgetattr(event, "source");
    json_t *a = args0();
    asets(a, "chat_id", jstr(jgetattr(source, "chat_id")));
    json_t *r1 = gwb_call(s, "should_auto_tts", a);
    json_free(a);
    bool want = jtruth(r1);
    json_free(r1);
    if (!want) return false;
    const char *mt = jstr(jgetattr(event, "message_type"));
    if (!mt || strcmp(mt, "voice") != 0) return false;
    if (!text_content || text_content[0] == '\0') return false;
    if (jtruth(media_files)) return false;
    a = args0();
    asets(a, "session_key", session_key);
    aset(a, "generation", generation);
    aset(a, "event", event);
    json_t *r2 = gwb_call(s, "tts_turn_completed", a);
    json_free(a);
    bool completed = jtruth(r2);
    json_free(r2);
    return !completed;
}

/* PoP: gwb_shared_runtime_owns_input @ gateway/platforms/base.py:_shared_runtime_owns_input */
bool gwb_shared_runtime_owns_input(const json_t *st, const json_t *event, const gwb_seams_t *s) {
    const json_t *runner = jgetattr(st, "_message_handler_self");
    const json_t *authority = jgetattr(runner, "session_authority");
    if (!authority || authority->type == JSON_NULL) return false;
    json_t *a = args0();
    aset(a, "event", event);
    json_t *cmd = gwb_call(s, "get_command", a);
    json_free(a);
    bool is_cmd = jtruth(cmd);
    json_free(cmd);
    return !is_cmd;
}

/* PoP: gwb_clear_session_guard @ gateway/platforms/base.py:_clear_session_guard */
void gwb_clear_session_guard(json_t *st, const char *session_key) {
    json_t *active = json_obj_get(st, "_active_sessions");
    json_t *guard = json_obj_get(active, session_key);
    if (guard) json_set(guard, "set", json_bool(false));
}

/* ── warning presentation policy ── */

/* PoP: gwb_warning_notifications_enabled @ gateway/platforms/base.py:warning_notifications_enabled */
bool gwb_warning_notifications_enabled(const json_t *st, const json_t *logical_platform,
                                       const gwb_seams_t *s) {
    const json_t *plat = jtruth(logical_platform) ? logical_platform : jgetattr(st, "platform");
    json_t *a = args0();
    aset(a, "platform", plat);
    json_t *r = gwb_call(s, "warning_policy", a);
    json_free(a);
    bool enabled = jtruth(r);
    json_free(r);
    return enabled;
}

/* PoP: gwb_warning_text @ gateway/platforms/base.py:warning_text */
const char *gwb_warning_text(const json_t *st, const char *visible, const char *hidden,
                             const json_t *logical_platform, const json_t *chat_id,
                             const json_t *metadata, const gwb_seams_t *s) {
    (void)chat_id; (void)metadata;
    if (gwb_warning_notifications_enabled(st, logical_platform, s)) return visible;
    return hidden ? hidden : "";
}

/* ── exec approval helpers ── */

/* PoP: gwb_exec_approval_actions @ gateway/platforms/base.py:_exec_approval_actions */
json_t *gwb_exec_approval_actions(bool allow_permanent, bool allow_session,
                                  bool smart_denied, const gwb_seams_t *s) {
    const char *choices[4];
    size_t nc = 0;
    choices[nc++] = "once";
    if (!smart_denied && allow_session) {
        choices[nc++] = "session";
        if (allow_permanent) choices[nc++] = "always";
    }
    choices[nc++] = "deny";
    json_t *a = args0();
    json_t *labels = gwb_call(s, "ea_labels", a);
    json_free(a);
    json_t *rows = json_array();
    for (size_t i = 0; i < nc; i++) {
        const char *style = strcmp(choices[i], "once") == 0 ? "primary"
                            : strcmp(choices[i], "deny") == 0 ? "danger" : "";
        const json_t *lv = json_obj_get(labels, choices[i]);
        char *label = lv ? jpystr(lv) : strdup("None");
        json_t *row = json_array();
        json_append(row, json_string(label));
        json_append(row, json_string(choices[i]));
        json_append(row, json_string(style));
        json_append(rows, row);
        free(label);
    }
    json_free(labels);
    return rows;
}

/* PoP: gwb_exec_approval_prompt_choices @ gateway/platforms/base.py:choices */
json_t *gwb_exec_approval_prompt_choices(const json_t *actions) {
    json_t *out = json_array();
    size_t n = actions ? json_len(actions) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *row = json_get(actions, i);
        json_append(out, json_copy(json_get(row, 1)));
    }
    return out;
}

/* PoP: gwb_text_debounce_cancel_timer @ gateway/platforms/base.py:cancel_timer */
void gwb_text_debounce_cancel_timer(json_t *state, bool has_unless, int unless_id) {
    json_t *task = json_obj_get(state, "task");
    if (!task || task->type == JSON_NULL) return;
    int tid = (int)json_get_num(task, "id", -1);
    if (has_unless && tid == unless_id) return;
    if (json_get_bool(task, "done", false)) return;
    json_set(task, "cancelled", json_bool(true));
}

/* PoP: gwb_unauthorized_action_notice @ gateway/platforms/base.py:unauthorized_action_notice */
char *gwb_unauthorized_action_notice(const json_t *platform, const gwb_seams_t *s) {
    const json_t *name = platform;
    if (platform && platform->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(platform, "value");
        if (v) name = v;
    }
    char *ns = jtruth(name) ? jpystr(name) : strdup("<platform>");
    json_t *kw = json_object();
    json_set(kw, "platform", json_string(ns));
    char *out = seam_t(s, "gateway.unauthorized.action_notice", kw);
    json_free(kw);
    free(ns);
    return out;
}

/* ── inbound media cap ── */

#define GWB_DEFAULT_INBOUND_MAX (128LL * 1024 * 1024)

/* PoP: gwb_get_inbound_media_max_bytes @ gateway/platforms/base.py:get_inbound_media_max_bytes */
long long gwb_get_inbound_media_max_bytes(const gwb_seams_t *s) {
    json_t *a = args0();
    asets(a, "name", "gateway");
    json_t *section = gwb_call(s, "config_section", a);
    json_free(a);
    long long out = GWB_DEFAULT_INBOUND_MAX;
    if (gwb_raise_class(section)) {
        /* _config_section swallows load failures -> {}; the int() coercion
         * then raises KeyError, caught by _or_default. */
        json_free(section);
        return out;
    }
    const json_t *v = json_obj_get(section, "max_inbound_media_bytes");
    if (!v) { json_free(section); return out; }  /* KeyError -> default */
    int exc = 0;
    long long parsed = py_int(v, &exc);
    if (exc == 0) out = parsed;
    json_free(section);
    return out;
}

/* PoP: gwb_validate_inbound_media_size @ gateway/platforms/base.py:validate_inbound_media_size */
char *gwb_validate_inbound_media_size(long long size, const char *media_type,
                                      const json_t *max_bytes, const gwb_seams_t *s) {
    long long limit;
    if (max_bytes && max_bytes->type != JSON_NULL) limit = (long long)max_bytes->num_val;
    else limit = gwb_get_inbound_media_max_bytes(s);
    if (limit != 0 && size > limit) {
        char *msg = malloc(256);
        snprintf(msg, 256, "Inbound %s payload is too large (%lld bytes > %lld bytes)",
                 media_type ? media_type : "media", size, limit);
        return msg;
    }
    return NULL;
}

/* Python repr() of a str for the invalid-Content-Length debug log (%r). */
static char *py_repr_str(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    size_t oi = 0;
    out[oi++] = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\'') { out[oi++] = '\\'; out[oi++] = (char)c; }
        else if (c == '\n') { out[oi++] = '\\'; out[oi++] = 'n'; }
        else if (c == '\r') { out[oi++] = '\\'; out[oi++] = 'r'; }
        else if (c == '\t') { out[oi++] = '\\'; out[oi++] = 't'; }
        else out[oi++] = (char)c;
    }
    out[oi++] = '\'';
    out[oi] = '\0';
    return out;
}

/* PoP: gwb_read_httpx_body_with_limit @ gateway/platforms/base.py:read_httpx_body_with_limit */
json_t *gwb_read_httpx_body_with_limit(const json_t *headers, const json_t *chunks,
                                       const char *media_type, const gwb_seams_t *s) {
    json_t *out = json_object();
    json_t *maxv = json_number((double)gwb_get_inbound_media_max_bytes(s));
    const json_t *cl = json_obj_get(headers, "content-length");
    if (jtruth(cl)) {
        int exc = 0;
        long long declared = py_int(cl, &exc);
        if (exc == 2) {
            char *rs = jpystr(cl);
            char *repr = py_repr_str(rs);
            char msg[512];
            snprintf(msg, sizeof(msg), "Ignoring invalid Content-Length for inbound %s: %s",
                     media_type ? media_type : "media", repr);
            json_t *a = args0();
            asets(a, "msg", msg);
            json_t *r = gwb_call(s, "log_debug", a);
            json_free(a); json_free(r);
            free(rs); free(repr);
        } else if (exc == 0) {
            char *err = gwb_validate_inbound_media_size(declared, media_type, maxv, s);
            if (err) {
                json_set(out, "body", json_string(""));
                json_set(out, "error", json_string(err));
                free(err);
                json_free(maxv);
                return out;
            }
        }
    }
    size_t total = 0;
    size_t n = chunks ? json_len(chunks) : 0;
    char *body = malloc(1);
    body[0] = '\0';
    size_t body_cap = 1;
    for (size_t i = 0; i < n; i++) {
        const char *chunk = jstr(json_get(chunks, i));
        size_t clen = chunk ? strlen(chunk) : 0;
        total += clen;
        char *err = gwb_validate_inbound_media_size((long long)total, media_type, maxv, s);
        if (err) {
            json_set(out, "body", json_string(""));
            json_set(out, "error", json_string(err));
            free(err); free(body);
            json_free(maxv);
            return out;
        }
        if (total + 1 > body_cap) {
            body_cap = (total + 1) * 2;
            body = realloc(body, body_cap);
        }
        if (chunk) memcpy(body + total - clen, chunk, clen);
        body[total] = '\0';
    }
    json_set(out, "body", json_string(body));
    json_set(out, "error", json_null());
    free(body);
    json_free(maxv);
    return out;
}

/* ── is_network_accessible ── */

/* Parse an IPv4 literal (Python ipaddress: no leading zeros). */
static bool parse_ipv4(const char *s, unsigned char out[4]) {
    int octet = 0, digits = 0, n = 0;
    for (const char *p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            if (digits == 1 && octet == 0 && *p != '0') return false; /* leading zero */
            if (digits >= 1 && octet == 0 && digits == 1 && p != s) { /* handled below */ }
            octet = octet * 10 + (*p - '0');
            digits++;
            if (digits > 3 || octet > 255) return false;
        } else if (*p == '.' || *p == '\0') {
            if (digits == 0 || n >= 4) return false;
            if (digits > 1 && s[p - s - digits] == '0') return false;
            out[n++] = (unsigned char)octet;
            octet = 0; digits = 0;
            if (*p == '\0') break;
        } else {
            return false;
        }
    }
    return n == 4;
}

/* Parse an IPv6 literal into 16 bytes; handles :: and embedded IPv4. */
static bool parse_ipv6(const char *s, unsigned char out[16]) {
    unsigned char head[16], tail[16];
    size_t nh = 0, nt = 0;
    bool seen_dc = false;
    const char *p = s;
    if (p[0] == ':' && p[1] == ':') { seen_dc = true; p += 2; }
    else if (p[0] == ':') return false;
    while (*p) {
        /* group of hex digits (1-4), optionally an embedded IPv4 tail */
        const char *start = p;
        unsigned int val = 0;
        size_t nd = 0;
        while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
            char c = *p;
            unsigned int d = (c <= '9') ? (unsigned)(c - '0')
                             : (unsigned)((c | 32) - 'a' + 10);
            val = val * 16 + d;
            nd++;
            if (nd > 4) return false;
            p++;
        }
        if (nd == 0) return false;
        if (*p == '.') {
            /* embedded IPv4: must be the last 2 groups */
            unsigned char v4[4];
            if (!parse_ipv4(start, v4)) return false;
            unsigned char *dst = seen_dc ? tail : head;
            size_t *n = seen_dc ? &nt : &nh;
            if (*n + 2 > 16) return false;
            dst[(*n)++] = v4[0]; dst[(*n)++] = v4[1];
            dst[(*n)++] = v4[2]; dst[(*n)++] = v4[3];
            p = start + strlen(start);
            break;
        }
        unsigned char *dst = seen_dc ? tail : head;
        size_t *n = seen_dc ? &nt : &nh;
        if (*n + 2 > 16) return false;
        dst[(*n)++] = (unsigned char)(val >> 8);
        dst[(*n)++] = (unsigned char)(val & 0xFF);
        if (*p == ':') {
            p++;
            if (*p == ':') {
                if (seen_dc) return false;
                seen_dc = true;
                p++;
                if (*p == '\0') break;
            } else if (*p == '\0') {
                return false;  /* trailing single colon */
            }
        } else if (*p == '\0') {
            break;
        } else {
            return false;
        }
    }
    if (!seen_dc) {
        if (nh != 16) return false;
        memcpy(out, head, 16);
        return true;
    }
    if (nh + nt >= 16) return false;  /* :: must stand for >= 1 group */
    memset(out, 0, 16);
    memcpy(out, head, nh);
    memcpy(out + 16 - nt, tail, nt);
    return true;
}

static bool ip_is_loopback(const unsigned char *b, bool v6) {
    if (!v6) return b[0] == 127;
    static const unsigned char lo6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    return memcmp(b, lo6, 16) == 0;
}

/* PoP: gwb_is_network_accessible @ gateway/platforms/base.py:is_network_accessible */
bool gwb_is_network_accessible(const char *host, const gwb_seams_t *s) {
    const char *h = host ? host : "";
    unsigned char b[16];
    bool v6 = strchr(h, ':') != NULL;
    bool parsed = v6 ? parse_ipv6(h, b) : parse_ipv4(h, b);
    if (parsed) {
        bool loop = ip_is_loopback(b, v6);
        bool mapped_loop = false;
        if (v6) {
            static const unsigned char pfx[12] = {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};
            if (memcmp(b, pfx, 12) == 0)
                mapped_loop = ip_is_loopback(b + 12, false);
        }
        return !(loop || mapped_loop);
    }
    /* hostname: resolve; DNS failure fails closed (True). */
    json_t *a = args0();
    asets(a, "host", h);
    json_t *r = gwb_call(s, "getaddrinfo", a);
    json_free(a);
    if (gwb_raise_class(r)) { json_free(r); return true; }
    bool any_non_loop = false;
    size_t n = r ? json_len(r) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *addr = jstr(json_get(r, i));
        if (!addr) continue;
        bool av6 = strchr(addr, ':') != NULL;
        unsigned char ab[16];
        if (av6 ? parse_ipv6(addr, ab) : parse_ipv4(addr, ab)) {
            if (!ip_is_loopback(ab, av6)) { any_non_loop = true; break; }
        }
    }
    json_free(r);
    return any_non_loop;
}
/* ── proxy family ── */

/* PoP: gwb_detect_macos_system_proxy @ gateway/platforms/base.py:_detect_macos_system_proxy */
char *gwb_detect_macos_system_proxy(json_t *cache, const gwb_seams_t *s) {
    json_t *a = args0();
    json_t *rd = gwb_call(s, "is_darwin", a);
    json_free(a);
    bool darwin = jtruth(rd);
    json_free(rd);
    if (!darwin) return NULL;
    a = args0();
    json_t *rn = gwb_call(s, "monotonic", a);
    json_free(a);
    double now = rn && rn->type == JSON_NUMBER ? rn->num_val : 0.0;
    json_free(rn);
    if (cache) {
        double ts = json_get_num(cache, "ts", 0.0);
        if (now - ts < 60.0) {
            const json_t *v = json_obj_get(cache, "value");
            return jstr(v) ? strdup(jstr(v)) : NULL;
        }
    }
    a = args0();
    json_t *out = gwb_call(s, "scutil", a);
    json_free(a);
    if (gwb_raise_class(out)) {
        json_free(out);
        if (cache) {
            json_set(cache, "ts", json_number(now));
            json_set(cache, "value", json_null());
        }
        return NULL;
    }
    /* props: line.strip().partition(" : ") with sep; key/val stripped. */
    const char *text = jstr(out);
    json_t *props = json_object();
    if (text) {
        const char *p = text;
        while (*p) {
            const char *nl = strchr(p, '\n');
            size_t len = nl ? (size_t)(nl - p) : strlen(p);
            char *line = strndup(p, len);
            char *ls = str_strip(line);
            free(line);
            char *sep = strstr(ls, " : ");
            if (sep) {
                *sep = '\0';
                char *key = str_strip(ls);
                char *val = str_strip(sep + 3);
                json_set(props, key, json_string(val));
                free(key); free(val);
            }
            free(ls);
            p = nl ? nl + 1 : p + len;
        }
    }
    static const char *const triples[2][3] = {
        {"HTTPSEnable", "HTTPSProxy", "HTTPSPort"},
        {"HTTPEnable", "HTTPProxy", "HTTPPort"},
    };
    char *resolved = NULL;
    for (int i = 0; i < 2 && !resolved; i++) {
        const json_t *en = json_obj_get(props, triples[i][0]);
        const json_t *ho = json_obj_get(props, triples[i][1]);
        const json_t *po = json_obj_get(props, triples[i][2]);
        if (jstr(en) && strcmp(jstr(en), "1") == 0 && jtruth(ho) && jtruth(po)) {
            char *hs = jpystr(ho);
            char *ps = jpystr(po);
            size_t cap = strlen(hs) + strlen(ps) + 16;
            resolved = malloc(cap);
            snprintf(resolved, cap, "http://%s:%s", hs, ps);
            free(hs); free(ps);
        }
    }
    json_free(props);
    json_free(out);
    if (cache) {
        json_set(cache, "ts", json_number(now));
        json_set(cache, "value", resolved ? json_string(resolved) : json_null());
    }
    return resolved;
}

/* PoP: gwb_reset_macos_proxy_cache @ gateway/platforms/base.py:reset_macos_proxy_cache */
void gwb_reset_macos_proxy_cache(json_t **cache) {
    if (cache && *cache) {
        json_free(*cache);
        *cache = NULL;
    }
}

/* PoP: gwb_resolve_proxy_url @ gateway/platforms/base.py:resolve_proxy_url */
char *gwb_resolve_proxy_url(const char *platform_env_var, const json_t *target_hosts,
                            const char *configured, const gwb_seams_t *s) {
    char *value = strdup("");
    if (platform_env_var && platform_env_var[0] != '\0') {
        json_t *a = args0();
        asets(a, "name", platform_env_var);
        asets(a, "default", "");
        json_t *r = gwb_call(s, "scoped_secret", a);
        json_free(a);
        free(value);
        char *raw = jtruth(r) ? jpystr(r) : strdup("");
        value = str_strip(raw);
        free(raw);
        json_free(r);
    }
    if (value[0] == '\0') {
        free(value);
        char *raw = configured && configured[0] != '\0' ? strdup(configured) : strdup("");
        value = str_strip(raw);
        free(raw);
    }
    if (value[0] == '\0') {
        json_t *a = args0();
        json_t *rt = gwb_call(s, "trust_env", a);
        json_free(a);
        bool trust = jtruth(rt);
        json_free(rt);
        if (!trust) { free(value); return NULL; }
        a = args0();
        json_t *re = gwb_call(s, "first_proxy_env", a);
        json_free(a);
        free(value);
        value = jstr(re) ? strdup(jstr(re)) : strdup("");
        json_free(re);
    }
    /* normalize_proxy_url(value or _detect_macos_system_proxy()) */
    char *norm_in = value;
    bool macos_called = false;
    if (value[0] == '\0') {
        json_t *a = args0();
        json_t *rm = gwb_call(s, "macos_proxy", a);
        json_free(a);
        norm_in = jstr(rm) ? strdup(jstr(rm)) : strdup("");
        macos_called = true;
        json_free(rm);
    }
    json_t *a = args0();
    asets(a, "value", norm_in);
    json_t *rp = gwb_call(s, "normalize_proxy", a);
    json_free(a);
    if (macos_called) free(norm_in);
    free(value);
    char *proxy = jstr(rp) ? strdup(jstr(rp)) : NULL;
    json_free(rp);
    if (proxy && proxy[0] != '\0') {
        a = args0();
        aset(a, "target_hosts", target_hosts);
        json_t *rb = gwb_call(s, "bypass", a);
        json_free(a);
        bool bypassed = jtruth(rb);
        json_free(rb);
        if (bypassed) { free(proxy); return NULL; }
    }
    return proxy;
}

/* PoP: gwb_config_section @ gateway/platforms/base.py:_config_section */
json_t *gwb_config_section(const char *name, const gwb_seams_t *s) {
    json_t *a = args0();
    json_t *cfg = gwb_call(s, "load_config", a);
    json_free(a);
    if (gwb_raise_class(cfg) || !cfg || cfg->type != JSON_OBJECT) {
        json_free(cfg);
        return json_object();
    }
    const json_t *section = json_obj_get(cfg, name);
    json_t *out = (section && section->type == JSON_OBJECT) ? json_copy(section) : json_object();
    json_free(cfg);
    return out;
}

/* PoP: gwb_proxy_kwargs_for_aiohttp @ gateway/platforms/base.py:proxy_kwargs_for_aiohttp */
json_t *gwb_proxy_kwargs_for_aiohttp(const char *proxy_url, const gwb_seams_t *s) {
    json_t *out = json_object();
    json_t *session = json_object(), *request = json_object();
    if (!proxy_url || proxy_url[0] == '\0') {
        json_set(out, "session", session);
        json_set(out, "request", request);
        return out;
    }
    json_t *a = args0();
    asets(a, "proxy_url", proxy_url);
    json_t *connector = gwb_call(s, "socks_connector", a);
    json_free(a);
    if (connector && connector->type != JSON_NULL && !gwb_raise_class(connector)) {
        json_set(session, "connector", connector);
    } else {
        json_free(connector);
        char *low = str_lower(proxy_url);
        if (!str_startswith(low, "socks"))
            json_set(request, "proxy", json_string(proxy_url));
        free(low);
    }
    json_set(out, "session", session);
    json_set(out, "request", request);
    return out;
}

/* PoP: gwb_aiohttp_socks_connector @ gateway/platforms/base.py:_aiohttp_socks_connector */
json_t *gwb_aiohttp_socks_connector(const char *proxy_url, const gwb_seams_t *s) {
    json_t *a = args0();
    asets(a, "proxy_url", proxy_url);
    asetb(a, "rdns", true);
    json_t *r = gwb_call(s, "import_connector", a);
    json_free(a);
    if (gwb_caught(r, "ImportError")) {
        json_free(r);
        char *low = str_lower(proxy_url);
        if (str_startswith(low, "socks")) {
            json_t *aa = args0();
            asets(aa, "url", proxy_url);
            json_t *safe = gwb_call(s, "safe_proxy_url", aa);
            json_free(aa);
            const char *ss = jstr(safe) ? jstr(safe) : proxy_url;
            char msg[1024];
            snprintf(msg, sizeof(msg),
                     "aiohttp_socks not installed \xe2\x80\x94 SOCKS proxy %s ignored. "
                     "Use an HTTP proxy instead.", ss);
            seam_log(s, "warning", msg);
            json_free(safe);
        }
        free(low);
        return NULL;
    }
    return r;
}

/* PoP: gwb_should_bypass_proxy @ gateway/platforms/base.py:should_bypass_proxy */
bool gwb_should_bypass_proxy(const json_t *target_hosts, const gwb_seams_t *s) {
    json_t *a = args0();
    aset(a, "target_hosts", target_hosts);
    json_t *r = gwb_call(s, "bypass_matcher", a);
    json_free(a);
    bool out = jtruth(r);
    json_free(r);
    return out;
}

/* PoP: gwb_is_host_excluded_by_no_proxy @ gateway/platforms/base.py:is_host_excluded_by_no_proxy */
bool gwb_is_host_excluded_by_no_proxy(const char *hostname, const json_t *no_proxy_value,
                                      const gwb_seams_t *s) {
    json_t *a = args0();
    asets(a, "target_hosts", hostname);
    aset(a, "no_proxy_value", no_proxy_value);
    json_t *r = gwb_call(s, "bypass_matcher", a);
    json_free(a);
    bool out = jtruth(r);
    json_free(r);
    return out;
}

/* ── trivial live defaults ── */

/* PoP: gwb_format_message @ gateway/platforms/base.py:format_message */
char *gwb_format_message(const char *content) {
    return strdup(content ? content : "");
}

/* PoP: gwb_message_len_fn @ gateway/platforms/base.py:message_len_fn */
int gwb_message_len_fn(const char *s) {
    return (int)cp_len(s ? s : "");
}

/* PoP: gwb_supports_draft_streaming @ gateway/platforms/base.py:supports_draft_streaming */
bool gwb_supports_draft_streaming(const json_t *chat_type, const json_t *metadata,
                                  const json_t *chat_id) {
    (void)chat_type; (void)metadata; (void)chat_id;
    return false;
}

/* PoP: gwb_send_draft @ gateway/platforms/base.py:send_draft */
char *gwb_send_draft(const char *adapter_class_name) {
    const char *n = adapter_class_name ? adapter_class_name : "BasePlatformAdapter";
    size_t cap = strlen(n) + 64;
    char *out = malloc(cap);
    snprintf(out, cap, "%s does not implement send_draft", n);
    return out;
}

/* DEGRADED_STATUS_MESSAGE class constant. */
#define GWB_DEGRADED_MSG "connected but not yet confirmed active; recovering in background"

/* PoP: gwb_mark_degraded @ gateway/platforms/base.py:_mark_degraded */
void gwb_mark_degraded(json_t *st, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "context", "connected_degraded");
    asets(a, "platform_state", "retrying");
    json_set(a, "error_code", json_null());
    asets(a, "error_message", GWB_DEGRADED_MSG);
    json_t *r = gwb_call(s, "write_status", a);
    json_free(a);
    json_free(r);
}

/* PoP: gwb_lazy_attr @ gateway/platforms/base.py:_lazy_attr */
json_t *gwb_lazy_attr(json_t *st, const char *name, const gwb_seams_t *s) {
    const json_t *value = json_obj_get(st, name);
    if (!value || value->type == JSON_NULL) {
        json_t *a = args0();
        asets(a, "name", name);
        json_t *made = gwb_call(s, "factory", a);
        json_free(a);
        json_set(st, name, made ? made : json_null());
        value = json_obj_get(st, name);
    }
    return value ? json_copy(value) : json_null();
}

/* PoP: gwb_accepts_kwarg @ gateway/platforms/base.py:_accepts_kwarg */
bool gwb_accepts_kwarg(const json_t *params, const char *name, bool var_kw, bool unknown) {
    if (!params || params->type != JSON_ARRAY) return unknown;
    size_t n = json_len(params);
    bool has_var_kw = false;
    for (size_t i = 0; i < n; i++) {
        const json_t *p = json_get(params, i);
        const char *pn = jstr(json_obj_get(p, "name"));
        if (pn && strcmp(pn, name) == 0) return true;
        if (json_get_bool(p, "var_keyword", false)) has_var_kw = true;
    }
    return var_kw && has_var_kw;
}

/* PoP: gwb_prepare_tts_text @ gateway/platforms/base.py:prepare_tts_text */
char *gwb_prepare_tts_text(const char *text, const gwb_seams_t *s) {
    const char *t = text ? text : "";
    json_t *a = args0();
    asets(a, "text", t);
    json_set(a, "max_chars", json_null());
    json_t *r = gwb_call(s, "spoken", a);
    json_free(a);
    if (!gwb_raise_class(r)) {
        char *out = jstr(r) ? strdup(jstr(r)) : strdup("");
        json_free(r);
        return out;
    }
    json_free(r);
    /* Fallback: re.sub(r'<think[\s>].*?</think>', ' ', text, DOTALL) then
     * re.sub(r'[*_`\[\]()]', '', ...).strip(). */
    size_t n = strlen(t);
    char *stage1 = malloc(n + 1);
    size_t oi = 0, i = 0;
    while (i < n) {
        if (strncmp(t + i, "<think", 6) == 0 && i + 6 < n
                && (is_py_ws(t[i + 6]) || t[i + 6] == '>')) {
            const char *close = strstr(t + i + 6, "</think>");
            if (close) {
                stage1[oi++] = ' ';
                i = (size_t)(close - t) + 8;
                continue;
            }
        }
        stage1[oi++] = t[i++];
    }
    stage1[oi] = '\0';
    char *stage2 = malloc(oi + 1);
    size_t o2 = 0;
    for (size_t k = 0; k < oi; k++) {
        char c = stage1[k];
        if (c == '*' || c == '_' || c == '`' || c == '[' || c == ']' || c == '(' || c == ')')
            continue;
        stage2[o2++] = c;
    }
    stage2[o2] = '\0';
    char *out = str_strip(stage2);
    free(stage1); free(stage2);
    return out;
}

/* PoP: gwb_media_delivery_scope @ gateway/platforms/base.py:_media_delivery_scope */
json_t *gwb_media_delivery_scope(const json_t *st, const json_t *source, const gwb_seams_t *s) {
    const json_t *runner = jgetattr(st, "gateway_runner");
    bool callable = runner && json_get_bool(runner, "scope_resolve", false);
    if (!callable || !source || source->type == JSON_NULL) {
        json_t *noop = json_object();
        json_set(noop, "noop", json_bool(true));
        return noop;
    }
    json_t *a = args0();
    aset(a, "source", source);
    json_t *r = gwb_call(s, "scope_resolve", a);
    json_free(a);
    if (gwb_raise_class(r)) {
        json_free(r);
        seam_log(s, "debug", "Failed to resolve media delivery scope");
        json_t *noop = json_object();
        json_set(noop, "noop", json_bool(true));
        return noop;
    }
    return r ? r : json_null();
}

/* PoP: gwb_pending_recovery_owners @ gateway/platforms/base.py:_pending_recovery_owners */
json_t *gwb_pending_recovery_owners(const json_t *st, const json_t *event,
                                    const char *session_key, const gwb_seams_t *s) {
    json_t *owners = json_array();
    json_append(owners, json_string("self"));
    const json_t *runner = jgetattr(st, "gateway_runner");
    if (runner && runner->type != JSON_NULL) {
        json_t *a = args0();
        aset(a, "source", jgetattr(event, "source"));
        json_t *da = gwb_call(s, "delivery_adapter", a);
        json_free(a);
        if (gwb_raise_class(da)) {
            json_free(da);
            da = NULL;
            seam_log(s, "debug", "Delivery-adapter lookup failed during pending recovery");
        }
        if (da && da->type != JSON_NULL) {
            const char *id = jstr(json_obj_get(da, "id"));
            bool is_self = id && strcmp(id, "self") == 0;
            const json_t *active = json_obj_get(da, "active_sessions");
            bool has_guard = false;
            size_t n = active ? json_len(active) : 0;
            for (size_t i = 0; i < n; i++) {
                const char *k = jstr(json_get(active, i));
                if (k && strcmp(k, session_key) == 0) { has_guard = true; break; }
            }
            if (!is_self && !has_guard)
                json_append(owners, json_string(id ? id : ""));
        }
        json_free(da);
    }
    return owners;
}

/* PoP: gwb_track_session_task @ gateway/platforms/base.py:_track_session_task */
bool gwb_track_session_task(json_t *st, const char *session_key, json_t *task) {
    json_t *tasks = json_obj_get(st, "_session_tasks");
    if (!tasks) { tasks = json_object(); json_set(st, "_session_tasks", tasks); }
    json_set(tasks, session_key, json_copy(task));
    json_t *bg = json_obj_get(st, "_background_tasks");
    if (!bg) { bg = json_array(); json_set(st, "_background_tasks", bg); }
    if (json_get_bool(task, "unhashable", false)) return false;  /* TypeError on set.add */
    int tid = (int)json_get_num(task, "id", -1);
    bool present = false;
    for (size_t i = 0; i < json_len(bg); i++)
        if ((int)json_get(bg, i)->num_val == tid) { present = true; break; }
    if (!present) json_append(bg, json_number((double)tid));
    /* two done-callbacks registered (background discard + expected-cancelled discard). */
    json_t *stored = json_obj_get(tasks, session_key);
    json_set(stored, "done_callbacks",
             json_number(json_get_num(stored, "done_callbacks", 0) + 2));
    return true;
}

/* PoP: gwb_stop_typing_quietly @ gateway/platforms/base.py:_stop_typing_quietly */
void gwb_stop_typing_quietly(const char *chat_id, const json_t *metadata, const gwb_seams_t *s) {
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    aset(a, "metadata", metadata);
    json_t *r = gwb_call(s, "stop_typing_meta", a);
    json_free(a);
    json_free(r);  /* contextlib.suppress(Exception) */
}

/* PoP: gwb_start_typing_refresh @ gateway/platforms/base.py:_start_typing_refresh */
json_t *gwb_start_typing_refresh(const json_t *st, const json_t *event,
                                 const json_t *interrupt_event, const json_t *metadata,
                                 const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    aset(a, "event", event);
    json_t *rw = gwb_call(s, "typing_wanted", a);
    json_free(a);
    bool wanted = jtruth(rw);
    json_free(rw);
    if (!wanted) return NULL;
    a = args0();
    asets(a, "name", "stop_event");
    asetb(a, "var_kw", false);
    asetb(a, "unknown", true);
    json_t *ra = gwb_call(s, "accepts_stop_event", a);
    json_free(a);
    bool accepts = jtruth(ra);
    json_free(ra);
    a = args0();
    asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
    aset(a, "metadata", metadata);
    if (accepts) aset(a, "stop_event", interrupt_event);
    json_t *task = gwb_call(s, "create_task", a);
    json_free(a);
    return task;
}

/* PoP: gwb_chain_callbacks @ gateway/platforms/base.py:_chain_callbacks */
void gwb_chain_callbacks(size_t n, const gwb_seams_t *s) {
    for (size_t i = 0; i < n; i++) {
        char name[32];
        snprintf(name, sizeof(name), "cb_%zu", i);
        json_t *a = args0();
        json_t *r = gwb_call(s, name, a);
        json_free(a);
        bool failed = gwb_raise_class(r) != NULL;
        if (!failed && r && json_get_bool(r, "awaitable", false)) {
            json_free(r);
            snprintf(name, sizeof(name), "await_%zu", i);
            a = args0();
            r = gwb_call(s, name, a);
            json_free(a);
            failed = gwb_raise_class(r) != NULL;
        }
        if (failed)
            seam_log(s, "debug", "Post-delivery callback failed");
        json_free(r);
    }
}

/* PoP: gwb_send_plain_fallback @ gateway/platforms/base.py:_send_plain_fallback */
json_t *gwb_send_plain_fallback(const json_t *st, const char *chat_id, const char *content,
                                const json_t *reply_to, const json_t *metadata, const gwb_seams_t *s) {
    char *c3500 = cp_slice(content ? content : "", 3500);
    json_t *kw = json_object();
    json_set(kw, "content", json_string(c3500));
    char *prefix = seam_t(s, "gateway.notify.plain_fallback_prefix", kw);
    json_free(kw);
    const char *chosen = gwb_warning_text(st, prefix, c3500, NULL, NULL, metadata, s);
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "content", chosen);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    json_t *r = gwb_call(s, "send", a);
    json_free(a);
    free(c3500); free(prefix);
    return r;
}

/* PoP: gwb_resume_partial_send @ gateway/platforms/base.py:_resume_partial_send */
json_t *gwb_resume_partial_send(const json_t *st, const char *chat_id, const json_t *result,
                                const json_t *reply_to, const json_t *metadata) {
    (void)st; (void)chat_id; (void)result; (void)reply_to; (void)metadata;
    return NULL;  /* live default: cannot resume */
}

/* PoP: gwb_play_tts_file @ gateway/platforms/base.py:_play_tts_file */
bool gwb_play_tts_file(const json_t *st, const json_t *event, const char *text_content,
                       const char *tts_path, bool first, const json_t *metadata,
                       const gwb_seams_t *s) {
    const char *caption = NULL;
    char *pn = platform_name(jgetattr(st, "platform"));
    if (first && strcmp(pn, "telegram") == 0 && text_content && text_content[0] != '\0'
            && cp_len(text_content) <= 1024)
        caption = text_content;
    free(pn);
    json_t *a = args0();
    asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
    asets(a, "audio_path", tts_path);
    asets(a, "caption", caption);
    aset(a, "metadata", metadata);
    json_t *result = gwb_call(s, "play_tts", a);
    json_free(a);
    a = args0();
    aset(a, "result", result);
    json_t *rr = gwb_call(s, "record_delivery", a);
    json_free(a);
    json_free(rr);
    bool ok = caption && jtruth(json_obj_get(result, "success"));
    json_free(result);
    return ok;
}

/* PoP: gwb_record_delivery_obligation @ gateway/platforms/base.py:_record_delivery_obligation */
json_t *gwb_record_delivery_obligation(const json_t *st, const json_t *event,
                                       const char *session_key, const char *text_content,
                                       const json_t *delivery_adapter, bool is_ephemeral_response,
                                       const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    aset(a, "event", event);
    asets(a, "session_key", session_key);
    asets(a, "text_content", text_content);
    aset(a, "delivery_adapter", delivery_adapter);
    asetb(a, "is_ephemeral_response", is_ephemeral_response);
    json_t *r = gwb_call(s, "record_final_obligation", a);
    json_free(a);
    return r;
}

/* PoP: gwb_finalize_delivery_obligation @ gateway/platforms/base.py:_finalize_delivery_obligation */
void gwb_finalize_delivery_obligation(const json_t *st, const char *obligation_id,
                                      const json_t *result, const json_t *event,
                                      const json_t *delivery_adapter, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "obligation_id", obligation_id);
    aset(a, "result", result);
    aset(a, "event", event);
    aset(a, "delivery_adapter", delivery_adapter);
    json_t *r = gwb_call(s, "finalize_final_obligation", a);
    json_free(a);
    json_free(r);
}

/* PoP: gwb_fire_post_delivery_callback @ gateway/platforms/base.py:_fire_post_delivery_callback */
void gwb_fire_post_delivery_callback(json_t *st, const char *session_key,
                                     const json_t *generation, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "session_key", session_key);
    aset(a, "generation", generation);
    json_t *cb = gwb_call(s, "pop_post_delivery", a);
    json_free(a);
    a = args0();
    aset(a, "callback", cb);
    asetn(a, "timeout", 30.0);
    json_t *r = gwb_call(s, "run_post_delivery", a);
    json_free(a);
    json_free(cb);
    json_free(r);
}

/* PoP: gwb_release_turn_marker @ gateway/platforms/base.py:_release_turn_marker */
void gwb_release_turn_marker(const json_t *st, json_t *event, const gwb_seams_t *s) {
    const json_t *runner = jgetattr(st, "gateway_runner");
    if (jtruth(jgetattr(event, "_turn_marker_handoff"))
            && jtruth(jgetattr(event, "_gateway_active_turn_token"))
            && runner && runner->type != JSON_NULL) {
        json_t *a = args0();
        aset(a, "event", event);
        json_t *r = gwb_call(s, "clear_durable_turn", a);
        json_free(a);
        json_free(r);
    }
    /* getattr(event, "_gateway_marker_released", asyncio.Event()).set() —
     * an absent attr creates a TRANSIENT Event (no persistent mutation). */
    json_t *marker = json_obj_get(event, "_gateway_marker_released");
    if (marker && marker->type == JSON_OBJECT)
        json_set(marker, "set", json_bool(true));
}

/* PoP: gwb_ledger_attempted_final @ gateway/platforms/base.py:ledger_attempted_final */
bool gwb_ledger_attempted_final(const json_t *st, const json_t *event, const char *session_key,
                                const char *response, const char *error, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "response", response);
    aset(a, "event", event);
    asets(a, "session_key", session_key);
    asetb(a, "is_ephemeral_response", false);
    json_t *text = gwb_call(s, "extract_text", a);
    json_free(a);
    bool has_text = jtruth(text);
    if (!has_text) { json_free(text); return false; }
    a = args0();
    aset(a, "event", event);
    asets(a, "session_key", session_key);
    aset(a, "text", text);
    asets(a, "error", error);
    json_t *r = gwb_call(s, "ledger_attempted", a);
    json_free(a);
    bool saved = jtruth(r);
    json_free(text);
    json_free(r);
    return saved;
}

/* ── identity / profile resolution ── */

/* PoP: gwb_session_key_profile @ gateway/platforms/base.py:_session_key_profile */
json_t *gwb_session_key_profile(const json_t *st, const json_t *source, const gwb_seams_t *s) {
    json_t *a = args0();
    aset(a, "source", source);
    json_t *identity = gwb_call(s, "identity_of", a);
    json_free(a);
    if (identity && identity->type != JSON_NULL) {
        const json_t *skp = json_obj_get(identity, "session_key_profile");
        json_t *out = skp ? json_copy(skp) : json_null();
        json_free(identity);
        return out;
    }
    json_free(identity);
    const json_t *candidates[2] = {
        (source && source->type != JSON_NULL) ? jgetattr(source, "profile") : NULL,
        jgetattr(st, "_owner_profile"),
    };
    for (int i = 0; i < 2; i++) {
        const json_t *c = candidates[i];
        if (c && c->type == JSON_STRING) {
            char *t = str_strip(c->str_val);
            bool ok = t[0] != '\0';
            free(t);
            if (ok) return json_copy(c);
        }
    }
    const json_t *store = jgetattr(st, "_session_store");
    bool callable = store && store->type != JSON_NULL
                    && json_get_bool(store, "has_resolver", false);
    if (!callable) return NULL;
    a = args0();
    aset(a, "source", source);
    json_t *resolved = gwb_call(s, "store_resolve", a);
    json_free(a);
    if (gwb_raise_class(resolved)) { json_free(resolved); return NULL; }
    if (resolved && resolved->type == JSON_STRING) {
        char *t = str_strip(resolved->str_val);
        bool ok = t[0] != '\0';
        free(t);
        if (ok) return resolved;
    }
    json_free(resolved);
    return NULL;
}

/* PoP: gwb_canonicalize @ gateway/platforms/base.py:_canonicalize */
json_t *gwb_canonicalize(const json_t *st, const json_t *source, const gwb_seams_t *s) {
    if (!source || source->type == JSON_NULL) return NULL;
    json_t *a = args0();
    aset(a, "source", source);
    json_t *identity = gwb_call(s, "identity_of", a);
    json_free(a);
    if (identity && identity->type != JSON_NULL) return identity;
    json_free(identity);
    const json_t *runner = jgetattr(st, "gateway_runner");
    if (!runner || runner->type == JSON_NULL
            || !json_get_bool(runner, "has_transport_owner", false))
        return NULL;
    a = args0();
    aset(a, "source", source);
    asets(a, "transport_profile", gwb_owner_transport_profile(st));
    json_t *r = gwb_call(s, "canonical_identity", a);
    json_free(a);
    if (gwb_raise_class(r)) {
        json_free(r);
        seam_log(s, "debug", "identity resolution failed; using legacy key readers");
        return NULL;
    }
    return r;
}

/* PoP: gwb_drop_unresolved @ gateway/platforms/base.py:_drop_unresolved */
bool gwb_drop_unresolved(const json_t *st, const json_t *event, const gwb_seams_t *s) {
    const json_t *source = jgetattr(event, "source");
    json_t *canon = gwb_canonicalize(st, source, s);
    if (canon && canon->type != JSON_NULL) { json_free(canon); return false; }
    json_free(canon);
    const json_t *rejected = jgetattr(source, "profile_route_rejected");
    if (!rejected || rejected->type != JSON_BOOL || !rejected->bool_val) return false;
    json_t *a = args0();
    aset(a, "source", source);
    json_t *r = gwb_call(s, "drop_rejected_route", a);
    json_free(a);
    json_free(r);
    return true;
}

/* ── exec-approval rendering helpers ── */

typedef struct { const gwb_seams_t *s; } ea_measure_ctx_t;

static int ea_measure_cb(void *ctx, const char *s) {
    ea_measure_ctx_t *m = ctx;
    json_t *a = args0();
    asets(a, "s", s);
    json_t *r = gwb_call(m->s, "measure", a);
    json_free(a);
    int units = r && r->type == JSON_NUMBER ? (int)r->num_val : 0;
    json_free(r);
    return units;
}

/* composed measure: message_len_fn(escape(s)) — escape applied to EACH prefix
 * exactly as the live lambda does, then measured. */
static int ea_composed_measure_cb(void *ctx, const char *s) {
    ea_measure_ctx_t *m = ctx;
    json_t *a = args0();
    asets(a, "text", s);
    json_t *esc = gwb_call(m->s, "ea_escape", a);
    json_free(a);
    const char *escaped = jstr(esc) ? jstr(esc) : s;
    a = args0();
    asets(a, "s", escaped);
    json_t *r = gwb_call(m->s, "measure", a);
    json_free(a);
    int units = r && r->type == JSON_NUMBER ? (int)r->num_val : 0;
    json_free(r);
    json_free(esc);
    return units;
}

/* PoP: gwb_ea_fit @ gateway/platforms/base.py:_ea_fit */
char *gwb_ea_fit(const char *text, int budget, const char *suffix, const gwb_seams_t *s) {
    const char *t = text ? text : "";
    ea_measure_ctx_t mctx = { s };
    return gwb_fit_prefix(t, budget, ea_composed_measure_cb, &mctx, suffix ? suffix : "...");
}

/* PoP: gwb_ea_deadline_line @ gateway/platforms/base.py:_ea_deadline_line */
char *gwb_ea_deadline_line(const json_t *st, const gwb_seams_t *s) {
    json_t *a = args0();
    json_t *to = gwb_call(s, "approval_timeout", a);
    json_free(a);
    a = args0();
    aset(a, "timeout", to);
    json_t *line = gwb_call(s, "deadline_line", a);
    json_free(a);
    json_free(to);
    a = args0();
    aset(a, "text", line);
    json_t *esc = gwb_call(s, "ea_escape", a);
    json_free(a);
    const char *prefix = json_get_str(st, "_EA_DEADLINE_PREFIX", "\n\n");
    const char *body = jstr(esc) ? jstr(esc) : "";
    size_t cap = strlen(prefix) + strlen(body) + 1;
    char *out = malloc(cap);
    snprintf(out, cap, "%s%s", prefix, body);
    json_free(line);
    json_free(esc);
    return out;
}

/* PoP: gwb_exec_approval_cmd_budget @ gateway/platforms/base.py:_exec_approval_cmd_budget */
int gwb_exec_approval_cmd_budget(const json_t *st, const char *description,
                                 bool smart_denied, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "description", description);
    asetb(a, "smart_denied", smart_denied);
    json_t *r = gwb_call(s, "cmd_budget", a);
    json_free(a);
    int budget = r && r->type == JSON_NUMBER ? (int)r->num_val : 0;
    json_free(r);
    return budget;
}

/* PoP: gwb_supports_exec_approval_buttons @ gateway/platforms/base.py:supports_exec_approval_buttons */
bool gwb_supports_exec_approval_buttons(bool overrides_prompt) {
    return overrides_prompt;
}

bool gwb_is_queue_text_debounce_candidate(const json_t *st, const json_t *event,
                                          const gwb_seams_t *s) {
    const char *mode = json_get_str(st, "_busy_text_mode", "interrupt");
    const char *mt = jstr(jgetattr(event, "message_type"));
    bool result = strcmp(mode, "queue") == 0
                  && mt && strcmp(mt, "text") == 0
                  && !json_get_bool(event, "internal", false);
    if (result) {
        json_t *a = args0();
        aset(a, "event", event);
        json_t *cmd = gwb_call(s, "is_command", a);
        json_free(a);
        result = !jtruth(cmd);
        json_free(cmd);
    }
    if (result) {
        char *t = str_strip(jstr(jgetattr(event, "text")) ? jstr(jgetattr(event, "text")) : "");
        result = t[0] != '\0';
        free(t);
    }
    if (result) {
        char *name = adapter_name(st);
        const char *sk = jstr(jgetattr(event, "session_key"));
        const char *text = jstr(jgetattr(event, "text"));
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "[%s] Queue-text debounce candidate accepted: session=%s text_len=%zu",
                 name, sk ? sk : "?", cp_len(text ? text : ""));
        seam_log(s, "debug", msg);
        free(name);
    }
    return result;
}

/* ── session keys / text batch plumbing ── */

/* PoP: gwb_source_session_key @ gateway/platforms/base.py:_source_session_key */
char *gwb_source_session_key(const json_t *st, const json_t *source, const gwb_seams_t *s) {
    json_t *canon = gwb_canonicalize(st, source, s);  /* identity FIRST */
    json_free(canon);
    const json_t *config = jgetattr(st, "config");
    const json_t *extra = jgetattr(config, "extra");
    const json_t *store = jgetattr(st, "_session_store");
    json_t *a = args0();
    aset(a, "source", source);
    aset(a, "extra", extra);
    json_t *key_source = gwb_call(s, "dm_thread_key_source", a);
    json_free(a);
    json_t *profile = gwb_session_key_profile(st, source, s);
    a = args0();
    aset(a, "source", key_source);
    asetb(a, "group_sessions_per_user", json_get_bool(extra, "group_sessions_per_user", true));
    asetb(a, "thread_sessions_per_user", json_get_bool(extra, "thread_sessions_per_user", false));
    aset(a, "profile", profile);
    json_t *key = gwb_call(s, "build_session_key", a);
    json_free(a);
    char *out = jstr(key) ? strdup(jstr(key)) : strdup("");
    json_free(key_source);
    json_free(profile);
    json_free(key);
    return out;
}

/* PoP: gwb_event_session_key @ gateway/platforms/base.py:_event_session_key */
char *gwb_event_session_key(const json_t *st, const json_t *event, const gwb_seams_t *s) {
    return gwb_source_session_key(st, jgetattr(event, "source"), s);
}

/* PoP: gwb_text_batch_key @ gateway/platforms/base.py:_text_batch_key */
char *gwb_text_batch_key(const json_t *st, const json_t *event, const gwb_seams_t *s) {
    char *base = gwb_event_session_key(st, event, s);
    const json_t *config = jgetattr(st, "config");
    json_t *a = args0();
    aset(a, "source", jgetattr(event, "source"));
    aset(a, "extra", jgetattr(config, "extra"));
    json_t *suffix = gwb_call(s, "dm_thread_batch_suffix", a);
    json_free(a);
    const char *suf = jstr(suffix) ? jstr(suffix) : "";
    size_t cap = strlen(base) + strlen(suf) + 1;
    char *out = malloc(cap);
    snprintf(out, cap, "%s%s", base, suf);
    free(base);
    json_free(suffix);
    return out;
}

/* PoP: gwb_pop_text_batch @ gateway/platforms/base.py:_pop_text_batch */
json_t *gwb_pop_text_batch(json_t *st, const char *key) {
    json_t *batches = json_obj_get(st, "_pending_text_batches");
    json_t *ev = json_obj_get(batches, key);
    if (!ev) return NULL;
    json_t *out = json_copy(ev);
    json_obj_del(batches, key);
    return out;
}

/* PoP: gwb_dispatch_text_batch @ gateway/platforms/base.py:_dispatch_text_batch */
void gwb_dispatch_text_batch(const json_t *st, const json_t *event, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    aset(a, "event", event);
    json_t *r = gwb_call(s, "handle_message", a);
    json_free(a);
    json_free(r);
}
/* ================================================================
 * FLOW cores
 * ================================================================ */

/* Path(path).suffix.lower() — last component's last-dot extension; dotfiles
 * (".bashrc") and dotless names have no suffix. Both separators handled. */
static char *path_suffix_lower(const char *path) {
    const char *p = path ? path : "";
    const char *base = p;
    for (const char *q = p; *q; q++)
        if (*q == '/' || *q == '\\') base = q + 1;
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return strdup("");
    return str_lower(dot);
}

/* urllib.parse.quote(path, safe="/") — UTF-8 percent-encoding, uppercase hex. */
static char *url_quote_path(const char *path) {
    const unsigned char *p = (const unsigned char *)(path ? path : "");
    size_t n = strlen((const char *)p);
    char *out = malloc(n * 3 + 1);
    size_t oi = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '_' || c == '.' || c == '-' || c == '~' || c == '/') {
            out[oi++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            out[oi++] = '%';
            out[oi++] = hex[c >> 4];
            out[oi++] = hex[c & 15];
        }
    }
    out[oi] = '\0';
    return out;
}

static bool ext_in_set(const char *ext, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(ext, set[i]) == 0) return true;
    return false;
}

static const char *const IMAGE_EXTS[] = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
static const char *const VIDEO_EXTS[] = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".3gp"};

/* raise object's str(exc) — the scripted "msg" field (default ""). */
static const char *raise_msg(const json_t *r) {
    const char *m = jstr(json_obj_get(r, "msg"));
    return m ? m : "";
}

/* PoP: gwb_handle_message_while_active @ gateway/platforms/base.py:_handle_message_while_active */
json_t *gwb_handle_message_while_active(json_t *st, json_t *event, const char *session_key,
                                        const gwb_seams_t *s) {
    json_t *ret = json_object();
    /* The shared runtime's durable FIFO replaces the adapter's busy slot. */
    if (gwb_shared_runtime_owns_input(st, event, s)) {
        json_t *a = args0();
        aset(a, "event", event);
        asets(a, "session_key", session_key);
        json_t *r = gwb_call(s, "dispatch_busy_burst", a);
        json_free(a);
        json_free(r);
        goto done;
    }
    json_t *canon = gwb_canonicalize(st, jgetattr(event, "source"), s);
    json_free(canon);
    json_t *a = args0();
    aset(a, "event", event);
    json_t *cmd = gwb_call(s, "get_command", a);
    json_free(a);
    a = args0();
    aset(a, "cmd", cmd);
    json_t *rb = gwb_call(s, "should_bypass", a);
    json_free(a);
    bool bypass = jtruth(rb);
    json_free(rb);
    if (bypass) {
        json_t *ri = NULL;
        bool interrupt = false;
        if (jtruth(cmd)) {
            a = args0();
            aset(a, "cmd", cmd);
            ri = gwb_call(s, "is_interrupt", a);
            json_free(a);
            interrupt = jtruth(ri);
            json_free(ri);
        }
        json_t *err = NULL;
        if (jtruth(cmd) && interrupt) {
            a = args0();
            aset(a, "event", event);
            asets(a, "session_key", session_key);
            aset(a, "cmd", cmd);
            err = gwb_call(s, "dispatch_active_session_command", a);
            json_free(a);
        } else {
            char *name = adapter_name(st);
            char *cs = cmd ? jpystr(cmd) : strdup("None");
            char msg[512];
            snprintf(msg, sizeof(msg), "[%s] Command '/%s' bypassing active-session guard for %s",
                     name, cs, session_key);
            seam_log(s, "debug", msg);
            free(name); free(cs);
            a = args0();
            aset(a, "event", event);
            err = gwb_call(s, "dispatch_inline_reply", a);
            json_free(a);
        }
        if (gwb_raise_class(err)) {
            char *name = adapter_name(st);
            char *cs = cmd ? jpystr(cmd) : strdup("None");
            char msg[768];
            snprintf(msg, sizeof(msg), "[%s] Command '/%s' dispatch failed: %s",
                     name, cs, raise_msg(err));
            seam_log(s, "error", msg);
            free(name); free(cs);
        }
        json_free(err);
        json_free(cmd);
        goto done;
    }
    /* Clarify bypass: the next message must reach the text-intercept. */
    if (!jtruth(cmd) && json_get_bool(event, "allow_gateway_control", false)) {
        a = args0();
        asets(a, "session_key", session_key);
        asetb(a, "include_choice_prompts", true);
        json_t *rp = gwb_call(s, "clarify_pending", a);
        json_free(a);
        bool has_clarify = !gwb_raise_class(rp) && rp && rp->type != JSON_NULL;
        json_free(rp);
        if (has_clarify) {
            char *name = adapter_name(st);
            char msg[512];
            snprintf(msg, sizeof(msg), "[%s] Routing message to clarify text-intercept for %s",
                     name, session_key);
            seam_log(s, "debug", msg);
            free(name);
            a = args0();
            aset(a, "event", event);
            json_t *err = gwb_call(s, "dispatch_inline_reply", a);
            json_free(a);
            if (gwb_raise_class(err)) {
                name = adapter_name(st);
                snprintf(msg, sizeof(msg), "[%s] Clarify text-intercept dispatch failed: %s",
                         name, raise_msg(err));
                seam_log(s, "error", msg);
                free(name);
            }
            json_free(err);
            json_free(cmd);
            goto done;
        }
    }
    json_free(cmd);
    /* Busy-session handler (runner-owned admission). */
    const json_t *bh = jgetattr(st, "_busy_session_handler");
    if (bh && bh->type != JSON_NULL) {
        a = args0();
        aset(a, "event", event);
        asets(a, "session_key", session_key);
        json_t *rh = gwb_call(s, "busy_handler", a);
        json_free(a);
        bool handled;
        if (gwb_raise_class(rh)) {
            char *name = adapter_name(st);
            char msg[768];
            snprintf(msg, sizeof(msg), "[%s] Busy-session handler failed: %s",
                     name, raise_msg(rh));
            seam_log(s, "error", msg);
            free(name);
            const json_t *acc = jgetattr(event, "_gateway_accepted");
            handled = acc && acc->type == JSON_BOOL && acc->bool_val;
        } else {
            handled = jtruth(rh);
        }
        json_free(rh);
        json_t *active = json_obj_get(st, "_active_sessions");
        if (!json_obj_get(active, session_key)) {
            json_t *owners = gwb_pending_recovery_owners(st, event, session_key, s);
            json_t *orphan = NULL;
            for (size_t i = 0; i < json_len(owners); i++) {
                const char *owner = jstr(json_get(owners, i));
                a = args0();
                asets(a, "owner", owner);
                asets(a, "session_key", session_key);
                orphan = gwb_call(s, "get_pending", a);
                json_free(a);
                if (orphan && orphan->type != JSON_NULL) {
                    a = args0();
                    asets(a, "owner", owner);
                    aset(a, "event", orphan);
                    asets(a, "session_key", session_key);
                    json_t *rs = gwb_call(s, "start_session", a);
                    json_free(a);
                    json_free(rs);
                    break;
                }
                json_free(orphan);
                orphan = NULL;
            }
            json_free(owners);
            if ((!orphan || orphan->type == JSON_NULL) && !handled) {
                json_free(orphan);
                a = args0();
                asets(a, "owner", "self");
                aset(a, "event", event);
                asets(a, "session_key", session_key);
                json_t *rs = gwb_call(s, "start_session", a);
                json_free(a);
                json_set(event, "_gateway_accepted", rs ? rs : json_bool(false));
                goto done;
            }
            json_free(orphan);
        }
        if (handled) goto done;
    }
    /* Without a runner FIFO, do not merge a wake into an occupied human slot. */
    json_t *pending = json_obj_get(st, "_pending_messages");
    if (json_get_bool(event, "internal", false) && json_obj_get(pending, session_key))
        goto done;
    const char *mt = jstr(jgetattr(event, "message_type"));
    if (mt && strcmp(mt, "photo") == 0) {
        char *name = adapter_name(st);
        char msg[512];
        snprintf(msg, sizeof(msg), "[%s] Queuing photo follow-up for session %s without interrupt",
                 name, session_key);
        seam_log(s, "debug", msg);
        free(name);
        a = args0();
        asets(a, "session_key", session_key);
        aset(a, "event", event);
        asetb(a, "merge_text", false);
        json_t *rm = gwb_call(s, "merge_pending", a);
        json_free(a);
        json_free(rm);
        json_set(event, "_gateway_accepted", json_bool(true));
        goto done;
    }
    if (gwb_is_queue_text_debounce_candidate(st, event, s)) {
        char *name = adapter_name(st);
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "[%s] New text message while session %s is active \xe2\x80\x94 "
                 "debouncing follow-up (busy_text_mode=queue, window=%.2fs)",
                 name, session_key, json_get_num(st, "_busy_text_debounce_seconds", 0.35));
        seam_log(s, "debug", msg);
        free(name);
        a = args0();
        asets(a, "session_key", session_key);
        aset(a, "event", event);
        json_t *rq = gwb_call(s, "queue_debounce", a);
        json_free(a);
        json_free(rq);
    } else {
        char *name = adapter_name(st);
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "[%s] New message while session %s is active \xe2\x80\x94 queuing follow-up "
                 "(no interrupt, will cascade after current turn)", name, session_key);
        seam_log(s, "debug", msg);
        free(name);
        a = args0();
        asets(a, "session_key", session_key);
        aset(a, "event", event);
        asetb(a, "merge_text", mt && strcmp(mt, "text") == 0);
        json_t *rm = gwb_call(s, "merge_pending", a);
        json_free(a);
        json_free(rm);
        json_set(event, "_gateway_accepted", json_bool(true));
    }
done:
    json_set(ret, "accepted", jdup(jgetattr(event, "_gateway_accepted")));
    return ret;
}

/* PoP: gwb_send_image_batch @ gateway/platforms/base.py:_send_image_batch */
void gwb_send_image_batch(json_t *st, const json_t *event, const json_t *images,
                          const json_t *metadata, double human_delay, const gwb_seams_t *s) {
    const char *chat_id = jstr(jgetattr(jgetattr(event, "source"), "chat_id"));
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    aset(a, "images", images);
    aset(a, "metadata", metadata);
    asetn(a, "human_delay", human_delay);
    json_t *result = gwb_call(s, "send_multiple_images", a);
    json_free(a);
    if (gwb_raise_class(result)) {
        char *name = adapter_name(st);
        char msg[768];
        snprintf(msg, sizeof(msg), "[%s] Error batching images: %s", name, raise_msg(result));
        seam_log(s, "warning", msg);
        free(name);
        char *errstr = strdup(raise_msg(result));
        json_free(result);
        result = json_object();
        json_set(result, "success", json_bool(false));
        json_set(result, "error", json_string(errstr));
        free(errstr);
    }
    if (result && result->type != JSON_NULL && !jtruth(json_obj_get(result, "success"))) {
        char *name = adapter_name(st);
        char msg[768];
        snprintf(msg, sizeof(msg), "[%s] %zu image(s) not delivered: %s",
                 name, json_len(images), jstr(json_obj_get(result, "error")) ? jstr(json_obj_get(result, "error")) : "");
        seam_log(s, "warning", msg);
        free(name);
    }
    a = args0();
    aset(a, "result", result);
    json_t *rr = gwb_call(s, "record_delivery", a);
    json_free(a);
    json_free(rr);
    a = args0();
    asets(a, "chat_id", chat_id);
    aset(a, "images", images);
    aset(a, "result", result);
    aset(a, "metadata", metadata);
    json_t *rf = gwb_call(s, "send_footer", a);
    json_free(a);
    json_free(rf);
    json_free(result);
}

/* _deliver_media_attachments._send_one */
static json_t *deliver_send_one(json_t *st, const char *chat_id, const char *path,
                                bool is_voice, bool media_tag, const json_t *metadata,
                                const gwb_seams_t *s) {
    char *ext = path_suffix_lower(path);
    char *pn = platform_name(jgetattr(st, "platform"));
    json_t *result = NULL;
    json_t *a = args0();
    if (media_tag && gwb_should_send_media_as_audio(pn, ext, is_voice)) {
        asets(a, "chat_id", chat_id);
        asets(a, "audio_path", path);
        aset(a, "metadata", metadata);
        asetb(a, "is_voice", is_voice);
        result = gwb_call(s, "send_voice", a);
    } else if (ext_in_set(ext, VIDEO_EXTS, sizeof(VIDEO_EXTS) / sizeof(VIDEO_EXTS[0]))) {
        if (media_tag) {
            char *name = adapter_name(st);
            char msg[512];
            snprintf(msg, sizeof(msg), "[%s] Sending video attachment (%s) to %s",
                     name, ext, chat_id ? chat_id : "");
            seam_log(s, "info", msg);
            free(name);
        }
        asets(a, "chat_id", chat_id);
        asets(a, "video_path", path);
        aset(a, "metadata", metadata);
        result = gwb_call(s, "send_video", a);
    } else {
        asets(a, "chat_id", chat_id);
        asets(a, "file_path", path);
        aset(a, "metadata", metadata);
        result = gwb_call(s, "send_document", a);
    }
    json_free(a);
    free(pn);
    if (result && !gwb_raise_class(result) && !jtruth(json_obj_get(result, "success"))) {
        char *name = adapter_name(st);
        char msg[768];
        snprintf(msg, sizeof(msg), "[%s] Failed to send %s (%s): %s",
                 name, media_tag ? "media" : "local file", ext,
                 jstr(json_obj_get(result, "error")) ? jstr(json_obj_get(result, "error")) : "");
        seam_log(s, "warning", msg);
        free(name);
        const json_t *raw = json_obj_get(result, "raw_response");
        bool notice_sent = raw && raw->type == JSON_OBJECT
                           && jtruth(json_obj_get(raw, "failure_notice_sent"));
        if (!notice_sent) {
            a = args0();
            asets(a, "chat_id", chat_id);
            asets(a, "path", path);
            asetb(a, "is_voice", is_voice);
            aset(a, "metadata", metadata);
            json_t *rn = gwb_call(s, "notify_failure", a);
            json_free(a);
            json_free(rn);
        }
    }
    free(ext);
    return result;
}

/* PoP: gwb_deliver_media_attachments @ gateway/platforms/base.py:_deliver_media_attachments */
void gwb_deliver_media_attachments(json_t *st, const json_t *event, const json_t *media_files,
                                   const json_t *local_files, bool force_document_attachments,
                                   double human_delay, const json_t *metadata, const gwb_seams_t *s) {
    /* _as_image closure: image ext AND not [[as_document]]. */
    json_t *image_paths = json_array();
    size_t nm = media_files ? json_len(media_files) : 0;
    for (size_t i = 0; i < nm; i++) {
        const json_t *pair = json_get(media_files, i);
        const char *p = jstr(json_get(pair, 0));
        bool v = jtruth(json_get(pair, 1));
        char *ext = path_suffix_lower(p);
        bool as_img = ext_in_set(ext, IMAGE_EXTS, sizeof(IMAGE_EXTS) / sizeof(IMAGE_EXTS[0]))
                      && !force_document_attachments;
        free(ext);
        if (!v && as_img) json_append(image_paths, json_string(p ? p : ""));
    }
    size_t nl = local_files ? json_len(local_files) : 0;
    for (size_t i = 0; i < nl; i++) {
        const char *p = jstr(json_get(local_files, i));
        char *ext = path_suffix_lower(p);
        bool as_img = ext_in_set(ext, IMAGE_EXTS, sizeof(IMAGE_EXTS) / sizeof(IMAGE_EXTS[0]))
                      && !force_document_attachments;
        free(ext);
        if (as_img) json_append(image_paths, json_string(p ? p : ""));
    }
    if (json_len(image_paths) > 0) {
        json_t *images = json_array();
        for (size_t i = 0; i < json_len(image_paths); i++) {
            char *q = url_quote_path(jstr(json_get(image_paths, i)));
            size_t cap = strlen(q) + 8;
            char *url = malloc(cap);
            snprintf(url, cap, "file://%s", q);
            free(q);
            json_t *pair = json_array();
            json_append(pair, json_string(url));
            json_append(pair, json_string(""));
            json_append(images, pair);
            free(url);
        }
        gwb_send_image_batch(st, event, images, metadata, human_delay, s);
        json_free(images);
    }
    json_free(image_paths);
    const char *chat_id = jstr(jgetattr(jgetattr(event, "source"), "chat_id"));
    /* queue: MEDIA (voice or non-image) first, then non-image local files. */
    typedef struct { const char *path; bool is_voice; bool media_tag; } qitem_t;
    qitem_t *queue = malloc((nm + nl + 1) * sizeof(qitem_t));
    size_t nq = 0;
    for (size_t i = 0; i < nm; i++) {
        const json_t *pair = json_get(media_files, i);
        const char *p = jstr(json_get(pair, 0));
        bool v = jtruth(json_get(pair, 1));
        char *ext = path_suffix_lower(p);
        bool as_img = ext_in_set(ext, IMAGE_EXTS, sizeof(IMAGE_EXTS) / sizeof(IMAGE_EXTS[0]))
                      && !force_document_attachments;
        free(ext);
        if (v || !as_img) { queue[nq].path = p; queue[nq].is_voice = v; queue[nq].media_tag = true; nq++; }
    }
    if (nq > 0) {
        char *name = adapter_name(st);
        char msg[256];
        snprintf(msg, sizeof(msg), "[%s] Delivering %zu non-image MEDIA attachment(s)", name, nq);
        seam_log(s, "info", msg);
        free(name);
    }
    for (size_t i = 0; i < nl; i++) {
        const char *p = jstr(json_get(local_files, i));
        char *ext = path_suffix_lower(p);
        bool as_img = ext_in_set(ext, IMAGE_EXTS, sizeof(IMAGE_EXTS) / sizeof(IMAGE_EXTS[0]))
                      && !force_document_attachments;
        free(ext);
        if (!as_img) { queue[nq].path = p; queue[nq].is_voice = false; queue[nq].media_tag = false; nq++; }
    }
    for (size_t i = 0; i < nq; i++) {
        if (human_delay > 0) {
            json_t *a = args0();
            asetn(a, "delay", human_delay);
            json_t *rs = gwb_call(s, "sleep", a);
            json_free(a);
            json_free(rs);
        }
        json_t *result = deliver_send_one(st, chat_id, queue[i].path, queue[i].is_voice,
                                          queue[i].media_tag, metadata, s);
        if (gwb_raise_class(result)) {
            json_t *fail = json_object();
            json_set(fail, "success", json_bool(false));
            json_set(fail, "error", json_string(raise_msg(result)));
            json_t *a = args0();
            aset(a, "result", fail);
            json_t *rr = gwb_call(s, "record_delivery", a);
            json_free(a);
            json_free(rr);
            char *name = adapter_name(st);
            char msg[768];
            if (queue[i].media_tag)
                snprintf(msg, sizeof(msg), "[%s] Error sending media: %s", name, raise_msg(result));
            else
                snprintf(msg, sizeof(msg), "[%s] Error sending local file %s: %s",
                         name, queue[i].path ? queue[i].path : "", raise_msg(result));
            seam_log(s, queue[i].media_tag ? "warning" : "error", msg);
            free(name);
            json_free(fail);
        } else {
            json_t *a = args0();
            aset(a, "result", result);
            json_t *rr = gwb_call(s, "record_delivery", a);
            json_free(a);
            json_free(rr);
        }
        json_free(result);
    }
    free(queue);
}

/* PoP: gwb_acquire_platform_lock @ gateway/platforms/base.py:_acquire_platform_lock */
bool gwb_acquire_platform_lock(json_t *st, const char *scope, const char *identity,
                               const char *resource_desc, const gwb_seams_t *s) {
    json_set(st, "_platform_lock_scope", json_string(scope ? scope : ""));
    json_set(st, "_platform_lock_identity", json_string(identity ? identity : ""));
    json_t *lock_meta = json_object();
    {
        char *pv = platform_name(jgetattr(st, "platform"));
        json_set(lock_meta, "platform", json_string(pv));
        free(pv);
    }
    json_t *a = args0();
    asets(a, "scope", scope);
    asets(a, "identity", identity);
    aset(a, "metadata", lock_meta);
    json_t *r = gwb_call(s, "acquire_lock", a);
    json_free(a);
    if (gwb_caught(r, "ScopedLockDirUnusable")) {
        const char *filename = jstr(json_obj_get(r, "filename"));
        json_set(st, "_platform_lock_identity", json_null());
        a = args0();
        asets(a, "filename", filename);
        json_t *rem = gwb_call(s, "dir_remedy", a);
        json_free(a);
        const char *remedy = jstr(rem) ? jstr(rem) : "";
        char msg[1024];
        snprintf(msg, sizeof(msg),
                 "%s lock cannot be taken: the gateway lock directory %s is not usable by this "
                 "user (likely created by an elevated Hermes). To repair, %s; the gateway "
                 "retries on its own.",
                 resource_desc ? resource_desc : "", filename ? filename : "", remedy);
        a = args0();
        asets(a, "code", "lock_dir_unusable");
        asets(a, "message", msg);
        asetb(a, "retryable", true);
        json_t *rf = gwb_call(s, "set_fatal_error", a);
        json_free(a);
        json_free(rf);
        json_free(rem);
        json_free(r);
        json_free(lock_meta);
        return false;
    }
    bool acquired = jtruth(json_obj_get(r, "acquired"));
    json_t *existing = json_copy(json_obj_get(r, "existing"));
    if (acquired) {
        json_free(r); json_free(existing); json_free(lock_meta);
        return true;
    }
    bool allowed = json_get_bool(st, "_platform_lock_takeover_allowed", false);
    bool attempted = json_get_bool(st, "_platform_lock_takeover_attempted", false);
    if (allowed && !attempted && existing && existing->type == JSON_OBJECT) {
        /* Consume the authority before any I/O: at most one termination attempt. */
        json_set(st, "_platform_lock_takeover_allowed", json_bool(false));
        json_set(st, "_platform_lock_takeover_attempted", json_bool(true));
        a = args0();
        aset(a, "existing", existing);
        json_t *pidr = gwb_call(s, "takeover", a);
        json_free(a);
        if (pidr && pidr->type != JSON_NULL) {
            long long pid = (long long)pidr->num_val;
            char *name = adapter_name(st);
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "[%s] %s was held by gateway PID %lld \xe2\x80\x94 explicit --replace "
                     "handoff completed", name, resource_desc ? resource_desc : "", pid);
            seam_log(s, "warning", msg);
            free(name);
            a = args0();
            asets(a, "scope", scope);
            asets(a, "identity", identity);
            aset(a, "metadata", lock_meta);
            json_t *r2 = gwb_call(s, "acquire_lock", a);
            json_free(a);
            if (jtruth(json_obj_get(r2, "acquired"))) {
                name = adapter_name(st);
                snprintf(msg, sizeof(msg), "[%s] Acquired %s after taking over PID %lld",
                         name, resource_desc ? resource_desc : "", pid);
                seam_log(s, "info", msg);
                free(name);
                json_free(r2); json_free(pidr); json_free(r); json_free(existing); json_free(lock_meta);
                return true;
            }
            json_free(existing);
            existing = json_copy(json_obj_get(r2, "existing"));
            json_free(r2);
        }
        json_free(pidr);
    }
    const json_t *pidv = (existing && existing->type == JSON_OBJECT)
                         ? json_obj_get(existing, "pid") : NULL;
    a = args0();
    aset(a, "existing", existing);
    json_t *labelr = gwb_call(s, "owner_label", a);
    json_free(a);
    const char *owner_profile = jstr(labelr);
    char pid_part[64] = "";
    if (jtruth(pidv)) snprintf(pid_part, sizeof(pid_part), " (PID %lld)", (long long)pidv->num_val);
    char holder[256];
    if (owner_profile && owner_profile[0])
        snprintf(holder, sizeof(holder), " by the '%s' profile gateway%s", owner_profile, pid_part);
    else
        snprintf(holder, sizeof(holder), "%s", pid_part);
    char remedy[256];
    if (owner_profile && owner_profile[0])
        snprintf(remedy, sizeof(remedy),
                 " Stop that gateway first (hermes --profile %s gateway stop).", owner_profile);
    else
        snprintf(remedy, sizeof(remedy), " Stop the other gateway first.");
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s already in use%s.%s",
             resource_desc ? resource_desc : "", holder, remedy);
    char *name = adapter_name(st);
    char logmsg[1200];
    snprintf(logmsg, sizeof(logmsg), "[%s] %s", name, msg);
    seam_log(s, "error", logmsg);
    free(name);
    char code[128];
    snprintf(code, sizeof(code), "%s_lock", scope ? scope : "");
    a = args0();
    asets(a, "code", code);
    asets(a, "message", msg);
    asetb(a, "retryable", true);
    json_t *rf = gwb_call(s, "set_fatal_error", a);
    json_free(a);
    json_free(rf);
    json_free(labelr);
    json_free(r);
    json_free(existing);
    json_free(lock_meta);
    return false;
}

/* PoP: gwb_release_platform_lock @ gateway/platforms/base.py:_release_platform_lock */
void gwb_release_platform_lock(json_t *st, const gwb_seams_t *s) {
    const json_t *identity = jgetattr(st, "_platform_lock_identity");
    if (!jtruth(identity)) return;
    json_t *a = args0();
    aset(a, "scope", jgetattr(st, "_platform_lock_scope"));
    aset(a, "identity", identity);
    json_t *r = gwb_call(s, "release_lock", a);
    json_free(a);
    json_free(r);
    json_set(st, "_platform_lock_identity", json_null());
}

/* PoP: gwb_wire_plugin_handlers @ gateway/platforms/base.py:_wire_plugin_handlers */
void gwb_wire_plugin_handlers(json_t *st, const json_t *native_id, const gwb_seams_t *s) {
    char *pn = platform_name(jgetattr(st, "platform"));
    json_t *a = args0();
    asets(a, "platform", pn);
    json_t *factories = gwb_call(s, "get_factories", a);
    json_free(a);
    free(pn);
    if (gwb_raise_class(factories)) {
        char *name = adapter_name(st);
        char msg[768];
        snprintf(msg, sizeof(msg), "[%s] Could not load plugin handler factories: %s",
                 name, raise_msg(factories));
        seam_log(s, "warning", msg);
        free(name);
        json_free(factories);
        return;
    }
    const json_t *cur_native = jgetattr(st, "_plugin_handler_native");
    json_t *wired = json_obj_get(st, "_plugin_handlers_wired");
    /* identity compare: same id value (NULL == None). */
    bool same_native = (!cur_native || cur_native->type == JSON_NULL)
                       ? (!native_id || native_id->type == JSON_NULL)
                       : (native_id && cur_native->type == native_id->type
                          && ((cur_native->type == JSON_STRING
                               && strcmp(cur_native->str_val, native_id->str_val) == 0)
                              || (cur_native->type == JSON_NUMBER
                                  && cur_native->num_val == native_id->num_val)));
    if (!same_native || !wired || wired->type == JSON_NULL) {
        json_set(st, "_plugin_handler_native", native_id ? json_copy(native_id) : json_null());
        json_set(st, "_plugin_handlers_wired", json_array());
        wired = json_obj_get(st, "_plugin_handlers_wired");
    }
    size_t n = factories ? json_len(factories) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *f = json_get(factories, i);
        const char *plugin = jstr(json_obj_get(f, "plugin"));
        const json_t *qv = json_obj_get(f, "qualname");
        char *key2 = jtruth(qv) ? jpystr(qv) : jpystr(json_obj_get(f, "repr"));
        bool already = false;
        for (size_t j = 0; j < json_len(wired); j++) {
            const json_t *k = json_get(wired, j);
            if (strcmp(jstr(json_get(k, 0)), plugin ? plugin : "") == 0
                    && strcmp(jstr(json_get(k, 1)), key2) == 0) { already = true; break; }
        }
        if (!already) {
            a = args0();
            aset(a, "native", native_id);
            aset(a, "factory_id", json_obj_get(f, "id"));
            json_t *r = gwb_call(s, "factory", a);
            json_free(a);
            char *name = adapter_name(st);
            char msg[768];
            if (gwb_raise_class(r)) {
                snprintf(msg, sizeof(msg), "[%s] Plugin '%s' handler factory raised: %s",
                         name, plugin ? plugin : "", raise_msg(r));
                seam_log(s, "error", msg);
            } else {
                snprintf(msg, sizeof(msg), "[%s] Wired native handlers from plugin '%s'",
                         name, plugin ? plugin : "");
                seam_log(s, "info", msg);
            }
            free(name);
            json_free(r);
            /* A raising factory is recorded too. */
            json_t *key = json_array();
            json_append(key, json_string(plugin ? plugin : ""));
            json_append(key, json_string(key2));
            json_append(wired, key);
        }
        free(key2);
    }
    json_free(factories);
}

/* PoP: gwb_rewire_plugin_handlers @ gateway/platforms/base.py:rewire_plugin_handlers */
void gwb_rewire_plugin_handlers(json_t *st, const gwb_seams_t *s) {
    const json_t *wired = jgetattr(st, "_plugin_handlers_wired");
    if (!wired || wired->type == JSON_NULL) return;
    gwb_wire_plugin_handlers(st, jgetattr(st, "_plugin_handler_native"), s);
}
/* PoP: gwb_enqueue_text_event @ gateway/platforms/base.py:_enqueue_text_event */
void gwb_enqueue_text_event(json_t *st, json_t *event, const gwb_seams_t *s) {
    if (gwb_drop_unresolved(st, event, s)) return;
    char *key = gwb_text_batch_key(st, event, s);
    json_t *batches = json_obj_get(st, "_pending_text_batches");
    if (!batches) { batches = json_object(); json_set(st, "_pending_text_batches", batches); }
    json_t *existing = json_obj_get(batches, key);
    const char *ev_text = jstr(jgetattr(event, "text"));
    if (!existing) {
        /* existing = batches[key] = event (aliasing: the chunk len lands on
         * the event itself when it IS the batch). */
        json_set(event, "_last_chunk_len", json_number((double)cp_len(ev_text ? ev_text : "")));
        json_set(batches, key, json_copy(event));
        existing = json_obj_get(batches, key);
    } else {
        if (ev_text && ev_text[0] != '\0') {
            char *merged = gwb_append_text(jstr(jgetattr(existing, "text")), ev_text);
            json_set(existing, "text", merged ? json_string(merged) : json_null());
            free(merged);
        }
        if (jtruth(jgetattr(event, "media_urls"))) {
            json_t *a = args0();
            aset(a, "event", existing);
            json_t *fe = gwb_call(s, "pad_flags", a);
            json_free(a);
            a = args0();
            aset(a, "event", event);
            json_t *fi = gwb_call(s, "pad_flags", a);
            json_free(a);
            json_t *flags = json_array();
            for (size_t i = 0; i < json_len(fe); i++) json_append(flags, json_copy(json_get(fe, i)));
            for (size_t i = 0; i < json_len(fi); i++) json_append(flags, json_copy(json_get(fi, i)));
            json_set(existing, "media_text_inlined", flags);
            json_free(fe); json_free(fi);
            a = args0();
            aset(a, "existing", existing);
            aset(a, "event", event);
            json_t *rm = gwb_call(s, "extend_media", a);
            json_free(a);
            json_free(rm);
        }
        json_t *a = args0();
        aset(a, "existing", existing);
        aset(a, "event", event);
        json_t *ra = gwb_call(s, "absorb_turn_input", a);
        json_free(a);
        json_free(ra);
        json_set(existing, "_last_chunk_len", json_number((double)cp_len(ev_text ? ev_text : "")));
    }
    json_t *tasks = json_obj_get(st, "_pending_text_batch_tasks");
    if (!tasks) { tasks = json_object(); json_set(st, "_pending_text_batch_tasks", tasks); }
    json_t *prior = json_obj_get(tasks, key);
    if (jtruth(prior) && !json_get_bool(prior, "done", false))
        json_set(prior, "cancelled", json_bool(true));
    json_t *a = args0();
    asets(a, "key", key);
    json_t *task = gwb_call(s, "create_task", a);
    json_free(a);
    json_set(tasks, key, task ? task : json_null());
    free(key);
}

/* PoP: gwb_flush_text_batch @ gateway/platforms/base.py:_flush_text_batch */
json_t *gwb_flush_text_batch(json_t *st, const char *key, int current_task_id, const gwb_seams_t *s) {
    json_t *uncaught = NULL;
    json_t *batches = json_obj_get(st, "_pending_text_batches");
    json_t *tasks = json_obj_get(st, "_pending_text_batch_tasks");
    double delay = gwb_text_batch_delay_for(st, json_obj_get(batches, key));
    json_t *a = args0();
    asetn(a, "delay", delay);
    json_t *rs = gwb_call(s, "sleep", a);
    json_free(a);
    if (gwb_raise_class(rs)) {
        if (!gwb_caught(rs, "CancelledError")) uncaught = rs;
        else json_free(rs);
        goto finally;
    }
    json_free(rs);
    {
        json_t *owner = json_obj_get(tasks, key);
        if (owner && owner->type != JSON_NULL
                && (int)json_get_num(owner, "id", -1) != current_task_id)
            goto finally;
        json_t *event = gwb_pop_text_batch(st, key);
        if (!event) goto finally;
        char *name = adapter_name(st);
        char msg[512];
        snprintf(msg, sizeof(msg), "[%s] Flushing text batch %s (%zu chars)",
                 name, key, cp_len(jstr(jgetattr(event, "text")) ? jstr(jgetattr(event, "text")) : ""));
        seam_log(s, "info", msg);
        free(name);
        a = args0();
        aset(a, "event", event);
        json_t *rd = gwb_call(s, "handle_message", a);
        json_free(a);
        json_free(event);
        if (gwb_raise_class(rd)) {
            if (!gwb_caught(rd, "CancelledError")) uncaught = rd;
            else json_free(rd);
        } else {
            json_free(rd);
        }
    }
finally:
    {
        json_t *owner = json_obj_get(tasks, key);
        if (owner && owner->type != JSON_NULL
                && (int)json_get_num(owner, "id", -1) == current_task_id)
            json_obj_del(tasks, key);
    }
    return uncaught;
}

/* PoP: gwb_flush_text_batch_now @ gateway/platforms/base.py:_flush_text_batch_now */
void gwb_flush_text_batch_now(json_t *st, const char *key, const gwb_seams_t *s) {
    json_t *event = gwb_pop_text_batch(st, key);
    if (event) {
        gwb_dispatch_text_batch(st, event, s);
        json_free(event);
    }
}

/* PoP: gwb_spawn_drain_task @ gateway/platforms/base.py:_spawn_drain_task */
void gwb_spawn_drain_task(json_t *st, const json_t *pending_event, const char *session_key,
                          double delay, const gwb_seams_t *s) {
    gwb_clear_session_guard(st, session_key);
    /* Capture the guard this drain owns now (a swapped-in guard must survive). */
    json_t *active = json_obj_get(st, "_active_sessions");
    const json_t *guard = json_obj_get(active, session_key);
    json_t *a = args0();
    aset(a, "pending_event", pending_event);
    asets(a, "session_key", session_key);
    asetn(a, "delay", delay);
    aset(a, "guard", guard);
    json_t *task = gwb_call(s, "create_task", a);
    json_free(a);
    if (task) {
        gwb_track_session_task(st, session_key, task);
        json_free(task);
    }
}

/* PoP: gwb_finish_session_task @ gateway/platforms/base.py:_finish_session_task */
void gwb_finish_session_task(json_t *st, const char *session_key, int current_task_id,
                             const json_t *interrupt_event, const gwb_seams_t *s) {
    json_t *pending = json_obj_get(st, "_pending_messages");
    if (!pending) { pending = json_object(); json_set(st, "_pending_messages", pending); }
    json_t *late = json_obj_get(pending, session_key);
    json_t *late_copy = late ? json_copy(late) : NULL;
    if (late) json_obj_del(pending, session_key);
    if (late_copy) {
        json_t *tasks = json_obj_get(st, "_session_tasks");
        json_t *existing = json_obj_get(tasks, session_key);
        if (existing && existing->type != JSON_NULL
                && (int)json_get_num(existing, "id", -1) != current_task_id) {
            /* A follow-up task already owns the session: re-queue for it. */
            json_set(pending, session_key, late_copy);
        } else {
            char *name = adapter_name(st);
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "[%s] Late-arrival pending message during cleanup \xe2\x80\x94 spawning drain task",
                     name);
            seam_log(s, "debug", msg);
            free(name);
            gwb_spawn_drain_task(st, late_copy, session_key, 0.0, s);
            json_free(late_copy);
        }
    } else if (current_task_id >= 0) {
        json_t *tasks = json_obj_get(st, "_session_tasks");
        json_t *existing = json_obj_get(tasks, session_key);
        if (existing && existing->type != JSON_NULL
                && (int)json_get_num(existing, "id", -1) == current_task_id) {
            json_t *a = args0();
            asets(a, "session_key", session_key);
            aset(a, "guard", interrupt_event);
            json_t *r = gwb_call(s, "cleanup_finished", a);
            json_free(a);
            json_free(r);
        }
    }
}

/* PoP: gwb_drain_after @ gateway/platforms/base.py:_drain_after */
void gwb_drain_after(json_t *st, const json_t *pending_event, const char *session_key,
                     double delay, const json_t *guard, const gwb_seams_t *s) {
    const json_t *ev = pending_event;
    json_t *popped = NULL;
    if (delay > 0) {
        json_t *a = args0();
        asetn(a, "delay", delay);
        json_t *rs = gwb_call(s, "sleep", a);
        json_free(a);
        json_free(rs);
        a = args0();
        asets(a, "session_key", session_key);
        json_t *rf = gwb_call(s, "flush_debounce", a);
        json_free(a);
        json_free(rf);
        json_t *pending = json_obj_get(st, "_pending_messages");
        json_t *slot = json_obj_get(pending, session_key);
        if (!slot) {
            a = args0();
            asets(a, "session_key", session_key);
            aset(a, "guard", guard);
            json_t *rc = gwb_call(s, "cleanup_finished", a);
            json_free(a);
            json_free(rc);
            return;
        }
        popped = json_copy(slot);
        json_obj_del(pending, session_key);
        ev = popped;
    }
    json_t *a = args0();
    aset(a, "event", ev);
    asets(a, "session_key", session_key);
    json_t *r = gwb_call(s, "process_background", a);
    json_free(a);
    json_free(r);
    json_free(popped);
}

/* PoP: gwb_send_final_ledgered @ gateway/platforms/base.py:send_final_ledgered */
json_t *gwb_send_final_ledgered(json_t *st, const json_t *event, const char *session_key,
                                const char *text_content, const json_t *metadata,
                                const json_t *reply_to, bool is_ephemeral_response,
                                const gwb_seams_t *s) {
    const char *chat_id = jstr(jgetattr(jgetattr(event, "source"), "chat_id"));
    /* ledger_bracket: record the obligation + resolve the CURRENT transport
     * (base_delivery_ledger orchestration; scripted bracket result). */
    json_t *a = args0();
    aset(a, "event", event);
    asets(a, "session_key", session_key);
    asets(a, "text_content", text_content);
    asetb(a, "is_ephemeral_response", is_ephemeral_response);
    json_t *bracket = gwb_call(s, "ledger_bracket", a);
    json_free(a);
    const json_t *obligation_id = json_obj_get(bracket, "obligation_id");
    const json_t *adapter = json_obj_get(bracket, "delivery_adapter");
    /* the _send closure (live base.py code): */
    char *aname = jstr(json_obj_get(adapter, "name")) ? strdup(jstr(json_obj_get(adapter, "name")))
                 : adapter_name(st);
    char msg[512];
    snprintf(msg, sizeof(msg), "[%s] Sending response (%zu chars) to %s",
             aname, cp_len(text_content ? text_content : ""), chat_id ? chat_id : "");
    seam_log(s, "info", msg);
    free(aname);
    if (obligation_id && obligation_id->type != JSON_NULL)
        gwb_release_turn_marker(st, (json_t *)event, s);  /* ledger owns crash recovery now */
    a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "content", text_content);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    json_t *result = gwb_call(s, "send_with_retry", a);
    json_free(a);
    a = args0();
    aset(a, "adapter", adapter);
    asets(a, "chat_id", chat_id);
    aset(a, "result", result);
    json_t *rc = gwb_call(s, "stop_clock", a);
    json_free(a);
    json_free(rc);
    /* bracket finalize (from the send result): */
    a = args0();
    aset(a, "event", event);
    asets(a, "session_key", session_key);
    aset(a, "obligation_id", obligation_id);
    aset(a, "result", result);
    aset(a, "delivery_adapter", adapter);
    json_t *rf = gwb_call(s, "ledger_finalize", a);
    json_free(a);
    json_free(rf);
    json_t *out = json_object();
    json_set(out, "result", result ? result : json_null());
    json_set(out, "adapter", adapter ? json_copy(adapter) : json_null());
    json_free(bracket);
    return out;
}

/* PoP: gwb_send_final_text @ gateway/platforms/base.py:_send_final_text */
void gwb_send_final_text(json_t *st, const json_t *event, const char *session_key,
                         const char *text_content, const json_t *metadata,
                         bool is_ephemeral_response, int ephemeral_ttl, const gwb_seams_t *s) {
    json_t *a = args0();
    aset(a, "event", event);
    json_t *anchor = gwb_call(s, "reply_anchor", a);
    json_free(a);
    json_t *sent = gwb_send_final_ledgered(st, event, session_key, text_content, metadata,
                                           anchor, is_ephemeral_response, s);
    json_free(anchor);
    const json_t *result = json_obj_get(sent, "result");
    const json_t *adapter = json_obj_get(sent, "adapter");
    a = args0();
    aset(a, "result", result);
    json_t *rr = gwb_call(s, "record_delivery", a);
    json_free(a);
    json_free(rr);
    if (ephemeral_ttl > 0 && jtruth(json_obj_get(result, "success"))
            && jtruth(json_obj_get(result, "message_id"))) {
        a = args0();
        asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
        aset(a, "message_id", json_obj_get(result, "message_id"));
        asetn(a, "ttl", (double)ephemeral_ttl);
        json_t *rd = gwb_call(s, "schedule_delete", a);
        json_free(a);
        json_free(rd);
    }
    json_free(sent);
}

/* PoP: gwb_notify_turn_error @ gateway/platforms/base.py:_notify_turn_error */
json_t *gwb_notify_turn_error(json_t *st, const json_t *event, const gwb_seams_t *s) {
    json_t *thread_meta = NULL;
    json_t *err = NULL;
    thread_meta = gwb_thread_metadata_for_event(event, s);
    json_t *scope = gwb_media_delivery_scope(st, jgetattr(event, "source"), s);
    json_free(scope);
    json_t *a = args0();
    aset(a, "event", event);
    json_t *rm = gwb_call(s, "wake_muted", a);
    json_free(a);
    if (gwb_raise_class(rm)) { err = rm; goto notify_done; }
    bool muted = jtruth(rm);
    json_free(rm);
    const char *content = NULL;
    char *visible = NULL, *hidden = NULL;
    if (muted) {
        content = NULL;
    } else {
        visible = seam_t(s, "gateway.errors.generic_failed", NULL);
        hidden = seam_t(s, "gateway.notify.turn_error", NULL);
        content = gwb_warning_text(st, visible, hidden, jgetattr(jgetattr(event, "source"), "platform"),
                                   jgetattr(jgetattr(event, "source"), "chat_id"), thread_meta, s);
    }
    if (!content) { free(visible); free(hidden); goto notify_done; }
    a = args0();
    asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
    asets(a, "content", content);
    aset(a, "metadata", thread_meta);
    json_t *rs = gwb_call(s, "send", a);
    json_free(a);
    free(visible); free(hidden);
    if (gwb_raise_class(rs)) { err = rs; goto notify_done; }
    json_free(rs);
notify_done:
    if (err) {
        char *name = adapter_name(st);
        char msg[768];
        snprintf(msg, sizeof(msg), "[%s] Failed to send error notification to user: %s",
                 name, raise_msg(err));
        seam_log(s, "error", msg);
        free(name);
        json_free(err);
    }
    return thread_meta;
}

/* uniform seam adapter for _get_human_delay inside flows. */
typedef struct { const gwb_seams_t *s; } flow_uniform_ctx_t;
static double flow_uniform_cb(void *ctx, double lo, double hi) {
    flow_uniform_ctx_t *u = ctx;
    json_t *a = args0();
    asetn(a, "lo", lo);
    asetn(a, "hi", hi);
    json_t *r = gwb_call(u->s, "uniform", a);
    json_free(a);
    double v = r && r->type == JSON_NUMBER ? r->num_val : 0.0;
    json_free(r);
    return v;
}

/* PoP: gwb_deliver_attachments @ gateway/platforms/base.py:_deliver_attachments */
void gwb_deliver_attachments(json_t *st, const json_t *event, const json_t *extracted,
                             const json_t *metadata, bool anything_sent, const gwb_seams_t *s) {
    json_t *a = args0();
    aset(a, "source", jgetattr(event, "source"));
    json_t *live = gwb_call(s, "final_adapter", a);
    json_free(a);
    json_free(live);  /* resolved transport id; base seams below run on it */
    flow_uniform_ctx_t uctx = { s };
    double human_delay = gwb_get_human_delay(st, flow_uniform_cb, &uctx);
    const json_t *images = jgetattr(extracted, "images");
    const json_t *media_files = jgetattr(extracted, "media_files");
    const json_t *local_files = jgetattr(extracted, "local_files");
    if (jtruth(images)) {
        char *name = adapter_name(st);
        char msg[256];
        snprintf(msg, sizeof(msg), "[%s] Extracted %zu image(s) to send as attachments",
                 name, json_len(images));
        seam_log(s, "info", msg);
        free(name);
        gwb_send_image_batch(st, event, images, metadata, human_delay, s);
    }
    gwb_deliver_media_attachments(st, event, media_files, local_files,
                                  json_get_bool(extracted, "force_document_attachments", false),
                                  human_delay, metadata, s);
    if (!(anything_sent || jtruth(images) || jtruth(local_files) || jtruth(media_files))) {
        char *pre = str_strip(jstr(jgetattr(extracted, "pre_extract")) ? jstr(jgetattr(extracted, "pre_extract")) : "");
        if (pre[0] != '\0') {
            char *name = adapter_name(st);
            char msg[768];
            snprintf(msg, sizeof(msg),
                     "[%s] response_delivery_dropped: non-empty response (%zu chars) produced no "
                     "delivered message or attachment for %s (empty after extract, recovery "
                     "yielded nothing).",
                     name, cp_len(jstr(jgetattr(extracted, "pre_extract")) ? jstr(jgetattr(extracted, "pre_extract")) : ""),
                     jstr(jgetattr(jgetattr(event, "source"), "chat_id")) ? jstr(jgetattr(jgetattr(event, "source"), "chat_id")) : "");
            seam_log(s, "error", msg);
            free(name);
        }
        free(pre);
    }
}

/* PoP: gwb_emit_warning @ gateway/platforms/base.py:emit_warning */
json_t *gwb_emit_warning(const json_t *st, const char *chat_id, const char *content,
                         const json_t *reply_to, const json_t *metadata,
                         const json_t *logical_platform, const gwb_seams_t *s) {
    if (!gwb_warning_notifications_enabled(st, logical_platform, s)) return NULL;
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "content", content);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    json_t *r = gwb_call(s, "send", a);
    json_free(a);
    return r;
}

/* PoP: gwb_emit_media_warning @ gateway/platforms/base.py:emit_media_warning */
json_t *gwb_emit_media_warning(const json_t *st, const char *chat_id, const char *notice,
                               const char *caption, const json_t *reply_to, const json_t *metadata,
                               const json_t *shown_metadata, const gwb_seams_t *s) {
    char *joined = NULL;
    if (caption && caption[0] != '\0') {
        size_t cap = strlen(caption) + 1 + strlen(notice ? notice : "") + 1;
        joined = malloc(cap);
        snprintf(joined, cap, "%s\n%s", caption, notice ? notice : "");
    }
    json_t *result = gwb_emit_warning(st, chat_id, joined ? joined : notice, reply_to,
                                    shown_metadata, NULL, s);
    free(joined);
    if (result) return result;
    if (caption && caption[0] != '\0') {
        json_t *a = args0();
        asets(a, "chat_id", chat_id);
        asets(a, "content", caption);
        aset(a, "reply_to", reply_to);
        aset(a, "metadata", metadata);
        json_t *r = gwb_call(s, "send", a);
        json_free(a);
        json_free(r);
    }
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "error", json_string(notice ? notice : ""));
    return out;
}

/* PoP: gwb_send_media_fallback_notice @ gateway/platforms/base.py:_send_media_fallback_notice */
json_t *gwb_send_media_fallback_notice(const json_t *st, const char *method, const char *kind,
                                       const char *path, const char *chat_id, const char *caption,
                                       const json_t *reply_to, const json_t *metadata,
                                       const char *file_name, const gwb_seams_t *s) {
    char *name = adapter_name(st);
    char msg[768];
    snprintf(msg, sizeof(msg), "[%s] %s fallback: native %s send unavailable for %s",
             name, method ? method : "", kind ? kind : "", path ? path : "");
    seam_log(s, "warning", msg);
    free(name);
    char *text = gwb_media_failure_text(kind, file_name, s);
    json_t *out = gwb_emit_media_warning(st, chat_id, text, caption, reply_to, metadata, metadata, s);
    free(text);
    return out;
}

/* PoP: gwb_send_exec_approval @ gateway/platforms/base.py:send_exec_approval */
json_t *gwb_send_exec_approval(json_t *st, const char *chat_id, const char *command,
                               const char *session_key, const json_t *description,
                               const json_t *metadata, bool allow_permanent, bool allow_session,
                               bool smart_denied, const json_t *request_id, const gwb_seams_t *s) {
    (void)st;
    char *desc;
    if (!description || description->type == JSON_NULL) {
        json_t *a = args0();
        json_t *r = gwb_call(s, "ea_default_reason", a);
        json_free(a);
        desc = jstr(r) ? strdup(jstr(r)) : strdup("");
        json_free(r);
    } else {
        desc = jpystr(description);
    }
    char *cmd = strdup(command && command[0] ? command : "");
    json_t *actions = gwb_exec_approval_actions(allow_permanent, allow_session, smart_denied, s);
    json_t *a = args0();
    asets(a, "command", cmd);
    asets(a, "description", desc);
    asetb(a, "smart_denied", smart_denied);
    asetb(a, "full_command_shown", true);
    json_t *rendered = gwb_call(s, "render", a);
    json_free(a);
    char *text = jstr(json_obj_get(rendered, "text")) ? strdup(jstr(json_obj_get(rendered, "text"))) : strdup("");
    bool truncated = jtruth(json_obj_get(rendered, "truncated"));
    a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "session_key", session_key);
    aset(a, "request_id", request_id);
    asets(a, "command", cmd);
    aset(a, "metadata", metadata);
    asetb(a, "truncated", truncated);
    json_t *whole = gwb_call(s, "card_whole", a);
    json_free(a);
    bool shows_whole = jtruth(whole);
    json_free(whole);
    if (!shows_whole) {
        a = args0();
        asets(a, "command", cmd);
        asets(a, "description", desc);
        asetb(a, "smart_denied", smart_denied);
        asetb(a, "full_command_shown", false);
        json_t *r2 = gwb_call(s, "render", a);
        json_free(a);
        free(text);
        text = jstr(json_obj_get(r2, "text")) ? strdup(jstr(json_obj_get(r2, "text"))) : strdup("");
        json_free(r2);
        json_t *deny_only = json_array();
        for (size_t i = 0; i < json_len(actions); i++) {
            const json_t *row = json_get(actions, i);
            if (strcmp(jstr(json_get(row, 1)), "deny") == 0)
                json_append(deny_only, json_copy(row));
        }
        json_free(actions);
        actions = deny_only;
    }
    json_t *prompt = json_object();
    asets(prompt, "chat_id", chat_id);
    asets(prompt, "session_key", session_key);
    aset(prompt, "metadata", metadata);
    asets(prompt, "command", cmd);
    asets(prompt, "description", desc);
    asetb(prompt, "smart_denied", smart_denied);
    aset(prompt, "request_id", request_id);
    asets(prompt, "text", text);
    json_set(prompt, "actions", actions);
    a = args0();
    aset(a, "prompt", prompt);
    json_t *result = gwb_call(s, "send_prompt", a);
    json_free(a);
    json_free(prompt);
    json_free(rendered);
    free(text); free(desc); free(cmd);
    return result;
}

/* PoP: gwb_send_clarify @ gateway/platforms/base.py:send_clarify */
json_t *gwb_send_clarify(const json_t *st, const char *chat_id, const char *question,
                         const json_t *choices, const char *clarify_id, const char *session_key,
                         const json_t *metadata, const gwb_seams_t *s) {
    (void)st; (void)session_key;
    char *text = NULL;
    if (jtruth(choices)) {
        json_t *a = args0();
        asets(a, "clarify_id", clarify_id);
        json_t *pending = gwb_call(s, "clarify_get_pending", a);
        json_free(a);
        bool multi = false;
        if (!gwb_raise_class(pending) && pending && pending->type == JSON_OBJECT)
            multi = jtruth(json_obj_get(pending, "multi_select"));
        json_free(pending);
        char *hint = seam_t(s, multi ? "gateway.clarify.hint_multi" : "gateway.clarify.hint_single", NULL);
        /* numbered option lines FIRST, then the question line (live builds the
         * join list [t(question), "", *numbered, "", hint] — question t runs
         * after the numbered comprehension). */
        size_t n = json_len(choices);
        char **numbered = malloc((n + 1) * sizeof(char *));
        size_t num_len = 0;
        for (size_t i = 0; i < n; i++) {
            const json_t *choice = json_get(choices, i);
            char *cs = jpystr(choice);
            json_t *okw = json_object();
            json_set(okw, "index", json_number((double)(i + 1)));
            json_set(okw, "choice", json_string(cs));
            numbered[i] = seam_t(s, "gateway.clarify.option_line", okw);
            json_free(okw);
            num_len += strlen(numbered[i]) + 1;
            free(cs);
        }
        json_t *kw = json_object();
        json_set(kw, "question", json_string(question ? question : ""));
        char *qline = seam_t(s, "gateway.clarify.question", kw);
        json_free(kw);
        /* "\n".join([question, "", *numbered, "", hint]) */
        size_t cap = strlen(qline) + strlen(hint) + num_len + 8;
        text = malloc(cap);
        size_t oi = 0;
        oi += (size_t)snprintf(text + oi, cap - oi, "%s\n\n", qline);
        for (size_t i = 0; i < n; i++)
            oi += (size_t)snprintf(text + oi, cap - oi, "%s\n", numbered[i]);
        snprintf(text + oi, cap - oi, "\n%s", hint);
        for (size_t i = 0; i < n; i++) free(numbered[i]);
        free(numbered);
        free(qline); free(hint);
        a = args0();
        asets(a, "clarify_id", clarify_id);
        json_t *rm = gwb_call(s, "mark_awaiting", a);
        json_free(a);
        json_free(rm);
    } else {
        json_t *kw = json_object();
        json_set(kw, "question", json_string(question ? question : ""));
        text = seam_t(s, "gateway.clarify.question", kw);
        json_free(kw);
    }
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "content", text);
    aset(a, "metadata", metadata);
    json_t *result = gwb_call(s, "send", a);
    json_free(a);
    free(text);
    return result;
}

/* PoP: gwb_build_source @ gateway/platforms/base.py:build_source */
json_t *gwb_build_source(json_t *st, const json_t *fields, const gwb_seams_t *s) {
    /* _opt(value): str(value) if value else None */
    json_t *out = json_object();
    json_set(out, "platform", json_copy(jgetattr(st, "platform")));
    char *chat_id_s = jpystr(jgetattr(fields, "chat_id"));
    json_set(out, "chat_id", json_string(chat_id_s));
    json_set(out, "chat_name", jdup(jgetattr(fields, "chat_name")));
    const json_t *ct = jgetattr(fields, "chat_type");
    json_set(out, "chat_type", json_string(jstr(ct) ? jstr(ct) : "dm"));
    const json_t *uid = jgetattr(fields, "user_id");
    if (!uid || uid->type == JSON_NULL || (uid->type == JSON_STRING && uid->str_val[0] == '\0'))
        json_set(out, "user_id", json_null());
    else {
        char *us = jpystr(uid);
        json_set(out, "user_id", json_string(us));
        free(us);
    }
    json_set(out, "user_name", jdup(jgetattr(fields, "user_name")));
    const json_t *tid = jgetattr(fields, "thread_id");
    if (jtruth(tid)) { char *ts = jpystr(tid); json_set(out, "thread_id", json_string(ts)); free(ts); }
    else json_set(out, "thread_id", json_null());
    const json_t *topic = jgetattr(fields, "chat_topic");
    char *topic_raw = jtruth(topic) ? jpystr(topic) : strdup("");
    char *topic_s = str_strip(topic_raw);
    json_set(out, "chat_topic", topic_s[0] ? json_string(topic_s) : json_null());
    free(topic_raw); free(topic_s);
    json_set(out, "user_id_alt", jdup(jgetattr(fields, "user_id_alt")));
    json_set(out, "chat_id_alt", jdup(jgetattr(fields, "chat_id_alt")));
    bool is_bot = json_get_bool(fields, "is_bot", false);
    if (!is_bot) {
        json_t *a = args0();
        aset(a, "config", jgetattr(st, "config"));
        aset(a, "user_id", uid);
        json_t *rb = gwb_call(s, "configured_bot", a);
        json_free(a);
        is_bot = jtruth(rb);
        json_free(rb);
    }
    json_set(out, "is_bot", json_bool(is_bot));
    const json_t *scope = jgetattr(fields, "scope_id");
    if (jtruth(scope)) { char *ss = jpystr(scope); json_set(out, "scope_id", json_string(ss)); free(ss); }
    else json_set(out, "scope_id", json_null());
    const json_t *guild = jgetattr(fields, "guild_id");
    if (jtruth(guild)) { char *gs = jpystr(guild); json_set(out, "guild_id", json_string(gs)); free(gs); }
    else json_set(out, "guild_id", json_null());
    const json_t *parent = jgetattr(fields, "parent_chat_id");
    if (jtruth(parent)) { char *ps = jpystr(parent); json_set(out, "parent_chat_id", json_string(ps)); free(ps); }
    else json_set(out, "parent_chat_id", json_null());
    const json_t *mid = jgetattr(fields, "message_id");
    if (jtruth(mid)) { char *ms = jpystr(mid); json_set(out, "message_id", json_string(ms)); free(ms); }
    else json_set(out, "message_id", json_null());
    /* Profile from configured routes, else the owning profile. */
    const json_t *owner_profile = jgetattr(st, "_owner_profile");
    json_t *profile = owner_profile ? json_copy(owner_profile) : json_null();
    bool rejected = false;
    json_t *rejected_profile = json_null();
    const json_t *runner = jgetattr(st, "gateway_runner");
    if (runner && runner->type != JSON_NULL) {
        json_t *a = args0();
        aset(a, "source", out);
        aset(a, "adapter_profile", owner_profile);
        json_t *rp = gwb_call(s, "profile_for_source", a);
        json_free(a);
        if (gwb_caught(rp, "ProfileRouteRejected")) {
            rejected = true;
            json_free(rejected_profile);
            rejected_profile = json_copy(json_obj_get(rp, "profile"));
        } else if (gwb_raise_class(rp)) {
            const json_t *plat = jgetattr(st, "platform");
            const char *pname = jstr(json_obj_get(plat, "name"));
            char *pn = platform_name(plat);
            char *up = strdup(pn);
            for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q -= 32;
            char msg[512];
            snprintf(msg, sizeof(msg), "Profile resolution failed for Platform.%s/%s, defaulting to active profile",
                     pname ? pname : up, chat_id_s);
            seam_log(s, "warning", msg);
            free(up); free(pn);
        } else {
            /* profile = resolved or owner_profile */
            if (jtruth(rp)) { json_free(profile); profile = json_copy(rp); }
        }
        json_free(rp);
    }
    json_set(out, "profile", profile);
    json_set(out, "role_authorized", json_bool(json_get_bool(fields, "role_authorized", false)));
    json_set(out, "auto_thread_created", json_bool(json_get_bool(fields, "auto_thread_created", false)));
    json_set(out, "auto_thread_initial_name", jdup(jgetattr(fields, "auto_thread_initial_name")));
    /* Transport-only fields (kept out of to_dict()). */
    json_set(out, "_transport_adapter_ref", json_string("self"));
    json_set(out, "profile_route_rejected", json_bool(rejected));
    json_set(out, "_rejected_route_profile", rejected_profile);
    free(chat_id_s);
    return out;
}

/* PoP: gwb_dispatch_inline_reply @ gateway/platforms/base.py:_dispatch_inline_reply */
void gwb_dispatch_inline_reply(json_t *st, const json_t *event, const json_t *log_cmd,
                               const gwb_seams_t *s) {
    (void)st;
    json_t *thread_meta = gwb_thread_metadata_for_event(event, s);
    json_t *a = args0();
    aset(a, "event", event);
    json_t *response = gwb_call(s, "message_handler", a);
    json_free(a);
    a = args0();
    aset(a, "response", response);
    json_t *unwrapped = gwb_call(s, "unwrap_ephemeral", a);
    json_free(a);
    json_free(response);
    const char *text = jstr(json_obj_get(unwrapped, "text"));
    long long ttl = (long long)json_get_num(unwrapped, "ttl", 0);
    if (!text || text[0] == '\0') {
        json_free(unwrapped);
        json_free(thread_meta);
        return;
    }
    if (log_cmd && log_cmd->type != JSON_NULL) {
        char *name = adapter_name(st);
        char msg[512];
        snprintf(msg, sizeof(msg), "[%s] Sending command '/%s' response (%zu chars) to %s",
                 name, jstr(log_cmd) ? jstr(log_cmd) : "", cp_len(text),
                 jstr(jgetattr(jgetattr(event, "source"), "chat_id")) ? jstr(jgetattr(jgetattr(event, "source"), "chat_id")) : "");
        seam_log(s, "info", msg);
        free(name);
    }
    a = args0();
    aset(a, "event", event);
    json_t *anchor = gwb_call(s, "reply_anchor", a);
    json_free(a);
    json_t *notify_meta = gwb_mark_notify_metadata(thread_meta);
    a = args0();
    asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
    asets(a, "content", text);
    aset(a, "reply_to", anchor);
    aset(a, "metadata", notify_meta);
    json_t *result = gwb_call(s, "send_with_retry", a);
    json_free(a);
    json_free(anchor);
    json_free(notify_meta);
    if (ttl > 0 && jtruth(json_obj_get(result, "success"))
            && jtruth(json_obj_get(result, "message_id"))) {
        a = args0();
        asets(a, "chat_id", jstr(jgetattr(jgetattr(event, "source"), "chat_id")));
        aset(a, "message_id", json_obj_get(result, "message_id"));
        asetn(a, "ttl", (double)ttl);
        json_t *rd = gwb_call(s, "schedule_delete", a);
        json_free(a);
        json_free(rd);
    }
    json_free(result);
    json_free(unwrapped);
    json_free(thread_meta);
}

/* ── default send delegations ── */

/* PoP: gwb_send_image @ gateway/platforms/base.py:send_image */
json_t *gwb_send_image(const json_t *st, const char *chat_id, const char *image_url,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s) {
    (void)st;
    char *text;
    if (caption && caption[0] != '\0') {
        size_t cap = strlen(caption) + 1 + strlen(image_url ? image_url : "") + 1;
        text = malloc(cap);
        snprintf(text, cap, "%s\n%s", caption, image_url ? image_url : "");
    } else {
        text = strdup(image_url ? image_url : "");
    }
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "content", text);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    json_t *r = gwb_call(s, "send", a);
    json_free(a);
    free(text);
    return r;
}

/* PoP: gwb_send_animation @ gateway/platforms/base.py:send_animation */
json_t *gwb_send_animation(const json_t *st, const char *chat_id, const char *animation_url,
                           const char *caption, const json_t *reply_to, const json_t *metadata,
                           const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "image_url", animation_url);
    asets(a, "caption", caption);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    json_t *r = gwb_call(s, "send_image", a);
    json_free(a);
    return r;
}

static json_t *media_fallback_delegate(const json_t *st, const char *method, const char *kind,
                                       const char *path, const char *chat_id, const char *caption,
                                       const json_t *reply_to, const json_t *metadata,
                                       const char *file_name, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "method", method);
    asets(a, "kind", kind);
    asets(a, "path", path);
    asets(a, "chat_id", chat_id);
    asets(a, "caption", caption);
    aset(a, "reply_to", reply_to);
    aset(a, "metadata", metadata);
    asets(a, "file_name", file_name);
    json_t *r = gwb_call(s, "fallback_notice", a);
    json_free(a);
    return r;
}

/* PoP: gwb_send_voice @ gateway/platforms/base.py:send_voice */
json_t *gwb_send_voice(const json_t *st, const char *chat_id, const char *audio_path,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s) {
    return media_fallback_delegate(st, "send_voice", "audio", audio_path, chat_id, caption,
                                   reply_to, metadata, NULL, s);
}

/* PoP: gwb_send_video @ gateway/platforms/base.py:send_video */
json_t *gwb_send_video(const json_t *st, const char *chat_id, const char *video_path,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s) {
    return media_fallback_delegate(st, "send_video", "video", video_path, chat_id, caption,
                                   reply_to, metadata, NULL, s);
}

/* PoP: gwb_send_document @ gateway/platforms/base.py:send_document */
json_t *gwb_send_document(const json_t *st, const char *chat_id, const char *file_path,
                          const char *caption, const char *file_name, const json_t *reply_to,
                          const json_t *metadata, const gwb_seams_t *s) {
    return media_fallback_delegate(st, "send_document", "file", file_path, chat_id, caption,
                                   reply_to, metadata, file_name, s);
}

/* PoP: gwb_send_image_file @ gateway/platforms/base.py:send_image_file */
json_t *gwb_send_image_file(const json_t *st, const char *chat_id, const char *image_path,
                            const char *caption, const json_t *reply_to, const json_t *metadata,
                            const gwb_seams_t *s) {
    return media_fallback_delegate(st, "send_image_file", "image", image_path, chat_id, caption,
                                   reply_to, metadata, NULL, s);
}

/* PoP: gwb_play_tts @ gateway/platforms/base.py:play_tts */
json_t *gwb_play_tts(const json_t *st, const char *chat_id, const char *audio_path,
                     const json_t *kwargs, const gwb_seams_t *s) {
    (void)st;
    json_t *a = args0();
    asets(a, "chat_id", chat_id);
    asets(a, "audio_path", audio_path);
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(a, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    json_t *r = gwb_call(s, "send_voice", a);
    json_free(a);
    return r;
}

/* PoP: gwb_send_slash_confirm @ gateway/platforms/base.py:send_slash_confirm */
json_t *gwb_send_slash_confirm(const json_t *st, const char *chat_id, const char *title,
                               const char *message, const char *session_key, const char *confirm_id,
                               const json_t *metadata) {
    (void)st; (void)chat_id; (void)title; (void)message; (void)session_key;
    (void)confirm_id; (void)metadata;
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "error", json_string("Not supported"));
    return out;
}

/* PoP: gwb_send_exec_approval_prompt_default @ gateway/platforms/base.py:_send_exec_approval_prompt */
json_t *gwb_send_exec_approval_prompt_default(const json_t *st, const json_t *prompt) {
    (void)st; (void)prompt;
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    json_set(out, "error", json_string("Not supported"));
    return out;
}
