/*
 * gateway_command_errors.c -- real C11 port of upstream Hermes
 *   hermes_cli/gateway_command_errors.py.
 *
 * PoP: w22_gatewaycom_7a0702_verb_for @ hermes_cli/gateway_command_errors.py:_verb_for
 * PoP: w22_gatewaycom_7a0702_explain @ hermes_cli/gateway_command_errors.py:explain_service_failure
 *
 * The two exception shapes the table knows (SystemctlUnavailableError, and a
 * non-zero subprocess.CalledProcessError) are carried in as data: `kind`,
 * `cmd` and `returncode`. Everything the module does with them -- the verb
 * scan, the str.format() of the three guidance lines, and str(exc) for the
 * Details line -- is computed here.
 */
#include "hermes_json.h"
#include "noc_w22_gatewaycom_7a0702.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const JOURNAL_HINT =
    "journalctl --user -u hermes-gateway --since \"5 min ago\"";

static char *gce_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* One element of a buffer builder; terminates on the empty path. */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int bad;
} gce_buf;

static void gce_buf_init(gce_buf *b) {
    b->cap = 128;
    b->len = 0;
    b->bad = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
    else b->bad = 1;
}

static void gce_buf_add(gce_buf *b, const char *s, size_t n) {
    if (b->bad) return;
    if (!s || n == 0) return;             /* empty path: nothing to append */
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) { b->bad = 1; return; }
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';                /* NUL-terminate after every append */
}

static void gce_buf_adds(gce_buf *b, const char *s) {
    if (!s) return;
    gce_buf_add(b, s, strlen(s));
}

/* Trailing "\0" of the buffer, or NULL when the build failed. */
static char *gce_buf_finish(gce_buf *b) {
    if (b->bad) { free(b->buf); b->buf = NULL; return NULL; }
    b->buf[b->len] = '\0';
    return b->buf;
}

/* --- Python str repr for the ASCII identifiers a command line carries ----- */

static void gce_repr_body(gce_buf *b, const char *s, char quote) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p++;
        if (c == (unsigned char)quote || c == '\\') {
            char esc[2];
            esc[0] = '\\';
            esc[1] = (char)c;
            gce_buf_add(b, esc, 2);
        } else if (c == '\n') {
            gce_buf_adds(b, "\\n");
        } else if (c == '\r') {
            gce_buf_adds(b, "\\r");
        } else if (c == '\t') {
            gce_buf_adds(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)c);
            gce_buf_adds(b, esc);
        } else {
            gce_buf_add(b, (const char *)&c, 1);
        }
    }
}

/* Python's `repr(str)`: single quotes unless that would need escaping and a
 * double quote would not. */
static char *gce_repr_str(const char *s) {
    gce_buf b;
    char quote = '\'';
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    gce_buf_init(&b);
    gce_buf_add(&b, &quote, 1);
    gce_repr_body(&b, s, quote);
    gce_buf_add(&b, &quote, 1);
    return gce_buf_finish(&b);
}

/* repr() of one non-container value as the command line would carry it. */
static char *gce_repr_value(const json_t *v) {
    char num[64];
    if (!v) return gce_dup("None");
    switch (v->type) {
    case JSON_NULL:   return gce_dup("None");
    case JSON_BOOL:   return gce_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return gce_repr_str(v->str_val);
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(num, sizeof(num), "%lld", (long long)v->num_val);
        } else {
            snprintf(num, sizeof(num), "%.17g", v->num_val);
        }
        return gce_dup(num);
    default:
        return gce_dup("None");
    }
}

/* str(exc.cmd): a list renders as "[a, b]", a string as itself, None as
 * "None", any other scalar through repr(). */
static char *gce_str_cmd(const json_t *cmd) {
    gce_buf b;
    size_t i;
    if (!cmd) return gce_dup("None");
    if (cmd->type == JSON_ARRAY) {
        gce_buf_init(&b);
        gce_buf_adds(&b, "[");
        for (i = 0; i < cmd->c.count; i++) {
            char *item = gce_repr_value(cmd->c.items[i]);
            if (i) gce_buf_adds(&b, ", ");
            gce_buf_adds(&b, item ? item : "");
            free(item);
        }
        gce_buf_adds(&b, "]");
        return gce_buf_finish(&b);
    }
    if (cmd->type == JSON_STRING) return gce_dup(cmd->str_val ? cmd->str_val : "");
    return gce_repr_value(cmd);
}

/* --- _verb_for ------------------------------------------------------------ */

static int gce_is_verb(const char *token) {
    return !strcmp(token, "start") || !strcmp(token, "stop") || !strcmp(token, "restart");
}

static const char *gce_verb_name(const char *token) {
    if (!strcmp(token, "stop")) return "stop";
    if (!strcmp(token, "restart")) return "restart";
    return "start";
}

const char *w22_gatewaycom_7a0702_verb_for(const json_t *cmd) {
    size_t i;
    if (!cmd) return "start";
    if (cmd->type == JSON_ARRAY) {
        for (i = 0; i < cmd->c.count; i++) {
            json_t *item = cmd->c.items[i];
            if (item && item->type == JSON_STRING && item->str_val && gce_is_verb(item->str_val)) {
                return gce_verb_name(item->str_val);
            }
        }
        return "start";
    }
    if (cmd->type == JSON_STRING && cmd->str_val) {
        /* str(cmd).split(): whitespace-separated tokens, first verb wins. */
        const char *p = cmd->str_val;
        char token[128];
        while (*p) {
            size_t n = 0;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
            if (!*p) break;
            while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v')) {
                if (n + 1 < sizeof(token)) token[n++] = *p;
                p++;
            }
            token[n] = '\0';              /* NUL-terminate the shortened token */
            if (gce_is_verb(token)) return gce_verb_name(token);
        }
    }
    return "start";
}

/* --- explain_service_failure --------------------------------------------- */

static long long gce_returncode(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

/* Python's "died with %r." line for a negative return code. CPython maps it
 * through signal.Signals; the names below are the members this host's
 * interpreter enum carries, and any value outside it renders "unknown
 * signal N" -- exactly what the live str() prints here. */
static const char *gce_signal_name(int sig) {
    switch (sig) {
    case 1:  return "CTRL_BREAK_EVENT";
    case 2:  return "SIGINT";
    case 4:  return "SIGILL";
    case 8:  return "SIGFPE";
    case 11: return "SIGSEGV";
    case 15: return "SIGTERM";
    case 21: return "SIGBREAK";
    case 22: return "SIGABRT";
    default: return NULL;
    }
}

static json_t *gce_lines(int unavailable, const char *verb, const char *details) {
    json_t *arr = json_array();
    char line[1024];
    if (unavailable) {
        json_append(arr, json_string(
            "This system has no systemd, so Hermes cannot install a background service here."));
        json_append(arr, json_string(
            "Run the gateway directly with `hermes gateway run` (keep it alive with tmux or screen)."));
        return arr;
    }
    snprintf(line, sizeof(line),
             "Could not %s the gateway service; systemd reported an error.",
             verb ? verb : "start");
    json_append(arr, json_string(line));
    snprintf(line, sizeof(line),
             "See why with `hermes gateway status --deep` or `%s`.", JOURNAL_HINT);
    json_append(arr, json_string(line));
    json_append(arr, json_string(
        "To reinstall the service run `hermes gateway install --force`."));
    snprintf(line, sizeof(line), "Details: %s", details ? details : "");
    json_append(arr, json_string(line));
    return arr;
}

json_t *w22_gatewaycom_7a0702_explain(const char *kind, const json_t *cmd,
                                      const json_t *returncode) {
    char *cmd_text;
    char *details;
    long long rc;
    json_t *out;
    gce_buf b;

    if (kind && !strcmp(kind, "unavailable")) return gce_lines(1, NULL, NULL);
    if (!kind || strcmp(kind, "cpe")) return json_null();

    rc = gce_returncode(returncode);
    cmd_text = gce_str_cmd(cmd);
    gce_buf_init(&b);
    gce_buf_adds(&b, "Command '");
    gce_buf_adds(&b, cmd_text ? cmd_text : "None");
    if (rc < 0) {
        const char *name = gce_signal_name((int)(-rc));
        char num[32];
        snprintf(num, sizeof(num), "%lld", -rc);
        if (name) {
            gce_buf_adds(&b, "' died with <Signals.");
            gce_buf_adds(&b, name);
            gce_buf_adds(&b, ": ");
            gce_buf_adds(&b, num);
            gce_buf_adds(&b, ">.");
        } else {
            gce_buf_adds(&b, "' died with unknown signal ");
            gce_buf_adds(&b, num);
            gce_buf_adds(&b, ".");
        }
    } else {
        char num[32];
        snprintf(num, sizeof(num), "%lld", rc);
        gce_buf_adds(&b, "' returned non-zero exit status ");
        gce_buf_adds(&b, num);
        gce_buf_adds(&b, ".");
    }
    details = gce_buf_finish(&b);
    out = gce_lines(0, w22_gatewaycom_7a0702_verb_for(cmd), details ? details : "");
    free(details);
    free(cmd_text);
    return out;
}
