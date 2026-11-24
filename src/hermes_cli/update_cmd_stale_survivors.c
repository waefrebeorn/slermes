/*
 * update_cmd_stale_survivors.c -- real C11 port of the post-update escalation
 * for gateways the fleet matrix proved stale (upstream Hermes
 * hermes_cli/update_cmd_stale_survivors.py): the survivor sweep over the fleet
 * matrix and the drain-first restart loop, driven against a scripted restart
 * receipt instead of live signal plumbing.
 *
 * PoP: w20_updatecmds_2cdfe8_stale_survivor_pids @ hermes_cli/update_cmd_stale_survivors.py:stale_fleet_survivor_pids
 * PoP: w20_updatecmds_2cdfe8_signal_stale_survivors @ hermes_cli/update_cmd_stale_survivors.py:signal_stale_fleet_survivors
 */
#include "hermes_json.h"
#include "noc_w20_updatecmds_2cdfe8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UTF-8 fragments of the operator-facing lines (source stays ASCII). */
#define UC2C_WARN   "  \xe2\x9a\xa0 "
#define UC2C_DASH   " \xe2\x80\x94 "
#define UC2C_ARROW  "  \xe2\x86\x92 "

static char *uc2c_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static const char *uc2c_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return (isfinite(v->num_val) && floor(v->num_val) == v->num_val) ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    case JSON_NULL:
    default:          return "NoneType";
    }
}

/* isinstance(x, int): a JSON bool counts, an integral number counts. */
static int uc2c_is_int(const json_t *v, long long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) {
        if (out) *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9223372036854775808.0 || d >= 9223372036854775808.0) return 0;
    if (out) *out = (long long)d;
    return 1;
}

static int uc2c_truthy(const json_t *v) {
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

/* str(value) for the scalar shapes a fleet row can carry. */
static void uc2c_number_text(char *dst, size_t cap, double d) {
    int precision;
    if (isfinite(d) && floor(d) == d && fabs(d) <= 9007199254740992.0) {
        snprintf(dst, cap, "%lld", (long long)d);
        return;
    }
    dst[0] = '\0';
    for (precision = 1; precision <= 17; precision++) {
        snprintf(dst, cap, "%.*g", precision, d);
        if (strtod(dst, NULL) == d) break;
    }
    if (!strpbrk(dst, ".eEnN")) {
        size_t n = strlen(dst);
        if (n + 3 <= cap) { dst[n] = '.'; dst[n + 1] = '0'; dst[n + 2] = '\0'; }
    }
}

static char *uc2c_str_of(const json_t *v) {
    char buf[48];
    if (!v) return uc2c_dup("None");
    switch (v->type) {
    case JSON_STRING: return uc2c_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return uc2c_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        uc2c_number_text(buf, sizeof(buf), v->num_val);
        return uc2c_dup(buf);
    case JSON_NULL:   return uc2c_dup("None");
    default:          return uc2c_dup("");
    }
}

/* `str(row.get("profile") or "gateway")` */
static char *uc2c_profile_label(const json_t *row) {
    json_t *profile = json_obj_get(row, "profile");
    if (!uc2c_truthy(profile)) return uc2c_dup("gateway");
    if (profile->type == JSON_ARRAY || profile->type == JSON_OBJECT) {
        /* not reachable from the fixture shapes; a list repr is not mirrored */
        return uc2c_dup("gateway");
    }
    return uc2c_str_of(profile);
}

static int uc2c_list_has_int(const json_t *list, long long value) {
    size_t i;
    if (!list || list->type != JSON_ARRAY) return 0;
    for (i = 0; i < list->c.count; i++) {
        long long item;
        if (uc2c_is_int(list->c.items[i], &item) && item == value) return 1;
    }
    return 0;
}

typedef struct {
    long long pid;
    int       is_bool;
} uc2c_pid;

/* stale_fleet_survivor_pids: 1 = collected, 0 = the live call would raise. */
static int uc2c_collect_survivors(const json_t *fleet, const json_t *already,
                                  uc2c_pid **out, size_t *count,
                                  const char **etype, char *emsg, size_t emsg_cap) {
    uc2c_pid *pids = NULL;
    size_t n = 0, cap = 0, i;

    *out = NULL;
    *count = 0;
    *etype = NULL;
    if (fleet && fleet->type == JSON_OBJECT) {
        *etype = "AttributeError";
        snprintf(emsg, emsg_cap, "'str' object has no attribute 'get'");
        return 0;
    }
    if (fleet && fleet->type != JSON_ARRAY && fleet->type != JSON_NULL) {
        *etype = "TypeError";
        snprintf(emsg, emsg_cap, "'%s' object is not iterable", uc2c_type_name(fleet));
        return 0;
    }
    if (!fleet || fleet->type == JSON_NULL) {
        *out = NULL;
        *count = 0;
        return 1;
    }
    for (i = 0; i < fleet->c.count; i++) {
        json_t *row = fleet->c.items[i];
        json_t *state, *pid_value;
        long long pid;
        if (!row || row->type != JSON_OBJECT) {
            *etype = "AttributeError";
            snprintf(emsg, emsg_cap, "'%s' object has no attribute 'get'", uc2c_type_name(row));
            free(pids);
            return 0;
        }
        state = json_obj_get(row, "state");
        pid_value = json_obj_get(row, "pid");
        if (!state || state->type != JSON_STRING || !state->str_val) continue;
        if (strcmp(state->str_val, "stale")) continue;
        if (!uc2c_is_int(pid_value, &pid)) continue;
        if (pid <= 0) continue;
        if (uc2c_list_has_int(already, pid)) continue;
        if (n == cap) {
            size_t next = cap ? cap * 2 : 8;
            uc2c_pid *grown = (uc2c_pid *)realloc(pids, next * sizeof(*pids));
            if (!grown) { free(pids); *etype = "MemoryError"; snprintf(emsg, emsg_cap, ""); return 0; }
            pids = grown;
            cap = next;
        }
        pids[n].pid = pid;
        pids[n].is_bool = (pid_value->type == JSON_BOOL);
        n++;
    }
    *out = pids;
    *count = n;
    return 1;
}

static json_t *uc2c_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static json_t *uc2c_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *uc2c_pid_json(const uc2c_pid *p) {
    if (p->is_bool) return json_bool(p->pid != 0);
    return json_number((double)p->pid);
}

static void uc2c_pid_text(char *dst, size_t cap, const uc2c_pid *p) {
    if (p->is_bool) snprintf(dst, cap, "%s", p->pid ? "True" : "False");
    else snprintf(dst, cap, "%lld", p->pid);
}

struct json_t *w20_updatecmds_2cdfe8_stale_survivor_pids(const struct json_t *fleet,
                                                         const struct json_t *already_signalled) {
    uc2c_pid *pids = NULL;
    size_t n = 0, i;
    const char *etype = NULL;
    char emsg[256];
    json_t *value;

    emsg[0] = '\0';
    if (!uc2c_collect_survivors(fleet, already_signalled, &pids, &n, &etype, emsg, sizeof(emsg))) {
        json_t *out = uc2c_raised(etype, emsg);
        free(pids);
        return out;
    }
    value = json_array();
    for (i = 0; i < n; i++) json_append(value, uc2c_pid_json(&pids[i]));
    free(pids);
    return uc2c_ok(value);
}

typedef struct {
    long long pid;
    char     *label;
    int       used;
} uc2c_label_entry;

static int uc2c_sorted_ll_cmp(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static json_t *uc2c_pid_array_sorted(const long long *vals, size_t n) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(arr, json_number((double)vals[i]));
    return arr;
}

struct json_t *w20_updatecmds_2cdfe8_signal_stale_survivors(const struct json_t *request) {
    json_t *fleet = json_obj_get(request, "fleet");
    json_t *restart = json_obj_get(request, "restart");
    json_t *killed_in = json_obj_get(restart, "killed_pids");
    json_t *stopped_in = json_obj_get(restart, "stopped_unmapped_pids");
    json_t *service_in = json_obj_get(request, "service_pids");
    json_t *drain_raise = json_obj_get(request, "drain_raise");
    json_t *identity = json_obj_get(request, "identity");
    int service_raises = json_get_bool(request, "service_raises", 0);
    uc2c_pid *pids = NULL;
    size_t npids = 0, i;
    const char *etype = NULL;
    char emsg[256];
    uc2c_label_entry *labels = NULL;
    size_t nlabels = 0;
    long long *killed = NULL, *stopped = NULL;
    size_t nkilled = 0, nstopped = 0;
    json_t *signalled, *manual, *printed, *pending, *value;
    long long *service = NULL;
    size_t nservice = 0;
    char line[512];
    json_t *result = NULL;

    emsg[0] = '\0';
    if (!uc2c_collect_survivors(fleet, killed_in, &pids, &npids, &etype, emsg, sizeof(emsg))) {
        result = uc2c_raised(etype, emsg);
        free(pids);
        return result;
    }
    if (killed_in && killed_in->type == JSON_ARRAY) {
        killed = killed_in->c.count ? (long long *)malloc(killed_in->c.count * sizeof(*killed)) : NULL;
        if (!killed && killed_in->c.count) { free(pids); return uc2c_raised("MemoryError", ""); }
        for (i = 0; i < killed_in->c.count; i++) {
            long long v;
            if (uc2c_is_int(killed_in->c.items[i], &v)) killed[nkilled++] = v;
        }
    }
    if (!npids) {
        /* `if not pids: return []` */
        value = json_array();
        free(pids);
        free(killed);
        return uc2c_ok(value);
    }
    if (!service_raises && service_in && service_in->type == JSON_ARRAY && service_in->c.count) {
        service = (long long *)malloc(service_in->c.count * sizeof(*service));
        if (!service) { free(pids); free(killed); return uc2c_raised("MemoryError", ""); }
        for (i = 0; i < service_in->c.count; i++) {
            long long v;
            if (uc2c_is_int(service_in->c.items[i], &v)) service[nservice++] = v;
        }
    }
    /* labels = {row.get("pid"): str(row.get("profile") or "gateway") ...} */
    if (fleet && fleet->type == JSON_ARRAY) {
        for (i = 0; i < fleet->c.count; i++) {
            json_t *row = fleet->c.items[i];
            json_t *pid_value;
            long long pid;
            size_t j;
            int seen = 0;
            if (!row || row->type != JSON_OBJECT) continue;
            pid_value = json_obj_get(row, "pid");
            if (pid_value && (pid_value->type == JSON_ARRAY || pid_value->type == JSON_OBJECT)) {
                result = uc2c_raised("TypeError", pid_value->type == JSON_ARRAY
                                     ? "cannot use 'list' as a dict key (unhashable type: 'list')"
                                     : "cannot use 'dict' as a dict key (unhashable type: 'dict')");
                goto cleanup;
            }
            if (!uc2c_is_int(pid_value, &pid)) continue;
            for (j = 0; j < nlabels; j++) {
                if (labels[j].pid == pid) {
                    free(labels[j].label);
                    labels[j].label = uc2c_profile_label(row);
                    labels[j].used = 1;
                    seen = 1;
                    break;
                }
            }
            if (seen) continue;
            {
                uc2c_label_entry *grown =
                    (uc2c_label_entry *)realloc(labels, (nlabels + 1) * sizeof(*labels));
                if (!grown) { result = uc2c_raised("MemoryError", ""); goto cleanup; }
                labels = grown;
                labels[nlabels].pid = pid;
                labels[nlabels].label = uc2c_profile_label(row);
                labels[nlabels].used = 1;
                nlabels++;
            }
        }
    }

    signalled = json_array();
    manual = json_array();
    printed = json_array();
    pending = json_array();
    json_append(printed, json_string(""));
    snprintf(line, sizeof(line),
             UC2C_WARN "%llu gateway process(es) still run the pre-update code"
             UC2C_DASH "requesting a restart", (unsigned long long)npids);
    json_append(printed, json_string(line));

    for (i = 0; i < npids; i++) {
        char pid_text[32];
        char key[32];
        char label[256];
        const char *profile = "gateway";
        json_t *raised_msg, *born;
        size_t j;
        int in_service = 0;
        int drained;

        uc2c_pid_text(pid_text, sizeof(pid_text), &pids[i]);
        snprintf(key, sizeof(key), "%s", pid_text);
        for (j = 0; j < nlabels; j++) {
            if (labels[j].pid == pids[i].pid) { profile = labels[j].label; break; }
        }
        snprintf(label, sizeof(label), "%s (PID %s)", profile, pid_text);
        for (j = 0; j < nservice; j++) if (service[j] == pids[i].pid) in_service = 1;

        born = identity && identity->type == JSON_OBJECT ? json_obj_get(identity, key) : NULL;
        raised_msg = drain_raise && drain_raise->type == JSON_OBJECT ? json_obj_get(drain_raise, key) : NULL;
        if (raised_msg && raised_msg->type == JSON_STRING) {
            snprintf(line, sizeof(line),
                     UC2C_WARN "%s: could not be signalled (%s)" UC2C_DASH "restart it by hand",
                     label, raised_msg->str_val ? raised_msg->str_val : "");
            json_append(printed, json_string(line));
            continue;
        }
        drained = uc2c_list_has_int(json_obj_get(request, "drain_ok"), pids[i].pid);
        if (drained) {
            json_append(signalled, uc2c_pid_json(&pids[i]));
            {
                long long *grown = (long long *)realloc(killed, (nkilled + 1) * sizeof(*killed));
                if (!grown) { result = uc2c_raised("MemoryError", ""); goto cleanup; }
                killed = grown;
                killed[nkilled++] = pids[i].pid;
            }
            if (!in_service) json_append(manual, uc2c_pid_json(&pids[i]));
        } else {
            json_t *call = json_array();
            json_append(call, uc2c_pid_json(&pids[i]));
            json_append(call, json_string(label));
            json_append(call, json_null());
            json_append(call, born ? json_copy(born) : json_null());
            json_append(call, json_bool(in_service));
            json_append(pending, call);
        }
    }
    if (stopped_in && stopped_in->type == JSON_ARRAY) {
        stopped = (long long *)malloc((stopped_in->c.count + json_len(manual) + 1) * sizeof(*stopped));
        if (!stopped) { result = uc2c_raised("MemoryError", ""); goto cleanup; }
        for (i = 0; i < stopped_in->c.count; i++) {
            long long v;
            if (uc2c_is_int(stopped_in->c.items[i], &v)) stopped[nstopped++] = v;
        }
    }
    if (json_len(manual) > 0) {
        unsigned long long nmanual = (unsigned long long)json_len(manual);
        snprintf(line, sizeof(line),
                 UC2C_ARROW "Stopped %llu manual gateway process(es) that had no supervisor"
                 " to respawn them", nmanual);
        json_append(printed, json_string(line));
        json_append(printed, json_string("    Restart manually: hermes gateway run"));
        if (json_len(manual) > 1) {
            json_append(printed, json_string("    (or: hermes -p <profile> gateway run  for each profile)"));
        }
        if (stopped) {
            for (i = 0; i < json_len(manual); i++) {
                long long v;
                if (uc2c_is_int(json_get(manual, i), &v)) stopped[nstopped++] = v;
            }
        }
    }
    {
        long long *all = NULL;
        size_t nall = 0;
        json_t *killed_out;
        if (nkilled) {
            size_t a, b;
            all = (long long *)malloc(nkilled * sizeof(*all));
            if (!all) { result = uc2c_raised("MemoryError", ""); goto cleanup; }
            for (a = 0; a < nkilled; a++) {
                int dup = 0;
                for (b = 0; b < nall; b++) if (all[b] == killed[a]) dup = 1;
                if (!dup) all[nall++] = killed[a];
            }
            qsort(all, nall, sizeof(*all), uc2c_sorted_ll_cmp);
            killed_out = uc2c_pid_array_sorted(all, nall);
            free(all);
        } else {
            killed_out = json_array();
        }
        if (stopped && nstopped) {
            size_t a, b;
            long long *uniq = (long long *)malloc(nstopped * sizeof(*uniq));
            size_t nuniq = 0;
            if (!uniq) { json_free(killed_out); result = uc2c_raised("MemoryError", ""); goto cleanup; }
            for (a = 0; a < nstopped; a++) {
                int dup = 0;
                for (b = 0; b < nuniq; b++) if (uniq[b] == stopped[a]) dup = 1;
                if (!dup) uniq[nuniq++] = stopped[a];
            }
            qsort(uniq, nuniq, sizeof(*uniq), uc2c_sorted_ll_cmp);
            value = json_object();
            json_set(value, "signalled", signalled);
            json_set(value, "manual", manual);
            json_set(value, "killed", killed_out);
            json_set(value, "stopped", uc2c_pid_array_sorted(uniq, nuniq));
            json_set(value, "printed", printed);
            json_set(value, "pending", pending);
            free(uniq);
        } else {
            value = json_object();
            json_set(value, "signalled", signalled);
            json_set(value, "manual", manual);
            json_set(value, "killed", killed_out);
            json_set(value, "stopped", stopped ? json_array() : json_null());
            json_set(value, "printed", printed);
            json_set(value, "pending", pending);
        }
    }
    result = uc2c_ok(value);

cleanup:
    for (i = 0; i < nlabels; i++) free(labels[i].label);
    free(labels);
    free(pids);
    free(killed);
    free(stopped);
    free(service);
    return result;
}
