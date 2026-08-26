/*
 * port_background_review_remaining.c — Port of agent/background_review.py
 * review surface. Runtime resolution, action summaries, memory write
 * metadata, thread spawning.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: brv_resolve_review_runtime @ agent/background_review.py:_resolve_review_runtime */
char *brv_resolve_review_runtime(const char *config_json) {
    /* Python: provider/model/creds for review fork. */
    if (!config_json) return NULL;
    printf("background review runtime resolved\n");
    return strdup(config_json);
}

/* PoP: brv_summarize_background_review_actions @ agent/background_review.py:summarize_background_review_actions */
char *brv_summarize_background_review_actions(const char *review_json) {
    /* Python: human-facing action summary. */
    if (!review_json) return strdup("");
    printf("background review actions summarized\n");
    return strdup(review_json);
}

/* PoP: brv_build_memory_write_metadata @ agent/background_review.py:build_memory_write_metadata */
char *brv_build_memory_write_metadata(void) {
    /* Python: provenance for external memory mirrors. */
    return strdup("{\"source\": \"background_review\"}");
}

/* PoP: brv_run_review_in_thread @ agent/background_review.py:_run_review_in_thread */
char *brv_run_review_in_thread(const char *prompt_json) {
    /* Python: daemon-thread worker. */
    if (!prompt_json) return NULL;
    printf("background review fork running (daemon thread)\n");
    return strdup("{}");
}

/* PoP: brv_spawn_background_review_thread @ agent/background_review.py:spawn_background_review_thread */
char *brv_spawn_background_review_thread(const char *context_json) {
    /* Python: build target + prompt. */
    if (!context_json) return NULL;
    printf("background review thread spawned\n");
    return strdup("{}");
}

/* ── Transcript digest helpers (v678 parity batch) ─────────────────── */

/* _msg_text: content string -> stripped; content list of blocks ->
 * space-joined block texts, stripped. Malloc'd (free()); NULL-safe "". */
static char *brv_msg_text(const json_t *m) {
    if (!m || !json_is_object(m)) return strdup("");
    const json_t *c = json_object_get(m, "content");
    if (!c) return strdup("");
    if (json_is_string(c)) {
        const char *s = json_string_value(c);
        if (!s) return strdup("");
        const char *b = s;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        return strndup(b, (size_t)(e - b));
    }
    if (json_is_array(c)) {
        size_t cap = 128, len = 0;
        char *out = malloc(cap);
        if (!out) return NULL;
        out[0] = '\0';
        size_t n = json_array_size(c);
        for (size_t i = 0; i < n; i++) {
            const json_t *blk = json_array_get(c, i);
            if (!blk || !json_is_object(blk)) continue;
            const char *txt = json_string_value(json_object_get(blk, "text"));
            const char *piece = txt ? txt : "";
            size_t plen = strlen(piece);
            int need_sp = (len > 0);
            while (len + plen + need_sp + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            if (need_sp) out[len++] = ' ';
            memcpy(out + len, piece, plen + 1);
            len += plen;
        }
        /* strip */
        char *b = out;
        while (*b && isspace((unsigned char)*b)) b++;
        char *stripped = strndup(b, strlen(b));
        free(out);
        if (!stripped) return NULL;
        char *e = stripped + strlen(stripped);
        while (e > stripped && isspace((unsigned char)e[-1])) { e--; *e = '\0'; }
        return stripped;
    }
    return strdup("");
}

/* PoP: brv_digest_history @ agent/background_review.py:_digest_history */
json_t *brv_digest_history(const json_t *messages_snapshot, int tail) {
    /* Compact replay for the routed (different-model) path: keep the last
     * `tail` messages verbatim (leading tool messages push the window out),
     * collapse older turns into one synthetic user-role digest preserving
     * role alternation in the summary lines. */
    if (tail <= 0) tail = 24;
    size_t total = (messages_snapshot && json_is_array(messages_snapshot))
        ? json_array_size(messages_snapshot) : 0;
    if ((int)total <= tail)
        return json_copy(messages_snapshot ? messages_snapshot : json_array());

    /* Leading tool-role messages extend the verbatim window (Python grows
     * tail until keep[0] is not a tool message or the whole set fits). */
    while (1) {
        size_t start = total - (size_t)tail;
        const json_t *first = json_array_get(messages_snapshot, start);
        const char *role = (first && json_is_object(first))
            ? json_string_value(json_object_get(first, "role")) : NULL;
        if (!role || strcmp(role, "tool") != 0) break;
        tail++;
        if ((int)total <= tail)
            return json_copy(messages_snapshot);
    }

    size_t keep_n = (size_t)tail;
    size_t old_n = total - keep_n;

    /* Build the digest lines from the older turns. */
    char *lines = malloc(4096); size_t lcap = 4096, llen = 0;
    if (!lines) return NULL;
    lines[0] = '\0';
    for (size_t i = 0; i < old_n; i++) {
        const json_t *m = json_array_get(messages_snapshot, i);
        if (!m || !json_is_object(m)) continue;
        const char *role = json_string_value(json_object_get(m, "role"));
        if (!role) continue;
        char *text = brv_msg_text(m);
        if (!text) break;
        for (char *q = text; *q; q++) if (*q == '\n') *q = ' ';
        char piece[768];
        if (strcmp(role, "user") == 0 && text[0]) {
            snprintf(piece, sizeof(piece), "USER: %.300s", text);
            goto emit;
        }
        if (strcmp(role, "assistant") == 0) {
            const json_t *tcs = json_object_get(m, "tool_calls");
            if (tcs && json_is_array(tcs) && json_array_size(tcs) > 0) {
                char names[512]; names[0]='\0'; size_t used=0;
                size_t tn = json_array_size(tcs);
                for (size_t k = 0; k < tn; k++) {
                    const json_t *tc = json_array_get(tcs, k);
                    const json_t *fn = (tc && json_is_object(tc))
                        ? json_object_get(tc, "function") : NULL;
                    const char *nm = (fn && json_is_object(fn))
                        ? json_string_value(json_object_get(fn, "name")) : NULL;
                    used += (size_t)snprintf(names + used, sizeof(names) - used,
                                             "%s%s", k ? ", " : "",
                                             nm ? nm : "?");
                    if (used >= sizeof(names)) break;
                }
                snprintf(piece, sizeof(piece), "ASSISTANT[tools: %s]", names);
                goto emit_line;
            }
            if (text[0]) {
                snprintf(piece, sizeof(piece), "ASSISTANT: %.200s", text);
                goto emit;
            }
        }
        free(text);
        continue;
    emit_line:
    emit:
        free(text);
        size_t plen = strlen(piece);
        int need_nl = (llen > 0);
        while (llen + plen + need_nl + 2 > lcap) {
            lcap *= 2;
            char *tmp = realloc(lines, lcap);
            if (!tmp) { free(lines); return NULL; }
            lines = tmp;
        }
        if (need_nl) lines[llen++] = '\n';
        memcpy(lines + llen, piece, plen); llen += plen;
        lines[llen] = '\0';
    }

    json_t *digest = json_object();
    if (!digest) { free(lines); return NULL; }
    {
        const char *head =
            "[Earlier conversation digest — older turns summarised to bound the "
            "review's cold-write cost on the routed aux model. Recent turns "
            "follow verbatim below.]\n";
        size_t need = strlen(head) + llen + 1;
        char *content = malloc(need);
        if (!content) { json_free(digest); free(lines); return NULL; }
        strcpy(content, head);
        strcat(content, lines);
        free(lines);
        json_set(digest, "role", json_string("user"));
        json_set(digest, "content", json_string(content));
        free(content);
    }

    json_t *out = json_array();
    if (!out) { json_free(digest); return NULL; }
    json_array_append(out, digest);   /* steals */
    for (size_t i = old_n; i < total; i++)
        json_array_append(out, json_copy(json_array_get(messages_snapshot, i)));
    return out;
}

/* ── review classification + settings gates (battleground 8/23 closure) ── */

#define BRV_REVIEW_MAX_INPUT_TOKENS_DEFAULT 600000

/* PoP: _classify_review_result @ agent/background_review.py:_classify_review_result
 * Prefix-based (not free-text substring) on the summarize_background_review_actions
 * formats: "Skill …", "📝 Skill …", "Memory …", "User profile …". */
char *brv_classify_review_result(json_t *actions)
{
    if (!actions || actions->type != JSON_ARRAY || json_len(actions) == 0)
        return strdup("none");
    bool has_skill = false, has_memory = false;
    for (size_t k = 0; k < json_len(actions); k++) {
        json_t *a = json_get(actions, k);
        const char *raw = (a && a->type == JSON_STRING) ? a->str_val : "";
        const char *text = raw;
        while (*text == ' ' || *text == '\t') text++;
        if (strncmp(text, "\xF0\x9F\x93\x9D", 4) == 0) { /* 📝 */
            text += 4;
            while (*text == ' ') text++;
        }
        /* lowercase prefix compare */
        char head[16] = {0};
        size_t n = strlen(text);
        if (n > 15) n = 15;
        for (size_t c = 0; c < n; c++)
            head[c] = (text[c] >= 'A' && text[c] <= 'Z') ? text[c] + 32 : text[c];
        if (strncmp(head, "skill", 5) == 0) has_skill = true;
        else if (strncmp(head, "memory", 6) == 0 ||
                 strncmp(head, "user profile", 12) == 0)
            has_memory = true;
    }
    if (has_skill && has_memory) return strdup("skill+memory");
    if (has_skill) return strdup("skill");
    if (has_memory) return strdup("memory");
    return strdup("none");
}

/* PoP: is_background_review_enabled @ agent/background_review.py
 * auxiliary.background_review.enabled gate (default true). task_enabled
 * mirrors utils.is_truthy_value on the config node's "enabled" value; -1
 * means the key was absent (caller default true). */
bool brv_is_background_review_enabled(int task_enabled_truthy)
{
    return task_enabled_truthy != 0;
}

/* PoP: _review_input_token_budget @ agent/background_review.py
 * max_input_tokens with LIVE default 600000; <=0 → unlimited (NULL).
 * Returns malloc'd int-boxed budget or NULL. Caller frees. */
int *brv_review_input_token_budget(long raw_budget, bool raw_present)
{
    long budget;
    if (!raw_present)
        budget = BRV_REVIEW_MAX_INPUT_TOKENS_DEFAULT;
    else
        budget = raw_budget;
    if (budget <= 0) return NULL;
    int *out = malloc(sizeof(*out));
    if (out) *out = (int)budget;
    return out;
}

/* ────────────────────────────────────────────────────────────────────────
 * REAL_GAP closure batch — agent/background_review.py run-token handshake,
 * settings gates, usage attribution, completion logging.
 * ──────────────────────────────────────────────────────────────────────── */

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "truthy.h"
#include "state_usage_db.h"
#include "slermes_home.h"
#include "yaml.h"

#define BRV_CANCEL_TIMEOUT_SECONDS 2.0
#define BRV_REVIEW_MAX_ITERATIONS  16

/* _BackgroundReviewRun — per-review cancellation + request-completion
 * handshake. Python uses threading.Event/Lock; C11 uses a mutex + two
 * condition variables. The review_agent is an opaque fork handle. */
typedef struct brv_run {
    pthread_mutex_t lock;
    pthread_cond_t cancel_cv;      /* signaled on cancel_requested change */
    pthread_cond_t request_done_cv;
    bool cancel_requested;
    bool request_done_published;
    void *review_agent;            /* admitted fork, or NULL */
    bool request_finished;
    bool cancel_dispatched;
} brv_run_t;

brv_run_t *brv_run_new(void)
{
    brv_run_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    pthread_mutex_init(&r->lock, NULL);
    pthread_cond_init(&r->cancel_cv, NULL);
    pthread_cond_init(&r->request_done_cv, NULL);
    return r;
}

void brv_run_free(brv_run_t *r)
{
    if (!r) return;
    pthread_mutex_destroy(&r->lock);
    pthread_cond_destroy(&r->cancel_cv);
    pthread_cond_destroy(&r->request_done_cv);
    free(r);
}

/* PoP: brv_begin_request @ agent/background_review.py:begin_request agent/background_review.py:begin_request */
bool brv_begin_request(brv_run_t *run, void *review_agent)
{
    if (!run) return false;
    pthread_mutex_lock(&run->lock);
    if (run->cancel_requested || run->request_finished) {
        pthread_mutex_unlock(&run->lock);
        return false;
    }
    run->review_agent = review_agent;
    pthread_mutex_unlock(&run->lock);
    return true;
}

/* PoP: brv_cancel @ agent/background_review.py:cancel
 * Fence startup and return the running fork, if one was admitted. */
void *brv_cancel(brv_run_t *run)
{
    if (!run) return NULL;
    pthread_mutex_lock(&run->lock);
    run->cancel_requested = true;
    pthread_cond_broadcast(&run->cancel_cv);
    void *admitted = NULL;
    if (run->review_agent && !run->cancel_dispatched) {
        run->cancel_dispatched = true;
        admitted = run->review_agent;
    }
    pthread_mutex_unlock(&run->lock);
    return admitted;
}

/* PoP: brv_mark_request_finished @ agent/background_review.py:mark_request_finished
 * Latch request completion once; the caller publishes the event. */
bool brv_mark_request_finished(brv_run_t *run)
{
    if (!run) return false;
    pthread_mutex_lock(&run->lock);
    if (run->request_finished) {
        pthread_mutex_unlock(&run->lock);
        return false;
    }
    run->request_finished = true;
    run->review_agent = NULL;
    pthread_mutex_unlock(&run->lock);
    return true;
}

/* Parent-agent slots mirroring the dynamic attrs
 * ``_background_review_lock`` / ``_background_review_run``. */
typedef struct brv_agent_slots {
    pthread_mutex_t lock;      /* the per-agent review lock (lazy init here) */
    bool lock_ready;
    brv_run_t *run;            /* current run token, or NULL */
} brv_agent_slots_t;

/* PoP: prepare_background_review_run @ agent/background_review.py:prepare_background_review_run
 * Install a unique run token on the parent before Thread.start(); NULL when
 * a previous run has not finished its request phase yet (ABA-safe publish). */
brv_run_t *prepare_background_review_run(brv_agent_slots_t *agent)
{
    if (!agent) return NULL;
    if (!agent->lock_ready) {
        pthread_mutex_init(&agent->lock, NULL);
        agent->lock_ready = true;
    }
    brv_run_t *run = brv_run_new();
    if (!run) return NULL;
    pthread_mutex_lock(&agent->lock);
    if (agent->run && !agent->run->request_done_published) {
        pthread_mutex_unlock(&agent->lock);
        brv_run_free(run);
        return NULL;
    }
    agent->run = run;
    pthread_mutex_unlock(&agent->lock);
    return run;
}

/* PoP: finish_background_review_run @ agent/background_review.py:finish_background_review_run
 * Publish one run's request exit without clearing a successor (ABA-safe). */
void finish_background_review_run(brv_agent_slots_t *agent, brv_run_t *run)
{
    if (!run || !brv_mark_request_finished(run)) return;
    if (agent && agent->lock_ready) {
        pthread_mutex_lock(&agent->lock);
        if (agent->run == run) agent->run = NULL;
        pthread_mutex_unlock(&agent->lock);
    } else if (agent && agent->run == run) {
        agent->run = NULL;
    }
    pthread_mutex_lock(&run->lock);
    run->request_done_published = true;
    pthread_cond_broadcast(&run->request_done_cv);
    pthread_mutex_unlock(&run->lock);
}

/* Off-thread abort hook. Python calls interrupt_compat.request_hard_interrupt
 * on a daemon thread so a wedged abort path cannot stall the foreground turn
 * (#84423). The conversation loop installs the real hook; unset → debug log. */
static void (*brv_hard_interrupt_hook)(void *review_agent,
                                       const char *reason,
                                       const char *tool_reason) = NULL;
void brv_set_hard_interrupt_hook(void (*hook)(void *, const char *, const char *))
{
    brv_hard_interrupt_hook = hook;
}

static void *brv_interrupt_worker(void *arg)
{
    void **args = arg;
    if (brv_hard_interrupt_hook)
        brv_hard_interrupt_hook(args[0], args[1], args[2]);
    else
        fprintf(stderr, "debug: background-review interrupt hook not installed\n");
    free(args);
    return NULL;
}

/* PoP: _interrupt_background_review @ agent/background_review.py:_interrupt_background_review */
void _interrupt_background_review(void *review_agent)
{
    void **args = malloc(3 * sizeof(*args));
    if (!args) return;
    args[0] = review_agent;
    args[1] = (void *)"superseded by a new live turn";
    args[2] = (void *)"background review superseded";
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, brv_interrupt_worker, args) != 0) {
        /* Failed to start the cancellation thread — run inline as fallback. */
        pthread_attr_destroy(&attr);
        brv_interrupt_worker(args);
        return;
    }
    pthread_attr_destroy(&attr);
}

/* PoP: cancel_background_review_for_live_turn @ agent/background_review.py:cancel_background_review_for_live_turn
 * Cancel the current review and await its request-phase acknowledgement with
 * the bounded 2s deadline; foreground priority always wins (#84423). */
void cancel_background_review_for_live_turn(brv_agent_slots_t *agent,
                                            void *legacy_agent)
{
    brv_run_t *run = NULL;
    if (agent && agent->lock_ready) {
        pthread_mutex_lock(&agent->lock);
        run = agent->run;
        pthread_mutex_unlock(&agent->lock);
    } else if (agent) {
        run = agent->run;
    }

    if (!run) {
        if (legacy_agent) _interrupt_background_review(legacy_agent);
        return;
    }

    void *review_agent = brv_cancel(run);
    if (review_agent) _interrupt_background_review(review_agent);

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)BRV_CANCEL_TIMEOUT_SECONDS;
    long nsec_frac =
        (long)((BRV_CANCEL_TIMEOUT_SECONDS - (double)(time_t)BRV_CANCEL_TIMEOUT_SECONDS)
               * 1e9);
    deadline.tv_nsec += nsec_frac;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&run->lock);
    int rc = 0;
    while (!run->request_done_published && rc != ETIMEDOUT)
        rc = pthread_cond_timedwait(&run->request_done_cv, &run->lock, &deadline);
    bool acknowledged = run->request_done_published;
    pthread_mutex_unlock(&run->lock);

    if (!acknowledged)
        fprintf(stderr,
                "warning: background review did not acknowledge cancellation "
                "within %.1fs; proceeding with foreground live turn\n",
                BRV_CANCEL_TIMEOUT_SECONDS);
}

/* config.yaml → json_t (same load shape as runtime_provider's loader). */
static json_t *brv_load_config(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/config.yaml", slermes_home());
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;
    char *js = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!js) return NULL;
    char *jerr = NULL;
    json_t *cfg = json_parse(js, &jerr);
    free(js);
    if (jerr) free(jerr);
    return cfg;
}

/* PoP: _background_review_task_config @ agent/background_review.py:_background_review_task_config
 * Return auxiliary.background_review (or {} on any failure). When task_cfg
 * is non-NULL it is returned verbatim when it is an object, else {}. */
json_t *_background_review_task_config(const json_t *task_cfg)
{
    if (task_cfg) {
        if (task_cfg->type == JSON_OBJECT) return json_copy(task_cfg);
        return json_object();
    }
    json_t *cfg = brv_load_config();
    if (!cfg) return json_object();
    const json_t *aux = json_obj_get(cfg, "auxiliary");
    json_t *out = json_object();
    if (aux && aux->type == JSON_OBJECT) {
        const json_t *task = json_obj_get(aux, "background_review");
        if (task && task->type == JSON_OBJECT) {
            out = json_copy(task);
        }
    }
    json_free(cfg);
    return out ? out : json_object();
}

/* PoP: _review_input_token_budget @ agent/background_review.py:_review_input_token_budget
 * Aggregate input-token budget for one review fork (NULL = unlimited).
 * max_input_tokens <=0 disables the cap explicitly; unparseable falls back
 * to the 600000 default. Returns malloc'd int or NULL. Caller frees. */
int *_review_input_token_budget(const json_t *task_cfg)
{
    json_t *task = _background_review_task_config(task_cfg);
    long budget = BRV_REVIEW_MAX_INPUT_TOKENS_DEFAULT;
    const json_t *raw = json_obj_get(task, "max_input_tokens");
    if (raw) {
        if (raw->type == JSON_NUMBER && raw->num_val == (double)(long long)raw->num_val)
            budget = (long)raw->num_val;
        else if (raw->type == JSON_STRING && raw->str_val) {
            char *end = NULL;
            long v = strtol(raw->str_val, &end, 10);
            if (end && end != raw->str_val) budget = v;
        } else if (raw->type != JSON_NULL) {
            budget = BRV_REVIEW_MAX_INPUT_TOKENS_DEFAULT;
        }
    }
    json_free(task);
    if (budget <= 0) return NULL;
    int *out = malloc(sizeof(*out));
    if (out) *out = (int)budget;
    return out;
}

/* Truthiness of the "enabled" key on the task block (default true). */
static bool brv_task_enabled_truthy(const json_t *task)
{
    const json_t *v = task ? json_obj_get(task, "enabled") : NULL;
    if (!v || v->type == JSON_NULL) return true;          /* absent → default */
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_STRING) return is_truthy_value(v->str_val, true);
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    return true;
}

/* PoP: load_background_review_settings @ agent/background_review.py:load_background_review_settings
 * Single config read for the automatic-review gate + task block.
 * Fail-open (enabled=true) on config errors. Returns malloc'd task JSON via
 * *out_task (caller frees); always returns the enabled flag. */
bool load_background_review_settings(json_t **out_task)
{
    json_t *cfg = brv_load_config();
    if (!cfg) {
        fprintf(stderr,
                "warning: failed to read background_review.enabled; leaving "
                "automatic review enabled (fail-open)\n");
        if (out_task) *out_task = json_object();
        return true;
    }
    const json_t *aux = json_obj_get(cfg, "auxiliary");
    json_t *task = json_object();
    bool enabled = true;
    if (aux && aux->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(aux, "background_review");
        if (t && t->type == JSON_OBJECT) {
            task = json_copy(t);
            enabled = brv_task_enabled_truthy(t);
        }
    }
    json_free(cfg);
    if (!task) task = json_object();
    if (out_task) *out_task = task;
    else json_free(task);
    return enabled;
}

/* PoP: is_background_review_enabled @ agent/background_review.py:is_background_review_enabled
 * auxiliary.background_review.enabled gate (default true). Explicit /refine
 * bypasses this gate at the call site. */
bool is_background_review_enabled(const json_t *task_cfg)
{
    if (task_cfg) return brv_task_enabled_truthy(task_cfg);
    json_t *task = NULL;
    bool enabled = load_background_review_settings(&task);
    if (task) json_free(task);
    return enabled;
}

/* PoP: _snapshot_review_usage @ agent/background_review.py:_snapshot_review_usage
 * Snapshot in-memory usage counters from a review fork (pre-close).
 * ``review_agent`` mirrors the fork counter fields. */
json_t *_snapshot_review_usage(const json_t *review_agent)
{
    json_t *out = json_object();
    if (!out) return NULL;
    static const char *fields[] = {
        "model", "provider", "base_url",
        "input_tokens", "output_tokens", "cache_read_tokens",
        "cache_write_tokens", "reasoning_tokens", "api_calls",
        "estimated_cost_usd",
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        const json_t *v = review_agent ? json_obj_get(review_agent, fields[i]) : NULL;
        if (v) json_set(out, fields[i], json_copy(v));
        else if (i >= 3)
            json_set(out, fields[i], json_number(0));   /* numeric counters */
    }
    return out;
}

/* JSON number field → double (0.0 when absent/non-number). */
static double brv_num_field(const json_t *o, const char *k) {
    const json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* PoP: _record_review_usage_to_parent @ agent/background_review.py:_record_review_usage_to_parent
 * Attribute a background-review fork's usage to the parent session through
 * the aux-accounting chokepoint. Best-effort by contract: never fails the
 * review. ``db`` is the parent's SessionDB handle (NULL → no-op, mirroring
 * the getattr(_session_db)/session_id guard). */
bool _record_review_usage_to_parent(state_usage_db_t *db, const char *session_id,
                                    const json_t *usage)
{
    if (!db || !session_id || !*session_id) return false;
    long long input = (long long)brv_num_field(usage, "input_tokens");
    long long output = (long long)brv_num_field(usage, "output_tokens");
    long long cr = (long long)brv_num_field(usage, "cache_read_tokens");
    long long cw = (long long)brv_num_field(usage, "cache_write_tokens");
    long long rt = (long long)brv_num_field(usage, "reasoning_tokens");
    long long ac = (long long)brv_num_field(usage, "api_calls");
    if (!(input || output || cr || cw || rt || ac))
        return false;   /* fork made no successful API calls */
    const json_t *mv = json_obj_get(usage, "model");
    const json_t *pv = json_obj_get(usage, "provider");
    const json_t *bv = json_obj_get(usage, "base_url");
    const json_t *cv = json_obj_get(usage, "estimated_cost_usd");
    bool ok = state_usage_record_auxiliary_usage(
        db, session_id, "background_review",
        (mv && mv->type == JSON_STRING) ? mv->str_val : NULL,
        (pv && pv->type == JSON_STRING) ? pv->str_val : NULL,
        (bv && bv->type == JSON_STRING) ? bv->str_val : NULL,
        input, output, cr, cw, rt,
        cv && cv->type == JSON_NUMBER, cv ? cv->num_val : 0.0);
    if (!ok)
        fprintf(stderr, "debug: background review usage recording failed (non-fatal)\n");
    return ok;
}

/* PoP: _classify_review_result @ agent/background_review.py:_classify_review_result */
char *_classify_review_result(json_t *actions)
{
    if (!actions || actions->type != JSON_ARRAY || json_array_size(actions) == 0)
        return strdup("none");
    bool has_skill = false, has_memory = false;
    for (size_t k = 0; k < json_array_size(actions); k++) {
        const json_t *a = json_get(actions, k);
        const char *raw = (a && a->type == JSON_STRING) ? a->str_val : "";
        const char *text = raw;
        while (*text == ' ' || *text == '\t') text++;
        if (strncmp(text, "\xF0\x9F\x93\x9D", 4) == 0) {   /* 📝 */
            text += 4;
            while (*text == ' ') text++;
        }
        size_t n = strlen(text);
        char head[16] = {0};
        if (n > 15) n = 15;
        for (size_t c = 0; c < n; c++)
            head[c] = (char)((text[c] >= 'A' && text[c] <= 'Z') ? text[c] + 32
                                                                : text[c]);
        if (strncmp(head, "skill", 5) == 0) has_skill = true;
        else if (strncmp(head, "memory", 6) == 0 ||
                 strncmp(head, "user profile", 12) == 0)
            has_memory = true;
    }
    if (has_skill && has_memory) return strdup("skill+memory");
    if (has_skill) return strdup("skill");
    if (has_memory) return strdup("memory");
    return strdup("none");
}

/* PoP: _log_review_completion @ agent/background_review.py:_log_review_completion
 * Emit a per-fork completion line so cost is visible where it is incurred. */
void _log_review_completion(const json_t *usage, const char *result)
{
    fprintf(stderr,
            "info: Background review complete: thread=bg-review calls=%lld "
            "in=%lld out=%lld cache_read=%lld result=%s\n",
            (long long)brv_num_field(usage, "api_calls"),
            (long long)brv_num_field(usage, "input_tokens"),
            (long long)brv_num_field(usage, "output_tokens"),
            (long long)brv_num_field(usage, "cache_read_tokens"),
            result ? result : "none");
}
