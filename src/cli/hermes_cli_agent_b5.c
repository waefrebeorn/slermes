/*
 * hermes_cli_agent_b5.c — HermesCLI batch 5 methods (cli.py class HermesCLI).
 * Dangerous-command approval callback, SIGWINCH resize recovery, status-bar
 * snapshot, spinner widget height, and the line-buffered stream-delta
 * callback with tag-boundary reasoning suppression.
 */
#define _GNU_SOURCE   /* memrchr */
#include "cli_hermes_cli.h"
#include "hermes_json.h"
#include "cli_helpers2.h"

#include <pthread.h>
#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* agent_state_t carries the live session token counters (opaque through
 * cli_hermes_cli.h); pull the real definition in for the snapshot. */
#include "hermes_core_types.h"

/* Sibling-batch symbols not yet in a public header — bind weakly so this
 * object links standalone (same pattern as hermes_cli_agent_b4.c). */
extern void hcli_paint_now(hcli_t *self) __attribute__((weak));
extern char *upr_format_duration_compact(double seconds) __attribute__((weak));

/* tools/approvals.timeout config read; bound by the CLI shell. Falls back
 * to the Python default of 300 when unlinked. */
extern int cli_config_approvals_timeout(void) __attribute__((weak));

/* ── shared helpers ─────────────────────────────────────────────────────── */

static double b5_now_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static double b5_now_wall(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* hermes_cli/model_switch.py:format_model_for_display — strip the Palantir
 * Foundry opaque proxy prefix, else pass through. Static: display-only
 * helper shared by this file's snapshot builder. */
static char *b5_format_model_for_display(const char *model)
{
    static const char *const PREFIX = "ri.language-model-service..language-model.";
    if (!model)
        return NULL;
    size_t pl = strlen(PREFIX);
    if (strncmp(model, PREFIX, pl) == 0 && model[pl])
        return strdup(model + pl);
    return strdup(model);
}

/* ── response queue (queue.Queue stand-in) ──────────────────────────────── */

typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t  cv;
    char           *value;   /* malloc'd verdict once set */
    bool            has;
} b5_queue_t;

static void b5_queue_free(b5_queue_t *q)
{
    if (!q) return;
    free(q->value);
    pthread_mutex_destroy(&q->mu);
    pthread_cond_destroy(&q->cv);
    free(q);
}

/* queue.put() seam consumed by hcli_handle_approval_selection (batch 4). */
static void b5_queue_put(void *queue, const char *value)
{
    b5_queue_t *q = queue;
    if (!q || !value)
        return;
    pthread_mutex_lock(&q->mu);
    free(q->value);
    q->value = strdup(value);
    q->has = true;
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->mu);
}

/* ── hcli_approval_choices ──────────────────────────────────────────────── */

/* PoP: hcli_approval_choices @ cli.py:_approval_choices */
char **hcli_approval_choices(const char *command, bool allow_permanent,
                             bool allow_session, bool smart_denied,
                             size_t *out_count)
{
    const char *base[4];
    size_t n = 0;

    /* if smart_denied or not allow_session: [once, deny] */
    if (smart_denied || !allow_session) {
        base[n++] = "once";
        base[n++] = "deny";
    } else if (allow_permanent) {
        base[n++] = "once";
        base[n++] = "session";
        base[n++] = "always";
        base[n++] = "deny";
    } else {
        base[n++] = "once";
        base[n++] = "session";
        base[n++] = "deny";
    }

    bool add_view = command && strlen(command) > 70;
    size_t total = n + (add_view ? 1 : 0);

    char **choices = malloc((total + 1) * sizeof(char *));
    if (!choices) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    for (size_t i = 0; i < n; i++)
        choices[i] = strdup(base[i]);
    if (add_view)
        choices[n] = strdup("view");
    choices[total] = NULL;
    if (out_count) *out_count = total;
    return choices;
}

/* ── hcli_approval_callback ─────────────────────────────────────────────── */

static void b5_persist(hcli_t *self, const char *command, const char *outcome)
{
    /* Python: self._persist_prompt_summary("⚠", "Approval", command, outcome).
     * Routed through the shell-bound seam; no-op on unbound test stand-ins,
     * matching the Python path when the TUI layer is absent. */
    if (self && self->persist_prompt_summary_fn)
        self->persist_prompt_summary_fn(self, "\xe2\x9a\xa0", "Approval",
                                        command, outcome);
}

static void b5_approval_state_clear(hcli_t *self)
{
    hcli_approval_state_t *st = self->approval_state;
    self->approval_state = NULL;
    self->approval_deadline = 0;
    if (!st) return;
    for (size_t i = 0; i < st->choice_count; i++) free(st->choices[i]);
    free(st->choices);
    free(st->command);
    free(st->description);
    /* st->response_queue (b5_queue_t) is freed by the caller after the wait. */
    free(st);
}

/* PoP: hcli_approval_callback @ cli.py:_approval_callback */
char *hcli_approval_callback(hcli_t *self, const char *command,
                             const char *description, bool allow_permanent,
                             bool allow_session, bool smart_denied)
{
    if (!self)
        return strdup("timeout");

    /* with self._approval_lock: serialize concurrent approval prompts. */
    static pthread_mutex_t s_lock_init_mu = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&s_lock_init_mu);
    if (!self->approval_lock) {
        pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
        if (m) pthread_mutex_init(m, NULL);
        self->approval_lock = m;
    }
    pthread_mutex_unlock(&s_lock_init_mu);
    pthread_mutex_t *lock = self->approval_lock;
    if (lock) pthread_mutex_lock(lock);

    char *result = NULL;
    b5_queue_t *q = malloc(sizeof(b5_queue_t));
    if (q) {
        pthread_mutex_init(&q->mu, NULL);
        pthread_cond_init(&q->cv, NULL);
        q->value = NULL;
        q->has = false;
    }

    int timeout = 300;   /* Python default: CLI_CONFIG approvals.timeout 300 */
    if (cli_config_approvals_timeout)
        timeout = cli_config_approvals_timeout();
    if (timeout < 0) timeout = 0;

    size_t choice_count = 0;
    char **choices = hcli_approval_choices(command, allow_permanent,
                                           allow_session, smart_denied,
                                           &choice_count);
    hcli_approval_state_t *st = calloc(1, sizeof(hcli_approval_state_t));
    if (st && choices) {
        st->choices = choices;
        st->choice_count = choice_count;
        st->selected = 0;
        st->show_full = false;
        st->command = strdup(command ? command : "");
        st->description = strdup(description ? description : "");
        st->response_queue = q;
        st->queue_put = q ? b5_queue_put : NULL;
        self->approval_state = st;
        self->approval_deadline = b5_now_monotonic() + (double)timeout;
    } else {
        for (size_t i = 0; choices && choices[i]; i++) free(choices[i]);
        free(choices);
        free(st);
    }

    /* Modal prompt — paint immediately, bypassing the throttle/resize guard
     * (#41098): a throttled paint can be dropped, denying a command the user
     * never saw. The countdown refresh paints the same way. */
    if (hcli_paint_now) hcli_paint_now(self);

    double deadline = self->approval_deadline;
    double last_refresh = b5_now_monotonic();

    if (st && q) {
        while (!result) {
            struct timespec until;
            clock_gettime(CLOCK_REALTIME, &until);
            until.tv_sec += 1;      /* response_queue.get(timeout=1) */

            pthread_mutex_lock(&q->mu);
            while (!q->has) {
                int rc = pthread_cond_timedwait(&q->cv, &q->mu, &until);
                double remaining = deadline - b5_now_monotonic();
                if (rc != 0 && remaining <= 0)
                    break;
                if (q->has)
                    break;
                if (rc == 0)
                    continue;
                /* timed out this second: countdown repaint at >=1s cadence */
                double now = b5_now_monotonic();
                if (now - last_refresh >= 1.0) {
                    last_refresh = now;
                    pthread_mutex_unlock(&q->mu);
                    if (hcli_paint_now) hcli_paint_now(self);
                    pthread_mutex_lock(&q->mu);
                }
            }
            if (q->has) {
                result = strdup(q->value);
                pthread_mutex_unlock(&q->mu);
                break;
            }
            pthread_mutex_unlock(&q->mu);

            if (deadline - b5_now_monotonic() <= 0)
                break;   /* queue.Empty with expired deadline → deny-on-timeout */
        }
    }

    b5_queue_free(q);
    b5_approval_state_clear(self);
    if (hcli_paint_now) hcli_paint_now(self);
    if (lock) pthread_mutex_unlock(lock);

    if (result) {
        /* _outcome_labels mapping for the persisted prompt summary. */
        const char *label;
        if      (!strcmp(result, "once"))    label = "allowed once";
        else if (!strcmp(result, "session")) label = "allowed for session";
        else if (!strcmp(result, "always"))  label = "added to allowlist";
        else if (!strcmp(result, "deny"))    label = "denied";
        else                                 label = result;
        b5_persist(self, command, label);
        return result;
    }

    /* Timeout — denying command. */
    fprintf(stderr, "\n\e[2m  ⏱ Timeout — denying command\e[0m\n");
    fflush(stderr);
    b5_persist(self, command, "timed out (no response)");
    return strdup("timeout");
}

/* ── hcli_recover_after_resize ──────────────────────────────────────────── */

typedef struct {
    hcli_t *self;
    int     gen;
} b5_unsup_t;

/* The debounced unsuppress body (_clear closure): clear suppression and
 * invalidate once the reflow has settled. */
static void *b5_unsup_thread(void *arg)
{
    b5_unsup_t *u = arg;
    usleep(350 * 1000);          /* delay = 0.35s */
    if (u->self->status_bar_unsuppress_gen == u->gen) {
        u->self->status_bar_suppressed_after_resize = false;
        if (u->self->invalidate_simple_fn)
            u->self->invalidate_simple_fn(u->self);
    }
    free(u);
    return NULL;
}

/* _schedule_status_bar_unsuppress: debounce via generation counter — a fresh
 * resize bumps the gen so any pending timer's work becomes a no-op. */
static void b5_schedule_status_bar_unsuppress(hcli_t *self)
{
    self->status_bar_unsuppress_gen++;
    b5_unsup_t *u = malloc(sizeof(b5_unsup_t));
    if (!u) return;
    u->self = self;
    u->gen = self->status_bar_unsuppress_gen;
    pthread_t th;
    if (pthread_create(&th, NULL, b5_unsup_thread, u) == 0)
        pthread_detach(th);
    else
        free(u);
}

/* PoP: hcli_recover_after_resize @ cli.py:_recover_after_resize */
void hcli_recover_after_resize(hcli_t *self, void *app,
                               void (*original_on_resize)(hcli_t *, void *))
{
    if (!self)
        return;
    self->status_bar_suppressed_after_resize = true;

    int new_width = -1;
    new_width = cli_get_tui_terminal_width();   /* probe failure → <=0 ≡ None */
    int prev_width = self->last_resize_width;

    /* Replay only on an OBSERVED width change (#65293): benign same-width
     * SIGWINCHes must not trigger a 2J + replay that duplicates scrollback. */
    bool width_changed = (new_width > 0 && prev_width > 0 && new_width != prev_width);
    if (width_changed) {
        if (self->clear_prompt_toolkit_screen_fn)
            self->clear_prompt_toolkit_screen_fn(self, app,
                    cli_redraw_rebuilds_scrollback());
        if (self->replay_output_history_fn)
            self->replay_output_history_fn();
    }
    if (new_width > 0)
        self->last_resize_width = new_width;

    if (original_on_resize)
        original_on_resize(self, app);

    /* Transient suppression: a short follow-up timer clears it and repaints
     * once the reflow has settled, so idle restores the status bar. */
    b5_schedule_status_bar_unsuppress(self);
}

/* ── hcli_get_status_bar_snapshot ───────────────────────────────────────── */

/* PoP: hcli_get_status_bar_snapshot @ cli.py:_get_status_bar_snapshot */
json_t *hcli_get_status_bar_snapshot(hcli_t *self)
{
    json_t *snapshot = json_object();
    if (!snapshot)
        return NULL;

    /* Prefer the agent's model name — it updates on fallback. In the C tree
     * agent_state_t carries no model string, so self.model is authoritative
     * (the originally configured model). */
    const char *model_name = (self && self->model && *self->model)
                                 ? self->model : "unknown";

    /* Friendly display: reverse alias first, then RID-prefix strip, then the
     * trailing path component, then .gguf suffix strip, then cap to 26. */
    char *model_short = cli_reverse_alias_for_display(model_name);
    if (!model_short || strcmp(model_short, model_name) == 0) {
        free(model_short);
        const char *slash = strrchr(model_name, '/');
        const char *tail = slash ? slash + 1 : model_name;
        model_short = b5_format_model_for_display(tail);
        if (!model_short)
            model_short = strdup(tail);
    }
    size_t ml = strlen(model_short);
    if (ml > 5 && strcmp(model_short + ml - 5, ".gguf") == 0)
        model_short[ml - 5] = '\0';
    if (strlen(model_short) > 26) {
        model_short[23] = '.';
        model_short[24] = '.';
        model_short[25] = '.';
        model_short[26] = '\0';
    }

    double elapsed_seconds = 0.0;
    bool has_start = self && self->has_session_start;
    if (has_start)
        elapsed_seconds = b5_now_wall() - self->session_start_epoch;
    if (elapsed_seconds < 0.0)
        elapsed_seconds = 0.0;
    char *duration = upr_format_duration_compact
                         ? upr_format_duration_compact(elapsed_seconds)
                         : strdup("0s");

    char *session_title = self ? hcli_get_status_bar_session_title(self)
                               : strdup("");

    bool turn_live = self && self->has_prompt_start_time;
    char *prompt_elapsed = cli_format_prompt_elapsed(
        turn_live,
        self ? self->prompt_start_time : 0.0,
        self ? self->prompt_duration : 0.0,
        turn_live);
    char *idle_since = cli_format_idle_since(
        self && self->has_last_turn_finished_at,
        self ? self->last_turn_finished_at : 0.0,
        turn_live);

    json_set(snapshot, "model_name", json_string(model_name));
    json_set(snapshot, "model_short", json_string(model_short));
    json_set(snapshot, "duration", json_string(duration ? duration : ""));
    json_set(snapshot, "session_title", json_string(session_title ? session_title : ""));
    json_set(snapshot, "prompt_elapsed", json_string(prompt_elapsed ? prompt_elapsed : ""));
    json_set(snapshot, "idle_since", json_string(idle_since ? idle_since : ""));
    free(model_short);
    free(duration);
    free(session_title);
    free(prompt_elapsed);
    free(idle_since);

    json_set(snapshot, "context_tokens", json_number(0));
    json_set(snapshot, "context_length", json_number(0));
    json_set(snapshot, "context_percent", json_number(0));
    json_set(snapshot, "session_input_tokens", json_number(0));
    json_set(snapshot, "session_output_tokens", json_number(0));
    json_set(snapshot, "session_cache_read_tokens", json_number(0));
    json_set(snapshot, "session_cache_write_tokens", json_number(0));
    json_set(snapshot, "session_prompt_tokens", json_number(0));
    json_set(snapshot, "session_completion_tokens", json_number(0));
    json_set(snapshot, "session_total_tokens", json_number(0));
    json_set(snapshot, "session_api_calls", json_number(0));
    json_set(snapshot, "compressions", json_number(0));
    json_set(snapshot, "active_background_tasks", json_number(0));
    json_set(snapshot, "active_background_processes", json_number(0));
    json_set(snapshot, "active_background_subagents", json_number(0));
    json_set(snapshot, "battery_label", json_string(""));
    json_set(snapshot, "battery_category", json_string("dim"));
    json_set(snapshot, "focus_label", json_string(""));
    json_set(snapshot, "goal_active", json_bool(false));
    json_set(snapshot, "goal_turns_used", json_number(0));
    json_set(snapshot, "goal_max_turns", json_number(0));

    if (!self)
        return snapshot;

    const agent_state_t *agent = self->agent;
    if (!agent)
        return snapshot;

#define B5_SET_INT(key, val) \
    json_set(snapshot, key, json_number((double)(val)))
    B5_SET_INT("session_input_tokens", agent->session_input_tokens);
    B5_SET_INT("session_output_tokens", agent->session_output_tokens);
    B5_SET_INT("session_cache_read_tokens", agent->session_cache_read_tokens);
    B5_SET_INT("session_cache_write_tokens", agent->session_cache_write_tokens);
    /* session_prompt/completion/api_calls have no agent_state_t counterpart
     * yet — stay at 0 exactly like the getattr defaults. */
#undef B5_SET_INT

    return snapshot;
}

/* ── hcli_spinner_widget_height ─────────────────────────────────────────── */

extern bool hcli_use_minimal_tui_chrome(hcli_t *self) __attribute__((weak));

/* PoP: hcli_spinner_widget_height @ cli.py:_spinner_widget_height */
int hcli_spinner_widget_height(hcli_t *self, int width)
{
    if (!self)
        return 0;

    const char *spinner_line = self->spinner_text;
    if (!spinner_line || !*spinner_line)
        return 0;
    if (hcli_use_minimal_tui_chrome && hcli_use_minimal_tui_chrome(self))
        return 0;
    if (width <= 0)
        width = cli_get_tui_terminal_width();
    if (width > 10) {
        int text_width = cli_status_bar_display_width(spinner_line);
        /* max(1, ceil(text_width / width)) */
        int rows = (text_width + width - 1) / width;
        return rows < 1 ? 1 : rows;
    }
    return 1;
}

/* ── hcli_stream_delta ──────────────────────────────────────────────────── */

static const char *const B5_OPEN_TAGS[] = {
    "<REASONING_SCRATCHPAD>", "<think>", "<reasoning>",
    "<THINKING>", "<thinking>", "<thought>"
};
static const char *const B5_CLOSE_TAGS[] = {
    "</REASONING_SCRATCHPAD>", "</think>", "</reasoning>",
    "</THINKING>", "</thinking>", "</thought>"
};

static void b5_reset_stream_state(hcli_t *self)
{
    self->stream_started = false;
    self->stream_box_opened = false;
    self->in_reasoning_block = false;
    self->stream_last_was_newline = true;
    free(self->stream_prefilt);
    self->stream_prefilt = NULL;
    free(self->deferred_content);
    self->deferred_content = NULL;
}

/* PoP: hcli_stream_delta @ cli.py:_stream_delta */
void hcli_stream_delta(hcli_t *self, const char *text)
{
    if (!self)
        return;

    if (!text) {                 /* intermediate turn boundary */
        if (self->stream_flush_fn)
            self->stream_flush_fn(self);
        b5_reset_stream_state(self);
        return;
    }
    if (!*text)
        return;

    self->stream_started = true;
    if (!self->stream_prefilt)
        self->stream_last_was_newline = true;   /* start of stream = boundary */

    /* Append to the pre-filter buffer. */
    size_t old_len = self->stream_prefilt ? strlen(self->stream_prefilt) : 0;
    size_t add_len = strlen(text);
    char *prefilt = realloc(self->stream_prefilt, old_len + add_len + 1);
    if (!prefilt)
        return;
    memcpy(prefilt + old_len, text, add_len + 1);
    self->stream_prefilt = prefilt;
    size_t pf_len = old_len + add_len;

    /* Case-insensitive view for tag scans. */
    char *pf_lower = malloc(pf_len + 1);
    if (!pf_lower)
        return;
    for (size_t i = 0; i < pf_len; i++)
        pf_lower[i] = (char)tolower((unsigned char)prefilt[i]);
    pf_lower[pf_len] = '\0';

    if (!self->in_reasoning_block) {
        bool matched = false;
        for (size_t t = 0; t < sizeof(B5_OPEN_TAGS) / sizeof(*B5_OPEN_TAGS) && !matched; t++) {
            const char *tag = B5_OPEN_TAGS[t];
            size_t tag_len = strlen(tag);
            size_t search_start = 0;
            while (!matched) {
                /* find tag case-insensitively from search_start */
                char *hit = NULL;
                if (search_start <= pf_len)
                    hit = memmem(pf_lower + search_start, pf_len - search_start,
                                 tag, tag_len);
                if (!hit)
                    break;
                size_t idx = (size_t)(hit - pf_lower);

                /* Block-boundary check: only whitespace between the last
                 * line boundary and the tag (or buffer start). */
                bool boundary;
                if (idx == 0) {
                    boundary = self->stream_last_was_newline;
                } else {
                    const char *last_nl = memrchr(prefilt, '\n', idx);
                    if (!last_nl) {
                        boundary = self->stream_last_was_newline;
                        for (size_t k = 0; k < idx && boundary; k++)
                            if (!isspace((unsigned char)prefilt[k]))
                                boundary = false;
                    } else {
                        boundary = true;
                        for (size_t k = (size_t)(last_nl - prefilt) + 1; k < idx; k++)
                            if (!isspace((unsigned char)prefilt[k]))
                                boundary = false;
                    }
                }
                if (boundary) {
                    if (idx > 0) {
                        char *preceding = malloc(idx + 1);
                        if (preceding) {
                            memcpy(preceding, prefilt, idx);
                            preceding[idx] = '\0';
                            if (self->stream_emit_text_fn)
                                self->stream_emit_text_fn(self, preceding);
                            self->stream_last_was_newline =
                                (idx > 0 && prefilt[idx - 1] == '\n');
                            free(preceding);
                        }
                    }
                    self->in_reasoning_block = true;
                    size_t keep_from = idx + tag_len;
                    memmove(prefilt, prefilt + keep_from,
                            pf_len - keep_from + 1);
                    matched = true;
                    break;
                }
                search_start = idx + 1;
                if (search_start + tag_len > pf_len)
                    break;
            }
        }

        if (!matched && !self->in_reasoning_block) {
            /* Partial open tag at the end — hold it back. Python probes
             * prefix lengths i = 1..len(tag)-1 and holds back the FIRST
             * (shortest) suffix that matches a tag prefix. */
            size_t safe = pf_len;
            for (size_t t = 0; t < sizeof(B5_OPEN_TAGS) / sizeof(*B5_OPEN_TAGS); t++) {
                size_t tag_len = strlen(B5_OPEN_TAGS[t]);
                size_t max_i = tag_len < pf_len + 1 ? tag_len - 1 : pf_len;
                for (size_t i = 1; i <= max_i && safe == pf_len; i++) {
                    if (pf_len >= i &&
                        strncasecmp(prefilt + pf_len - i, B5_OPEN_TAGS[t], i) == 0)
                        safe = pf_len - i;
                }
                if (safe != pf_len)
                    break;
            }
            if (safe > 0) {
                char *emit = malloc(safe + 1);
                if (emit) {
                    memcpy(emit, prefilt, safe);
                    emit[safe] = '\0';
                    if (self->stream_emit_text_fn)
                        self->stream_emit_text_fn(self, emit);
                    self->stream_last_was_newline = (safe > 0 && prefilt[safe - 1] == '\n');
                    free(emit);
                    size_t keep = pf_len - safe;
                    memmove(prefilt, prefilt + safe, keep + 1);
                }
            }
        }
    } else {
        /* Inside a reasoning block — look for the close tag. Keep
         * accumulating because close tags can split across tokens. */
        bool closed = false;
        for (size_t t = 0; t < sizeof(B5_CLOSE_TAGS) / sizeof(*B5_CLOSE_TAGS) && !closed; t++) {
            const char *tag = B5_CLOSE_TAGS[t];
            size_t tag_len = strlen(tag);
            char *hit = pf_len >= tag_len
                            ? memmem(pf_lower, pf_len, tag, tag_len) : NULL;
            if (hit) {
                size_t idx = (size_t)(hit - pf_lower);
                self->in_reasoning_block = false;
                if (self->show_reasoning && idx > 0) {
                    char *inner_buf = malloc(idx + 1);
                    if (inner_buf) {
                        memcpy(inner_buf, prefilt, idx);
                        inner_buf[idx] = '\0';
                        if (self->stream_reasoning_delta_fn)
                            self->stream_reasoning_delta_fn(self, inner_buf);
                        free(inner_buf);
                    }
                }
                size_t after_len = pf_len - (idx + tag_len);
                char *after = malloc(after_len + 1);
                if (after) {
                    memcpy(after, prefilt + idx + tag_len, after_len + 1);
                    free(self->stream_prefilt);
                    self->stream_prefilt = NULL;
                    free(pf_lower);
                    /* Re-run the full filter over the remainder — it could
                     * hold another open tag. */
                    hcli_stream_delta(self, after);
                    free(after);
                    return;
                }
                closed = true;
            }
        }
        if (!closed) {
            /* Stream reasoning content live when show_reasoning; keep only
             * the tail that could be a partial close-tag prefix. */
            size_t max_tag_len = 0;
            for (size_t t = 0; t < sizeof(B5_CLOSE_TAGS) / sizeof(*B5_CLOSE_TAGS); t++) {
                size_t l = strlen(B5_CLOSE_TAGS[t]);
                if (l > max_tag_len)
                    max_tag_len = l;
            }
            if (pf_len > max_tag_len) {
                size_t safe_len = pf_len - max_tag_len;
                if (self->show_reasoning) {
                    char *safe_reasoning = malloc(safe_len + 1);
                    if (safe_reasoning) {
                        memcpy(safe_reasoning, prefilt, safe_len);
                        safe_reasoning[safe_len] = '\0';
                        if (self->stream_reasoning_delta_fn)
                            self->stream_reasoning_delta_fn(self, safe_reasoning);
                        free(safe_reasoning);
                    }
                }
                memmove(prefilt, prefilt + safe_len, max_tag_len + 1);
            }
        }
    }

    free(pf_lower);
}
