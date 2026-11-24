/*
 * run_delivery_queue_watch.c -- real C11 port of upstream Hermes
 *   gateway/run_delivery_queue_watch.py.
 *
 * PoP: w20_rundeliver_c36151_watch_seconds @ gateway/run_delivery_queue_watch.py:DELIVERY_QUEUE_WATCH_SECONDS
 * PoP: w20_rundeliver_c36151_drain_label @ gateway/run_delivery_queue_watch.py:DRAIN_LABEL
 * PoP: w20_rundeliver_c36151_stamp @ gateway/run_delivery_queue_watch.py:DeliveryQueueWatch._stamp
 * PoP: w20_rundeliver_c36151_changed @ gateway/run_delivery_queue_watch.py:DeliveryQueueWatch.changed
 * PoP: w20_rundeliver_c36151_wait_plan @ gateway/run_delivery_queue_watch.py:wait_for_next_tick
 *
 * os.stat and the monotonic clock are injected from the fixture; the watch
 * logic itself (which files are stamped, tuple equality, the drain-on-change
 * loop) is ported verbatim.  Homes are taken as already-resolved absolute
 * roots, matching queue_path()'s resolved root.
 */
#include "hermes_json.h"
#include "noc_w20_rundeliver_c36151.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNDEL_MAX_STAMPS 32

double w20_rundeliver_c36151_watch_seconds(void) { return 1.0; }

const char *w20_rundeliver_c36151_drain_label(void) {
    return "Cron durable delivery queue drain";
}

typedef struct {
    char path[1024];
    long long mtime_ns;
    long long size;
} rundel_stamp;

/* json_get_num only reads object keys; array items are numbers themselves. */
static double rundel_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static int rundel_truthy(const json_t *v) {
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

static void rundel_join(char *out, size_t cap, const char *root, const char *name) {
    snprintf(out, cap, "%s/cron/%s", root ? root : "", name);
}

/* os.stat(candidate) as injected: a matching {"path","mtime_ns","size"} record
 * is a hit; {"path","errno"} (or a missing record) is the OSError the watch
 * skips. */
static const json_t *rundel_stat_lookup(const json_t *stats, const char *path) {
    size_t i;
    if (!stats || stats->type != JSON_ARRAY) return NULL;
    for (i = 0; i < stats->c.count; i++) {
        json_t *rec = stats->c.items[i];
        const char *p = json_get_str(rec, "path", NULL);
        if (!p || strcmp(p, path)) continue;
        if (rundel_truthy(json_obj_get(rec, "errno"))) return NULL;
        return rec;
    }
    return NULL;
}

static int rundel_collect(const json_t *homes, const json_t *stats, const char *default_home,
                          rundel_stamp *out, int cap) {
    size_t i;
    int n = 0;
    if (!homes || homes->type != JSON_ARRAY) return 0;
    for (i = 0; i < homes->c.count && n < cap; i++) {
        json_t *home = homes->c.items[i];
        const char *root = default_home ? default_home : "";
        char path[1024];
        int pass;
        if (home && home->type == JSON_STRING && home->str_val && home->str_val[0]) {
            root = home->str_val;
        }
        for (pass = 0; pass < 2; pass++) {
            const json_t *rec;
            if (pass == 0) {
                rundel_join(path, sizeof(path), root, "deliveries.db");
            } else {
                char base[1024];
                rundel_join(base, sizeof(base), root, "deliveries.db");
                snprintf(path, sizeof(path), "%s-wal", base);
            }
            rec = rundel_stat_lookup(stats, path);
            if (!rec) continue;
            snprintf(out[n].path, sizeof(out[n].path), "%s", path);
            out[n].mtime_ns = (long long)json_get_num(rec, "mtime_ns", 0.0);
            out[n].size = (long long)json_get_num(rec, "size", 0.0);
            n++;
        }
    }
    return n;
}

static json_t *rundel_stamps_json(const rundel_stamp *rows, int n) {
    json_t *arr = json_array();
    int i;
    for (i = 0; i < n; i++) {
        json_t *t = json_array();
        char buf[32];
        json_append(t, json_string(rows[i].path));
        snprintf(buf, sizeof(buf), "%lld", rows[i].mtime_ns);
        json_append(t, json_string(buf));
        snprintf(buf, sizeof(buf), "%lld", rows[i].size);
        json_append(t, json_string(buf));
        json_append(arr, t);
    }
    return arr;
}

json_t *w20_rundeliver_c36151_stamp(const json_t *homes, const json_t *stats,
                                    const char *default_home) {
    rundel_stamp rows[RUNDEL_MAX_STAMPS];
    int n = rundel_collect(homes, stats, default_home, rows, RUNDEL_MAX_STAMPS);
    return rundel_stamps_json(rows, n);
}

/* tuple equality over (path, mtime_ns, size). */
static int rundel_same(const json_t *seen, const rundel_stamp *rows, int n) {
    size_t i;
    if (!seen || seen->type != JSON_ARRAY) return 0;   /* None never equals a tuple */
    if ((int)seen->c.count != n) return 0;
    for (i = 0; i < seen->c.count; i++) {
        json_t *t = seen->c.items[i];
        const char *path;
        const char *mtime;
        const char *size;
        char buf[32];
        if (!t || t->type != JSON_ARRAY || t->c.count != 3) return 0;
        path = json_get_str(t->c.items[0], "", NULL);
        mtime = json_get_str(t->c.items[1], "", NULL);
        size = json_get_str(t->c.items[2], "", NULL);
        if (!path || !mtime || !size) return 0;
        if (strcmp(path, rows[i].path)) return 0;
        snprintf(buf, sizeof(buf), "%lld", rows[i].mtime_ns);
        if (strcmp(mtime, buf)) return 0;
        snprintf(buf, sizeof(buf), "%lld", rows[i].size);
        if (strcmp(size, buf)) return 0;
    }
    return 1;
}

json_t *w20_rundeliver_c36151_changed(const json_t *seen, const json_t *homes,
                                       const json_t *stats, const char *default_home) {
    rundel_stamp rows[RUNDEL_MAX_STAMPS];
    json_t *out = json_object();
    int n = rundel_collect(homes, stats, default_home, rows, RUNDEL_MAX_STAMPS);
    int same = rundel_same(seen, rows, n);
    json_set(out, "changed", json_bool(same ? 0 : 1));
    json_set(out, "seen", same && seen ? json_copy(seen) : rundel_stamps_json(rows, n));
    return out;
}

/* --- wait_for_next_tick --------------------------------------------------- */

static void rundel_push_action(json_t *actions, const char *action) {
    json_t *a = json_object();
    json_set(a, "action", json_string(action));
    json_append(actions, a);
}

json_t *w20_rundeliver_c36151_wait_plan(const json_t *in) {
    json_t *out = json_object();
    json_t *actions = json_array();
    json_t *watch, *clock, *stop_results, *changed_flags;
    double interval;
    size_t k = 0, s = 0, c = 0;
    int drains = 0;

    if (in && in->type != JSON_OBJECT) in = NULL;
    interval = in ? json_get_num(in, "interval", 0.0) : 0.0;
    watch = in ? json_obj_get(in, "watch") : NULL;
    clock = in ? json_obj_get(in, "clock") : NULL;
    stop_results = in ? json_obj_get(in, "stop_results") : NULL;
    changed_flags = in ? json_obj_get(in, "changed_flags") : NULL;

    if (!rundel_truthy(watch)) {
        json_t *a = json_object();
        json_set(a, "action", json_string("wait"));
        json_set(a, "timeout", json_number(interval));
        json_append(actions, a);
        a = json_object();
        json_set(a, "action", json_string("return"));
        json_set(a, "why", json_string("no-watch"));
        json_append(actions, a);
        json_set(out, "actions", actions);
        json_set(out, "drains", json_number(0));
        return out;
    }

    rundel_push_action(actions, "refresh");

    {
        double deadline, now;
        if (!clock || clock->type != JSON_ARRAY || clock->c.count == 0) {
            json_set(out, "actions", actions);
            json_set(out, "drains", json_number(0));
            return out;
        }
        deadline = rundel_num(clock->c.items[0]) + interval;
        k = 1;
        for (;;) {
            double remaining, timeout, after;
            int stopped;
            json_t *a;

            if (k >= clock->c.count) {
                a = json_object();
                json_set(a, "action", json_string("return"));
                json_set(a, "why", json_string("clock-exhausted"));
                json_append(actions, a);
                break;
            }
            now = rundel_num(clock->c.items[k++]);
            remaining = deadline - now;
            if (remaining < 0.0) remaining = 0.0;

            stopped = 0;
            if (stop_results && stop_results->type == JSON_ARRAY && s < stop_results->c.count) {
                stopped = rundel_truthy(stop_results->c.items[s++]);
            }
            timeout = remaining < w20_rundeliver_c36151_watch_seconds()
                        ? remaining : w20_rundeliver_c36151_watch_seconds();
            a = json_object();
            json_set(a, "action", json_string("wait"));
            json_set(a, "timeout", json_number(timeout));
            json_append(actions, a);
            if (stopped) {
                a = json_object();
                json_set(a, "action", json_string("return"));
                json_set(a, "why", json_string("stopped"));
                json_append(actions, a);
                break;
            }
            if (changed_flags && changed_flags->type == JSON_ARRAY &&
                c < changed_flags->c.count && rundel_truthy(changed_flags->c.items[c])) {
                a = json_object();
                json_set(a, "action", json_string("drain"));
                json_set(a, "label", json_string(w20_rundeliver_c36151_drain_label()));
                json_append(actions, a);
                drains++;
            }
            c++;

            if (k >= clock->c.count) {
                a = json_object();
                json_set(a, "action", json_string("return"));
                json_set(a, "why", json_string("clock-exhausted"));
                json_append(actions, a);
                break;
            }
            after = rundel_num(clock->c.items[k++]);
            if (after >= deadline) {
                a = json_object();
                json_set(a, "action", json_string("return"));
                json_set(a, "why", json_string("deadline"));
                json_append(actions, a);
                break;
            }
        }
    }

    json_set(out, "actions", actions);
    json_set(out, "drains", json_number((double)drains));
    return out;
}
