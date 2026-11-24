/*
 * kanban_watchers_notifier.c - C11 port of upstream
 * gateway/kanban_watchers_notifier.py (gates w30_kanbanwatc_3847d4_p1/p2).
 *
 * PoP: w30_kanbanwatc_3847d4_p1_constants          @ gateway/kanban_watchers_notifier.py:TERMINAL_KINDS/_WAKE_KINDS/_THREAD_CHAT_TYPES/MAX_SEND_FAILURES
 * PoP: w30_kanbanwatc_3847d4_p1_diagnostic_event   @ gateway/kanban_watchers_notifier.py:diagnostic_event
 * PoP: w30_kanbanwatc_3847d4_p1_first_line         @ gateway/kanban_watchers_notifier.py:_first_line
 * PoP: w30_kanbanwatc_3847d4_p1_clip               @ gateway/kanban_watchers_notifier.py:_clip (_payload inside)
 * PoP: w30_kanbanwatc_3847d4_p1_safe_review_reason @ gateway/kanban_watchers_notifier.py:_safe_review_reason
 * PoP: w30_kanbanwatc_3847d4_p1_format_event       @ gateway/kanban_watchers_notifier.py:_EVENT_FORMATTERS + _fmt_completed/_fmt_review_requested/_fmt_changes_requested/_fmt_block_loop_detected/_fmt_gave_up/_fmt_timed_out + the blocked/crashed/status lambdas + _KanbanNotification.__init__ text shaping
 * PoP: w30_kanbanwatc_3847d4_p1_format_batch       @ gateway/kanban_watchers_notifier.py:_KanbanNotification.format_event (accumulator)
 * PoP: w30_kanbanwatc_3847d4_p1_ping_plan          @ gateway/kanban_watchers_notifier.py:_KanbanNotification._send_pings (selection)
 * PoP: w30_kanbanwatc_3847d4_p2_platform_names     @ gateway/kanban_watchers_notifier.py:_platform_names
 * PoP: w30_kanbanwatc_3847d4_p2_collect_scope      @ gateway/kanban_watchers_notifier.py:_Collector.__init__
 * PoP: w30_kanbanwatc_3847d4_p2_chat_type_of       @ gateway/kanban_watchers_notifier.py:_chat_type_of
 * PoP: w30_kanbanwatc_3847d4_p2_wake_scope_id      @ gateway/kanban_watchers_notifier.py:_wake_scope_id
 * PoP: w30_kanbanwatc_3847d4_p2_anchorless_warn    @ gateway/kanban_watchers_notifier.py:_warn_anchorless_thread_sub_once
 * PoP: w30_kanbanwatc_3847d4_p2_unroutable_warn    @ gateway/kanban_watchers_notifier.py:_warn_unroutable_sub_once
 * PoP: w30_kanbanwatc_3847d4_p2_resolve_adapter    @ gateway/kanban_watchers_notifier.py:_adapter_for_subscription
 * PoP: w30_kanbanwatc_3847d4_p2_wake_plan          @ gateway/kanban_watchers_notifier.py:_KanbanNotification.build_wake_text + deliver() wake split + _served_wake_profile
 * PoP: w30_kanbanwatc_3847d4_p2_failure_step        @ gateway/kanban_watchers_notifier.py:_KanbanNotification.delivery_failed
 * PoP: w30_kanbanwatc_3847d4_p2_after_delivery     @ gateway/kanban_watchers_notifier.py:_KanbanNotification.deliver (tail)
 *
 * SEAMS (values handed in, never performed here): the DB claim
 * (claim_unseen_events_for_sub / list_notify_subs / count_notify_subs), the
 * cursor writes (_kanban_advance / _kanban_rewind / record_notify_ping /
 * _kanban_unsub), the adapter registry plus adapter.send() and
 * adapter_supports_push(), profile_routes[].matches() (each route's two match
 * answers arrive per route), _multiplex_profile_homes (the served list),
 * session_owned_by_profile (one boolean), the logger, asyncio.to_thread, and
 * agent.redact.redact_sensitive_text (safe_review_reason takes redacted text).
 * The i18n catalog is embedded below as the ``en`` templates live ``t()``
 * resolves; a miss falls back to the bare key, like ``t()`` does.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w30_kanbanwatc_3847d4_p1.h"
#include "noc_w30_kanbanwatc_3847d4_p2.h"

/* ------------------------------------------------------------------ tables */

static const char *const KW_TERMINAL_KINDS[] = {
    "completed", "blocked", "gave_up", "crashed", "timed_out", "status",
    "archived", "unblocked", "block_loop_detected", "review_requested",
    "changes_requested",
};
#define KW_N_TERMINAL (sizeof(KW_TERMINAL_KINDS) / sizeof(KW_TERMINAL_KINDS[0]))

static const char *const KW_WAKE_KINDS[] = {
    "completed", "gave_up", "crashed", "timed_out", "blocked",
    "review_requested", "changes_requested", "block_loop_detected",
};
#define KW_N_WAKE (sizeof(KW_WAKE_KINDS) / sizeof(KW_WAKE_KINDS[0]))

/* frozenset({"thread","forum","forum_post","forum-post","topic"}), sorted */
static const char *const KW_THREAD_CHAT_TYPES[] = {
    "forum", "forum-post", "forum_post", "thread", "topic",
};
#define KW_N_THREAD (sizeof(KW_THREAD_CHAT_TYPES) / sizeof(KW_THREAD_CHAT_TYPES[0]))

#define KW_MAX_SEND_FAILURES 12

typedef struct { const char *key; const char *tpl; } kw_cat_entry;

/* Bundled ``en`` catalog for gateway.kanban.{ping,wake}.* (locales/en.yaml). */
static const kw_cat_entry KW_CATALOG[] = {
    { "gateway.kanban.ping.assignee_tag",
      "@{assignee} " },
    { "gateway.kanban.ping.blocked",
      "\342\217\270 {head} blocked{reason}" },
    { "gateway.kanban.ping.board_tag",
      "[{board}] " },
    { "gateway.kanban.ping.changes_default_reason",
      "reviewer feedback requires changes" },
    { "gateway.kanban.ping.changes_requested",
      "\360\237\233\221 {board_tag}Kanban {task_id} review requested changes/BLOCK: {reason}{provenance}" },
    { "gateway.kanban.ping.completed",
      "\342\234\224 {head} done \342\200\224 {title}{handoff}" },
    { "gateway.kanban.ping.crashed",
      "\342\234\226 {head} \342\200\224 its worker stopped unexpectedly; it will be retried automatically." },
    { "gateway.kanban.ping.failed_n_times",
      "it failed {count} times in a row" },
    { "gateway.kanban.ping.gave_up",
      "\342\233\224 {head} is now blocked: {count}{last}. Fix the cause, then `hermes kanban unblock {task_id}` (or `hermes kanban reassign {task_id}`). Logs: `hermes kanban log {task_id}`." },
    { "gateway.kanban.ping.head",
      "{board_tag}{assignee_tag}Kanban {task_id}" },
    { "gateway.kanban.ping.implementer_suffix",
      " \342\206\222 implementer @{implementer}" },
    { "gateway.kanban.ping.kept_failing",
      "it kept failing" },
    { "gateway.kanban.ping.last_error",
      " (last: {value})" },
    { "gateway.kanban.ping.limit_generic",
      "its time limit" },
    { "gateway.kanban.ping.limit_minutes",
      "its {minutes}-minute limit" },
    { "gateway.kanban.ping.reason_suffix",
      ": {value}" },
    { "gateway.kanban.ping.review_requested",
      "\360\237\221\200 {head} ready for review \342\200\224 {title}{handoff}" },
    { "gateway.kanban.ping.reviewer_suffix",
      " \342\200\224 reviewer @{reviewer}" },
    { "gateway.kanban.ping.status",
      "\360\237\224\204 {head} \342\206\222 {status}" },
    { "gateway.kanban.ping.timed_out",
      "\342\217\261 {head} ran past {span} and was stopped; it will be retried automatically." },
    { "gateway.kanban.ping.triage",
      "\360\237\233\221 {head} routed to TRIAGE \342\200\224 {why}{recurrences}{reason}" },
    { "gateway.kanban.ping.triage_attention",
      "for orchestration attention" },
    { "gateway.kanban.ping.triage_decision",
      "needs a human decision" },
    { "gateway.kanban.ping.triage_recurrences",
      " (blocked {value}x for the same cause)" },
    { "gateway.kanban.wake.block_loop_detected",
      "routed to triage after repeated blocks; orchestration attention needed" },
    { "gateway.kanban.wake.blocked",
      "blocked; needs attention" },
    { "gateway.kanban.wake.changes_requested",
      "review requested changes (BLOCK); implementation is not approved" },
    { "gateway.kanban.wake.completed",
      "completed" },
    { "gateway.kanban.wake.crashed",
      "crashed (worker exited); dispatcher will retry" },
    { "gateway.kanban.wake.gave_up",
      "gave up (retries exhausted)" },
    { "gateway.kanban.wake.guidance",
      "This is an automatic task-status notification, not a request to decompose the task again. Inspect the current board before creating follow-up tasks; do not recreate tasks or graphs that already exist." },
    { "gateway.kanban.wake.handoff",
      "Result: {summary}" },
    { "gateway.kanban.wake.message",
      "[kanban] Task {task_id} {status}.\nTitle: {title}\nAssignee: @{assignee}\nBoard: {board}\n\nCheck the result or decide the next step." },
    { "gateway.kanban.wake.review_detail",
      "Review feedback: {reason}\nInspect the existing card and its current review run; return work to the same implementation task. Do not treat this BLOCK as approval and do not create a duplicate task." },
    { "gateway.kanban.wake.review_requested",
      "handed off for review; the implementation is done" },
    { "gateway.kanban.wake.status_default",
      "status changed" },
    { "gateway.kanban.wake.status_joiner",
      ", " },
    { "gateway.kanban.wake.timed_out",
      "timed out; dispatcher will retry" },
};
#define KW_N_CATALOG (sizeof(KW_CATALOG) / sizeof(KW_CATALOG[0]))

/* ------------------------------------------------------------- primitives */

typedef struct { char *p; size_t n, cap; } kw_buf;

static void kw_buf_free(kw_buf *b) {
    if (!b) return;
    free(b->p);
    b->p = NULL;
    b->n = b->cap = 0;
}

static int kw_buf_reserve(kw_buf *b, size_t extra) {
    size_t cap;
    char *np;
    if (b->n + extra + 1 <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + extra + 1) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}

static void kw_buf_putn(kw_buf *b, const char *s, size_t n) {
    if (!s || !n) return;
    if (!kw_buf_reserve(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void kw_buf_put(kw_buf *b, const char *s) {
    if (s) kw_buf_putn(b, s, strlen(s));
}

static char *kw_buf_take(kw_buf *b) {
    char *out;
    if (!b->p) {
        out = (char *)malloc(1);
        if (!out) return NULL;
        out[0] = '\0';
        b->n = b->cap = 0;
        return out;
    }
    out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

static char *kw_dup(const char *s) {
    size_t n;
    char *o;
    if (!s) s = "";
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

static char *kw_concat(const char *a, const char *b) {
    kw_buf buf;
    memset(&buf, 0, sizeof buf);
    kw_buf_put(&buf, a);
    kw_buf_put(&buf, b);
    return kw_buf_take(&buf);
}

/* Python truthiness of a decoded JSON value. */
static int kw_truthy(const json_t *v) {
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

/* Python str() over the shapes these payloads hold. */
static char *kw_pystr(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return kw_dup("None");
    switch (v->type) {
    case JSON_BOOL:
        return kw_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1.0e15 && d < 1.0e15) {
            snprintf(tmp, sizeof tmp, "%lld", (long long)d);
        } else {
            int p;
            for (p = 1; p <= 17; p++) {
                snprintf(tmp, sizeof tmp, "%.*g", p, d);
                if (strtod(tmp, NULL) == d) break;
            }
            if (!strpbrk(tmp, ".eEn")) strncat(tmp, ".0", sizeof tmp - strlen(tmp) - 1);
        }
        return kw_dup(tmp);
    }
    case JSON_STRING:
        return kw_dup(v->str_val ? v->str_val : "");
    default: {
        char *s = json_serialize(v);
        char *o = kw_dup(s ? s : "");
        free(s);
        return o;
    }
    }
}

/* Python int(); 1 on success. */
static int kw_pyint(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long got;
        if (!v->str_val) return 0;
        got = strtoll(v->str_val, &end, 10);
        if (end == v->str_val) return 0;
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        if (end && *end) return 0;
        *out = got;
        return 1;
    }
    return 0;
}

static int kw_ascii_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || (c >= 0x1c && c <= 0x1f) || c == 0x85;
}

/* Python strip() over ASCII whitespace. */
static char *kw_strip(const char *s) {
    size_t n, a = 0, b;
    if (!s) return kw_dup("");
    n = strlen(s);
    b = n;
    while (a < b && kw_ascii_space((unsigned char)s[a])) a++;
    while (b > a && kw_ascii_space((unsigned char)s[b - 1])) b--;
    {
        char *o = (char *)malloc(b - a + 1);
        if (!o) return NULL;
        memcpy(o, s + a, b - a);
        o[b - a] = '\0';
        return o;
    }
}

/* Byte length of the first ``limit`` code points of s. */
static size_t kw_cp_bytes(const char *s, size_t limit) {
    size_t i = 0, cps = 0, n;
    if (!s) return 0;
    n = strlen(s);
    while (i < n && cps < limit) {
        i++;
        while (i < n && (((unsigned char)s[i]) & 0xC0) == 0x80) i++;
        cps++;
    }
    return i;
}

static size_t kw_cp_count(const char *s) {
    size_t i = 0, cps = 0;
    if (!s) return 0;
    while (s[i]) {
        i++;
        while (s[i] && (((unsigned char)s[i]) & 0xC0) == 0x80) i++;
        cps++;
    }
    return cps;
}

static char *kw_prefix(const char *s, size_t limit) {
    size_t n;
    char *o;
    if (!s) return kw_dup("");
    n = kw_cp_bytes(s, limit);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* First line of a stripped text, [:limit].  ``text`` is the original. */
static char *kw_first_line(const char *text, size_t limit) {
    char *stripped, *line, *out;
    size_t i, end;
    if (!text) return kw_dup("");
    stripped = kw_strip(text);
    if (!stripped) return NULL;
    if (stripped[0] == '\0') {
        free(stripped);
        return kw_prefix(text, limit);
    }
    end = strlen(stripped);
    for (i = 0; i < end; i++) {
        unsigned char c = (unsigned char)stripped[i];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
            (c >= 0x1c && c <= 0x1e)) {
            end = i;
            break;
        }
    }
    line = (char *)malloc(end + 1);
    if (!line) { free(stripped); return NULL; }
    memcpy(line, stripped, end);
    line[end] = '\0';
    free(stripped);
    out = kw_prefix(line, limit);
    free(line);
    return out;
}

/* Python " ".join(s.split()). */
static char *kw_collapse_ws(const char *s) {
    kw_buf b;
    size_t i = 0;
    int first = 1;
    if (!s) return kw_dup("");
    memset(&b, 0, sizeof b);
    while (s[i]) {
        size_t start;
        while (s[i] && kw_ascii_space((unsigned char)s[i])) i++;
        if (!s[i]) break;
        start = i;
        while (s[i] && !kw_ascii_space((unsigned char)s[i])) i++;
        if (!first) kw_buf_putn(&b, " ", 1);
        kw_buf_putn(&b, s + start, i - start);
        first = 0;
    }
    return kw_buf_take(&b);
}

/* rstrip() over ASCII whitespace. */
static char *kw_rstrip(const char *s) {
    size_t n;
    char *o;
    if (!s) return kw_dup("");
    n = strlen(s);
    while (n > 0 && kw_ascii_space((unsigned char)s[n - 1])) n--;
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static int kw_word_byte(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

static int kw_rx_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* _LOCAL_PATH_RE match at i: end offset (> i), or i when there is no match. */
static size_t kw_local_path_end(const char *s, size_t n, size_t i) {
    static const char *const roots[] = {
        "Users", "home", "private", "tmp", "var", "etc", "workspace",
    };
    size_t r, j;
    if (i >= n) return i;
    if (i > 0) {
        unsigned char pc = (unsigned char)s[i - 1];
        if (kw_word_byte(pc) || pc == ':' || pc == '/') return i;
    }
    if (s[i] == '/') {
        for (r = 0; r < sizeof(roots) / sizeof(roots[0]); r++) {
            size_t rl = strlen(roots[r]);
            if (i + 1 + rl >= n) continue;
            if (strncmp(s + i + 1, roots[r], rl) != 0) continue;
            if (s[i + 1 + rl] != '/') continue;
            j = i + 2 + rl;
            while (j < n && !kw_rx_space((unsigned char)s[j]) &&
                   s[j] != ',' && s[j] != ';') j++;
            if (j > i + 2 + rl) return j;
            return i;
        }
        return i;
    }
    if (((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')) &&
        i + 2 < n && s[i + 1] == ':' && s[i + 2] == '\\') {
        j = i + 3;
        while (j < n && !kw_rx_space((unsigned char)s[j]) &&
               s[j] != ',' && s[j] != ';') j++;
        if (j > i + 3) return j;
    }
    return i;
}

/* re.sub(_LOCAL_PATH_RE, "[local path]", s) */
static char *kw_mask_local_paths(const char *s) {
    kw_buf b;
    size_t i = 0, n;
    if (!s) return kw_dup("");
    n = strlen(s);
    memset(&b, 0, sizeof b);
    while (i < n) {
        size_t end = kw_local_path_end(s, n, i);
        if (end > i) {
            kw_buf_put(&b, "[local path]");
            i = end;
        } else {
            kw_buf_putn(&b, s + i, 1);
            i++;
        }
    }
    return kw_buf_take(&b);
}

/* _safe_review_reason over already-redacted text. */
static char *kw_safe_reason(const char *redacted, long long limit) {
    char *masked, *collapsed, *cut, *rstripped, *out;
    size_t cps;
    if (limit < 0) limit = 160;
    masked = kw_mask_local_paths(redacted ? redacted : "");
    collapsed = kw_collapse_ws(masked);
    free(masked);
    cps = kw_cp_count(collapsed);
    if ((long long)cps <= limit) return collapsed;
    {
        long long stop = limit - 1;
        if (stop >= 0) cut = kw_prefix(collapsed, (size_t)stop);
        else if ((long long)cps + stop > 0) cut = kw_prefix(collapsed, (size_t)((long long)cps + stop));
        else cut = kw_dup("");
    }
    free(collapsed);
    rstripped = kw_rstrip(cut);
    free(cut);
    out = kw_concat(rstripped ? rstripped : "", "\342\200\246");
    free(rstripped);
    return out;
}

typedef struct { const char *name; const char *val; } kw_targ;

/* agent.i18n.t() over the embedded catalog (+ its bare-key fallback). */
static char *kw_t(const char *key, const kw_targ *args, size_t nargs) {
    const char *tpl = NULL;
    kw_buf b;
    size_t i, k;
    for (k = 0; k < KW_N_CATALOG; k++)
        if (!strcmp(KW_CATALOG[k].key, key)) { tpl = KW_CATALOG[k].tpl; break; }
    if (!tpl) return kw_dup(key);
    memset(&b, 0, sizeof b);
    for (i = 0; tpl[i]; i++) {
        if (tpl[i] == '{' && tpl[i + 1] == '{') { kw_buf_putn(&b, "{", 1); i++; continue; }
        if (tpl[i] == '}' && tpl[i + 1] == '}') { kw_buf_putn(&b, "}", 1); i++; continue; }
        if (tpl[i] == '{') {
            const char *end = strchr(tpl + i + 1, '}');
            int found = 0;
            if (!end) { kw_buf_putn(&b, "{", 1); continue; }
            for (k = 0; k < nargs; k++) {
                size_t nl = strlen(args[k].name);
                if (nl == (size_t)(end - (tpl + i + 1)) &&
                    !strncmp(args[k].name, tpl + i + 1, nl)) {
                    kw_buf_put(&b, args[k].val ? args[k].val : "");
                    found = 1;
                    break;
                }
            }
            if (!found) {  /* KeyError -> t() returns the unformatted value */
                kw_buf_free(&b);
                return kw_dup(tpl);
            }
            i = (size_t)(end - tpl);
            continue;
        }
        kw_buf_putn(&b, tpl + i, 1);
    }
    return kw_buf_take(&b);
}

static json_t *kw_jstr(char *owned) {
    json_t *out;
    if (!owned) return json_null();
    out = json_string(owned);
    free(owned);
    return out;
}

static int kw_in_kinds(const char *kind, const char *const *list, size_t n) {
    size_t i;
    if (!kind) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(kind, list[i])) return 1;
    return 0;
}

/* --------------------------------------------------------- list utilities */

typedef struct { char **v; size_t n, cap; } kw_list;

static void kw_list_free(kw_list *l) {
    size_t i;
    for (i = 0; i < l->n; i++) free(l->v[i]);
    free(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

static void kw_list_add_owned(kw_list *l, char *s) {
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **nv = (char **)realloc(l->v, cap * sizeof(char *));
        if (!nv) { free(s); return; }
        l->v = nv;
        l->cap = cap;
    }
    l->v[l->n++] = s;
}

/* Set semantics; NULL stands for Python None. */
static void kw_list_add_uniq(kw_list *l, const char *s) {
    size_t i;
    for (i = 0; i < l->n; i++) {
        if (!l->v[i] && !s) return;
        if (l->v[i] && s && !strcmp(l->v[i], s)) return;
    }
    kw_list_add_owned(l, s ? kw_dup(s) : NULL);
}

static int kw_cmp_none_first(const void *pa, const void *pb) {
    const char *a = *(const char *const *)pa;
    const char *b = *(const char *const *)pb;
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

static void kw_list_sort(kw_list *l) {
    if (l->n > 1) qsort(l->v, l->n, sizeof(char *), kw_cmp_none_first);
}

static json_t *kw_list_json(const kw_list *l) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < l->n; i++)
        json_append(arr, l->v[i] ? json_string(l->v[i]) : json_null());
    return arr;
}

/* ``getattr(platform, "value", str(platform)).lower()`` for one mapping entry:
 * a JSON string is itself, {"value": s} is an enum member. */
static char *kw_platform_name(const json_t *entry) {
    const json_t *src = entry;
    char *s;
    size_t i;
    if (!src || src->type == JSON_NULL) return kw_dup("none");
    if (src->type == JSON_OBJECT) src = json_obj_get(src, "value");
    if (!src) return kw_dup("none");
    s = kw_pystr(src);
    for (i = 0; s && s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    return s;
}

static void kw_platform_names_into(kw_list *out, const json_t *mapping) {
    size_t i, n;
    if (!mapping) return;
    if (mapping->type == JSON_OBJECT) {
        n = json_object_size(mapping);
        for (i = 0; i < n; i++) {
            json_t *tmp = json_string(json_object_get_key_at(mapping, i));
            char *name = kw_platform_name(tmp);
            json_free(tmp);
            if (name) { kw_list_add_uniq(out, name); free(name); }
        }
        return;
    }
    if (mapping->type != JSON_ARRAY) return;
    n = json_len(mapping);
    for (i = 0; i < n; i++) {
        char *name = kw_platform_name(json_get(mapping, i));
        if (name) { kw_list_add_uniq(out, name); free(name); }
    }
}

/* ------------------------------------------------------------ text context */

typedef struct {
    char *task_id;
    char *title;
    char *board_tag;
    char *head;
} kw_ctx;

static void kw_ctx_free(kw_ctx *c) {
    free(c->task_id);
    free(c->title);
    free(c->board_tag);
    free(c->head);
    memset(c, 0, sizeof *c);
}

/* _KanbanNotification.__init__ text shaping. */
static void kw_ctx_init(kw_ctx *ctx, const json_t *sub, const json_t *task, const json_t *board) {
    const json_t *tid = sub ? json_obj_get(sub, "task_id") : NULL;
    const json_t *assignee = NULL;
    char *src = NULL, *assignee_s = NULL, *board_s = NULL, *tag = NULL;
    kw_targ a[3];
    memset(ctx, 0, sizeof *ctx);
    ctx->task_id = kw_pystr(tid);
    if (task && task->type == JSON_OBJECT && json_obj_get(task, "title"))
        src = kw_pystr(json_obj_get(task, "title"));
    else
        src = kw_dup(ctx->task_id ? ctx->task_id : "");
    ctx->title = kw_prefix(src ? src : "", 120);
    free(src);

    if (kw_truthy(board)) board_s = kw_pystr(board);
    if (board_s) {
        kw_targ bt[1];
        bt[0].name = "board";
        bt[0].val = board_s;
        ctx->board_tag = kw_t("gateway.kanban.ping.board_tag", bt, 1);
    } else {
        ctx->board_tag = kw_dup("");
    }
    free(board_s);

    if (task && task->type == JSON_OBJECT) assignee = json_obj_get(task, "assignee");
    if (kw_truthy(assignee)) {
        kw_targ at[1];
        assignee_s = kw_pystr(assignee);
        at[0].name = "assignee";
        at[0].val = assignee_s;
        tag = kw_t("gateway.kanban.ping.assignee_tag", at, 1);
    } else {
        assignee_s = kw_dup("");
        tag = kw_dup("");
    }
    a[0].name = "board_tag";
    a[0].val = ctx->board_tag;
    a[1].name = "assignee_tag";
    a[1].val = tag;
    a[2].name = "task_id";
    a[2].val = ctx->task_id;
    ctx->head = kw_t("gateway.kanban.ping.head", a, 3);
    free(tag);
    free(assignee_s);
}

/* _payload(ev, key) */
static const json_t *kw_payload_get(const json_t *ev, const char *key) {
    const json_t *payload = ev ? json_obj_get(ev, "payload") : NULL;
    const json_t *v;
    if (!payload || !kw_truthy(payload)) return NULL;
    v = json_obj_get(payload, key);
    return kw_truthy(v) ? v : NULL;
}

/* _clip(ev, key, msg_key, limit) */
static char *kw_clip(const json_t *ev, const char *key, const char *tkey, size_t limit) {
    const json_t *v = kw_payload_get(ev, key);
    char *s, *cut, *out;
    kw_targ a[1];
    if (!v) return kw_dup("");
    s = kw_pystr(v);
    cut = kw_prefix(s ? s : "", limit);
    free(s);
    a[0].name = "value";
    a[0].val = cut;
    out = kw_t(tkey, a, 1);
    free(cut);
    return out;
}

/* One formatter.  *msg NULL => silent kind; *handoff / *rd NULL => untouched. */
static void kw_format_one(const kw_ctx *ctx, const json_t *task, const json_t *ev,
                          char **msg, char **handoff, char **rd) {
    const char *kind = ev ? json_get_str(ev, "kind", "") : "";
    *msg = NULL;
    *handoff = NULL;
    *rd = NULL;

    if (!strcmp(kind, "completed")) {
        const json_t *summary = kw_payload_get(ev, "summary");
        char *h = NULL, *handoff_text, *body;
        kw_targ a[3];
        if (summary) {
            char *s = kw_pystr(summary);
            h = kw_first_line(s ? s : "", 200);
            free(s);
        } else if (task && kw_truthy(json_obj_get(task, "result"))) {
            h = kw_first_line(json_get_str(task, "result", ""), 160);
        }
        handoff_text = h ? kw_concat("\n", h) : kw_dup("");
        a[0].name = "head";    a[0].val = ctx->head;
        a[1].name = "title";   a[1].val = ctx->title;
        a[2].name = "handoff"; a[2].val = handoff_text;
        body = kw_t("gateway.kanban.ping.completed", a, 3);
        *msg = body;
        *handoff = h;
        free(handoff_text);
        return;
    }
    if (!strcmp(kind, "review_requested")) {
        const json_t *summary = kw_payload_get(ev, "summary");
        char *handoff_text = kw_dup(""), *h = NULL, *body;
        kw_targ a[3];
        if (summary) {
            char *s = kw_pystr(summary);
            char *cut = kw_prefix(s ? s : "", 200);
            free(handoff_text);
            handoff_text = kw_concat("\n", cut);
            h = kw_first_line(s ? s : "", 200);
            free(cut);
            free(s);
        }
        a[0].name = "head";    a[0].val = ctx->head;
        a[1].name = "title";   a[1].val = ctx->title;
        a[2].name = "handoff"; a[2].val = handoff_text;
        body = kw_t("gateway.kanban.ping.review_requested", a, 3);
        *msg = body;
        *handoff = h;
        free(handoff_text);
        return;
    }
    if (!strcmp(kind, "changes_requested")) {
        const json_t *payload = json_obj_get(ev, "payload");
        const json_t *rv, *iv;
        char *reason, *reviewer, *implementer, *reason_text, *provenance, *body;
        kw_targ a[4];
        if (!payload || payload->type != JSON_OBJECT) payload = NULL;
        rv = payload ? json_obj_get(payload, "reviewer") : NULL;
        iv = payload ? json_obj_get(payload, "implementer") : NULL;
        {
            const json_t *raw = payload ? json_obj_get(payload, "reason") : NULL;
            char *raw_s = (!raw || raw->type == JSON_NULL) ? kw_dup("") : kw_pystr(raw);
            reason = kw_safe_reason(raw_s ? raw_s : "", 160);
            free(raw_s);
        }
        {
            char *raw_s = (!rv || rv->type == JSON_NULL) ? kw_dup("") : kw_pystr(rv);
            reviewer = kw_safe_reason(raw_s ? raw_s : "", 48);
            free(raw_s);
        }
        {
            char *raw_s = (!iv || iv->type == JSON_NULL) ? kw_dup("") : kw_pystr(iv);
            implementer = kw_safe_reason(raw_s ? raw_s : "", 48);
            free(raw_s);
        }
        if (reason && reason[0]) {
            reason_text = kw_dup(reason);
        } else {
            reason_text = kw_t("gateway.kanban.ping.changes_default_reason", NULL, 0);
        }
        if (reviewer && reviewer[0]) {
            kw_targ ra[1];
            ra[0].name = "reviewer";
            ra[0].val = reviewer;
            provenance = kw_t("gateway.kanban.ping.reviewer_suffix", ra, 1);
        } else {
            provenance = kw_dup("");
        }
        if (implementer && implementer[0]) {
            kw_targ ia[1];
            char *more, *joined;
            ia[0].name = "implementer";
            ia[0].val = implementer;
            more = kw_t("gateway.kanban.ping.implementer_suffix", ia, 1);
            joined = kw_concat(provenance, more);
            free(more);
            free(provenance);
            provenance = joined;
        }
        a[0].name = "board_tag";  a[0].val = ctx->board_tag;
        a[1].name = "task_id";    a[1].val = ctx->task_id;
        a[2].name = "reason";     a[2].val = reason_text;
        a[3].name = "provenance"; a[3].val = provenance;
        body = kw_t("gateway.kanban.ping.changes_requested", a, 4);
        *msg = body;
        *rd = reason_text;
        free(reason);
        free(reviewer);
        free(implementer);
        free(provenance);
        return;
    }
    if (!strcmp(kind, "block_loop_detected")) {
        const json_t *kindv = kw_payload_get(ev, "kind");
        int decision = kindv && kindv->type == JSON_STRING && kindv->str_val &&
                       !strcmp(kindv->str_val, "needs_input");
        char *recurrences = kw_clip(ev, "recurrences", "gateway.kanban.ping.triage_recurrences", 200);
        char *reason = kw_clip(ev, "reason", "gateway.kanban.ping.reason_suffix", 160);
        char *why = kw_t(decision ? "gateway.kanban.ping.triage_decision"
                                  : "gateway.kanban.ping.triage_attention", NULL, 0);
        kw_targ a[4];
        a[0].name = "head";        a[0].val = ctx->head;
        a[1].name = "why";         a[1].val = why;
        a[2].name = "recurrences"; a[2].val = recurrences;
        a[3].name = "reason";      a[3].val = reason;
        *msg = kw_t("gateway.kanban.ping.triage", a, 4);
        free(recurrences);
        free(reason);
        free(why);
        return;
    }
    if (!strcmp(kind, "gave_up")) {
        const json_t *failv = kw_payload_get(ev, "failures");
        char *count, *last, *body;
        kw_targ b[4];
        if (failv) {
            long long n = 0;
            char num[32];
            kw_targ ca[1];
            kw_pyint(failv, &n);
            snprintf(num, sizeof num, "%lld", n);
            ca[0].name = "count";
            ca[0].val = num;
            count = kw_t("gateway.kanban.ping.failed_n_times", ca, 1);
        } else {
            count = kw_t("gateway.kanban.ping.kept_failing", NULL, 0);
        }
        last = kw_clip(ev, "error", "gateway.kanban.ping.last_error", 160);
        b[0].name = "head";    b[0].val = ctx->head;
        b[1].name = "count";   b[1].val = count;
        b[2].name = "last";    b[2].val = last;
        b[3].name = "task_id"; b[3].val = ctx->task_id;
        body = kw_t("gateway.kanban.ping.gave_up", b, 4);
        *msg = body;
        free(count);
        free(last);
        return;
    }
    if (!strcmp(kind, "timed_out")) {
        const json_t *limv = kw_payload_get(ev, "limit_seconds");
        long long limit = 0, minutes = 0;
        char *span, *body;
        kw_targ a[2];
        if (limv) kw_pyint(limv, &limit);
        if (limit) {
            long long m = (long long)nearbyint((double)limit / 60.0);
            minutes = m > 1 ? m : 1;
        }
        if (minutes) {
            char num[32];
            kw_targ sa[1];
            snprintf(num, sizeof num, "%lld", minutes);
            sa[0].name = "minutes";
            sa[0].val = num;
            span = kw_t("gateway.kanban.ping.limit_minutes", sa, 1);
        } else {
            span = kw_t("gateway.kanban.ping.limit_generic", NULL, 0);
        }
        a[0].name = "head"; a[0].val = ctx->head;
        a[1].name = "span"; a[1].val = span;
        body = kw_t("gateway.kanban.ping.timed_out", a, 2);
        *msg = body;
        free(span);
        return;
    }
    if (!strcmp(kind, "blocked")) {
        char *reason = kw_clip(ev, "reason", "gateway.kanban.ping.reason_suffix", 160);
        kw_targ a[2];
        a[0].name = "head";   a[0].val = ctx->head;
        a[1].name = "reason"; a[1].val = reason;
        *msg = kw_t("gateway.kanban.ping.blocked", a, 2);
        free(reason);
        return;
    }
    if (!strcmp(kind, "crashed")) {
        kw_targ a[1];
        a[0].name = "head";
        a[0].val = ctx->head;
        *msg = kw_t("gateway.kanban.ping.crashed", a, 1);
        return;
    }
    if (!strcmp(kind, "status")) {
        const json_t *sv = kw_payload_get(ev, "status");
        char *status = sv ? kw_pystr(sv) : kw_dup("");
        kw_targ a[2];
        a[0].name = "head";   a[0].val = ctx->head;
        a[1].name = "status"; a[1].val = status;
        *msg = kw_t("gateway.kanban.ping.status", a, 2);
        free(status);
        return;
    }
    /* archived / unblocked / unknown kinds: claimed but silent. */
}

/* ------------------------------------------------------- p1: exported API */

json_t *w30_kanbanwatc_3847d4_p1_constants(void) {
    json_t *o = json_object();
    json_t *term = json_array();
    json_t *wake = json_array();
    json_t *threads = json_array();
    size_t i;
    for (i = 0; i < KW_N_TERMINAL; i++) json_append(term, json_string(KW_TERMINAL_KINDS[i]));
    for (i = 0; i < KW_N_WAKE; i++) json_append(wake, json_string(KW_WAKE_KINDS[i]));
    for (i = 0; i < KW_N_THREAD; i++) json_append(threads, json_string(KW_THREAD_CHAT_TYPES[i]));
    json_set(o, "max_send_failures", json_number((double)KW_MAX_SEND_FAILURES));
    json_set(o, "terminal_kinds", term);
    json_set(o, "wake_kinds", wake);
    json_set(o, "thread_chat_types", threads);
    return o;
}

json_t *w30_kanbanwatc_3847d4_p1_diagnostic_event(const json_t *req) {
    const json_t *ev = req ? json_obj_get(req, "event") : NULL;
    const char *kind = ev ? json_get_str(ev, "kind", "") : "";
    const json_t *payload = ev ? json_obj_get(ev, "payload") : NULL;
    if (!payload || !kw_truthy(payload)) payload = NULL;
    if (!strcmp(kind, "crashed") || !strcmp(kind, "timed_out") || !strcmp(kind, "gave_up"))
        return json_bool(true);
    if (!strcmp(kind, "blocked") || !strcmp(kind, "block_loop_detected")) {
        const json_t *k = payload ? json_obj_get(payload, "kind") : NULL;
        int needs_input = k && k->type == JSON_STRING && k->str_val &&
                          !strcmp(k->str_val, "needs_input");
        return json_bool(!needs_input);
    }
    if (!strcmp(kind, "status")) {
        const json_t *s = payload ? json_obj_get(payload, "status") : NULL;
        int blocked = s && s->type == JSON_STRING && s->str_val && !strcmp(s->str_val, "blocked");
        int triage = s && s->type == JSON_STRING && s->str_val && !strcmp(s->str_val, "triage");
        return json_bool(blocked || triage);
    }
    return json_bool(false);
}

json_t *w30_kanbanwatc_3847d4_p1_first_line(const json_t *req) {
    const json_t *text = req ? json_obj_get(req, "text") : NULL;
    long long limit = 160;
    if (req && json_obj_get(req, "limit")) kw_pyint(json_obj_get(req, "limit"), &limit);
    if (!text || text->type != JSON_STRING) return json_null();
    return kw_jstr(kw_first_line(text->str_val ? text->str_val : "",
                                 (size_t)(limit < 0 ? 0 : limit)));
}

json_t *w30_kanbanwatc_3847d4_p1_clip(const json_t *req) {
    const json_t *ev = req ? json_obj_get(req, "event") : NULL;
    const char *key = req ? json_get_str(req, "key", "") : "";
    const char *tkey = req ? json_get_str(req, "tkey", "") : "";
    long long limit = 160;
    if (req && json_obj_get(req, "limit")) kw_pyint(json_obj_get(req, "limit"), &limit);
    return kw_jstr(kw_clip(ev, key, tkey, (size_t)(limit < 0 ? 0 : limit)));
}

json_t *w30_kanbanwatc_3847d4_p1_safe_review_reason(const json_t *req) {
    const json_t *text = req ? json_obj_get(req, "redacted") : NULL;
    long long limit = 160;
    char *s, *out;
    if (req && json_obj_get(req, "limit")) kw_pyint(json_obj_get(req, "limit"), &limit);
    s = (!text || text->type == JSON_NULL) ? kw_dup("") : kw_pystr(text);
    out = kw_safe_reason(s ? s : "", limit);
    free(s);
    return kw_jstr(out);
}

json_t *w30_kanbanwatc_3847d4_p1_format_event(const json_t *req) {
    const json_t *ev = req ? json_obj_get(req, "event") : NULL;
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const json_t *task = req ? json_obj_get(req, "task") : NULL;
    const json_t *board = req ? json_obj_get(req, "board") : NULL;
    kw_ctx ctx;
    char *msg = NULL, *handoff = NULL, *rd = NULL;
    json_t *out = json_object();
    if (task && task->type != JSON_OBJECT) task = NULL;
    kw_ctx_init(&ctx, sub, task, board);
    kw_format_one(&ctx, task, ev, &msg, &handoff, &rd);
    json_set(out, "msg", msg ? kw_jstr(msg) : json_null());
    json_set(out, "handoff", handoff ? kw_jstr(handoff) : json_null());
    json_set(out, "review_detail", rd ? kw_jstr(rd) : json_null());
    kw_ctx_free(&ctx);
    return out;
}

static json_t *kw_batch_run(const json_t *req, int with_ids) {
    const json_t *events = req ? json_obj_get(req, "events") : NULL;
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const json_t *task = req ? json_obj_get(req, "task") : NULL;
    const json_t *board = req ? json_obj_get(req, "board") : NULL;
    int is_push = req ? (json_get_bool(req, "is_push_adapter", true) ? 1 : 0) : 1;
    const char *mode = sub ? json_get_str(sub, "delivery_mode", "") : "";
    int wake_agent, send_passive;
    json_t *messages = json_array();
    json_t *ids = json_array();
    json_t *out = json_object();
    kw_ctx ctx;
    char *handoff, *rd;
    size_t i, n;

    if (task && task->type != JSON_OBJECT) task = NULL;
    if (!mode || !mode[0]) mode = "notify";
    wake_agent = !strcmp(mode, "notify+wake") || !strcmp(mode, "wake");
    send_passive = strcmp(mode, "wake") != 0;
    kw_ctx_init(&ctx, sub, task, board);
    handoff = kw_dup("");
    rd = kw_dup("");

    n = (events && events->type == JSON_ARRAY) ? json_len(events) : 0;
    for (i = 0; i < n; i++) {
        const json_t *ev = json_get(events, i);
        char *msg = NULL, *h = NULL, *r = NULL;
        int has_msg;
        kw_format_one(&ctx, task, ev, &msg, &h, &r);
        if (h) { free(handoff); handoff = h; }
        if (r) { free(rd); rd = r; }
        has_msg = msg ? 1 : 0;
        json_append(messages, msg ? kw_jstr(msg) : json_null());  /* kw_jstr owns msg */
        msg = NULL;
        if (with_ids) {
            long long last = 0, id = 0;
            int send_it = has_msg;
            if (send_it && !is_push && wake_agent) send_it = 0;
            if (send_it && !send_passive) send_it = 0;
            if (send_it) {
                if (sub && json_obj_get(sub, "last_ping_event_id"))
                    kw_pyint(json_obj_get(sub, "last_ping_event_id"), &last);
                if (ev && json_obj_get(ev, "id")) kw_pyint(json_obj_get(ev, "id"), &id);
                if (id <= last) send_it = 0;
            }
            if (send_it) json_append(ids, json_number((double)id));
        }
    }
    if (with_ids) json_set(out, "ids", ids);
    else json_free(ids);
    json_set(out, "messages", messages);
    json_set(out, "wake_handoff", kw_jstr(handoff));
    json_set(out, "wake_review_detail", kw_jstr(rd));
    kw_ctx_free(&ctx);
    return out;
}

json_t *w30_kanbanwatc_3847d4_p1_format_batch(const json_t *req) {
    return kw_batch_run(req, 0);
}

json_t *w30_kanbanwatc_3847d4_p1_ping_plan(const json_t *req) {
    return kw_batch_run(req, 1);
}

/* ------------------------------------------------------- p2: exported API */

json_t *w30_kanbanwatc_3847d4_p2_platform_names(const json_t *req) {
    kw_list out;
    json_t *arr;
    memset(&out, 0, sizeof out);
    kw_platform_names_into(&out, req ? json_obj_get(req, "mapping") : NULL);
    kw_list_sort(&out);
    arr = kw_list_json(&out);
    kw_list_free(&out);
    return arr;
}

json_t *w30_kanbanwatc_3847d4_p2_chat_type_of(const json_t *req) {
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const json_t *md = req ? json_obj_get(req, "metadata") : NULL;
    const json_t *v = sub ? json_obj_get(sub, "chat_type") : NULL;
    char *raw, *stripped;
    size_t i;
    if (md && md->type != JSON_OBJECT) md = NULL;
    if (!kw_truthy(v) && md) v = json_obj_get(md, "chat_type");
    if (!kw_truthy(v)) return json_string("");
    raw = kw_pystr(v);
    stripped = kw_strip(raw ? raw : "");
    free(raw);
    for (i = 0; stripped && stripped[i]; i++)
        if (stripped[i] >= 'A' && stripped[i] <= 'Z')
            stripped[i] = (char)(stripped[i] - 'A' + 'a');
    return kw_jstr(stripped);
}

json_t *w30_kanbanwatc_3847d4_p2_wake_scope_id(const json_t *req) {
    static const char *const keys[] = { "scope_id", "guild_id", "slack_team_id", "team_id" };
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const json_t *md = sub ? json_obj_get(sub, "delivery_metadata") : NULL;
    const json_t *resolver = req ? json_obj_get(req, "resolver") : NULL;
    size_t i;
    if (md && md->type == JSON_OBJECT) {
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const json_t *v = json_obj_get(md, keys[i]);
            if (kw_truthy(v)) return kw_jstr(kw_pystr(v));
        }
    }
    if (!resolver || resolver->type != JSON_OBJECT) return json_null();
    if (!json_get_bool(resolver, "present", false)) return json_null();
    {
        const json_t *v = json_obj_get(resolver, "value");
        if (!kw_truthy(v)) return json_null();
        return kw_jstr(kw_pystr(v));
    }
}

/* (task_id, platform, chat_id, thread_id or "") as a JSON array. */
static json_t *kw_warn_key(const json_t *sub, const char *platform) {
    json_t *key = json_array();
    const json_t *tid = sub ? json_obj_get(sub, "task_id") : NULL;
    const json_t *cid = sub ? json_obj_get(sub, "chat_id") : NULL;
    const json_t *thid = sub ? json_obj_get(sub, "thread_id") : NULL;
    json_append(key, tid ? json_copy(tid) : json_null());
    json_append(key, json_string(platform ? platform : ""));
    json_append(key, cid ? json_copy(cid) : json_null());
    if (kw_truthy(thid)) json_append(key, json_copy(thid));
    else json_append(key, json_string(""));
    return key;
}

static int kw_key_in(const json_t *warned, const json_t *key) {
    size_t i, n;
    if (!warned || warned->type != JSON_ARRAY || !key) return 0;
    n = json_len(warned);
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(warned, i);
        size_t j, m;
        int same = 1;
        if (!row || row->type != JSON_ARRAY) continue;
        if (json_len(row) != json_len(key)) continue;
        m = json_len(row);
        for (j = 0; j < m && same; j++) {
            char *as = kw_pystr(json_get(row, j));
            char *bs = kw_pystr(json_get(key, j));
            if (!as || !bs || strcmp(as, bs) != 0) same = 0;
            free(as);
            free(bs);
        }
        if (same) return 1;
    }
    return 0;
}

/* _chat_type_of for the anchorless decision. */
static char *kw_chat_type_raw(const json_t *sub, const json_t *md) {
    const json_t *v = sub ? json_obj_get(sub, "chat_type") : NULL;
    char *raw, *stripped;
    size_t i;
    if (!kw_truthy(v) && md) v = json_obj_get(md, "chat_type");
    if (!kw_truthy(v)) return kw_dup("");
    raw = kw_pystr(v);
    stripped = kw_strip(raw ? raw : "");
    free(raw);
    for (i = 0; stripped && stripped[i]; i++)
        if (stripped[i] >= 'A' && stripped[i] <= 'Z')
            stripped[i] = (char)(stripped[i] - 'A' + 'a');
    return stripped;
}

json_t *w30_kanbanwatc_3847d4_p2_anchorless_warn(const json_t *req) {
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const char *platform = req ? json_get_str(req, "platform", "") : "";
    const json_t *warned = req ? json_obj_get(req, "warned") : NULL;
    const json_t *md = sub ? json_obj_get(sub, "delivery_metadata") : NULL;
    const json_t *thread = sub ? json_obj_get(sub, "thread_id") : NULL;
    json_t *out = json_object();
    json_t *key;
    int thread_like;

    if (md && md->type != JSON_OBJECT) md = NULL;
    thread_like = kw_truthy(thread);
    if (!thread_like) {
        char *ct = kw_chat_type_raw(sub, md);
        thread_like = kw_in_kinds(ct, KW_THREAD_CHAT_TYPES, KW_N_THREAD);
        free(ct);
    }
    if (!thread_like || (md && kw_truthy(json_obj_get(md, "parent_chat_id")))) {
        json_set(out, "warn", json_bool(false));
        json_set(out, "key", json_null());
        return out;
    }
    key = kw_warn_key(sub, platform);
    json_set(out, "warn", json_bool(!kw_key_in(warned, key)));
    json_set(out, "key", key);
    return out;
}

json_t *w30_kanbanwatc_3847d4_p2_unroutable_warn(const json_t *req) {
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const char *platform = req ? json_get_str(req, "platform", "") : "";
    const json_t *warned = req ? json_obj_get(req, "warned") : NULL;
    json_t *out = json_object();
    json_t *key = kw_warn_key(sub, platform);
    json_set(out, "warn", json_bool(!kw_key_in(warned, key)));
    json_set(out, "key", key);
    return out;
}

json_t *w30_kanbanwatc_3847d4_p2_collect_scope(const json_t *req) {
    const json_t *notifier = req ? json_obj_get(req, "notifier_profile") : NULL;
    const json_t *padapters = req ? json_obj_get(req, "profile_adapters") : NULL;
    const json_t *adapters = req ? json_obj_get(req, "adapters") : NULL;
    const json_t *routes = req ? json_obj_get(req, "routes") : NULL;
    int multiplex = req ? (json_get_bool(req, "multiplex", false) ? 1 : 0) : 0;
    kw_list profiles, active, adapter_names;
    json_t *out = json_object();
    size_t i, n;

    memset(&profiles, 0, sizeof profiles);
    memset(&active, 0, sizeof active);
    memset(&adapter_names, 0, sizeof adapter_names);

    kw_platform_names_into(&adapter_names, adapters);
    kw_list_add_uniq(&profiles, (notifier && notifier->type == JSON_STRING) ? notifier->str_val : NULL);

    if (padapters && padapters->type == JSON_OBJECT) {
        n = json_object_size(padapters);
        for (i = 0; i < n; i++) {
            char *stripped = kw_strip(json_object_get_key_at(padapters, i));
            if (stripped && stripped[0]) kw_list_add_uniq(&profiles, stripped);
            free(stripped);
        }
        for (i = 0; i < n; i++)
            kw_platform_names_into(&active, json_object_get_at(padapters, i));
    }
    for (i = 0; i < adapter_names.n; i++) kw_list_add_uniq(&active, adapter_names.v[i]);

    if (multiplex && routes && routes->type == JSON_ARRAY) {
        n = json_len(routes);
        for (i = 0; i < n; i++) {
            const json_t *route = json_get(routes, i);
            const json_t *pf, *enabled, *plat;
            size_t k;
            int hit = 0;
            if (!route || route->type != JSON_OBJECT) continue;
            pf = json_obj_get(route, "profile");
            enabled = json_obj_get(route, "enabled");
            plat = json_obj_get(route, "platform");
            if (!kw_truthy(enabled)) continue;
            if (!plat || plat->type != JSON_STRING) continue;  /* a Platform enum never equals a str */
            for (k = 0; k < adapter_names.n; k++)
                if (adapter_names.v[k] && !strcmp(adapter_names.v[k], plat->str_val)) { hit = 1; break; }
            if (!hit) continue;
            if (pf && pf->type == JSON_STRING) kw_list_add_uniq(&profiles, pf->str_val);
        }
    }

    kw_list_sort(&profiles);
    kw_list_sort(&active);
    json_set(out, "notifier_profiles", kw_list_json(&profiles));
    json_set(out, "active_platforms", kw_list_json(&active));
    kw_list_free(&profiles);
    kw_list_free(&active);
    kw_list_free(&adapter_names);
    return out;
}

json_t *w30_kanbanwatc_3847d4_p2_resolve_adapter(const json_t *req) {
    const char *platform = req ? json_get_str(req, "platform", "") : "";
    const json_t *owner = req ? json_obj_get(req, "owner_profile") : NULL;
    const json_t *notifier = req ? json_obj_get(req, "notifier_profile") : NULL;
    const char *primary_profile = req ? json_get_str(req, "primary_profile", "") : "";
    int multiplex = req ? (json_get_bool(req, "multiplex", false) ? 1 : 0) : 0;
    const char *adapter = req ? json_get_str(req, "adapter", "absent") : "absent";
    int has_primary = req ? (json_get_bool(req, "has_primary", true) ? 1 : 0) : 1;
    const json_t *own = req ? json_obj_get(req, "own_platforms") : NULL;
    const json_t *served = req ? json_obj_get(req, "served") : NULL;
    const json_t *routes = req ? json_obj_get(req, "routes") : NULL;
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    int api_server_owned = req ? (json_get_bool(req, "api_server_owned", false) ? 1 : 0) : 0;
    json_t *out = json_object();
    const char *result = "absent";
    json_t *warn_out = json_null();
    char *platform_lc = kw_dup(platform ? platform : "");
    char *profile = NULL;
    size_t i, n, k;

    for (i = 0; platform_lc && platform_lc[i]; i++)
        if (platform_lc[i] >= 'A' && platform_lc[i] <= 'Z')
            platform_lc[i] = (char)(platform_lc[i] - 'A' + 'a');

    if (!multiplex) {
        result = adapter;
        goto done;
    }
    if (strcmp(adapter, "absent") != 0 && strcmp(adapter, "primary") != 0) {
        result = adapter;
        goto done;
    }
    if (owner && owner->type == JSON_STRING && kw_truthy(owner))
        profile = kw_dup(owner->str_val);
    else if (notifier && notifier->type == JSON_STRING && kw_truthy(notifier))
        profile = kw_dup(notifier->str_val);
    else
        profile = kw_dup(primary_profile ? primary_profile : "");

    {
        int owns = 0;
        if (own && own->type == JSON_ARRAY) {
            n = json_len(own);
            for (k = 0; k < n && !owns; k++) {
                char *nm = kw_platform_name(json_get(own, k));
                if (nm && platform_lc && !strcmp(nm, platform_lc)) owns = 1;
                free(nm);
            }
        } else if (own && own->type == JSON_OBJECT) {
            n = json_object_size(own);
            for (k = 0; k < n && !owns; k++) {
                json_t *tmp = json_string(json_object_get_key_at(own, k));
                char *nm = kw_platform_name(tmp);
                json_free(tmp);
                if (nm && platform_lc && !strcmp(nm, platform_lc)) owns = 1;
                free(nm);
            }
        }
        if (owns) {
            result = "absent";
            goto done;
        }
    }

    n = (routes && routes->type == JSON_ARRAY) ? json_len(routes) : 0;
    for (i = 0; i < n; i++) {
        const json_t *route = json_get(routes, i);
        const json_t *pf;
        if (!route || route->type != JSON_OBJECT) continue;
        pf = json_obj_get(route, "profile");
        if (json_get_bool(route, "plain", false)) {
            const char *route_profile = (pf && pf->type == JSON_STRING && pf->str_val) ? pf->str_val : "";
            if (!profile || !primary_profile || strcmp(route_profile, profile) != 0) {
                json_t *w = json_object();
                json_set(w, "key", kw_warn_key(sub, platform));
                json_set(w, "profile", json_string(profile ? profile : ""));
                json_set(w, "route_profile", json_string(route_profile));
                json_free(warn_out);
                warn_out = w;
                result = "absent";
                goto done;
            }
            {
                int in_served = 0;
                if (served && served->type == JSON_ARRAY) {
                    size_t m = json_len(served);
                    for (k = 0; k < m; k++) {
                        const json_t *sv = json_get(served, k);
                        if (sv && sv->type == JSON_STRING && sv->str_val &&
                            !strcmp(sv->str_val, profile)) in_served = 1;
                    }
                }
                result = (in_served && has_primary) ? "primary" : "absent";
                goto done;
            }
        }
        if (json_get_bool(route, "relaxed", false)) {
            result = "absent";
            goto done;
        }
    }
    if (profile && primary_profile && strcmp(profile, primary_profile) != 0 &&
        platform_lc && !strcmp(platform_lc, "api_server") && api_server_owned) {
        result = has_primary ? "primary" : "absent";
        goto done;
    }
    result = (has_primary && profile && primary_profile && !strcmp(profile, primary_profile))
                 ? "primary"
                 : "absent";
done:
    json_set(out, "adapter", json_string(result ? result : "absent"));
    json_set(out, "warn", warn_out);
    free(profile);
    free(platform_lc);
    return out;
}

/* ---- wake planning (build_wake_text + deliver()'s split + session key) ---- */

typedef struct {
    char *handoff;
    char *rd;
    char *session_key;
    char *synth;
    int kind_flags[KW_N_WAKE];
    int n_kinds;
    int diagnostic;
    int has_diagnostic;
} kw_wake_state;

static int kw_kind_index(const char *kind) {
    size_t i;
    for (i = 0; i < KW_N_WAKE; i++)
        if (!strcmp(kind, KW_WAKE_KINDS[i])) return (int)i;
    return -1;
}

static int kw_is_diagnostic(const json_t *ev) {
    json_t *wrapped = json_object();
    json_t *probe;
    int out;
    json_set(wrapped, "event", ev ? json_copy(ev) : json_null());
    probe = w30_kanbanwatc_3847d4_p1_diagnostic_event(wrapped);
    out = probe && probe->type == JSON_BOOL && probe->bool_val;
    json_free(probe);
    json_free(wrapped);
    return out;
}

static void kw_build_wake_text(const kw_ctx *ctx, const json_t *sub, const json_t *task,
                               const json_t *board, const json_t *events, int wake_agent,
                               int is_push, kw_wake_state *st) {
    size_t i, n = (events && events->type == JSON_ARRAY) ? json_len(events) : 0;
    int any_kinds = 0, all_diag = 1;
    char *status_joined, *synth;
    kw_targ b[5];

    for (i = 0; i < KW_N_WAKE; i++) st->kind_flags[i] = 0;
    st->n_kinds = 0;
    if (wake_agent) {
        for (i = 0; i < n; i++) {
            const json_t *ev = json_get(events, i);
            int idx = kw_kind_index(ev ? json_get_str(ev, "kind", "") : "");
            if (idx >= 0) st->kind_flags[idx] = 1;
        }
    }
    for (i = 0; i < KW_N_WAKE; i++) if (st->kind_flags[i]) { st->n_kinds++; any_kinds = 1; }
    for (i = 0; i < n; i++) {
        const json_t *ev = json_get(events, i);
        int idx = kw_kind_index(ev ? json_get_str(ev, "kind", "") : "");
        if (idx < 0 || !st->kind_flags[idx]) continue;
        if (!kw_is_diagnostic(ev)) all_diag = 0;
    }
    st->diagnostic = all_diag;
    st->has_diagnostic = 1;
    if (!any_kinds) return;

    {
        kw_buf sk;
        memset(&sk, 0, sizeof sk);
        if (is_push) {
            const json_t *sid = task ? json_obj_get(task, "session_id") : NULL;
            if (kw_truthy(sid)) kw_buf_put(&sk, kw_pystr(sid));
        } else {
            const json_t *cid = sub ? json_obj_get(sub, "chat_id") : NULL;
            const json_t *sid = task ? json_obj_get(task, "session_id") : NULL;
            if (kw_truthy(cid)) kw_buf_put(&sk, kw_pystr(cid));
            else if (kw_truthy(sid)) kw_buf_put(&sk, kw_pystr(sid));
        }
        free(st->session_key);
        st->session_key = kw_buf_take(&sk);
    }

    {
        kw_buf parts;
        memset(&parts, 0, sizeof parts);
        for (i = 0; i < KW_N_WAKE; i++) {
            char key[64];
            char *piece;
            if (!st->kind_flags[i]) continue;
            snprintf(key, sizeof key, "gateway.kanban.wake.%s", KW_WAKE_KINDS[i]);
            piece = kw_t(key, NULL, 0);
            if (piece && piece[0]) {
                if (parts.n) kw_buf_put(&parts, ", ");
                kw_buf_put(&parts, piece);
            }
            free(piece);
        }
        status_joined = kw_buf_take(&parts);
        if (status_joined && !status_joined[0]) {
            free(status_joined);
            status_joined = kw_t("gateway.kanban.wake.status_default", NULL, 0);
        }
    }

    {
        char *task_id_s = kw_dup(ctx->task_id ? ctx->task_id : "");
        char *assignee_s = kw_dup(""), *board_s;
        if (task && task->type == JSON_OBJECT) {
            /* task.assignee if task else "" -- a present task with no assignee is None. */
            free(assignee_s);
            assignee_s = kw_pystr(json_obj_get(task, "assignee"));
        }
        board_s = (board && board->type != JSON_NULL) ? kw_pystr(board) : kw_dup("None");
        b[0].name = "task_id";  b[0].val = task_id_s;
        b[1].name = "status";   b[1].val = status_joined;
        b[2].name = "title";    b[2].val = ctx->title;
        b[3].name = "assignee"; b[3].val = assignee_s;
        b[4].name = "board";    b[4].val = board_s;
        synth = kw_t("gateway.kanban.wake.message", b, 5);
        free(task_id_s);
        free(assignee_s);
        free(board_s);
    }
    if (st->handoff && st->handoff[0]) {
        kw_targ h[1];
        char *piece, *joined;
        h[0].name = "summary";
        h[0].val = st->handoff;
        piece = kw_t("gateway.kanban.wake.handoff", h, 1);
        joined = kw_concat(synth, "\n");
        free(synth);
        synth = kw_concat(joined, piece ? piece : "");
        free(joined);
        free(piece);
    }
    if (st->rd && st->rd[0]) {
        kw_targ r[1];
        char *piece, *joined;
        r[0].name = "reason";
        r[0].val = st->rd;
        piece = kw_t("gateway.kanban.wake.review_detail", r, 1);
        joined = kw_concat(synth, "\n");
        free(synth);
        synth = kw_concat(joined, piece ? piece : "");
        free(joined);
        free(piece);
    }
    {
        char *guidance = kw_t("gateway.kanban.wake.guidance", NULL, 0);
        char *tail = kw_concat("\n\n", guidance ? guidance : "");
        char *final = kw_concat(synth, tail);
        free(synth);
        synth = final;
        free(tail);
        free(guidance);
    }
    free(st->synth);
    st->synth = synth;
    free(status_joined);
}

json_t *w30_kanbanwatc_3847d4_p2_wake_plan(const json_t *req) {
    const json_t *events = req ? json_obj_get(req, "events") : NULL;
    const json_t *sub = req ? json_obj_get(req, "sub") : NULL;
    const json_t *task = req ? json_obj_get(req, "task") : NULL;
    const json_t *board = req ? json_obj_get(req, "board") : NULL;
    int split = req ? (json_get_bool(req, "split", false) ? 1 : 0) : 0;
    int is_push = req ? (json_get_bool(req, "is_push_adapter", true) ? 1 : 0) : 1;
    int multiplex = req ? (json_get_bool(req, "multiplex", false) ? 1 : 0) : 0;
    const char *mode = sub ? json_get_str(sub, "delivery_mode", "") : "";
    int wake_agent;
    kw_ctx ctx;
    kw_wake_state st;
    json_t *out = json_object();
    json_t *groups = json_array();
    size_t i, n;

    if (task && task->type != JSON_OBJECT) task = NULL;
    if (!mode || !mode[0]) mode = "notify";
    wake_agent = !strcmp(mode, "notify+wake") || !strcmp(mode, "wake");

    memset(&st, 0, sizeof st);
    st.handoff = kw_dup("");
    st.rd = kw_dup("");
    st.session_key = kw_dup("");
    st.synth = kw_dup("");
    kw_ctx_init(&ctx, sub, task, board);

    n = (events && events->type == JSON_ARRAY) ? json_len(events) : 0;
    {
        int pass, passes = split ? 2 : 1;
        for (pass = 0; pass < passes; pass++) {
            json_t *group = json_array();
            json_t *row;
            int has_events = 0;
            for (i = 0; i < n; i++) {
                const json_t *ev = json_get(events, i);
                int diag = kw_is_diagnostic(ev);
                if (split && ((pass == 0 && !diag) || (pass == 1 && diag))) continue;
                json_append(group, ev ? json_copy(ev) : json_null());
                has_events = 1;
            }
            if (!has_events) { json_free(group); continue; }
            free(st.handoff); st.handoff = kw_dup("");
            free(st.rd); st.rd = kw_dup("");
            for (i = 0; i < json_len(group); i++) {
                char *msg = NULL, *h = NULL, *r = NULL;
                kw_format_one(&ctx, task, json_get(group, i), &msg, &h, &r);
                if (h) { free(st.handoff); st.handoff = h; }
                if (r) { free(st.rd); st.rd = r; }
                free(msg);
            }
            kw_build_wake_text(&ctx, sub, task, board, group, wake_agent, is_push, &st);
            json_free(group);
            if (st.n_kinds > 0) {
                json_t *kinds = json_array();
                size_t k;
                for (k = 0; k < KW_N_WAKE; k++)
                    if (st.kind_flags[k]) json_append(kinds, json_string(KW_WAKE_KINDS[k]));
                row = json_object();
                json_set(row, "synth", json_string(st.synth ? st.synth : ""));
                json_set(row, "diagnostic", json_bool(st.diagnostic != 0));
                json_set(row, "kinds", kinds);
                json_append(groups, row);
            }
        }
    }

    {
        json_t *kinds = json_array();
        size_t k;
        for (k = 0; k < KW_N_WAKE; k++)
            if (st.kind_flags[k]) json_append(kinds, json_string(KW_WAKE_KINDS[k]));
        json_set(out, "groups", groups);
        json_set(out, "synth", json_string(st.synth ? st.synth : ""));
        json_set(out, "session_key", json_string(st.session_key ? st.session_key : ""));
        json_set(out, "wake_kinds", kinds);
        json_set(out, "wake_diagnostic", st.has_diagnostic ? json_bool(st.diagnostic != 0) : json_null());
        {
            /* _served_wake_profile: the sub's profile when a multiplexer serves it. */
            const json_t *prof = sub ? json_obj_get(sub, "notifier_profile") : NULL;
            if (multiplex && kw_truthy(prof))
                json_set(out, "served_profile", kw_jstr(kw_pystr(prof)));
            else
                json_set(out, "served_profile", json_null());
        }
    }
    free(st.handoff);
    free(st.rd);
    free(st.session_key);
    free(st.synth);
    kw_ctx_free(&ctx);
    return out;
}

json_t *w30_kanbanwatc_3847d4_p2_failure_step(const json_t *req) {
    long long count = 0, fails;
    json_t *out = json_object();
    if (req && json_obj_get(req, "count")) kw_pyint(json_obj_get(req, "count"), &count);
    fails = count + 1;
    json_set(out, "fails", json_number((double)fails));
    json_set(out, "action", json_string(fails >= KW_MAX_SEND_FAILURES ? "unsub" : "rewind"));
    json_set(out, "count_after", fails >= KW_MAX_SEND_FAILURES ? json_null() : json_number((double)fails));
    return out;
}

json_t *w30_kanbanwatc_3847d4_p2_after_delivery(const json_t *req) {
    const json_t *task = req ? json_obj_get(req, "task") : NULL;
    int is_push = req ? (json_get_bool(req, "is_push", true) ? 1 : 0) : 1;
    const char *status = (task && task->type == JSON_OBJECT) ? json_get_str(task, "status", "") : "";
    int has_task = task && task->type == JSON_OBJECT;
    json_t *out = json_object();
    json_set(out, "advance", json_bool(true));
    json_set(out, "clear_failures", json_bool(is_push ? false : true));
    json_set(out, "unsub", json_bool(has_task && status && !strcmp(status, "archived")));
    return out;
}
