/*
 * turn_progress_flow.c — progress-reporting FLOW cores of
 * gateway/run_turn_progress.py (module leaves + GatewayTurnProgressMixin).
 *
 * Ports the DECISION sequences: flood-window arming and boundary comparisons,
 * owed-send/owed-tail retry ladders, partial-delivery settlement, the dedup
 * coalesce + native/queue routing of _progress_emit, bubble reset carry-over,
 * the send/edit transient-vs-permanent-vs-flood branches, overflow roll
 * ordering (owed tail before edit before sends), the idle flood-pending tick,
 * the progress_callback emission gate chain (subagent notice first, log mode,
 * thinking opt-in, native cards, interrupt gate, "new" mode dedup), the
 * build_message rendering branches (verbose queue-direct, terminal fences,
 * verb/preview composition) and the status_callback projection branches
 * (mute/live/bot-peer/prepared gates, local-viewer vs schedule+cleanup lane).
 *
 * Modeling contract (shared with tests/oracle_turn_progress_flow.py):
 *  - State mirror sections: st (_ProgressEditState fields), ctx (TurnContext
 *    fields), adapters {id: fields}, consumers {id: fields}. Adapter/consumer
 *    references are ID STRINGS; the oracle keeps per-id doubles so attribute
 *    reads and in-place mutations agree.
 *  - list[0] holder cells (last_tool, repeat_count, last_progress_msg,
 *    long_tool_hint_fired, last_was_terminal_block) are plain scalar ctx
 *    fields; the oracle wraps them into 1-element lists for the live call.
 *  - Queues are {"items": [...]} objects; presence is an IDENTITY test (a live
 *    queue.Queue is truthy even when empty); put appends to items. Tuples
 *    model as JSON arrays ("__dedup__"/"__reset__" markers).
 *  - st.owed_tail is a result-fields object or null; owed_bubbles is a list of
 *    [message_id, text] pairs. Result objects carry success/message_id/error/
 *    retry_after/retryable/raw_response; raw_response stays a plain dict.
 *  - _progress_len_fn is the "len_fn" seam; BasePlatformAdapter.truncate_message
 *    and ._is_partial_delivery are seams; datetime.now().strftime is the
 *    "now_str" seam (returns the formatted stamp); _schedule is the "schedule"
 *    seam with the loop resolved; render_notification records then ALWAYS
 *    invokes present() (a scripted raise fires BEFORE present); futures are id
 *    strings whose add_done_callback is a seam.
 *  - t(key, **kw) returns key (i18n identity, unrecorded): the decision chain
 *    is observable through WHICH key is queued/returned.
 *  - Unported pure collaborators (_progress_text, _cap_sized_pieces,
 *    _split_progress_groups, _preview_cap, _progress_terminal_blocks,
 *    _send_progress_text, _edit_progress_message, _track_progress_result,
 *    _status_live) run REAL on both sides; the tpf_*_core transcriptions below
 *    call the identical seams in the identical order. The piece generator in
 *    _split_progress_groups is LAZY: per-item piece checks interleave with
 *    per-piece join checks (len_fn script order follows that interleaving).
 *  - Scripted seam results: NULL == None; {"__raise__": kind, "message": m}
 *    == a raised exception. BaseException kinds (SystemExit, KeyboardInterrupt,
 *    GeneratorExit, CancelledError) pass through every `except Exception` site
 *    via env->exc; Exception kinds are swallowed exactly where live swallows.
 *  - Clock seams return floats; a null clock result raises the live TypeError
 *    (operand type names: number -> "float", null -> "NoneType").
 *  - Self-contained: the harness #includes this .c; every symbol is static.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <math.h>
#include "gateway_lifecycle_flow.h"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct tpf_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} tpf_env;

/* Universal seam call: args BORROWED by the stub (it copies into the trace),
 * result NEW (caller frees). NULL result == Python None. */
static json_t *tpf_call(tpf_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

static bool tpf_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && gflow_is_str(json_obj_get(r, "__raise__"));
}
static const char *tpf_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *tpf_msg(const json_t *r) {
    return json_get_str(r, "message", "");
}
static bool tpf_is_base(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}
/* Park a raise spec (owned) as the pending exception. */
static void tpf_park(tpf_env *e, json_t *spec) {
    if (e->exc) json_free(e->exc);
    e->exc = spec;
}
static json_t *tpf_raise_spec(const char *kind, const char *msg) {
    json_t *s = json_object();
    json_set(s, "__raise__", json_string(kind));
    json_set(s, "message", json_string(msg ? msg : ""));
    return s;
}

/* ── state mirror access (borrowed pointers) ─────────────────────────────── */
static json_t *tpf_st(tpf_env *e) { return json_obj_get(e->state, "st"); }
static json_t *tpf_ctx(tpf_env *e) { return json_obj_get(e->state, "ctx"); }
static json_t *tpf_adapter(tpf_env *e, const char *id) {
    return (id && id[0]) ? json_obj_get(json_obj_get(e->state, "adapters"), id) : NULL;
}
static json_t *tpf_consumer(tpf_env *e, const char *id) {
    return (id && id[0]) ? json_obj_get(json_obj_get(e->state, "consumers"), id) : NULL;
}
static json_t *tpf_st_adapter(tpf_env *e, json_t *st) {
    const char *id = json_get_str(st, "adapter", NULL);
    return tpf_adapter(e, id);
}
static const char *tpf_chat_id(tpf_env *e) {
    json_t *src = json_obj_get(tpf_ctx(e), "source");
    return json_get_str(src, "chat_id", "");
}
/* Queue presence is identity (live queue.Queue is always truthy). */
static bool tpf_queue_present(json_t *ctx, const char *key) {
    json_t *q = json_obj_get(ctx, key);
    return q && q->type != JSON_NULL;
}
static void tpf_queue_put(tpf_env *e, const char *key, json_t *item) {
    json_t *q = json_obj_get(tpf_ctx(e), key);
    if (!q || q->type == JSON_NULL) {
        tpf_park(e, tpf_raise_spec("AttributeError",
                                   "'NoneType' object has no attribute 'put'"));
        json_free(item);
        return;
    }
    json_t *items = json_obj_get(q, "items");
    if (items && items->type == JSON_ARRAY) json_append(items, item);
    else json_free(item);
}

/* ── unicode string helpers ──────────────────────────────────────────────── */
static unsigned long tpf_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80) { cp = u[0]; n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { cp = u[0]; n = 1; }
    for (size_t i = 1; i < n && u[i]; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
/* Python str.strip() White_Space set. */
static bool tpf_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || (cp >= 0x1C && cp <= 0x1F)
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
static size_t tpf_cplen(const char *s) {
    size_t n = 0, i = 0;
    while (s && s[i]) { size_t adv; tpf_cp(s + i, &adv); i += adv; n++; }
    return n;
}
/* Python s[:stop] with negative-stop semantics; malloc'd. */
static char *tpf_slice(const char *s, long stop) {
    if (!s) return strdup("");
    size_t len = tpf_cplen(s);
    long st = stop;
    if (st < 0) st = (long)len + st;
    if (st < 0) st = 0;
    if ((size_t)st > len) st = (long)len;
    size_t i = 0; long k = 0;
    while (s[i] && k < st) { size_t adv; tpf_cp(s + i, &adv); i += adv; k++; }
    char *r = (char *)malloc(i + 1);
    if (!r) return strdup("");
    memcpy(r, s, i);
    r[i] = '\0';
    return r;
}
/* Right-strip the full Unicode White_Space set (str.rstrip()). */
static char *tpf_rstrip_ws(const char *s) {
    if (!s) return strdup("");
    size_t end = 0, i = 0;
    bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = tpf_cp(s + i, &adv);
        if (!tpf_ws(cp)) { end = i + adv; any = true; }
        i += adv;
    }
    if (!any) end = 0;
    char *r = (char *)malloc(end + 1);
    if (!r) return strdup("");
    memcpy(r, s, end);
    r[end] = '\0';
    return r;
}
/* Both-ends strip (str.strip()) truthiness probe. */
static bool tpf_strip_nonempty(const char *s) {
    if (!s) return false;
    size_t i = 0, len = strlen(s);
    while (s[i]) {
        size_t adv; unsigned long cp = tpf_cp(s + i, &adv);
        if (!tpf_ws(cp)) break;
        i += adv;
    }
    if (i >= len) return false;
    size_t j = len;
    while (j > i) {
        /* decode the codepoint ending at j */
        size_t start = j - 1;
        while (start > i && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
        size_t adv; unsigned long cp = tpf_cp(s + start, &adv);
        if (!tpf_ws(cp)) break;
        j = start;
    }
    return j > i;
}
/* rstrip("\r\n"): strip trailing chars in the set {\r, \n}. */
static char *tpf_rstrip_crlf(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n')) len--;
    char *r = (char *)malloc(len + 1);
    if (!r) return strdup("");
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool tpf_endswith_crlf(const char *s) {
    size_t len = strlen(s);
    return len >= 2 && s[len - 2] == '\r' && s[len - 1] == '\n';
}
static char *tpf_lower(const char *s) {
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
/* str.splitlines() boundaries: \n \r \r\n \v \f \x1c \x1d \x1e \x85 2028 2029.
 * Returns a malloc'd array of malloc'd lines; *n gets the count. */
static char **tpf_splitlines(const char *s, size_t *n) {
    *n = 0;
    char **out = NULL;
    size_t cap = 0, start = 0, i = 0;
    if (!s) s = "";
    while (s[i]) {
        size_t adv; unsigned long cp = tpf_cp(s + i, &adv);
        bool brk = cp == '\n' || cp == '\r' || cp == 0x0B || cp == 0x0C
                || (cp >= 0x1C && cp <= 0x1E) || cp == 0x85
                || cp == 0x2028 || cp == 0x2029;
        if (brk) {
            size_t end = i;
            if (cp == '\r' && s[i + 1] == '\n') adv = 2;
            if (*n == cap) { cap = cap ? cap * 2 : 8;
                out = (char **)realloc(out, cap * sizeof(char *)); }
            out[(*n)++] = strndup(s + start, end - start);
            i += adv;
            start = i;
        } else {
            i += adv;
        }
    }
    if (start < i || *n == 0) {
        /* trailing segment (splitlines drops a trailing empty segment only
         * when the string ends with a boundary; a non-empty tail is a line) */
        if (start < strlen(s)) {
            if (*n == cap) { cap = cap ? cap * 2 : 8;
                out = (char **)realloc(out, cap * sizeof(char *)); }
            out[(*n)++] = strdup(s + start);
        }
    }
    return out;
}
static void tpf_free_lines(char **lines, size_t n) {
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}
/* Closing-fence probe: (?:^|\n)[ \t]*```[ \t]*\r?\Z on an already
 * \r\n-rstripped body == the LAST line is [ \t]*```[ \t]*. */
static bool tpf_fence_match(const char *body) {
    const char *nl = strrchr(body, '\n');
    const char *p = nl ? nl + 1 : body;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "```", 3) != 0) return false;
    p += 3;
    while (*p == ' ' || *p == '\t') p++;
    return *p == '\0';
}
/* re.sub(r" \(\d+/\d+\)$", "", chunk): strip a trailing " (N/N)" suffix. */
static char *tpf_strip_chunk_suffix(const char *chunk) {
    size_t len = strlen(chunk);
    if (len == 0 || chunk[len - 1] != ')') return strdup(chunk);
    size_t i = len - 1;
    while (i > 0 && chunk[i - 1] >= '0' && chunk[i - 1] <= '9') i--;
    size_t d2 = i;               /* second digit run start */
    if (d2 == len - 1) return strdup(chunk);
    if (i == 0 || chunk[i - 1] != '/') return strdup(chunk);
    i--;                         /* at '/' */
    size_t slash = i;
    while (i > 0 && chunk[i - 1] >= '0' && chunk[i - 1] <= '9') i--;
    size_t d1 = i;
    if (d1 == slash) return strdup(chunk);
    if (i == 0 || chunk[i - 1] != '(') return strdup(chunk);
    i--;
    if (i == 0 || chunk[i - 1] != ' ') return strdup(chunk);
    return strndup(chunk, i - 1);
}
/* Integral-aware number str (Python str(int) / str(float) for fixtures). */
static void tpf_num_str(double d, char *buf, size_t n) {
    if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
        snprintf(buf, n, "%lld", (long long)d);
    else
        snprintf(buf, n, "%.17g", d);
}
/* Python float() string coercion (whitespace, sign, underscores between
 * digits, inf/nan). *ok=false models the swallowed TypeError/ValueError. */
static bool tpf_py_float(const char *s, double *out) {
    *out = 0.0;
    if (!s) return false;
    const char *p = s;
    while (*p && tpf_ws((unsigned char)*p)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    /* inf / infinity / nan (case-insensitive) */
    char word[16]; size_t wn = 0;
    const char *q = p;
    while (isalpha((unsigned char)q[wn]) && wn < 15) {
        word[wn] = (char)tolower((unsigned char)q[wn]); wn++;
    }
    word[wn] = '\0';
    if (strcmp(word, "inf") == 0 || strcmp(word, "infinity") == 0
        || strcmp(word, "nan") == 0) {
        const char *end = p + wn;
        while (*end && tpf_ws((unsigned char)*end)) end++;
        if (*end) return false;
        bool neg = (p > start && start[0] == '-');
        *out = strcmp(word, "nan") == 0 ? NAN : (neg ? -INFINITY : INFINITY);
        return true;
    }
    /* numeric: digits with underscores between digits, optional . and exp */
    const char *d = p;
    bool any_digit = false, prev_digit = false;
    while (*d) {
        if (*d >= '0' && *d <= '9') { any_digit = true; prev_digit = true; d++; }
        else if (*d == '_') {
            if (!prev_digit || !(d[1] >= '0' && d[1] <= '9')) return false;
            prev_digit = false; d++;
        } else break;
    }
    if (*d == '.') {
        d++;
        prev_digit = false;   /* a leading '_' in the fraction is invalid */
        while (*d) {
            if (*d >= '0' && *d <= '9') { any_digit = true; prev_digit = true; d++; }
            else if (*d == '_') {
                if (!prev_digit || !(d[1] >= '0' && d[1] <= '9')) return false;
                prev_digit = false; d++;
            } else break;
        }
    }
    if (!any_digit) return false;
    if (*d == 'e' || *d == 'E') {
        d++;
        if (*d == '+' || *d == '-') d++;
        bool exp_digit = false; prev_digit = false;
        while (*d) {
            if (*d >= '0' && *d <= '9') { exp_digit = true; prev_digit = true; d++; }
            else if (*d == '_') {
                if (!prev_digit || !(d[1] >= '0' && d[1] <= '9')) return false;
                prev_digit = false; d++;
            } else break;
        }
        if (!exp_digit) return false;
    }
    while (*d && tpf_ws((unsigned char)*d)) d++;
    if (*d) return false;
    /* strip underscores for strtod */
    size_t n = (size_t)(d - start);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return false;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) if (start[i] != '_') buf[k++] = start[i];
    buf[k] = '\0';
    char *endp = NULL;
    double v = strtod(buf, &endp);
    bool ok = endp && *endp == '\0' && k > 0;
    free(buf);
    if (ok) *out = v;
    return ok;
}
/* Type names for TypeError messages. Clock operands are floats by contract;
 * attribute-error subjects use the integral-aware py type. */
static const char *tpf_clock_type(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_NUMBER) return "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
static const char *tpf_py_type(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_NUMBER)
        return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}

/* ── log seam (raw fmt + args; stub emits {ev:"log",level,fmt,args}) ─────── */
static void tpf_log(tpf_env *e, int level, const char *fmt, json_t *args) {
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args);   /* adopts */
    json_free(tpf_call(e, "log", a));
}
static json_t *tpf_logargs1(const char *s) {
    json_t *a = json_array();
    json_append(a, json_string(s ? s : ""));
    return a;
}

/* ── container ops ───────────────────────────────────────────────────────── */
static json_t *tpf_arr_pop(json_t *arr, size_t idx) {   /* detaches; caller owns */
    if (!arr || arr->type != JSON_ARRAY || idx >= arr->c.count) return NULL;
    json_t *it = arr->c.items[idx];
    for (size_t j = idx; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
    arr->c.count--;
    return it;
}
static void tpf_arr_set_last(json_t *arr, json_t *item) {   /* adopts item */
    if (!arr || arr->type != JSON_ARRAY || arr->c.count == 0) { json_free(item); return; }
    size_t last = arr->c.count - 1;
    json_free(arr->c.items[last]);
    arr->c.items[last] = item;
}
/* "\n".join(lines) — lines are strings in the editable-bubble model. */
static char *tpf_join(json_t *lines, const char *sep) {
    size_t total = 1, slen = strlen(sep);
    size_t n = (lines && lines->type == JSON_ARRAY) ? lines->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = gflow_str(json_get(lines, i));
        total += strlen(s ? s : "") + (i ? slen : 0);
    }
    char *r = (char *)malloc(total);
    if (!r) return strdup("");
    r[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (i) strcat(r, sep);
        const char *s = gflow_str(json_get(lines, i));
        strcat(r, s ? s : "");
    }
    return r;
}
/* _progress_text: "\n".join(str(line) for line in lines) — strings model. */
static char *tpf_progress_text(json_t *lines) { return tpf_join(lines, "\n"); }
/* Flatten groups[k:] into a NEW line array (copies). */
static json_t *tpf_flatten_groups(json_t *groups, size_t k) {
    json_t *out = json_array();
    size_t n = groups ? json_len(groups) : 0;
    for (size_t i = k; i < n; i++) {
        json_t *g = json_get(groups, i);
        for (size_t j = 0; j < json_len(g); j++)
            json_append(out, json_copy(json_get(g, j)));
    }
    return out;
}

/* ── shared seam probes ──────────────────────────────────────────────────── */
static json_t *tpf_monotonic(tpf_env *e) {
    return tpf_call(e, "monotonic", json_object());
}
static json_t *tpf_run_still_current(tpf_env *e) {
    return tpf_call(e, "run_still_current", json_object());
}
static json_t *tpf_agent_interrupted(tpf_env *e) {
    return tpf_call(e, "agent_interrupted", json_object());
}
/* result.success direct attribute read (None result raises AttributeError). */
static bool tpf_attr_success(tpf_env *e, const json_t *result) {
    if (!result || result->type == JSON_NULL) {
        tpf_park(e, tpf_raise_spec("AttributeError",
                                   "'NoneType' object has no attribute 'success'"));
        return false;
    }
    return gflow_truth(json_obj_get(result, "success"));
}

/* ── internal real-sibling transcriptions (no PoP claim) ─────────────────── */
/* _track_progress_result: remember a delivered message id for cleanup. */
static void tpf_track_progress_result(tpf_env *e, const json_t *result) {
    json_t *ctx = tpf_ctx(e);
    if (!gflow_truth(json_obj_get(ctx, "_cleanup_progress"))) return;
    if (!gflow_truth(json_obj_get(result, "success"))) return;
    json_t *mid = json_obj_get(result, "message_id");
    if (!gflow_truth(mid)) return;
    char *s = gflow_pystr(mid);
    json_t *ids = json_obj_get(ctx, "_cleanup_msg_ids");
    if (ids && ids->type == JSON_ARRAY) json_append(ids, json_string(s));
    free(s);
}
/* _status_live: bool(ctx._status_adapter) and ctx._run_still_current(). */
static bool tpf_status_live(tpf_env *e) {
    json_t *ctx = tpf_ctx(e);
    json_t *ad = json_obj_get(ctx, "_status_adapter");
    if (!ad || ad->type == JSON_NULL) return false;
    json_t *r = tpf_run_still_current(e);
    if (tpf_is_raise(r)) { tpf_park(e, r); return false; }
    bool cur = gflow_truth(r);
    json_free(r);
    return cur;
}
/* _preview_cap: pl if pl > 0 else 40. */
static double tpf_preview_cap(tpf_env *e) {
    json_t *r = tpf_call(e, "preview_max_len", json_object());
    if (tpf_is_raise(r)) { tpf_park(e, r); return 0; }
    double pl = (r && r->type == JSON_NUMBER) ? r->num_val : 0;
    json_free(r);
    return pl > 0 ? pl : 40;
}
/* get_tool_preview_max_len() RAW (verbose args text uses it unclamped). */
static double tpf_preview_max_len_raw(tpf_env *e) {
    json_t *r = tpf_call(e, "preview_max_len", json_object());
    if (tpf_is_raise(r)) { tpf_park(e, r); return 0; }
    double pl = (r && r->type == JSON_NUMBER) ? r->num_val : 0;
    json_free(r);
    return pl;
}
/* st._progress_len_fn(text) seam. */
static double tpf_len_fn(tpf_env *e, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_t *r = tpf_call(e, "len_fn", a);
    if (tpf_is_raise(r)) { tpf_park(e, r); return 0; }
    double v = (r && r->type == JSON_NUMBER) ? r->num_val : 0;
    json_free(r);
    return v;
}
/* _cap_sized_pieces: one item as cap-fitting pieces (fences closed/reopened
 * by the adapter seam); the " (N/N)" continuation suffix is stripped. */
static json_t *tpf_cap_sized_pieces(tpf_env *e, json_t *st, json_t *line) {
    json_t *out = json_array();
    char *text = gflow_pystr(line);
    double limit = json_get_num(st, "_PROGRESS_TEXT_LIMIT", 0);
    double l = tpf_len_fn(e, text);
    if (e->exc) { free(text); return out; }
    if (l <= limit) {
        json_append(out, json_copy(line));
        free(text);
        return out;
    }
    json_t *a = json_object();
    json_set(a, "text", json_string(text));
    json_set(a, "limit", json_number(limit));
    json_t *r = tpf_call(e, "truncate_message", a);
    free(text);
    if (tpf_is_raise(r)) { tpf_park(e, r); return out; }
    if (r && r->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(r); i++) {
            const char *chunk = gflow_str(json_get(r, i));
            char *stripped = tpf_strip_chunk_suffix(chunk ? chunk : "");
            json_append(out, json_string(stripped));
            free(stripped);
        }
    }
    json_free(r);
    return out;
}
/* _split_progress_groups: LAZY piece generator — per-item piece checks
 * interleave with per-piece join checks exactly like the live generator. */
static json_t *tpf_split_progress_groups(tpf_env *e, json_t *st, json_t *lines) {
    json_t *groups = json_array();
    json_t *current = json_array();
    double limit = json_get_num(st, "_PROGRESS_TEXT_LIMIT", 0);
    size_t n = (lines && lines->type == JSON_ARRAY) ? json_len(lines) : 0;
    for (size_t i = 0; i < n && !e->exc; i++) {
        json_t *pieces = tpf_cap_sized_pieces(e, st, json_get(lines, i));
        for (size_t j = 0; j < json_len(pieces) && !e->exc; j++) {
            json_t *piece = json_get(pieces, j);
            json_t *candidate = json_copy(current);
            json_append(candidate, json_copy(piece));
            if (json_len(current) > 0) {
                char *text = tpf_progress_text(candidate);
                double l = tpf_len_fn(e, text);
                free(text);
                if (e->exc) { json_free(candidate); break; }
                if (l > limit) {
                    json_append(groups, current);
                    current = json_array();
                    json_free(candidate);
                    candidate = json_array();
                    json_append(candidate, json_copy(piece));
                }
            }
            json_free(current);
            current = candidate;
        }
        json_free(pieces);
    }
    if (e->exc) { json_free(current); return groups; }
    if (json_len(current) > 0) json_append(groups, current);
    else json_free(current);
    return groups;
}
/* _send_progress_text: adapter.send + _track_progress_result. */
static json_t *tpf_send_progress_text(tpf_env *e, json_t *st, const char *text) {
    (void)st;
    json_t *ctx = tpf_ctx(e);
    json_t *a = json_object();
    json_set(a, "chat_id", json_string(tpf_chat_id(e)));
    json_set(a, "content", json_string(text ? text : ""));
    json_t *rt = json_obj_get(ctx, "_progress_reply_to");
    if (rt && rt->type != JSON_NULL) json_set(a, "reply_to", json_copy(rt));
    json_t *md = json_obj_get(ctx, "_progress_metadata");
    if (md && md->type != JSON_NULL) json_set(a, "metadata", json_copy(md));
    json_t *r = tpf_call(e, "adapter_send", a);
    if (tpf_is_raise(r)) { tpf_park(e, r); return NULL; }
    tpf_track_progress_result(e, r);
    return r;
}
/* _edit_progress_message: kwargs construction + adapter.edit_message. */
static json_t *tpf_edit_progress_message(tpf_env *e, json_t *st,
                                         const char *message_id, const char *content) {
    json_t *ctx = tpf_ctx(e);
    json_t *a = json_object();
    json_set(a, "chat_id", json_string(tpf_chat_id(e)));
    json_set(a, "message_id", json_string(message_id ? message_id : ""));
    json_set(a, "content", json_string(content ? content : ""));
    json_t *ad = tpf_st_adapter(e, st);
    if (gflow_truth(json_obj_get(ad, "REQUIRES_EDIT_FINALIZE")))
        json_set(a, "finalize", json_bool(true));
    if (gflow_truth(json_obj_get(st, "_edit_accepts_metadata"))) {
        json_t *md = json_obj_get(ctx, "_progress_metadata");
        json_set(a, "metadata", md ? json_copy(md) : json_null());
    }
    json_t *r = tpf_call(e, "adapter_edit", a);
    if (tpf_is_raise(r)) { tpf_park(e, r); return NULL; }
    return r;
}
/* _progress_terminal_blocks: (full, short) fenced blocks or (None, None). */
static json_t *tpf_terminal_blocks(tpf_env *e, json_t *adapter, const char *tool_name,
                                   const json_t *args, const char *emoji) {
    json_t *ret = json_array();
    json_t *command = (args && args->type == JSON_OBJECT)
        ? json_obj_get(args, "command") : NULL;
    bool supports = gflow_truth(json_obj_get(adapter, "supports_code_blocks"));
    const char *cmd = gflow_str(command);
    if (!(supports && tool_name && strcmp(tool_name, "terminal") == 0
          && args && args->type == JSON_OBJECT
          && command && command->type == JSON_STRING && tpf_strip_nonempty(cmd))) {
        json_append(ret, json_null());
        json_append(ret, json_null());
        return ret;
    }
    char *cmd_full = tpf_rstrip_ws(cmd);
    json_t *ctx = tpf_ctx(e);
    const char *header = gflow_truth(json_obj_get(ctx, "last_was_terminal_block"))
        ? "" : "gateway.progress.tool_head\n";
    double cap = tpf_preview_cap(e);
    if (e->exc) { free(cmd_full); json_append(ret, json_null());
        json_append(ret, json_null()); return ret; }
    size_t nlines = 0;
    char **lines = tpf_splitlines(cmd_full, &nlines);
    char *cmd_short = strdup(nlines > 0 ? lines[0] : cmd_full);
    size_t slen = tpf_cplen(cmd_short);
    if ((double)slen > cap) {
        char *cut = tpf_slice(cmd_short, (long)(cap - 3));
        free(cmd_short);
        cmd_short = (char *)malloc(strlen(cut) + 4);
        if (cmd_short) sprintf(cmd_short, "%s...", cut);
        else cmd_short = strdup("...");
        free(cut);
    } else if (nlines > 1) {
        char *suffixed = (char *)malloc(strlen(cmd_short) + 5);
        if (suffixed) sprintf(suffixed, "%s ...", cmd_short);
        free(cmd_short);
        cmd_short = suffixed ? suffixed : strdup("");
    }
    (void)emoji;   /* t() identity: the emoji kwarg is unrecorded */
    size_t fn = strlen(header) + 4 + strlen(cmd_full) + 5 + 1;
    char *full = (char *)malloc(fn);
    if (full) snprintf(full, fn, "%s```\n%s\n```", header, cmd_full);
    size_t sn = strlen(header) + 4 + strlen(cmd_short) + 5 + 1;
    char *shortb = (char *)malloc(sn);
    if (shortb) snprintf(shortb, sn, "%s```\n%s\n```", header, cmd_short);
    json_append(ret, json_string(full ? full : ""));
    json_append(ret, json_string(shortb ? shortb : ""));
    free(full); free(shortb); free(cmd_short); free(cmd_full);
    tpf_free_lines(lines, nlines);
    return ret;
}
/* _append_progress_repeat_count core (shared with _progress_emit/_absorb). */
static char *tpf_append_repeat_str(const char *message, double occurrences) {
    char num[64];
    tpf_num_str(occurrences, num, sizeof(num));
    char suffix[96];
    snprintf(suffix, sizeof(suffix), "(\xc3\x97%s)", num);
    char *body = tpf_rstrip_crlf(message);
    char *ret;
    if (tpf_fence_match(body)) {
        const char *line_end = tpf_endswith_crlf(message) ? "\r\n" : "\n";
        size_t n = strlen(body) + strlen(line_end) + strlen(suffix) + 1;
        ret = (char *)malloc(n);
        if (ret) snprintf(ret, n, "%s%s%s", body, line_end, suffix);
        else ret = strdup("");
    } else {
        size_t n = strlen(message) + 1 + strlen(suffix) + 1;
        ret = (char *)malloc(n);
        if (ret) snprintf(ret, n, "%s %s", message, suffix);
        else ret = strdup("");
    }
    free(body);
    return ret;
}
/* ── flood / owed / partial cores (ported siblings run real on both sides) ── */
/* _note_flood core. result borrowed (may be NULL == None). */
static bool tpf_note_flood_core(tpf_env *e, json_t *st, const json_t *result) {
    json_t *errn = result ? json_obj_get(result, "error") : NULL;
    /* error = getattr(result, "error", "") or "" */
    const char *error = "";
    json_t *errarg;
    if (gflow_truth(errn)) {
        if (errn->type == JSON_STRING) { error = errn->str_val ? errn->str_val : "";
            errarg = json_copy(errn); }
        else errarg = json_copy(errn);
    } else {
        errarg = json_string("");
    }
    json_t *a = json_object();
    json_set(a, "error", errarg);
    json_t *ifr = tpf_call(e, "is_flood_error", a);
    if (tpf_is_raise(ifr)) { tpf_park(e, ifr); return false; }
    bool gate = gflow_truth(ifr);
    json_free(ifr);
    if (!gate && gflow_truth(errn) && errn->type != JSON_STRING) {
        char m[128];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'lower'", tpf_py_type(errn));
        tpf_park(e, tpf_raise_spec("AttributeError", m));
        return false;
    }
    if (!gate) {
        char *low = tpf_lower(error);
        gate = strstr(low, "flood") != NULL;
        if (!gate) gate = strstr(low, "retry after") != NULL;
        free(low);
    }
    if (!gate) return false;
    /* wait = float(getattr(result, "retry_after", None) or 0.0) */
    json_t *ra = result ? json_obj_get(result, "retry_after") : NULL;
    double wait = 0.0;
    if (gflow_truth(ra)) {
        if (ra->type == JSON_NUMBER) wait = ra->num_val;
        else if (ra->type == JSON_BOOL) wait = ra->bool_val ? 1.0 : 0.0;
        else if (ra->type == JSON_STRING) {
            double v = 0.0;
            if (tpf_py_float(ra->str_val, &v)) wait = v;   /* TypeError/ValueError -> 0 */
        }
        /* containers: float() TypeError swallowed -> 0.0 */
    }
    if (wait <= 0) {
        json_t *fa = json_object();
        json_set(fa, "error", json_string(error));
        json_set(fa, "default", json_number(30.0));
        json_t *fr = tpf_call(e, "flood_wait_seconds", fa);
        if (tpf_is_raise(fr)) { tpf_park(e, fr); return false; }
        if (!fr || fr->type != JSON_NUMBER) {
            char m[160];
            snprintf(m, sizeof(m), "unsupported operand type(s) for +: '%s' and 'float'",
                     tpf_clock_type(fr));
            json_free(fr);
            tpf_park(e, tpf_raise_spec("TypeError", m));
            return false;
        }
        wait = fr->num_val;
        json_free(fr);
    }
    json_t *mr = tpf_monotonic(e);
    if (tpf_is_raise(mr)) { tpf_park(e, mr); return false; }
    if (!mr || mr->type != JSON_NUMBER) {
        char m[160];
        snprintf(m, sizeof(m), "unsupported operand type(s) for +: '%s' and 'float'",
                 tpf_clock_type(mr));
        json_free(mr);
        tpf_park(e, tpf_raise_spec("TypeError", m));
        return false;
    }
    double now = mr->num_val;
    json_free(mr);
    json_set(st, "flood_until", json_number(now + wait));
    json_set(st, "flood_pending", json_bool(true));
    return true;
}
/* _in_flood_window core: time.monotonic() < st.flood_until. */
static bool tpf_in_flood_window_core(tpf_env *e, json_t *st) {
    json_t *mr = tpf_monotonic(e);
    if (tpf_is_raise(mr)) { tpf_park(e, mr); return false; }
    json_t *until = json_obj_get(st, "flood_until");
    if (!mr || mr->type != JSON_NUMBER || !until || until->type != JSON_NUMBER) {
        char m[160];
        snprintf(m, sizeof(m), "'<' not supported between instances of '%s' and '%s'",
                 tpf_clock_type(mr), tpf_clock_type(until));
        json_free(mr);
        tpf_park(e, tpf_raise_spec("TypeError", m));
        return false;
    }
    bool r = mr->num_val < until->num_val;
    json_free(mr);
    return r;
}
/* _owed_send_failed core. */
static bool tpf_owed_send_failed_core(tpf_env *e, json_t *st) {
    double fails = json_get_num(st, "owed_send_failures", 0) + 1;
    json_set(st, "owed_send_failures", json_number(fails));
    if (fails >= 5) {   /* _OWED_SEND_ATTEMPTS */
        json_t *la = json_array();
        json_append(la, json_number(5));
        tpf_log(e, 30, "Progress: gave up on carried tool-progress lines after %d failed sends", la);
        json_set(st, "owed_send_failures", json_number(0));
        json_set(st, "owed_fresh", json_bool(false));
        return false;
    }
    json_set(st, "flood_pending", json_bool(true));
    return true;
}
/* _settle_progress_send core. */
static void tpf_settle_progress_send_core(tpf_env *e, json_t *st, const json_t *result) {
    bool success = tpf_attr_success(e, result);
    if (e->exc) return;
    if (success) { json_set(st, "flood_pending", json_bool(false)); return; }
    bool nf = tpf_note_flood_core(e, st, result);
    if (e->exc) return;
    if (!nf) json_set(st, "flood_pending", json_bool(false));
}
/* _hold_partial_tail core. */
static bool tpf_hold_partial_tail_core(tpf_env *e, json_t *st, const json_t *result) {
    json_t *a = json_object();
    json_set(a, "result", result ? json_copy(result) : json_null());
    json_t *r = tpf_call(e, "is_partial_delivery", a);
    if (tpf_is_raise(r)) { tpf_park(e, r); return false; }
    bool partial = gflow_truth(r);
    json_free(r);
    if (!partial) return false;
    if (!result || result->type == JSON_NULL) {
        tpf_park(e, tpf_raise_spec("AttributeError",
                                   "'NoneType' object has no attribute 'raw_response'"));
        return false;
    }
    json_t *raw = json_obj_get(result, "raw_response");
    json_t *chunks = (raw && raw->type == JSON_OBJECT)
        ? json_obj_get(raw, "undelivered_chunks") : NULL;
    if (gflow_truth(chunks)) json_set(st, "owed_tail", json_copy(result));
    return true;
}
/* _settle_partial_progress core. rest borrowed array. */
static bool tpf_settle_partial_progress_core(tpf_env *e, json_t *st,
                                             const json_t *result, json_t *rest) {
    bool hold = tpf_hold_partial_tail_core(e, st, result);
    if (e->exc) return false;
    if (!hold) return false;
    tpf_note_flood_core(e, st, result);
    if (e->exc) return false;
    json_set(st, "progress_msg_id", json_null());
    json_set(st, "progress_lines", rest ? json_copy(rest) : json_array());
    bool has_rest = gflow_truth(rest);
    json_set(st, "owed_fresh", json_bool(has_rest));
    json_set(st, "flood_pending", json_bool(has_rest));
    return true;
}
/* _owed_tail_blocks core. */
static bool tpf_owed_tail_blocks_core(tpf_env *e, json_t *st) {
    json_t *owed = json_obj_get(st, "owed_tail");
    if (!owed || owed->type == JSON_NULL) return false;
    json_t *ad = tpf_st_adapter(e, st);
    json_t *resume = json_obj_get(ad, "_resume_partial_send");
    json_t *result = NULL;
    if (gflow_truth(resume)) {
        json_t *ctx = tpf_ctx(e);
        json_t *a = json_object();
        json_set(a, "chat_id", json_string(tpf_chat_id(e)));
        json_set(a, "owed_tail", json_copy(owed));
        json_t *rt = json_obj_get(ctx, "_progress_reply_to");
        if (rt && rt->type != JSON_NULL) json_set(a, "reply_to", json_copy(rt));
        json_t *md = json_obj_get(ctx, "_progress_metadata");
        if (md && md->type != JSON_NULL) json_set(a, "metadata", json_copy(md));
        result = tpf_call(e, "resume_partial_send", a);
        if (tpf_is_raise(result)) { tpf_park(e, result); return false; }
    }
    bool success = result && result->type != JSON_NULL
        && gflow_truth(json_obj_get(result, "success"));
    if (!result || result->type == JSON_NULL || success) {
        if (result && result->type != JSON_NULL) tpf_track_progress_result(e, result);
        json_set(st, "owed_tail", json_null());
        json_set(st, "owed_tail_failures", json_number(0));
        json_free(result);
        return false;
    }
    json_t *raw = json_obj_get(result, "raw_response");
    json_t *chunks = (raw && raw->type == JSON_OBJECT)
        ? json_obj_get(raw, "undelivered_chunks") : NULL;
    bool flooded = tpf_note_flood_core(e, st, result);
    if (e->exc) { json_free(result); return false; }
    double fails = json_get_num(st, "owed_tail_failures", 0) + (flooded ? 0 : 1);
    json_set(st, "owed_tail_failures", json_number(fails));
    if (gflow_truth(chunks) && (flooded || fails < 5)) {
        json_set(st, "owed_tail", json_copy(result));
        json_free(result);
        return true;
    }
    if (!flooded)
        tpf_log(e, 30, "Progress: gave up on the undelivered tail of a split tool-progress message",
                json_array());
    json_set(st, "owed_tail", json_null());
    json_set(st, "owed_tail_failures", json_number(0));
    json_free(result);
    return false;
}

/* ── claimed flood / owed / partial entries ──────────────────────────────── */
/* PoP: tpf_append_progress_repeat_count @ gateway/run_turn_progress.py:_append_progress_repeat_count */
static json_t *tpf_append_progress_repeat_count(tpf_env *e, const json_t *in) {
    (void)e;
    const char *message = json_get_str(in, "message", "");
    double occ = json_get_num(in, "occurrences", 0);
    char *r = tpf_append_repeat_str(message, occ);
    json_t *ret = json_string(r);
    free(r);
    return ret;
}
/* PoP: tpf_note_flood @ gateway/run_turn_progress.py:_note_flood */
static json_t *tpf_note_flood(tpf_env *e, const json_t *in) {
    bool r = tpf_note_flood_core(e, tpf_st(e), json_obj_get(in, "result"));
    return json_bool(r);
}
/* PoP: tpf_in_flood_window @ gateway/run_turn_progress.py:_in_flood_window */
static json_t *tpf_in_flood_window(tpf_env *e, const json_t *in) {
    (void)in;
    return json_bool(tpf_in_flood_window_core(e, tpf_st(e)));
}
/* PoP: tpf_owed_send_failed @ gateway/run_turn_progress.py:_owed_send_failed */
static json_t *tpf_owed_send_failed(tpf_env *e, const json_t *in) {
    (void)in;
    return json_bool(tpf_owed_send_failed_core(e, tpf_st(e)));
}
/* PoP: tpf_settle_progress_send @ gateway/run_turn_progress.py:_settle_progress_send */
static json_t *tpf_settle_progress_send(tpf_env *e, const json_t *in) {
    tpf_settle_progress_send_core(e, tpf_st(e), json_obj_get(in, "result"));
    return json_null();
}
/* PoP: tpf_hold_partial_tail @ gateway/run_turn_progress.py:_hold_partial_tail */
static json_t *tpf_hold_partial_tail(tpf_env *e, const json_t *in) {
    return json_bool(tpf_hold_partial_tail_core(e, tpf_st(e), json_obj_get(in, "result")));
}
/* PoP: tpf_settle_partial_progress @ gateway/run_turn_progress.py:_settle_partial_progress */
static json_t *tpf_settle_partial_progress(tpf_env *e, const json_t *in) {
    return json_bool(tpf_settle_partial_progress_core(e, tpf_st(e),
                     json_obj_get(in, "result"), json_obj_get(in, "rest")));
}
/* PoP: tpf_owed_tail_blocks @ gateway/run_turn_progress.py:_owed_tail_blocks */
static json_t *tpf_owed_tail_blocks(tpf_env *e, const json_t *in) {
    (void)in;
    return json_bool(tpf_owed_tail_blocks_core(e, tpf_st(e)));
}

/* ── dedup / absorb / reset cores ────────────────────────────────────────── */
/* _progress_emit core: dedup consecutive identical lines, route native/queue. */
static void tpf_emit_core(tpf_env *e, const char *msg) {
    json_t *ctx = tpf_ctx(e);
    const char *scid = json_get_str(ctx, "stream_consumer_id", NULL);
    json_t *sc = tpf_consumer(e, scid);
    bool native = sc && gflow_truth(json_obj_get(sc, "accepts_tool_progress"));
    json_t *last = json_obj_get(ctx, "last_progress_msg");
    bool same = last && last->type == JSON_STRING && last->str_val
        && strcmp(last->str_val, msg) == 0;
    if (same) {
        double rc = json_get_num(ctx, "repeat_count", 0) + 1;
        json_set(ctx, "repeat_count", json_number(rc));
        if (native) {
            char *text = tpf_append_repeat_str(msg, rc + 1);
            json_t *a = json_object();
            json_set(a, "consumer_id", json_string(scid));
            json_set(a, "text", json_string(text));
            free(text);
            json_t *r = tpf_call(e, "on_tool_progress", a);
            if (tpf_is_raise(r)) tpf_park(e, r);
            else json_free(r);
        } else {
            json_t *item = json_array();
            json_append(item, json_string("__dedup__"));
            json_append(item, json_string(msg));
            json_append(item, json_number(rc));
            tpf_queue_put(e, "progress_queue", item);
        }
        return;
    }
    json_set(ctx, "last_progress_msg", json_string(msg));
    json_set(ctx, "repeat_count", json_number(0));
    if (native) {
        json_t *a = json_object();
        json_set(a, "consumer_id", json_string(scid));
        json_set(a, "text", json_string(msg));
        json_t *r = tpf_call(e, "on_tool_progress", a);
        if (tpf_is_raise(r)) tpf_park(e, r);
        else json_free(r);
    } else {
        tpf_queue_put(e, "progress_queue", json_string(msg));
    }
}
/* PoP: tpf_progress_emit @ gateway/run_turn_progress.py:_progress_emit */
static json_t *tpf_progress_emit(tpf_env *e, const json_t *in) {
    tpf_emit_core(e, json_get_str(in, "msg", ""));
    return json_null();
}
/* PoP: tpf_progress_absorb @ gateway/run_turn_progress.py:_progress_absorb */
static json_t *tpf_progress_absorb(tpf_env *e, const json_t *in) {
    json_t *st = tpf_st(e);
    json_t *raw = json_obj_get(in, "raw");
    if (raw && raw->type == JSON_ARRAY && json_len(raw) == 3) {
        json_t *m = json_get(raw, 0);
        if (m && m->type == JSON_STRING && strcmp(m->str_val, "__dedup__") == 0) {
            json_t *base = json_get(raw, 1);
            json_t *cn = json_get(raw, 2);
            double count = (cn && cn->type == JSON_NUMBER) ? cn->num_val : 0;
            json_t *lines = json_obj_get(st, "progress_lines");
            if (!gflow_truth(lines)) return base ? json_copy(base) : json_null();
            const char *bs = gflow_str(base);
            char *text = tpf_append_repeat_str(bs ? bs : "", count + 1);
            tpf_arr_set_last(lines, json_string(text));
            json_t *ret = json_string(text);
            free(text);
            return ret;
        }
    }
    json_t *lines = json_obj_get(st, "progress_lines");
    if (lines && lines->type == JSON_ARRAY)
        json_append(lines, raw ? json_copy(raw) : json_null());
    return raw ? json_copy(raw) : json_null();
}
/* PoP: tpf_is_reset_marker @ gateway/run_turn_progress.py:_is_reset_marker */
static json_t *tpf_is_reset_marker(tpf_env *e, const json_t *in) {
    (void)e;
    json_t *raw = json_obj_get(in, "raw");
    bool r = raw && raw->type == JSON_ARRAY && json_len(raw) >= 1
        && json_get(raw, 0) && json_get(raw, 0)->type == JSON_STRING
        && strcmp(json_get(raw, 0)->str_val, "__reset__") == 0;
    return json_bool(r);
}
/* PoP: tpf_reset_progress_bubble @ gateway/run_turn_progress.py:_reset_progress_bubble */
static json_t *tpf_reset_progress_bubble(tpf_env *e, const json_t *in) {
    (void)in;
    json_t *st = tpf_st(e);
    json_t *ctx = tpf_ctx(e);
    json_t *carried = json_array();
    json_t *msg_id = json_obj_get(st, "progress_msg_id");
    json_t *lines = json_obj_get(st, "progress_lines");
    bool pending = gflow_truth(json_obj_get(st, "flood_pending"));
    bool can_edit = gflow_truth(json_obj_get(st, "can_edit"));
    if (pending && can_edit && msg_id && msg_id->type != JSON_NULL && gflow_truth(lines)) {
        json_t *groups = tpf_split_progress_groups(e, st, lines);
        if (e->exc) { json_free(groups); json_free(carried); return json_null(); }
        if (json_len(groups) == 0) {
            tpf_park(e, tpf_raise_spec("ValueError",
                     "not enough values to unpack (expected at least 1, got 0)"));
            json_free(groups); json_free(carried);
            return json_null();
        }
        char *text = tpf_progress_text(json_get(groups, 0));
        json_t *bubble = json_array();
        json_append(bubble, json_copy(msg_id));
        json_append(bubble, json_string(text));
        free(text);
        json_t *bubbles = json_obj_get(st, "owed_bubbles");
        if (bubbles && bubbles->type == JSON_ARRAY) json_append(bubbles, bubble);
        else json_free(bubble);
        json_free(carried);
        carried = tpf_flatten_groups(groups, 1);
        json_free(groups);
    } else if (gflow_truth(json_obj_get(st, "owed_fresh"))
               && (!msg_id || msg_id->type == JSON_NULL)) {
        json_free(carried);
        carried = lines ? json_copy(lines) : json_array();
    }
    json_set(st, "owed_fresh", json_bool(gflow_truth(carried)));
    json_set(st, "progress_msg_id", json_null());
    json_set(st, "progress_lines", carried);   /* adopts */
    json_set(ctx, "last_progress_msg", json_null());
    json_set(ctx, "repeat_count", json_number(0));
    return json_null();
}

/* ── send/edit + overflow roll + idle tick cores ─────────────────────────── */
/* _progress_send_or_edit core. */
static bool tpf_send_or_edit_core(tpf_env *e, json_t *st, const char *msg) {
    if (tpf_in_flood_window_core(e, st)) {
        if (e->exc) return false;
        json_set(st, "flood_pending", json_bool(true));
        return false;
    }
    if (e->exc) return false;
    if (tpf_owed_tail_blocks_core(e, st)) {
        if (e->exc) return false;
        json_set(st, "flood_pending", json_bool(true));
        return false;
    }
    if (e->exc) return false;
    bool can_edit = gflow_truth(json_obj_get(st, "can_edit"));
    json_t *msg_id = json_obj_get(st, "progress_msg_id");
    json_t *empty = json_array();
    if (can_edit && msg_id && msg_id->type != JSON_NULL) {
        char *text = tpf_join(json_obj_get(st, "progress_lines"), "\n");
        json_t *result = tpf_edit_progress_message(e, st, gflow_str(msg_id), text);
        free(text);
        if (e->exc) { json_free(empty); return false; }
        bool success = tpf_attr_success(e, result);
        if (e->exc) { json_free(result); json_free(empty); return false; }
        if (success) {
            json_set(st, "flood_pending", json_bool(false));
            json_free(result); json_free(empty);
            return true;
        }
        bool sp = tpf_settle_partial_progress_core(e, st, result, empty);
        if (e->exc) { json_free(result); json_free(empty); return false; }
        if (sp) { json_free(result); json_free(empty); return true; }
        if (gflow_truth(json_obj_get(result, "retryable"))) {
            json_t *ad = tpf_st_adapter(e, st);
            tpf_log(e, 10, "[%s] Transient edit failure \xe2\x80\x94 keeping can_edit=True",
                    tpf_logargs1(json_get_str(ad, "name", "")));
            json_free(result); json_free(empty);
            return false;
        }
        bool nf = tpf_note_flood_core(e, st, result);
        if (e->exc) { json_free(result); json_free(empty); return false; }
        if (nf) {
            json_t *ad = tpf_st_adapter(e, st);
            tpf_log(e, 20, "[%s] Progress edit flood control, backing off",
                    tpf_logargs1(json_get_str(ad, "name", "")));
            json_free(result); json_free(empty);
            return false;
        }
        json_set(st, "can_edit", json_bool(false));
        json_free(result);
        result = tpf_send_progress_text(e, st, msg);
        if (e->exc) { json_free(empty); return false; }
        sp = tpf_settle_partial_progress_core(e, st, result, empty);
        if (e->exc) { json_free(result); json_free(empty); return false; }
        if (!sp) {
            tpf_settle_progress_send_core(e, st, result);
            if (e->exc) { json_free(result); json_free(empty); return false; }
        }
        json_free(result); json_free(empty);
        return true;
    }
    char *text = can_edit ? tpf_join(json_obj_get(st, "progress_lines"), "\n") : strdup(msg);
    json_t *result = tpf_send_progress_text(e, st, text);
    free(text);
    if (e->exc) { json_free(empty); return false; }
    bool sp = tpf_settle_partial_progress_core(e, st, result, empty);
    if (e->exc) { json_free(result); json_free(empty); return false; }
    if (sp) { json_free(result); json_free(empty); return true; }
    bool success = tpf_attr_success(e, result);
    if (e->exc) { json_free(result); json_free(empty); return false; }
    if (gflow_truth(json_obj_get(st, "owed_fresh")) && !success) {
        bool nf = tpf_note_flood_core(e, st, result);
        if (e->exc) { json_free(result); json_free(empty); return false; }
        if (!nf && tpf_owed_send_failed_core(e, st)) {
            json_free(result); json_free(empty);
            return true;
        }
    }
    tpf_settle_progress_send_core(e, st, result);
    if (e->exc) { json_free(result); json_free(empty); return false; }
    if (success && gflow_truth(json_obj_get(result, "message_id"))) {
        json_set(st, "progress_msg_id", json_copy(json_obj_get(result, "message_id")));
        json_set(st, "owed_fresh", json_bool(false));
        json_set(st, "owed_send_failures", json_number(0));
    }
    json_free(result); json_free(empty);
    return true;
}
/* PoP: tpf_progress_send_or_edit @ gateway/run_turn_progress.py:_progress_send_or_edit */
static json_t *tpf_progress_send_or_edit(tpf_env *e, const json_t *in) {
    return json_bool(tpf_send_or_edit_core(e, tpf_st(e), json_get_str(in, "msg", "")));
}
/* _roll_progress_overflow_if_needed core. */
static bool tpf_roll_core(tpf_env *e, json_t *st) {
    json_t *lines = json_obj_get(st, "progress_lines");
    if (!gflow_truth(lines) || !gflow_truth(json_obj_get(st, "can_edit"))) return false;
    bool fw = tpf_in_flood_window_core(e, st);
    if (e->exc) return false;
    if (fw) return false;
    json_t *groups = tpf_split_progress_groups(e, st, lines);
    if (e->exc) { json_free(groups); return false; }
    if (json_len(groups) <= 1) { json_free(groups); return false; }
    bool otb = tpf_owed_tail_blocks_core(e, st);
    if (e->exc) { json_free(groups); return false; }
    if (otb) { json_free(groups); return true; }
    json_t *msg_id = json_obj_get(st, "progress_msg_id");
    size_t start = 0;
    if (msg_id && msg_id->type != JSON_NULL) {
        char *text = tpf_progress_text(json_get(groups, 0));
        json_t *result = tpf_edit_progress_message(e, st, gflow_str(msg_id), text);
        free(text);
        if (e->exc) { json_free(groups); return false; }
        json_t *rest = tpf_flatten_groups(groups, 1);
        bool sp = tpf_settle_partial_progress_core(e, st, result, rest);
        json_free(rest);
        if (e->exc) { json_free(result); json_free(groups); return false; }
        if (sp) { json_free(result); json_free(groups); return true; }
        bool success = tpf_attr_success(e, result);
        if (e->exc) { json_free(result); json_free(groups); return false; }
        if (!success) {
            bool retryable = gflow_truth(json_obj_get(result, "retryable"));
            bool nf = false;
            if (!retryable) {
                nf = tpf_note_flood_core(e, st, result);
                if (e->exc) { json_free(result); json_free(groups); return false; }
            }
            if (retryable || nf) {
                json_t *ad = tpf_st_adapter(e, st);
                tpf_log(e, 10,
                        "[%s] Transient overflow edit failure \xe2\x80\x94 keeping can_edit=True",
                        tpf_logargs1(json_get_str(ad, "name", "")));
                json_free(result); json_free(groups);
                return true;
            }
            json_set(st, "can_edit", json_bool(false));
            json_free(result); json_free(groups);
            return false;
        }
        json_free(result);
        start = 1;
    }
    size_t n = json_len(groups);
    for (size_t i = start; i < n; i++) {
        char *text = tpf_progress_text(json_get(groups, i));
        json_t *result = tpf_send_progress_text(e, st, text);
        free(text);
        if (e->exc) { json_free(groups); return false; }
        json_t *rest = tpf_flatten_groups(groups, i + 1);
        bool sp = tpf_settle_partial_progress_core(e, st, result, rest);
        json_free(rest);
        if (e->exc) { json_free(result); json_free(groups); return false; }
        if (sp) { json_free(result); json_free(groups); return true; }
        bool success = tpf_attr_success(e, result);
        if (e->exc) { json_free(result); json_free(groups); return false; }
        if (!success) {
            bool nf = tpf_note_flood_core(e, st, result);
            if (e->exc) { json_free(result); json_free(groups); return false; }
            if (nf) {
                json_set(st, "progress_msg_id", json_null());
                json_set(st, "progress_lines", tpf_flatten_groups(groups, i));
                json_free(result); json_free(groups);
                return true;
            }
        }
        if (gflow_truth(json_obj_get(st, "owed_fresh")) && !success
            && tpf_owed_send_failed_core(e, st)) {
            json_set(st, "progress_msg_id", json_null());
            json_set(st, "progress_lines", tpf_flatten_groups(groups, i));
            json_free(result); json_free(groups);
            return true;
        }
        if (success && gflow_truth(json_obj_get(result, "message_id"))) {
            json_set(st, "progress_msg_id", json_copy(json_obj_get(result, "message_id")));
            json_set(st, "owed_send_failures", json_number(0));
        }
        json_free(result);
    }
    json_set(st, "flood_pending", json_bool(false));
    json_set(st, "owed_fresh", json_bool(false));
    json_set(st, "progress_lines", json_copy(json_get(groups, n - 1)));
    json_free(groups);
    return true;
}
/* PoP: tpf_roll_progress_overflow_if_needed @ gateway/run_turn_progress.py:_roll_progress_overflow_if_needed */
static json_t *tpf_roll_progress_overflow_if_needed(tpf_env *e, const json_t *in) {
    (void)in;
    return json_bool(tpf_roll_core(e, tpf_st(e)));
}
/* _deliver_flood_pending_progress core. */
static bool tpf_deliver_core(tpf_env *e, json_t *st) {
    bool fw = tpf_in_flood_window_core(e, st);
    if (e->exc) return false;
    bool pending_lines = gflow_truth(json_obj_get(st, "flood_pending"))
        && gflow_truth(json_obj_get(st, "progress_lines"));
    json_t *bubbles = json_obj_get(st, "owed_bubbles");
    json_t *tail = json_obj_get(st, "owed_tail");
    if (fw || !(pending_lines || gflow_truth(bubbles)
                || (tail && tail->type != JSON_NULL))) return false;
    json_t *rsc = tpf_run_still_current(e);
    if (tpf_is_raise(rsc)) { tpf_park(e, rsc); return false; }
    bool cur = gflow_truth(rsc);
    json_free(rsc);
    if (!cur) return false;
    json_t *ai = tpf_agent_interrupted(e);
    if (tpf_is_raise(ai)) { tpf_park(e, ai); return false; }
    bool interrupted = gflow_truth(ai);
    json_free(ai);
    if (interrupted) return false;
    if (gflow_truth(bubbles)) {
        json_t *b0 = json_get(bubbles, 0);
        const char *mid = gflow_str(json_get(b0, 0));
        const char *txt = gflow_str(json_get(b0, 1));
        json_t *result = tpf_edit_progress_message(e, st, mid, txt);
        if (e->exc) return false;
        bool partial = tpf_hold_partial_tail_core(e, st, result);
        if (e->exc) { json_free(result); return false; }
        bool success = tpf_attr_success(e, result);
        if (e->exc) { json_free(result); return false; }
        bool flooded = false;
        if (!success) {
            flooded = tpf_note_flood_core(e, st, result);
            if (e->exc) { json_free(result); return false; }
        }
        bool retryable = gflow_truth(json_obj_get(result, "retryable"));
        if (success || partial || !(retryable || flooded)) {
            json_t *popped = tpf_arr_pop(bubbles, 0);
            json_free(popped);
        }
        json_free(result);
        return true;
    }
    if (tail && tail->type != JSON_NULL) {
        bool otb = tpf_owed_tail_blocks_core(e, st);
        if (e->exc) return false;
        bool pl2 = gflow_truth(json_obj_get(st, "flood_pending"))
            && gflow_truth(json_obj_get(st, "progress_lines"));
        if (otb || !pl2) return true;
    }
    bool roll = tpf_roll_core(e, st);
    if (e->exc) return false;
    if (!roll) {
        json_t *lines = json_obj_get(st, "progress_lines");
        size_t n = json_len(lines);
        const char *lastline = n ? gflow_str(json_get(lines, n - 1)) : "";
        tpf_send_or_edit_core(e, st, lastline ? lastline : "");
        if (e->exc) return false;
    }
    return true;
}
/* PoP: tpf_deliver_flood_pending_progress @ gateway/run_turn_progress.py:_deliver_flood_pending_progress */
static json_t *tpf_deliver_flood_pending_progress(tpf_env *e, const json_t *in) {
    (void)in;
    return json_bool(tpf_deliver_core(e, tpf_st(e)));
}
/* ── build_message rendering core ────────────────────────────────────────── */
static json_t *tpf_build_message_core(tpf_env *e, const char *tool_name,
                                      const json_t *preview, const json_t *args) {
    json_t *ctx = tpf_ctx(e);
    /* emoji = get_tool_emoji(tool_name, default="\u2699\ufe0f") — before the try */
    json_t *ea = json_object();
    json_set(ea, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_set(ea, "default", json_string("\xe2\x9a\x99\xef\xb8\x8f"));
    json_t *er = tpf_call(e, "get_tool_emoji", ea);
    if (tpf_is_raise(er)) { tpf_park(e, er); return json_null(); }
    char *emoji = strdup(gflow_str(er) ? gflow_str(er) : "");
    json_free(er);
    /* try: adapter = self._runner._delivery_adapter_for(ctx.source) */
    json_t *aa = json_object();
    json_t *src = json_obj_get(ctx, "source");
    json_set(aa, "source", src ? json_copy(src) : json_null());
    json_t *ar = tpf_call(e, "delivery_adapter_for", aa);
    char *adapter_id = NULL;
    if (tpf_is_raise(ar)) {
        if (tpf_is_base(tpf_kind(ar))) { tpf_park(e, ar); free(emoji); return json_null(); }
        json_free(ar);   /* except Exception -> adapter = None */
    } else {
        adapter_id = gflow_str(ar) ? strdup(gflow_str(ar)) : NULL;
        json_free(ar);
    }
    json_t *adapter = tpf_adapter(e, adapter_id);
    json_t *blocks = tpf_terminal_blocks(e, adapter, tool_name, args, emoji);
    if (e->exc) { free(emoji); free(adapter_id); json_free(blocks); return json_null(); }
    json_t *code_full = json_get(blocks, 0);
    json_t *code_short = json_get(blocks, 1);
    bool verbose = strcmp(json_get_str(ctx, "progress_mode", ""), "verbose") == 0;
    json_t *code = verbose ? code_full : code_short;   /* borrowed */
    json_set(ctx, "last_was_terminal_block", json_bool(code && code->type != JSON_NULL));
    if (verbose) {
        json_t *put = NULL;
        if ((!code || code->type == JSON_NULL) && gflow_truth(args)) {
            double pl = tpf_preview_max_len_raw(e);
            if (e->exc) { free(emoji); free(adapter_id); json_free(blocks); return json_null(); }
            bool supports = gflow_truth(json_obj_get(adapter, "supports_code_blocks"));
            json_t *va = json_object();
            json_set(va, "args", args ? json_copy(args) : json_null());
            json_set(va, "max_len", json_number(pl));
            json_set(va, "supports_code_blocks", json_bool(supports));
            json_t *vr = tpf_call(e, "verbose_tool_args_text", va);
            if (tpf_is_raise(vr)) { tpf_park(e, vr); free(emoji); free(adapter_id);
                json_free(blocks); return json_null(); }
            json_free(vr);
            put = json_string("gateway.progress.tool_verbose");
        } else if (!code || code->type == JSON_NULL) {
            put = json_string(gflow_truth(preview)
                ? "gateway.progress.tool_preview" : "gateway.progress.tool_pending");
        } else {
            put = json_copy(code);
        }
        tpf_queue_put(e, "progress_queue", put);
        free(emoji); free(adapter_id); json_free(blocks);
        return json_null();
    }
    if (code && code->type != JSON_NULL) {
        json_t *ret = json_copy(code);
        free(emoji); free(adapter_id); json_free(blocks);
        return ret;
    }
    json_free(blocks);
    if (!gflow_truth(preview)) {
        free(emoji); free(adapter_id);
        return json_string("gateway.progress.tool_pending");
    }
    /* prepared = prepare_tool_preview(tool_name, args, fallback=preview,
     *                                 max_len=self._preview_cap()) */
    double pl = tpf_preview_cap(e);
    if (e->exc) { free(emoji); free(adapter_id); return json_null(); }
    json_t *pa = json_object();
    json_set(pa, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_set(pa, "args", args ? json_copy(args) : json_null());
    json_set(pa, "fallback", preview ? json_copy(preview) : json_null());
    json_set(pa, "max_len", json_number(pl));
    json_t *prepared = tpf_call(e, "prepare_tool_preview", pa);
    if (tpf_is_raise(prepared)) { tpf_park(e, prepared); free(emoji); free(adapter_id);
        return json_null(); }
    const char *prepared_text = gflow_str(json_obj_get(prepared, "text"));
    char *preview_str = NULL;
    if (adapter) {
        json_t *fa = json_object();
        json_set(fa, "adapter_id", json_string(adapter_id));
        json_set(fa, "prepared", json_copy(prepared));
        json_t *fr = tpf_call(e, "format_tool_preview", fa);
        if (tpf_is_raise(fr)) { tpf_park(e, fr); json_free(prepared); free(emoji);
            free(adapter_id); return json_null(); }
        preview_str = strdup(gflow_str(fr) ? gflow_str(fr) : "");
        json_free(fr);
    } else {
        preview_str = strdup(prepared_text ? prepared_text : "");
    }
    json_free(prepared);
    json_t *vba = json_object();
    json_set(vba, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_t *vr = tpf_call(e, "get_tool_verb", vba);
    if (tpf_is_raise(vr)) { tpf_park(e, vr); free(preview_str); free(emoji);
        free(adapter_id); return json_null(); }
    if (!gflow_truth(vr)) {
        json_free(vr); free(preview_str); free(emoji); free(adapter_id);
        return json_string("gateway.progress.tool_preview");
    }
    char *verb = strdup(gflow_str(vr) ? gflow_str(vr) : "");
    json_free(vr);
    json_t *da = json_object();
    json_set(da, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_t *dr = tpf_call(e, "verb_drops_preview", da);
    if (tpf_is_raise(dr)) { tpf_park(e, dr); free(verb); free(preview_str); free(emoji);
        free(adapter_id); return json_null(); }
    bool drops = gflow_truth(dr);
    json_free(dr);
    char *conn = NULL;
    if (!drops) {
        json_t *ca = json_object();
        json_set(ca, "tool_name", tool_name ? json_string(tool_name) : json_null());
        json_t *cr = tpf_call(e, "tool_verb_connector", ca);
        if (tpf_is_raise(cr)) { tpf_park(e, cr); free(verb); free(preview_str); free(emoji);
            free(adapter_id); return json_null(); }
        conn = strdup(gflow_str(cr) ? gflow_str(cr) : "");
        json_free(cr);
    }
    size_t n = strlen(emoji) + 1 + strlen(verb) + (conn ? strlen(conn) : 0)
        + strlen(preview_str) + 1;
    char *buf = (char *)malloc(n);
    if (buf) {
        if (drops) snprintf(buf, n, "%s %s", emoji, verb);
        else snprintf(buf, n, "%s %s%s%s", emoji, verb, conn, preview_str);
    }
    json_t *ret = json_string(buf ? buf : "");
    free(buf); free(conn); free(verb); free(preview_str); free(emoji);
    free(adapter_id);
    return ret;
}
/* PoP: tpf_progress_build_message @ gateway/run_turn_progress.py:_progress_build_message */
static json_t *tpf_progress_build_message(tpf_env *e, const json_t *in) {
    return tpf_build_message_core(e, json_get_str(in, "tool_name", NULL),
                                  json_obj_get(in, "preview"), json_obj_get(in, "args"));
}

/* ── progress_callback emission gate chain ───────────────────────────────── */
/* PoP: tpf_progress_callback @ gateway/run_turn_progress.py:progress_callback */
static json_t *tpf_progress_callback(tpf_env *e, const json_t *in) {
    json_t *ctx = tpf_ctx(e);
    const char *event_type = json_get_str(in, "event_type", "");
    const char *tool_name = json_get_str(in, "tool_name", NULL);
    json_t *preview = json_obj_get(in, "preview");
    json_t *args = json_obj_get(in, "args");
    json_t *kwargs = json_obj_get(in, "kwargs");
    if (!kwargs || kwargs->type != JSON_OBJECT) kwargs = json_object();
    else kwargs = json_copy(kwargs);
    /* if local_viewer_turn(self): try publish_progress (except Exception) */
    json_t *lv = tpf_call(e, "local_viewer_turn", json_object());
    if (tpf_is_raise(lv)) { tpf_park(e, lv); json_free(kwargs); return json_null(); }
    bool local = gflow_truth(lv);
    json_free(lv);
    if (local) {
        json_t *pa = json_object();
        json_set(pa, "event_type", json_string(event_type));
        json_set(pa, "tool_name", tool_name ? json_string(tool_name) : json_null());
        json_set(pa, "preview", preview ? json_copy(preview) : json_null());
        json_set(pa, "kwargs", json_copy(kwargs));
        json_t *pr = tpf_call(e, "publish_progress", pa);
        if (tpf_is_raise(pr)) {
            if (tpf_is_base(tpf_kind(pr))) { tpf_park(e, pr); json_free(kwargs); return json_null(); }
            tpf_log(e, 10, "Local viewer progress publish failed for %s",
                    tpf_logargs1(event_type));
            json_free(pr);
        } else {
            json_free(pr);
        }
    }
    /* subagent.complete handled FIRST, before every progress-queue gate */
    if (strcmp(event_type, "subagent.complete") == 0) {
        json_t *na = json_object();
        json_set(na, "preview", preview ? json_copy(preview) : json_null());
        json_set(na, "kwargs", json_copy(kwargs));
        json_t *nr = tpf_call(e, "subagent_notice", na);
        if (tpf_is_raise(nr)) tpf_park(e, nr);
        else json_free(nr);
        json_free(kwargs);
        return json_null();
    }
    json_t *lsa = json_object();
    json_set(lsa, "event_type", json_string(event_type));
    json_set(lsa, "tool_name", tool_name ? json_string(tool_name) : json_null());
    json_set(lsa, "args", args ? json_copy(args) : json_null());
    json_t *lsr = tpf_call(e, "live_status", lsa);
    if (tpf_is_raise(lsr)) { tpf_park(e, lsr); json_free(kwargs); return json_null(); }
    json_free(lsr);
    /* "log" mode: append tool.started lines (before the progress_queue guard) */
    if (tpf_queue_present(ctx, "log_queue") && strcmp(event_type, "tool.started") == 0
        && tool_name && tool_name[0] && strcmp(tool_name, "_thinking") != 0) {
        json_t *ta = json_object();
        json_set(ta, "fmt", json_string("%Y-%m-%d %H:%M:%S"));
        json_t *tr = tpf_call(e, "now_str", ta);
        if (tpf_is_raise(tr)) { tpf_park(e, tr); json_free(kwargs); return json_null(); }
        const char *ts = gflow_str(tr) ? gflow_str(tr) : "";
        const char *pv = gflow_str(preview);
        char *line;
        if (gflow_truth(preview)) {
            size_t n = strlen(ts) + 2 + strlen(tool_name) + 1 + 1 + strlen(pv) + 1 + 1;
            line = (char *)malloc(n);
            if (line) snprintf(line, n, "%s  %s: \"%s\"", ts, tool_name, pv);
            else line = strdup("");
        } else {
            size_t n = strlen(ts) + 2 + strlen(tool_name) + 1 + 1;
            line = (char *)malloc(n);
            if (line) snprintf(line, n, "%s  %s:", ts, tool_name);
            else line = strdup("");
        }
        json_free(tr);
        char *stripped = tpf_rstrip_ws(line);
        free(line);
        tpf_queue_put(e, "log_queue", json_string(stripped));
        free(stripped);
        if (e->exc) { json_free(kwargs); return json_null(); }
    }
    /* if not ctx.progress_queue or not ctx._run_still_current(): return */
    if (!tpf_queue_present(ctx, "progress_queue")) { json_free(kwargs); return json_null(); }
    json_t *rc = tpf_run_still_current(e);
    if (tpf_is_raise(rc)) { tpf_park(e, rc); json_free(kwargs); return json_null(); }
    bool cur = gflow_truth(rc);
    json_free(rc);
    if (!cur) { json_free(kwargs); return json_null(); }
    if (strcmp(event_type, "tool.completed") == 0
        && !gflow_truth(json_obj_get(ctx, "long_tool_hint_fired"))) {
        json_t *ha = json_object();
        json_set(ha, "kwargs", json_copy(kwargs));
        json_t *hr = tpf_call(e, "onboarding_hint", ha);
        if (tpf_is_raise(hr)) tpf_park(e, hr);
        else json_free(hr);
        json_free(kwargs);
        return json_null();
    }
    /* "_thinking" relayed only when thinking_progress opted in */
    if (strcmp(event_type, "_thinking") == 0
        || (tool_name && strcmp(tool_name, "_thinking") == 0)) {
        const char *thinking_text = NULL;
        if (gflow_truth(json_obj_get(ctx, "_thinking_enabled")))
            thinking_text = (tool_name && strcmp(tool_name, "_thinking") == 0)
                ? gflow_str(preview) : tool_name;
        if (thinking_text && thinking_text[0])
            tpf_queue_put(e, "progress_queue", json_string("gateway.progress.thinking_prefix"));
        json_free(kwargs);
        return json_null();
    }
    if (gflow_truth(json_obj_get(ctx, "_native_slack_task_cards"))
        && (strcmp(event_type, "tool.started") == 0 || strcmp(event_type, "tool.completed") == 0)) {
        json_free(kwargs);
        return json_null();
    }
    if (!gflow_truth(json_obj_get(ctx, "tool_progress_enabled"))
        || strcmp(event_type, "tool.started") != 0
        || (tool_name && strcmp(tool_name, "clarify") == 0)) {
        json_free(kwargs);
        return json_null();
    }
    json_t *ai = tpf_agent_interrupted(e);
    if (tpf_is_raise(ai)) { tpf_park(e, ai); json_free(kwargs); return json_null(); }
    bool interrupted = gflow_truth(ai);
    json_free(ai);
    if (interrupted) { json_free(kwargs); return json_null(); }
    /* "new" mode: only report when tool changes */
    json_t *last_tool = json_obj_get(ctx, "last_tool");
    if (strcmp(json_get_str(ctx, "progress_mode", ""), "new") == 0) {
        bool same = (!tool_name && (!last_tool || last_tool->type == JSON_NULL))
            || (tool_name && last_tool && last_tool->type == JSON_STRING
                && strcmp(last_tool->str_val, tool_name) == 0);
        if (same) { json_free(kwargs); return json_null(); }
    }
    json_set(ctx, "last_tool", tool_name ? json_string(tool_name) : json_null());
    json_t *msg = tpf_build_message_core(e, tool_name, preview, args);
    if (e->exc) { json_free(msg); json_free(kwargs); return json_null(); }
    if (msg && msg->type != JSON_NULL) {
        tpf_emit_core(e, gflow_str(msg));
        json_free(msg);
        if (e->exc) { json_free(kwargs); return json_null(); }
    } else {
        json_free(msg);
    }
    json_free(kwargs);
    return json_null();
}

/* ── status_callback projection branches ─────────────────────────────────── */
/* PoP: tpf_status_callback_sync @ gateway/run_turn_progress.py:_status_callback_sync */
static json_t *tpf_status_callback_sync(tpf_env *e, const json_t *in) {
    json_t *ctx = tpf_ctx(e);
    const char *event_type = json_get_str(in, "event_type", "");
    const char *message = json_get_str(in, "message", "");
    if (gflow_truth(json_obj_get(ctx, "mute_notification_reply"))) return json_null();
    if (!tpf_status_live(e)) { if (e->exc) return json_null(); return json_null(); }
    json_t *src = json_obj_get(ctx, "source");
    json_t *ala = json_object();
    json_set(ala, "source", src ? json_copy(src) : json_null());
    json_t *alr = tpf_call(e, "automatic_notices_allowed", ala);
    if (tpf_is_raise(alr)) { tpf_park(e, alr); return json_null(); }
    bool allowed = gflow_truth(alr);
    json_free(alr);
    if (!allowed) {
        tpf_log(e, 10, "status_callback suppressed for bot-authored message: %s",
                tpf_logargs1(event_type));
        return json_null();
    }
    /* prepared = _prepare_gateway_status_message(platform, event_type, message,
     *                                            status_chat_type(source)) */
    json_t *cta = json_object();
    json_set(cta, "source", src ? json_copy(src) : json_null());
    json_t *ctr = tpf_call(e, "status_chat_type", cta);
    if (tpf_is_raise(ctr)) { tpf_park(e, ctr); return json_null(); }
    char *chat_type = strdup(gflow_str(ctr) ? gflow_str(ctr) : "");
    json_free(ctr);
    json_t *platform = json_obj_get(src, "platform");
    json_t *pma = json_object();
    json_set(pma, "platform", platform ? json_copy(platform) : json_null());
    json_set(pma, "event_type", json_string(event_type));
    json_set(pma, "message", json_string(message));
    json_set(pma, "chat_type", json_string(chat_type));
    free(chat_type);
    json_t *prepared = tpf_call(e, "prepare_status_message", pma);
    if (tpf_is_raise(prepared)) { tpf_park(e, prepared); return json_null(); }
    if (!prepared || prepared->type == JSON_NULL) {
        const char *pv = "unknown";
        if (platform && platform->type != JSON_NULL)
            pv = json_get_str(platform, "value", "unknown");
        json_t *ra = json_object();
        json_set(ra, "text", json_string(message));
        json_t *rr = tpf_call(e, "redact_secrets", ra);
        if (tpf_is_raise(rr)) { tpf_park(e, rr); json_free(prepared); return json_null(); }
        char *redacted = gflow_str(rr) ? strdup(gflow_str(rr)) : strdup("");
        json_free(rr);
        char *slice = tpf_slice(redacted, 160);
        free(redacted);
        json_t *la = json_array();
        json_append(la, json_string(pv));
        json_append(la, json_string(event_type));
        json_append(la, json_string(slice));
        free(slice);
        tpf_log(e, 10, "status_callback suppressed for %s/%s: %s", la);
        json_free(prepared);
        return json_null();
    }
    /* status_key = f"{session_key}:run:{generation}:{event_type}" when both set */
    json_t *sk = json_obj_get(ctx, "session_key");
    json_t *gen = json_obj_get(ctx, "run_generation");
    char *status_key;
    if (gflow_truth(sk) && gen && gen->type != JSON_NULL) {
        char gn[64];
        tpf_num_str(gen->num_val, gn, sizeof(gn));
        const char *sks = gflow_str(sk) ? gflow_str(sk) : "";
        size_t n = strlen(sks) + 5 + strlen(gn) + 1 + strlen(event_type) + 1;
        status_key = (char *)malloc(n);
        if (status_key) snprintf(status_key, n, "%s:run:%s:%s", sks, gn, event_type);
        else status_key = strdup(event_type);
    } else {
        status_key = strdup(event_type);
    }
    /* render_notification(present, platform=..., user_config=...,
     *                       diagnostic=is_warning_status(event_type, message)) */
    json_t *wa = json_object();
    json_set(wa, "event_type", json_string(event_type));
    json_set(wa, "message", json_string(message));
    json_t *wr = tpf_call(e, "is_warning_status", wa);
    if (tpf_is_raise(wr)) { tpf_park(e, wr); free(status_key); json_free(prepared);
        return json_null(); }
    bool diagnostic = gflow_truth(wr);
    json_free(wr);
    json_t *rna = json_object();
    json_set(rna, "platform", platform ? json_copy(platform) : json_null());
    json_t *uc = json_obj_get(ctx, "user_config");
    json_set(rna, "user_config", uc ? json_copy(uc) : json_null());
    json_set(rna, "diagnostic", json_bool(diagnostic));
    json_t *rnr = tpf_call(e, "render_notification", rna);
    if (tpf_is_raise(rnr)) { tpf_park(e, rnr); free(status_key); json_free(prepared);
        return json_null(); }
    json_free(rnr);
    /* present() — the render seam always invokes it after recording */
    json_t *lv = tpf_call(e, "local_viewer_turn", json_object());
    if (tpf_is_raise(lv)) { tpf_park(e, lv); free(status_key); json_free(prepared);
        return json_null(); }
    bool local = gflow_truth(lv);
    json_free(lv);
    if (local) {
        json_t *ka = json_object();
        json_set(ka, "event_type", json_string(event_type));
        json_set(ka, "message", json_string(message));
        json_t *kr = tpf_call(e, "compaction_status_kind", ka);
        if (tpf_is_raise(kr)) { tpf_park(e, kr); free(status_key); json_free(prepared);
            return json_null(); }
        json_t *payload = json_object();
        json_set(payload, "kind", kr ? json_copy(kr) : json_null());
        json_set(payload, "text", json_copy(prepared));
        json_free(kr);
        json_t *pea = json_object();
        json_set(pea, "event_type", json_string("status.update"));
        json_set(pea, "payload", payload);
        json_t *per = tpf_call(e, "publish_execution", pea);
        if (tpf_is_raise(per)) tpf_park(e, per);
        else json_free(per);
        free(status_key); json_free(prepared);
        return json_null();
    }
    /* fut = self._schedule(_send_or_update_status_coro(...), ...) */
    json_t *coa = json_object();
    json_t *sad = json_obj_get(ctx, "_status_adapter");
    if (sad && sad->type != JSON_NULL) json_set(coa, "adapter_id", json_copy(sad));
    json_set(coa, "chat_id", json_string(json_get_str(ctx, "_status_chat_id", "")));
    json_set(coa, "status_key", json_string(status_key));
    json_set(coa, "prepared", json_copy(prepared));
    json_t *tm = json_obj_get(ctx, "_status_thread_metadata");
    if (tm && tm->type != JSON_NULL) json_set(coa, "thread_metadata", json_copy(tm));
    json_t *coro = tpf_call(e, "send_or_update_status_coro", coa);
    if (tpf_is_raise(coro)) { tpf_park(e, coro); free(status_key); json_free(prepared);
        return json_null(); }
    json_t *scha = json_object();
    if (coro && coro->type != JSON_NULL) json_set(scha, "coro", json_copy(coro));
    json_free(coro);
    size_t lmn = strlen(event_type) + 40;
    char *lm = (char *)malloc(lmn);
    if (lm) snprintf(lm, lmn, "status_callback (%s) scheduling error", event_type);
    json_set(scha, "log_message", json_string(lm ? lm : ""));
    free(lm);
    json_t *loop = json_obj_get(ctx, "_loop_for_step");
    if (loop && loop->type != JSON_NULL) json_set(scha, "loop", json_copy(loop));
    json_t *fut = tpf_call(e, "schedule", scha);
    if (tpf_is_raise(fut)) { tpf_park(e, fut); free(status_key); json_free(prepared);
        return json_null(); }
    if (fut && fut->type != JSON_NULL && gflow_truth(json_obj_get(ctx, "_cleanup_progress"))) {
        json_t *da = json_object();
        json_set(da, "fut", json_copy(fut));
        json_set(da, "callback", json_string("_track_future_cleanup_id"));
        json_t *dr = tpf_call(e, "add_done_callback", da);
        if (tpf_is_raise(dr)) tpf_park(e, dr);
        else json_free(dr);
    }
    json_free(fut);
    free(status_key);
    json_free(prepared);
    return json_null();
}
