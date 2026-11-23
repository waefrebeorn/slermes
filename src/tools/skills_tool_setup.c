/*
 * skills_tool_setup.c -- faithful C11 port of upstream Hermes
 * tools/skills_tool_setup.py (gate w10_skillstool_3abc40).
 *
 * PoP: w10_skillstool_3abc40_is_remote_env_backend @ tools/skills_tool_setup.py:_is_remote_env_backend
 * PoP: w10_skillstool_3abc40_as_dict_list          @ tools/skills_tool_setup.py:_as_dict_list
 * PoP: w10_skillstool_3abc40_clean_str             @ tools/skills_tool_setup.py:_clean_str
 * PoP: w10_skillstool_3abc40_get_required_environment_variables @ tools/skills_tool_setup.py:_get_required_environment_variables
 * PoP: w10_skillstool_3abc40_capture_result        @ tools/skills_tool_setup.py:_capture_result
 * PoP: w10_skillstool_3abc40_build_setup_note      @ tools/skills_tool_setup.py:_build_setup_note
 * PoP: w10_skillstool_3abc40_is_env_var_persisted  @ tools/skills_tool_setup.py:_is_env_var_persisted
 *
 * Prompt/capture callbacks and plugin registry lookups are injected via seams;
 * env values come from the fixture rather than the process environment.
 */
#include "noc_w10_skillstool_3abc40.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 1;
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *o = (char *)malloc(n); if (o) strcpy(o, s ? s : ""); return o;
}
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupstr("");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_NULL: return dupstr("None");
        case JSON_BOOL: return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}
static char *clean_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    const char *s = v->str_val;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t l = strlen(s);
    while (l && isspace((unsigned char)s[l-1])) l--;
    if (!l) return NULL;
    char *o = (char *)malloc(l + 1);
    memcpy(o, s, l); o[l] = '\0';
    return o;
}
static int env_name_ok(const char *s) {
    if (!s || !(*s)) return 0;
    if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_')) return 0;
    for (const char *p = s + 1; *p; p++)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}

json_t *w10_skillstool_3abc40_is_remote_env_backend(const char *backend) {
    static const char *const REMOTE[] = {"docker","modal","ssh","daytona","vercel_sandbox",NULL};
    if (!backend || !*backend || !strcmp(backend, "local")) return json_bool(0);
    for (int i = 0; REMOTE[i]; i++) if (!strcmp(backend, REMOTE[i])) return json_bool(1);
    /* provider_flag(backend, "is_remote", False) -- stubbed False in the oracle. */
    return json_bool(0);
}

json_t *w10_skillstool_3abc40_as_dict_list(const json_t *raw) {
    json_t *out = json_array();
    if (!raw) return out;
    if (raw->type == JSON_OBJECT) json_append(out, json_copy(raw));
    else if (raw->type == JSON_ARRAY) {
        size_t n = json_len(raw);
        for (size_t i = 0; i < n; i++) json_append(out, json_copy(json_get(raw, i)));
    }
    return out;
}

json_t *w10_skillstool_3abc40_clean_str(const json_t *value) {
    char *s = clean_str(value);
    json_t *out = s ? json_string(s) : json_null();
    free(s);
    return out;
}

json_t *w10_skillstool_3abc40_get_required_environment_variables(const json_t *frontmatter) {
    const json_t *fm = (frontmatter && frontmatter->type == JSON_OBJECT) ? frontmatter : NULL;
    json_t *setup_v = fm ? json_obj_get(fm, "setup") : NULL;
    json_t *setup = (setup_v && setup_v->type == JSON_OBJECT) ? setup_v : NULL;
    char *setup_help = setup ? clean_str(json_obj_get(setup, "help")) : NULL;

    /* legacy prerequisites.env_vars */
    json_t *legacy_v = NULL;
    json_t *prereqs = fm ? json_obj_get(fm, "prerequisites") : NULL;
    if (prereqs && prereqs->type == JSON_OBJECT) legacy_v = json_obj_get(prereqs, "env_vars");
    json_t *legacy = json_array();
    if (legacy_v && truthy(legacy_v)) {
        if (legacy_v->type == JSON_STRING) json_append(legacy, json_copy(legacy_v));
        else if (legacy_v->type == JSON_ARRAY) {
            size_t n = json_len(legacy_v);
            for (size_t i = 0; i < n; i++) json_append(legacy, json_copy(json_get(legacy_v, i)));
        }
    }

    /* entries = declared (string -> {"name": s}); then collect_secrets; then legacy */
    json_t *entries = json_array();
    json_t *declared = fm ? json_obj_get(fm, "required_environment_variables") : NULL;
    json_t *decl_list = w10_skillstool_3abc40_as_dict_list(declared);
    size_t nd = json_len(decl_list);
    for (size_t i = 0; i < nd; i++) {
        json_t *it = json_get(decl_list, i);
        if (it && it->type == JSON_STRING) {
            json_t *d = json_object(); json_set(d, "name", json_copy(it)); json_append(entries, d);
        } else if (it && it->type == JSON_OBJECT) {
            json_append(entries, json_copy(it));
        }
    }
    json_free(decl_list);

    json_t *collect_v = setup ? json_obj_get(setup, "collect_secrets") : NULL;
    json_t *collect_list = w10_skillstool_3abc40_as_dict_list(collect_v);
    size_t nc = json_len(collect_list);
    for (size_t i = 0; i < nc; i++) {
        json_t *it = json_get(collect_list, i);
        if (!it || it->type != JSON_OBJECT) continue;
        json_t *d = json_object();
        json_t *envvar = json_obj_get(it, "env_var");
        json_set(d, "name", envvar ? json_copy(envvar) : json_null());
        json_t *prompt = json_obj_get(it, "prompt");
        json_set(d, "prompt", prompt ? json_copy(prompt) : json_null());
        json_t *u = json_obj_get(it, "provider_url");
        if (!truthy(u)) u = json_obj_get(it, "url");
        char *us = u ? py_str(u) : dupstr("");
        char *utd = clean_str(json_string(us));
        json_set(d, "url", utd ? json_string(utd) : json_null());
        free(utd); free(us);
        json_append(entries, d);
    }
    json_free(collect_list);

    size_t nl = json_len(legacy);
    for (size_t i = 0; i < nl; i++) {
        json_t *v = json_get(legacy, i);
        char *s = py_str(v);
        char *t = clean_str(json_string(s));
        if (t) {
            json_t *d = json_object();
            json_set(d, "name", json_string(t));
            json_append(entries, d);
        }
        free(t); free(s);
    }
    json_free(legacy);

    /* Merge into insertion-ordered required, first name wins. */
    json_t *required = json_array();
    size_t ne = json_len(entries);
    for (size_t i = 0; i < ne; i++) {
        json_t *e = json_get(entries, i);
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *nv = json_obj_get(e, "name");
        if (!truthy(nv)) nv = json_obj_get(e, "env_var");
        char *raw_name = py_str(nv);
        char *env_name = clean_str(json_string(raw_name));
        free(raw_name);
        if (!env_name || !env_name_ok(env_name)) { free(env_name); continue; }
        int dup = 0;
        size_t nr = json_len(required);
        for (size_t k = 0; k < nr; k++)
            if (!strcmp(json_get_str(json_get(required, k), "name", ""), env_name)) { dup = 1; break; }
        if (dup) { free(env_name); continue; }

        json_t *norm = json_object();
        json_set(norm, "name", json_string(env_name));
        json_t *pv = json_obj_get(e, "prompt");
        char *praw = NULL;
        if (truthy(pv)) praw = py_str(pv);
        else { size_t n = strlen(env_name) + 32; praw = (char *)malloc(n);
               snprintf(praw, n, "Enter value for %s", env_name); }
        char *prompt = clean_str(json_string(praw));
        json_set(norm, "prompt", json_string(prompt ? prompt : ""));
        free(prompt); free(praw);

        json_t *hv = json_obj_get(e, "help");
        if (!truthy(hv)) hv = json_obj_get(e, "provider_url");
        if (!truthy(hv)) hv = json_obj_get(e, "url");
        char *help_text = NULL;
        if (truthy(hv)) { char *hs = py_str(hv); help_text = clean_str(json_string(hs)); free(hs); }
        if (!help_text && setup_help) help_text = dupstr(setup_help);
        if (help_text) { json_set(norm, "help", json_string(help_text)); free(help_text); }

        json_t *rfv = json_obj_get(e, "required_for");
        if (truthy(rfv)) { char *rs = py_str(rfv); char *rfc = clean_str(json_string(rs));
                           if (rfc) json_set(norm, "required_for", json_string(rfc)); free(rfc); free(rs); }

        if (truthy(json_obj_get(e, "optional"))) json_set(norm, "optional", json_bool(1));

        json_append(required, norm);
        free(env_name);
    }
    json_free(entries);
    free(setup_help);
    return required;
}

json_t *w10_skillstool_3abc40_capture_result(const json_t *missing_names, int setup_skipped, const json_t *gateway_setup_hint) {
    json_t *o = json_object();
    json_set(o, "missing_names", missing_names ? json_copy(missing_names) : json_array());
    json_set(o, "setup_skipped", json_bool(setup_skipped));
    json_set(o, "gateway_setup_hint", gateway_setup_hint ? json_copy(gateway_setup_hint) : json_null());
    return o;
}

json_t *w10_skillstool_3abc40_build_setup_note(const char *readiness_status, const json_t *missing, const json_t *setup_help) {
    if (!readiness_status || strcmp(readiness_status, "setup_needed")) return json_null();
    const char *joined = "";
    char *buf = NULL;
    size_t n = 0, nm = (missing && missing->type == JSON_ARRAY) ? json_len(missing) : 0;
    if (nm) {
        for (size_t i = 0; i < nm; i++) {
            const char *s = jstr(json_get(missing, i));
            n += strlen(s) + 2;
        }
        buf = (char *)malloc(n + 1); buf[0] = '\0';
        for (size_t i = 0; i < nm; i++) {
            if (i) strcat(buf, ", ");
            strcat(buf, jstr(json_get(missing, i)));
        }
        joined = buf;
    } else joined = "required prerequisites";
    char *help = (setup_help && setup_help->type == JSON_STRING) ? (char *)setup_help->str_val : NULL;
    size_t total = strlen(joined) + (help ? strlen(help) : 0) + 96;
    char *out = (char *)malloc(total);
    snprintf(out, total, "Setup needed before using this skill: missing %s.%s%s",
             joined, help ? " " : "", help ? help : "");
    json_t *res = json_string(out);
    free(out); free(buf);
    return res;
}

json_t *w10_skillstool_3abc40_is_env_var_persisted(const char *var_name, const json_t *env_snapshot, const json_t *secret_value) {
    json_t *v = env_snapshot ? json_obj_get(env_snapshot, var_name) : NULL;
    if (v) return json_bool(truthy(v));
    return json_bool(truthy(secret_value));
}
