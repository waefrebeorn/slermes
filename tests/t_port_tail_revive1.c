/* t_port_tail_revive1.c — differential harness for the tail_revive1 wave.
 *
 * Reads one JSON case per stdin line: {"func": "<c_name>", "in": {...}}.
 * Emits one JSON line per case: {"func", "in", "out", "trace", "st"}:
 *   out   — the port's return (bool/number/array/null) or a raise spec
 *           {"__raise__": cls, "msg": str(exc)} for propagated exceptions.
 *   trace — ordered seam events {"ev", "args", "ret"} (ret is the raw
 *           scripted value; a scripted null records null and returns a
 *           NULL pointer to the port).
 *   st    — post-call state ({} for stateless ports; the probe cache
 *           object for clear_backend_probe_cache).
 * The oracle (tests/oracle_tail_revive1.py) recomputes the live Python
 * reference from the echoed "in" with identical scripted seams.
 *
 * Seam scripts ("in".io): a bare JSON array is a per-call queue; any
 * other value is a constant return; {"__script__": true, "ret": X}
 * scripts a constant X (required for list-valued returns such as the
 * device_pool_view 2-tuple). {"__raise__": cls, "msg": m} scripts an
 * exception. Missing/exhausted scripts fail closed with RuntimeError.
 *
 * Gate (code-only wave; run later from the repo root, explicit objs —
 * the harness #includes every port .c, so libjson is the only object;
 * prompt_builder.c's unrelated functions are dropped by --gc-sections):
 *
 *   source build/winlibs-env.sh
 *   gcc -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h \
 *       -I include/compat -I include -I lib/libjson -I src \
 *       -I C:/winlibs/extra-inc \
 *       -ffunction-sections -fdata-sections -O1 \
 *       tests/t_port_tail_revive1.c lib/libjson/json.o \
 *       -Wl,--gc-sections -o .testbin/t_port_tail_revive1.exe
 *   .testbin/t_port_tail_revive1.exe < tests/fixtures/tail_revive1.jsonl \
 *       | python tests/oracle_tail_revive1.py
 *
 * (or: bash tests/run_one_oracle.sh tail_revive1 tail_revive1 "" \
 *      "lib/libjson/json.o"  — the fixture auto-feeds by name.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* Area-qualified includes: -I src resolves these to the port files. */
#include "agent/context_compressor_gaps.c"
#include "agent/turn_context_gaps.c"
#include "hermes_cli/local_runtime/estimator_gaps.c"
#include "hermes_cli/local_runtime/hardware_gaps.c"
#include "tools/web_result_cache_gaps.c"
#include "gateway/restart_gaps.c"
#include "tools/environments/local_gaps.c"
#include "agent/prompt_builder.c"

/* ---- link seams for prompt_builder.c's unrelated functions ----
 * The author's gate above drops build_skills_system_prompt /
 * build_nous_subscription_prompt with -ffunction-sections + --gc-sections.
 * The runner's default link (run_one_oracle.sh, no --gc-sections) keeps
 * every included function, so the two skills-scan externals they call
 * need definitions. No fixture case drives those two builders, so these
 * stubs are link-only and never execute — behaviour is identical to the
 * gc-sections gate. Signatures match include/hermes_skills.h. */
skill_list_t *skills_scan_all(void) { return NULL; }
void skills_scan_free(skill_list_t *list) { (void)list; }

/* ---- scripted seam recorder (shared by every port) ---- */

typedef struct {
    const json_t *io;      /* borrowed per-case script table */
    json_t *trace;         /* owned ordered event array */
    json_t *counts;        /* owned per-name queue counters */
} t1_ctx;

static json_t *t1_raise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg));
    return o;
}

static json_t *t1_seam(void *vctx, const char *name, const json_t *args)
{
    t1_ctx *c = (t1_ctx *)vctx;
    json_t *script = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *ret = NULL;
    if (!script) {
        char msg[160];
        snprintf(msg, sizeof msg, "unscripted seam: %s", name);
        ret = t1_raise("RuntimeError", msg);
    } else if (script->type == JSON_ARRAY) {
        json_t *cnt = json_obj_get(c->counts, name);
        size_t idx = cnt ? (size_t)cnt->num_val : 0;
        if (idx >= script->c.count) {
            char msg[160];
            snprintf(msg, sizeof msg, "script exhausted: %s", name);
            ret = t1_raise("RuntimeError", msg);
        } else {
            ret = json_copy(script->c.items[idx]);
            json_set(c->counts, name, json_number((double)(idx + 1)));
        }
    } else if (script->type == JSON_OBJECT && json_obj_get(script, "__script__")
               && json_obj_get(script, "__script__")->type == JSON_BOOL
               && json_obj_get(script, "__script__")->bool_val) {
        json_t *r = json_obj_get(script, "ret");
        ret = r ? json_copy(r) : NULL;
    } else {
        ret = json_copy(script);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_array());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(c->trace, ev);
    /* scripted JSON null must reach the port as a NULL pointer */
    if (ret && ret->type == JSON_NULL) {
        json_free(ret);
        return NULL;
    }
    return ret;
}

/* ---- case I/O ---- */

static char *t1_read_line(FILE *f)
{
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 2 > cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = 0;
    return buf;
}

static void t1_emit(const char *func, const json_t *in, json_t *out,
                    json_t *trace, json_t *st)
{
    json_t *env = json_object();
    json_set(env, "func", json_string(func));
    json_set(env, "in", in ? json_copy(in) : json_null());
    json_set(env, "out", out ? out : json_null());
    json_set(env, "trace", trace ? trace : json_array());
    json_set(env, "st", st ? st : json_object());
    char *s = json_serialize(env);
    printf("%s\n", s);
    free(s);
    json_free(env);
}

static void t1_cache_reset(json_t *cache)
{
    for (size_t i = 0; i < cache->c.count; i++) {
        json_free(cache->c.items[i]);
        free(cache->c.keys[i]);
    }
    free(cache->c.items);
    free(cache->c.keys);
    cache->c.items = NULL;
    cache->c.keys = NULL;
    cache->c.count = 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = t1_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) {
            fprintf(stderr, "tail_revive1 harness: bad case line\n");
            return 2;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        t1_ctx ctx = { in ? json_obj_get(in, "io") : NULL,
                       json_array(), json_object() };
        ccg_seams_install(t1_seam, &ctx);
        tcg_seams_install(t1_seam, &ctx);
        hwg_seams_install(t1_seam, &ctx);
        wrcg_seams_install(t1_seam, &ctx);
        lg_seams_install(t1_seam, &ctx);
        json_t *out = NULL;
        json_t *st = json_object();
        if (strcmp(func, "compression_cancelled") == 0) {
            ccg_state_install(json_obj_get(in, "state"));
            out = compression_cancelled();
        } else if (strcmp(func, "is_hygiene_preagent_only_cooldown") == 0) {
            out = json_bool(is_hygiene_preagent_only_cooldown(json_obj_get(in, "error")));
        } else if (strcmp(func, "stale_thinking_on_wire") == 0) {
            ccg_state_install(json_obj_get(in, "state"));
            out = stale_thinking_on_wire();
        } else if (strcmp(func, "billed_basis_over_threshold") == 0) {
            ccg_state_install(json_obj_get(in, "state"));
            out = billed_basis_over_threshold(json_obj_get(in, "current_tokens"));
        } else if (strcmp(func, "agent_stale_thinking_on_wire") == 0) {
            tcg_state_install(json_obj_get(in, "state"));
            out = agent_stale_thinking_on_wire();
        } else if (strcmp(func, "review_fork_first_request_pending") == 0) {
            out = json_bool(review_fork_first_request_pending(json_obj_get(in, "state")));
        } else if (strcmp(func, "ctx_bytes") == 0) {
            long long window = (long long)json_get_num(in, "window", 0);
            bool fa = json_get_bool(in, "flash_attention", true);
            out = json_number((double)ctx_bytes(json_obj_get(in, "profile"), window, fa));
        } else if (strcmp(func, "per_token_kv_f16") == 0) {
            out = json_number((double)per_token_kv_f16(json_obj_get(in, "layers")));
        } else if (strcmp(func, "recurrent_layer_count") == 0) {
            out = json_number((double)recurrent_layer_count(json_obj_get(in, "layers")));
        } else if (strcmp(func, "kv_dtype_factor") == 0) {
            out = json_number(kv_dtype_factor(json_obj_get(in, "flash_attention"),
                                              json_obj_get(in, "kv_q8_ok")));
        } else if (strcmp(func, "unified_pool_bytes") == 0) {
            out = unified_pool_bytes((long long)json_get_num(in, "smi_total", 0),
                                     (long long)json_get_num(in, "ram_total", 0));
        } else if (strcmp(func, "nvidia_vram") == 0) {
            out = nvidia_vram();
        } else if (strcmp(func, "is_cache_exempt_host") == 0) {
            out = is_cache_exempt_host(json_get_str(in, "url", ""));
        } else if (strcmp(func, "host_matches_pattern") == 0) {
            out = json_bool(host_matches_pattern(json_get_str(in, "host", ""),
                                                 json_get_str(in, "pattern", NULL)));
        } else if (strcmp(func, "resolve_cron_drain_budget") == 0) {
            out = json_number(resolve_cron_drain_budget(
                json_obj_get(in, "drain_timeout"),
                json_obj_get(in, "cron_drain_timeout"),
                json_obj_get(in, "watchdog_delay"),
                json_obj_get(in, "elapsed"),
                json_obj_get(in, "cleanup_reserve_s")));
        } else if (strcmp(func, "is_global_startup_conflict") == 0) {
            out = json_bool(is_global_startup_conflict(json_get_str(in, "error_code", NULL)));
        } else if (strcmp(func, "parse_signal_interrupt_grace_timeout") == 0) {
            out = json_number(parse_signal_interrupt_grace_timeout(json_obj_get(in, "raw")));
        } else if (strcmp(func, "buzz_terminal_context_active") == 0) {
            lg_env_install(json_obj_get(in, "env"));
            out = buzz_terminal_context_active();
        } else if (strcmp(func, "is_terminal_first_party_env") == 0) {
            lg_env_install(json_obj_get(in, "env"));
            out = is_terminal_first_party_env(json_get_str(in, "name", ""));
        } else if (strcmp(func, "matches_terminal_first_party_prefix") == 0) {
            out = json_bool(matches_terminal_first_party_prefix(json_get_str(in, "name", "")));
        } else if (strcmp(func, "skill_should_show") == 0) {
            skill_should_show_ctx_install(json_obj_get(in, "conditions"),
                                          json_obj_get(in, "available_tools"),
                                          json_obj_get(in, "available_toolsets"),
                                          json_get_str(in, "session_platform", NULL));
            bool r = skill_should_show();
            json_t *raise = skill_should_show_take_raise();
            out = raise ? raise : json_bool(r);
        } else if (strcmp(func, "clear_backend_probe_cache") == 0) {
            json_t *cache = pb_backend_probe_cache_ref();
            t1_cache_reset(cache);
            json_t *seed = in ? json_obj_get(in, "state") : NULL;
            seed = seed ? json_obj_get(seed, "cache") : NULL;
            if (seed && seed->type == JSON_OBJECT) {
                for (size_t i = 0; i < seed->c.count; i++)
                    json_set(cache, seed->c.keys[i], json_copy(seed->c.items[i]));
            }
            clear_backend_probe_cache();
            out = json_null();
            json_free(st);
            st = json_copy(cache);
        } else {
            fprintf(stderr, "tail_revive1 harness: unknown func %s\n", func);
            json_free(rec);
            json_free(ctx.trace);
            json_free(ctx.counts);
            json_free(st);
            return 2;
        }
        t1_emit(func, in, out, ctx.trace, st);
        json_free(ctx.counts);
        json_free(rec);
    }
    return 0;
}
