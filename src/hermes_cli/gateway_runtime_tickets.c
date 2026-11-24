/*
 * gateway_runtime_tickets.c - C11 port of upstream
 * hermes_cli/gateway_runtime_tickets.py (gate w26_gatewayrun_1eda26).
 *
 * Ported value->value: the one-channel ticket protocol's request validator and
 * reply shape. Proven by the differential oracle gate w26_gatewayrun_1eda26.
 * PoP: w26_gatewayrun_1eda26_request_lines @ hermes_cli/gateway_runtime_tickets.py:_request_lines
 * PoP: w26_gatewayrun_1eda26_reply @ hermes_cli/gateway_runtime_tickets.py:_reply
 * PoP: w26_gatewayrun_1eda26_serve @ hermes_cli/gateway_runtime_tickets.py:cmd_gateway_tickets
 * PoP: w26_gatewayrun_1eda26_constants @ hermes_cli/gateway_runtime_tickets.py:<literals>
 *
 * Seams: the profile registry (a name -> home table) and `mint` (the control
 * socket).  Everything else -- including the 64 KiB line refusal and every
 * error code -- is decided here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "noc_w26_gatewayrun_1eda26.h"

#define GT_MAX_LINE W26_GATEWAYRUN_1EDA26_MAX_REQUEST_LINE_BYTES
#define GT_LIMIT    (GT_MAX_LINE + 2)
#define GT_TTL_SECONDS 30

static const char *const GT_PURPOSES[W26_GATEWAYRUN_1EDA26_PURPOSE_COUNT] = {
    "native-http", "native-profile-create", "native-profile-registry", "interactive",
};

/* ---------------------------------------------------------------- helpers */

/* BytesIO.readline(limit): at most `limit` bytes, stopping after a newline. */
static size_t gt_readline(const char *buf, size_t total, size_t *pos, const char **out) {
    size_t start = *pos, n = 0;
    if (start >= total) { *out = NULL; return 0; }
    while (*pos < total && n < (size_t)GT_LIMIT) {
        char c = buf[*pos];
        (*pos)++;
        n++;
        if (c == '\n') break;
    }
    *out = buf + start;
    return n;
}

static int gt_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *gt_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (gt_is_space(*b)) b++;
    e = b + strlen(b);
    while (e > b && gt_is_space(e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

static int gt_purpose_index(const char *p) {
    int i;
    if (!p) return -1;
    for (i = 0; i < W26_GATEWAYRUN_1EDA26_PURPOSE_COUNT; i++)
        if (!strcmp(p, GT_PURPOSES[i])) return i;
    return -1;
}

static json_t *gt_refuse(json_t *request_id, const char *code, const char *message) {
    json_t *out = json_object();
    json_t *err = json_object();
    json_set(err, "code", json_string(code));
    json_set(err, "message", json_string(message));
    json_set(out, "id", request_id ? request_id : json_null());
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", err);
    return out;
}

/* ---------------------------------------------------------------- entries */

json_t *w26_gatewayrun_1eda26_request_lines(const json_t *in) {
    const json_t *wv = json_obj_get(in, "wire");
    const char *wire = (wv && wv->type == JSON_STRING && wv->str_val) ? wv->str_val : "";
    size_t total = strlen(wire), pos = 0;
    json_t *out = json_array();

    for (;;) {
        const char *line = NULL;
        size_t len = gt_readline(wire, total, &pos, &line);
        size_t blen;
        int over = 0;
        if (len == 0) break;
        while (len == (size_t)GT_LIMIT && line[len - 1] != '\n') {
            over = 1;
            len = gt_readline(wire, total, &pos, &line);
            if (len == 0) { line = ""; break; }
        }
        blen = len;
        if (blen && line[blen - 1] == '\n') blen--;
        if (blen && line[blen - 1] == '\r') blen--;
        if (over || blen > (size_t)GT_MAX_LINE) {
            json_append(out, json_null());
        } else {
            char *body = (char *)malloc(blen + 1);
            if (!body) { json_append(out, json_null()); continue; }
            memcpy(body, line, blen);
            body[blen] = '\0';
            json_append(out, json_string(body));
            free(body);
        }
    }
    return out;
}

/* The profile registry seam: {"existing": {name: home}, "invalid": [name, ...]} */
static void gt_profile_home(const json_t *in, const char *name, char **home_out,
                            int *found, int *invalid) {
    const json_t *profiles = json_obj_get(in, "profiles");
    const json_t *existing = profiles ? json_obj_get(profiles, "existing") : NULL;
    const json_t *bad = profiles ? json_obj_get(profiles, "invalid") : NULL;
    char *trimmed = gt_strip(name);
    size_t i;

    *found = 0;
    *invalid = 0;
    *home_out = NULL;
    if (!trimmed) return;
    if (bad && bad->type == JSON_ARRAY) {
        for (i = 0; i < json_len(bad); i++) {
            const json_t *b = json_get(bad, i);
            if (b && b->type == JSON_STRING && b->str_val && !strcmp(b->str_val, trimmed)) {
                *invalid = 1;
                free(trimmed);
                return;
            }
        }
    }
    if (existing && existing->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(existing, trimmed);
        if (v && v->type == JSON_STRING && v->str_val) {
            *found = 1;
            *home_out = (char *)malloc(strlen(v->str_val) + 1);
            if (*home_out) strcpy(*home_out, v->str_val);
        }
    }
    free(trimmed);
}

/* The mint seam: {"ticket": str} or {"raise": reason[, "detail": str]}. */
static json_t *gt_reply_line(const json_t *in, const char *line) {
    const json_t *mint = json_obj_get(in, "mint");
    const char *mint_raise = mint ? json_get_str(mint, "raise", NULL) : NULL;
    json_t *request = NULL;
    json_t *request_id = NULL;
    json_t *out = NULL;
    const char *purpose;
    const json_t *profile_v;
    char *home = NULL;
    int found = 0, invalid = 0;

    if (!line) return gt_refuse(NULL, "invalid_request",
                                "request line is longer than 65536 bytes");
    request = json_parse(line, NULL);
    if (!request) return gt_refuse(NULL, "invalid_request", "request line is not JSON");
    if (request->type != JSON_OBJECT) {
        json_free(request);
        return gt_refuse(NULL, "invalid_request", "request must be a JSON object");
    }

    /* id must be an int or a str -- a bool is explicitly not. */
    {
        const json_t *rid = json_obj_get(request, "id");
        int ok = 0;
        if (rid && rid->type == JSON_STRING) ok = 1;
        else if (rid && rid->type == JSON_NUMBER &&
                 rid->num_val == (double)(long long)rid->num_val) ok = 1;
        if (!ok) {
            json_free(request);
            return gt_refuse(NULL, "invalid_request", "id must be an integer or a string");
        }
        request_id = json_copy(rid);
    }

    purpose = json_get_str(request, "purpose", NULL);
    if (!purpose || gt_purpose_index(purpose) < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "purpose must be one of: %s, %s, %s, %s",
                 GT_PURPOSES[0], GT_PURPOSES[1], GT_PURPOSES[2], GT_PURPOSES[3]);
        out = gt_refuse(request_id, "unsupported_purpose", msg);
        json_free(request);
        return out;
    }

    profile_v = json_obj_get(request, "profile");   /* present, even as null */
    if (!strcmp(purpose, "interactive") && profile_v) {
        out = gt_refuse(request_id, "invalid_request",
                        "an interactive ticket takes no profile: it serves every one");
        json_free(request);
        return out;
    }
    if (profile_v) {
        if (profile_v->type != JSON_STRING) {
            out = gt_refuse(request_id, "invalid_request", "profile must be a profile name");
            json_free(request);
            return out;
        }
        gt_profile_home(in, profile_v->str_val, &home, &found, &invalid);
        free(home);
        if (invalid) {
            out = gt_refuse(request_id, "invalid_request", "profile must be a profile name");
            json_free(request);
            return out;
        }
        if (!found) {
            out = gt_refuse(request_id, "unknown_profile", "no such profile on this host");
            json_free(request);
            return out;
        }
    }

    if (mint_raise && mint_raise[0]) {
        if (!strcmp(mint_raise, "profile_not_served")) {
            out = gt_refuse(request_id, "profile_not_served",
                            "the running gateway does not serve this profile");
        } else {
            const char *detail = json_get_str(mint, "detail", NULL);
            char msg[512];
            if (detail && detail[0])
                snprintf(msg, sizeof(msg), "gateway refused the ticket: %s (%s)",
                         mint_raise, detail);
            else
                snprintf(msg, sizeof(msg), "gateway refused the ticket: %s", mint_raise);
            out = gt_refuse(request_id, "ticket_unavailable", msg);
        }
        json_free(request);
        return out;
    }

    out = json_object();
    json_set(out, "id", request_id ? request_id : json_null());
    json_set(out, "ok", json_bool(1));
    json_set(out, "ticket", json_string(json_get_str(mint, "ticket", "TICKET")));
    json_set(out, "expires_in", json_number(GT_TTL_SECONDS));
    json_free(request);
    return out;
}

json_t *w26_gatewayrun_1eda26_reply(const json_t *in) {
    const json_t *lv = json_obj_get(in, "line");
    if (!lv || lv->type != JSON_STRING) return gt_reply_line(in, NULL);
    return gt_reply_line(in, lv->str_val ? lv->str_val : "");
}

json_t *w26_gatewayrun_1eda26_serve(const json_t *in) {
    json_t *lines = w26_gatewayrun_1eda26_request_lines(in);
    json_t *out = json_array();
    size_t i, n = json_len(lines);

    for (i = 0; i < n; i++) {
        const json_t *l = json_get(lines, i);
        const char *body = (l && l->type == JSON_STRING) ? l->str_val : NULL;
        json_append(out, gt_reply_line(in, body));
    }
    json_free(lines);
    return out;
}

json_t *w26_gatewayrun_1eda26_constants(void) {
    json_t *o = json_object();
    json_t *purposes = json_array();
    int i;
    for (i = 0; i < W26_GATEWAYRUN_1EDA26_PURPOSE_COUNT; i++)
        json_append(purposes, json_string(GT_PURPOSES[i]));
    json_set(o, "protocol", json_number(W26_GATEWAYRUN_1EDA26_PROTOCOL));
    json_set(o, "purposes", purposes);
    json_set(o, "max_request_line_bytes", json_number(W26_GATEWAYRUN_1EDA26_MAX_REQUEST_LINE_BYTES));
    json_set(o, "profile_not_served", json_string("profile_not_served"));
    json_set(o, "ticket_ttl_seconds", json_number(GT_TTL_SECONDS));
    return o;
}
