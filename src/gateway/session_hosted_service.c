/*
 * session_hosted_service.c — real C11 port of upstream Hermes
 *   gateway/session_hosted_service.py pure helpers and publish-gate route
 *   decision. The async/live-I/O wrappers (ensure_hosted_service,
 *   _ensure_hosted_service, stop_hosted_service, probe_native_room_owner) and
 *   the store/repository-bound service methods stay in Python; everything that
 *   is a pure decision, validation, normalization or payload builder is here.
 *
 * PoP: w18_sessionhos_6fbeb0_check_claim @ gateway/session_hosted_service.py:_check_claim
 * PoP: w18_sessionhos_6fbeb0_attempt_retired @ gateway/session_hosted_service.py:attempt_retired
 * PoP: w18_sessionhos_6fbeb0_retired_attempt_conflict @ gateway/session_hosted_service.py:retired_attempt_conflict
 * PoP: w18_sessionhos_6fbeb0_start_ready @ gateway/session_hosted_service.py:start_ready_hosted_services
 * PoP: w18_sessionhos_6fbeb0_room_profiles @ gateway/session_hosted_service.py:CanonicalHostedRoomService.room_profiles
 * PoP: w18_sessionhos_6fbeb0_stop_read @ gateway/session_hosted_service.py:_STOP_READS
 */
#include "noc_w18_sessionhos_6fbeb0.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* _RETIRED_ATTEMPT_STATUSES = frozenset({'queued','deferred','settled','failed','cancelled'}) */
static const char *const RETIRED_ATTEMPT_STATUSES[] = {
    "queued", "deferred", "settled", "failed", "cancelled", NULL
};

/* _STOP_READS = frozenset({'resolve_exact','resume','info','interrupt','history','results','discard'}) */
static const char *const STOP_READS[] = {
    "resolve_exact", "resume", "info", "interrupt", "history", "results",
    "discard", NULL
};

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness of a parsed JSON value (None/False/0/""/[]/{} are falsy). */
static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* len(str) in code points, as Python counts it. */
static size_t utf8_codepoints(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    size_t n = 0;
    for (; *p; p++) {
        if ((*p & 0xC0u) != 0x80u) n++;
    }
    return n;
}

static int is_ascii_alnum(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

/* IDENTIFIER_RE.fullmatch: ^[A-Za-z0-9][A-Za-z0-9._:-]*$ — the whole string,
 * so a trailing newline (which a $-anchored search would tolerate) fails. */
static int identifier_fullmatch(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i, n;
    if (!s || !*s) return 0;
    if (!is_ascii_alnum(p[0])) return 0;
    n = strlen(s);
    for (i = 1; i < n; i++) {
        unsigned char c = p[i];
        if (is_ascii_alnum(c) || c == '.' || c == '_' || c == ':' || c == '-')
            continue;
        return 0;
    }
    return 1;
}

const char *w18_sessionhos_6fbeb0_check_claim(const char *actor_subject,
                                              const char *room_id) {
    /* not isinstance(actor_subject, str) or not actor_subject */
    if (!actor_subject || !actor_subject[0]) return "invalid_params";
    /* not isinstance(room_id, str) or len(room_id) > 128 or not fullmatch */
    if (!room_id) return "invalid_params";
    if (utf8_codepoints(room_id) > 128) return "invalid_params";
    if (!identifier_fullmatch(room_id)) return "invalid_params";
    return NULL;
}

int w18_sessionhos_6fbeb0_stop_read(const char *operation) {
    int i;
    if (!operation) return 0;
    for (i = 0; STOP_READS[i]; i++) {
        if (!strcmp(operation, STOP_READS[i])) return 1;
    }
    return 0;
}

/* type(generation) is int: a JSON number with an integral value. A boolean is
 * its own JSON type (and Python bool is not int for an identity check); a
 * string, null or non-integral number is not an int. */
static int generation_is_int(const json_t *v, long long *out) {
    double d, ip;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (modf(d, &ip) != 0.0) return 0;
    if (ip < -9.0e18 || ip > 9.0e18) return 0;
    *out = (long long)ip;
    return 1;
}

static int py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v';
}

/* current = int(task['execution_generation']) — int() accepts ints, bools and
 * decimal strings with surrounding whitespace. */
static int task_execution_generation(const json_t *task, long long *out) {
    const json_t *g;
    if (!task || task->type != JSON_OBJECT) return 0;
    g = json_obj_get(task, "execution_generation");
    if (!g) return 0;
    if (g->type == JSON_NUMBER) {
        *out = (long long)g->num_val;
        return 1;
    }
    if (g->type == JSON_BOOL) {
        *out = g->bool_val ? 1 : 0;
        return 1;
    }
    if (g->type == JSON_STRING) {
        const char *s = g->str_val ? g->str_val : "";
        char *end = NULL;
        long long v;
        while (py_space(*s)) s++;
        v = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (py_space(*end)) end++;
        if (*end) return 0;
        *out = v;
        return 1;
    }
    return 0;
}

int w18_sessionhos_6fbeb0_attempt_retired(const json_t *task,
                                          const json_t *generation) {
    long long gen, current;
    const json_t *status_node;
    int i;

    if (!generation_is_int(generation, &gen)) return 0;
    if (!task_execution_generation(task, &current)) return 0;
    if (current > gen) return 1;
    if (current != gen) return 0;
    status_node = json_obj_get(task, "status");
    if (!status_node || status_node->type != JSON_STRING) return 0;
    for (i = 0; RETIRED_ATTEMPT_STATUSES[i]; i++) {
        if (!strcmp(status_node->str_val, RETIRED_ATTEMPT_STATUSES[i]))
            return 1;
    }
    return 0;
}

json_t *w18_sessionhos_6fbeb0_retired_attempt_conflict(void) {
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "reason", json_string("admission_conflict"));
    json_set(out, "field", json_string("task"));
    json_set(out, "detail",
             json_string("the room task no longer runs this attempt"));
    return out;
}

json_t *w18_sessionhos_6fbeb0_start_ready(const char *state, int draining,
                                          const json_t *services) {
    json_t *out = json_object();
    size_t i, n, started = 0;
    const char *action = "skip";

    if (!out) return NULL;
    n = (services && services->type == JSON_ARRAY) ? services->c.count : 0;

    /* getattr(runner,'session_runtime_descriptor',{}).get('state') != 'ready' */
    if (!state || strcmp(state, "ready") != 0) {
        action = "skip";
    } else if (draining) {
        /* or getattr(runner, '_draining', False) */
        action = "skip";
    } else {
        int unprepared = 0;
        for (i = 0; i < n; i++) {
            const json_t *svc = json_array_get(services, i);
            if (!svc || svc->type == JSON_NULL) {
                unprepared = 1; /* service is None */
                break;
            }
            if (!json_truthy(json_obj_get(svc, "transport_installed"))) {
                unprepared = 1; /* not getattr(service,'_transport_installed',False) */
                break;
            }
        }
        if (!unprepared) {
            /* Every service prepared: the live loop calls start() on each and
             * then import_after_ready(...) — one start per served authority. */
            action = "start";
            started = n;
        }
    }

    json_set(out, "action", json_string(action));
    json_set(out, "started", json_number((double)started));
    return out;
}

/* (m.get('target') or {}).get('kind', 'local') — falsy targets fall back to
 * the empty dict; a truthy non-object target would raise upstream and is not
 * representable as a route decision here. A present-but-non-string kind is
 * returned as the empty sentinel: it never equals "local". */
static const char *member_target_kind(const json_t *member) {
    const json_t *target = json_obj_get(member, "target");
    const json_t *kind;
    if (!json_truthy(target)) return "local";
    if (target->type != JSON_OBJECT) return "local";
    kind = json_obj_get(target, "kind");
    if (!kind) return "local";
    if (kind->type != JSON_STRING) return "";
    return kind->str_val;
}

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} str_set;

static void set_add(str_set *set, const char *value) {
    size_t i;
    char *copy;
    if (!set || !value) return;
    for (i = 0; i < set->count; i++) {
        if (!strcmp(set->items[i], value)) return;
    }
    if (set->count == set->cap) {
        size_t cap = set->cap ? set->cap * 2 : 8;
        char **grown = (char **)realloc(set->items, cap * sizeof *grown);
        if (!grown) return;
        set->items = grown;
        set->cap = cap;
    }
    copy = dupstr(value);
    if (!copy) return;
    set->items[set->count++] = copy;
}

static void set_free(str_set *set) {
    size_t i;
    if (!set) return;
    for (i = 0; i < set->count; i++) free(set->items[i]);
    free(set->items);
    set->items = NULL;
    set->count = set->cap = 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w18_sessionhos_6fbeb0_room_profiles(const json_t *room,
                                            const json_t *local_profiles) {
    str_set seated = {NULL, 0, 0};
    json_t *out;
    size_t i, n;

    /* set(self.local_profiles()) */
    if (local_profiles && local_profiles->type == JSON_ARRAY) {
        n = local_profiles->c.count;
        for (i = 0; i < n; i++) {
            const json_t *p = json_array_get(local_profiles, i);
            if (p && p->type == JSON_STRING) set_add(&seated, p->str_val);
        }
    }

    /* room.get('members') or () */
    if (room && room->type == JSON_OBJECT) {
        const json_t *members = json_obj_get(room, "members");
        if (members && members->type == JSON_ARRAY) {
            n = members->c.count;
            for (i = 0; i < n; i++) {
                const json_t *m = json_array_get(members, i);
                const json_t *profile;
                if (!m || m->type != JSON_OBJECT) continue;
                profile = json_obj_get(m, "profile");
                if (!profile || profile->type != JSON_STRING) continue;
                if (strcmp(member_target_kind(m), "local") != 0) continue;
                set_add(&seated, profile->str_val);
            }
        }
    }

    if (seated.count > 1) {
        qsort(seated.items, seated.count, sizeof *seated.items, cmp_str);
    }
    out = json_array();
    if (out) {
        for (i = 0; i < seated.count; i++) {
            json_append(out, json_string(seated.items[i]));
        }
    }
    set_free(&seated);
    return out;
}
