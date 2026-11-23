/*
 * config_check_diagnostics.c — C home for upstream Hermes
 * hermes_cli/config_check_diagnostics.py.
 *
 * PoP: cfgdiag_config_check_diagnostics @ hermes_cli/config_check_diagnostics.py:_validate_voice
 * PoP: cfgdiag_config_check_diagnostics @ hermes_cli/config_check_diagnostics.py:config_check_diagnostics
 *
 * The bundled-platform manifest scan, the disabled-plugin set and the
 * get_env_value callback are injected seams (the live function late-imports
 * them from config / plugins_discovery). Voice checks are the live
 * config_value_checks._validate_voice logic.
 */
#include "noc_cfgdiag.h"

#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cd_strip_lower(const char *s, char *out, size_t cap) {
    const char *a=s; while(*a && isspace((unsigned char)*a)) a++;
    const char *b=a+strlen(a); while(b>a && isspace((unsigned char)b[-1])) b--;
    size_t n=0, lim=cap?cap-1:0;
    for(const char*p=a;p<b && n<lim;++p) out[n++]=(char)tolower((unsigned char)*p);
    out[n]=0;
}

/* Python repr() for the scalar values _validate_voice reports. */
static void cd_py_repr(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out,cap,"None"); return; }
    if (v->type == JSON_BOOL) { snprintf(out,cap,"%s",v->bool_val?"True":"False"); return; }
    if (v->type == JSON_NUMBER) {
        double d=v->num_val;
        if (d==floor(d) && fabs(d)<9.0e18) snprintf(out,cap,"%lld",(long long)d);
        else snprintf(out,cap,"%g",d);
        return;
    }
    if (v->type == JSON_STRING) {
        const char *s=v->str_val;
        char q = (strchr(s,'\'') && !strchr(s,'"')) ? '"' : '\'';
        size_t n=0; size_t lim=cap?cap-3:0;
        if (n<lim) out[n++]=q;
        for (const char *p=s; *p && n<lim; ++p) {
            unsigned char c=(unsigned char)*p;
            if (c=='\\' || c==(unsigned char)q) { if(n+1<lim) { out[n++]='\\'; out[n++]=(char)c; } }
            else if (c=='\n') { if(n+1<lim){out[n++]='\\';out[n++]='n';} }
            else if (c=='\t') { if(n+1<lim){out[n++]='\\';out[n++]='t';} }
            else if (c=='\r') { if(n+1<lim){out[n++]='\\';out[n++]='r';} }
            else out[n++]=(char)c;
        }
        if (n<lim+2) out[n++]=q;
        out[n]=0;
        return;
    }
    snprintf(out,cap,"None");
}

struct json_t *cfgdiag_voice(const struct json_t *config) {
    json_t *out = json_array();
    const json_t *voice = (config && config->type == JSON_OBJECT) ? json_obj_get(config,"voice") : NULL;
    if (voice && voice->type == JSON_OBJECT && json_has(voice,"auto_tts_mode")) {
        const json_t *raw = json_obj_get(voice,"auto_tts_mode");
        char norm[512];
        if (raw && raw->type == JSON_STRING) cd_strip_lower(raw->str_val,norm,sizeof norm);
        else norm[0]=0;
        if (strcmp(norm,"all") && strcmp(norm,"voice_only")) {
            char repr[1024], msg[2048];
            cd_py_repr(raw,repr,sizeof repr);
            snprintf(msg,sizeof msg,
                "voice.auto_tts_mode must be 'all' or 'voice_only', got %s. "
                "Set voice.auto_tts_mode to all (audio with every reply) or "
                "voice_only (audio only after voice input).", repr);
            json_append(out,json_string(msg));
        }
    }
    if (!(voice && voice->type == JSON_OBJECT && json_has(voice,"submit_mode"))) return out;
    const json_t *raw = json_obj_get(voice,"submit_mode");
    char norm[512];
    if (raw && raw->type == JSON_STRING) cd_strip_lower(raw->str_val,norm,sizeof norm);
    else norm[0]=0;
    if (strcmp(norm,"direct") && strcmp(norm,"draft")) {
        char repr[1024], msg[2048];
        cd_py_repr(raw,repr,sizeof repr);
        snprintf(msg,sizeof msg,
            "voice.submit_mode must be 'direct' or 'draft', got %s. "
            "Set voice.submit_mode to direct (submit immediately) or draft (edit before sending).",
            repr);
        json_append(out,json_string(msg));
    }
    return out;
}

/* Python truthiness of get_env_value(env) (None / "" / 0 / false are falsy). */
static int cd_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 1;
}

struct json_t *cfgdiag_disabled_platforms(const struct json_t *manifests,
                                          const struct json_t *disabled,
                                          const struct json_t *env) {
    json_t *out = json_array();
    if (!manifests || manifests->type != JSON_ARRAY) return out;
    for (size_t i=0; i<json_len(manifests); ++i) {
        const json_t *m = json_get(manifests,i);
        if (!m || m->type != JSON_OBJECT) continue;
        const char *name = json_get_str(m,"name","");
        char key[1024];
        snprintf(key,sizeof key,"platforms/%s",name ? name : "");
        const json_t *mn = json_obj_get(m,"manifest_name");
        const char *manifest_name = (mn && mn->type == JSON_STRING) ? mn->str_val : "None";
        int is_disabled = 0;
        if (disabled && disabled->type == JSON_ARRAY) {
            for (size_t j=0;j<json_len(disabled);++j) {
                json_t *dv = json_get(disabled,j);
                const char *d = (dv && dv->type == JSON_STRING) ? dv->str_val : "";
                if (!strcmp(d,key) || !strcmp(d,manifest_name)) { is_disabled=1; break; }
            }
        }
        if (!is_disabled) continue;
        const json_t *req = json_obj_get(m,"required");
        size_t nreq = (req && req->type == JSON_ARRAY) ? json_len(req) : 0;
        int all = nreq > 0;
        for (size_t j=0;j<nreq && all;++j) {
            json_t *envname_v = json_get(req,j);
            const char *envname = (envname_v && envname_v->type == JSON_STRING) ? envname_v->str_val : "";
            if (!cd_truthy(json_obj_get(env,envname))) all=0;
        }
        if (!all) continue;
        char msg[2048];
        snprintf(msg,sizeof msg,
            "platform plugin '%s' is disabled while its required credentials are configured. "
            "Run `hermes plugins enable %s` if you want it active.", key, key);
        json_append(out,json_string(msg));
    }
    return out;
}
