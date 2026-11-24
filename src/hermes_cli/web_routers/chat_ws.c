/*
 * chat_ws.c - C11 port of upstream hermes_cli/web_routers/chat_ws.py
 * (gates w32_chatws_cd49ef_p1, w32_chatws_cd49ef_p2).
 *
 * PoP: w32_chatws_cd49ef_p1_console_run @ hermes_cli/web_routers/chat_ws.py:console_ws
 * PoP: w32_chatws_cd49ef_p1_console_payload @ hermes_cli/web_routers/chat_ws.py:_console_json_payload
 * PoP: w32_chatws_cd49ef_p1_channel_key @ hermes_cli/web_routers/chat_ws.py:_channel_or_close_code
 * PoP: w32_chatws_cd49ef_p1_marker_session @ hermes_cli/web_routers/chat_ws.py:_read_active_session_file
 * PoP: w32_chatws_cd49ef_p2_sidecar_gate @ hermes_cli/web_routers/chat_ws.py:_close_unless_sidecar_allowed
 * PoP: w32_chatws_cd49ef_p2_event_state @ hermes_cli/web_routers/chat_ws.py:_get_event_state
 * PoP: w32_chatws_cd49ef_p2_lease_holder @ hermes_cli/web_routers/chat_ws.py:_lease_holder_pid
 * PoP: w32_chatws_cd49ef_p2_broadcast @ hermes_cli/web_routers/chat_ws.py:_broadcast_event
 * PoP: w32_chatws_cd49ef_p2_marker_state @ hermes_cli/web_routers/chat_ws.py:_pty_channel_marker_state
 *
 * Every socket, lock, thread and file is a seam: the websocket is a frame list
 * fed in as a fixture, the console worker answers from a canned result list,
 * the lease registry is a snapshot list, and the app's event-channels map is
 * plain data.  What is ported here is what those values decide: the console
 * frames a frame sequence produces (and the close codes it earns), the payload
 * a websocket message decodes to, the channel/topic key a query string may
 * name, the session id a marker file yields, the sidecar accept/reject
 * decision, the lazy event-state initialisation, the lease holder, the
 * broadcast fan-out, and the /api/pty marker state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w32_chatws_cd49ef_p1.h"
#include "noc_w32_chatws_cd49ef_p2.h"

/* _CONSOLE_PROMPT */
#define CW_CONSOLE_PROMPT "hermes> "
/* _VALID_CHANNEL_RE quantifier bound */
#define CW_MAX_CHANNEL 128

/* ------------------------------------------------------------------ */
/* shared value helpers                                               */
/* ------------------------------------------------------------------ */

/* Python truthiness of a JSON value. */
static int cw_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static char *cw_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s ? s : "", n + 1);
    return out;
}

/* Python str() over the scalar shapes a frame field holds.  Containers are
 * never stringified here: the fixtures keep frame fields scalar. */
static char *cw_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return cw_dup("");
    if (v->type == JSON_STRING) return cw_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return cw_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return cw_dup(buf);
    }
    return cw_dup("");
}

static int cw_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip(). */
static char *cw_strip(const char *s) {
    const char *b = s ? s : "";
    size_t n;
    char *out;
    while (*b && cw_space((unsigned char)*b)) b++;
    n = strlen(b);
    while (n > 0 && cw_space((unsigned char)b[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

static void cw_lower(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* str(x or "").strip().lower() -- the console's frame-type key. */
static char *cw_norm_key(const json_t *v) {
    char *s = cw_py_str(v);
    char *t = cw_strip(s);
    free(s);
    if (!t) return cw_dup("");
    cw_lower(t);
    return t;
}

/* Strict UTF-8 validation: overlongs, surrogates and >U+10FFFF are refused,
 * as bytes.decode("utf-8") refuses them. */
static int cw_utf8_valid(const unsigned char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        size_t need, k;
        unsigned long cp, min;
        if (c < 0x80) { i++; continue; }
        if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1Fu; min = 0x80; }
        else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0Fu; min = 0x800; }
        else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07u; min = 0x10000; }
        else return 0;
        if (i + need >= n) return 0;
        for (k = 1; k <= need; k++) {
            unsigned char cc = s[i + k];
            if ((cc & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (unsigned long)(cc & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF) return 0;
        if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
        i += need + 1;
    }
    return 1;
}

static int cw_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "6869" -> two bytes.  Returns 0 on odd length or a non-hex digit. */
static int cw_hex_decode(const char *hex, unsigned char **out, size_t *n) {
    size_t len = strlen(hex ? hex : ""), i;
    unsigned char *buf;
    *out = NULL;
    *n = 0;
    if (len % 2) return 0;
    buf = (unsigned char *)malloc(len / 2 + 1);
    if (!buf) return 0;
    for (i = 0; i < len; i += 2) {
        int hi = cw_hexval(hex[i]), lo = cw_hexval(hex[i + 1]);
        if (hi < 0 || lo < 0) { free(buf); return 0; }
        buf[i / 2] = (unsigned char)((hi << 4) | lo);
    }
    buf[len / 2] = 0;
    *out = buf;
    *n = len / 2;
    return 1;
}

/* ------------------------------------------------------------------ */
/* console frame shaping (_ConsoleSender)                             */
/* ------------------------------------------------------------------ */

/* send({**payload, "prompt": _CONSOLE_PROMPT}) */
static json_t *cw_frame_prompt(const json_t *payload) {
    json_t *f = (payload && payload->type == JSON_OBJECT) ? json_copy(payload) : json_object();
    if (!f || f->type != JSON_OBJECT) {
        json_free(f);
        f = json_object();
    }
    json_set(f, "prompt", json_string(CW_CONSOLE_PROMPT));
    return f;
}

/* {type:error, id?, message, command?, prompt?} */
static json_t *cw_frame_error(const char *message, const json_t *id,
                              const char *command, const char *prompt) {
    json_t *f = json_object();
    json_set(f, "type", json_string("error"));
    if (id) json_set(f, "id", json_copy(id));
    json_set(f, "message", json_string(message ? message : ""));
    if (command) json_set(f, "command", json_string(command));
    if (prompt) json_set(f, "prompt", json_string(prompt));
    return f;
}

/* {type:complete, id, status, command, prompt} */
static json_t *cw_frame_complete(const char *status, const char *command,
                                 int id, const char *prompt) {
    json_t *f = json_object();
    json_set(f, "type", json_string("complete"));
    json_set(f, "id", json_number((double)id));
    json_set(f, "status", json_string(status ? status : ""));
    json_set(f, "command", json_string(command ? command : ""));
    json_set(f, "prompt", json_string(prompt ? prompt : ""));
    return f;
}

/* result.command or "" */
static char *cw_result_command(const json_t *res) {
    const json_t *c = json_obj_get(res, "command");
    if (!cw_truthy(c)) return cw_dup("");
    return cw_py_str(c);
}

/* result.output, or NULL when falsy. */
static const char *cw_result_output(const json_t *res) {
    const json_t *o = json_obj_get(res, "output");
    if (!cw_truthy(o) || o->type != JSON_STRING) return NULL;
    return o->str_val;
}

static const char *cw_result_status(const json_t *res) {
    const json_t *s = res ? json_obj_get(res, "status") : NULL;
    return (s && s->type == JSON_STRING && s->str_val) ? s->str_val : "ok";
}

/* _ConsoleSender.send_result: the frames one engine result becomes. */
static void cw_send_result(json_t *sent, const json_t *res, int id) {
    char *command = cw_result_command(res);
    const char *status = cw_result_status(res);
    const char *output = cw_result_output(res);

    if (!strcmp(status, "ok")) {
        if (output && output[0]) {
            json_t *f = json_object();
            json_set(f, "type", json_string("output"));
            json_set(f, "id", json_number((double)id));
            json_set(f, "stream", json_string("stdout"));
            json_set(f, "data", json_string(output));
            json_set(f, "command", json_string(command));
            json_append(sent, f);
        }
        json_append(sent, cw_frame_complete("ok", command, id, CW_CONSOLE_PROMPT));
    } else if (!strcmp(status, "error")) {
        json_append(sent, cw_frame_error(output ? output : "Command failed.",
                                         json_number((double)id), command, NULL));
        json_append(sent, cw_frame_complete("error", command, id, CW_CONSOLE_PROMPT));
    } else if (!strcmp(status, "confirm_required")) {
        const json_t *cm = json_obj_get(res, "confirmation_message");
        const char *msg = (cw_truthy(cm) && cm->type == JSON_STRING) ? cm->str_val : NULL;
        char *built = NULL;
        json_t *f = json_object();
        if (!msg) {
            size_t need = strlen("Run ``?") + strlen(command) + 1;
            built = (char *)malloc(need);
            if (built) snprintf(built, need, "Run `%s`?", command);
        }
        json_set(f, "type", json_string("confirm_required"));
        json_set(f, "id", json_number((double)id));
        json_set(f, "command", json_string(command));
        json_set(f, "message", json_string(msg ? msg : (built ? built : "")));
        json_set(f, "prompt", json_string(CW_CONSOLE_PROMPT));
        json_append(sent, f);
        free(built);
        json_append(sent, cw_frame_complete("confirm_required", command, id, CW_CONSOLE_PROMPT));
    } else if (!strcmp(status, "clear")) {
        json_t *f = json_object();
        json_set(f, "type", json_string("clear"));
        json_set(f, "id", json_number((double)id));
        json_append(sent, f);
        json_append(sent, cw_frame_complete("clear", command, id, CW_CONSOLE_PROMPT));
    } else if (!strcmp(status, "exit")) {
        json_append(sent, cw_frame_complete("exit", command, id, ""));
    } else {
        size_t need = strlen("Unknown console result status: ") + strlen(status) + 1;
        char *msg = (char *)malloc(need);
        if (msg) snprintf(msg, need, "Unknown console result status: %s", status);
        json_append(sent, cw_frame_error(msg ? msg : "Unknown console result status",
                                         json_number((double)id), command, NULL));
        free(msg);
    }
    free(command);
}

/* The i-th canned engine result, or NULL when the fixture ran out. */
static const json_t *cw_result_at(const json_t *results, size_t i) {
    const json_t *r;
    if (!results || results->type != JSON_ARRAY) return NULL;
    r = json_get(results, i);
    if (!r || r->type != JSON_OBJECT) return NULL;
    return r;
}

/* Runs one started command's frames.  Returns 1 when the command asked the
 * socket to close, so the caller records the close. */
static int cw_run_command(json_t *sent, const json_t *res, int id, char **pending) {
    const char *status = cw_result_status(res);
    cw_send_result(sent, res, id);
    free(*pending);
    *pending = NULL;
    if (res && !strcmp(status, "confirm_required"))
        *pending = cw_result_command(res);
    return res && !strcmp(status, "exit");
}

static void cw_close_1000(json_t *closed) {
    json_t *pair = json_array();
    json_append(pair, json_number(1000.0));
    json_append(pair, json_null());
    json_append(closed, pair);
}

/* ------------------------------------------------------------------ */
/* _console_json_payload                                              */
/* ------------------------------------------------------------------ */

/* Reads "text" first, then "bytes_hex" (the fixture stand-in for msg["bytes"])
 * and returns the decoded payload (owned), or NULL with *err set (owned). */
static json_t *cw_payload_of(const json_t *msg, char **err) {
    const json_t *text = json_obj_get(msg, "text");
    const json_t *hex = json_obj_get(msg, "bytes_hex");
    const char *raw = NULL;
    unsigned char *bytes = NULL;
    size_t nbytes = 0;
    json_t *parsed;

    *err = NULL;
    if (text && text->type == JSON_STRING) {
        raw = text->str_val ? text->str_val : "";
    } else if (hex && hex->type == JSON_STRING) {
        if (!cw_hex_decode(hex->str_val, &bytes, &nbytes)) {
            *err = cw_dup("Console frames must be JSON objects.");
            return NULL;
        }
        if (!cw_utf8_valid(bytes, nbytes)) {
            free(bytes);
            *err = cw_dup("Console frames must be UTF-8 JSON.");
            return NULL;
        }
        raw = (const char *)bytes; /* NUL-terminated by cw_hex_decode */
    } else {
        return NULL; /* raw is None: nothing to say */
    }

    parsed = json_parse(raw, NULL);
    free(bytes);
    if (!parsed) {
        *err = cw_dup("Console frames must be JSON objects.");
        return NULL;
    }
    if (parsed->type != JSON_OBJECT) {
        json_free(parsed);
        *err = cw_dup("Console frames must be JSON objects.");
        return NULL;
    }
    return parsed;
}

json_t *w32_chatws_cd49ef_p1_console_payload(const json_t *req) {
    json_t *out = json_object();
    char *err = NULL;
    json_t *payload = cw_payload_of(req, &err);
    json_set(out, "payload", payload ? payload : json_null());
    json_set(out, "error", err ? json_string(err) : json_null());
    free(err);
    return out;
}

/* ------------------------------------------------------------------ */
/* console_ws: the frame sequence one websocket session produces      */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p1_console_run(const json_t *req) {
    const json_t *query = json_obj_get(req, "query");
    const json_t *msgs = json_obj_get(req, "msgs");
    const json_t *results = json_obj_get(req, "results");
    json_t *sent = json_array();
    json_t *closed = json_array();
    json_t *out = json_object();
    char *profile = NULL;
    char *pending = NULL;     /* pending_confirmation (owned) */
    int generation = 0;
    size_t ri = 0, i, nmsgs;

    /* profile = (ws.query_params.get("profile") or "").strip() or None */
    {
        const json_t *p = json_obj_get(query, "profile");
        char *raw = cw_truthy(p) ? cw_py_str(p) : cw_dup("");
        char *t = cw_strip(raw);
        free(raw);
        if (t && t[0]) profile = t; else free(t);
    }

    /* await out.prompt(type="ready", profile=profile or "current") */
    {
        json_t *p = json_object();
        json_set(p, "type", json_string("ready"));
        json_set(p, "profile", json_string(profile ? profile : "current"));
        json_append(sent, cw_frame_prompt(p));
        json_free(p);
    }

    nmsgs = (msgs && msgs->type == JSON_ARRAY) ? json_len(msgs) : 0;
    for (i = 0; i < nmsgs; i++) {
        const json_t *msg = json_get(msgs, i);
        const json_t *mtype = (msg && msg->type == JSON_OBJECT) ? json_obj_get(msg, "type") : NULL;
        json_t *payload;
        char *err = NULL, *frame_type;

        if (mtype && mtype->type == JSON_STRING && mtype->str_val &&
            !strcmp(mtype->str_val, "websocket.disconnect"))
            break;

        payload = cw_payload_of(msg, &err);
        if (err) {
            json_t *p = json_object();
            json_set(p, "type", json_string("error"));
            json_set(p, "message", json_string(err));
            json_append(sent, cw_frame_prompt(p));
            json_free(p);
            free(err);
            continue;
        }
        if (!payload) continue;

        frame_type = cw_norm_key(json_obj_get(payload, "type"));

        if (!strcmp(frame_type, "ping")) {
            json_t *p = json_object();
            json_set(p, "type", json_string("pong"));
            json_append(sent, cw_frame_prompt(p));
            json_free(p);
        } else if (!strcmp(frame_type, "cancel")) {
            /* The canned worker exits before the next frame arrives, so the
             * running and pending branches both emit "cancelled"; only the
             * idle case differs. */
            json_t *p = json_object();
            if (pending) {
                free(pending);
                pending = NULL;
                json_set(p, "type", json_string("complete"));
                json_set(p, "status", json_string("cancelled"));
            } else {
                json_set(p, "type", json_string("complete"));
                json_set(p, "status", json_string("idle"));
            }
            json_append(sent, cw_frame_prompt(p));
            json_free(p);
        } else if (!strcmp(frame_type, "confirm")) {
            const json_t *pc = json_obj_get(payload, "command");
            char *raw = cw_truthy(pc) ? cw_py_str(pc) : cw_dup(pending ? pending : "");
            char *cmd = cw_strip(raw);
            free(raw);
            if (!pending) {
                json_append(sent, cw_frame_error(
                    "No command is waiting for confirmation.", NULL, NULL, CW_CONSOLE_PROMPT));
            } else if (!cmd || strcmp(cmd, pending) != 0) {
                json_append(sent, cw_frame_error(
                    "Confirmation does not match the pending command.",
                    NULL, NULL, CW_CONSOLE_PROMPT));
            } else {
                generation++;
                if (cw_run_command(sent, cw_result_at(results, ri++), generation, &pending))
                    cw_close_1000(closed);
            }
            free(cmd);
        } else if (!strcmp(frame_type, "input") || !strcmp(frame_type, "command")) {
            const json_t *pl = json_obj_get(payload, "line");
            const json_t *pc = json_obj_get(payload, "command");
            const json_t *src = cw_truthy(pl) ? pl : (cw_truthy(pc) ? pc : NULL);
            char *raw = src ? cw_py_str(src) : cw_dup("");
            char *line = cw_strip(raw);
            free(raw);
            if (!line || !line[0]) {
                json_t *p = json_object();
                json_set(p, "type", json_string("complete"));
                json_set(p, "status", json_string("ok"));
                json_append(sent, cw_frame_prompt(p));
                json_free(p);
            } else if (pending) {
                json_append(sent, cw_frame_error(
                    "Confirm or cancel the pending command before running another one.",
                    NULL, NULL, CW_CONSOLE_PROMPT));
            } else {
                generation++;
                if (cw_run_command(sent, cw_result_at(results, ri++), generation, &pending))
                    cw_close_1000(closed);
            }
            free(line);
        } else {
            size_t need = strlen("Unsupported console frame: ") +
                          strlen(frame_type[0] ? frame_type : "?") + 1;
            char *msg_text = (char *)malloc(need);
            if (msg_text)
                snprintf(msg_text, need, "Unsupported console frame: %s",
                         frame_type[0] ? frame_type : "?");
            json_append(sent, cw_frame_error(msg_text ? msg_text : "Unsupported console frame:",
                                             NULL, NULL, CW_CONSOLE_PROMPT));
            free(msg_text);
        }

        free(frame_type);
        json_free(payload);
    }

    json_set(out, "sent", sent);
    json_set(out, "closed", closed);
    free(pending);
    free(profile);
    return out;
}

/* ------------------------------------------------------------------ */
/* _channel_or_close_code                                             */
/* ------------------------------------------------------------------ */

/* ^[A-Za-z0-9._-]{1,128}$ under re.match(): '$' also matches before one
 * trailing newline, so "ch1\n" is valid and "ch1\n\n" is not. */
static char *cw_channel_value(const json_t *v) {
    const char *s;
    size_t n, limit, i;
    char *copy;
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    s = v->str_val;
    n = strlen(s);
    limit = n;
    if (limit > 0 && s[limit - 1] == '\n') limit--;   /* '$' before the final newline */
    if (limit < 1 || limit > CW_MAX_CHANNEL) return NULL;
    for (i = 0; i < limit; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return NULL;
    }
    /* The caller keeps the query value verbatim, trailing newline included:
     * re.match() validates, it does not normalise. */
    copy = (char *)malloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n + 1);
    return copy;
}

json_t *w32_chatws_cd49ef_p1_channel_key(const json_t *req) {
    char *chan = cw_channel_value(json_obj_get(req, "channel"));
    json_t *out;
    if (!chan) return json_null();
    out = json_string(chan);
    free(chan);
    return out;
}

/* ------------------------------------------------------------------ */
/* _read_active_session_file                                          */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p1_marker_session(const json_t *req) {
    const json_t *doc = json_obj_get(req, "doc");
    const json_t *sid;
    const char *text;
    json_t *data, *out;
    char *raw, *stripped;

    if (!doc || doc->type != JSON_STRING || !doc->str_val) return json_null();
    text = doc->str_val;
    /* path.read_text(encoding="utf-8-sig") strips one leading BOM. */
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        text += 3;
    data = json_parse(text, NULL);
    if (!data) return json_null();
    if (data->type != JSON_OBJECT) {
        json_free(data);
        return json_null();
    }
    sid = json_obj_get(data, "session_id");
    if (!cw_truthy(sid)) {
        json_free(data);
        return json_null();
    }
    raw = cw_py_str(sid);
    stripped = cw_strip(raw);
    free(raw);
    json_free(data);
    if (!stripped || !stripped[0]) {
        free(stripped);
        return json_null();
    }
    out = json_string(stripped);
    free(stripped);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _close_unless_sidecar_allowed                                  */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p2_sidecar_gate(const json_t *req) {
    json_t *out = json_object();
    json_t *closed = json_array();
    int code = 0;

    if (!cw_truthy(json_obj_get(req, "chat_enabled"))) code = 4403;
    else if (!cw_truthy(json_obj_get(req, "auth_ok"))) code = 4401;
    else if (!cw_truthy(json_obj_get(req, "request_allowed"))) code = 4403;

    json_set(out, "accepted", json_bool(code == 0));
    if (code) json_append(closed, json_number((double)code));
    json_set(out, "closed", closed);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _get_event_state                                               */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p2_event_state(const json_t *req) {
    int fresh = !(cw_truthy(json_obj_get(req, "has_channels")) &&
                  cw_truthy(json_obj_get(req, "has_lock")));
    json_t *out = json_object();
    json_t *counts = json_object();
    const json_t *channels = json_obj_get(req, "channels");

    json_set(out, "created", json_bool(fresh));
    if (!fresh && channels && channels->type == JSON_OBJECT) {
        size_t i, n = json_object_size(channels);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(channels, i);
            const json_t *v = json_object_get_at(channels, i);
            if (!k) continue;
            json_set(counts, k,
                     json_number((double)((v && v->type == JSON_ARRAY) ? json_len(v) : 0)));
        }
    }
    json_set(out, "channels", counts);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _lease_holder_pid                                              */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p2_lease_holder(const json_t *req) {
    const json_t *sid = json_obj_get(req, "session_id");
    const json_t *entries = json_obj_get(req, "entries");
    size_t i, n;

    if (cw_truthy(json_obj_get(req, "raises"))) return json_null();
    if (!cw_truthy(sid) || sid->type != JSON_STRING) return json_null();
    if (!entries || entries->type != JSON_ARRAY) return json_null();
    n = json_len(entries);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        const json_t *esid, *pid;
        if (!e || e->type != JSON_OBJECT) continue;
        esid = json_obj_get(e, "session_id");
        if (!esid || esid->type != JSON_STRING) continue;
        if (strcmp(esid->str_val ? esid->str_val : "", sid->str_val) != 0) continue;
        pid = json_obj_get(e, "pid");
        if (!pid) continue;
        /* isinstance(pid, int): a bool counts, an integral number counts, a
         * string and a fractional number do not. */
        if (pid->type == JSON_BOOL) return json_bool(pid->bool_val);
        if (pid->type == JSON_NUMBER && pid->num_val == floor(pid->num_val) &&
            fabs(pid->num_val) < 1e15)
            return json_number(pid->num_val);
    }
    return json_null();
}

/* ------------------------------------------------------------------ */
/* p2: _broadcast_event                                               */
/* ------------------------------------------------------------------ */

/* One [subscriber, payload] pair per delivered frame, in send order.  A
 * subscriber whose send raises is skipped and the rest still receive. */
json_t *w32_chatws_cd49ef_p2_broadcast(const json_t *req) {
    const json_t *channels = json_obj_get(req, "channels");
    const json_t *channel = json_obj_get(req, "channel");
    const json_t *payload = json_obj_get(req, "payload");
    const json_t *failing = json_obj_get(req, "failing");
    json_t *out = json_array();
    const json_t *subs = NULL;
    const char *ch = (channel && channel->type == JSON_STRING) ? channel->str_val : "";
    const char *text = (payload && payload->type == JSON_STRING) ? payload->str_val : "";
    size_t i, n;

    if (channels && channels->type == JSON_OBJECT) subs = json_obj_get(channels, ch);
    if (!subs || subs->type != JSON_ARRAY) return out;
    n = json_len(subs);
    for (i = 0; i < n; i++) {
        const json_t *s = json_get(subs, i);
        const char *name = (s && s->type == JSON_STRING) ? s->str_val : NULL;
        int dead = 0;
        if (!name) continue;
        if (failing && failing->type == JSON_ARRAY) {
            size_t j, fn = json_len(failing);
            for (j = 0; j < fn; j++) {
                const json_t *f = json_get(failing, j);
                if (f && f->type == JSON_STRING && f->str_val && !strcmp(f->str_val, name)) {
                    dead = 1;
                    break;
                }
            }
        }
        if (dead) continue;
        {
            json_t *pair = json_array();
            json_append(pair, json_string(name));
            json_append(pair, json_string(text));
            json_append(out, pair);
        }
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* p2: _pty_channel_marker_state                                      */
/* ------------------------------------------------------------------ */

json_t *w32_chatws_cd49ef_p2_marker_state(const json_t *req) {
    const json_t *markers = json_obj_get(req, "marker_channels");
    const json_t *active = json_obj_get(req, "active_file");
    const json_t *resume = json_obj_get(req, "resume");
    const json_t *doc = json_obj_get(req, "marker_doc");
    json_t *out = json_object();
    char *chan = cw_channel_value(json_obj_get(req, "channel"));
    char *res = NULL;
    int preexisting = 0;
    int force_fresh = cw_truthy(json_obj_get(req, "force_fresh"));

    if (!chan) {
        json_set(out, "file", json_null());
        json_set(out, "preexisting", json_bool(0));
        json_set(out, "resume", (resume && resume->type == JSON_STRING)
                                     ? json_copy(resume) : json_null());
        return out;
    }

    /* marker_preexisting = channel in marker_files */
    if (markers && markers->type == JSON_ARRAY) {
        size_t i, n = json_len(markers);
        for (i = 0; i < n; i++) {
            const json_t *m = json_get(markers, i);
            if (m && m->type == JSON_STRING && m->str_val && !strcmp(m->str_val, chan)) {
                preexisting = 1;
                break;
            }
        }
    }

    if (cw_truthy(resume)) res = cw_py_str(resume);

    if (force_fresh) {
        free(res);
        res = NULL;
        preexisting = 0;
    } else if (!cw_truthy(resume)) {
        /* resume = _read_active_session_file(active_session_file) */
        json_t *sid = json_null();
        free(res);
        res = NULL;
        if (active && active->type == JSON_STRING) {
            json_t *sub = json_object();
            json_set(sub, "doc", json_copy(doc));
            sid = w32_chatws_cd49ef_p1_marker_session(sub);
            json_free(sub);
            if (sid && sid->type == JSON_STRING && sid->str_val) res = cw_dup(sid->str_val);
        }
        json_free(sid);
    }

    json_set(out, "file", (active && active->type == JSON_STRING)
                              ? json_copy(active) : json_null());
    json_set(out, "preexisting", json_bool(preexisting));
    json_set(out, "resume", res ? json_string(res) : json_null());
    free(res);
    free(chan);
    return out;
}

