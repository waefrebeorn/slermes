/* t_port_browser_launcher_flow2.c — harness for the tools/browser_launcher.py
 * UNCLAIMED-remainder FLOW port (src/tools/browser_launcher_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args...,
 * "state": {browsers,procs,launching}, "io": {seam: script}, "env": {...}}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}. "ret" is the
 * canonical return (tuples/sets -> arrays, None -> null, uncaught exception ->
 * {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call module state
 * ({browsers,procs,launching}, {} on any raise). The oracle drives LIVE
 * tools/browser_launcher.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 * Gate: bash tests/run_one_oracle.sh browser_launcher_flow2 \
 *           oracle_browser_launcher_flow2.py "" "lib/libjson/json.o"
 */
#include "hermes_json.h"
#include "tools/browser_launcher_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fill the LaunchedBrowser default fields so a fixture browser dict projects
 * identically to the oracle's LaunchedBrowser(**d) (record_name=None,
 * foreign=False). Required fields are supplied by the generator. */
static void bl2h_fill_browser(json_t *b) {
    if (!bl2_is_obj(b)) return;
    if (!json_has(b, "record_name")) json_set(b, "record_name", json_null());
    if (!json_has(b, "foreign")) json_set(b, "foreign", json_bool(false));
}
static json_t *bl2h_browser(const json_t *d) {
    if (!bl2_is_obj(d)) return NULL;
    json_t *b = json_copy(d);
    bl2h_fill_browser(b);
    return b;
}
/* materialize the module state object from in.state (owned) */
static json_t *bl2h_state(const json_t *in) {
    json_t *st = json_object();
    const json_t *src = bl2_get(in, "state");
    json_t *browsers = json_object();
    json_t *procs = json_object();
    json_t *launching = json_object();
    const json_t *sb = bl2_get(src, "browsers");
    if (bl2_is_obj(sb))
        for (size_t i = 0; i < sb->c.count; i++) {
            json_t *b = bl2h_browser(sb->c.items[i]);
            json_set(browsers, sb->c.keys[i], b ? b : json_null());
        }
    const json_t *sp = bl2_get(src, "procs");
    if (bl2_is_obj(sp))
        for (size_t i = 0; i < sp->c.count; i++)
            json_set(procs, sp->c.keys[i], json_copy(sp->c.items[i]));
    const json_t *sl = bl2_get(src, "launching");
    if (bl2_is_obj(sl))
        for (size_t i = 0; i < sl->c.count; i++) {
            json_t *b = bl2h_browser(sl->c.items[i]);
            json_set(launching, sl->c.keys[i], b ? b : json_null());
        }
    json_set(st, "browsers", browsers);
    json_set(st, "procs", procs);
    json_set(st, "launching", launching);
    return st;
}

/* Resolve a browser argument: browser_ref aliases the state node (identity),
 * browser is a fresh materialized double. Returns a BORROWED-or-owned node;
 * *owned tells the caller whether to free it. */
static json_t *bl2h_browser_arg(const json_t *in, json_t *state, bool *owned) {
    *owned = false;
    const char *ref = json_get_str(in, "browser_ref", NULL);
    if (ref) {
        json_t *b = bl2_get(bl2_get(state, "browsers"), ref);
        if (b) return b;  /* borrowed alias into state */
    }
    const json_t *d = bl2_get(in, "browser");
    if (bl2_is_obj(d)) { *owned = true; return bl2h_browser(d); }
    return NULL;
}

static json_t *bl2h_run(bl2_ctx *ctx, const char *func, const json_t *in, json_t *state) {
    bool owned = false;
    json_t *browser = bl2h_browser_arg(in, state, &owned);
    json_t *a = bl2h_browser(bl2_get(in, "a"));
    json_t *b = bl2h_browser(bl2_get(in, "b"));
    json_t *record = bl2_get(in, "record");
    if (record) record = json_copy(record);
    json_t *proc = bl2_get(in, "proc");
    if (proc) proc = json_copy(proc);
    json_t *starting = bl2h_browser(bl2_get(in, "starting"));
    json_t *pid = bl2_get(in, "pid");
    json_t *start_time = bl2_get(in, "start_time");
    const char *key = json_get_str(in, "key", "");
    const char *home = json_get_str(in, "home", "");
    const char *udd = json_get_str(in, "user_data_dir", "");
    const char *path = json_get_str(in, "path", "");
    const char *record_path = json_get_str(in, "record_path", "");
    json_t *ret = NULL;

    if (strcmp(func, "_ephemeral_root") == 0) ret = bl2_ephemeral_root(ctx, home);
    else if (strcmp(func, "_records_dir") == 0) ret = bl2_records_dir(ctx, home);
    else if (strcmp(func, "_profile_digest") == 0) ret = bl2_profile_digest(ctx, udd);
    else if (strcmp(func, "_same_profile") == 0) ret = bl2_same_profile(ctx, bl2_get(in, "recorded"), udd);
    else if (strcmp(func, "_devtools_id") == 0) ret = bl2_devtools_id(ctx, browser);
    else if (strcmp(func, "_record_name") == 0) ret = bl2_record_name(ctx, browser);
    else if (strcmp(func, "_shared_profiles_root") == 0) ret = bl2_shared_profiles_root(ctx);
    else if (strcmp(func, "_is_private_dir") == 0) ret = bl2_is_private_dir(ctx, path);
    else if (strcmp(func, "_private_dir") == 0) ret = bl2_private_dir(ctx, path);
    else if (strcmp(func, "_shared_state_dir") == 0) ret = bl2_shared_state_dir(ctx, udd);
    else if (strcmp(func, "_state_dir") == 0) ret = bl2_state_dir(ctx, browser);
    else if (strcmp(func, "_record_path") == 0) ret = bl2_record_path(ctx, browser);
    else if (strcmp(func, "_make_browser") == 0) ret = bl2_make_browser(ctx, in);
    else if (strcmp(func, "_record_is") == 0) ret = bl2_record_is(ctx, record, browser);
    else if (strcmp(func, "_owner_alive") == 0) ret = bl2_owner_alive(ctx, record);
    else if (strcmp(func, "_load_record") == 0) ret = bl2_load_record(ctx, path);
    else if (strcmp(func, "_write_record") == 0) ret = bl2_write_record(ctx, browser);
    else if (strcmp(func, "_remove_record") == 0) ret = bl2_remove_record(ctx, browser);
    else if (strcmp(func, "_adopt_record") == 0) ret = bl2_adopt_record(ctx, browser);
    else if (strcmp(func, "_alive") == 0) ret = bl2_alive(ctx, browser);
    else if (strcmp(func, "_foreign_alive") == 0) ret = bl2_foreign_alive(ctx, browser);
    else if (strcmp(func, "_same_browser") == 0) ret = bl2_same_browser(ctx, a, b);
    else if (strcmp(func, "_heir_key") == 0) ret = bl2_heir_key(ctx, browser);
    else if (strcmp(func, "_tracked_browser") == 0) ret = bl2_tracked_browser(ctx, record);
    else if (strcmp(func, "_terminate") == 0) ret = bl2_terminate(ctx, pid, start_time, proc);
    else if (strcmp(func, "_forget") == 0) ret = bl2_forget(ctx, key, browser, json_get_bool(in, "drop_record", true));
    else if (strcmp(func, "get_browser") == 0) ret = bl2_get_browser(ctx, key);
    else if (strcmp(func, "_last_holder_out") == 0) ret = bl2_last_holder_out(ctx, record_path, browser);
    else if (strcmp(func, "_stop_locked") == 0) ret = bl2_stop_locked(ctx, key);
    else if (strcmp(func, "stop_browser") == 0) ret = bl2_stop_browser(ctx, key);
    else if (strcmp(func, "stop_all_browsers") == 0) ret = bl2_stop_all_browsers(ctx);
    else if (strcmp(func, "_abandon_launch") == 0) ret = bl2_abandon_launch(ctx, proc, starting);
    else if (strcmp(func, "_launch_owned") == 0) ret = bl2_launch_owned(ctx, in);
    else { json_free(browser); json_free(a); json_free(b); json_free(record); json_free(proc); json_free(starting); return NULL; }

    if (owned) json_free(browser);
    json_free(a); json_free(b); json_free(record); json_free(proc); json_free(starting);
    return ret;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        bl2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.state = bl2h_state(in);
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = bl2h_run(&ctx, func, in, ctx.state);
        json_t *ret_out;
        json_t *state_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(bl2_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            json_free(ctx.state);
            state_out = json_object();  /* {} on any raise (both sides) */
        } else {
            ret_out = ret ? ret : json_null();
            state_out = ctx.state;
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state_out);
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}