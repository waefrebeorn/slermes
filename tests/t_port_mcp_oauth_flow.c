/* t_port_mcp_oauth_flow.c — harness for the tools/mcp_oauth.py FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"[,"data","result"]}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated module globals /
 * storage attributes; "data"/"result" the mutated dict for the mutator flows.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise"} or a queue
 * LIST of those), applied by the dispatcher exactly as the oracle's recording
 * stubs are fed. A scripted raise becomes {"__raise__": type, "message": ...};
 * KeyError messages are repr(key) on both sides. Object scripts MUST wrap their
 * return in "ret" (a bare object is read as a script envelope, not a dict ret).
 */
#include "hermes_json.h"
#include "mcp_oauth_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
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

static void apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *dispatch(mof_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *result = script_next(sc, name);
    apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_t *ty = json_obj_get(raise, "type");
            json_set(exc, "__raise__", ty ? json_copy(ty) : json_string("Exception"));
            const char *kind = ty && ty->type == JSON_STRING ? ty->str_val : "Exception";
            json_t *key = json_obj_get(raise, "key");
            if (strcmp(kind, "KeyError") == 0 && key) {
                /* str(KeyError(k)) is repr(k) */
                char *rr = mof_py_repr(key);
                json_set(exc, "message", json_string(rr ? rr : "''"));
                free(rr);
            } else {
                json_t *msg = json_obj_get(raise, "message");
                json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            }
            json_t *en = json_obj_get(raise, "errno");
            if (en) json_set(exc, "errno", json_copy(en));
            json_t *errs = json_obj_get(raise, "errors");
            if (errs) json_set(exc, "errors", json_copy(errs));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    if (result) return json_copy(result);
    return json_null();
}

static void io_init(mof_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static void ensure_obj(json_t *state, const char *key) {
    json_t *v = json_obj_get(state, key);
    if (!v || v->type != JSON_OBJECT) json_set(state, key, json_object());
}

static json_t *copy_or_obj(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

static json_t *make_out(mof_io_t *io, json_t *ret, json_t *data, json_t *result) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    if (data) json_set(out, "data", json_copy(data));
    if (result) json_set(out, "result", json_copy(result));
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    mof_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *data_out = NULL;
    json_t *result_out = NULL;
    const char *platform = json_get_str(in, "platform", "posix");

    if (strcmp(func, "refresh_lock_path") == 0) {
        ret = mof_refresh_lock_path(json_get_str(in, "path", ""));
    } else if (strcmp(func, "acquire_refresh_fence") == 0) {
        ret = mof_acquire_refresh_fence(&io, json_get_str(in, "path", ""),
            json_get_num(in, "timeout", MO_REFRESH_FENCE_TIMEOUT_SECONDS), platform);
    } else if (strcmp(func, "release_refresh_fence") == 0) {
        ret = mof_release_refresh_fence(&io, (long)json_get_num(in, "fd", 0), platform);
    } else if (strcmp(func, "sdk_class") == 0) {
        ensure_obj(state, "_SDK_CLASSES");
        if (!json_has(state, "_OAUTH_AVAILABLE")) json_set(state, "_OAUTH_AVAILABLE", json_bool(true));
        ret = mof_sdk_class(&io, json_get_str(in, "name", ""));
    } else if (strcmp(func, "bind_reserved") == 0) {
        ensure_obj(state, "_reserved_sockets");
        ret = mof_bind_reserved(&io, (long)json_get_num(in, "port", 0));
    } else if (strcmp(func, "cached_client_info") == 0) {
        ret = mof_cached_client_info(&io, json_obj_get(in, "storage"));
    } else if (strcmp(func, "cached_redirect") == 0) {
        ret = mof_cached_redirect(&io, json_obj_get(in, "storage"));
    } else if (strcmp(func, "load_model") == 0) {
        ret = mof_load_model(&io, json_get_str(in, "path", ""),
            json_get_str(in, "sdk_name", "OAuthToken"), json_get_str(in, "label", "tokens"),
            json_get_bool(in, "fixup", false));
    } else if (strcmp(func, "rebase_expires_in") == 0) {
        json_t *data = copy_or_obj(in, "data");
        ret = mof_rebase_expires_in(&io, data);
        data_out = data;
    } else if (strcmp(func, "fixup_loaded_tokens") == 0) {
        json_t *data = copy_or_obj(in, "data");
        ret = mof_fixup_loaded_tokens(&io, data);
        data_out = data;
    } else if (strcmp(func, "stamp_issuer") == 0) {
        ret = mof_stamp_issuer(&io, json_get_str(in, "issuer", ""));
    } else if (strcmp(func, "strip_refresh_token") == 0) {
        ret = mof_strip_refresh_token(&io);
    } else if (strcmp(func, "coerce_secret_auth_method") == 0) {
        json_t *data = copy_or_obj(in, "data");
        ret = mof_coerce_secret_auth_method(data);
        data_out = data;
    } else if (strcmp(func, "result_taken") == 0) {
        ret = mof_result_taken(json_obj_get(in, "result"));
    } else if (strcmp(func, "claim_result") == 0) {
        json_t *result = copy_or_obj(in, "result");
        ret = mof_claim_result(&io, result, json_obj_get(in, "values"));
        result_out = result;
    } else if (strcmp(func, "start_callback_server") == 0) {
        ensure_obj(state, "_reserved_sockets");
        ret = mof_start_callback_server(&io, (long)json_get_num(in, "port", 0),
            json_get_str(in, "handler", "H"));
    } else if (strcmp(func, "callback_outcome") == 0) {
        ret = mof_callback_outcome(&io, json_obj_get(in, "result"),
            json_get_str(in, "cimd_url", NULL));
    } else if (strcmp(func, "login_connect_timeout") == 0) {
        ret = mof_login_connect_timeout(json_obj_get(in, "config"));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret, data_out, result_out);
    if (data_out) json_free(data_out);
    if (result_out) json_free(result_out);
    json_free(sc.cursors);
    json_free(io.trace);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = run_case(func, in);
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
    return 0;
}
