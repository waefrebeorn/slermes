/*
 * profile_repair.c - real C11 port of upstream Hermes
 *   persistence/sqlite/admin/profile_repair.py pure/helper logic.
 *
 * The live class methods are SQL drivers; what this file ports is the row-shaping
 * decision each of them makes, with the store's answers (row sets, presence probes)
 * handed in as explicit seams instead of being queried here.
 *
 * PoP: w19_profilerep_3b4c1c_stored_profile @ persistence/sqlite/admin/profile_repair.py:_stored_profile
 * PoP: w19_profilerep_3b4c1c_find_crossed @ persistence/sqlite/admin/profile_repair.py:find_crossed_profile_sessions
 * PoP: w19_profilerep_3b4c1c_less_topic_rows @ persistence/sqlite/admin/profile_repair.py:find_profile_less_telegram_topic_rows
 * PoP: w19_profilerep_3b4c1c_key_profiles @ persistence/sqlite/admin/profile_repair.py:key_profiles_for_chat
 * PoP: w19_profilerep_3b4c1c_import_moved @ persistence/sqlite/admin/profile_repair.py:import_moved_session
 */
#include "hermes_json.h"
#include "noc_w19_profilerep_3b4c1c.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W19P_MAX_TITLE_LENGTH 100

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python truthiness of a JSON value. */
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python str() of a scalar (the values these rows carry). Caller buffer >= 512. */
static const char *pystr(const json_t *v, char *buf, size_t n) {
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) return buf;
    switch (v->type) {
        case JSON_BOOL:
            snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
            return buf;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, n, "%lld", (long long)d);
            else snprintf(buf, n, "%g", d);
            return buf;
        }
        case JSON_STRING:
            snprintf(buf, n, "%s", v->str_val ? v->str_val : "");
            return buf;
        default:
            /* containers: not part of any sessions column this port reads */
            return buf;
    }
}

/* str.strip() on an ASCII-defined whitespace set. */
static char *strip_dup(const char *s) {
    if (!s) s = "";
    while (*s && is_ws(*s)) s++;
    size_t len = strlen(s);
    while (len && is_ws(s[len - 1])) len--;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* persistence.core.keys.session_key_profile: profile from "agent:<ns>:...". */
static const char *key_profile(const char *key, char *buf, size_t n) {
    if (!key) return NULL;
    if (strncmp(key, "agent:", 6) != 0) return NULL;
    const char *ns = key + 6;
    const char *c2 = strchr(ns, ':');
    if (!c2) return NULL;              /* fewer than three parts */
    size_t nslen = (size_t)(c2 - ns);
    if (nslen == 0) return NULL;
    if (nslen == 4 && !strncmp(ns, "main", 4)) { snprintf(buf, n, "default"); return buf; }
    if (nslen == 5 && !strncmp(ns, "main~", 5)) { snprintf(buf, n, "main"); return buf; }
    if (nslen + 1 > n) return NULL;
    memcpy(buf, ns, nslen);
    buf[nslen] = '\0';
    return buf;
}

/* Copy a node, answering JSON null when the key is absent. */
static json_t *jcn(json_t *v) { return v ? json_copy(v) : json_null(); }

/* Identical-string form of a scalar, for set membership (`id in ledgered`). */
static const char *skey(const json_t *v, char *buf, size_t n) {
    return pystr(v, buf, n);
}

json_t *w19_profilerep_3b4c1c_stored_profile(json_t *value) {
    char raw[512];
    const char *s = jtruthy(value) ? pystr(value, raw, sizeof raw) : "";
    char *name = strip_dup(s);
    json_t *out;
    if (!name) return json_null();
    if (!name[0]) { free(name); return json_null(); }
    out = json_string(name);
    free(name);
    return out;
}

json_t *w19_profilerep_3b4c1c_find_crossed(json_t *rows, const char *owner, json_t *ledgered) {
    json_t *out = json_object();
    json_t *mis = json_array(), *foreign = json_array(), *crossed = json_array();
    const char *own = owner ? owner : "";
    size_t nrows = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    for (size_t i = 0; i < nrows; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_OBJECT) continue;
        json_t *jkey = json_obj_get(row, "session_key");
        const char *key = (jkey && jkey->type == JSON_STRING) ? jkey->str_val : NULL;
        char kp[256];
        const char *kprof = key_profile(key, kp, sizeof kp);
        if (!kprof) continue;
        json_t *label_j = w19_profilerep_3b4c1c_stored_profile(json_obj_get(row, "profile_name"));
        const char *label = (label_j && label_j->type == JSON_STRING) ? label_j->str_val : NULL;
        if (label && strcmp(label, kprof) != 0) {
            json_t *e = json_object();
            json_set(e, "id", jcn(json_obj_get(row, "id")));
            json_set(e, "session_key", json_string(key));
            json_set(e, "profile_name", json_string(label));
            json_set(e, "key_profile", json_string(kprof));
            json_append(mis, e);
        }
        json_free(label_j);
        if (strcmp(kprof, own) != 0) {
            json_t *e = json_object();
            json_t *mc = json_obj_get(row, "message_count");
            long long count = 0;
            if (mc && mc->type == JSON_NUMBER) count = (long long)mc->num_val;
            else if (mc && mc->type == JSON_STRING) count = strtoll(mc->str_val, NULL, 10);
            int led = 0;
            if (ledgered && ledgered->type == JSON_ARRAY) {
                char ibuf[64];
                const char *want = skey(json_obj_get(row, "id"), ibuf, sizeof ibuf);
                for (size_t k = 0; k < json_len(ledgered); k++) {
                    char obuf[64];
                    const char *have = skey(json_get(ledgered, k), obuf, sizeof obuf);
                    if (want[0] && strcmp(want, have) == 0) { led = 1; break; }
                }
            }
            json_set(e, "id", jcn(json_obj_get(row, "id")));
            json_set(e, "session_key", json_string(key));
            json_set(e, "key_profile", json_string(kprof));
            json_set(e, "message_count", json_number((double)count));
            json_set(e, "parent_session_id", jcn(json_obj_get(row, "parent_session_id")));
            json_set(e, "ledgered", json_bool(led ? true : false));
            json_append(foreign, e);
        }
        json_t *pj = json_obj_get(row, "parent_session_key");
        const char *pkey = (pj && pj->type == JSON_STRING) ? pj->str_val : NULL;
        char pp[256];
        const char *pprof = key_profile(pkey, pp, sizeof pp);
        if (pprof && strcmp(pprof, kprof) != 0) {
            json_t *e = json_object();
            json_set(e, "id", jcn(json_obj_get(row, "id")));
            json_set(e, "key_profile", json_string(kprof));
            json_set(e, "parent_session_id", jcn(json_obj_get(row, "parent_session_id")));
            json_set(e, "parent_profile", json_string(pprof));
            json_append(crossed, e);
        }
    }
    json_set(out, "mislabelled", mis);
    json_set(out, "foreign", foreign);
    json_set(out, "crossed_parents", crossed);
    return out;
}

json_t *w19_profilerep_3b4c1c_less_topic_rows(json_t *rows, int has_profile) {
    json_t *out = json_array();
    if (!has_profile) return out;
    size_t n = (rows && rows->type == JSON_ARRAY) ? json_len(rows) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_OBJECT) continue;
        json_t *jkey = json_obj_get(row, "session_key");
        const char *key = (jkey && jkey->type == JSON_STRING) ? jkey->str_val : NULL;
        char buf[256];
        const char *kp = key_profile(key, buf, sizeof buf);
        if (!kp || !strcmp(kp, "default")) continue;
        json_t *e = json_copy(row);
        if (!e || e->type != JSON_OBJECT) { json_free(e); e = json_object(); }
        json_set(e, "key_profile", json_string(kp));
        json_append(out, e);
    }
    return out;
}

json_t *w19_profilerep_3b4c1c_key_profiles(json_t *session_keys) {
    json_t *out = json_array();
    size_t n = (session_keys && session_keys->type == JSON_ARRAY) ? json_len(session_keys) : 0;
    char seen[512][128];
    size_t nseen = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(session_keys, i);
        const char *key = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        char buf[256];
        const char *kp = key_profile(key, buf, sizeof buf);
        if (!kp) continue;
        int dup = 0;
        for (size_t s = 0; s < nseen; s++) if (!strcmp(seen[s], kp)) { dup = 1; break; }
        if (dup) continue;
        if (nseen >= 512) continue;
        snprintf(seen[nseen], sizeof seen[nseen], "%s", kp);
        nseen++;
    }
    /* set comprehension -> deterministic sorted output */
    for (size_t a = 0; a < nseen; a++)
        for (size_t b = a + 1; b < nseen; b++)
            if (strcmp(seen[a], seen[b]) > 0) {
                char tmp[128];
                memcpy(tmp, seen[a], sizeof tmp);
                memcpy(seen[a], seen[b], sizeof tmp);
                memcpy(seen[b], tmp, sizeof tmp);
            }
    for (size_t s = 0; s < nseen; s++) json_append(out, json_string(seen[s]));
    return out;
}

json_t *w19_profilerep_3b4c1c_import_moved(json_t *session, const char *profile_name,
                                          int id_present, int parent_present, int title_present) {
    json_t *out = json_object();
    if (!session || session->type != JSON_OBJECT) {
        json_set(out, "status", json_string("present"));
        json_set(out, "title", json_null());
        json_set(out, "profile_name", json_null());
        json_set(out, "parent_session_id", json_null());
        return out;
    }
    if (id_present) {
        json_set(out, "status", json_string("present"));
        json_set(out, "title", jcn(json_obj_get(session, "title")));
        json_set(out, "profile_name", jcn(json_obj_get(session, "profile_name")));
        json_set(out, "parent_session_id", jcn(json_obj_get(session, "parent_session_id")));
        return out;
    }
    char sid[512];
    pystr(json_obj_get(session, "id"), sid, sizeof sid);
    json_set(out, "status", json_string("imported"));
    json_set(out, "profile_name", json_string(profile_name ? profile_name : ""));
    json_t *parent = json_obj_get(session, "parent_session_id");
    if (jtruthy(parent) && !parent_present) json_set(out, "parent_session_id", json_null());
    else json_set(out, "parent_session_id", jcn(parent));
    json_t *title = json_obj_get(session, "title");
    if (title && title->type == JSON_STRING && title_present) {
        const char *t = title->str_val;
        /* suffix = f" ({session_id[-12:]})" - the MOVED row's id tail, not the title's. */
        size_t sidlen = strlen(sid);
        const char *tail = sidlen > 12 ? sid + (sidlen - 12) : sid;
        size_t taillen = strlen(tail);
        char suffix[544];   /* " (" + a 512-byte id + ")" */
        snprintf(suffix, sizeof suffix, " (%s)", tail);
        size_t slen = taillen + 3;                 /* " (" + tail + ")" */
        long keep = (long)W19P_MAX_TITLE_LENGTH - (long)slen;
        if (keep < 0) keep = 0;
        char buf[2048];
        snprintf(buf, sizeof buf, "%.*s%s", (int)keep, t, suffix);
        json_set(out, "title", json_string(buf));
    } else {
        json_set(out, "title", jcn(title));
    }
    return out;
}
