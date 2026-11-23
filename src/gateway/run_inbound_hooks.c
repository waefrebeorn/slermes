/*
 * run_inbound_hooks.c — C home for upstream Hermes gateway/run_inbound_hooks.py.
 *
 * PoP: w11_runinbound_903938_expand_alias @ gateway/run_inbound_hooks.py:GatewayInboundHooksMixin._hm_expand_alias_quick_command
 */
#include "noc_w11_runinbound_903938.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() on ASCII whitespace. */
static char *strip(const char *s) {
    if (!s) return strdup("");
    while (*s && is_ws(*s)) ++s;
    size_t n = strlen(s);
    while (n && is_ws(s[n - 1])) --n;
    char *o = (char *)malloc(n + 1);
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

struct json_t *w11_runinbound_903938_expand_alias(const struct json_t *qcmd,
                                                   const char *command_args,
                                                   const char *event_text) {
    json_t *out = json_object();
    const char *tv = json_get_str(qcmd, "target", NULL);
    char *target = strip(tv); /* (qcmd.get("target") or "").strip() */

    if (*target == '\0') { /* not target -> None; event.text unchanged */
        json_set(out, "command", json_null());
        json_set(out, "text", json_string(event_text ? event_text : ""));
        free(target);
        return out;
    }

    char *t;
    if (target[0] == '/') {
        t = strdup(target);
    } else {
        t = (char *)malloc(strlen(target) + 2);
        t[0] = '/';
        strcpy(t + 1, target);
    }
    free(target);

    /* event.text = f"{target} {event.get_command_args().strip()}".strip() */
    char *args = strip(command_args);
    size_t raw_len = strlen(t) + 1 + strlen(args) + 1;
    char *raw = (char *)malloc(raw_len);
    snprintf(raw, raw_len, "%s %s", t, args);
    char *text = strip(raw);
    json_set(out, "text", json_string(text));

    /* target_command = target.lstrip("/"); return target_command.split()[0] if ... else ... */
    const char *tc = t;
    while (*tc == '/') ++tc;
    const char *p = tc;
    while (*p && is_ws(*p)) ++p; /* split() ignores leading whitespace */
    if (!*p) {
        json_set(out, "command", json_string(""));
    } else {
        const char *e = p;
        while (*e && !is_ws(*e)) ++e;
        size_t n = (size_t)(e - p);
        char *cmd = (char *)malloc(n + 1);
        memcpy(cmd, p, n);
        cmd[n] = '\0';
        json_set(out, "command", json_string(cmd));
        free(cmd);
    }

    free(text);
    free(raw);
    free(args);
    free(t);
    return out;
}
