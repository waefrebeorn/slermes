/*
 * run_turn_runner_approval_text.c -- real C11 port of the plain-text approval
 * prompt for adapters whose send cuts long text:
 *   gateway/run_turn_runner_approval_text.py.
 *
 * PoP: w20_runturnrun_9ce529_cutting_text_prompt @ gateway/run_turn_runner_approval_text.py:_cutting_text_prompt
 * PoP: w20_runturnrun_9ce529_code_fence @ gateway/platforms/base_exec_approval.py:code_fence
 *
 * The localized strings arrive as data (the caller's i18n bundle) and the
 * adapter is reduced to its two measurement hooks (the chat's limit and its
 * length function), so the layout, the choice ladder, the deny-only fallback
 * and the shortening arithmetic are the ported code.
 */
#include "hermes_json.h"
#include "noc_w20_runturnrun_9ce529.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The default heading mark: U+26A0 U+FE0F, two spaces, "**", then the header. */
static const char AT529_WARN_PREFIX[] = "\xe2\x9a\xa0\xef\xb8\x8f **";
static const char AT529_WARN_SUFFIX[] = "**";

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} at529_sb;

static void at529_sb_init(at529_sb *sb) {
    sb->cap = 1024;
    sb->buf = (char *)malloc(sb->cap);
    if (sb->buf) sb->buf[0] = '\0';
    sb->len = 0;
}

static void at529_sb_add(at529_sb *sb, const char *text) {
    size_t need;
    if (!sb->buf || !text) return;
    need = sb->len + strlen(text) + 1;
    if (need > sb->cap) {
        size_t cap = sb->cap * 2;
        char *grown;
        while (cap < need) cap *= 2;
        grown = (char *)realloc(sb->buf, cap);
        if (!grown) return;
        sb->buf = grown;
        sb->cap = cap;
    }
    memcpy(sb->buf + sb->len, text, strlen(text) + 1);
    sb->len += strlen(text);
}

static void at529_sb_free(at529_sb *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = sb->cap = 0;
}

static const char *at529_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const char *at529_str_or(const json_t *obj, const char *key, const char *def) {
    const char *s = at529_str(obj, key);
    return s ? s : def;
}

static char *at529_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s ? s : "", n + 1);
    return out;
}

/* str.format-style substitution for the one placeholder the live code passes. */
static char *at529_subst(const char *tpl, const char *key, const char *value) {
    char token[64];
    const char *p;
    at529_sb sb;
    at529_sb_init(&sb);
    if (!sb.buf) return NULL;
    snprintf(token, sizeof(token), "{%s}", key);
    p = tpl ? tpl : "";
    while (*p) {
        const char *hit = strstr(p, token);
        if (!hit) {
            at529_sb_add(&sb, p);
            break;
        }
        {
            size_t n = (size_t)(hit - p);
            char *chunk = (char *)malloc(n + 1);
            if (!chunk) break;
            memcpy(chunk, p, n);
            chunk[n] = '\0';
            at529_sb_add(&sb, chunk);
            free(chunk);
        }
        at529_sb_add(&sb, value ? value : "");
        p = hit + strlen(token);
    }
    return sb.buf;
}

/* len(re.findall(r"`+", command)) maximum, then max(3, that + 1). */
static size_t at529_fence_len(const char *s) {
    size_t best = 0, run = 0;
    const char *p = s ? s : "";
    for (; *p; p++) {
        if (*p == '`') {
            run++;
            if (run > best) best = run;
        } else {
            run = 0;
        }
    }
    return best + 1 > 3 ? best + 1 : 3;
}

static char *at529_fence(const char *command) {
    size_t n = at529_fence_len(command);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memset(out, '`', n);
    out[n] = '\0';
    return out;
}

json_t *w20_runturnrun_9ce529_code_fence(const char *command) {
    char *fence = at529_fence(command);
    json_t *out = json_string(fence ? fence : "");
    free(fence);
    return out;
}

/* Python len(): code points, not bytes (the heading carries an emoji). */
static size_t at529_cp_len(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t n = 0;
    for (; *p; p++) {
        if ((*p & 0xC0) != 0x80) n++;
    }
    return n;
}

/* The first `count` code points of s. */
static char *at529_cp_prefix(const char *s, size_t count) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t n = 0;
    const unsigned char *start = p;
    char *out;
    while (*p && n < count) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        n++;
    }
    out = (char *)malloc((size_t)(p - start) + 1);
    if (!out) return NULL;
    memcpy(out, start, (size_t)(p - start));
    out[(size_t)(p - start)] = '\0';
    return out;
}

/* fit_prefix(text, budget, measure, suffix="..."): the longest prefix within
 * budget units plus the suffix (outside it).  measure is the chat's length
 * function; every adapter this gate compares measures in len() units. */
static char *at529_fit_prefix(const char *text, long budget, const char *suffix) {
    size_t n = at529_cp_len(text);
    at529_sb sb;
    if ((long)n <= budget) return at529_dup(text);
    at529_sb_init(&sb);
    if (!sb.buf) return NULL;
    {
        char *head = at529_cp_prefix(text, budget > 0 ? (size_t)budget : 0);
        if (!head) { at529_sb_free(&sb); return NULL; }
        at529_sb_add(&sb, head);
        free(head);
    }
    at529_sb_add(&sb, suffix ? suffix : "");
    return sb.buf;
}

/* The prompt body, assembled in one place so every branch matches the source. */
static char *at529_prompt(const char *heading, json_t *choices, const char *notice,
                          const char *shown, const char *fence, const json_t *i18n,
                          const char *description) {
    at529_sb sb;
    size_t n, i;
    const char *joiner = at529_str_or(i18n, "joiner", "");
    const char *last = at529_str_or(i18n, "last", "");
    const char *deadline = at529_str_or(i18n, "deadline", "");
    const char *reason_label = at529_str_or(i18n, "reason_label", "");

    at529_sb_init(&sb);
    if (!sb.buf) return NULL;
    at529_sb_add(&sb, heading ? heading : "");
    at529_sb_add(&sb, "\n\n");
    n = choices ? choices->c.count : 0;
    if (n > 1) {
        for (i = 0; i + 1 < n; i++) {
            if (i) at529_sb_add(&sb, joiner);
            at529_sb_add(&sb, choices->c.items[i]->str_val);
        }
        {
            char *tail = at529_subst(last, "choice", choices->c.items[n - 1]->str_val);
            at529_sb_add(&sb, tail ? tail : "");
            free(tail);
        }
    } else if (n == 1) {
        at529_sb_add(&sb, choices->c.items[0]->str_val);
        at529_sb_add(&sb, ".\n");
    }
    at529_sb_add(&sb, notice ? notice : "");
    at529_sb_add(&sb, deadline);
    at529_sb_add(&sb, "\n\n");
    at529_sb_add(&sb, reason_label);
    at529_sb_add(&sb, ": ");
    at529_sb_add(&sb, description ? description : "");
    at529_sb_add(&sb, "\n");
    at529_sb_add(&sb, fence ? fence : "");
    at529_sb_add(&sb, "\n");
    at529_sb_add(&sb, shown ? shown : "");
    at529_sb_add(&sb, "\n");
    at529_sb_add(&sb, fence ? fence : "");
    return sb.buf;
}

/* max(0, min(len(command) - 1, limit - measure(prompt(deny, notice, "", fence)) - measure("..."))) */
static long at529_budget(const char *heading, json_t *choices, const char *notice,
                         const char *fence, const json_t *i18n, const char *description,
                         long limit, size_t command_len) {
    char *zero = at529_prompt(heading, choices, notice, "", fence, i18n, description);
    long fixed = (long)at529_cp_len(zero ? zero : "");
    long budget = (long)command_len - 1;
    free(zero);
    if (limit - fixed - 3 < budget) budget = limit - fixed - 3;
    if (budget < 0) budget = 0;
    return budget;
}

json_t *w20_runturnrun_9ce529_cutting_text_prompt(const json_t *req) {
    const json_t *i18n = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "i18n") : NULL;
    const char *command = at529_str_or(req, "command", "");
    const char *description = at529_str_or(req, "description", "");
    const char *prefix = at529_str_or(req, "prefix", "");
    long limit = (long)json_get_num(req, "limit", 4096);
    int allow_permanent = json_get_bool(req, "allow_permanent", 1);
    int allow_session = json_get_bool(req, "allow_session", 1);
    int smart_denied = json_get_bool(req, "smart_denied", 0);
    json_t *choices = json_array();
    char *fence = at529_fence(command);
    char *heading = NULL;
    char *full = NULL;
    json_t *out = json_object();

    if (smart_denied) {
        heading = at529_subst(at529_str_or(i18n, "smart_heading", ""), "", "");
    } else {
        at529_sb sb;
        at529_sb_init(&sb);
        at529_sb_add(&sb, AT529_WARN_PREFIX);
        at529_sb_add(&sb, at529_str_or(i18n, "header", ""));
        at529_sb_add(&sb, AT529_WARN_SUFFIX);
        heading = sb.buf;
    }
    {
        char *once = at529_subst(at529_str_or(i18n, "once", ""), "prefix", prefix);
        char *deny = at529_subst(at529_str_or(i18n, "deny", ""), "prefix", prefix);
        json_append(choices, json_string(once ? once : ""));
        if (!smart_denied && allow_session) {
            char *session = at529_subst(at529_str_or(i18n, "session", ""), "prefix", prefix);
            json_append(choices, json_string(session ? session : ""));
            free(session);
            if (allow_permanent) {
                char *always = at529_subst(at529_str_or(i18n, "always", ""), "prefix", prefix);
                json_append(choices, json_string(always ? always : ""));
                free(always);
            }
        }
        json_append(choices, json_string(deny ? deny : ""));
        free(once);
        free(deny);
    }
    full = at529_prompt(heading, choices, "", command, fence ? fence : "", i18n, description);
    if ((long)at529_cp_len(full) <= limit) {
        json_set(out, "text", json_string(full));
        json_set(out, "deny_only", json_bool(0));
        free(full);
        free(heading);
        free(fence);
        json_free(choices);
        return out;
    }
    {
        json_t *deny_only_choices = json_array();
        char *deny = at529_subst(at529_str_or(i18n, "deny", ""), "prefix", prefix);
        char *notice = at529_subst(at529_str_or(i18n, "deny_only", ""), "", "");
        char *whole;
        at529_sb notice_sb;

        json_append(deny_only_choices, json_string(deny ? deny : ""));
        at529_sb_init(&notice_sb);
        at529_sb_add(&notice_sb, notice ? notice : "");
        at529_sb_add(&notice_sb, "\n");
        whole = at529_prompt(heading, deny_only_choices, notice_sb.buf, command,
                             fence ? fence : "", i18n, description);
        free(notice);
        if ((long)at529_cp_len(whole) <= limit) {
            json_set(out, "text", json_string(whole));
            json_set(out, "deny_only", json_bool(1));
            free(whole);
            free(notice_sb.buf);
            free(deny);
            json_free(deny_only_choices);
            free(full);
            free(heading);
            free(fence);
            json_free(choices);
            return out;
        }
        free(whole);
        {
            /* The fence is sized from the shortened command, and the budget
             * stays under the whole command so the notice never sits beside
             * the full text. */
            char total_text[32];
            char *truncated;
            at529_sb notice2_sb;
            size_t command_len = at529_cp_len(command);
            size_t lo = 3, hi = at529_fence_len(command);
            char *shown = NULL, *final = NULL;

            snprintf(total_text, sizeof(total_text), "%lu", (unsigned long)command_len);
            truncated = at529_subst(at529_str_or(i18n, "truncated", ""), "total", total_text);
            at529_sb_init(&notice2_sb);
            at529_sb_add(&notice2_sb, truncated ? truncated : "");
            at529_sb_add(&notice2_sb, "\n");
            free(truncated);

            /* fit_fenced's bisection: the smallest fence its own shown prefix
             * cannot close. */
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                char *probe = (char *)malloc(mid + 1);
                char *probe_shown;
                if (!probe) break;
                memset(probe, '`', mid);
                probe[mid] = '\0';
                probe_shown = at529_fit_prefix(
                    command,
                    at529_budget(heading, deny_only_choices, notice2_sb.buf, probe,
                                 i18n, description, limit, command_len),
                    "...");
                if (at529_fence_len(probe_shown ? probe_shown : "") <= mid) hi = mid;
                else lo = mid + 1;
                free(probe_shown);
                free(probe);
            }
            {
                char *probe = (char *)malloc(lo + 1);
                char *shown_fence;
                if (probe) {
                    memset(probe, '`', lo);
                    probe[lo] = '\0';
                }
                shown = at529_fit_prefix(
                    command,
                    at529_budget(heading, deny_only_choices, notice2_sb.buf,
                                 probe ? probe : "", i18n, description, limit, command_len),
                    "...");
                free(probe);
                shown_fence = at529_fence(shown ? shown : "");
                final = at529_prompt(heading, deny_only_choices, notice2_sb.buf, shown,
                                     shown_fence ? shown_fence : "", i18n, description);
                free(shown_fence);
            }
            json_set(out, "text", json_string(final ? final : ""));
            json_set(out, "deny_only", json_bool(1));
            free(final);
            free(shown);
            at529_sb_free(&notice2_sb);
            free(deny);
            json_free(deny_only_choices);
        }
        free(full);
        free(heading);
        free(fence);
        json_free(choices);
        return out;
    }
}