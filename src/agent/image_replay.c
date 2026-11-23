/*
 * image_replay.c — faithful C11 port of upstream Hermes agent/image_replay.py.
 *
 * PoP: imagereplay_resolve_recent @ agent/image_replay.py:resolve_replay_recent_images
 * PoP: imagereplay_frontier @ agent/image_replay.py:replay_frontier
 * PoP: imagereplay_split_row @ agent/image_replay.py:_split_rebuilt_row
 * PoP: imagereplay_replay_recent @ agent/image_replay.py:replay_recent_images
 *
 * Filesystem/agent-side helpers (_retained_data_url, _turn_replay_window,
 * replay_for_agent) are resolved by the caller; the data URL map is injected.
 */
#include "noc_w6_imagereplay_7778b4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define IR_DEFAULT_REPLAY_RECENT_IMAGES 3

static char *ir_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

json_t *imagereplay_resolve_recent(const json_t *cfg) {
    int raw_is_default = 1;
    double raw_val = IR_DEFAULT_REPLAY_RECENT_IMAGES;
    int valid = 1; /* default is a valid non-negative int */
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *vision = json_obj_get(cfg, "vision");
        if (vision && vision->type == JSON_OBJECT) {
            json_t *raw = json_obj_get(vision, "replay_recent_images");
            if (raw) {
                raw_is_default = 0;
                if (raw->type == JSON_NULL) {
                    raw_is_default = 1;              /* raw is None -> default */
                } else if (raw->type == JSON_NUMBER &&
                           raw->num_val == floor(raw->num_val) && raw->num_val >= 0) {
                    raw_val = raw->num_val;
                } else {
                    valid = 0;                        /* bool/str/negative -> warn, 0 */
                }
            }
        }
    }
    (void)raw_is_default;
    return json_number(valid ? raw_val : 0);
}

json_t *imagereplay_frontier(double total, double keep) {
    if (keep <= 0 || total <= keep) return json_number(0);
    double q = floor((total - keep) / keep);
    return json_number(q * keep);
}

/* --- line helpers --- */
static char **ir_split_lines(const char *content, size_t *count) {
    size_t cap = 8, n = 0;
    char **lines = (char **)malloc(cap * sizeof(char *));
    if (!lines) { *count = 0; return NULL; }
    const char *p = content ? content : "";
    const char *start = p;
    for (; *p; p++) {
        if (*p == '\n') {
            if (n == cap) { cap *= 2; lines = (char **)realloc(lines, cap * sizeof(char *)); }
            lines[n++] = ir_ndup(start, (size_t)(p - start));
            start = p + 1;
        }
    }
    if (n == cap) { cap *= 2; lines = (char **)realloc(lines, cap * sizeof(char *)); }
    lines[n++] = ir_ndup(start, (size_t)(p - start)); /* trailing empty line kept */
    *count = n;
    return lines;
}
/* _MARKER_RE: ^\[screenshot( detail=(low|high|auto))?\]$ */
static int ir_marker(const char *line, const char **detail) {
    if (!strcmp(line, "[screenshot]")) { *detail = NULL; return 1; }
    const char *pre = "[screenshot detail=";
    size_t pl = strlen(pre);
    size_t n = strlen(line);
    if (n > pl && strncmp(line, pre, pl) == 0 && line[n - 1] == ']') {
        const char *inner = line + pl;
        size_t il = n - pl - 1;
        if ((il == 3 && !memcmp(inner, "low", 3)) ||
            (il == 4 && (!memcmp(inner, "high", 4) || !memcmp(inner, "auto", 4)))) {
            *detail = ir_ndup(inner, il);   /* copy: the source line keeps its ']' */
            return 1;
        }
    }
    return 0;
}
/* _HINT_RE: ^\[Image attached( at)?: (?P<ref>.+)\]$ */
static int ir_hint(const char *line, char **ref_out, int *local_out) {
    const char *pre = "[Image attached";
    size_t pl = strlen(pre);
    if (strncmp(line, pre, pl) != 0) return 0;
    const char *p = line + pl;
    int local = 0;
    if (strncmp(p, " at", 3) == 0) { local = 1; p += 3; }
    if (strncmp(p, ": ", 2) != 0) return 0;
    p += 2;
    size_t n = strlen(p);
    if (n < 2 || p[n - 1] != ']') return 0;   /* ref must be non-empty */
    *ref_out = ir_ndup(p, n - 1);             /* drop trailing ']' */
    *local_out = local;
    return 1;
}

json_t *imagereplay_split_row(const char *content) {
    size_t nlines = 0;
    char **lines = ir_split_lines(content, &nlines);
    size_t *mark_i = (size_t *)malloc((nlines ? nlines : 1) * sizeof(size_t));
    const char **mark_d = (const char **)malloc((nlines ? nlines : 1) * sizeof(char *));
    char **hint_ref = (char **)malloc((nlines ? nlines : 1) * sizeof(char *));
    int *hint_local = (int *)malloc((nlines ? nlines : 1) * sizeof(int));
    size_t nm = 0, nh = 0;
    for (size_t i = 0; i < nlines; i++) {
        const char *d = NULL;
        if (ir_marker(lines[i], &d)) { mark_i[nm] = i; mark_d[nm] = d; nm++; }
        char *r = NULL; int loc = 0;
        if (ir_hint(lines[i], &r, &loc)) { hint_ref[nh] = r; hint_local[nh] = loc; nh++; }
    }
    if (nm == 0 || nh != nm) {
        for (size_t i = 0; i < nm; i++) free((void *)mark_d[i]);
        for (size_t i = 0; i < nh; i++) free(hint_ref[i]);
        for (size_t i = 0; i < nlines; i++) free(lines[i]);
        free(lines); free(mark_i); free(mark_d); free(hint_ref); free(hint_local);
        return json_null();
    }
    json_t *parts = json_array();
    size_t start = 0;
    for (size_t k = 0; k < nm; k++) {
        size_t i = mark_i[k];
        if (i > start) {
            /* "\n".join(lines[start:i]) */
            size_t len = 0;
            for (size_t j = start; j < i; j++) len += strlen(lines[j]) + (j + 1 < i ? 1 : 0);
            char *buf = (char *)malloc(len + 1);
            size_t at = 0;
            for (size_t j = start; j < i; j++) {
                size_t l = strlen(lines[j]);
                memcpy(buf + at, lines[j], l); at += l;
                if (j + 1 < i) buf[at++] = '\n';
            }
            buf[at] = '\0';
            json_t *t = json_array(); json_append(t, json_string("text")); json_append(t, json_string(buf));
            json_append(parts, t); free(buf);
        }
        json_t *img = json_array();
        json_append(img, json_string("image"));
        json_append(img, json_string(hint_local[k] ? hint_ref[k] : ""));
        json_append(img, mark_d[k] ? json_string(mark_d[k]) : json_null());
        json_append(img, json_string(lines[i]));
        json_append(parts, img);
        start = i + 1;
    }
    if (start < nlines) {
        size_t len = 0;
        for (size_t j = start; j < nlines; j++) len += strlen(lines[j]) + (j + 1 < nlines ? 1 : 0);
        char *buf = (char *)malloc(len + 1);
        size_t at = 0;
        for (size_t j = start; j < nlines; j++) {
            size_t l = strlen(lines[j]);
            memcpy(buf + at, lines[j], l); at += l;
            if (j + 1 < nlines) buf[at++] = '\n';
        }
        buf[at] = '\0';
        json_t *t = json_array(); json_append(t, json_string("text")); json_append(t, json_string(buf));
        json_append(parts, t); free(buf);
    }
    for (size_t i = 0; i < nm; i++) free((void *)mark_d[i]);
    for (size_t i = 0; i < nh; i++) free(hint_ref[i]);
    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines); free(mark_i); free(mark_d); free(hint_ref); free(hint_local);
    return parts;
}

static int ir_is_image_type(const json_t *v) {
    if (!v || v->type != JSON_STRING) return 0;
    return !strcmp(v->str_val, "image") || !strcmp(v->str_val, "image_url") ||
           !strcmp(v->str_val, "input_image");
}

json_t *imagereplay_replay_recent(const json_t *messages, double keep, const json_t *urls) {
    if (!messages || messages->type != JSON_ARRAY) {
        json_t *res = json_object();
        json_set(res, "attached", json_number(0));
        json_set(res, "messages", json_copy(messages ? messages : json_array()));
        return res;
    }
    /* Build a working copy so we never mutate the caller's tree. */
    json_t *work = json_copy(messages);
    size_t nmsg = json_len(work);
    /* rows: message index, parts array copy, starting slot. */
    size_t *row_msg = (size_t *)malloc((nmsg ? nmsg : 1) * sizeof(size_t));
    json_t **row_parts = (json_t **)malloc((nmsg ? nmsg : 1) * sizeof(json_t *));
    size_t *row_slot = (size_t *)malloc((nmsg ? nmsg : 1) * sizeof(size_t));
    size_t nrow = 0;
    size_t slots = 0;
    for (size_t mi = 0; mi < nmsg; mi++) {
        json_t *msg = json_get(work, mi);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *role = json_obj_get(msg, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "user")) continue;
        json_t *content = json_obj_get(msg, "content");
        if (content && content->type == JSON_ARRAY) {
            for (size_t ci = 0; ci < json_len(content); ci++) {
                json_t *part = json_get(content, ci);
                if (part && part->type == JSON_OBJECT && ir_is_image_type(json_obj_get(part, "type"))) slots++;
            }
            continue;
        }
        if (!content || content->type != JSON_STRING) continue;
        json_t *parts = imagereplay_split_row(content->str_val);
        if (!parts || parts->type == JSON_NULL) { json_free(parts); continue; }
        size_t imgs = 0;
        for (size_t pi = 0; pi < json_len(parts); pi++) {
            json_t *p = json_get(parts, pi);
            const char *k = json_string_value(json_get(p, 0));
            if (p && p->type == JSON_ARRAY && k && !strcmp(k, "image")) imgs++;
        }
        row_msg[nrow] = mi; row_parts[nrow] = parts; row_slot[nrow] = slots; nrow++;
        slots += imgs;
    }
    double frontier = json_number_value(imagereplay_frontier((double)slots, keep));
    size_t attached = 0;
    for (size_t r = 0; r < nrow; r++) {
        json_t *parts = row_parts[r];
        size_t slot = row_slot[r];
        json_t *rebuilt = json_array();
        size_t urls_in_row = 0;
        for (size_t pi = 0; pi < json_len(parts); pi++) {
            json_t *p = json_get(parts, pi);
            const char *kind = (json_len(p) && json_string_value(json_get(p, 0))) ? json_string_value(json_get(p, 0)) : "";
            if (!strcmp(kind, "text")) {
                json_t *tp = json_object();
                json_set(tp, "type", json_string("text"));
                json_set(tp, "text", json_copy(json_get(p, 1)));
                json_append(rebuilt, tp);
                continue;
            }
            const char *ref = json_string_value(json_get(p, 1));
            json_t *detail_v = json_get(p, 2);
            const char *marker = json_string_value(json_get(p, 3));
            json_t *url = NULL;
            if ((double)slot >= frontier && urls && ref) url = json_obj_get(urls, ref);
            slot++;
            int url_ok = url && url->type == JSON_STRING && url->str_val[0];
            if (url_ok) {
                json_t *iu = json_object();
                json_set(iu, "url", json_copy(url));
                if (detail_v && detail_v->type == JSON_STRING) json_set(iu, "detail", json_copy(detail_v));
                json_t *ip = json_object();
                json_set(ip, "type", json_string("image_url"));
                json_set(ip, "image_url", iu);
                json_append(rebuilt, ip);
                urls_in_row++;
            } else {
                json_t *tp = json_object();
                json_set(tp, "type", json_string("text"));
                json_set(tp, "text", json_string(marker ? marker : ""));
                json_append(rebuilt, tp);
            }
        }
        if (urls_in_row) {
            json_t *msg = json_get(work, row_msg[r]);
            json_obj_del(msg, "content");
            json_set(msg, "content", rebuilt);
            attached += urls_in_row;
        } else {
            json_free(rebuilt);
        }
        json_free(parts);
    }
    free(row_msg); free(row_parts); free(row_slot);
    json_t *res = json_object();
    json_set(res, "attached", json_number((double)attached));
    json_set(res, "messages", work);
    return res;
}
