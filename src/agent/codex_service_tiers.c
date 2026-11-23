/*
 * codex_service_tiers.c - C home for upstream Hermes agent/codex_service_tiers.py.
 * PoP: w11_codexservi_5c6a93_advertised_tiers @ agent/codex_service_tiers.py:_advertised_tiers
 * PoP: w11_codexservi_5c6a93_advertised_cyber_programs @ agent/codex_service_tiers.py:_advertised_cyber_programs
 * PoP: w11_codexservi_5c6a93_daybreak_program @ agent/codex_service_tiers.py:DAYBREAK_PROGRAM
 *
 * Module-level literal constants mirrored from the Python source (provenance):
 *   DAYBREAK_PROGRAM = "daybreak_blue"
 *   _TTL = 3600 ; _MAX_CATALOGS = 128
 *   {"fast": "priority"}  -- additional_speed_tiers alias map
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_codexservi_5c6a93.h"

static const char *const W11CS_DAYBREAK_PROGRAM = "daybreak_blue";
const char *w11_codexservi_5c6a93_daybreak_program(void) { return W11CS_DAYBREAK_PROGRAM; }

/* Python frozenset -> deterministic sorted JSON array of strings (ASCII inputs). */
static json_t *w11cs_strset_to_arr(char **vals, size_t n) {
    /* dedupe */
    char **uniq = (char **)calloc(n ? n : 1, sizeof(char *));
    size_t u = 0;
    for (size_t i=0;i<n;i++) {
        int seen=0;
        for (size_t j=0;j<u;j++) if (strcmp(uniq[j],vals[i])==0) { seen=1; break; }
        if (!seen) uniq[u++]=vals[i];
    }
    for (size_t i=0;i<u;i++) for (size_t j=i+1;j<u;j++)
        if (strcmp(uniq[i],uniq[j])>0) { char *t=uniq[i]; uniq[i]=uniq[j]; uniq[j]=t; }
    json_t *arr=json_array();
    for (size_t i=0;i<u;i++) json_append(arr, json_string(uniq[i]));
    free(uniq);
    return arr;
}
static char *w11cs_dup(const char *s){ return strdup(s ? s : ""); }

json_t *w11_codexservi_5c6a93_advertised_tiers(json_t *entry) {
    json_t *null_out = json_null();
    if (!entry || entry->type != JSON_OBJECT) return null_out;
    json_t *tiers = json_obj_get(entry,"service_tiers");
    if (tiers && tiers->type == JSON_ARRAY) {
        for (size_t i=0;i<tiers->c.count;i++) {
            json_t *item=json_get(tiers,i);
            if (!item || item->type!=JSON_OBJECT) return null_out;
            json_t *id=json_obj_get(item,"id");
            if (!id || id->type!=JSON_STRING) return null_out;
        }
        if (tiers->c.count > 0) {
            char **v=(char **)calloc(tiers->c.count,sizeof(char *));
            for (size_t i=0;i<tiers->c.count;i++) {
                json_t *id=json_obj_get(json_get(tiers,i),"id");
                v[i]=w11cs_dup(id->str_val);
            }
            json_t *out=w11cs_strset_to_arr(v,tiers->c.count);
            for (size_t i=0;i<tiers->c.count;i++) free(v[i]);
            free(v); return out;
        }
    }
    json_t *speeds=json_obj_get(entry,"additional_speed_tiers");
    if (speeds && speeds->type == JSON_ARRAY) {
        int all_str=1;
        for (size_t i=0;i<speeds->c.count;i++) {
            json_t *it=json_get(speeds,i);
            if (!it || it->type!=JSON_STRING) { all_str=0; break; }
        }
        if (all_str) {
            char **v=(char **)calloc(speeds->c.count ? speeds->c.count : 1,sizeof(char *));
            for (size_t i=0;i<speeds->c.count;i++) {
                const char *s=json_get(speeds,i)->str_val;
                v[i]=w11cs_dup(strcmp(s,"fast")==0 ? "priority" : s);
            }
            json_t *out=w11cs_strset_to_arr(v,speeds->c.count);
            for (size_t i=0;i<speeds->c.count;i++) free(v[i]);
            free(v); return out;
        }
    }
    /* frozenset() when tiers == [] ; None otherwise */
    if (tiers && tiers->type == JSON_ARRAY && tiers->c.count == 0) return json_array();
    return null_out;
}
json_t *w11_codexservi_5c6a93_advertised_cyber_programs(json_t *entry) {
    json_t *null_out=json_null();
    if (!entry || entry->type!=JSON_OBJECT) return null_out;
    json_t *programs=json_obj_get(entry,"available_access_programs");
    json_t *cyber = (programs && programs->type==JSON_OBJECT) ? json_obj_get(programs,"cyber") : NULL;
    if (cyber && cyber->type == JSON_ARRAY) {
        for (size_t i=0;i<cyber->c.count;i++) {
            json_t *it=json_get(cyber,i);
            if (!it || it->type!=JSON_STRING) return null_out;
        }
        char **v=(char **)calloc(cyber->c.count ? cyber->c.count : 1,sizeof(char *));
        for (size_t i=0;i<cyber->c.count;i++) v[i]=w11cs_dup(json_get(cyber,i)->str_val);
        json_t *out=w11cs_strset_to_arr(v,cyber->c.count);
        for (size_t i=0;i<cyber->c.count;i++) free(v[i]);
        free(v); return out;
    }
    return null_out;
}
