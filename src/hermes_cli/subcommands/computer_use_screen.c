/*
 * computer_use_screen.c -- C11 port of upstream Hermes
 *   hermes_cli/subcommands/computer_use_screen.py
 *
 * PoP: w21_computerus_9582b9_status @ hermes_cli/subcommands/computer_use_screen.py:_screen_status
 * PoP: w21_computerus_9582b9_start @ hermes_cli/subcommands/computer_use_screen.py:_screen_start
 * PoP: w21_computerus_9582b9_stop @ hermes_cli/subcommands/computer_use_screen.py:_screen_stop
 * PoP: w21_computerus_9582b9_install @ hermes_cli/subcommands/computer_use_screen.py:_screen_install
 * PoP: w21_computerus_9582b9_actions @ hermes_cli/subcommands/computer_use_screen.py:SCREEN_ACTIONS
 *
 * The Bot Desktop runtime (tools.bot_desktop.runtime / lease / install) is a
 * host seam: the C side receives its answers as an injected state object and
 * reproduces every printed line and exit code the Python handler derives from
 * them. The argparse wiring (build_screen_parser) stays upstream: it is pure
 * argparse plumbing with no branch of its own.
 *
 * Non-ASCII report text (the U+2192 arrow) is emitted as UTF-8 escapes so this
 * source stays 7-bit ASCII.
 */
#include "hermes_json.h"
#include "noc_w21_computerus_9582b9.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPUTERUS_ARROW "\xe2\x86\x92"

/* --- growable text buffer ------------------------------------------------- */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} computerus_buf;

static void computerus_buf_init(computerus_buf *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static void computerus_buf_reserve(computerus_buf *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *grown;
    if (want <= b->cap) return;
    if (b->cap == 0) b->cap = 128;
    while (b->cap < want) b->cap *= 2;
    grown = (char *)realloc(b->p, b->cap);
    if (!grown) return;
    b->p = grown;
}

static void computerus_buf_add(computerus_buf *b, const char *s) {
    size_t n;
    if (!s) s = "";
    n = strlen(s);
    computerus_buf_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n + 1);
    b->n += n;
}

static void computerus_buf_fmt(computerus_buf *b, const char *fmt, ...) {
    va_list ap, ap2;
    int need;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) {
        va_end(ap2);
        return;
    }
    computerus_buf_reserve(b, (size_t)need);
    if (b->p) {
        vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap2);
        b->n += (size_t)need;
    }
    va_end(ap2);
}

static json_t *computerus_text_out(int rc, computerus_buf *b) {
    json_t *out = json_object();
    json_set(out, "rc", json_number((double)rc));
    json_set(out, "text", json_string(b->p ? b->p : ""));
    free(b->p);
    computerus_buf_init(b);
    return out;
}

/* ", ".join(items) for an array of scalars; Python str() of each item. */
static void computerus_join(computerus_buf *b, const json_t *arr, const char *sep) {
    size_t i;
    char numbuf[64];
    int first = 1;
    if (!arr || arr->type != JSON_ARRAY) return;
    for (i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        const char *s = "";
        if (v && v->type == JSON_STRING) s = v->str_val ? v->str_val : "";
        else if (v && v->type == JSON_NUMBER) {
            snprintf(numbuf, sizeof(numbuf), "%.17g", v->num_val);
            s = numbuf;
        }
        if (!first) computerus_buf_add(b, sep);
        computerus_buf_add(b, s);
        first = 0;
    }
}

static const char *computerus_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = json_obj_get(obj, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

static int computerus_truthy(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- _screen_status ------------------------------------------------------- */

json_t *w21_computerus_9582b9_status(const json_t *state) {
    computerus_buf b;
    json_t *missing = json_obj_get(state, "missing");
    int rc;

    computerus_buf_init(&b);
    if (json_get_bool(state, "json", 0)) {
        json_t *as_dict = json_obj_get(state, "as_dict");
        json_t *lease_view = json_obj_get(state, "lease_view");
        json_t *payload = (as_dict && as_dict->type == JSON_OBJECT) ? json_copy(as_dict)
                                                                   : json_object();
        json_t *out;
        json_set(payload, "lease", lease_view ? json_copy(lease_view) : json_null());
        rc = computerus_truthy(state, "running") ? 0 : 1;
        out = json_object();
        json_set(out, "rc", json_number((double)rc));
        json_set(out, "payload", payload);
        free(b.p);
        return out;
    }

    if (!computerus_truthy(state, "supported")) {
        computerus_buf_add(&b, "Bot Desktop screens run on Linux gateway hosts only "
                              "(this host keeps its real display).\n");
        return computerus_text_out(1, &b);
    }
    if (!computerus_truthy(state, "installed")) {
        const char *cmd;
        computerus_buf_add(&b, "Bot Desktop: packages missing " COMPUTERUS_ARROW " ");
        computerus_join(&b, missing, ", ");
        computerus_buf_add(&b, "\n  Install: ");
        cmd = computerus_str(state, "install_command", "");
        computerus_buf_add(&b, cmd[0] ? cmd : "hermes computer-use screen install");
        computerus_buf_add(&b, "\n");
        return computerus_text_out(1, &b);
    }
    if (computerus_truthy(state, "running")) {
        json_t *view = json_obj_get(state, "lease_view");
        const char *holder = json_get_str(view, "holder", "");
        computerus_buf_fmt(&b, "Bot Desktop [%s]: running on DISPLAY %s (%s), pid %lld\n",
                           computerus_str(state, "profile", ""),
                           computerus_str(state, "display", ""),
                           computerus_str(state, "geometry", ""),
                           (long long)json_get_num(state, "pid", 0.0));
        if (!strcmp(holder, "human")) {
            computerus_buf_fmt(&b, "  control: human (viewer %s)   rfb socket: %s\n",
                               json_get_str(view, "viewer_hash", ""),
                               computerus_str(state, "socket", ""));
        } else {
            computerus_buf_fmt(&b, "  control: agent   rfb socket: %s\n",
                               computerus_str(state, "socket", ""));
        }
        computerus_buf_add(&b, "  View it: Hermes Desktop " COMPUTERUS_ARROW " Bots "
                              COMPUTERUS_ARROW " this bot " COMPUTERUS_ARROW " Screen\n");
        return computerus_text_out(0, &b);
    }
    if (computerus_truthy(state, "blocker")) {
        computerus_buf_fmt(&b, "Bot Desktop [%s]: installed, not running. %s\n",
                           computerus_str(state, "profile", ""),
                           computerus_str(state, "blocker", ""));
        return computerus_text_out(1, &b);
    }
    computerus_buf_fmt(&b, "Bot Desktop [%s]: installed, not running. Start: "
                           "hermes computer-use screen start\n",
                       computerus_str(state, "profile", ""));
    return computerus_text_out(1, &b);
}

/* --- _screen_start -------------------------------------------------------- */

json_t *w21_computerus_9582b9_start(const json_t *state) {
    computerus_buf b;
    computerus_buf_init(&b);
    if (computerus_truthy(state, "error")) {
        computerus_buf_fmt(&b, "Bot Desktop: %s\n", computerus_str(state, "error", ""));
        return computerus_text_out(1, &b);
    }
    computerus_buf_fmt(&b, "Bot Desktop [%s]: running on DISPLAY %s (%s)\n",
                       computerus_str(state, "profile", ""),
                       computerus_str(state, "display", ""),
                       computerus_str(state, "geometry", ""));
    return computerus_text_out(0, &b);
}

/* --- _screen_stop --------------------------------------------------------- */

json_t *w21_computerus_9582b9_stop(const json_t *state) {
    computerus_buf b;
    computerus_buf_init(&b);
    if (computerus_truthy(state, "supported")) {
        if (!strcmp(computerus_str(state, "released_holder", ""), "human")) {
            computerus_buf_add(&b, "Bot Desktop: a human holds this screen; re-run with "
                                  "--force to take it down anyway.\n");
            return computerus_text_out(1, &b);
        }
    }
    if (computerus_truthy(state, "stopped")) {
        computerus_buf_add(&b, "Bot Desktop: stopped\n");
    } else {
        computerus_buf_add(&b, "Bot Desktop: was not running\n");
    }
    return computerus_text_out(0, &b);
}

/* --- _screen_install ------------------------------------------------------ */

json_t *w21_computerus_9582b9_install(const json_t *state) {
    computerus_buf b;
    json_t *missing = json_obj_get(state, "missing");
    computerus_buf_init(&b);

    if (!computerus_truthy(state, "supported")) {
        computerus_buf_add(&b, "Bot Desktop screens run on Linux gateway hosts only.\n");
        return computerus_text_out(1, &b);
    }
    if (!missing || missing->type != JSON_ARRAY || missing->c.count == 0) {
        computerus_buf_add(&b, "Bot Desktop: packages already installed.\n");
        return computerus_text_out(0, &b);
    }
    {
        const char *cmd = computerus_str(state, "command", "");
        if (!cmd[0]) {
            computerus_buf_add(&b, "Bot Desktop: no supported package manager "
                                  "(apt/dnf/pacman) found. Install TigerVNC (Xvnc) and "
                                  "the Xfce core (xfwm4, xfce4-panel, xfdesktop, "
                                  "xfce4-settings) by hand.\n");
            return computerus_text_out(1, &b);
        }
        computerus_buf_fmt(&b, "Bot Desktop: installing " COMPUTERUS_ARROW " %s\n", cmd);
        if (!computerus_truthy(state, "yes") && computerus_truthy(state, "tty")) {
            const char *answer = computerus_str(state, "answer", "");
            char lowered[16];
            size_t i, n;
            computerus_buf_add(&b, "Proceed? [Y/n] ");
            n = strlen(answer);
            while (n > 0 && (answer[n - 1] == ' ' || answer[n - 1] == '\t')) n--;
            while (n > 0 && (*answer == ' ' || *answer == '\t')) {
                answer++;
                n--;
            }
            if (n >= sizeof(lowered)) n = sizeof(lowered) - 1;
            for (i = 0; i < n; i++) {
                char c = answer[i];
                lowered[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            }
            lowered[n] = '\0';
            if (strcmp(lowered, "") && strcmp(lowered, "y") && strcmp(lowered, "yes")) {
                return computerus_text_out(1, &b);
            }
            /* builtins.input() consumed the line; the prompt is not re-printed. */
        }
    }
    if (computerus_truthy(state, "busy")) {
        computerus_buf_fmt(&b, "Bot Desktop: %s\n",
                           computerus_str(state, "busy_message", ""));
        return computerus_text_out(1, &b);
    }
    {
        long long rc = (long long)json_get_num(state, "rc", 0.0);
        json_t *after = json_obj_get(state, "after_missing");
        if (computerus_truthy(state, "no_sudo")) {
            const char *bare = computerus_str(state, "command", "");
            computerus_buf_add(&b, "Bot Desktop: this host has no sudo. Run as root on "
                                  "the host:\n  ");
            if (!strncmp(bare, "sudo ", 5)) bare += 5;   /* str.removeprefix("sudo ") */
            computerus_buf_add(&b, bare);
            computerus_buf_add(&b, "\n");
            return computerus_text_out(1, &b);
        }
        if (rc != 0) {
            computerus_buf_fmt(&b, "Bot Desktop: installer exited %lld\n", rc);
            return computerus_text_out((int)rc, &b);
        }
        if (after && after->type == JSON_ARRAY && after->c.count > 0) {
            computerus_buf_add(&b, "Bot Desktop: still missing ");
            computerus_join(&b, after, ", ");
            computerus_buf_add(&b, "\n");
            return computerus_text_out(1, &b);
        }
    }
    computerus_buf_add(&b, "Bot Desktop: ready. Start with `hermes computer-use screen "
                          "start` or from Hermes Desktop.\n");
    return computerus_text_out(0, &b);
}

/* --- SCREEN_ACTIONS ------------------------------------------------------- */

json_t *w21_computerus_9582b9_actions(void) {
    json_t *out = json_object();
    json_set(out, "install", json_string("_screen_install"));
    json_set(out, "start", json_string("_screen_start"));
    json_set(out, "status", json_string("_screen_status"));
    json_set(out, "stop", json_string("_screen_stop"));
    return out;
}
