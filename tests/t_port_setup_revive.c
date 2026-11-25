/* t_port_setup_revive.c — harness for the setup wizard cluster revival
 * (src/hermes_cli/setup_gaps.c).
 *
 * Reads JSON LINES from stdin: {"func": <c_name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","config","nav","noscript"}}.
 *
 * in.config  — the config dict the flow mutates (borrowed copy per case).
 * in.args    — extra positional arguments for the target function.
 * in.io      — seam-name -> scripted result (single value reused, or an
 *              array consumed per call, clamped at the last element).
 *              {"__raise__": kind, "msg": ...} scripts an exception.
 * in.env     — ambient values read without trace events (platform, home,
 *              toolsets, configurable_toolsets, *_import_error flags).
 * in.nav     — initial _SETUP_NAVIGATION state (object or null).
 *
 * The dispatcher records {ev, args, ret} per seam call in order; ret is the
 * script MARKER consumed. "print" and the provider_setup_surface enter/exit
 * events are ambient (no script needed). A seam with no script is flagged in
 * out.noscript (fail-closed: the oracle counts the case as a mismatch).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── API mirrored from src/hermes_cli/setup_gaps.c (linked as an object) ── */
typedef json_t *(*sg_seam_fn)(void *ctx, const char *name, const json_t *args);
typedef struct sg_nav_state {
    int section_index;
    int prompt_index;
    int active_prompt_index;
    json_t *resolved_choices;
    json_t *replay_choices;
} sg_nav_state_t;
typedef struct sg_io {
    sg_seam_fn call;
    void *ctx;
    const json_t *env;
    sg_nav_state_t *nav;
} sg_io_t;

json_t *handle_setup_menu_navigation(sg_io_t *io, const char *event, const json_t *value);
json_t *run_setup_steps(sg_io_t *io, const json_t *steps);
json_t *run_setup_action_with_navigation(sg_io_t *io, const char *label, const char *action,
                                         const char *cancelled_message);
json_t *setup_model_provider(sg_io_t *io, json_t *config);
json_t *setup_tools(sg_io_t *io, json_t *config, int first_install);
json_t *setup_telemetry(sg_io_t *io, json_t *config);
json_t *skip_configured_section(sg_io_t *io, json_t *config, const char *section_key, const char *label);
json_t *run_xai_oauth_login_from_setup(sg_io_t *io);
json_t *setup_telegram_auto_result(sg_io_t *io);
json_t *print_macos_fda_tip(sg_io_t *io);
json_t *blank_slate_minimal_toolsets(sg_io_t *io, json_t *config);
json_t *blank_slate_minimize_config(sg_io_t *io, json_t *config);
json_t *run_portal_one_shot(sg_io_t *io, json_t *config);
json_t *run_first_time_quick_setup(sg_io_t *io, json_t *config, const char *hermes_home,
                                   int is_existing, const char *imported_from);
json_t *run_blank_slate_setup(sg_io_t *io, json_t *config, const char *hermes_home, int is_existing);
json_t *blank_slate_walkthrough(sg_io_t *io, json_t *config, const char *hermes_home);
json_t *setup_tts_provider(sg_io_t *io, json_t *config);

/* ── scripted seam dispatcher ── */
typedef struct {
    const json_t *scripts;
    json_t *cursors;
    json_t *trace;
    json_t *noscript;
} sgr_ctx_t;

static void sgr_record(sgr_ctx_t *sc, const char *name, const json_t *args, const json_t *ret) {
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(sc->trace, ev);
}

static json_t *sgr_dispatch(void *ctx, const char *name, const json_t *args) {
    sgr_ctx_t *sc = (sgr_ctx_t *)ctx;
    if (strcmp(name, "print") == 0) {
        sgr_record(sc, name, NULL, NULL);
        return NULL;
    }
    if (strcmp(name, "provider_setup_surface.enter") == 0) {
        sgr_record(sc, name, args, NULL);
        return NULL;
    }
    if (strcmp(name, "provider_setup_surface.exit") == 0) {
        json_t *f = json_bool(false);
        sgr_record(sc, name, args, f);
        return f;
    }
    const json_t *entry = sc->scripts ? json_obj_get(sc->scripts, name) : NULL;
    if (!entry) {
        json_append(sc->noscript, json_string(name));
        json_t *marker = json_object();
        json_set(marker, "__noscript__", json_string(name));
        sgr_record(sc, name, args, marker);
        json_t *err = json_object();
        json_set(err, "__raise__", json_string("RuntimeError"));
        char *m = (char *)malloc(strlen(name) + 32);
        if (m) { sprintf(m, "no script for seam %s", name); json_set(err, "msg", json_string(m)); free(m); }
        return err;
    }
    const json_t *val = entry;
    if (entry->type == JSON_ARRAY) {
        size_t n = json_len(entry);
        double cur = json_get_num(sc->cursors, name, 0.0);
        size_t idx = (size_t)cur;
        if (n > 0 && idx >= n) idx = n - 1;   /* clamp: last element repeats */
        json_set(sc->cursors, name, json_number(cur + 1.0));
        val = n > 0 ? json_get(entry, idx) : NULL;
    }
    sgr_record(sc, name, args, val);
    if (val && val->type == JSON_OBJECT && json_obj_get(val, "__raise__"))
        return json_copy(val);
    if (!val || val->type == JSON_NULL) return NULL;   /* scripted null == None */
    return json_copy(val);
}

/* ── navigation state ── */
static sg_nav_state_t *sgr_nav_from(const json_t *spec) {
    if (!spec || spec->type != JSON_OBJECT) return NULL;
    sg_nav_state_t *st = (sg_nav_state_t *)calloc(1, sizeof *st);
    if (!st) return NULL;
    st->section_index = (int)json_get_num(spec, "section_index", -1.0);
    st->prompt_index = (int)json_get_num(spec, "prompt_index", 0.0);
    st->active_prompt_index = (int)json_get_num(spec, "active_prompt_index", -1.0);
    const json_t *rc = json_obj_get(spec, "resolved_choices");
    const json_t *rp = json_obj_get(spec, "replay_choices");
    st->resolved_choices = (rc && rc->type == JSON_ARRAY) ? json_copy(rc) : json_array();
    st->replay_choices = (rp && rp->type == JSON_ARRAY) ? json_copy(rp) : json_array();
    return st;
}

static json_t *sgr_nav_json(const sg_nav_state_t *st) {
    if (!st) return json_null();
    json_t *o = json_object();
    json_set(o, "section_index", json_number((double)st->section_index));
    json_set(o, "prompt_index", json_number((double)st->prompt_index));
    json_set(o, "active_prompt_index", json_number((double)st->active_prompt_index));
    json_set(o, "resolved_choices", json_copy(st->resolved_choices));
    json_set(o, "replay_choices", json_copy(st->replay_choices));
    return o;
}

static void sgr_nav_free(sg_nav_state_t *st) {
    if (!st) return;
    json_free(st->resolved_choices);
    json_free(st->replay_choices);
    free(st);
}

/* ── case runner ── */
static json_t *sgr_run_case(const char *func, json_t *in) {
    sgr_ctx_t sc;
    sc.scripts = json_obj_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    sc.noscript = json_array();
    sg_io_t io;
    io.call = sgr_dispatch;
    io.ctx = &sc;
    io.env = json_obj_get(in, "env");
    sg_nav_state_t *nav = sgr_nav_from(json_obj_get(in, "nav"));
    io.nav = nav;
    const json_t *cfg = json_obj_get(in, "config");
    json_t *config = (cfg && cfg->type == JSON_OBJECT) ? json_copy(cfg) : NULL;
    const json_t *args = json_obj_get(in, "args");
    json_t *args_owned = NULL;
    if (!args) { args_owned = json_object(); args = args_owned; }
    json_t *ret = NULL;

    if (strcmp(func, "handle_setup_menu_navigation") == 0) {
        ret = handle_setup_menu_navigation(&io, json_get_str(args, "event", "begin"),
                                           json_obj_get(args, "value"));
    } else if (strcmp(func, "run_setup_steps") == 0) {
        ret = run_setup_steps(&io, json_obj_get(args, "steps"));
    } else if (strcmp(func, "run_setup_action_with_navigation") == 0) {
        ret = run_setup_action_with_navigation(&io, json_get_str(args, "label", "Section"),
                                               json_get_str(args, "action", "act"),
                                               json_get_str(args, "cancelled_message", "Setup cancelled."));
    } else if (strcmp(func, "setup_model_provider") == 0) {
        ret = setup_model_provider(&io, config);
    } else if (strcmp(func, "setup_tools") == 0) {
        ret = setup_tools(&io, config, json_get_bool(args, "first_install", false));
    } else if (strcmp(func, "setup_telemetry") == 0) {
        ret = setup_telemetry(&io, config);
    } else if (strcmp(func, "skip_configured_section") == 0) {
        ret = skip_configured_section(&io, config, json_get_str(args, "section_key", "model"),
                                      json_get_str(args, "label", "Model"));
    } else if (strcmp(func, "run_xai_oauth_login_from_setup") == 0) {
        ret = run_xai_oauth_login_from_setup(&io);
    } else if (strcmp(func, "setup_telegram_auto_result") == 0) {
        ret = setup_telegram_auto_result(&io);
    } else if (strcmp(func, "print_macos_fda_tip") == 0) {
        ret = print_macos_fda_tip(&io);
    } else if (strcmp(func, "blank_slate_minimal_toolsets") == 0) {
        ret = blank_slate_minimal_toolsets(&io, config);
    } else if (strcmp(func, "blank_slate_minimize_config") == 0) {
        ret = blank_slate_minimize_config(&io, config);
    } else if (strcmp(func, "run_portal_one_shot") == 0) {
        ret = run_portal_one_shot(&io, config);
    } else if (strcmp(func, "run_first_time_quick_setup") == 0) {
        ret = run_first_time_quick_setup(&io, config, json_get_str(args, "hermes_home", "/h"),
                                         json_get_bool(args, "is_existing", false),
                                         json_get_str(args, "imported_from", ""));
    } else if (strcmp(func, "run_blank_slate_setup") == 0) {
        ret = run_blank_slate_setup(&io, config, json_get_str(args, "hermes_home", "/h"),
                                    json_get_bool(args, "is_existing", false));
    } else if (strcmp(func, "blank_slate_walkthrough") == 0) {
        ret = blank_slate_walkthrough(&io, config, json_get_str(args, "hermes_home", "/h"));
    } else if (strcmp(func, "setup_tts_provider") == 0) {
        ret = setup_tts_provider(&io, config);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", sc.trace);
    json_set(out, "config", config ? config : json_null());
    json_set(out, "nav", sgr_nav_json(io.nav));
    json_set(out, "noscript", sc.noscript);
    sgr_nav_free(nav);
    json_free(sc.cursors);
    json_free(args_owned);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = (char *)malloc(1 << 20);
    if (!line) return 1;
    while (fgets(line, 1 << 20, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = sgr_run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    free(line);
    return 0;
}
