/* t_port_mcp_oauth_flow2.c — harness for the tools/mcp_oauth.py unclaimed-remainder
 * FLOW port (src/tools/mcp_oauth_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state","prints","writes"}}.
 * "trace" is the ordered seam-name sequence; "state" the mutated storage attrs;
 * "prints" the ordered print(text, file=stderr) captures; "writes" the ordered
 * wfile.write payloads. Scripted I/O lives in in.io (seam-name -> {"ret","state",
 * "raise"} or a queue LIST of those), applied by the dispatcher exactly as the
 * oracle's recording stubs are fed. A scripted raise becomes {"__raise__": type,
 * "message": ...}; KeyError messages are repr(key) on both sides. Object scripts
 * MUST wrap their return in "ret" (a bare object is read as a script envelope).
 */
#include "hermes_json.h"
#include "tools/mcp_oauth_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
    json_t *prints;
    json_t *writes;
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

static json_t *dispatch(mo2_io_t *io, const char *name, json_t *args) {
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    /* capture print / wfile.write payloads before the seam frees args */
    if (strcmp(name, "print") == 0) {
        json_t *cap = json_object();
        json_set(cap, "text", json_copy(mo2_get(args, "text")));
        json_set(cap, "file", json_string("stderr"));
        json_append(sc->prints, cap);
    } else if (strcmp(name, "wfile.write") == 0) {
        json_append(sc->writes, json_copy(mo2_get(args, "data")));
    }
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
                char *rr = mo2_py_repr(key);
                json_set(exc, "message", json_string(rr ? rr : "''"));
                free(rr);
            } else {
                json_t *msg = json_obj_get(raise, "message");
                json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            }
            json_t *en = json_obj_get(raise, "errno");
            if (en) json_set(exc, "errno", json_copy(en));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    if (result) return json_copy(result);
    return json_null();
}

static void io_init(mo2_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
    sc->prints = json_array();
    sc->writes = json_array();
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

static json_t *make_out(mo2_io_t *io, script_ctx_t *sc, json_t *ret) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    json_set(out, "prints", json_copy(sc->prints));
    json_set(out, "writes", json_copy(sc->writes));
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    mo2_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;

    if (strcmp(func, "stdin_is_console") == 0) {
        ret = mo2_stdin_is_console(&io, in);
    } else if (strcmp(func, "model_json") == 0) {
        ret = mo2_model_json(&io, json_obj_get(in, "model"));
    } else if (strcmp(func, "path") == 0) {
        ret = mo2_path(&io, json_get_str(in, "suffix", ".json"));
    } else if (strcmp(func, "state_paths") == 0) {
        ret = mo2_state_paths(&io);
    } else if (strcmp(func, "bind_issuer") == 0) {
        ret = mo2_bind_issuer(&io, json_obj_get(in, "issuer"));
    } else if (strcmp(func, "credential_stamp") == 0) {
        ret = mo2_credential_stamp(&io);
    } else if (strcmp(func, "parse_redirect_query") == 0) {
        ret = mo2_parse_redirect_query(json_get_str(in, "query", ""));
    } else if (strcmp(func, "do_get") == 0) {
        ret = mo2_do_get(&io, json_get_str(in, "path", ""));
    } else if (strcmp(func, "log_request") == 0) {
        ret = mo2_log_request(&io, json_get_str(in, "path", ""), json_obj_get(in, "command"));
    } else if (strcmp(func, "log_message") == 0) {
        ret = mo2_log_message(&io, json_get_str(in, "fmt", ""), json_obj_get(in, "args"));
    } else if (strcmp(func, "announce_authorization_url") == 0) {
        ret = mo2_announce_authorization_url(&io,
            json_get_str(in, "authorization_url", ""),
            json_obj_get(in, "port"), json_obj_get(in, "redirect_uri"),
            json_obj_get(in, "redirect_host"));
    } else if (strcmp(func, "redirect_handler") == 0) {
        ret = mo2_redirect_handler(&io,
            json_get_str(in, "authorization_url", ""),
            json_obj_get(in, "port"), json_obj_get(in, "redirect_uri"),
            json_obj_get(in, "redirect_host"));
    } else if (strcmp(func, "close_callback_server") == 0) {
        ret = mo2_close_callback_server(&io);
    } else if (strcmp(func, "settle") == 0) {
        ret = mo2_settle(&io);
    } else if (strcmp(func, "close") == 0) {
        ret = mo2_close(&io);
    } else if (strcmp(func, "wait") == 0) {
        ret = mo2_wait(&io, json_obj_get(in, "port"), json_obj_get(in, "cimd_url"),
                       json_get_num(in, "timeout", 300.0));
    } else if (strcmp(func, "seconds") == 0) {
        ret = mo2_seconds(json_obj_get(in, "value"), json_get_num(in, "default", 0.0));
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, &sc, ret);
    json_free(sc.cursors);
    json_free(sc.prints);
    json_free(sc.writes);
    json_free(io.trace);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
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