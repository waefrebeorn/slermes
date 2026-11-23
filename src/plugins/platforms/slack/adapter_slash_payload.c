/*
 * adapter_slash_payload.c — C home for plugins/platforms/slack/adapter_slash_payload.py.
 * PoP: slslash_command_text @ plugins/platforms/slack/adapter_slash_payload.py:slash_command_text
 * PoP: slslash_thread_id @ plugins/platforms/slack/adapter_slash_payload.py:slash_thread_id
 *
 * hermes_cli.commands_platforms.slack_subcommand_map() is a live registry lookup;
 * the deterministic map below is the injected registry value used by the oracle
 * stub.  Both sides then apply the module's compact -> /compress override.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *const SUBCMD_KEYS[] = {
    "new","model","compact","help","status","config","sessions","resume","compress"
};
static const char *const SUBCMD_VALS[] = {
    "/new","/model","/compress","/help","/status","/config","/sessions","/resume","/compress"
};
static const char *map_get(const char *k) {
    for (size_t i = 0; i < sizeof(SUBCMD_KEYS)/sizeof(SUBCMD_KEYS[0]); i++)
        if (!strcmp(SUBCMD_KEYS[i], k)) return SUBCMD_VALS[i];
    return NULL;
}

static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 0 : 1;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return v->str_val[0] == 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 1;
}

/* Python str(x) for the scalar values a Slack payload carries. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return (char *)strdup("");
    if (v->type == JSON_STRING) return (char *)strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return (char *)strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return (char *)strdup(buf);
    }
    return json_dumps(v, 0);
}

static char *trim(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    char *out = (char *)malloc(b - a + 1);
    memcpy(out, s + a, b - a); out[b - a] = 0;
    return out;
}

static char *lstrip_slashes(const char *s) {
    size_t i = 0;
    while (s && s[i] == '/') i++;
    return (char *)strdup((s ? s : "") + i);
}

char *slslash_command_text(const json_t *command) {
    const json_t *cv = command ? json_obj_get(command, "command") : NULL;
    const char *raw_cmd = (cv && cv->type == JSON_STRING) ? cv->str_val : "";
    char *ls = lstrip_slashes(raw_cmd);
    char *slash_name = trim(ls); free(ls);
    char *tv = falsy(json_obj_get(command, "text")) ? (char *)strdup("") : py_str(json_obj_get(command, "text"));

    char *result;
    if (strcmp(slash_name, "hermes") && strcmp(slash_name, "")) {
        size_t need = strlen(slash_name) + strlen(tv) + 4;
        result = (char *)calloc(need, 1);
        if (tv[0]) snprintf(result, need, "/%s %s", slash_name, tv);
        else snprintf(result, need, "/%s", slash_name);
    } else {
        char *legacy = trim(tv);
        /* split() on whitespace; first word (empty when legacy is blank) */
        char first[256]; first[0] = 0;
        const char *p = legacy;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) {
            size_t j = 0;
            while (*p && !isspace((unsigned char)*p) && j + 1 < sizeof(first)) first[j++] = *p++;
            first[j] = 0;
        }
        const char *mapped = map_get(first);
        if (mapped) {
            char *rest = trim(legacy + strlen(first));
            if (rest[0]) {
                size_t need = strlen(mapped) + strlen(rest) + 2;
                char *joined = (char *)calloc(need, 1);
                snprintf(joined, need, "%s %s", mapped, rest);
                result = trim(joined); free(joined);
            } else result = (char *)strdup(mapped);
            free(rest);
        } else {
            result = strdup(legacy[0] ? legacy : "/help");
        }
        free(legacy);
    }
    free(tv); free(slash_name);
    return result;
}

char *slslash_thread_id(const json_t *command) {
    if (!command || command->type != JSON_OBJECT) return NULL;
    const json_t *cands[3]; size_t nc = 0;
    cands[nc++] = command;
    const json_t *n = json_obj_get(command, "message");
    if (n && n->type == JSON_OBJECT) cands[nc++] = n;
    n = json_obj_get(command, "container");
    if (n && n->type == JSON_OBJECT) cands[nc++] = n;
    const char *keys[2] = {"thread_ts", "message_ts"};
    for (int ki = 0; ki < 2; ki++) {
        for (size_t i = 0; i < nc; i++) {
            const json_t *v = json_obj_get(cands[i], keys[ki]);
            if (v && !falsy(v)) return py_str(v);
        }
    }
    return NULL;
}
