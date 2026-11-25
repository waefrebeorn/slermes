/* t_port_scheduler_delivery_flow2.c — harness for scheduler_delivery_flow2.
 * Reads {"func","in"} JSONL on stdin; emits {"func","in","out"} JSONL.
 * NAME-ONLY seam trace; scripted I/O in in.io (name -> result | [results]).
 * Result shape: {"ret":v, "raise":{"type","message",...}, "state":{...}};
 * optional top-level "started"/"result"/"send_retryable" propagate onto a raise.
 */
#include "hermes_json.h"
#include "cron/scheduler_delivery_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { json_t *io; json_t *cursors; } sc_t;

static json_t *script_next(sc_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}
static void apply_state(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}
static json_t *dispatch(sd2_io_t *io, const char *name, json_t *args) {
    (void)args;
    sc_t *sc = (sc_t *)io->ctx;
    json_t *result = script_next(sc, name);
    apply_state(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *rs = json_obj_get(result, "raise");
        if (rs && rs->type == JSON_OBJECT) {
            json_t *exc = json_copy(rs);
            json_set(exc, "__raise__", json_copy(json_obj_get(rs, "type")));
            json_obj_del(exc, "type");
            const char *extra[] = {"started", "result", "send_retryable"};
            for (size_t i = 0; i < 3; i++) {
                json_t *e = json_obj_get(result, extra[i]);
                if (e) json_set(exc, extra[i], json_copy(e));
            }
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}
static json_t *copy_or_obj(json_t *v) { json_t *o = v ? json_copy(v) : NULL; return o ? o : json_object(); }
static json_t *copy_or_null(json_t *v) { return v ? json_copy(v) : json_null(); }

/* echo keys per function (borrowed objects are mutated in place) */
static const char *echo_key(const char *func, int i) {
    static const char *const bot[]   = {"job", NULL};
    static const char *const note[]  = {"errors", NULL};
    static const char *const live[]  = {"target_errors","delivery_errors","unverified_targets","t",NULL};
    static const char *const res[]   = {"metadata","delivery_errors","t",NULL};
    static const char *const media[] = {"delivery_errors","unverified_targets","t",NULL};
    static const char *const prep[]  = {"delivery_errors",NULL};
    static const char *const st[]    = {"target_errors","delivery_errors","t",NULL};
    const char *const *p = NULL;
    if (!strcmp(func,"_deliver_to_bot_chat")) p = bot;
    else if (!strcmp(func,"_note_target_error")) p = note;
    else if (!strcmp(func,"_live_send_text") || !strcmp(func,"_deliver_via_live_adapter")) p = live;
    else if (!strcmp(func,"_resolve_named_dm_topic")) p = res;
    else if (!strcmp(func,"_live_send_media")) p = media;
    else if (!strcmp(func,"_prepare_target_delivery")) p = prep;
    else if (!strcmp(func,"_deliver_standalone")) p = st;
    if (!p) return NULL;
    return p[i];
}

static json_t *run_case(const char *func, json_t *in) {
    sd2_io_t io; sc_t sc;
    json_t *state = json_obj_get(in, "state");
    io.call = dispatch; io.trace = json_array();
    io.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
    io.ctx = &sc; sc.io = json_obj_get(in, "io"); sc.cursors = json_object();
    json_t *ret = NULL;
    json_t *t = NULL;
    json_t *job = NULL, *errors = NULL, *target_errors = NULL, *delivery_errors = NULL;
    json_t *unverified = NULL, *metadata = NULL;

    if (!strcmp(func,"_resolve_cron_surface_mode"))
        ret = sd2_resolve_cron_surface_mode(&io, json_obj_get(in,"pconfig"), json_obj_get(in,"logical_name"));
    else if (!strcmp(func,"_fresh_thread_overrides_origin_topic"))
        ret = sd2_fresh_thread_overrides_origin_topic(&io, json_obj_get(in,"pconfig"));
    else if (!strcmp(func,"_origin_thread_is"))
        ret = sd2_origin_thread_is(&io, json_obj_get(in,"job"), json_obj_get(in,"thread_id"));
    else if (!strcmp(func,"_resolve_origin"))
        ret = sd2_resolve_origin(&io, json_obj_get(in,"job"));
    else if (!strcmp(func,"_redact_cron_payload"))
        ret = sd2_redact_cron_payload(&io, json_obj_get(in,"text"), json_obj_get(in,"what"));
    else if (!strcmp(func,"_cron_display_name"))
        ret = sd2_cron_display_name(&io, json_obj_get(in,"job"));
    else if (!strcmp(func,"_cron_mirror_message"))
        ret = sd2_cron_mirror_message(&io, json_obj_get(in,"job"), json_obj_get(in,"text"));
    else if (!strcmp(func,"_seed_cron_session"))
        ret = sd2_seed_cron_session(&io, json_obj_get(in,"job"), json_obj_get(in,"adapter"),
            json_obj_get(in,"platform_name"), json_obj_get(in,"chat_id"), json_obj_get(in,"text"),
            json_obj_get(in,"thread_id"), json_obj_get(in,"chat_type"), json_obj_get(in,"user_id"),
            json_obj_get(in,"user_name"), json_obj_get(in,"chat_name"), json_obj_get(in,"scope_id"),
            json_get_bool(in,"discord_keys_on_thread", false));
    else if (!strcmp(func,"_home_env_lookup"))
        ret = sd2_home_env_lookup(&io, json_obj_get(in,"env_var"), json_obj_get(in,"suffix"),
                                  json_get_bool(in,"strip", false));
    else if (!strcmp(func,"_relay_fronted_delivery_platforms"))
        ret = sd2_relay_fronted_delivery_platforms(&io, json_obj_get(in,"connected"));
    else if (!strcmp(func,"_home_target"))
        ret = sd2_home_target(&io, json_obj_get(in,"platform_name"), json_obj_get(in,"chat_id"),
                              json_obj_get(in,"resolved_from"));
    else if (!strcmp(func,"_get_standalone_send_timeout"))
        ret = sd2_get_standalone_send_timeout(&io);
    else if (!strcmp(func,"bot_chat_message"))
        ret = sd2_bot_chat_message(&io, json_obj_get(in,"job"), json_obj_get(in,"content"));
    else if (!strcmp(func,"_deliver_to_bot_chat")) {
        job = copy_or_obj(json_obj_get(in,"job"));
        ret = sd2_deliver_to_bot_chat(&io, job, json_obj_get(in,"content"),
                  json_obj_get(in,"profile"), json_get_bool(in,"for_failure", false));
    }
    else if (!strcmp(func,"_resolve_bot_chat_target"))
        ret = sd2_resolve_bot_chat_target(&io, json_obj_get(in,"job"), json_obj_get(in,"profile_arg"));
    else if (!strcmp(func,"_result_field"))
        ret = sd2_result_field(&io, json_obj_get(in,"send_result"), json_obj_get(in,"key"), json_obj_get(in,"default"));
    else if (!strcmp(func,"_note_target_error")) {
        errors = copy_or_obj(json_obj_get(in,"errors"));
        if (errors->type != JSON_ARRAY) { json_free(errors); errors = json_array(); }
        ret = sd2_note_target_error(&io, json_obj_get(in,"job"), json_obj_get(in,"msg"), errors);
    }
    else if (!strcmp(func,"_warn_live_lane_failure"))
        ret = sd2_warn_live_lane_failure(&io, json_obj_get(in,"job"), json_obj_get(in,"msg"),
                                         json_get_bool(in,"is_relay", false));
    else if (!strcmp(func,"_resolve_target_transport"))
        ret = sd2_resolve_target_transport(&io, json_obj_get(in,"job"), json_obj_get(in,"platform"),
                  json_obj_get(in,"platform_name"), json_obj_get(in,"target"),
                  json_obj_get(in,"adapters"), json_obj_get(in,"config"));
    else if (!strcmp(func,"_inchannel_surface_supported"))
        ret = sd2_inchannel_surface_supported(&io, json_obj_get(in,"adapter"), json_obj_get(in,"platform_name"));
    else if (!strcmp(func,"_named_dm_topic"))
        ret = sd2_named_dm_topic(&io, json_obj_get(in,"t"));
    else if (!strcmp(func,"_live_route_metadata"))
        ret = sd2_live_route_metadata(&io, json_obj_get(in,"t"));
    else if (!strcmp(func,"_live_send_text")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        target_errors = copy_or_obj(json_obj_get(in,"target_errors")); if (target_errors->type!=JSON_ARRAY){json_free(target_errors);target_errors=json_array();}
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        unverified = copy_or_obj(json_obj_get(in,"unverified_targets")); if (unverified->type!=JSON_ARRAY){json_free(unverified);unverified=json_array();}
        ret = sd2_live_send_text(&io, t, json_obj_get(in,"text"), json_obj_get(in,"route_thread_id"),
                  json_obj_get(in,"route_metadata"), target_errors, delivery_errors, unverified);
    }
    else if (!strcmp(func,"_resolve_named_dm_topic")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        metadata = copy_or_obj(json_obj_get(in,"metadata"));
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        ret = sd2_resolve_named_dm_topic(&io, t, json_obj_get(in,"topic_name"), metadata, delivery_errors,
                  (long long)json_get_num(in,"n_media",0), json_get_bool(in,"refresh",false));
    }
    else if (!strcmp(func,"_live_send_media")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        unverified = copy_or_obj(json_obj_get(in,"unverified_targets")); if (unverified->type!=JSON_ARRAY){json_free(unverified);unverified=json_array();}
        ret = sd2_live_send_media(&io, t, json_obj_get(in,"media_metadata"), json_obj_get(in,"media_files"),
                  delivery_errors, unverified, json_obj_get(in,"topic_name"));
    }
    else if (!strcmp(func,"_seed_live_delivery_sessions")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        ret = sd2_seed_live_delivery_sessions(&io, t, json_obj_get(in,"delivered_message_id"));
    }
    else if (!strcmp(func,"_deliver_via_live_adapter")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        target_errors = copy_or_obj(json_obj_get(in,"target_errors")); if (target_errors->type!=JSON_ARRAY){json_free(target_errors);target_errors=json_array();}
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        unverified = copy_or_obj(json_obj_get(in,"unverified_targets")); if (unverified->type!=JSON_ARRAY){json_free(unverified);unverified=json_array();}
        ret = sd2_deliver_via_live_adapter(&io, t, json_obj_get(in,"cleaned_text"), json_obj_get(in,"media_files"),
                  target_errors, delivery_errors, unverified);
    }
    else if (!strcmp(func,"_deliver_standalone")) {
        t = copy_or_obj(json_obj_get(in,"t"));
        target_errors = copy_or_obj(json_obj_get(in,"target_errors")); if (target_errors->type!=JSON_ARRAY){json_free(target_errors);target_errors=json_array();}
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        ret = sd2_deliver_standalone(&io, t, json_obj_get(in,"content"), json_obj_get(in,"media_files"),
                  target_errors, delivery_errors);
    }
    else if (!strcmp(func,"_prepare_target_delivery")) {
        delivery_errors = copy_or_obj(json_obj_get(in,"delivery_errors")); if (delivery_errors->type!=JSON_ARRAY){json_free(delivery_errors);delivery_errors=json_array();}
        ret = sd2_prepare_target_delivery(&io, json_obj_get(in,"job"), json_obj_get(in,"target"),
                  json_obj_get(in,"adapters"), json_obj_get(in,"loop"), json_obj_get(in,"config"),
                  json_get_bool(in,"notify_delivery",false), json_get_bool(in,"mirror_enabled",false),
                  json_obj_get(in,"mirror_text"), delivery_errors);
    }
    else if (!strcmp(func,"_unresolved_delivery_outcome"))
        ret = sd2_unresolved_delivery_outcome(&io, json_obj_get(in,"job"), json_get_bool(in,"for_failure",false));
    else if (!strcmp(func,"_should_wrap_cron_response"))
        ret = sd2_should_wrap_cron_response(&io, json_obj_get(in,"job"), json_obj_get(in,"user_cfg"));
    else {
        ret = json_object(); json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", json_copy(io.trace));
    json_set(out, "state", json_copy(io.state));
    for (int i = 0; ; i++) {
        const char *k = echo_key(func, i);
        if (!k) break;
        json_t *val = NULL;
        if (!strcmp(k,"job")) val = job;
        else if (!strcmp(k,"errors")) val = errors;
        else if (!strcmp(k,"target_errors")) val = target_errors;
        else if (!strcmp(k,"delivery_errors")) val = delivery_errors;
        else if (!strcmp(k,"unverified_targets")) val = unverified;
        else if (!strcmp(k,"metadata")) val = metadata;
        else if (!strcmp(k,"t")) val = t;
        if (val) json_set(out, k, json_copy(val));
    }
    json_free(io.trace); json_free(io.state); json_free(sc.cursors);
    if (job) json_free(job); if (errors) json_free(errors);
    if (target_errors) json_free(target_errors); if (delivery_errors) json_free(delivery_errors);
    if (unverified) json_free(unverified); if (metadata) json_free(metadata); if (t) json_free(t);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 16];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = run_case(func, in);
        json_t *lo = json_object();
        json_set(lo, "func", json_string(func));
        json_set(lo, "in", json_copy(in));
        json_set(lo, "out", out);
        char *ser = json_serialize(lo);
        printf("%s\n", ser ? ser : "{}");
        free(ser); json_free(lo); json_free(rec);
    }
    return 0;
}
