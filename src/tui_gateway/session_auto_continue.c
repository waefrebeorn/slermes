/*
 * session_auto_continue.c - C11 port of upstream
 * tui_gateway/session_auto_continue.py (gate w31_sessionaut_4e7286).
 *
 * PoP: w31_sessionaut_4e7286_note              @ tui_gateway/session_auto_continue.py:_auto_continue_note
 * PoP: w31_sessionaut_4e7286_config            @ tui_gateway/session_auto_continue.py:_auto_continue_config
 * PoP: w31_sessionaut_4e7286_busy_mode         @ tui_gateway/session_auto_continue.py:_handle_busy_submit:mode
 * PoP: w31_sessionaut_4e7286_inflight_original @ tui_gateway/session_auto_continue.py:_ac_inflight_original
 * PoP: w31_sessionaut_4e7286_sanitize_entry    @ tui_gateway/session_auto_continue.py:_sanitize_queued_entry_vs_inflight_user
 * PoP: w31_sessionaut_4e7286_set_queue         @ tui_gateway/session_auto_continue.py:_ac_set_queue
 * PoP: w31_sessionaut_4e7286_enqueue           @ tui_gateway/session_auto_continue.py:_enqueue_prompt
 * PoP: w31_sessionaut_4e7286_inflight_snapshot @ tui_gateway/session_auto_continue.py:_inflight_snapshot
 * PoP: w31_sessionaut_4e7286_queued_snapshot   @ tui_gateway/session_auto_continue.py:_queued_prompt_snapshot
 * PoP: w31_sessionaut_4e7286_maybe_schedule    @ tui_gateway/session_auto_continue.py:_maybe_schedule_auto_continue
 *
 * The module rebinds its bodies onto server.py globals at install time, so the
 * live code reads clocks, DB handles and threads bare. Here those are seams:
 * `now` and `pid` are numbers, the durable turn marker and the loaded
 * config.yaml root are dicts, the marker-writer probe is a state string. What
 * is ported is what those values decide -- when a crash-interrupted turn is
 * resumed, what its recovery note says, how the server queue dedupes and
 * merges, and how the live-session projections are shaped.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_sessionaut_4e7286.h"

/* The note opening shared with the gateway's recovery notes. */
#define SA_NOTE_PREFIX "[System note: Your previous turn was interrupted mid-run"
#define SA_NOTE_BODY \
    " \xe2\x80\x94 the app or its backend process stopped before the turn could finish. " \
    "Some of the work may already be complete; check the current state before redoing " \
    "anything, then finish the task. The interrupted request was:]\n\n"

/* ---------------------------------------------------------------- helpers */

static char *sa_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int sa_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int sa_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int sa_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* Python truthiness of a JSON value. */
static int sa_truthy(const json_t *v) {
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

/* is_truthy_value(value, default=True) from utils/env.py. */
static int sa_truthy_default_true(const json_t *v) {
    static const char *const yes[4] = {"1", "true", "yes", "on"};
    size_t i;
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_STRING) {
        char *s = sa_dup(v->str_val ? v->str_val : "");
        char *t;
        int hit = 0;
        if (!s) return 0;
        for (t = s; *t; t++) *t = (char)tolower((unsigned char)*t);
        t = s;
        while (*t && isspace((unsigned char)*t)) t++;
        {
            size_t n = strlen(t);
            while (n && isspace((unsigned char)t[n - 1])) t[--n] = '\0';
        }
        for (i = 0; i < 4; i++) if (!strcmp(t, yes[i])) hit = 1;
        free(s);
        return hit;
    }
    return sa_truthy(v);
}

/* Python's numeric view: an int, a float or a bool; None is none of those. */
static int sa_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python str() over the scalar shapes a prompt / offset / text can hold. */
static char *sa_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return sa_dup("");
    switch (v->type) {
    case JSON_BOOL:
        return sa_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15) {
            snprintf(buf, sizeof buf, "%lld", (long long)d);
            return sa_dup(buf);
        }
        snprintf(buf, sizeof buf, "%.17g", d);
        return sa_dup(buf);
    }
    case JSON_STRING:
        return sa_dup(v->str_val ? v->str_val : "");
    default: {
        char *s = json_dumps(v, 0);
        return s ? s : sa_dup("");
    }
    }
}

/* str.strip() over ASCII whitespace (the charset every transcript uses). */
static char *sa_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* float(cfg.get("freshness_minutes", 15)) -> max(0.0, minutes) */
static double sa_minutes(const json_t *v) {
    double d;
    if (!v) return (double)W31_SESSIONAUT_4E7286_FRESHNESS_MINUTES_DEFAULT;
    if (v->type == JSON_NUMBER) d = v->num_val;
    else if (v->type == JSON_BOOL) d = v->bool_val ? 1.0 : 0.0;
    else if (v->type == JSON_STRING) {
        char *s = sa_strip(v->str_val ? v->str_val : "");
        char *end = NULL;
        if (!s || !*s) { free(s); return (double)W31_SESSIONAUT_4E7286_FRESHNESS_MINUTES_DEFAULT; }
        d = strtod(s, &end);
        if (!end || *end != '\0') { free(s); return (double)W31_SESSIONAUT_4E7286_FRESHNESS_MINUTES_DEFAULT; }
        free(s);
    } else return (double)W31_SESSIONAUT_4E7286_FRESHNESS_MINUTES_DEFAULT;
    if (d != d) return 0.0; /* float('nan'): max(0.0, nan) is 0.0 */
    return d > 0.0 ? d : 0.0;
}

/* int(value) with a floor: default on a raise or below min_value
 * (server.py:_coerce_int_config_value). */
static long long sa_int_coerce(const json_t *v, long long dflt, long long min_value) {
    long long got;
    if (!v || v->type == JSON_NULL) return dflt;
    if (v->type == JSON_BOOL) got = v->bool_val ? 1 : 0;
    else if (v->type == JSON_NUMBER) got = (long long)v->num_val; /* trunc toward zero */
    else if (v->type == JSON_STRING) {
        char *s = sa_strip(v->str_val ? v->str_val : "");
        const char *p;
        char *end = NULL;
        if (!s) return dflt;
        p = s;
        if (*p == '+' || *p == '-') p++;
        if (!*p) { free(s); return dflt; }
        for (; *p; p++) if (!isdigit((unsigned char)*p)) { free(s); return dflt; }
        got = strtoll(s, &end, 10);
        if (!end || *end != '\0') { free(s); return dflt; }
        free(s);
    } else return dflt;
    return got >= min_value ? got : dflt;
}

/* `_history_dict_text(dict, image_urls=False)`. */
static char *sa_dict_text(const json_t *d) {
    const json_t *kind = json_obj_get(d, "type");
    char *k = NULL, *out = NULL;
    if (kind) k = sa_pystr(kind);
    if (k && (!strcmp(k, "text") || !strcmp(k, "input_text") || !strcmp(k, "output_text"))) {
        const json_t *t = json_obj_get(d, "text");
        if (!sa_truthy(t)) t = json_obj_get(d, "content");
        out = (t && sa_truthy(t)) ? sa_pystr(t) : sa_dup("");
    } else if (k && (!strcmp(k, "image_url") || !strcmp(k, "input_image") || !strcmp(k, "image"))) {
        out = sa_dup("[image]");
    } else if (k && (!strcmp(k, "input_audio") || !strcmp(k, "audio"))) {
        out = sa_dup("[audio]");
    } else if (kind && sa_truthy(kind)) {
        size_t n = strlen(k) + 3;
        out = (char *)malloc(n);
        if (out) snprintf(out, n, "[%s]", k);
    } else if (json_obj_get(d, "text") != NULL) {
        const json_t *t = json_obj_get(d, "text");
        out = (t && sa_truthy(t)) ? sa_pystr(t) : sa_dup("");
    } else {
        out = sa_dup("[structured content]");
    }
    free(k);
    return out;
}

/* `_content_display_text`: each list part stripped, non-empty joined by "\n". */
static char *sa_display_text(const json_t *v) {
    if (!v) return sa_dup("");
    if (v->type == JSON_ARRAY) {
        size_t i, n = json_len(v), cap = 64, len = 0;
        char *joined = (char *)malloc(cap);
        if (!joined) return NULL;
        joined[0] = '\0';
        for (i = 0; i < n; i++) {
            char *part = sa_display_text(json_get(v, i));
            char *trimmed = part ? sa_strip(part) : NULL;
            free(part);
            if (trimmed && *trimmed) {
                size_t add = strlen(trimmed);
                size_t need = len + add + 2;
                if (need > cap) {
                    char *grown = (char *)realloc(joined, need * 2);
                    if (!grown) { free(trimmed); free(joined); return NULL; }
                    joined = grown;
                    cap = need * 2;
                }
                if (len) joined[len++] = '\n';
                memcpy(joined + len, trimmed, add);
                len += add;
                joined[len] = '\0';
            }
            free(trimmed);
        }
        return joined;
    }
    if (v->type == JSON_OBJECT) return sa_dict_text(v);
    return sa_pystr(v);
}

/* `_inflight_text`: display text, stripped. */
static char *sa_inflight_text(const json_t *v) {
    char *raw = sa_display_text(v);
    char *out = sa_strip(raw);
    free(raw);
    return out;
}

/* ------------------------------------------------------------ the pieces */

json_t *w31_sessionaut_4e7286_note(const json_t *prompt) {
    char *p = sa_pystr(prompt);
    size_t n = strlen(SA_NOTE_PREFIX) + strlen(SA_NOTE_BODY) + (p ? strlen(p) : 0) + 1;
    char *text = (char *)malloc(n);
    json_t *out;
    if (!text) { free(p); return json_null(); }
    snprintf(text, n, "%s%s%s", SA_NOTE_PREFIX, SA_NOTE_BODY, p ? p : "");
    out = json_string(text);
    free(text);
    free(p);
    return out;
}

json_t *w31_sessionaut_4e7286_config(const json_t *cfg_root) {
    const json_t *desktop, *cfg;
    double minutes;
    json_t *out = json_array();
    desktop = sa_is_obj(cfg_root) ? json_obj_get(cfg_root, "desktop") : NULL;
    cfg = sa_is_obj(desktop) ? json_obj_get(desktop, "auto_continue") : NULL;
    if (!sa_is_obj(cfg)) cfg = NULL;
    minutes = sa_minutes(cfg ? json_obj_get(cfg, "freshness_minutes") : NULL);
    json_append(out, json_bool(sa_truthy_default_true(cfg ? json_obj_get(cfg, "enabled") : NULL)));
    json_append(out, json_number(minutes * 60.0));
    json_append(out, json_number((double)sa_int_coerce(
        cfg ? json_obj_get(cfg, "max_attempts") : NULL,
        W31_SESSIONAUT_4E7286_MAX_ATTEMPTS_DEFAULT, 0)));
    return out;
}

json_t *w31_sessionaut_4e7286_busy_mode(const json_t *queued, const json_t *configured,
                                        const json_t *compression) {
    json_t *mode;
    if (sa_truthy(queued)) return json_string("queue");
    mode = configured ? json_copy(configured) : json_null();
    if (sa_truthy(compression) && sa_is_str(mode)
        && (!strcmp(mode->str_val, "steer") || !strcmp(mode->str_val, "interrupt"))) {
        json_free(mode);
        return json_string("queue");
    }
    return mode;
}

json_t *w31_sessionaut_4e7286_inflight_original(const json_t *session) {
    const json_t *turn = sa_is_obj(session) ? json_obj_get(session, "inflight_turn") : NULL;
    const json_t *user;
    char *raw, *stripped;
    json_t *out;
    if (!sa_is_obj(turn)) return json_string("");
    user = json_obj_get(turn, "user");
    raw = sa_truthy(user) ? sa_pystr(user) : sa_dup("");
    stripped = sa_strip(raw);
    free(raw);
    out = json_string(stripped ? stripped : "");
    free(stripped);
    return out;
}

/* _sanitize_queued_entry_vs_inflight_user: NULL means "drop this envelope". */
static json_t *sa_sanitize(const json_t *entry, const char *original) {
    const json_t *text;
    char *rest = NULL;
    if (!sa_is_obj(entry)) return NULL;
    text = json_obj_get(entry, "text");
    if (!original || !*original || sa_truthy(json_obj_get(entry, "image_paths"))
        || sa_truthy(json_obj_get(entry, "turn_author")) || !sa_is_str(text))
        return json_copy(entry);
    {
        const char *t = text->str_val ? text->str_val : "";
        size_t olen = strlen(original);
        static const char *const seps[2] = {"\n\n", "\n"};
        char *cand = NULL;
        size_t i;
        for (i = 0; i < 2 && !cand; i++) {
            size_t slen = strlen(seps[i]);
            if (!strncmp(t, original, olen) && !strncmp(t + olen, seps[i], slen))
                cand = sa_dup(t + olen + slen);
        }
        if (!cand) cand = sa_dup(t);
        rest = sa_strip(cand);
        free(cand);
    }
    if (!rest || !*rest || !strcmp(rest, original)) { free(rest); return NULL; }
    {
        char *plain = sa_strip(text->str_val ? text->str_val : "");
        int unchanged = plain && !strcmp(rest, plain);
        free(plain);
        if (unchanged) { free(rest); return json_copy(entry); }
    }
    {
        json_t *out = json_copy(entry);
        json_set(out, "text", json_string(rest));
        free(rest);
        return out;
    }
}

json_t *w31_sessionaut_4e7286_sanitize_entry(const json_t *entry, const json_t *original) {
    const char *orig = sa_is_str(original) ? (original->str_val ? original->str_val : "") : "";
    json_t *out = sa_sanitize(entry, orig);
    return out ? out : json_null();
}

json_t *w31_sessionaut_4e7286_set_queue(const json_t *session, const json_t *entries) {
    json_t *s = sa_is_obj(session) ? json_copy(session) : json_object();
    size_t n = sa_is_arr(entries) ? json_len(entries) : 0;
    json_set(s, "queued_prompt", n ? json_copy(json_get(entries, 0)) : json_null());
    if (n > 1) {
        json_t *rest = json_array();
        size_t i;
        for (i = 1; i < n; i++) json_append(rest, json_copy(json_get(entries, i)));
        json_set(s, "queued_prompts", rest);
    } else {
        json_obj_del(s, "queued_prompts");
    }
    return s;
}

/* _drop_queued_duplicates_of_inflight_user, in place on an owned session. */
static void sa_drop_dupes(json_t *session) {
    json_t *origj = w31_sessionaut_4e7286_inflight_original(session);
    const char *orig = origj->str_val ? origj->str_val : "";
    const json_t *head, *rest;
    json_t *kept;
    if (!*orig) { json_free(origj); return; }
    kept = json_array();
    head = json_obj_get(session, "queued_prompt");
    if (sa_truthy(head)) {
        json_t *c = sa_sanitize(head, orig);
        if (c) json_append(kept, c);
    }
    rest = json_obj_get(session, "queued_prompts");
    if (sa_is_arr(rest)) {
        size_t i;
        for (i = 0; i < json_len(rest); i++) {
            json_t *c = sa_sanitize(json_get(rest, i), orig);
            if (c) json_append(kept, c);
        }
    }
    {
        size_t n = json_len(kept);
        json_set(session, "queued_prompt", n ? json_copy(json_get(kept, 0)) : json_null());
        if (n > 1) {
            json_t *copy = json_array();
            size_t i;
            for (i = 1; i < n; i++) json_append(copy, json_copy(json_get(kept, i)));
            json_set(session, "queued_prompts", copy);
        } else {
            json_obj_del(session, "queued_prompts");
        }
        json_free(kept);
    }
    json_free(origj);
}

json_t *w31_sessionaut_4e7286_enqueue(const json_t *session, const json_t *text,
                                      const json_t *transport, const json_t *image_paths,
                                      const json_t *turn_author) {
    json_t *s = sa_is_obj(session) ? json_copy(session) : json_object();
    json_t *out = json_object();
    json_t *queued;
    json_t *existing;
    int has_images = sa_is_arr(image_paths) && json_len(image_paths) > 0;
    int text_only;
    sa_drop_dupes(s);
    text_only = !has_images && sa_is_str(text);
    if (text_only && !sa_truthy(turn_author)) {
        json_t *origj = w31_sessionaut_4e7286_inflight_original(s);
        const char *orig = origj->str_val ? origj->str_val : "";
        char *plain = sa_strip(text->str_val ? text->str_val : "");
        int self_copy = *orig && plain && !strcmp(plain, orig);
        free(plain);
        json_free(origj);
        if (self_copy) {
            json_set(out, "envelope", json_null());
            json_set(out, "session", s);
            return out;
        }
    }
    queued = json_object();
    json_set(queued, "text", text ? json_copy(text) : json_null());
    json_set(queued, "transport", transport ? json_copy(transport) : json_null());
    if (has_images) json_set(queued, "image_paths", json_copy(image_paths));
    if (sa_truthy(turn_author)) json_set(queued, "turn_author", json_copy(turn_author));
    existing = json_obj_get(s, "queued_prompt");
    if (sa_truthy(existing) && text_only && !sa_truthy(turn_author)
        && sa_is_str(json_obj_get(existing, "text"))
        && !sa_truthy(json_obj_get(existing, "image_paths"))
        && !sa_truthy(json_obj_get(existing, "turn_author"))
        && !sa_truthy(json_obj_get(s, "queued_prompts"))) {
        const char *prev = json_obj_get(existing, "text")->str_val;
        const char *add = text->str_val ? text->str_val : "";
        char *merged;
        size_t n;
        if (!prev) prev = "";
        n = strlen(prev) + strlen(add) + 3;
        merged = (char *)malloc(n);
        if (merged) {
            if (*prev && *add) snprintf(merged, n, "%s\n\n%s", prev, add);
            else snprintf(merged, n, "%s", *prev ? prev : add);
            json_set(existing, "text", json_string(merged));
            free(merged);
        }
        json_free(queued);
        json_set(out, "envelope", json_copy(existing));
        json_set(out, "session", s);
        return out;
    }
    if (sa_truthy(existing)) {
        json_t *arr = json_obj_get(s, "queued_prompts");
        if (!sa_is_arr(arr)) {
            arr = json_array();
            json_set(s, "queued_prompts", arr);
        }
        json_append(arr, queued);
    } else {
        json_set(s, "queued_prompt", queued);
    }
    json_set(out, "envelope", json_copy(queued));
    json_set(out, "session", s);
    return out;
}

json_t *w31_sessionaut_4e7286_inflight_snapshot(const json_t *session) {
    const json_t *turn = sa_is_obj(session) ? json_obj_get(session, "inflight_turn") : NULL;
    const json_t *dk, *dm, *corr, *offs, *surface, *status;
    json_t *snap;
    char *user, *assistant, *error;
    int streaming;
    if (!sa_is_obj(turn)) return json_null();
    {
        const json_t *u = json_obj_get(turn, "user");
        char *raw = sa_truthy(u) ? sa_pystr(u) : sa_dup("");
        user = sa_strip(raw);
        free(raw);
    }
    {
        const json_t *a = json_obj_get(turn, "assistant");
        assistant = sa_truthy(a) ? sa_pystr(a) : sa_dup("");
    }
    streaming = sa_truthy(json_obj_get(turn, "streaming"));
    {
        const json_t *e = json_obj_get(turn, "error");
        char *raw = sa_truthy(e) ? sa_pystr(e) : sa_dup("");
        error = sa_strip(raw);
        free(raw);
    }
    if (!*user && !*assistant && !streaming && !*error) {
        free(user); free(assistant); free(error);
        return json_null();
    }
    snap = json_object();
    json_set(snap, "assistant", json_string(assistant));
    json_set(snap, "streaming", json_bool(streaming));
    json_set(snap, "user", json_string(user));
    dk = json_obj_get(turn, "display_kind");
    if (sa_is_str(dk) && dk->str_val && dk->str_val[0]) json_set(snap, "display_kind", json_copy(dk));
    dm = json_obj_get(turn, "display_metadata");
    if (sa_is_obj(dm)) json_set(snap, "display_metadata", json_copy(dm));
    corr = json_obj_get(turn, "corrections");
    offs = json_obj_get(turn, "correction_offsets");
    if (sa_is_arr(corr)) {
        json_t *cvals = json_array(), *ovals = json_array();
        int all_off = 1;
        size_t i;
        for (i = 0; i < json_len(corr); i++) {
            char *cs = sa_pystr(json_get(corr, i));
            char *trimmed = sa_strip(cs);
            if (trimmed && *trimmed) {
                const json_t *o = (sa_is_arr(offs) && i < json_len(offs)) ? json_get(offs, i) : NULL;
                json_append(cvals, json_string(cs));
                if (o && o->type == JSON_BOOL) {
                    json_append(ovals, json_number(o->bool_val ? 1.0 : 0.0));
                } else if (o && o->type == JSON_NUMBER
                           && o->num_val >= 0.0 && o->num_val == (double)(long long)o->num_val) {
                    json_append(ovals, json_number((double)(long long)o->num_val));
                } else {
                    all_off = 0;
                }
            }
            free(trimmed);
            free(cs);
        }
        if (json_len(cvals)) {
            json_set(snap, "corrections", cvals);
            if (all_off && json_len(ovals) == json_len(cvals)) json_set(snap, "correction_offsets", ovals);
            else json_free(ovals);
        } else {
            json_free(cvals);
            json_free(ovals);
        }
    }
    if (*error) {
        json_set(snap, "error", json_string(error));
        status = json_obj_get(turn, "status");
        {
            char *st = (status && sa_truthy(status)) ? sa_pystr(status) : sa_dup("error");
            json_set(snap, "status", json_string(st ? st : "error"));
            free(st);
        }
        json_set(snap, "recoverable", json_bool(sa_truthy(json_obj_get(turn, "recoverable"))));
        surface = json_obj_get(turn, "error_surface");
        if (sa_is_obj(surface) && json_object_size(surface) > 0)
            json_set(snap, "error_surface", json_copy(surface));
    }
    free(user); free(assistant); free(error);
    return snap;
}

json_t *w31_sessionaut_4e7286_queued_snapshot(const json_t *session) {
    const json_t *queued = sa_is_obj(session) ? json_obj_get(session, "queued_prompt") : NULL;
    char *user;
    json_t *out;
    if (!sa_is_obj(queued)) return json_null();
    user = sa_inflight_text(json_obj_get(queued, "text"));
    if (!user || !*user) { free(user); return json_null(); }
    out = json_object();
    json_set(out, "user", json_string(user));
    free(user);
    return out;
}

json_t *w31_sessionaut_4e7286_maybe_schedule(const json_t *session, const json_t *marker,
                                             const json_t *cfg_root, const json_t *writer_state,
                                             const json_t *now, const json_t *pid) {
    json_t *res = json_object();
    json_t *out = NULL;
    int cleared = 0, scheduled = 0;
    const json_t *src, *flag;
    if (!sa_is_obj(marker)) goto done;
    src = sa_is_obj(session) ? json_obj_get(session, "source") : NULL;
    if (sa_is_str(src) && !strcmp(src->str_val, "bot_room")) goto done;
    flag = json_obj_get(marker, "auto_continue");
    if (flag && !sa_truthy(flag)) goto done; /* the mailbox owns this turn's recovery */
    if (sa_is_str(writer_state) && !strcmp(writer_state->str_val, "alive")) {
        double wpid, mpid;
        int same = sa_number(json_obj_get(marker, "writer_pid"), &wpid)
                   && sa_number(pid, &mpid) && wpid == mpid;
        if (!same) goto done; /* ownership evidence: the live writer clears it */
    }
    {
        json_t *cfg = w31_sessionaut_4e7286_config(cfg_root);
        int enabled = sa_truthy(json_get(cfg, 0));
        double fresh = json_get(cfg, 1) ? json_get(cfg, 1)->num_val : 0.0;
        double maxa = json_get(cfg, 2) ? json_get(cfg, 2)->num_val : 0.0;
        double started = 0.0, nowd = 0.0, attempts = 0.0, age;
        json_free(cfg);
        sa_number(json_obj_get(marker, "started_at"), &started);
        sa_number(now, &nowd);
        sa_number(json_obj_get(marker, "attempts"), &attempts);
        age = nowd - started;
        if (!enabled || age > fresh || attempts >= maxa) { cleared = 1; goto done; }
        if (sa_truthy(json_obj_get(session, "_auto_continue_scheduled"))) goto done;
        scheduled = 1;
        out = json_object();
        json_set(out, "attempt", json_number(attempts + 1.0));
        json_set(out, "interrupted_at", json_number(started));
    }
done:
    json_set(res, "out", out ? out : json_null());
    json_set(res, "cleared", json_bool(cleared));
    json_set(res, "scheduled", json_bool(scheduled));
    return res;
}
