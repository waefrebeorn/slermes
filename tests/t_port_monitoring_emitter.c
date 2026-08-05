/* t_port_monitoring_emitter.c — differential oracle driver for
 * agent/monitoring/emitter.py. Exercises deterministic behavior (emit/drain/
 * subscribe/stats/singleton) without racing the background thread by using
 * drain_once() in place of the dispatcher loop where determinism matters.
 */

#include "port_monitoring_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jb(bool v) { return v ? "true" : "false"; }

static json_t *jobj2(const char *k, const char *v) {
    json_t *o = json_object();
    json_set(o, k, json_string(v));
    return o;
}
static json_t *jobj_num(const char *k, long n) {
    json_t *o = json_object();
    json_set(o, k, json_number((double)n));
    return o;
}

/* subscriber that records received events into a captured batch (global) */
static json_t *g_captured = NULL;
static int g_fail_next = 0;
static void sub_capture(void *ctx, const json_t *batch) {
    (void)ctx;
    if (g_fail_next) { g_fail_next = 0; monitoring_mark_subscriber_failed(); return; }
    if (g_captured) json_free(g_captured);
    g_captured = json_deep_copy(batch);
}
static void sub_count(void *ctx, const json_t *batch) {
    long *p = (long *)ctx;
    *p += (long)json_len(batch);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    /* small object builders */
    /* json_t *jobj2(const char *k, const char *v) */

    /* --- emit disabled is a no-op --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(false, 0);
        int q = monitoring_emitter_emit(em, json_object());
        printf("{\"case\":\"disabled_emit\",\"ret\":%d,\"queued\":%d}\n", q,
               (int)json_get_num(monitoring_emitter_stats(em), "queued", -1));
        monitoring_emitter_free(em);
    }

    /* --- emit + drain_once dispatches to subscriber --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(true, 0);
        monitoring_emitter_subscribe(em, sub_capture, NULL);
        monitoring_emitter_emit(em, jobj2("name","a"));
        monitoring_emitter_emit(em, jobj2("name","b"));
        int n = monitoring_emitter_drain_once(em);
        printf("{\"case\":\"drain\",\"dispatched\":%d,\"captured\":%d,\"first_name\":%s}\n",
               n,
               (int)json_len(g_captured),
               jb(json_get_str(json_get(g_captured, 0), "name", "") &&
                   strcmp(json_get_str(json_get(g_captured, 0), "name", ""), "a") == 0));
        if (g_captured) { json_free(g_captured); g_captured = NULL; }
        monitoring_emitter_free(em);
    }

    /* --- drop-oldest on full queue --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(true, 3); /* maxsize 3 */
        monitoring_emitter_emit(em, jobj_num("i",1));
        monitoring_emitter_emit(em, jobj_num("i",2));
        monitoring_emitter_emit(em, jobj_num("i",3));
        /* 4th push: drop oldest (i=1) */
        monitoring_emitter_emit(em, jobj_num("i",4));
        json_t *st = monitoring_emitter_stats(em);
        long dropped = (long)json_get_num(st, "dropped", -1);
        long queued = (long)json_get_num(st, "queued", -1);
        json_free(st);
        printf("{\"case\":\"drop_oldest\",\"dropped\":%ld,\"queued\":%ld}\n", dropped, queued);
        monitoring_emitter_free(em);
    }

    /* --- subscribe enables; unsubscribe-last disables --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(false, 0);
        long cnt = 0;
        /* disabled: emit no-op */
        int before = monitoring_emitter_emit(em, json_object());
        monitoring_emitter_subscribe(em, sub_count, &cnt);
        int after = monitoring_emitter_emit(em, json_object());
        printf("{\"case\":\"sub_toggle\",\"before\":%d,\"after\":%d}\n", before, after);
        monitoring_emitter_unsubscribe(em, sub_count, &cnt);
        /* now disabled again */
        int after_unsub = monitoring_emitter_emit(em, json_object());
        printf("{\"case\":\"unsub_toggle\",\"after_unsub\":%d}\n", after_unsub);
        monitoring_emitter_free(em);
    }

    /* --- fail isolation: a raising subscriber doesn't stop peers --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(true, 0);
        long cnt = 0;
        monitoring_emitter_subscribe(em, sub_capture, NULL);
        monitoring_emitter_subscribe(em, sub_count, &cnt);
        monitoring_emitter_emit(em, jobj_num("x",1));
        /* make capture fail on next dispatch */
        g_fail_next = 1;
        int n = monitoring_emitter_drain_once(em);
        printf("{\"case\":\"fail_iso\",\"dispatched\":%d,\"peer_count\":%ld}\n", n, cnt);
        if (g_captured) { json_free(g_captured); g_captured = NULL; }
        monitoring_emitter_free(em);
    }

    /* --- stats after dispatch --- */
    {
        monitoring_emitter_t *em = monitoring_emitter_new(true, 0);
        monitoring_emitter_subscribe(em, sub_capture, NULL);
        monitoring_emitter_emit(em, jobj_num("k",1));
        monitoring_emitter_emit(em, jobj_num("k",2));
        monitoring_emitter_drain_once(em);
        json_t *st = monitoring_emitter_stats(em);
        long disp = (long)json_get_num(st, "dispatched", -1);
        long subs = (long)json_get_num(st, "subscribers", -1);
        json_free(st);
        printf("{\"case\":\"stats\",\"dispatched\":%ld,\"subscribers\":%ld}\n", disp, subs);
        if (g_captured) { json_free(g_captured); g_captured = NULL; }
        monitoring_emitter_free(em);
    }

    /* --- singleton get/reset --- */
    {
        monitoring_emitter_t *a = monitoring_emitter_get();
        monitoring_emitter_t *b = monitoring_emitter_get();
        printf("{\"case\":\"singleton\",\"same\":%s}\n", jb(a == b));
        monitoring_emitter_t *c = monitoring_emitter_new(false, 0);
        monitoring_emitter_reset_for_tests(c);
        monitoring_emitter_t *d = monitoring_emitter_get();
        printf("{\"case\":\"reset\",\"changed\":%s}\n", jb(d == c));
        monitoring_emitter_reset_for_tests(NULL); /* restore */
        monitoring_emitter_free(c);
    }

    return 0;
}
