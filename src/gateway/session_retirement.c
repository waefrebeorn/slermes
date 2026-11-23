/*
 * session_retirement.c — C port of upstream Hermes gateway/session_retirement.py.
 *
 * PoP: w13_sessionret_ac44c5_reviewing @ gateway/session_retirement.py:_reviewing
 * PoP: w13_sessionret_ac44c5_pinned @ gateway/session_retirement.py:_pinned
 *
 * The authority's live-session registry and the asyncio loop are seams in the
 * oracle; the pin decision is a pure boolean projection of the in-memory state
 * that keeps a conversation resident. Platform identity is projected as the
 * enum's string value ("local" / "api_server").
 */
#include "noc_w13_sessionret_ac44c5.h"
#include "hermes_json.h"
#include <string.h>

static int jb(const struct json_t *ctx, const char *key) {
    struct json_t *v = json_is_object(ctx) ? json_obj_get(ctx, key) : NULL;
    return v && v->type == JSON_BOOL && v->bool_val;
}

struct json_t *w13_sessionret_ac44c5_reviewing(const struct json_t *ctx) {
    /* run = getattr(live.review, '_background_review_run', None)
     * return run is not None and not run.request_done.is_set() */
    int pinned = jb(ctx, "review_run") && !jb(ctx, "request_done");
    return json_bool(pinned);
}

struct json_t *w13_sessionret_ac44c5_pinned(const struct json_t *ctx) {
    if (!json_is_object(ctx)) return json_bool(0);

    /* task is not None and not task.done() and task is not asyncio.current_task() */
    const char *task = json_get_str(ctx, "task", "none");

    /* source is None or source.platform not in {LOCAL, API_SERVER} */
    struct json_t *source = json_obj_get(ctx, "source");
    int platform_bad = 0;
    if (!source || source->type == JSON_NULL) {
        platform_bad = 1;
    } else {
        const char *p = json_is_object(source) ? json_get_str(source, "platform", "") : "";
        platform_bad = strcmp(p, "local") && strcmp(p, "api_server");
    }

    int pinned =
        !jb(ctx, "mapped")                       /* authority.sessions.get(id) is not live */
        || platform_bad
        || jb(ctx, "subscribers")
        || jb(ctx, "observers")
        || jb(ctx, "rescan")
        || jb(ctx, "settling")
        || jb(ctx, "detached")
        || jb(ctx, "detaching")
        || jb(ctx, "history_mount")
        || jb(ctx, "mutation_locked")
        || jb(ctx, "claim_locked")
        || !strcmp(task, "running")
        || jb(ctx, "in_executing")
        || jb(ctx, "in_pending_closes")
        || jb(ctx, "in_orphan_closes")
        || (jb(ctx, "review_run") && !jb(ctx, "request_done"));
    return json_bool(pinned);
}

