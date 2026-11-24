/*
 * auxiliary_stream_watchdog.c -- real C11 port of the pure window arithmetic,
 * the re-arm/fire state machine and the message shaping of upstream Hermes
 *   agent/auxiliary_stream_watchdog.py.
 *
 * PoP: w20_auxiliarys_066a48_stream_windows @ agent/auxiliary_stream_watchdog.py:chat_stream_windows
 * PoP: w20_auxiliarys_066a48_timeout_error @ agent/auxiliary_stream_watchdog.py:ChatStreamWatchdog.timeout_error
 * PoP: w20_auxiliarys_066a48_close_chunk_stream @ agent/auxiliary_stream_watchdog.py:_close_chunk_stream
 * PoP: w20_auxiliarys_066a48_simulate @ agent/auxiliary_stream_watchdog.py:ChatStreamWatchdog
 *
 * The live socket pinning (_stream_socket, _fence_release), the real
 * threading.Timer and the config reads (waits_like_local,
 * _cloud_stale_timeout_for, _get_task_no_progress_timeout) are seams: their
 * answers arrive in the spec, and inside `simulate` the clock is injected while
 * _schedule() is recorded as a generation bump. Everything else -- the min/max
 * caps, the "replace only an earlier deadline" rule and the fired/deadline
 * checks -- is the upstream code path.
 */
#include "hermes_json.h"
#include "noc_w20_auxiliarys_066a48.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *aux20_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int aux20_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* A JSON value Python would feed to float(): bool is an int there. */
static int aux20_is_number(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static double aux20_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* --- chat_stream_windows -------------------------------------------------- */

json_t *w20_auxiliarys_066a48_stream_windows(const json_t *spec) {
    json_t *task_timeout = json_obj_get(spec, "task_timeout");
    json_t *aux_default = json_obj_get(spec, "aux_default");
    json_t *cloud_stale = json_obj_get(spec, "cloud_stale");
    json_t *local = json_obj_get(spec, "local");
    json_t *timeout = json_obj_get(spec, "timeout");
    double window, first = 0.0;
    int has_first;
    json_t *out = json_array();

    /* `_get_task_no_progress_timeout(...) or _AUX_STREAM_NO_PROGRESS_TIMEOUT_SECONDS` */
    window = aux20_truthy(task_timeout) ? aux20_num(task_timeout, 60.0)
                                        : aux20_num(aux_default, 60.0);
    has_first = aux20_truthy(local) ? 0 : 1;
    if (has_first) {
        double stale = aux20_num(cloud_stale, 0.0);
        first = stale > window ? stale : window; /* max(window, cloud_stale) */
    }
    /* isinstance(timeout, (int, float)) and timeout > 0 -> cap both */
    if (aux20_is_number(timeout) && aux20_num(timeout, 0.0) > 0.0) {
        double cap = aux20_num(timeout, 0.0);
        if (cap < window) window = cap;
        if (has_first && cap < first) first = cap;
    }
    json_append(out, json_number(window));
    json_append(out, has_first ? json_number(first) : json_null());
    return out;
}

/* --- timeout_error -------------------------------------------------------- */

char *w20_auxiliarys_066a48_timeout_error(int saw_progress, double window,
                                          const json_t *first_token_window, double elapsed) {
    char buf[512];
    if (!saw_progress) {
        double first = 0.0;
        if (first_token_window && first_token_window->type == JSON_NUMBER &&
            first_token_window->num_val != 0.0) {
            first = first_token_window->num_val; /* `first_token_window or 0` */
        }
        snprintf(buf, sizeof(buf),
                 "Auxiliary chat stream produced no output within %.1fs "
                 "(no-progress timeout, %.1fs elapsed)", first, elapsed);
    } else {
        snprintf(buf, sizeof(buf),
                 "Auxiliary chat stream stalled: no new output for %.1fs "
                 "(%.1fs elapsed, timed out)", window, elapsed);
    }
    return aux20_dup(buf);
}

/* --- _close_chunk_stream -------------------------------------------------- */

const char *w20_auxiliarys_066a48_close_chunk_stream(const json_t *spec) {
    int has_close = aux20_truthy(json_obj_get(spec, "has_close"));
    int has_aclose = aux20_truthy(json_obj_get(spec, "has_aclose"));
    int allow_aclose = aux20_truthy(json_obj_get(spec, "allow_aclose"));
    int raises = aux20_truthy(json_obj_get(spec, "raises"));
    int awaitable = aux20_truthy(json_obj_get(spec, "awaitable"));

    /* getattr(chunks, "close", None) or (getattr(chunks, "aclose", None) if allow_aclose else None) */
    if (!has_close && !(allow_aclose && has_aclose)) return NULL;
    if (raises) return NULL;               /* except Exception: return None */
    return awaitable ? "awaitable" : NULL; /* only an awaitable result survives */
}

/* --- ChatStreamWatchdog state machine ------------------------------------- */

json_t *w20_auxiliarys_066a48_simulate(const json_t *spec) {
    json_t *first_v = json_obj_get(spec, "first_token");
    json_t *progress_v = json_obj_get(spec, "progress_at");
    json_t *fire_v = json_obj_get(spec, "fire_at");
    double window = aux20_num(json_obj_get(spec, "window"), 0.0);
    double start = aux20_num(json_obj_get(spec, "start"), 0.0);
    double clock = start;
    double deadline = 0.0;
    int has_deadline = 0;
    int timer_exists = 0;
    int generation = 0;
    int done = 0, saw = 0, fired = 0;
    json_t *out, *err;
    char *msg;

    /* __init__: _started = now; if first_token_window is not None: _rearm(it) */
    if (first_v && first_v->type != JSON_NULL) {
        deadline = clock + aux20_num(first_v, 0.0);
        has_deadline = 1;
        timer_exists = 1;
        generation++; /* _schedule() */
    }

    if (progress_v && progress_v->type != JSON_NULL) {
        clock = aux20_num(progress_v, clock);
        saw = 1;                 /* progress(): self.saw_progress = True */
        if (!done) {             /*            if not self._done: _rearm(self.window) */
            double previous = deadline;
            int had_previous = has_deadline;
            double new_deadline = clock + window;
            deadline = new_deadline; /* the deadline moves even when the timer is kept */
            has_deadline = 1;
            if (!timer_exists || (had_previous && new_deadline < previous)) {
                timer_exists = 1;
                generation++; /* cancel + _schedule() */
            }
        }
    }

    if (fire_v && fire_v->type != JSON_NULL) {
        int stale = aux20_truthy(json_obj_get(spec, "stale_generation"));
        clock = aux20_num(fire_v, clock);
        /* _fire(generation): ignore when done, no deadline, or superseded */
        if (!(done || !has_deadline || stale)) {
            double remaining = deadline - clock;
            if (remaining > 0) {
                timer_exists = 1;
                generation++; /* _schedule(remaining) */
            } else {
                fired = 1;    /* _sock is NULL in this simulation */
            }
        }
    }

    /* timeout_error() is shaped before finish(), exactly as the consumer calls it */
    msg = w20_auxiliarys_066a48_timeout_error(saw, window, first_v, clock - start);
    err = json_string(msg);
    free(msg);

    done = 1; /* finish() */
    out = json_object();
    json_set(out, "saw_progress", json_bool(saw));
    json_set(out, "fired", json_bool(fired));
    json_set(out, "done", json_bool(0)); /* sampled before finish(), like the live replay */
    json_set(out, "deadline_offset",
             has_deadline ? json_number(deadline - start) : json_null());
    json_set(out, "generation", json_number((double)generation));
    json_set(out, "error", err);
    return out;
}
