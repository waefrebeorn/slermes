/*
 * gateway_chat_view.c — C home for upstream Hermes hermes_cli/gateway_chat_view.py.
 *
 * PoP: w16_gatewaycha_19399b_pending_preview     @ hermes_cli/gateway_chat_view.py:_pending_preview
 * PoP: w16_gatewaycha_19399b_quoted              @ hermes_cli/gateway_chat_view.py:_quoted
 * PoP: w16_gatewaycha_19399b_pending_lines       @ hermes_cli/gateway_chat_view.py:pending_lines
 * PoP: w16_gatewaycha_19399b_lost_admission      @ hermes_cli/gateway_chat_view.py:lost_admission
 * PoP: w16_gatewaycha_19399b_oneshot_admission   @ hermes_cli/gateway_chat_view.py:_oneshot_admission
 *
 * Pure presentation helpers. No asyncio, emitter, or stdout side effects.
 * ANSI stripping is limited to the CSI form injected by the oracle's strip_ansi seam.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w16_gatewaycha_19399b.h"

#define PREVIEW_CHARS 80

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Basic CSI stripping: ESC '[' params/intermediates then one final byte 0x40..0x7e.
 * Also drops ESC followed by one byte 0x30..0x7e. Matches the simple escape
 * sequences used by the oracle's injected strip_ansi seam. */
static char *ansi_strip_basic(const char *s) {
    size_t cap = strlen(s) + 2, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return dupstr("");
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p == 0x1b) {
            const unsigned char *q = p + 1;
            if (*q == '[') {
                q++;
                while (*q && *q >= 0x30 && *q <= 0x3f) q++; /* params */
                while (*q && *q >= 0x20 && *q <= 0x2f) q++;  /* intermediates */
                if (*q >= 0x40 && *q <= 0x7e) { p = q + 1; continue; }
            } else if (*q >= 0x30 && *q <= 0x7e) {
                p = q + 1; continue;
            }
        }
        if (n + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        out[n++] = (char)*p++;
    }
    out[n] = '\0';
    return out;
}

static char *split_join_ws(const char *s) {
    size_t cap = strlen(s) + 2, n = 0;
    char *out = (char *)malloc(cap);
    if (!out) return dupstr("");
    int need_space = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        int ws = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
        if (ws) { need_space = n > 0; ++p; continue; }
        if (need_space) { out[n++] = ' '; need_space = 0; }
        if (n + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        out[n++] = (char)c;
        ++p;
    }
    out[n] = '\0';
    return out;
}

static int ascii_printable(unsigned char c) { return c >= 0x20 && c <= 0x7e; }

static char *preview_of(const json_t *text) {
    if (!text || text->type != JSON_STRING || !text->str_val) return dupstr("");
    char *stripped = ansi_strip_basic(text->str_val);
    size_t n = strlen(stripped);
    char *folded = (char *)malloc(n + 1);
    if (!folded) { free(stripped); return dupstr(""); }
    for (size_t i = 0; i < n; ++i)
        folded[i] = ascii_printable((unsigned char)stripped[i]) ? stripped[i] : ' ';
    folded[n] = '\0';
    free(stripped);
    char *line = split_join_ws(folded);
    free(folded);
    if (strlen(line) > PREVIEW_CHARS) {
        line[PREVIEW_CHARS - 1] = (char)0xE2;
        line[PREVIEW_CHARS]     = (char)0x80;
        line[PREVIEW_CHARS + 1] = (char)0xA6;
        line[PREVIEW_CHARS + 2] = '\0';
    }
    return line;
}

json_t *w16_gatewaycha_19399b_pending_preview(const json_t *text) {
    char *p = preview_of(text);
    json_t *out = json_string(p);
    free(p);
    return out;
}

static const char *row_str(const json_t *row, const char *key) {
    json_t *v = row ? json_obj_get(row, key) : NULL;
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static char *quoted_of(const json_t *row, const char *fallback) {
    char *preview = preview_of(row ? json_obj_get(row, "text") : NULL);
    size_t cap = strlen(preview) + strlen(fallback) + 64;
    char *out = (char *)malloc(cap);
    if (!out) { free(preview); return dupstr(""); }
    if (preview[0])
        snprintf(out, cap, "    \"%s\"", preview);
    else
        snprintf(out, cap, "    (%s %s)", fallback, row_str(row, "admission_id"));
    free(preview);
    return out;
}

json_t *w16_gatewaycha_19399b_quoted(const json_t *row, const char *fallback) {
    char *q = quoted_of(row, fallback ? fallback : "");
    json_t *out = json_string(q);
    free(q);
    return out;
}

json_t *w16_gatewaycha_19399b_pending_lines(const json_t *pending, int interactive) {
    json_t *lines = json_array();
    if (!pending || pending->type != JSON_ARRAY) return lines;

    const json_t *lost[4096]; size_t nlost = 0;
    const json_t *queued[4096]; size_t nqueued = 0;
    for (size_t i = 0; i < json_len(pending); ++i) {
        const json_t *row = json_get(pending, i);
        if (!row || row->type != JSON_OBJECT) continue;
        const char *st = row_str(row, "status");
        if (!strcmp(st, "unknown") && nlost < 4096) lost[nlost++] = row;
        else if (!strcmp(st, "queued") && nqueued < 4096) queued[nqueued++] = row;
    }
    char buf[8192];
    if (nlost == 1) {
        json_append(lines, json_string("⏸ This chat is paused. Hermes couldn't confirm how the last turn ended (usually because it restarted mid-turn):"));
        char *p = preview_of(json_obj_get(lost[0], "text"));
        if (p[0]) { char *q = quoted_of(lost[0], "lost turn"); json_append(lines, json_string(q)); free(q); }
        free(p);
        json_append(lines, json_string("  Hermes won't re-run it by itself, since it may already have done part of the work."));
        if (interactive) {
            json_append(lines, json_string("  To drop it and continue, type:"));
            snprintf(buf, sizeof buf, "/discard %s", row_str(lost[0], "admission_id"));
            json_append(lines, json_string(buf));
        }
    } else if (nlost > 1) {
        snprintf(buf, sizeof buf, "⏸ This chat is paused. Hermes couldn't confirm how %zu turns ended (usually because it restarted mid-turn):", nlost);
        json_append(lines, json_string(buf));
        json_append(lines, json_string("  Hermes won't re-run them by itself, since they may already have done part of the work."));
        if (interactive)
            json_append(lines, json_string("  To continue, drop each one with the /discard line under it:"));
        for (size_t i = 0; i < nlost; ++i) {
            char *q = quoted_of(lost[i], "lost turn");
            json_append(lines, json_string(q));
            free(q);
            if (interactive) {
                snprintf(buf, sizeof buf, "/discard %s", row_str(lost[i], "admission_id"));
                json_append(lines, json_string(buf));
            }
        }
    }
    if (nqueued) {
        const char *count = nqueued == 1 ? "1 message is" : NULL;
        char countbuf[64];
        if (!count) { snprintf(countbuf, sizeof countbuf, "%zu messages are", nqueued); count = countbuf; }
        const char *after = nlost == 0 ? "to run" :
                            (nlost == 1 ? "and will run once that turn is dropped" :
                                          "and will run once those turns are dropped");
        snprintf(buf, sizeof buf, "  %s waiting %s:", count, after);
        json_append(lines, json_string(buf));
        for (size_t i = 0; i < nqueued; ++i) {
            char *q = quoted_of(queued[i], "queued message");
            json_append(lines, json_string(q));
            free(q);
        }
    }
    return lines;
}

static json_t *raise_marker(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg));
    return o;
}

json_t *w16_gatewaycha_19399b_lost_admission(const json_t *snapshot, const char *admission_id) {
    if (snapshot && snapshot->type == JSON_OBJECT) {
        json_t *p = json_obj_get(snapshot, "pending");
        if (p && p->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(p); ++i) {
                json_t *row = json_get(p, i);
                if (!row || row->type != JSON_OBJECT) continue;
                const char *id = row_str(row, "admission_id");
                const char *st = row_str(row, "status");
                if (id && admission_id && !strcmp(id, admission_id) && !strcmp(st, "unknown"))
                    return json_copy(row);
            }
        }
    }
    return raise_marker("GatewayClientError",
            "No unknown (lost) admission with that id; resume to refresh");
}

json_t *w16_gatewaycha_19399b_oneshot_admission(const json_t *receipt) {
    if (!receipt || receipt->type == JSON_NULL)
        return raise_marker("GatewayClientError", "One-shot requires a query");
    json_t *vs = json_obj_get(receipt, "voice_stopped");
    if (vs && vs->type == JSON_BOOL && vs->bool_val) return json_null();
    json_t *id = json_obj_get(receipt, "admission_id");
    if (id && id->type == JSON_STRING) return json_copy(id);
    return raise_marker("KeyError", "'admission_id'");
}
