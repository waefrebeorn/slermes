/*
 * port_web_server_console.c — /api/console frame protocol engine.
 * Faithful port of _console_json_payload, _console_profile_from_ws,
 * _console_send_result, and the console_ws dispatch state machine from
 * hermes_cli/web_server.py.
 */

#include "web_server_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── UTF-8 validation (Python bytes.decode("utf-8") strictness) ─────────── */
static bool utf8_valid(const unsigned char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = s[i];
        size_t need;
        unsigned int cp;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07; }
        else return false;
        if (i + need >= len) return false; /* truncated tail */
        for (size_t k = 1; k <= need; k++) {
            if ((s[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (s[i + k] & 0x3F);
        }
        /* overlong / surrogate / out-of-range */
        if (need == 1 && cp < 0x80) return false;
        if (need == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
        if (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
        i += need + 1;
    }
    return true;
}

/* ── _console_json_payload ──────────────────────────────────────────────── */
/* PoP: ws_console_json_payload @ hermes_cli/web_server.py:_console_json_payload */
json_t *ws_console_json_payload(const char *text, const unsigned char *bytes,
                                size_t bytes_len, char **error) {
    if (error) *error = NULL;
    char *owned = NULL;
    const char *raw = text;
    if (raw == NULL && bytes != NULL) {
        if (!utf8_valid(bytes, bytes_len) ||
            memchr(bytes, '\0', bytes_len) != NULL) {
            /* NUL check: json_parse takes a C string, and a NUL inside the
             * frame would truncate — Python would parse the full buffer and
             * fail on the JSON layer instead; either way it's an error, but
             * an embedded NUL is not valid in JSON text anyway. Keep the
             * UTF-8 error message only for genuine decode failures. */
            if (!utf8_valid(bytes, bytes_len)) {
                if (error) *error = strdup("Console frames must be UTF-8 JSON.");
                return NULL;
            }
            if (error) *error = strdup("Console frames must be JSON objects.");
            return NULL;
        }
        owned = malloc(bytes_len + 1);
        memcpy(owned, bytes, bytes_len);
        owned[bytes_len] = '\0';
        raw = owned;
    }
    if (raw == NULL) return NULL; /* (None, None) */

    json_t *payload = json_parse(raw, NULL);
    free(owned);
    if (!payload) {
        if (error) *error = strdup("Console frames must be JSON objects.");
        return NULL;
    }
    if (payload->type != JSON_OBJECT) {
        json_free(payload);
        if (error) *error = strdup("Console frames must be JSON objects.");
        return NULL;
    }
    return payload;
}

/* ── _console_profile_from_ws ───────────────────────────────────────────── */
/* PoP: ws_console_profile_from_query @ hermes_cli/web_server.py:_console_profile_from_ws */
char *ws_console_profile_from_query(const char *profile) {
    if (!profile) return NULL;
    while (*profile == ' ' || *profile == '\t' || *profile == '\n' ||
           *profile == '\r') profile++;
    size_t l = strlen(profile);
    while (l && (profile[l-1] == ' ' || profile[l-1] == '\t' ||
                 profile[l-1] == '\n' || profile[l-1] == '\r')) l--;
    if (!l) return NULL;
    char *r = malloc(l + 1);
    memcpy(r, profile, l);
    r[l] = '\0';
    return r;
}

/* ── frame builders ─────────────────────────────────────────────────────── */

static json_t *frame_base(const char *type) {
    json_t *o = json_object();
    json_set(o, "type", json_string(type));
    return o;
}

static void push(json_t *frames, json_t *f) { json_append(frames, f); }

/* ── _console_send_result ───────────────────────────────────────────────── */
/* PoP: ws_console_send_result @ hermes_cli/web_server.py:_console_send_result */
void ws_console_send_result(json_t *frames, const ws_console_result_t *result,
                            int command_id) {
    const char *command = result->command ? result->command : "";
    const char *status = result->status ? result->status : "";

    if (strcmp(status, "ok") == 0) {
        if (result->output && *result->output) {
            json_t *f = frame_base("output");
            json_set(f, "id", json_number(command_id));
            json_set(f, "stream", json_string("stdout"));
            json_set(f, "data", json_string(result->output));
            json_set(f, "command", json_string(command));
            push(frames, f);
        }
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("ok"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "error") == 0) {
        json_t *e = frame_base("error");
        json_set(e, "id", json_number(command_id));
        json_set(e, "message", json_string(
            result->output && *result->output ? result->output
                                              : "Command failed."));
        json_set(e, "command", json_string(command));
        push(frames, e);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("error"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "confirm_required") == 0) {
        char fallback[512];
        snprintf(fallback, sizeof(fallback), "Run `%s`?", command);
        json_t *f = frame_base("confirm_required");
        json_set(f, "id", json_number(command_id));
        json_set(f, "command", json_string(command));
        json_set(f, "message", json_string(
            result->confirmation_message && *result->confirmation_message
                ? result->confirmation_message : fallback));
        json_set(f, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, f);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("confirm_required"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "clear") == 0) {
        json_t *f = frame_base("clear");
        json_set(f, "id", json_number(command_id));
        push(frames, f);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("clear"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "exit") == 0) {
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("exit"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(""));
        push(frames, c);
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Unknown console result status: %s", status);
    json_t *e = frame_base("error");
    json_set(e, "id", json_number(command_id));
    json_set(e, "message", json_string(msg));
    json_set(e, "command", json_string(command));
    push(frames, e);
}

/* ── session state machine ──────────────────────────────────────────────── */

struct ws_console_session {
    char *pending_confirmation;  /* NULL when none */
    int command_generation;
    bool busy;                   /* active_task and not done() */
};

ws_console_session_t *ws_console_session_new(void) {
    return calloc(1, sizeof(ws_console_session_t));
}

void ws_console_session_free(ws_console_session_t *s) {
    if (!s) return;
    free(s->pending_confirmation);
    free(s);
}

void ws_console_session_set_busy(ws_console_session_t *s, bool busy) {
    if (s) s->busy = busy;
}
bool ws_console_session_busy(const ws_console_session_t *s) {
    return s && s->busy;
}
const char *ws_console_session_pending(const ws_console_session_t *s) {
    return s ? s->pending_confirmation : NULL;
}
int ws_console_session_generation(const ws_console_session_t *s) {
    return s ? s->command_generation : 0;
}

static void set_pending(ws_console_session_t *s, const char *cmd) {
    free(s->pending_confirmation);
    s->pending_confirmation = cmd ? strdup(cmd) : NULL;
}

static json_t *simple_error(const char *message) {
    json_t *e = frame_base("error");
    json_set(e, "message", json_string(message));
    json_set(e, "prompt", json_string(WS_CONSOLE_PROMPT));
    return e;
}

static json_t *simple_complete(const char *status) {
    json_t *c = frame_base("complete");
    json_set(c, "status", json_string(status));
    json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
    return c;
}

/* Python str.strip().lower() of payload.get("type"). */
static char *frame_type_of(const json_t *payload) {
    const char *t = json_get_str(payload, "type", NULL);
    if (!t) t = "";  /* str(None or "") == "" */
    while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r') t++;
    size_t l = strlen(t);
    while (l && (t[l-1] == ' ' || t[l-1] == '\t' || t[l-1] == '\n' ||
                 t[l-1] == '\r')) l--;
    char *r = malloc(l + 1);
    for (size_t i = 0; i < l; i++)
        r[i] = (t[i] >= 'A' && t[i] <= 'Z') ? (char)(t[i] + 32) : t[i];
    r[l] = '\0';
    return r;
}

/* run_command inline (synchronous seam: exec + result frames). */
/* PoP: ws_console_handle_frame @ hermes_cli/web_server.py:run_command */
/* PoP: ws_console_handle_frame @ hermes_cli/web_server.py:start_command */
static bool run_command_now(ws_console_session_t *s, const char *line,
                            bool confirmed, ws_console_exec_fn exec,
                            void *exec_ctx, json_t *frames) {
    /* start_command: command_generation += 1 */
    s->command_generation++;
    int command_id = s->command_generation;

    ws_console_result_t result = {0};
    bool ok = exec ? exec(exec_ctx, line, confirmed, &result) : false;
    if (!ok) {
        /* except Exception path */
        set_pending(s, NULL);
        json_t *e = frame_base("error");
        json_set(e, "id", json_number(command_id));
        json_set(e, "message", json_string(
            result.output && *result.output ? result.output
                                            : "Exception"));
        json_set(e, "command", json_string(line));
        push(frames, e);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("error"));
        json_set(c, "command", json_string(line));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return false;
    }
    /* else branch: pending = command if confirm_required else None */
    if (result.status && strcmp(result.status, "confirm_required") == 0)
        set_pending(s, result.command ? result.command : "");
    else
        set_pending(s, NULL);
    ws_console_send_result(frames, &result, command_id);
    return result.status && strcmp(result.status, "exit") == 0;
}

/* PoP: ws_console_handle_frame @ hermes_cli/web_server.py:console_ws */
bool ws_console_handle_frame(ws_console_session_t *s, const json_t *payload,
                             ws_console_exec_fn exec, void *exec_ctx,
                             json_t *frames) {
    char *ft = frame_type_of(payload);

    if (strcmp(ft, "ping") == 0) {
        json_t *p = frame_base("pong");
        json_set(p, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, p);
        free(ft);
        return false;
    }

    if (strcmp(ft, "cancel") == 0) {
        if (s->busy) {
            s->command_generation++;
            s->busy = false;
            set_pending(s, NULL);
            push(frames, simple_complete("cancelled"));
        } else if (s->pending_confirmation) {
            set_pending(s, NULL);
            push(frames, simple_complete("cancelled"));
        } else {
            push(frames, simple_complete("idle"));
        }
        free(ft);
        return false;
    }

    if (s->busy) {
        push(frames, simple_error("A console command is already running."));
        free(ft);
        return false;
    }

    if (strcmp(ft, "confirm") == 0) {
        const char *cmd_raw = json_get_str(payload, "command", NULL);
        const char *src = cmd_raw && *cmd_raw ? cmd_raw
                          : (s->pending_confirmation ? s->pending_confirmation
                                                     : "");
        /* .strip() */
        while (*src == ' ' || *src == '\t') src++;
        size_t l = strlen(src);
        while (l && (src[l-1] == ' ' || src[l-1] == '\t')) l--;
        char *command = malloc(l + 1);
        memcpy(command, src, l);
        command[l] = '\0';

        if (!s->pending_confirmation) {
            push(frames, simple_error("No command is waiting for confirmation."));
            free(command);
            free(ft);
            return false;
        }
        if (strcmp(command, s->pending_confirmation) != 0) {
            push(frames, simple_error(
                "Confirmation does not match the pending command."));
            free(command);
            free(ft);
            return false;
        }
        set_pending(s, NULL);
        bool close = run_command_now(s, command, true, exec, exec_ctx, frames);
        free(command);
        free(ft);
        return close;
    }

    if (strcmp(ft, "input") == 0 || strcmp(ft, "command") == 0) {
        const char *line_raw = json_get_str(payload, "line", NULL);
        if (!line_raw || !*line_raw)
            line_raw = json_get_str(payload, "command", NULL);
        if (!line_raw) line_raw = "";
        while (*line_raw == ' ' || *line_raw == '\t') line_raw++;
        size_t l = strlen(line_raw);
        while (l && (line_raw[l-1] == ' ' || line_raw[l-1] == '\t')) l--;
        char *line = malloc(l + 1);
        memcpy(line, line_raw, l);
        line[l] = '\0';

        if (!*line) {
            push(frames, simple_complete("ok"));
            free(line);
            free(ft);
            return false;
        }
        if (s->pending_confirmation) {
            push(frames, simple_error(
                "Confirm or cancel the pending command before running "
                "another one."));
            free(line);
            free(ft);
            return false;
        }
        bool close = run_command_now(s, line, false, exec, exec_ctx, frames);
        free(line);
        free(ft);
        return close;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Unsupported console frame: %s",
             *ft ? ft : "?");
    push(frames, simple_error(msg));
    free(ft);
    return false;
}
