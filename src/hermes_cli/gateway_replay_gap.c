/*
 * gateway_replay_gap.c — faithful C11 port of hermes_cli/gateway_replay_gap.py
 * (gate w17_gatewayrep_9bc643).
 *
 * PoP: w17_gatewayrep_9bc643_pending_status @ hermes_cli/gateway_replay_gap.py:pending_status
 * PoP: w17_gatewayrep_9bc643_settled @ hermes_cli/gateway_replay_gap.py:settled
 * PoP: w17_gatewayrep_9bc643_owed_reply @ hermes_cli/gateway_replay_gap.py:owed_reply
 *
 * owed_reply slices final at len(prefix) BYTES. Because each candidate prefix
 * is itself a valid UTF-8 prefix taken from shown/final, that byte offset always
 * lands on a lead byte (or one past the end), never inside a multibyte codepoint.
 */
#include "noc_w17_gatewayrep_9bc643.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESUMED "[output resumed]"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *str_of(const json_t *v) {
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

json_t *w17_gatewayrep_9bc643_pending_status(const json_t *pending, const char *admission) {
    size_t i, n = pending && pending->type == JSON_ARRAY ? pending->c.count : 0;
    for (i = 0; i < n; ++i) {
        const json_t *row = pending->c.items[i];
        const char *aid;
        if (!row || row->type != JSON_OBJECT) continue;
        aid = str_of(json_obj_get(row, "admission_id"));
        if (aid && admission && !strcmp(aid, admission)) {
            const json_t *st = json_obj_get(row, "status");
            return st ? json_copy(st) : json_null();
        }
    }
    return json_null();
}

json_t *w17_gatewayrep_9bc643_settled(const json_t *receipt) {
    const char *status, *outcome;
    if (!receipt || receipt->type != JSON_OBJECT) return json_bool(0);
    status = str_of(json_obj_get(receipt, "status"));
    if (!status || strcmp(status, "terminal")) return json_bool(0);
    outcome = str_of(json_obj_get(receipt, "outcome"));
    if ((outcome && (!strcmp(outcome, "cancelled") || !strcmp(outcome, "interrupted") ||
                     !strcmp(outcome, "failed"))) || json_obj_get(receipt, "result"))
        return json_bool(1);
    return json_null();
}

static unsigned int utf8_decode(const unsigned char *p, size_t avail, size_t *used) {
    unsigned char b0 = p[0];
    if (b0 < 0x80) { *used = 1; return b0; }
    if ((b0 & 0xE0) == 0xC0 && avail >= 2) {
        *used = 2; return ((unsigned int)(b0 & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && avail >= 3) {
        *used = 3; return ((unsigned int)(b0 & 0x0F) << 12) |
                          ((unsigned int)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && avail >= 4) {
        *used = 4; return ((unsigned int)(b0 & 0x07) << 18) |
                          ((unsigned int)(p[1] & 0x3F) << 12) |
                          ((unsigned int)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    *used = 1;
    return b0;
}

static int cp_is_ws(unsigned int c) {
    return c == ' ' || (c >= 0x09 && c <= 0x0D) || c == 0x85 || c == 0xA0 ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 ||
           c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

/* str.rstrip() with no argument strips Unicode whitespace code points. */
static size_t rstrip_ws_len(const char *s, size_t n) {
    const unsigned char *b = (const unsigned char *)s;
    while (n > 0) {
        size_t start = n - 1, used = 1;
        unsigned int cp;
        while (start > 0 && (b[start] & 0xC0) == 0x80) start--;
        cp = utf8_decode(b + start, n - start, &used);
        if (!cp_is_ws(cp)) break;
        n = start;
    }
    return n;
}

json_t *w17_gatewayrep_9bc643_owed_reply(const char *shown, const char *final) {
    const char *head = shown ? shown : "";
    const char *tail = final ? final : "";
    size_t hlen = strlen(head), flen = strlen(tail);
    int which;
    if (hlen >= 2 && head[0] == '\n' && head[1] == '\n') head += 2;
    hlen = strlen(head);
    for (which = 0; which < 2; ++which) {
        size_t plen = which == 0 ? hlen : rstrip_ws_len(head, hlen);
        if (plen <= flen && !memcmp(tail, head, plen)) {
            char *suffix = dupstr(tail + plen);   /* plen lands on a UTF-8 lead byte */
            json_t *out = json_string(suffix ? suffix : "");
            free(suffix);
            return out;
        }
    }
    {
        size_t total = strlen("\n" RESUMED "\n") + flen + 1;
        char *marker = (char *)malloc(total);
        json_t *out;
        if (!marker) return json_null();
        snprintf(marker, total, "\n%s\n%s", RESUMED, tail);
        out = json_string(marker);
        free(marker);
        return out;
    }
}
