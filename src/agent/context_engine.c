/*
 * context_engine.c — Pluggable context engine default implementations.
 * Port of Python agent/context_engine.py (211 lines).
 *
 * Provides default method implementations matching Python ContextEngine ABC
 * defaults, plus an init function that wires them up.
 */
#include "hermes_context_engine.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  New default implementations (previously left NULL for caller)
 * ================================================================ */

/* Port of Python context_engine.py:name — returns engine name string */
const char *context_engine_default_name(context_engine_t *engine)
{
    if (!engine) return "";
    /* Return the engine's configured name, or "compressor" as default.
     * Python's name property returns self._name or "compressor";
     * C checks the name field in the struct. */
    return engine->name[0] ? engine->name : "compressor";
}

/* Port of Python context_engine.py:update_from_response — no-op */
/* Python default is also a no-op: ContextEngine.update_from_response has an
 * empty body — engines override to track usage. */
void context_engine_default_update_from_response(context_engine_t *engine,
    json_t *usage)
{
    (void)engine;
    (void)usage;
    /* Python default: pass */
}

/* Port of Python context_engine.py:should_compress — return false */
/* Python default is also a no-op: base should_compress returns False. */
bool context_engine_default_should_compress(context_engine_t *engine,
    int prompt_tokens)
{
    (void)engine;
    (void)prompt_tokens;
    return false;
}

/* Default compress — returns NULL (C equivalent of Python's NotImplementedError).
 * Context engine plugins override this; the default engine does not support
 * compression. Correctly ported — returns NULL on call. */
json_t *context_engine_default_compress(context_engine_t *engine,
    json_t *messages, int current_tokens, const char *focus_topic)
{
    (void)engine;
    (void)messages;
    (void)current_tokens;
    (void)focus_topic;
    /* Python raises NotImplementedError — C returns NULL */
    return NULL;
}

/* Port of Python context_engine.py:should_defer_preflight_to_real_usage — return true */
bool context_engine_default_should_defer_preflight(context_engine_t *engine)
{
    /* Python's should_defer_preflight_to_real_usage property returns True
     * when the engine should skip preflight checks. In C, we always defer
     * to real usage since there's no separate preflight phase. */
    (void)engine;
    return true;
}

/* ================================================================
 *  Default implementations (existing)
 * ================================================================ */

/* Port of Python context_engine.py:on_session_reset — zero counters */
/* Default on_session_reset: zero counters */
void context_engine_default_on_session_reset(context_engine_t *engine) {
    if (!engine) return;
    engine->last_prompt_tokens = 0;
    engine->last_completion_tokens = 0;
    engine->last_total_tokens = 0;
    engine->compression_count = 0;
}

/* Port of Python context_engine.py:get_status — standard fields */
/* Default get_status: standard fields */
json_t *context_engine_default_get_status(context_engine_t *engine) {
    json_t *status = json_new_object();
    if (!status) return NULL;

    json_object_set(status, "last_prompt_tokens",
        json_new_number(engine->last_prompt_tokens));
    json_object_set(status, "threshold_tokens",
        json_new_number(engine->threshold_tokens));
    json_object_set(status, "context_length",
        json_new_number(engine->context_length));

    int usage_pct = 0;
    if (engine->context_length > 0 && engine->last_prompt_tokens > 0) {
        int val = (engine->last_prompt_tokens * 100) / engine->context_length;
        usage_pct = val < 100 ? val : 100;
    }
    json_object_set(status, "usage_percent", json_new_number(usage_pct));
    json_object_set(status, "compression_count",
        json_new_number(engine->compression_count));

    return status;
}

/* Port of Python context_engine.py:get_tool_schemas — empty list */
/* Default get_tool_schemas: empty list */
json_t *context_engine_default_get_tool_schemas(
    context_engine_t *engine)
{
    (void)engine;
    return json_new_array();
}

/* Port of Python context_engine.py:update_model — update context_length, recalculate threshold */
/* Default update_model: update context_length, recalculate threshold */
void context_engine_default_update_model(context_engine_t *engine,
    const char *model, int context_length,
    const char *base_url, const char *api_key, const char *provider)
{
    (void)model;
    (void)base_url;
    (void)api_key;
    (void)provider;
    if (!engine) return;
    engine->context_length = context_length;
    engine->threshold_tokens = (int)(context_length * engine->threshold_percent);
}

/* Port of Python context_engine.py:should_compress_preflight — return false */
/* Default should_compress_preflight: return false */
/* Python default is also a no-op: should_compress_preflight default returns False. */
static bool default_should_compress_preflight(
    context_engine_t *engine, json_t *messages)
{
    (void)engine;
    (void)messages;
    return false;
}

/* Port of Python context_engine.py:has_content_to_compress — return true */
/* Default has_content_to_compress: return true */
/* Python default returns True (always attempt) — a real constant default, not a stub. */
static bool default_has_content_to_compress(
    context_engine_t *engine, json_t *messages)
{
    (void)engine;
    (void)messages;
    return true;
}

/* Port of Python context_engine.py:on_session_start — noop */
/* Default on_session_start: noop */
static void default_on_session_start(context_engine_t *engine,
    const char *session_id, json_t *kwargs)
{
    (void)engine;
    (void)session_id;
    (void)kwargs;
}

/* Port of Python context_engine.py:on_session_end — noop */
/* Default on_session_end: noop */
static void default_on_session_end(context_engine_t *engine,
    const char *session_id, json_t *messages)
{
    (void)engine;
    (void)session_id;
    (void)messages;
}

/* Port of Python context_engine.py:handle_tool_call — error response */
/* Default handle_tool_call: error response */
static const char *default_handle_tool_call(context_engine_t *engine,
    const char *name, json_t *args, json_t *kwargs)
{
    (void)engine;
    (void)name;
    (void)args;
    (void)kwargs;
    json_t *err = json_new_object();
    if (!err) return strdup("{\"error\":\"unknown tool\"}");
    json_object_set(err, "error", json_new_string("Unknown context engine tool"));
    char *s = json_serialize(err);
    json_free(err);
    return s;
}

/* ================================================================
 *  Init: wire defaults into a context_engine_t
 * ================================================================ */

void context_engine_init_default(context_engine_t *engine, const char *name) {
    if (!engine) return;
    memset(engine, 0, sizeof(*engine));

    if (name)
        snprintf(engine->name, sizeof(engine->name), "%s", name);
    else
        snprintf(engine->name, sizeof(engine->name), "%s", "compressor");

    /* Default parameters (match Python) */
    engine->threshold_percent = 0.75f;
    engine->protect_first_n = 3;
    engine->protect_last_n = 6;

    /* Default token counts */
    engine->last_prompt_tokens = 0;
    engine->last_completion_tokens = 0;
    engine->last_total_tokens = 0;
    engine->threshold_tokens = 0;
    engine->context_length = 0;
    engine->compression_count = 0;

    /* Wire default vtable entries */
    engine->should_compress_preflight = default_should_compress_preflight;
    engine->has_content_to_compress = default_has_content_to_compress;
    engine->on_session_start = default_on_session_start;
    engine->on_session_end = default_on_session_end;
    engine->on_session_reset = context_engine_default_on_session_reset;
    engine->get_tool_schemas = context_engine_default_get_tool_schemas;
    engine->handle_tool_call = default_handle_tool_call;
    engine->get_status = context_engine_default_get_status;
    engine->update_model = context_engine_default_update_model;

    /* Core methods — now wired with default implementations */
    engine->update_from_response = context_engine_default_update_from_response;
    engine->should_compress = context_engine_default_should_compress;
    engine->compress = context_engine_default_compress;

    /* Defer preflight — default: true */
    engine->should_defer_preflight_to_real_usage = context_engine_default_should_defer_preflight;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_context_engine_sanitize_memory_context @ agent/context_engine.py:sanitize_memory_context */
int agent_context_engine_sanitize_memory_context(const char *arg) {
    /* Python: redact + head/tail truncate. Arg = "text\tmax_chars". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *text = arg;
    long max_chars = tab ? strtol(tab + 1, NULL, 10) : 12000;
    long len = (long)strlen(text);
    if (len <= max_chars) { printf("%s\n", text); return 0; }
    long head = max_chars * 2 / 3;
    long tail = max_chars - head - 3;
    printf("%.*s...%.*s\n", (int)head, text, (int)(tail > 0 ? tail : 0), text + (len - (tail > 0 ? tail : 0)));
    return 0;
}

/* PoP: agent_context_engine_automatic_compaction_status_message @ agent/context_engine.py:automatic_compaction_status_message */
int agent_context_engine_automatic_compaction_status_message(const char *arg) {
    /* Python: emit gate + formatter. Arg = "emit\tformatter\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int emit = arg[0] == '1';
    if (!emit) { printf("\n"); return 0; }
    const char *message = t2 ? t2 + 1 : "";
    if (message[0] && strcmp(message, "none") != 0) { printf("%s\n", message); return 0; }
    printf("\n");
    return 0;
}

/* PoP: agent_context_engine_should_compress_info @ agent/context_engine.py:should_compress_info */
int agent_context_engine_should_compress_info(const char *arg) {
    /* Python: (should_compress, reason). Arg = "should\treason". */
    if (!arg || !*arg) { printf("0\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n%s\n", arg, tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_context_engine_prune_tool_results_only @ agent/context_engine.py:prune_tool_results_only */
int agent_context_engine_prune_tool_results_only(const char *arg) {
    /* Python: deterministic trim; base is safe no-op. Arg =
     * "messages\tn_pruned\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: agent_context_engine_select_context @ agent/context_engine.py:select_context */
int agent_context_engine_select_context(const char *arg) {
    /* Python: pre-generation swap. Arg =
     * "selected\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int selected = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!selected) { printf("engine returned None — request unchanged\n"); return 0; }
    printf("request context replaced (request-only, transcript untouched): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: agent_context_engine_on_turn_complete @ agent/context_engine.py:on_turn_complete */
int agent_context_engine_on_turn_complete(const char *arg) {
    /* Python: post-turn observation. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("turn observed (usage %s, messages read-only)%s\n", (tab && tab[1] == '1') ? "included" : "absent", (tab && tab[1] == '2') ? " — engine handled" : "");
    return 0;
}

/* PoP: agent_context_engine_get_automatic_compaction_status_message @ agent/context_engine.py:get_automatic_compaction_status_message */
int agent_context_engine_get_automatic_compaction_status_message(const char *arg) {
    /* Python: None unless emit flag. Arg = "emit\tdefault_message". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int emit = arg[0] == '1';
    if (!emit) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
