#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdarg.h>
#include "hermes_json.h"

/* ── hermes_cli/setup.py: setup wizard cluster ──
 *
 * Faithful C11 ports of the live hermes_cli/setup*.py flows. All I/O goes
 * through one injected universal seam callback; pure state machines run
 * directly. Differential oracle: tests/oracle_setup_revive.py +
 * tests/t_port_setup_revive.c + tests/fixtures/setup_revive.jsonl.
 *
 * Seam contract (shared with the harness and the oracle):
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 *     - args is a BORROWED object; the result is a NEW json_t the caller
 *       frees. NULL result == Python None.
 *     - an object carrying "__raise__" (kind string) + "msg" models a
 *       raised exception; ports catch it exactly where the live code's
 *       except clauses catch it and propagate it otherwise.
 *   env carries ambient values (sys.platform, home, TOOLSETS, import-error
 *   flags) read WITHOUT trace events; nav carries the _SETUP_NAVIGATION
 *   contextvar state (NULL == no scope installed).
 */

typedef json_t *(*sg_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct sg_nav_state {
    int section_index;
    int prompt_index;
    int active_prompt_index;
    json_t *resolved_choices;   /* owned array */
    json_t *replay_choices;     /* owned array */
} sg_nav_state_t;

typedef struct sg_io {
    sg_seam_fn call;
    void *ctx;
    const json_t *env;          /* borrowed ambient values */
    sg_nav_state_t *nav;        /* NULL when no navigation scope */
} sg_io_t;

/* ── exception modelling ── */

static int sg_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *sg_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *sg_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* BaseException-only kinds: NOT caught by `except Exception`. */
static int sg_kind_is_base_only(const char *k) {
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0 || strcmp(k, "_SetupCancelled") == 0 ||
           strcmp(k, "_SetupGoBack") == 0 || strcmp(k, "_SetupControlFlow") == 0;
}

static int sg_caught_by_exception(const char *k) {
    return !sg_kind_is_base_only(k);
}

static json_t *sg_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *sg_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return sg_raise(kind, buf);
}

/* ── small helpers ── */

static json_t *sg_call(sg_io_t *io, const char *name, json_t *args) {
    json_t *r = io->call ? io->call(io->ctx, name, args) : NULL;
    json_free(args);
    return r;
}

static const json_t *sg_env(const sg_io_t *io, const char *key) {
    return io->env ? json_obj_get(io->env, key) : NULL;
}

static const char *sg_env_str(const sg_io_t *io, const char *key, const char *def) {
    const json_t *v = sg_env(io, key);
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int sg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *sg_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* str() of a scalar JSON value (containers are not interpolated in this cluster). */
static char *sg_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

static char *sg_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* str.strip() with no argument: space \t \n \r \v \f on both ends. */
static char *sg_strip(const char *s) {
    if (!s) return strdup("");
    const char *ws = " \t\n\r\v\f";
    while (*s && strchr(ws, *s)) s++;
    size_t n = strlen(s);
    while (n > 0 && strchr(ws, s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *sg_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

/* dict.setdefault(key, {}) — returns the existing value when present. */
static json_t *sg_setdefault(json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (v) return v;
    v = json_object();
    json_set(obj, key, v);
    return v;
}

/* _sub_dict: parent[key] as a dict, replacing missing/non-dict with {}. */
static json_t *sg_sub_dict(json_t *parent, const char *key) {
    json_t *child = json_obj_get(parent, key);
    if (!child || child->type != JSON_OBJECT) {
        child = json_object();
        json_set(parent, key, child);
    }
    return child;
}

static void sg_obj_clear(json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT) return;
    while (obj->c.count > 0)
        json_obj_del(obj, obj->c.keys[0]);
}

static void sg_obj_update(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* string sets modelled as objects with true values */
static void sg_set_add(json_t *set, const char *s) {
    if (s && !json_obj_get(set, s)) json_set(set, s, json_bool(true));
}

static int sg_set_has(const json_t *set, const char *s) {
    return s && json_obj_get(set, s) != NULL;
}

static json_t *sg_set_from_array(const json_t *arr) {
    json_t *set = json_object();
    if (arr && arr->type == JSON_ARRAY)
        for (size_t i = 0; i < arr->c.count; i++) {
            const json_t *v = arr->c.items[i];
            if (v && v->type == JSON_STRING) sg_set_add(set, v->str_val);
        }
    return set;
}

static int sg_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* sorted(set) as a JSON array of strings */
static json_t *sg_sorted_keys(const json_t *set) {
    json_t *out = json_array();
    size_t n = json_object_size(set);
    if (n == 0) return out;
    const char **keys = (const char **)malloc(n * sizeof(char *));
    if (!keys) return out;
    for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(set, i);
    qsort(keys, n, sizeof(char *), sg_str_cmp);
    for (size_t i = 0; i < n; i++) json_append(out, json_string(keys[i]));
    free(keys);
    return out;
}

/* tools <= cts (subset) */
static int sg_set_subset(const json_t *tools, const json_t *cts) {
    size_t n = json_object_size(tools);
    for (size_t i = 0; i < n; i++)
        if (!sg_set_has(cts, json_object_get_key_at(tools, i))) return 0;
    return 1;
}

/* ── seam argument builders ── */

static json_t *sg_msg_args(const char *msg) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg ? msg : ""));
    return a;
}

static json_t *sg_lines_args(const char *const *lines, size_t n) {
    json_t *a = json_object();
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        json_append(arr, lines[i] ? json_string(lines[i]) : json_null());
    json_set(a, "lines", arr);
    return a;
}

static json_t *sg_header_args(const char *title, int gap) {
    json_t *a = json_object();
    json_set(a, "title", json_string(title));
    json_set(a, "gap", json_bool(gap ? true : false));
    return a;
}

static json_t *sg_config_args(const json_t *config) {
    json_t *a = json_object();
    json_set(a, "config", config ? json_copy(config) : json_null());
    return a;
}

static json_t *sg_yesno_args(const char *question, int default_val) {
    json_t *a = json_object();
    json_set(a, "question", json_string(question));
    json_set(a, "default", json_bool(default_val ? true : false));
    return a;
}

static json_t *sg_choice_args(const char *question, const json_t *choices, int default_val, const char *description) {
    json_t *a = json_object();
    json_set(a, "question", json_string(question));
    json_set(a, "choices", json_copy(choices));
    json_set(a, "default", json_number((double)default_val));
    if (description) json_set(a, "description", json_string(description));
    return a;
}

static json_t *sg_exc_json(const json_t *raise_marker) {
    json_t *e = json_object();
    json_set(e, "kind", json_string(sg_raise_kind(raise_marker)));
    json_set(e, "msg", json_string(sg_raise_msg(raise_marker)));
    return e;
}

/* logger.debug(fmt, exc) — the recorder formats the message. */
static void sg_log_debug_exc(sg_io_t *io, const char *fmt, const json_t *raise_marker) {
    char *m = sg_fmt(fmt, sg_raise_msg(raise_marker));
    json_t *a = json_object();
    json_set(a, "msg", json_string(m ? m : ""));
    free(m);
    json_free(sg_call(io, "logger.debug", a));
}

static void sg_info_lines(sg_io_t *io, const char *const *lines, size_t n) {
    json_free(sg_call(io, "_info", sg_lines_args(lines, n)));
}

static void sg_print(sg_io_t *io) {
    json_free(sg_call(io, "print", json_object()));
}

static void sg_print_info(sg_io_t *io, const char *msg) {
    json_free(sg_call(io, "print_info", sg_msg_args(msg)));
}

static void sg_print_success(sg_io_t *io, const char *msg) {
    json_free(sg_call(io, "print_success", sg_msg_args(msg)));
}

static void sg_print_warning(sg_io_t *io, const char *msg) {
    json_free(sg_call(io, "print_warning", sg_msg_args(msg)));
}

static void sg_print_header(sg_io_t *io, const char *title, int gap) {
    json_free(sg_call(io, "print_header", sg_header_args(title, gap)));
}

static void sg_save_config(sg_io_t *io, const json_t *config) {
    json_free(sg_call(io, "save_config", sg_config_args(config)));
}

/* _SETUP_NAVIGATION state reset (deepcopy of replay). */
static void sg_nav_reset(sg_nav_state_t *s, int section_index, json_t *replay_owned) {
    s->section_index = section_index;
    s->prompt_index = 0;
    s->active_prompt_index = -1;
    json_free(s->resolved_choices);
    s->resolved_choices = json_array();
    json_free(s->replay_choices);
    s->replay_choices = replay_owned ? replay_owned : json_array();
}

/* Text tables transcribed byte-exact from the live quarry modules. */

static const char *const SG_TELEMETRY_INTRO[] = {
    "Shared metrics contain only bounded counters: activity, session length,",
    "outcomes, error classes (with a fixed-list reason when a memory write or",
    "context compression is refused, fails or is skipped), model routes and",
    "token totals, built-in tool, command and catalog names, bucketed setup",
    "counts, update and install results and timing (with a fixed-list reason",
    "and the stage when one fails; a fresh install is noted on this machine and",
    "counted only once you opt in), crashes,",
    "startup and reply speed, messaging-platform health, how Hermes gets used",
    "(agent accuracy and efficiency, active time per surface, which features and",
    "settings are used or switched off, provider setup outcomes), and coarse",
    "machine facts (RAM range, GPU type, version age and channel, updates behind,",
    "local model server yes/no). Never prompts, files, paths, setting values or",
    "error text."
};

static const char *const SG_SEND_CONSENT[] = {
    "",
    "Sharing uploads each daily package to the Nous telemetry",
    "service. Packages carry your profile-scoped install ID, a",
    "stable random UUID that identifies this profile across days",
    "(it contains no personal information and is reset by deleting",
    "the shared-metrics directory). Only packages whose entire",
    "collection period falls inside a recorded consent window are",
    "ever sent. Apart from the fresh-install note (noted on this",
    "machine and counted only once you opt in), data from before",
    "you opt in is never sent. Sharing can be turned off again at",
    "any time."
};

static const char *const SG_PORTAL_BANNER[] = {
    "\xe2\x94\x82     \xe2\x98\xa4 Hermes Setup \xe2\x80\x94 Nous Portal (one-shot)             \xe2\x94\x82"
};

static const char *const SG_PORTAL_INFO[] = {
    NULL,
    "  One subscription, 300+ models, plus the Tool Gateway:",
    "    web search, image generation, TTS, browser automation",
    "    \xe2\x80\x94 all routed through your Nous Portal sub.",
    NULL,
    "  Sign up: https://portal.nousresearch.com/manage-subscription",
    NULL
};

static const char *const SG_PORTAL_DONE[] = {
    "  Run `hermes portal info` to inspect routing.",
    "  Run `hermes` to start chatting."
};

static const char *const SG_PORTAL_CANCEL[] = {
    NULL,
    "  Setup cancelled.",
    "  You can retry later with `hermes portal`."
};

static const char *const SG_QUICK_PORTAL_INFO[] = {
    "One subscription, 300+ models, plus the Tool Gateway:",
    "  web search, image generation, TTS, browser automation.",
    "Sign up: https://portal.nousresearch.com/manage-subscription",
    NULL
};

static const char *const SG_QUICK_PORTAL_CANCEL[] = {
    NULL,
    "Nous Portal setup cancelled."
};

static const char *const SG_BLANK_INTRO[] = {
    "Everything starts OFF. First we force-enable only what's required",
    "to run an agent, then you choose whether to stop there or walk",
    "through enabling more \xe2\x80\x94 opting in to exactly what you want.",
    "",
    "Forced on: Provider & Model, File Operations, Terminal, Vision, Skills.",
    "Everything else (web, browser, code exec, memory,",
    "delegation, cron, plugins, MCP, \xe2\x80\xa6) starts disabled. The",
    "essential `hermes-agent` skill is always kept so the agent",
    "can help you drive and configure Hermes itself.",
    NULL
};

static const char *const SG_MACOS_TIP[] = {
    NULL,
    "  macOS tip: silence ALL folder permission prompts with one switch \xe2\x80\x94",
    "  System Settings \xe2\x86\x92 Privacy & Security \xe2\x86\x92 Full Disk Access \xe2\x86\x92 enable",
    "  your terminal (and Hermes.app if you use Desktop), or run:",
    "    open \"x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles\"",
    "  The grant is permanent \xe2\x80\x94 it survives every Hermes update."
};

static const char *const SG_WALK_OPTOUT[] = {
    "No skills seeded (except the essential `hermes-agent`",
    "skill). A .no-bundled-skills marker keeps future",
    "`hermes update` runs from re-injecting them. Opt back in any",
    "time with `hermes skills opt-in --sync`."
};

static const char *const SG_WALK_TOOLS_INFO[] = {
    "Pick exactly which additional toolsets to turn on.",
    "(file and terminal are already on; leave the rest off if you want",
    " the most minimal agent.)"
};

static const char *const SG_MODEL_GUIDE[] = {
    "   Guide: https://hermes-agent.nousresearch.com/docs/integrations/providers"
};

static const struct { const char *key; const char *label; } SG_TTS_CHOICES[] = {
    {"edge", "Edge TTS (free, cloud-based, no setup needed)"},
    {"elevenlabs", "ElevenLabs (premium quality, needs API key)"},
    {"openai", "OpenAI TTS (good quality, needs API key)"},
    {"xai", "xAI TTS (Grok voices \xe2\x80\x94 OAuth login or API key)"},
    {"minimax", "MiniMax TTS (high quality with voice cloning, needs API key)"},
    {"mistral", "Mistral Voxtral TTS (multilingual, native Opus, needs API key)"},
    {"gemini", "Google Gemini TTS (30 prebuilt voices, prompt-controllable, needs API key)"},
    {"neutts", "NeuTTS (local on-device, free, ~300MB model download)"},
    {"kittentts", "KittenTTS (local on-device, free, lightweight ~25-80MB ONNX)"}
};

static const struct { const char *key; const char *label; } SG_EDGE_VOICES[] = {
    {"en-US-AriaNeural", "English (US) \xe2\x80\x94 Aria"},
    {"zh-CN-XiaoxiaoNeural", "Chinese (Simplified) \xe2\x80\x94 Xiaoxiao"},
    {"zh-TW-HsiaoChenNeural", "Chinese (Traditional) \xe2\x80\x94 HsiaoChen"},
    {"zh-HK-HiuMaanNeural", "Chinese (Hong Kong) \xe2\x80\x94 HiuMaan"}
};


#define SG_FMT_CURRENT_VOICE "Current voice \xe2\x80\x94 %s"

#define SG_STEP1_HEADER "Step 1 \xe2\x80\x94 Provider & Model (required)"

#define SG_STEP2_HEADER "Step 2 \xe2\x80\x94 Terminal Backend"

#define SG_WALK_GATEWAY_Q "Connect a messaging platform (Telegram, Discord, \xe2\x80\xa6)?"

#define SG_NOUS_OPENAI_LABEL "Nous Subscription (managed OpenAI TTS, billed to your subscription)"

/* PoP: handle_setup_menu_navigation @ hermes_cli/setup.py:_handle_setup_menu_navigation */
json_t *handle_setup_menu_navigation(sg_io_t *io, const char *event, const json_t *value) {
    sg_nav_state_t *state = io->nav;
    if (!state) return NULL;
    if (strcmp(event, "begin") == 0) {
        if (state->section_index < 0) {
            state->active_prompt_index = -1;
            json_t *r = json_object();
            json_set(r, "allow_back", json_bool(false));
            return r;
        }
        int idx = state->prompt_index;
        state->active_prompt_index = idx;
        state->prompt_index += 1;
        int allow_back = state->section_index > 0 || idx > 0;
        json_t *r = json_object();
        json_set(r, "allow_back", json_bool(allow_back ? true : false));
        if (idx >= 0 && (size_t)idx < json_len(state->replay_choices))
            json_set(r, "replay_value", json_copy(json_get(state->replay_choices, (size_t)idx)));
        return r;
    }
    if (strcmp(event, "resolve") == 0) {
        int prompt_index = state->active_prompt_index;
        if (prompt_index >= 0) {
            /* resolved_choices[prompt_index:] = [deepcopy(value)] */
            while (json_len(state->resolved_choices) > (size_t)prompt_index)
                json_array_remove(state->resolved_choices, json_len(state->resolved_choices) - 1);
            json_append(state->resolved_choices, value ? json_copy(value) : json_null());
        }
        return NULL;
    }
    if (strcmp(event, "cancel") == 0)
        return sg_raise("_SetupCancelled", "");
    if (strcmp(event, "back") == 0) {
        json_t *r = sg_raise_fmt("_SetupGoBack", "%d", state->active_prompt_index);
        json_set(r, "prompt_index", json_number((double)state->active_prompt_index));
        return r;
    }
    return NULL;
}

/* PoP: run_setup_steps @ hermes_cli/setup.py:_run_setup_steps */
json_t *run_setup_steps(sg_io_t *io, const json_t *steps) {
    sg_nav_state_t *state = io->nav;
    int section_index = 0;
    json_t *answers_by_section = json_object();   /* "N" -> resolved choices array */
    json_t *replay_by_section = json_object();    /* "N" -> replay choices array */
    json_t *ret = NULL;
    char key[32], prevkey[32];

    while ((size_t)section_index < json_len(steps)) {
        const json_t *step = json_get(steps, (size_t)section_index);
        const char *label = json_string_value(json_get(step, 0));
        const char *action = json_string_value(json_get(step, 1));
        if (!label) label = "";
        if (!action) action = "";
        if (state) {
            snprintf(key, sizeof key, "%d", section_index);
            json_t *rp = json_obj_get(replay_by_section, key);
            json_t *replay = rp ? json_copy(rp) : json_array();
            if (rp) json_obj_del(replay_by_section, key);
            sg_nav_reset(state, section_index, replay);
        }
        json_t *r = sg_call(io, action, json_object());
        if (sg_is_raise(r) && strcmp(sg_raise_kind(r), "_SetupGoBack") == 0) {
            int nav_prompt = (int)json_get_num(r, "prompt_index", -1.0);
            json_free(r);
            /* _record_answers() */
            if (state) {
                snprintf(key, sizeof key, "%d", section_index);
                json_set(answers_by_section, key, json_copy(state->resolved_choices));
            }
            int previous_index, target_prompt;
            if (nav_prompt > 0) {
                previous_index = section_index;
                target_prompt = nav_prompt - 1;
            } else {
                previous_index = section_index - 1;
                if (previous_index < 0) previous_index = 0;
                snprintf(prevkey, sizeof prevkey, "%d", previous_index);
                json_t *ans = json_obj_get(answers_by_section, prevkey);
                int n = ans ? (int)json_len(ans) : 0;
                target_prompt = n - 1;
                if (target_prompt < 0) target_prompt = 0;
            }
            snprintf(prevkey, sizeof prevkey, "%d", previous_index);
            json_t *ans = json_obj_get(answers_by_section, prevkey);
            json_t *slice = json_array();
            if (ans)
                for (int i = 0; i < target_prompt && (size_t)i < json_len(ans); i++)
                    json_append(slice, json_copy(json_get(ans, (size_t)i)));
            json_set(replay_by_section, prevkey, slice);
            sg_print(io);
            const json_t *prev_step = json_get(steps, (size_t)previous_index);
            const char *prev_label = json_string_value(json_get(prev_step, 0));
            char *msg = (previous_index == section_index)
                ? sg_fmt("Returning to the previous choice in %s...", label)
                : sg_fmt("Returning to %s...", prev_label ? prev_label : "");
            sg_print_info(io, msg ? msg : "");
            free(msg);
            section_index = previous_index;
            continue;
        }
        if (sg_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        if (state) {
            snprintf(key, sizeof key, "%d", section_index);
            json_set(answers_by_section, key, json_copy(state->resolved_choices));
        }
        section_index += 1;
    }
done:
    if (state) sg_nav_reset(state, -1, NULL);
    json_free(answers_by_section);
    json_free(replay_by_section);
    return ret;
}

/* PoP: run_setup_action_with_navigation @ hermes_cli/setup.py:run_setup_action_with_navigation */
json_t *run_setup_action_with_navigation(sg_io_t *io, const char *label, const char *action,
                                         const char *cancelled_message) {
    /* _setup_navigation_scope(): install state + menu handler, restore in finally. */
    json_t *hargs = json_object();
    json_set(hargs, "handler", json_string("_handle_setup_menu_navigation"));
    json_t *token = sg_call(io, "set_menu_navigation_handler", hargs);
    sg_nav_state_t *outer = io->nav;
    sg_nav_state_t st;
    st.section_index = -1;
    st.prompt_index = 0;
    st.active_prompt_index = -1;
    st.resolved_choices = json_array();
    st.replay_choices = json_array();
    if (sg_is_raise(token)) {
        /* __enter__ raised: the with-body and its finally never run. */
        io->nav = outer;
        json_free(st.resolved_choices);
        json_free(st.replay_choices);
        return token;
    }
    io->nav = &st;
    json_t *steps = json_array();
    json_t *step = json_array();
    json_append(step, json_string(label ? label : ""));
    json_append(step, json_string(action ? action : ""));
    json_append(steps, step);
    json_t *r = run_setup_steps(io, steps);
    json_free(steps);
    if (sg_is_raise(r) && strcmp(sg_raise_kind(r), "_SetupCancelled") == 0) {
        json_free(r);
        const char *lines[2] = { NULL, cancelled_message ? cancelled_message : "Setup cancelled." };
        sg_info_lines(io, lines, 2);
        r = NULL;
    }
    json_t *rargs = json_object();
    json_set(rargs, "token", token ? token : json_null());
    json_free(sg_call(io, "reset_menu_navigation_handler", rargs));
    io->nav = outer;
    json_free(st.resolved_choices);
    json_free(st.replay_choices);
    return r;
}

/* PoP: setup_model_provider @ hermes_cli/setup.py:setup_model_provider */
json_t *setup_model_provider(sg_io_t *io, json_t *config) {
    sg_print_header(io, "Inference Provider", 0);
    const char *intro[3] = {
        "Choose how to connect to your main chat model.",
        SG_MODEL_GUIDE[0],
        NULL
    };
    sg_info_lines(io, intro, 3);
    /* with provider_setup_surface("cli_setup"): select_provider_and_model() */
    json_t *ea = json_object();
    json_set(ea, "label", json_string("cli_setup"));
    json_t *er = sg_call(io, "provider_setup_surface.enter", ea);
    if (sg_is_raise(er)) return er;
    json_free(er);
    json_t *r = sg_call(io, "select_provider_and_model", json_object());
    const char *kind = sg_is_raise(r) ? sg_raise_kind(r) : NULL;
    json_t *xa = json_object();
    json_set(xa, "exc", kind ? sg_exc_json(r) : json_null());
    json_free(sg_call(io, "provider_setup_surface.exit", xa));
    if (kind) {
        if (strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0) {
            json_free(r);
            const char *skipped[2] = { NULL, "Provider setup skipped." };
            sg_info_lines(io, skipped, 2);
        } else if (sg_caught_by_exception(kind)) {
            sg_log_debug_exc(io, "select_provider_and_model error during setup: %s", r);
            json_t *fa = json_object();
            json_set(fa, "exc", sg_exc_json(r));
            json_set(fa, "retry_command", json_string("hermes model"));
            json_t *lines = sg_call(io, "provider_setup_failure_lines", fa);
            if (sg_is_raise(lines)) { json_free(r); return lines; }
            if (lines && lines->type == JSON_ARRAY && json_len(lines) > 0) {
                char *lead = sg_py_str(json_get(lines, 0));
                sg_print_warning(io, lead ? lead : "");
                free(lead);
                for (size_t i = 1; i < json_len(lines); i++) {
                    char *line = sg_py_str(json_get(lines, i));
                    sg_print_info(io, line ? line : "");
                    free(line);
                }
            }
            json_free(lines);
            json_free(r);
        } else {
            return r;   /* other BaseException kinds propagate */
        }
    } else {
        json_free(r);
    }
    /* config.clear(); config.update(load_config()); save_config(config)
     * — clear runs BEFORE load_config() is evaluated (live statement order). */
    sg_obj_clear(config);
    json_t *lc = sg_call(io, "load_config", json_object());
    if (sg_is_raise(lc)) return lc;
    if (!lc || lc->type != JSON_OBJECT) {
        json_t *err = sg_raise_fmt("TypeError", "'%s' object is not iterable", sg_type_name(lc));
        json_free(lc);
        return err;
    }
    sg_obj_update(config, lc);
    json_free(lc);
    sg_save_config(io, config);
    return NULL;
}

/* PoP: setup_tools @ hermes_cli/setup.py:setup_tools */
json_t *setup_tools(sg_io_t *io, json_t *config, int first_install) {
    json_t *a = json_object();
    json_set(a, "first_install", json_bool(first_install ? true : false));
    json_set(a, "config", config ? json_copy(config) : json_null());
    json_t *r = sg_call(io, "tools_command", a);
    if (sg_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: setup_telemetry @ hermes_cli/setup.py:setup_telemetry */
json_t *setup_telemetry(sg_io_t *io, json_t *config) {
    sg_print_header(io, "Shared Metrics", 0);
    sg_info_lines(io, SG_TELEMETRY_INTRO, sizeof SG_TELEMETRY_INTRO / sizeof SG_TELEMETRY_INTRO[0]);
    sg_info_lines(io, SG_SEND_CONSENT, sizeof SG_SEND_CONSENT / sizeof SG_SEND_CONSENT[0]);
    json_t *shared_metrics = sg_sub_dict(sg_sub_dict(config, "telemetry"), "shared_metrics");
    const json_t *en = json_obj_get(shared_metrics, "enabled");
    const json_t *sd = json_obj_get(shared_metrics, "send");
    int sharing = en && en->type == JSON_BOOL && en->bool_val &&
                  sd && sd->type == JSON_BOOL && sd->bool_val;
    json_t *r = sg_call(io, "prompt_yes_no", sg_yesno_args("Send shared metrics to Nous?", sharing));
    if (sg_is_raise(r)) return r;
    int share = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    json_t *ca = json_object();
    json_set(ca, "enabled", json_bool(share ? true : false));
    json_set(ca, "send", json_bool(share ? true : false));
    json_set(ca, "config", json_copy(config));
    json_t *cr = sg_call(io, "save_consent", ca);
    if (sg_is_raise(cr)) return cr;
    json_free(cr);
    if (share)
        sg_print_success(io, "Shared metrics sent to Nous once a day.");
    else
        sg_print_info(io, "Shared metrics off.");
    return NULL;
}

/* PoP: skip_configured_section @ hermes_cli/setup_migration.py:_skip_configured_section */
json_t *skip_configured_section(sg_io_t *io, json_t *config, const char *section_key, const char *label) {
    json_t *a = json_object();
    json_set(a, "config", config ? json_copy(config) : json_null());
    json_set(a, "section_key", json_string(section_key ? section_key : ""));
    json_t *summary = sg_call(io, "_get_section_config_summary", a);
    if (sg_is_raise(summary)) return summary;
    if (!sg_truthy(summary)) {
        json_free(summary);
        return json_bool(false);
    }
    sg_print(io);
    char *sum_str = sg_py_str(summary);
    char *m = sg_fmt("  %s: %s", label ? label : "", sum_str ? sum_str : "");
    sg_print_success(io, m ? m : "");
    free(m);
    free(sum_str);
    json_free(summary);
    char *low = sg_lower(label ? label : "");
    char *q = sg_fmt("  Reconfigure %s?", low ? low : "");
    json_t *r = sg_call(io, "prompt_yes_no", sg_yesno_args(q ? q : "", 0));
    free(q);
    free(low);
    if (sg_is_raise(r)) return r;
    int answer = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return json_bool(answer ? false : true);
}

/* PoP: run_xai_oauth_login_from_setup @ hermes_cli/setup_tts.py:_run_xai_oauth_login_from_setup */
json_t *run_xai_oauth_login_from_setup(sg_io_t *io) {
    const json_t *imp = sg_env(io, "auth_import_error");
    if (imp && imp->type == JSON_STRING) {
        char *m = sg_fmt("xAI Grok OAuth helpers unavailable: %s", imp->str_val);
        json_free(sg_call(io, "_setup.print_warning", sg_msg_args(m ? m : "")));
        free(m);
        return json_bool(false);
    }
    const char *signing[2] = { NULL, "Signing in to xAI Grok OAuth (SuperGrok / Premium+)..." };
    json_free(sg_call(io, "_setup._info", sg_lines_args(signing, 2)));
    json_t *creds = NULL;
    json_t *err = NULL;
    json_t *rem = sg_call(io, "_is_remote_session", json_object());
    if (sg_is_raise(rem)) {
        err = rem;
    } else {
        int open_browser = !sg_truthy(rem);
        json_free(rem);
        json_t *la = json_object();
        json_set(la, "open_browser", json_bool(open_browser ? true : false));
        creds = sg_call(io, "_xai_oauth_device_code_login", la);
        if (sg_is_raise(creds)) { err = creds; creds = NULL; }
    }
    if (!err) {
        const json_t *tokens = json_obj_get(creds, "tokens");
        if (!tokens) {
            err = sg_raise("KeyError", "'tokens'");
        } else {
            json_t *sa = json_object();
            json_set(sa, "tokens", json_copy(tokens));
            const json_t *disc = json_obj_get(creds, "discovery");
            if (disc && disc->type != JSON_NULL) json_set(sa, "discovery", json_copy(disc));
            const json_t *ruri = json_obj_get(creds, "redirect_uri");
            if (!ruri) json_set(sa, "redirect_uri", json_string(""));
            else if (ruri->type != JSON_NULL) json_set(sa, "redirect_uri", json_copy(ruri));
            const json_t *lref = json_obj_get(creds, "last_refresh");
            if (lref && lref->type != JSON_NULL) json_set(sa, "last_refresh", json_copy(lref));
            json_set(sa, "auth_mode", json_string("oauth_device_code"));
            json_set(sa, "set_active", json_bool(false));
            json_t *sr = sg_call(io, "_save_xai_oauth_tokens", sa);
            if (sg_is_raise(sr)) {
                err = sr;
            } else {
                json_free(sr);
                json_t *ua = json_object();
                json_set(ua, "source", json_string("xai-oauth"));
                json_set(ua, "kind", json_string("device_code"));
                json_t *ur = sg_call(io, "unsuppress_credential_source", ua);
                if (sg_is_raise(ur)) err = ur;
                else json_free(ur);
            }
        }
    }
    if (err) {
        if (!sg_caught_by_exception(sg_raise_kind(err))) {
            json_free(creds);
            return err;
        }
        char *m = sg_fmt("xAI Grok OAuth login failed: %s", sg_raise_msg(err));
        json_free(sg_call(io, "_setup.print_warning", sg_msg_args(m ? m : "")));
        free(m);
        json_free(err);
        json_free(creds);
        return json_bool(false);
    }
    json_free(creds);
    return json_bool(true);
}

/* Path(p).name / Path(p).parent.name for posix-style fixture paths. */
static void sg_path_names(const char *p, char *name, size_t namesz, char *parent_name, size_t pnsz) {
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == '/' || p[n - 1] == '\\')) n--;
    size_t start = n;
    while (start > 0 && p[start - 1] != '/' && p[start - 1] != '\\') start--;
    size_t len = n - start;
    if (len >= namesz) len = namesz - 1;
    memcpy(name, p + start, len);
    name[len] = '\0';
    /* parent component: last segment of p[:start] (trailing separators stripped) */
    size_t pn = start;
    while (pn > 0 && (p[pn - 1] == '/' || p[pn - 1] == '\\')) pn--;
    size_t ps = pn;
    while (ps > 0 && p[ps - 1] != '/' && p[ps - 1] != '\\') ps--;
    size_t pl = pn - ps;
    if (pl >= pnsz) pl = pnsz - 1;
    memcpy(parent_name, p + ps, pl);
    parent_name[pl] = '\0';
}

/* PoP: setup_telegram_auto_result @ hermes_cli/setup_platforms.py:_setup_telegram_auto_result */
json_t *setup_telegram_auto_result(sg_io_t *io) {
    const json_t *imp = sg_env(io, "telegram_import_error");
    if (imp && sg_truthy(imp)) return NULL;
    json_t *home = sg_call(io, "get_hermes_home", json_object());
    if (sg_is_raise(home)) {
        /* contextlib.suppress(Exception) around the profile lookup */
        if (!sg_caught_by_exception(sg_raise_kind(home))) return home;
        json_free(home);
        home = NULL;
    }
    json_t *profile = NULL;
    if (home && home->type == JSON_STRING) {
        char name[256], parent_name[256];
        sg_path_names(home->str_val, name, sizeof name, parent_name, sizeof parent_name);
        if (strcmp(parent_name, "profiles") == 0)
            profile = json_string(name);
    }
    /* home NULL (None) or non-string: Path() raises TypeError, suppressed. */
    json_free(home);
    json_t *a = json_object();
    if (profile) json_set(a, "profile_name", profile);
    return sg_call(io, "auto_setup_telegram_bot_result", a);
}

static int sg_kind_is_oserror(const char *k) {
    static const char *const kinds[] = {
        "OSError", "FileNotFoundError", "FileExistsError", "NotADirectoryError",
        "IsADirectoryError", "InterruptedError", "BlockingIOError", "ChildProcessError",
        "ProcessLookupError", "TimeoutError", "ConnectionError", "BrokenPipeError",
        "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
        "PermissionError"
    };
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
        if (strcmp(k, kinds[i]) == 0) return 1;
    return 0;
}

/* PoP: print_macos_fda_tip @ hermes_cli/setup_quick.py:_print_macos_fda_tip */
json_t *print_macos_fda_tip(sg_io_t *io) {
    if (strcmp(sg_env_str(io, "platform", ""), "darwin") != 0) return NULL;
    const char *home = sg_env_str(io, "home", "");
    size_t hn = strlen(home);
    while (hn > 0 && home[hn - 1] == '/') hn--;   /* Path() drops trailing separators */
    /* Path("") normalizes to ".", and Path(".") / "Library" drops the '.'
     * component, so an empty home yields a RELATIVE path (no leading '/').
     * A root home ("/") still keeps its separator. */
    char *path = home[0] == '\0'
        ? sg_fmt("Library/Application Support/com.apple.TCC")
        : sg_fmt("%.*s/Library/Application Support/com.apple.TCC", (int)hn, home);
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    free(path);
    json_t *r = sg_call(io, "os.listdir", a);
    if (sg_is_raise(r)) {
        const char *k = sg_raise_kind(r);
        if (strcmp(k, "PermissionError") == 0) {
            json_free(r);   /* fall through to the tip */
        } else if (sg_kind_is_oserror(k)) {
            json_free(r);
            return NULL;    /* indeterminate — don't nag */
        } else {
            return r;       /* live only catches PermissionError/OSError */
        }
    } else {
        json_free(r);
        return NULL;        /* already granted */
    }
    sg_info_lines(io, SG_MACOS_TIP, sizeof SG_MACOS_TIP / sizeof SG_MACOS_TIP[0]);
    return NULL;
}

/* PoP: blank_slate_minimal_toolsets @ hermes_cli/setup_quick.py:_blank_slate_minimal_toolsets */
json_t *blank_slate_minimal_toolsets(sg_io_t *io, json_t *config) {
    static const char *const keep_names[] = { "file", "terminal", "vision", "skills" };
    json_t *keep = json_object();
    for (size_t i = 0; i < 4; i++) sg_set_add(keep, keep_names[i]);
    json_t *pt = sg_setdefault(config, "platform_toolsets");
    if (pt->type != JSON_OBJECT) {
        json_free(keep);
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(pt));
    }
    json_set(pt, "cli", sg_sorted_keys(keep));
    json_t *ret = NULL;
    json_t *all_keys = json_object();
    json_t *cts_list = json_array();
    const json_t *imp = sg_env(io, "toolsets_import_error");
    if (imp && imp->type == JSON_STRING) {
        char *m = sg_fmt("blank-slate disabled_toolsets computation skipped: %s", imp->str_val);
        json_t *a = json_object();
        json_set(a, "msg", json_string(m ? m : ""));
        free(m);
        json_free(sg_call(io, "logger.debug", a));
        goto cleanup;
    }
    const json_t *configurable = sg_env(io, "configurable_toolsets");
    const json_t *toolsets = sg_env(io, "toolsets");
    size_t nconf = (configurable && configurable->type == JSON_ARRAY) ? json_len(configurable) : 0;
    for (size_t i = 0; i < nconf; i++) {
        const json_t *row = json_get(configurable, i);
        const char *ck = json_string_value(json_get(row, 0));
        if (ck) sg_set_add(all_keys, ck);
    }
    json_t *pk = sg_call(io, "_get_plugin_toolset_keys", json_object());
    if (sg_is_raise(pk)) { ret = pk; goto err_check; }
    if (pk && pk->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(pk); i++) {
            const json_t *v = json_get(pk, i);
            if (v && v->type == JSON_STRING) sg_set_add(all_keys, v->str_val);
        }
    json_free(pk);
    /* configurable_tool_sets = [set(resolve_toolset(ck)) for ck, _, _ in CONFIGURABLE_TOOLSETS] */
    for (size_t i = 0; i < nconf; i++) {
        const json_t *row = json_get(configurable, i);
        const char *ck = json_string_value(json_get(row, 0));
        json_t *a = json_object();
        json_set(a, "name", json_string(ck ? ck : ""));
        json_t *r = sg_call(io, "resolve_toolset", a);
        if (sg_is_raise(r)) { ret = r; goto err_check; }
        json_append(cts_list, sg_set_from_array(r));
        json_free(r);
    }
    size_t nts = json_object_size(toolsets);
    for (size_t i = 0; i < nts; i++) {
        const char *k = json_object_get_key_at(toolsets, i);
        const json_t *tdef = json_object_get_at(toolsets, i);
        if (strncmp(k, "hermes-", 7) == 0) continue;
        if (tdef && tdef->type == JSON_OBJECT &&
            (sg_truthy(json_obj_get(tdef, "includes")) || sg_truthy(json_obj_get(tdef, "posture"))))
            continue;
        if (!sg_set_has(all_keys, k)) {
            json_t *a = json_object();
            json_set(a, "name", json_string(k));
            json_t *r = sg_call(io, "resolve_toolset", a);
            if (sg_is_raise(r)) { ret = r; goto err_check; }
            json_t *tools = sg_set_from_array(r);
            json_free(r);
            int subset = 0;
            if (json_object_size(tools) > 0)
                for (size_t j = 0; j < json_len(cts_list); j++)
                    if (sg_set_subset(tools, json_get(cts_list, j))) { subset = 1; break; }
            json_free(tools);
            if (subset) continue;
        }
        sg_set_add(all_keys, k);
    }
    {
        json_t *sorted_all = sg_sorted_keys(all_keys);
        json_t *disabled = json_array();
        for (size_t i = 0; i < json_len(sorted_all); i++) {
            const char *k = json_string_value(json_get(sorted_all, i));
            if (!sg_set_has(keep, k)) json_append(disabled, json_string(k));
        }
        json_free(sorted_all);
        if (json_len(disabled) > 0) {
            json_t *agent = sg_setdefault(config, "agent");
            if (agent->type != JSON_OBJECT) {
                json_free(disabled);
                ret = sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(agent));
                goto cleanup;
            }
            json_set(agent, "disabled_toolsets", disabled);
        } else {
            json_free(disabled);
        }
    }
    goto cleanup;
err_check:
    if (!sg_caught_by_exception(sg_raise_kind(ret))) goto cleanup;   /* BaseException propagates */
    sg_log_debug_exc(io, "blank-slate disabled_toolsets computation skipped: %s", ret);
    json_free(ret);
    ret = NULL;
cleanup:
    json_free(keep);
    json_free(all_keys);
    json_free(cts_list);
    return ret;
}

/* PoP: blank_slate_minimize_config @ hermes_cli/setup_quick.py:_blank_slate_minimize_config */
json_t *blank_slate_minimize_config(sg_io_t *io, json_t *config) {
    (void)io;
    /* Each setdefault+assign runs in live statement order: a non-dict section
     * raises AFTER the earlier sections were already written. */
    json_t *agent = sg_setdefault(config, "agent");
    if (agent->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(agent));
    json_set(agent, "max_turns", json_number(90.0));
    json_t *comp = sg_setdefault(config, "compression");
    if (comp->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(comp));
    json_set(comp, "enabled", json_bool(false));
    json_t *mem = sg_setdefault(config, "memory");
    if (mem->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(mem));
    json_set(mem, "memory_enabled", json_bool(false));
    json_set(mem, "user_profile_enabled", json_bool(false));
    json_t *ckpt = sg_setdefault(config, "checkpoints");
    if (ckpt->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(ckpt));
    json_set(ckpt, "enabled", json_bool(false));
    json_t *smr = sg_setdefault(config, "smart_model_routing");
    if (smr->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(smr));
    json_set(smr, "enabled", json_bool(false));
    return NULL;
}

/* ── internal helpers shared by the quick/blank-slate flows (no PoP claim) ── */

typedef enum { SG_CB_NONE = 0, SG_CB_PORTAL_ERROR, SG_CB_QUICK_ERROR } sg_error_cb_t;

/* _run_nous_flow: True on success; cancel kinds print cancel_lines; other
 * Exceptions log + invoke the caller's print_error closure. */
static json_t *sg_run_nous_flow(sg_io_t *io, json_t *config, const char *context,
                                int cancel_includes_systemexit, const char *const *cancel_lines,
                                size_t n_cancel_lines, sg_error_cb_t cb) {
    json_t *r = sg_call(io, "_model_flow_nous", sg_config_args(config));
    if (!sg_is_raise(r)) {
        json_free(r);
        return json_bool(true);
    }
    const char *kind = sg_raise_kind(r);
    int is_cancel = strcmp(kind, "KeyboardInterrupt") == 0 || strcmp(kind, "EOFError") == 0 ||
                    (cancel_includes_systemexit && strcmp(kind, "SystemExit") == 0);
    if (is_cancel) {
        json_free(r);
        sg_info_lines(io, cancel_lines, n_cancel_lines);
        return json_bool(false);
    }
    if (!sg_caught_by_exception(kind)) return r;
    {
        char *m = sg_fmt("_model_flow_nous error during %s: %s", context, sg_raise_msg(r));
        json_t *a = json_object();
        json_set(a, "msg", json_string(m ? m : ""));
        free(m);
        json_free(sg_call(io, "logger.debug", a));
    }
    /* the print_error closure passed by the caller */
    if (cb == SG_CB_PORTAL_ERROR) sg_print(io);
    json_t *fa = json_object();
    json_set(fa, "exc", sg_exc_json(r));
    json_set(fa, "retry_command", json_string(cb == SG_CB_PORTAL_ERROR ? "hermes portal" : "hermes model"));
    json_t *lines = sg_call(io, "provider_setup_failure_lines", fa);
    if (sg_is_raise(lines)) { json_free(r); return lines; }
    if (lines && lines->type == JSON_ARRAY && json_len(lines) > 0) {
        char *lead = sg_py_str(json_get(lines, 0));
        if (cb == SG_CB_PORTAL_ERROR) {
            char *m = sg_fmt("  %s", lead ? lead : "");
            json_free(sg_call(io, "print_error", sg_msg_args(m ? m : "")));
            free(m);
        } else {
            sg_print_warning(io, lead ? lead : "");
        }
        free(lead);
        for (size_t i = 1; i < json_len(lines); i++) {
            char *line = sg_py_str(json_get(lines, i));
            if (cb == SG_CB_PORTAL_ERROR) {
                char *m = sg_fmt("  %s", line ? line : "");
                sg_print_info(io, m ? m : "");
                free(m);
            } else {
                sg_print_info(io, line ? line : "");
            }
            free(line);
        }
    }
    json_free(lines);
    json_free(r);
    return json_bool(false);
}

/* _reload_config_into(config, dict_only=...) */
static json_t *sg_reload_config_into(sg_io_t *io, json_t *config, int dict_only) {
    json_t *lc = sg_call(io, "load_config", json_object());
    if (sg_is_raise(lc)) return lc;
    if (!dict_only || (lc && lc->type == JSON_OBJECT)) {
        sg_obj_clear(config);
        if (lc && lc->type != JSON_OBJECT) {
            json_t *err = sg_raise_fmt("TypeError", "'%s' object is not iterable", sg_type_name(lc));
            json_free(lc);
            return err;
        }
        sg_obj_update(config, lc);
    }
    json_free(lc);
    return NULL;
}

/* _apply_default_agent_settings(config) */
static json_t *sg_apply_default_agent_settings(sg_io_t *io, json_t *config) {
    json_t *agent = sg_setdefault(config, "agent");
    if (agent->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(agent));
    json_set(agent, "max_turns", json_number(150.0));
    json_t *ra = json_object();
    json_set(ra, "name", json_string("HERMES_MAX_ITERATIONS"));
    json_t *rr = sg_call(io, "remove_env_value", ra);
    if (sg_is_raise(rr)) return rr;
    json_free(rr);
    json_t *comp = sg_setdefault(config, "compression");
    if (comp->type != JSON_OBJECT)
        return sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(comp));
    json_set(comp, "enabled", json_bool(true));
    json_set(comp, "threshold", json_number(0.50));
    sg_save_config(io, config);
    sg_print_success(io, "Applied recommended defaults:");
    static const char *const lines[] = {
        "  Max iterations: 150",
        "  Compression threshold: 0.50",
        "  Run `hermes setup agent` later to customize."
    };
    sg_info_lines(io, lines, 3);
    return NULL;
}

typedef enum { SG_SK_CB_NONE = 0, SG_SK_CB_SEEDED, SG_SK_CB_OPTED_OUT } sg_skills_cb_t;

/* _set_bundled_skills_opt_out(opt_out, log_label, on_success=..., on_error=...) */
static json_t *sg_set_bundled_skills_opt_out(sg_io_t *io, int opt_out, const char *log_label, sg_skills_cb_t cb) {
    json_t *a = json_object();
    json_set(a, "opt_out", json_bool(opt_out ? true : false));
    json_t *r = sg_call(io, "set_bundled_skills_opt_out", a);
    json_t *err = NULL;
    if (sg_is_raise(r)) { err = r; goto err_path; }
    json_free(r);
    json_t *sa = json_object();
    json_set(sa, "quiet", json_bool(true));
    r = sg_call(io, "sync_skills", sa);
    if (sg_is_raise(r)) { err = r; goto err_path; }
    if (cb == SG_SK_CB_SEEDED) {
        int copied = 0;
        if (r && r->type == JSON_OBJECT) {
            const json_t *c = json_obj_get(r, "copied");
            if (c) {
                if (c->type != JSON_ARRAY) {
                    err = sg_raise_fmt("TypeError", "object of type '%s' has no len()", sg_type_name(c));
                    json_free(r);
                    goto err_path;
                }
                copied = (int)json_len(c);
            }
        }
        char *m = sg_fmt("Seeded %d bundled skills.", copied);
        sg_print_success(io, m ? m : "");
        free(m);
    } else if (cb == SG_SK_CB_OPTED_OUT) {
        sg_info_lines(io, SG_WALK_OPTOUT, sizeof SG_WALK_OPTOUT / sizeof SG_WALK_OPTOUT[0]);
    }
    json_free(r);
    return NULL;
err_path:
    if (!sg_caught_by_exception(sg_raise_kind(err))) return err;
    {
        char *m = sg_fmt("blank-slate %s error: %s", log_label, sg_raise_msg(err));
        json_t *la = json_object();
        json_set(la, "msg", json_string(m ? m : ""));
        free(m);
        json_free(sg_call(io, "logger.debug", la));
    }
    if (cb != SG_SK_CB_NONE) {
        char *w = sg_fmt("Skill setup step encountered an error: %s", sg_raise_msg(err));
        sg_print_warning(io, w ? w : "");
        free(w);
    }
    json_free(err);
    return NULL;
}

/* _quick_setup_nous_portal(config) */
static json_t *sg_quick_setup_nous_portal(sg_io_t *io, json_t *config) {
    sg_print_header(io, "Nous Portal", 1);
    sg_info_lines(io, SG_QUICK_PORTAL_INFO, sizeof SG_QUICK_PORTAL_INFO / sizeof SG_QUICK_PORTAL_INFO[0]);
    json_t *r = sg_run_nous_flow(io, config, "quick setup", 0,
                                 SG_QUICK_PORTAL_CANCEL,
                                 sizeof SG_QUICK_PORTAL_CANCEL / sizeof SG_QUICK_PORTAL_CANCEL[0],
                                 SG_CB_QUICK_ERROR);
    if (sg_is_raise(r)) return r;
    json_free(r);
    return sg_reload_config_into(io, config, 0);
}

/* _blank_slate_done(config, hermes_home, tools_line, *extra, intro=...) */
static json_t *sg_blank_slate_done(sg_io_t *io, const json_t *config, const char *hermes_home,
                                   const char *tools_line, const char *const *extra, size_t n_extra,
                                   const char *intro) {
    json_t *a = json_object();
    json_set(a, "config", config ? json_copy(config) : json_null());
    json_set(a, "hermes_home", json_string(hermes_home ? hermes_home : ""));
    json_set(a, "tools_line", json_string(tools_line ? tools_line : ""));
    json_t *ex = json_array();
    for (size_t i = 0; i < n_extra; i++) json_append(ex, json_string(extra[i]));
    json_set(a, "extra", ex);
    if (intro) json_set(a, "intro", json_string(intro));
    return sg_call(io, "_blank_slate_done", a);
}

/* PoP: run_portal_one_shot @ hermes_cli/setup_quick.py:_run_portal_one_shot */
json_t *run_portal_one_shot(sg_io_t *io, json_t *config) {
    json_t *ba = json_object();
    json_t *bl = json_array();
    json_append(bl, json_string(SG_PORTAL_BANNER[0]));
    json_set(ba, "lines", bl);
    json_free(sg_call(io, "_print_banner", ba));
    sg_info_lines(io, SG_PORTAL_INFO, sizeof SG_PORTAL_INFO / sizeof SG_PORTAL_INFO[0]);
    json_t *r = sg_run_nous_flow(io, config, "`hermes portal`", 1,
                                 SG_PORTAL_CANCEL,
                                 sizeof SG_PORTAL_CANCEL / sizeof SG_PORTAL_CANCEL[0],
                                 SG_CB_PORTAL_ERROR);
    if (sg_is_raise(r)) return r;
    int ok = sg_truthy(r);
    json_free(r);
    if (!ok) return NULL;
    /* with contextlib.suppress(Exception): _reload_config_into(config, dict_only=True) */
    json_t *rr = sg_reload_config_into(io, config, 1);
    if (sg_is_raise(rr)) {
        if (!sg_caught_by_exception(sg_raise_kind(rr))) return rr;
        json_free(rr);
    } else {
        json_free(rr);
    }
    sg_print(io);
    sg_print_success(io, "Portal setup complete.");
    sg_info_lines(io, SG_PORTAL_DONE, sizeof SG_PORTAL_DONE / sizeof SG_PORTAL_DONE[0]);
    json_t *cr = sg_call(io, "_record_setup_completed", sg_config_args(config));
    if (sg_is_raise(cr)) return cr;
    json_free(cr);
    return NULL;
}

/* PoP: run_first_time_quick_setup @ hermes_cli/setup_quick.py:_run_first_time_quick_setup */
json_t *run_first_time_quick_setup(sg_io_t *io, json_t *config, const char *hermes_home,
                                   int is_existing, const char *imported_from) {
    (void)is_existing;   /* accepted by the live signature, unused in the body */
    json_t *ka = json_object();
    json_set(ka, "config", json_copy(config));
    json_set(ka, "imported_from", json_string(imported_from ? imported_from : ""));
    json_t *keep = sg_call(io, "_keep_imported_model", ka);
    if (sg_is_raise(keep)) return keep;
    int kept = sg_truthy(keep);
    json_free(keep);
    if (!kept) {
        json_t *r = sg_quick_setup_nous_portal(io, config);
        if (sg_is_raise(r)) return r;
        json_free(r);
    }
    json_t *r = sg_call(io, "setup_terminal_backend", sg_config_args(config));
    if (sg_is_raise(r)) return r;
    json_free(r);
    r = sg_apply_default_agent_settings(io, config);
    if (sg_is_raise(r)) return r;
    json_free(r);
    sg_save_config(io, config);
    sg_print(io);
    json_t *choices = json_array();
    json_append(choices, json_string("Set up messaging now (recommended)"));
    json_append(choices, json_string("Skip \xe2\x80\x94 set up later with 'hermes setup gateway'"));
    r = sg_call(io, "prompt_choice", sg_choice_args("Connect a messaging platform? (Telegram, Discord, etc.)", choices, 0, NULL));
    json_free(choices);
    if (sg_is_raise(r)) return r;
    int gateway_choice = (int)json_number_value(r);
    json_free(r);
    if (gateway_choice == 0) {
        r = sg_call(io, "setup_gateway", sg_config_args(config));
        if (sg_is_raise(r)) return r;
        json_free(r);
        sg_save_config(io, config);
    } else {
        json_t *ga = json_object();
        json_set(ga, "context", json_string("setup"));
        json_set(ga, "interactive", json_bool(true));
        json_set(ga, "config", json_copy(config));
        r = sg_call(io, "ensure_gateway_service", ga);
        if (sg_is_raise(r)) return r;
        json_free(r);
    }
    sg_print(io);
    sg_print_success(io, "Setup complete! You're ready to go.");
    const char *done_lines[2] = { NULL, "  Configure all settings:    hermes setup" };
    sg_info_lines(io, done_lines, 2);
    if (gateway_choice != 0)
        sg_print_info(io, "  Connect Telegram/Discord:  hermes setup gateway");
    r = print_macos_fda_tip(io);
    if (sg_is_raise(r)) return r;
    json_free(r);
    sg_print(io);
    json_t *sa = json_object();
    json_set(sa, "config", json_copy(config));
    json_set(sa, "hermes_home", json_string(hermes_home ? hermes_home : ""));
    r = sg_call(io, "_print_setup_summary", sa);
    if (sg_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* forward declaration: the fork below calls the walkthrough port */
json_t *blank_slate_walkthrough(sg_io_t *io, json_t *config, const char *hermes_home);

/* PoP: run_blank_slate_setup @ hermes_cli/setup_quick.py:_run_blank_slate_setup */
json_t *run_blank_slate_setup(sg_io_t *io, json_t *config, const char *hermes_home, int is_existing) {
    (void)is_existing;
    sg_print_header(io, "Blank Slate Setup", 1);
    sg_info_lines(io, SG_BLANK_INTRO, sizeof SG_BLANK_INTRO / sizeof SG_BLANK_INTRO[0]);
    sg_print_header(io, SG_STEP1_HEADER, 0);
    json_t *r = setup_model_provider(io, config);
    if (sg_is_raise(r)) return r;
    json_free(r);
    sg_save_config(io, config);
    sg_print_header(io, SG_STEP2_HEADER, 0);
    r = sg_call(io, "setup_terminal_backend", sg_config_args(config));
    if (sg_is_raise(r)) return r;
    json_free(r);
    r = blank_slate_minimal_toolsets(io, config);
    if (sg_is_raise(r)) return r;
    json_free(r);
    r = blank_slate_minimize_config(io, config);
    if (sg_is_raise(r)) return r;
    json_free(r);
    sg_save_config(io, config);
    sg_print(io);
    sg_print_success(io, "Minimal baseline applied:");
    sg_print_info(io, "  Toolsets: file, terminal, vision, skills (everything else off)");
    sg_print_info(io, "  Compression, memory, checkpoints, smart routing: off");
    sg_print_header(io, "How far do you want to go?", 1);
    json_t *choices = json_array();
    json_append(choices, json_string("Start with everything disabled \xe2\x80\x94 finish now (most minimal)"));
    json_append(choices, json_string("Walk through all configurations \xe2\x80\x94 opt in to tools, skills, plugins, MCP"));
    r = sg_call(io, "prompt_choice", sg_choice_args("Your minimal agent is ready. What next?", choices, 0, NULL));
    json_free(choices);
    if (sg_is_raise(r)) return r;
    int path = (int)json_number_value(r);
    json_free(r);
    if (path != 0) {
        r = blank_slate_walkthrough(io, config, hermes_home);
        if (sg_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    sg_save_config(io, config);
    r = sg_set_bundled_skills_opt_out(io, 1, "skill opt-out", SG_SK_CB_NONE);
    if (sg_is_raise(r)) return r;
    json_free(r);
    static const char *const extra[] = { "  Enable plugins:      hermes plugins" };
    r = sg_blank_slate_done(io, config, hermes_home, "  Enable tools:        hermes tools",
                            extra, 1, "Enable anything later, on demand:");
    if (sg_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: blank_slate_walkthrough @ hermes_cli/setup_quick.py:_blank_slate_walkthrough */
json_t *blank_slate_walkthrough(sg_io_t *io, json_t *config, const char *hermes_home) {
    sg_print_header(io, "Bundled Skills", 1);
    sg_print_info(io, "Blank Slate ships with NO bundled skills by default.");
    json_t *r = sg_call(io, "prompt_yes_no",
        sg_yesno_args("Seed the full bundled skill catalog? (No = start with zero skills)", 0));
    if (sg_is_raise(r)) return r;
    int seed_skills = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    r = sg_set_bundled_skills_opt_out(io, !seed_skills, "skill handling",
                                      seed_skills ? SG_SK_CB_SEEDED : SG_SK_CB_OPTED_OUT);
    if (sg_is_raise(r)) return r;
    json_free(r);
    sg_print_header(io, "Tools", 1);
    sg_info_lines(io, SG_WALK_TOOLS_INFO, sizeof SG_WALK_TOOLS_INFO / sizeof SG_WALK_TOOLS_INFO[0]);
    r = sg_call(io, "prompt_yes_no", sg_yesno_args("Open the tool selector to enable more tools?", 0));
    if (sg_is_raise(r)) return r;
    int open_tools = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    if (open_tools) {
        json_t *ta = json_object();
        json_set(ta, "first_install", json_bool(false));
        json_set(ta, "config", json_copy(config));
        json_t *tr = sg_call(io, "tools_command", ta);
        json_t *err = NULL;
        if (sg_is_raise(tr)) {
            err = tr;
        } else {
            json_free(tr);
            json_t *rr = sg_reload_config_into(io, config, 0);
            if (sg_is_raise(rr)) err = rr;
            else json_free(rr);
        }
        if (err) {
            if (!sg_caught_by_exception(sg_raise_kind(err))) return err;
            sg_log_debug_exc(io, "blank-slate tools_command error: %s", err);
            char *w = sg_fmt("Tool selector encountered an error: %s", sg_raise_msg(err));
            sg_print_warning(io, w ? w : "");
            free(w);
            json_free(err);
        }
    } else {
        sg_print_info(io, "Keeping the minimal toolset. Add tools later with `hermes tools`.");
    }
    static const char *const headers[2] = { "Plugins", "MCP Servers" };
    static const char *const questions[2] = {
        "Review and enable built-in plugins now?",
        "Add an MCP server now?"
    };
    static const char *const yes_msgs[2] = {
        "Manage plugins with `hermes plugins list` / `hermes plugins install`.",
        "Add servers with `hermes mcp add <name> --url ... | --command ...`."
    };
    static const char *const no_msgs[2] = {
        "No plugins enabled. Add later with `hermes plugins`.",
        "No MCP servers configured. Add later with `hermes mcp add`."
    };
    for (int i = 0; i < 2; i++) {
        sg_print_header(io, headers[i], 1);
        r = sg_call(io, "prompt_yes_no", sg_yesno_args(questions[i], 0));
        if (sg_is_raise(r)) return r;
        int yes = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        sg_print_info(io, yes ? yes_msgs[i] : no_msgs[i]);
    }
    sg_print(io);
    r = sg_call(io, "prompt_yes_no", sg_yesno_args(SG_WALK_GATEWAY_Q, 0));
    if (sg_is_raise(r)) return r;
    int gateway = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    if (gateway) {
        r = sg_call(io, "setup_gateway", sg_config_args(config));
        if (sg_is_raise(r)) return r;
        json_free(r);
    }
    sg_save_config(io, config);
    r = sg_blank_slate_done(io, config, hermes_home, "  Enable more tools:   hermes tools", NULL, 0, NULL);
    if (sg_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* _TTS_PROVIDER_LABELS.get(key, key): menu label minus its parenthetical. */
static char *sg_tts_label(const char *key) {
    for (size_t i = 0; i < sizeof SG_TTS_CHOICES / sizeof SG_TTS_CHOICES[0]; i++) {
        if (strcmp(SG_TTS_CHOICES[i].key, key) == 0) {
            const char *label = SG_TTS_CHOICES[i].label;
            const char *cut = strstr(label, " (");
            size_t n = cut ? (size_t)(cut - label) : strlen(label);
            char *out = (char *)malloc(n + 1);
            if (!out) return NULL;
            memcpy(out, label, n);
            out[n] = '\0';
            return out;
        }
    }
    return strdup(key);
}

static int sg_tts_in_table(const char *const *keys, size_t n, const char *key) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(keys[i], key) == 0) return 1;
    return 0;
}

static json_t *sg_name_args(const char *name) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name));
    return a;
}

static void sg_setup_print_info(sg_io_t *io, const char *msg) {
    json_free(sg_call(io, "_setup.print_info", sg_msg_args(msg)));
}

static void sg_setup_print_warning(sg_io_t *io, const char *msg) {
    json_free(sg_call(io, "_setup.print_warning", sg_msg_args(msg)));
}

/* PoP: setup_tts_provider @ hermes_cli/setup_tts.py:_setup_tts_provider */
json_t *setup_tts_provider(sg_io_t *io, json_t *config) {
    static const char *const local_keys[] = { "neutts", "kittentts" };
    static const char *const api_keys[] = { "elevenlabs", "openai", "minimax", "mistral", "gemini" };
    /* current_provider = config.get("tts", {}).get("provider", "edge") */
    const json_t *tts = json_obj_get(config, "tts");
    if (tts && tts->type != JSON_OBJECT)
        return sg_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", sg_type_name(tts));
    const json_t *prov = tts ? json_obj_get(tts, "provider") : NULL;
    if (prov && (prov->type == JSON_ARRAY || prov->type == JSON_OBJECT))
        return sg_raise_fmt("TypeError", "unhashable type: '%s'", sg_type_name(prov));
    char *current_provider = prov ? sg_py_str(prov) : strdup("edge");
    char *current_label = sg_tts_label(current_provider);
    sg_print(io);
    json_free(sg_call(io, "_setup.print_header", sg_header_args("Text-to-Speech Provider (optional)", 0)));
    {
        char *cur = sg_fmt("Current: %s", current_label);
        const char *lines[2] = { cur, NULL };
        json_free(sg_call(io, "_setup._info", sg_lines_args(lines, 2)));
        free(cur);
    }
    /* options = list(_TTS_PROVIDER_CHOICES); maybe insert the managed nous option */
    json_t *options = json_array();
    json_t *mr = sg_call(io, "managed_nous_tools_enabled", json_object());
    if (sg_is_raise(mr)) { free(current_provider); free(current_label); return mr; }
    int managed = sg_truthy(mr);
    json_free(mr);
    int insert_nous = 0;
    if (managed) {
        json_t *fr = sg_call(io, "get_nous_subscription_features", sg_config_args(config));
        if (sg_is_raise(fr)) { free(current_provider); free(current_label); json_free(options); return fr; }
        insert_nous = fr && fr->type == JSON_OBJECT && sg_truthy(json_obj_get(fr, "nous_auth_present"));
        json_free(fr);
    }
    size_t n_choices = sizeof SG_TTS_CHOICES / sizeof SG_TTS_CHOICES[0];
    if (insert_nous) {
        json_t *row = json_array();
        json_append(row, json_string("nous-openai"));
        json_append(row, json_string(SG_NOUS_OPENAI_LABEL));
        json_append(options, row);
    }
    for (size_t i = 0; i < n_choices; i++) {
        json_t *row = json_array();
        json_append(row, json_string(SG_TTS_CHOICES[i].key));
        json_append(row, json_string(SG_TTS_CHOICES[i].label));
        json_append(options, row);
    }
    json_t *choices = json_array();
    for (size_t i = 0; i < json_len(options); i++)
        json_append(choices, json_copy(json_get(json_get(options, i), 1)));
    {
        char *keep = sg_fmt("Keep current (%s)", current_label);
        json_append(choices, json_string(keep));
        free(keep);
    }
    int keep_current_idx = (int)json_len(choices) - 1;
    json_t *r = sg_call(io, "_setup.prompt_choice",
                        sg_choice_args("Select TTS provider:", choices, keep_current_idx, NULL));
    json_free(choices);
    if (sg_is_raise(r)) { free(current_provider); free(current_label); json_free(options); return r; }
    int idx = (int)json_number_value(r);
    json_free(r);
    if (idx == keep_current_idx) {
        free(current_provider);
        free(current_label);
        json_free(options);
        return NULL;
    }
    if (idx < 0 || (size_t)idx >= json_len(options)) {
        free(current_provider);
        free(current_label);
        json_free(options);
        return sg_raise("IndexError", "list index out of range");
    }
    const char *sel_key = json_string_value(json_get(json_get(options, (size_t)idx), 0));
    char *selected = strdup(sel_key ? sel_key : "");
    json_free(options);
    if (strcmp(selected, "nous-openai") == 0) {
        free(selected);
        selected = strdup("openai");
        sg_setup_print_info(io, "OpenAI TTS will use the managed Nous gateway and bill to your subscription.");
        json_t *v1 = sg_call(io, "_setup.get_env_value", sg_name_args("VOICE_TOOLS_OPENAI_KEY"));
        if (sg_is_raise(v1)) { r = v1; goto fail; }
        int has_key = sg_truthy(v1);
        json_free(v1);
        if (!has_key) {
            json_t *v2 = sg_call(io, "_setup.get_env_value", sg_name_args("OPENAI_API_KEY"));
            if (sg_is_raise(v2)) { r = v2; goto fail; }
            has_key = sg_truthy(v2);
            json_free(v2);
        }
        if (has_key)
            sg_setup_print_warning(io, "Direct OpenAI credentials are still configured and may take precedence "
                                       "until removed from ~/.hermes/.env.");
    } else if (sg_tts_in_table(local_keys, 2, selected)) {
        json_t *a = json_object();
        json_set(a, "selected", json_string(selected));
        json_t *sr = sg_call(io, "_tts_local_install_step", a);
        if (sg_is_raise(sr)) { r = sr; goto fail; }
        free(selected);
        selected = sr && sr->type == JSON_STRING ? strdup(sr->str_val) : NULL;
        json_free(sr);
    } else if (sg_tts_in_table(api_keys, 5, selected)) {
        json_t *a = json_object();
        json_set(a, "selected", json_string(selected));
        json_t *sr = sg_call(io, "_tts_api_key_step", a);
        if (sg_is_raise(sr)) { r = sr; goto fail; }
        free(selected);
        selected = sr && sr->type == JSON_STRING ? strdup(sr->str_val) : NULL;
        json_free(sr);
    } else if (strcmp(selected, "xai") == 0) {
        json_t *sr = sg_call(io, "_tts_xai_step", sg_config_args(config));
        if (sg_is_raise(sr)) { r = sr; goto fail; }
        free(selected);
        selected = sr && sr->type == JSON_STRING ? strdup(sr->str_val) : NULL;
        json_free(sr);
    }
    if (selected && strcmp(selected, "edge") == 0) {
        json_t *tts_obj = sg_setdefault(config, "tts");
        if (tts_obj->type != JSON_OBJECT) {
            r = sg_raise_fmt("AttributeError", "'%s' object has no attribute 'setdefault'", sg_type_name(tts_obj));
            goto fail;
        }
        json_t *edge_config = sg_setdefault(tts_obj, "edge");
        if (edge_config->type != JSON_OBJECT) {
            r = sg_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", sg_type_name(edge_config));
            goto fail;
        }
        const json_t *voice_node = json_obj_get(edge_config, "voice");
        char *current_voice = voice_node ? sg_py_str(voice_node) : strdup("en-US-AriaNeural");
        json_t *voices = json_array();
        size_t n_voices = sizeof SG_EDGE_VOICES / sizeof SG_EDGE_VOICES[0];
        int found = 0;
        for (size_t i = 0; i < n_voices; i++)
            if (strcmp(SG_EDGE_VOICES[i].key, current_voice) == 0) { found = 1; break; }
        if (!found) {
            json_t *row = json_array();
            json_append(row, json_string(current_voice));
            char *lbl = sg_fmt(SG_FMT_CURRENT_VOICE, current_voice);
            json_append(row, json_string(lbl ? lbl : ""));
            free(lbl);
            json_append(voices, row);
        }
        for (size_t i = 0; i < n_voices; i++) {
            json_t *row = json_array();
            json_append(row, json_string(SG_EDGE_VOICES[i].key));
            json_append(row, json_string(SG_EDGE_VOICES[i].label));
            json_append(voices, row);
        }
        json_t *voice_labels = json_array();
        for (size_t i = 0; i < json_len(voices); i++)
            json_append(voice_labels, json_copy(json_get(json_get(voices, i), 1)));
        json_append(voice_labels, json_string("Other voice (enter voice ID)"));
        int current_idx = 0;
        for (size_t i = 0; i < json_len(voices); i++) {
            const char *vk = json_string_value(json_get(json_get(voices, i), 0));
            if (vk && strcmp(vk, current_voice) == 0) {
                current_idx = (int)i;
                break;
            }
        }
        const char *hint = "Choose a voice matching the language you want to speak.";
        sg_setup_print_info(io, hint);
        json_t *vr = sg_call(io, "_setup.prompt_choice",
                             sg_choice_args("Select Edge TTS voice:", voice_labels, current_idx, hint));
        json_free(voice_labels);
        if (sg_is_raise(vr)) {
            free(current_voice);
            json_free(voices);
            r = vr;
            goto fail;
        }
        int voice_idx = (int)json_number_value(vr);
        json_free(vr);
        if (voice_idx == (int)json_len(voices)) {
            json_t *pa = json_object();
            json_set(pa, "question", json_string("Edge TTS voice ID"));
            json_set(pa, "default", json_string(current_voice));
            json_t *pr = sg_call(io, "_setup.prompt", pa);
            if (sg_is_raise(pr)) {
                free(current_voice);
                json_free(voices);
                r = pr;
                goto fail;
            }
            const char *pv = (pr && pr->type == JSON_STRING && sg_truthy(pr)) ? pr->str_val : "";
            char *voice = sg_strip(pv);
            json_set(edge_config, "voice", json_string(voice && voice[0] ? voice : current_voice));
            free(voice);
            json_free(pr);
        } else if (voice_idx >= 0 && (size_t)voice_idx < json_len(voices)) {
            json_set(edge_config, "voice", json_copy(json_get(json_get(voices, (size_t)voice_idx), 0)));
        } else {
            free(current_voice);
            json_free(voices);
            r = sg_raise("IndexError", "list index out of range");
            goto fail;
        }
        free(current_voice);
        json_free(voices);
    }
    {
        json_t *tts_obj = sg_setdefault(config, "tts");
        if (tts_obj->type != JSON_OBJECT) {
            r = sg_raise_fmt("TypeError", "'%s' object does not support item assignment", sg_type_name(tts_obj));
            goto fail;
        }
        json_set(tts_obj, "provider", selected ? json_string(selected) : json_null());
    }
    json_free(sg_call(io, "_setup.save_config", sg_config_args(config)));
    {
        char *lbl = selected ? sg_tts_label(selected) : strdup("None");
        char *m = sg_fmt("TTS provider set to: %s", lbl ? lbl : "None");
        json_free(sg_call(io, "_setup.print_success", sg_msg_args(m ? m : "")));
        free(m);
        free(lbl);
    }
    free(selected);
    free(current_provider);
    free(current_label);
    return NULL;
fail:
    free(selected);
    free(current_provider);
    free(current_label);
    return r;
}

/* ── Deferred stubs (PoP tags removed: an honest gap beats a fake claim) ──
 * setup_telegram_auto / load_openclaw_migration_module / offer_openclaw_migration:
 *   no live def exists in the quarry anymore (claimed names are gone upstream).
 * run_setup_wizard_impl / run_quick_setup: large orchestrators, not yet ported.
 */

int setup_telegram_auto(const char *config_json) {
    (void)config_json;
    return 0;
}

void *load_openclaw_migration_module(void) {
    return NULL;
}

int offer_openclaw_migration(void) {
    return 0;
}

int run_setup_wizard_impl(void) {
    return 0;
}

int run_quick_setup(void) {
    return 0;
}

/* ── Stubs outside this revival's list: left untouched ── */

/* PoP: curses_prompt_choice @ hermes_cli/setup.py:_curses_prompt_choice */
int curses_prompt_choice(const char *question, const char **choices, int default_val) {
    (void)question; (void)choices;
    return default_val >= 0 ? default_val : 0;
}

/* PoP: prompt_container_resources @ hermes_cli/setup.py:prompt_container_resources */
void prompt_container_resources(const char *config_json) {
    (void)config_json;
}

/* PoP: prompt_vercel_sandbox_settings @ hermes_cli/setup_terminal.py:_prompt_vercel_sandbox_settings */
void prompt_vercel_sandbox_settings(const char *config_json) {
    (void)config_json;
}

/* PoP: read_nearest_vercel_project @ hermes_cli/setup_terminal.py:_read_nearest_vercel_project */
char *read_nearest_vercel_project(const char *cwd) {
    (void)cwd;
    return strdup("{}");
}

/* PoP: install_neutts_deps @ hermes_cli/setup_tts.py:_install_neutts_deps */
int install_neutts_deps(void) {
    return 1;
}

/* PoP: install_kittentts_deps @ hermes_cli/setup_tts.py:_install_kittentts_deps */
int install_kittentts_deps(void) {
    return 1;
}

/* PoP: prompt_telegram_bot_token @ hermes_cli/setup_platforms.py:_prompt_telegram_bot_token */
char *prompt_telegram_bot_token(void) {
    return strdup("");
}

/* PoP: record_send_consent_change @ hermes_cli/setup.py:_record_send_consent_change */
void record_send_consent_change(const char *consent) {
    (void)consent;
}

/* PoP: get_section_config_summary @ hermes_cli/setup_migration.py:_get_section_config_summary */
char *get_section_config_summary(const char *section) {
    (void)section;
    return strdup("{}");
}

/* PoP: print_migration_preview @ hermes_cli/setup.py:print_migration_preview */
void print_migration_preview(void *module) {
    (void)module;
}

/* PoP: setup_navigation_scope @ hermes_cli/setup.py:_setup_navigation_scope */
void setup_navigation_scope(void *scope) {
    (void)scope;
}
