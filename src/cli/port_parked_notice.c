/*
 * port_parked_notice.c — pure helper (see header).
 * PARKED_NOTICE_WINDOW_S = 60.0.
 */

#include "port_parked_notice.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARKED_NOTICE_WINDOW_S 60.0

typedef struct {
    char *profile;
    char *platform;
    char *chat;
    double ts;
} Entry;

static char *pn_composite(const char *profile, const char *platform,
                          const char *chat)
{
    size_t a = strlen(profile), b = strlen(platform), c = strlen(chat);
    char *o = malloc(a + b + c + 3);
    if (!o) return NULL;
    memcpy(o, profile, a);
    o[a] = '\x1f';
    memcpy(o + a + 1, platform, b);
    o[a + 1 + b] = '\x1f';
    memcpy(o + a + 2 + b, chat, c);
    o[a + 2 + b + c] = '\0';
    return o;
}

char *prr_notice_due(const char *sent_json, double now, const char *profile,
                     const char *platform, const char *chat_id)
{
    json_t *arr = sent_json ? json_parse(sent_json, NULL) : NULL;
    Entry *es = NULL;
    size_t n = 0;
    json_t *out_arr = NULL, *result = NULL;
    char *key = NULL, *ser = NULL;
    char *p = profile ? profile : "";
    char *pl = platform ? platform : "";
    char *ch = chat_id ? chat_id : "";
    int due = 0;

    if (arr && arr->type == JSON_ARRAY) {
        size_t cnt = arr->c.count;
        es = calloc(cnt ? cnt : 1, sizeof(Entry));
        for (size_t i = 0; i < cnt; i++) {
            const json_t *it = arr->c.items[i];
            const json_t *q;
            if (!it || it->type != JSON_OBJECT) continue;
            q = json_obj_get(it, "profile");
            es[n].profile = strdup(q && q->type == JSON_STRING ? q->str_val : "");
            q = json_obj_get(it, "platform");
            es[n].platform = strdup(q && q->type == JSON_STRING ? q->str_val : "");
            q = json_obj_get(it, "chat");
            es[n].chat = strdup(q && q->type == JSON_STRING ? q->str_val : "");
            q = json_obj_get(it, "ts");
            es[n].ts = (q && q->type == JSON_NUMBER) ? q->num_val : 0.0;
            n++;
        }
    }
    if (arr) { json_free(arr); arr = NULL; }

    key = pn_composite(p, pl, ch);
    if (!key) { free(es); return NULL; }

    /* evict stale first (now - ts >= 60.0) */
    {
        size_t w = 0;
        for (size_t i = 0; i < n; i++) {
            if (now - es[i].ts >= PARKED_NOTICE_WINDOW_S)
                continue;
            if (w != i) es[w] = es[i];
            w++;
        }
        n = w;
    }

    /* due = key not present after eviction */
    {
        size_t i;
        int present = 0;
        for (i = 0; i < n; i++) {
            char *k = pn_composite(es[i].profile, es[i].platform, es[i].chat);
            int eq = k && strcmp(k, key) == 0;
            free(k);
            if (eq) { present = 1; break; }
        }
        due = !present;
    }

    if (due) {
        /* sent[key] = now */
        size_t old = n;
        es = realloc(es, (n + 1) * sizeof(Entry));
        if (es) {
            es[n].profile = strdup(p);
            es[n].platform = strdup(pl);
            es[n].chat = strdup(ch);
            es[n].ts = now;
            n++;
        }
        (void)old;
    }

    /* output kept entries preserving order */
    out_arr = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *o = json_object();
        json_set(o, "profile", json_string(es[i].profile));
        json_set(o, "platform", json_string(es[i].platform));
        json_set(o, "chat", json_string(es[i].chat));
        json_set(o, "ts", json_number(es[i].ts));
        json_array_append(out_arr, o);
        free(es[i].profile);
        free(es[i].platform);
        free(es[i].chat);
    }
    free(es);

    result = json_object();
    json_set(result, "due", json_bool(due));
    json_set(result, "sent", out_arr);
    ser = json_serialize(result);
    json_free(result);
    free(key);
    return ser;
}
