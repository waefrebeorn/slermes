/*
 * slash_worker_client.c -- real C11 port of the gateway side of the persistent
 * slash worker: tui_gateway/slash_worker_client.py.
 *
 * PoP: w20_slashworke_cdc456_prepend_tool_paths @ tui_gateway/slash_worker_client.py:_prepend_tool_paths
 * PoP: w20_slashworke_cdc456_run_reply @ tui_gateway/slash_worker_client.py:_SlashWorker.run
 * PoP: w20_slashworke_cdc456_closed_pipe @ tui_gateway/slash_worker_client.py:_SlashWorker.run
 * PoP: w20_slashworke_cdc456_stop_plan @ tui_gateway/slash_worker_client.py:_stop_worker_group
 *
 * The child process, its pipes and the signal syscalls stay with the caller;
 * the PATH composition, the reply state machine (foreign ids, ok/error, seed
 * and output coercions, the closed pipe and the timeout) and the stop policy
 * are the ported behaviour.
 */
#include "hermes_json.h"
#include "noc_w20_slashworke_cdc456.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *swc_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int swc_truthy(const json_t *v) {
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

static int swc_bool_or(const json_t *obj, const char *key, int def) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return def;
    v = json_obj_get(obj, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val ? 1 : 0;
}

static double swc_num_or(const json_t *obj, const char *key, double def) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return def;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* str(x) for the scalar shapes a reply field carries; None is "None". */
static const char *swc_text(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return buf; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val); break;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            fabs(v->num_val) < 9.007199254740992e15) {
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, cap, "%.17g", v->num_val);
        }
        break;
    default: snprintf(buf, cap, "%s", ""); break;
    }
    return buf;
}

/* ---- _prepend_tool_paths ------------------------------------------------- */

json_t *w20_slashworke_cdc456_prepend_tool_paths(const json_t *req) {
    const char *parts[4];
    const char *env_path = swc_str(req, "env_path");
    const char *managed = swc_bool_or(req, "managed_error", 0) ? "" : swc_str(req, "managed_bin");
    const char *home_dir = swc_str(req, "home_dir");
    const char *exe = swc_str(req, "executable");
    int store_first = swc_bool_or(req, "store_first", 0);
    json_t *out = json_object();
    char joined[8192];
    char venv[1024];
    char home[1024];
    size_t len = 0;
    int i, n = 0;

    /* venv_bin = str(Path(sys.executable).parent): the last path component. */
    if (exe && exe[0]) {
        const char *slash = strrchr(exe, '\\');
        const char *fslash = strrchr(exe, '/');
        if (fslash && (!slash || fslash > slash)) slash = fslash;
        if (!slash) snprintf(venv, sizeof(venv), ".");
        else if (slash == exe) snprintf(venv, sizeof(venv), "%c", exe[0]);
        else {
            size_t k = (size_t)(slash - exe);
            if (k >= sizeof(venv)) k = sizeof(venv) - 1;
            memcpy(venv, exe, k);
            venv[k] = '\0';
        }
    } else {
        snprintf(venv, sizeof(venv), ".");
    }
    /* ~/.local/bin: Path.home()/".local"/"bin". */
    if (home_dir && home_dir[0]) snprintf(home, sizeof(home), "%s\\.local\\bin", home_dir);
    else snprintf(home, sizeof(home), ".local\\bin");

    parts[n++] = managed ? managed : "";
    parts[n++] = venv;
    parts[n++] = home;
    parts[n++] = env_path ? env_path : "";
    joined[0] = '\0';
    for (i = 0; i < n; i++) {
        if (!parts[i][0]) continue;                   /* `if p` drops empty parts */
        if (len) joined[len++] = ';';                 /* os.pathsep on this host */
        snprintf(joined + len, sizeof(joined) - len, "%s", parts[i]);
        len = strlen(joined);
    }
    if (store_first && len + 4 < sizeof(joined)) {
        /* pm.store_first_path(env["PATH"]): the PM store dirs go in front. */
        char reordered[8192];
        snprintf(reordered, sizeof(reordered), "PM:%s", joined);
        snprintf(joined, sizeof(joined), "%s", reordered);
    }
    json_set(out, "path", json_string(joined));
    json_set(out, "store_first", json_bool(store_first ? 1 : 0));
    return out;
}

/* ---- closed pipe text ---------------------------------------------------- */

static json_t *swc_closed_pipe_text(const json_t *stderr_tail) {
    char buf[8192];
    size_t len = 0, start = 0, count = 0;
    json_t *out;

    snprintf(buf, sizeof(buf), "slash worker closed pipe");
    len = strlen(buf);
    if (stderr_tail && stderr_tail->type == JSON_ARRAY && stderr_tail->c.count > 0) {
        count = stderr_tail->c.count;
        start = count > 8 ? count - 8 : 0;            /* self.stderr_tail[-8:] */
        buf[len++] = ':';
        buf[len++] = ' ';
        buf[len] = '\0';
        for (; start < count; start++) {
            json_t *line = stderr_tail->c.items[start];
            const char *text = (line && line->type == JSON_STRING) ? line->str_val : "";
            if (len + strlen(text) + 2 >= sizeof(buf)) break;
            snprintf(buf + len, sizeof(buf) - len, "%s", text);
            len = strlen(buf);
            if (start + 1 < count) {
                buf[len++] = '\n';
                buf[len] = '\0';
            }
        }
    }
    out = json_string(buf);
    return out;
}

json_t *w20_slashworke_cdc456_closed_pipe(const json_t *req) {
    json_t *tail = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "stderr_tail") : NULL;
    return swc_closed_pipe_text(tail);
}

/* ---- _SlashWorker.run ---------------------------------------------------- */

static json_t *swc_raised(const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string("RuntimeError"));
    json_set(out, "message", json_string(message));
    return out;
}

/* str.rstrip(): no trailing whitespace left. */
static void swc_rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' ||
                 s[n - 1] == '\r' || s[n - 1] == '\v' || s[n - 1] == '\f')) {
        s[--n] = '\0';
    }
}

json_t *w20_slashworke_cdc456_run_reply(const json_t *req) {
    json_t *messages = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "messages") : NULL;
    json_t *tail = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "stderr_tail") : NULL;
    double seq = swc_num_or(req, "seq", 0.0);
    double rid = seq + 1.0;
    size_t i;

    if (swc_bool_or(req, "exited", 0)) return swc_raised("slash worker exited");

    for (i = 0; messages && messages->type == JSON_ARRAY && i < messages->c.count; i++) {
        const json_t *msg = messages->c.items[i];
        const json_t *id, *ok, *error;
        char text[4096];
        if (!msg || msg->type == JSON_NULL) {
            /* a None sentinel: the worker closed its pipe */
            json_t *closed = swc_closed_pipe_text(tail);
            json_t *out = swc_raised(closed && closed->str_val ? closed->str_val : "");
            json_free(closed);
            return out;
        }
        if (msg->type != JSON_OBJECT) continue;        /* msg.get would raise upstream */
        id = json_obj_get(msg, "id");
        if (!id || id->type != JSON_NUMBER || id->num_val != rid) continue;
        ok = json_obj_get(msg, "ok");
        if (!swc_truthy(ok)) {
            error = json_obj_get(msg, "error");
            if (error && error->type == JSON_STRING) {
                return swc_raised(error->str_val);
            }
            if (error) {
                return swc_raised(swc_text(error, text, sizeof(text)));
            }
            return swc_raised("slash worker failed");   /* msg.get("error", default) */
        }
        {
            json_t *out = json_object();
            char output_buf[4096];
            char seed_buf[1024];
            char tmp[4096];
            json_t *seed = json_obj_get(msg, "seed");

            snprintf(output_buf, sizeof(output_buf), "%s",
                     swc_text(json_obj_get(msg, "output"), tmp, sizeof(tmp)));
            swc_rstrip(output_buf);
            /* `str(msg.get("seed", "") or "")`: any falsy seed reads empty. */
            if (swc_truthy(seed)) {
                char stmp[1024];
                snprintf(seed_buf, sizeof(seed_buf), "%s", swc_text(seed, stmp, sizeof(stmp)));
            } else {
                seed_buf[0] = '\0';
            }
            json_set(out, "status", json_string("ok"));
            json_set(out, "output", json_string(output_buf));
            json_set(out, "seed", json_string(seed_buf));
            return out;
        }
    }
    return swc_raised("slash worker timed out");       /* queue.Empty */
}

/* ---- _stop_worker_group -------------------------------------------------- */

json_t *w20_slashworke_cdc456_stop_plan(const json_t *req) {
    int posix = swc_bool_or(req, "posix", 0);
    int alive = swc_bool_or(req, "alive", 0);
    json_t *signals = json_array();
    json_t *out = json_object();

    if (!posix) {
        /* for stop in (proc.terminate, proc.kill): if poll() is not None: return */
        if (alive) {
            json_append(signals, json_string("terminate"));
            json_append(signals, json_string("kill"));
        }
    } else if (alive) {
        /* Only a leader still alive on entry proves the number is its group. */
        json_append(signals, json_string("SIGTERM"));
        json_append(signals, json_string("SIGKILL"));
    }
    json_set(out, "signals", signals);
    json_set(out, "released", json_bool(posix ? 1 : 0));
    return out;
}
