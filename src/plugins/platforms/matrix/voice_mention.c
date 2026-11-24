/*
 * voice_mention.c - real C11 port of upstream Hermes
 *   plugins/platforms/matrix/voice_mention.py: the MSC3245 voice checks and the
 *   ParkedVoices state machine (arrival sequence, claim window, per-sender cap,
 *   in-flight floor), replayed deterministically with an injected monotonic clock.
 * PoP: w19_voicementi_2db922_has_voice_marker @ plugins/platforms/matrix/voice_mention.py:has_voice_marker
 * PoP: w19_voicementi_2db922_is_voice_event @ plugins/platforms/matrix/voice_mention.py:is_voice_event
 * PoP: w19_voicementi_2db922_parked_replay @ plugins/platforms/matrix/voice_mention.py:ParkedVoices
 */
#include "noc_w19_voicementi_2db922.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module constants mirrored from the Python source. */
static const double CLAIM_WINDOW_SECONDS = 120.0;
static const int MAX_PARKED_PER_SENDER = 4;

#define MAX_GROUPS 8
#define MAX_PARKED 16
#define MAX_INFLIGHT 16
#define KEY_LEN 80

typedef struct {
    double parked_at;
    long long seq;
    json_t *voice;   /* owned [event_id, content, relates_to] */
} parked_t;

typedef struct {
    char room[KEY_LEN];
    char sender[KEY_LEN];
    int used;
    parked_t parked[MAX_PARKED];
    int nparked;
    long long inflight[MAX_INFLIGHT];
    int ninflight;
    long long floor;
    int has_floor;
} group_t;

typedef struct {
    group_t groups[MAX_GROUPS];
    int ngroups;
    long long next_seq;
} state_t;

int w19_voicementi_2db922_has_voice_marker(json_t *content) {
    if (!content || content->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(content, "org.matrix.msc3245.voice");
    /* dict.get(...) is None for an absent key and for an explicit null. */
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

int w19_voicementi_2db922_is_voice_event(json_t *content) {
    if (!content || content->type != JSON_OBJECT) return 0;
    json_t *msgtype = json_obj_get(content, "msgtype");
    int audio = msgtype && msgtype->type == JSON_STRING && msgtype->str_val
             && !strcmp(msgtype->str_val, "m.audio");
    return audio && w19_voicementi_2db922_has_voice_marker(content);
}

static void key_copy(char *dst, const char *src) {
    snprintf(dst, KEY_LEN, "%s", src ? src : "");
}

static group_t *group_find(state_t *st, const char *room, const char *sender, int create) {
    char r[KEY_LEN], s[KEY_LEN];
    key_copy(r, room);
    key_copy(s, sender);
    for (int i = 0; i < st->ngroups; i++)
        if (!strcmp(st->groups[i].room, r) && !strcmp(st->groups[i].sender, s))
            return &st->groups[i];
    if (!create || st->ngroups >= MAX_GROUPS) return NULL;
    group_t *g = &st->groups[st->ngroups++];
    memset(g, 0, sizeof(*g));
    g->used = 1;
    key_copy(g->room, r);
    key_copy(g->sender, s);
    return g;
}

static void free_parked(parked_t *p) {
    if (p->voice) json_free(p->voice);
    p->voice = NULL;
}

/* _prune: keep entries inside the claim window; a group whose parked list empties leaves
 * the parked map (the in-flight list, when empty, is dropped by release). */
static void prune_group(group_t *g, double now) {
    double cutoff = now - CLAIM_WINDOW_SECONDS;
    int w = 0;
    for (int i = 0; i < g->nparked; i++) {
        if (g->parked[i].parked_at >= cutoff) {
            if (w != i) g->parked[w] = g->parked[i];
            w++;
        } else {
            free_parked(&g->parked[i]);
        }
    }
    g->nparked = w;
}

static void prune_all(state_t *st, double now) {
    for (int i = 0; i < st->ngroups; i++) prune_group(&st->groups[i], now);
}

static void inflight_remove(group_t *g, long long seq) {
    for (int i = 0; i < g->ninflight; i++) {
        if (g->inflight[i] == seq) {
            for (int j = i; j + 1 < g->ninflight; j++) g->inflight[j] = g->inflight[j + 1];
            g->ninflight--;
            if (g->ninflight == 0) g->has_floor = 0;   /* no older voice can park any more */
            return;
        }
    }
}

static void sort_parked_by_seq(group_t *g) {
    for (int i = 1; i < g->nparked; i++) {
        parked_t key = g->parked[i];
        int j = i - 1;
        while (j >= 0 && g->parked[j].seq > key.seq) {
            g->parked[j + 1] = g->parked[j];
            j--;
        }
        g->parked[j + 1] = key;
    }
}

static double op_num(json_t *op, const char *key) {
    return json_get_num(op, key, 0.0);
}

static long long op_int(json_t *op, const char *key, long long def) {
    json_t *v = op ? json_obj_get(op, key) : NULL;
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}

json_t *w19_voicementi_2db922_parked_replay(json_t *ops) {
    state_t st;
    memset(&st, 0, sizeof(st));
    json_t *trace = json_array();
    if (!ops || ops->type != JSON_ARRAY) return trace;

    for (size_t i = 0; i < json_len(ops); i++) {
        json_t *op = json_get(ops, i);
        if (!op || op->type != JSON_OBJECT) { json_append(trace, json_null()); continue; }
        const char *name = json_get_str(op, "op", "");
        const char *room = json_get_str(op, "room", "");
        const char *sender = json_get_str(op, "sender", "");
        double now = op_num(op, "now");
        group_t *g = group_find(&st, room, sender, 0);

        if (!strcmp(name, "mark")) {
            json_append(trace, json_number((double)st.next_seq));
        } else if (!strcmp(name, "begin")) {
            long long seq = st.next_seq++;
            g = group_find(&st, room, sender, 1);
            if (g && g->ninflight < MAX_INFLIGHT) g->inflight[g->ninflight++] = seq;
            json_append(trace, json_number((double)seq));
        } else if (!strcmp(name, "release")) {
            long long seq = op_int(op, "gate", -1);
            if (g) inflight_remove(g, seq);
            json_append(trace, json_null());
        } else if (!strcmp(name, "park")) {
            long long seq = op_int(op, "gate", -1);
            /* gate.seq <= self._floor.get(key, -1): a newer voice was already claimed. */
            long long floor = (g && g->has_floor) ? g->floor : -1;
            if (seq <= floor) { json_append(trace, json_null()); continue; }
            prune_all(&st, now);
            g = group_find(&st, room, sender, 1);
            if (g && g->nparked < MAX_PARKED) {
                parked_t *p = &g->parked[g->nparked++];
                p->parked_at = now;
                p->seq = seq;
                json_t *voice = json_array();
                json_t *event_id = json_obj_get(op, "event_id");
                json_append(voice, event_id ? json_copy(event_id) : json_null());
                json_t *content = json_obj_get(op, "content");
                json_append(voice, content ? json_copy(content) : json_object());
                json_t *relates = json_obj_get(op, "relates_to");
                json_append(voice, relates ? json_copy(relates) : json_object());
                p->voice = voice;
                sort_parked_by_seq(g);
                while (g->nparked > MAX_PARKED_PER_SENDER) {
                    free_parked(&g->parked[0]);
                    for (int k = 0; k + 1 < g->nparked; k++) g->parked[k] = g->parked[k + 1];
                    g->nparked--;
                }
            }
            json_append(trace, json_null());
        } else if (!strcmp(name, "claim")) {
            long long before = op_int(op, "before", 0);
            prune_all(&st, now);
            g = group_find(&st, room, sender, 0);
            int idx = -1;
            if (g) {
                for (int k = 0; k < g->nparked; k++)
                    if (g->parked[k].seq < before) idx = k;
            }
            if (idx < 0) { json_append(trace, json_null()); continue; }
            json_t *voice = g->parked[idx].voice;
            long long claimed_seq = g->parked[idx].seq;
            g->parked[idx].voice = NULL;
            for (int k = 0; k <= idx; k++) {
                if (k != idx) free_parked(&g->parked[k]);
            }
            for (int k = idx + 1; k < g->nparked; k++) g->parked[k - idx - 1] = g->parked[k];
            g->nparked -= (idx + 1);
            if (g->ninflight > 0) {   /* a late older voice must not park after this claim */
                long long floor = g->has_floor ? g->floor : -1;
                g->floor = claimed_seq > floor ? claimed_seq : floor;
                g->has_floor = 1;
            }
            json_append(trace, voice ? voice : json_null());
        } else if (!strcmp(name, "pending")) {
            int pending = 0;
            if (g && g->ninflight > 0) {
                pending = 1;
            } else if (g) {
                double cutoff = now - CLAIM_WINDOW_SECONDS;
                for (int k = 0; k < g->nparked; k++)
                    if (g->parked[k].parked_at >= cutoff) { pending = 1; break; }
            }
            json_append(trace, json_bool(pending));
        } else if (!strcmp(name, "prune")) {
            prune_all(&st, now);
            json_append(trace, json_null());
        } else {
            json_append(trace, json_string("UNKNOWN_OP"));
        }
    }

    for (int i = 0; i < st.ngroups; i++)
        for (int k = 0; k < st.groups[i].nparked; k++) free_parked(&st.groups[i].parked[k]);
    return trace;
}
