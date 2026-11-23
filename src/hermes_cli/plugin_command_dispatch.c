/*
 * plugin_command_dispatch.c — C home for upstream Hermes
 * hermes_cli/plugin_command_dispatch.py.
 *
 * PoP: plugcmd_invoke_plugin_command @ hermes_cli/plugin_command_dispatch.py:invoke_plugin_command
 *
 * The handler signature is injected as parameter descriptors; the port
 * reproduces which context keywords reach the handler and whether the legacy
 * one-argument call is used.
 */
#include "noc_plugcmd.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *pc_param(const json_t *p, const char *key) {
    return json_get_str(p, key, "");
}

struct json_t *plugcmd_invoke(const struct json_t *params,
                              const struct json_t *raw_args,
                              const struct json_t *context) {
    json_t *out = json_object();
    json_set(out,"raw_args", json_copy(raw_args));
    json_t *kwargs = json_object();
    size_t nparams = (params && params->type == JSON_ARRAY) ? json_len(params) : 0;

    /* signature() failure -> handler(raw_args), no context. */
    for (size_t i=0;i<nparams;++i)
        if (!strcmp(pc_param(json_get(params,i),"kind"),"NOSIG")) {
            json_set(out,"kwargs",kwargs);
            json_set(out,"legacy",json_bool(true));
            return out;
        }

    size_t start = 0;
    int strip_first = 0;
    const char *first_name = NULL;
    if (nparams) {
        const char *kind = pc_param(json_get(params,0),"kind");
        if (strcmp(kind,"KEYWORD_ONLY")) {
            if (!strcmp(kind,"POSITIONAL_OR_KEYWORD")) {
                strip_first = 1;
                first_name = pc_param(json_get(params,0),"name");
            }
            start = 1;  /* parameters = parameters[1:] */
        }
    }

    int has_varkw = 0;
    for (size_t i=start;i<nparams;++i)
        if (!strcmp(pc_param(json_get(params,i),"kind"),"VAR_KEYWORD")) { has_varkw=1; break; }

    size_t nctx = (context && context->type == JSON_OBJECT) ? json_object_size(context) : 0;
    for (size_t i=0;i<nctx;++i) {
        const char *k = json_object_get_key_at(context,i);
        if (!k) continue;
        if (strip_first && first_name && !strcmp(k,first_name)) continue;
        json_t *v = json_object_get_at(context,i);
        if (has_varkw) { json_set(kwargs,k,json_copy(v)); continue; }
        int accepted = 0;
        for (size_t j=start;j<nparams;++j) {
            const json_t *p = json_get(params,j);
            const char *kind = pc_param(p,"kind");
            if ((!strcmp(kind,"POSITIONAL_OR_KEYWORD") || !strcmp(kind,"KEYWORD_ONLY")) &&
                !strcmp(pc_param(p,"name"),k)) { accepted=1; break; }
        }
        if (accepted) json_set(kwargs,k,json_copy(v));
    }
    json_set(out,"kwargs",kwargs);
    json_set(out,"legacy",json_bool(false));
    return out;
}
