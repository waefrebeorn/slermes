/*
 * models_profile_cache.c — C home for upstream Hermes hermes_cli/models_profile_cache.py.
 *
 * PoP: profile_slot_get @ hermes_cli/models_profile_cache.py:profile_slot_get
 * PoP: profile_slot_set @ hermes_cli/models_profile_cache.py:profile_slot_set
 *
 * Under a HERMES_HOME override the value lives in _SLOTS_BY_HOME[(home, attr)];
 * without one it is the module attribute. The override/key are injected seams.
 */
#include "noc_profilecache.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PC_MAX 64
static char *pc_mkey[PC_MAX]; static json_t *pc_mval[PC_MAX]; static size_t pc_nm;
static char *pc_shome[PC_MAX]; static char *pc_sattr[PC_MAX];
static json_t *pc_sval[PC_MAX]; static size_t pc_ns;

static void pc_reset(void) {
    for (size_t i=0;i<pc_nm;++i) { free(pc_mkey[i]); json_free(pc_mval[i]); }
    for (size_t i=0;i<pc_ns;++i) { free(pc_shome[i]); free(pc_sattr[i]); json_free(pc_sval[i]); }
    pc_nm=pc_ns=0;
    memset(pc_mkey,0,sizeof pc_mkey); memset(pc_mval,0,sizeof pc_mval);
    memset(pc_shome,0,sizeof pc_shome); memset(pc_sattr,0,sizeof pc_sattr);
    memset(pc_sval,0,sizeof pc_sval);
}
static size_t pc_mod(const char *k) {
    for (size_t i=0;i<pc_nm;++i) if (!strcmp(pc_mkey[i],k)) return i;
    if (pc_nm<PC_MAX) { pc_mkey[pc_nm]=strdup(k); pc_mval[pc_nm]=NULL; return pc_nm++; }
    return (size_t)-1;
}
static size_t pc_slot(const char *home, const char *attr) {
    for (size_t i=0;i<pc_ns;++i)
        if (!strcmp(pc_shome[i],home) && !strcmp(pc_sattr[i],attr)) return i;
    if (pc_ns<PC_MAX) {
        pc_shome[pc_ns]=strdup(home); pc_sattr[pc_ns]=strdup(attr);
        pc_sval[pc_ns]=NULL; return pc_ns++;
    }
    return (size_t)-1;
}

struct json_t *profilecache_run(const struct json_t *initial, const struct json_t *ops) {
    pc_reset();
    json_t *results = json_array();
    if (initial && initial->type == JSON_OBJECT) {
        for (size_t i=0;i<json_object_size(initial);++i) {
            const char *k = json_object_get_key_at(initial,i);
            if (!k) continue;
            size_t mi = pc_mod(k);
            if (mi != (size_t)-1) json_free(pc_mval[mi]);
            pc_mval[mi] = json_copy(json_object_get_at(initial,i));
        }
    }
    if (!ops || ops->type != JSON_ARRAY) return results;
    for (size_t oi=0; oi<json_len(ops); ++oi) {
        const json_t *op = json_get(ops,oi);
        const char *attr = json_get_str(op,"attr","");
        const json_t *ov = json_obj_get(op,"override");
        int override = (ov && ov->type != JSON_NULL);  /* None when null/missing */
        const char *home = json_get_str(op,"home","");
        const char *what = json_get_str(op,"op","");
        if (!strcmp(what,"set")) {
            json_t *value = json_copy(json_obj_get(op,"value"));
            if (!override) {
                size_t mi=pc_mod(attr);
                if (mi!=(size_t)-1) { json_free(pc_mval[mi]); pc_mval[mi]=value; }
                else json_free(value);
            } else {
                size_t si=pc_slot(home,attr);
                if (si!=(size_t)-1) { json_free(pc_sval[si]); pc_sval[si]=value; }
                else json_free(value);
            }
            continue;
        }
        if (!strcmp(what,"get")) {
            if (!override) {
                size_t mi=pc_mod(attr);
                /* Python getattr() raises when absent; fixtures always seed/set first. */
                json_append(results, (mi!=(size_t)-1 && pc_mval[mi]) ?
                                     json_copy(pc_mval[mi]) : json_null());
            } else {
                size_t si=pc_slot(home,attr);
                if (si!=(size_t)-1 && pc_sval[si]) json_append(results,json_copy(pc_sval[si]));
                else json_append(results,json_copy(json_obj_get(op,"default")));
            }
            continue;
        }
        json_append(results,json_null());
    }
    return results;
}
