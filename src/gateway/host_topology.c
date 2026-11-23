/*
 * host_topology.c — C home for upstream Hermes gateway/host_topology.py.
 *
 * PoP: w8_hosttopolog_1e949c_normalized @ gateway/host_topology.py:_normalized
 * PoP: w8_hosttopolog_1e949c_serves @ gateway/host_topology.py:HostGatewayTopology.serves
 * PoP: w8_hosttopolog_1e949c_describe @ gateway/host_topology.py:HostGatewayTopology.describe
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_hosttopolog_1e949c.h"

/* Mirror of hermes_cli.profiles.normalize_profile_name for string names:
 * strip(), casefold()=="default" -> "default", else lower(). */
static char *w8_normalize_name(const char *name) {
    size_t n = strlen(name);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)name[a])) a++;
    while (b > a && isspace((unsigned char)name[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = a; i < b; i++) out[j++] = (char)tolower((unsigned char)name[i]);
    out[j] = '\0';
    if (!strcmp(out, "default")) { free(out); return strdup("default"); }
    return out;
}

json_t *w8_hosttopolog_1e949c_normalized(const json_t *name) {
    /* Python `_normalized`: falsy (None / "") -> ""; otherwise normalize. */
    if (!name || name->type == JSON_NULL || name->type != JSON_STRING
        || name->str_val[0] == '\0')
        return json_string("");
    char *norm = w8_normalize_name(name->str_val);
    if (!norm) return json_string("");
    json_t *out = json_string(norm);
    free(norm);
    return out;
}

json_t *w8_hosttopolog_1e949c_serves(const json_t *profiles, const json_t *wanted) {
    if (!wanted || wanted->type != JSON_STRING || wanted->str_val[0] == '\0')
        return json_bool(false);
    char *wn = w8_normalize_name(wanted->str_val);
    if (!wn || wn[0] == '\0') { free(wn); return json_bool(false); }
    bool found = false;
    if (profiles && profiles->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(profiles); i++) {
            json_t *p = json_get(profiles, i);
            if (!p || p->type != JSON_STRING) continue; /* Python would raise; fixtures avoid */
            char *pn = w8_normalize_name(p->str_val);
            if (pn && !strcmp(pn, wn)) { found = true; free(pn); break; }
            free(pn);
        }
    }
    free(wn);
    return json_bool(found);
}

json_t *w8_hosttopolog_1e949c_describe(const char *pid, const json_t *profiles) {
    long long p = strtoll(pid ? pid : "0", NULL, 10);
    char *roster = NULL;
    size_t cap = 0, used = 0;
    if (profiles && profiles->type == JSON_ARRAY && json_len(profiles) > 0) {
        for (size_t i = 0; i < json_len(profiles); i++) {
            json_t *v = json_get(profiles, i);
            const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
            size_t sl = strlen(s);
            if (used + sl + 3 > cap) {
                cap = (used + sl + 3) * 2 + 16;
                char *tmp = (char *)realloc(roster, cap);
                if (!tmp) break;
                roster = tmp;
            }
            if (i) roster[used++] = ',', roster[used++] = ' ';
            memcpy(roster + used, s, sl);
            used += sl;
        }
        if (roster) roster[used] = '\0';
    }
    size_t outsz = 160 + used + strlen(pid ? pid : "0");
    char *buf = (char *)malloc(outsz);
    if (!buf) { free(roster); return json_string(""); }
    snprintf(buf, outsz, "the host gateway (PID %lld) serving profiles %s",
             p, roster && roster[0] ? roster : "an unknown profile set");
    free(roster);
    json_t *out = json_string(buf);
    free(buf);
    return out;
}
