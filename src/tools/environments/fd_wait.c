/*
 * fd_wait.c -- real C11 port of the selector-choice and timeout-clamp logic of
 * tools/environments/fd_wait.py.
 *
 * PoP: w21_fdwait_850f24_open_plan @ tools/environments/fd_wait.py:open_fd_waiter
 * PoP: w21_fdwait_850f24_wait_plan @ tools/environments/fd_wait.py:FdWaiter.wait
 *
 * Faithful details:
 *  - `except ValueError: return None` -- fd past FD_SETSIZE is not an error, it
 *    just means a select-based reader cannot serve it;
 *  - `except OSError` with errno EBADF returns None (the fd is closed), any
 *    other errno falls through to PollSelector because poll() needs no fd of
 *    its own -- except on darwin, where a character device (the PTY master)
 *    reports POLLNVAL at once and the read loop would spin a core, so that
 *    case re-raises;
 *  - FdWaiter.wait clamps a negative timeout to 0 before selecting and returns
 *    bool() of the ready list, so EOF and hangup count as readable.
 */
#include "noc_w21_fdwait_850f24.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fw_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int fw_truthy(const json_t *v) {
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

json_t *w21_fdwait_850f24_open_plan(const json_t *in) {
    const char *reg = "ok";
    const char *errno_name = "";
    const char *platform = "linux";
    int is_char = 0;
    json_t *out = json_object();

    if (!in || in->type != JSON_OBJECT) in = json_object();
    {
        const json_t *v = json_obj_get(in, "register");
        if (v && v->type == JSON_STRING && v->str_val) reg = v->str_val;
        v = json_obj_get(in, "errno_name");
        if (v && v->type == JSON_STRING && v->str_val) errno_name = v->str_val;
        v = json_obj_get(in, "platform");
        if (v && v->type == JSON_STRING && v->str_val) platform = v->str_val;
        is_char = fw_truthy(json_obj_get(in, "is_char_device"));
    }

    if (!strcmp(reg, "ok")) {
        json_set(out, "kind", json_string("select"));
        json_set(out, "raised", json_null());
        return out;
    }
    if (!strcmp(reg, "ValueError")) {
        /* fd out of range for select(): nothing to drain */
        json_set(out, "kind", json_null());
        json_set(out, "raised", json_null());
            return out;
    }
    if (!strcmp(reg, "EBADF") || !strcmp(errno_name, "EBADF")) {
        json_set(out, "kind", json_null());
        json_set(out, "raised", json_null());
            return out;
    }
    /* any other OSError (EMFILE: the process is at its fd limit) */
    if (!strcmp(platform, "darwin") && is_char) {
        json_set(out, "kind", json_null());
        json_set(out, "raised", json_string("OSError"));
            return out;
    }
    json_set(out, "kind", json_string("poll"));
    json_set(out, "raised", json_null());
    return out;
}

json_t *w21_fdwait_850f24_clamp_timeout(const json_t *timeout_s) {
    double t = 0.0;
    if (timeout_s && timeout_s->type == JSON_NUMBER) t = timeout_s->num_val;
    else if (timeout_s && timeout_s->type == JSON_BOOL) t = timeout_s->bool_val ? 1.0 : 0.0;
    if (t < 0.0) t = 0.0;
    return json_number(t);
}

json_t *w21_fdwait_850f24_wait_plan(const json_t *in) {
    json_t *out = json_object();
    json_t *arg;
    int ready = 0;
    if (!in || in->type != JSON_OBJECT) in = json_object();
    arg = w21_fdwait_850f24_clamp_timeout(json_obj_get(in, "timeout"));
    ready = fw_truthy(json_obj_get(in, "ready"));
    json_set(out, "arg", arg);
    json_set(out, "result", json_bool(ready ? 1 : 0));
    return out;
}

