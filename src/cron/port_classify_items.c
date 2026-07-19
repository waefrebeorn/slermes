/*
 * port_classify_items.c — pure helpers ported from
 * cron/scripts/classify_items.py. Self-contained; uses libjson for parsing.
 *
 *   - _item_id      -> cron_classify_item_id
 *   - _build_prompt -> cron_classify_build_prompt
 *   - _parse_scores -> cron_classify_parse_scores
 */

#include "classify_items_helpers.h"
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* PoP: _item_id @ cron/scripts/classify_items.py:_item_id
 * Prefer id/guid/message_id/url/link; else "item-<index>". Result malloc'd. */
char *cron_classify_item_id(const json_t *item, int index)
{
    static const char *keys[] = {"id", "guid", "message_id", "url", "link"};
    if (item && item->type == JSON_OBJECT) {
        for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
            const json_t *v = json_obj_get(item, keys[i]);
            if (v && v->type == JSON_STRING && v->str_val && *v->str_val)
                return strdup(v->str_val);
        }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "item-%d", index);
    return strdup(buf);
}

/* PoP: _build_prompt @ cron/scripts/classify_items.py:_build_prompt
 * Build the classifier prompt. `items` is a json_t array; each element's
 * salient fields are serialized (capped at 1200 bytes) like Python. Returns a
 * malloc'd string. */
char *cron_classify_build_prompt(const json_t *items, const char *criteria)
{
    const char *crit = criteria ? criteria : "";
    size_t n = (items && items->type == JSON_ARRAY) ? items->c.count : 0;

    /* Measure first pass. */
    size_t need = 0;
    need += strlen("USER IMPORTANCE CRITERIA:\n") + strlen(crit) + strlen("\n") + 1;
    need += strlen("ITEMS:") + 1;
    for (size_t i = 0; i < n; i++) {
        const json_t *item = json_get(items, i);
        char *view = NULL;
        if (item && item->type == JSON_OBJECT) {
            static const char *fld[] = {"title","subject","summary","text","body","from","sender","url"};
            json_t *sub = json_object();
            int added = 0;
            for (size_t k = 0; k < sizeof(fld)/sizeof(fld[0]); k++) {
                const json_t *v = json_obj_get(item, fld[k]);
                if (v) { json_set(sub, fld[k], json_copy(v)); added = 1; }
            }
            if (!added) sub = json_copy(item);
            char *ser = json_serialize(sub);
            if (strlen(ser) > 1200) ser[1200] = '\0';
            view = ser;
            json_free(sub);
        } else {
            view = strdup("");
        }
        need += 4 /* "[i] " */ + 10 /* index digits */ + 1 /* space */ + strlen(view) + 1 /* nl */;
        free(view);
    }
    need += strlen("\nReturn the JSON array of scores now (one object per item, same order).") + 1 + 1;

    char *out = malloc(need + 1);
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, need + 1 - pos, "USER IMPORTANCE CRITERIA:\n%s\n", crit);
    pos += (size_t)snprintf(out + pos, need + 1 - pos, "ITEMS:\n");
    for (size_t i = 0; i < n; i++) {
        const json_t *item = json_get(items, i);
        char *view = NULL;
        if (item && item->type == JSON_OBJECT) {
            static const char *fld[] = {"title","subject","summary","text","body","from","sender","url"};
            json_t *sub = json_object();
            int added = 0;
            for (size_t k = 0; k < sizeof(fld)/sizeof(fld[0]); k++) {
                const json_t *v = json_obj_get(item, fld[k]);
                if (v) { json_set(sub, fld[k], json_copy(v)); added = 1; }
            }
            if (!added) sub = json_copy(item);
            char *ser = json_serialize(sub);
            if (strlen(ser) > 1200) ser[1200] = '\0';
            view = ser;
            json_free(sub);
        } else {
            view = strdup("");
        }
        pos += (size_t)snprintf(out + pos, need + 1 - pos, "[%zu] %s\n", i, view);
        free(view);
    }
    snprintf(out + pos, need + 1 - pos, "\nReturn the JSON array of scores now (one object per item, same order).");
    return out;
}

/* PoP: _parse_scores @ cron/scripts/classify_items.py:_parse_scores
 * Parse a tolerant JSON array of {index, score, reason}. Returns a malloc'd
 * array of classify_score_t (count in *out_count); caller frees with
 * cron_classify_free_scores. Empty/invalid -> count 0. */
classify_score_t *cron_classify_parse_scores(const char *content, int n_items, int *out_count)
{
    *out_count = 0;
    if (!content) return NULL;
    char *text = strdup(content);
    /* strip leading/trailing whitespace */
    char *p = text;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r' || p[len-1] == '\n')) p[--len] = '\0';

    /* Tolerate accidental markdown fences. */
    if (len >= 3 && strncmp(p, "```", 3) == 0) {
        /* strip backticks */
        char *q = p;
        while (*q == '`') q++;
        /* drop the first line (language tag) */
        char *nl = strchr(q, '\n');
        if (nl) q = nl + 1;
        /* strip trailing backticks */
        size_t ql = strlen(q);
        while (ql > 0 && q[ql-1] == '`') q[--ql] = '\0';
        p = q;
    }

    char *err = NULL;
    json_t *arr = json_parse(p, &err);
    free(text);
    if (err) free(err);
    if (!arr) {
        /* Last-ditch: find first [...] block. */
        const char *s = content ? strchr(content, '[') : NULL;
        const char *e = content ? strrchr(content, ']') : NULL;
        if (s && e && e > s) {
            size_t blen = (size_t)(e - s + 1);
            char *block = malloc(blen + 1);
            memcpy(block, s, blen);
            block[blen] = '\0';
            arr = json_parse(block, &err);
            free(block);
            if (err) free(err);
        }
    }
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return NULL; }

    size_t cap = arr->c.count ? arr->c.count : 1;
    classify_score_t *out = calloc(cap, sizeof(classify_score_t));
    int cnt = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *obj = json_get(arr, i);
        if (!obj || obj->type != JSON_OBJECT) continue;
        double didx = json_get_num(obj, "index", -1);
        int idx = (int)didx;
        if (idx != didx) continue;                 /* non-integer index */
        if (idx < 0 || (n_items > 0 && idx >= n_items)) continue;
        double dscore = json_get_num(obj, "score", 0);
        const char *reason = json_get_str(obj, "reason", "");
        out[cnt].index = idx;
        out[cnt].score = (int)dscore;
        out[cnt].reason = strdup(reason ? reason : "");
        cnt++;
    }
    json_free(arr);
    *out_count = cnt;
    return out;
}

void cron_classify_free_scores(classify_score_t *scores, int count)
{
    if (!scores) return;
    for (int i = 0; i < count; i++) free(scores[i].reason);
    free(scores);
}
