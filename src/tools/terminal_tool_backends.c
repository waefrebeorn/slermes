/*
 * terminal_tool_backends.c - C home for upstream Hermes tools/terminal_tool_backends.py.
 *
 * PoP: w16_terminalto_f0d0d6_ssh_config @ tools/terminal_tool_backends.py:_ssh_config_from_config
 * PoP: w16_terminalto_f0d0d6_local_config @ tools/terminal_tool_backends.py:_local_config_from_config
 * PoP: w16_terminalto_f0d0d6_container_config @ tools/terminal_tool_backends.py:_container_config_from_config
 * PoP: w16_terminalto_f0d0d6_resources @ tools/terminal_tool_backends.py:_resources
 * PoP: w16_terminalto_f0d0d6_is_supported_vercel_runtime @ tools/terminal_tool_backends.py:_is_supported_vercel_runtime
 * PoP: w16_terminalto_f0d0d6_check_vercel @ tools/terminal_tool_backends.py:_check_vercel
 * PoP: w16_terminalto_f0d0d6_ssh_pre @ tools/terminal_tool_backends.py:_ssh_pre
 *
 * SDK/module discovery and secret lookups are injected from the fixture; the
 * config-shaping tables and requirement-check messages mirror the live module.
 */
#include "noc_w16_terminalto_f0d0d6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : ""; }
static int is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static int is_nullish(const json_t *v) { return !v || v->type == JSON_NULL; }

/* config.get(key, default) with a freshly-built default. */
static json_t *cfgget(const json_t *cfg, const char *key, json_t *def) {
    json_t *v = json_obj_get(cfg, key);
    if (!v) return def;
    return json_copy(v);
}
static json_t *mkdefault(char t) {
    switch (t) {
        case 's': return json_string("");
        case 'n': return json_number(0);
        case 'b': return json_bool(false);
        case 'a': return json_array();
        case 'o': return json_object();
        default:  return json_null();
    }
}

/* --- config shapers --- */
typedef struct { const char *out, *key; char t; double num; int b; const char *str; } KeyDef;
static json_t *shape(const json_t *cfg, const KeyDef *defs, size_t n) {
    json_t *out = json_object();
    for (size_t i = 0; i < n; ++i) {
        json_t *def;
        switch (defs[i].t) {
            case 's': def = json_string(defs[i].str); break;
            case 'n': def = json_number(defs[i].num); break;
            case 'b': def = json_bool(defs[i].b ? true : false); break;
            default:  def = mkdefault(defs[i].t); break;
        }
        json_set(out, defs[i].out, cfgget(cfg, defs[i].key, def));
    }
    return out;
}

json_t *w16_terminalto_f0d0d6_ssh_config(const json_t *config) {
    static const KeyDef D[] = {
        {"host","ssh_host",'s',0,0,""}, {"user","ssh_user",'s',0,0,""},
        {"port","ssh_port",'n',22,0,NULL}, {"key","ssh_key",'s',0,0,""},
        {"persistent","ssh_persistent",'b',0,1,NULL},
    };
    return shape(config, D, sizeof(D)/sizeof(D[0]));
}
json_t *w16_terminalto_f0d0d6_local_config(const json_t *config) {
    static const KeyDef D[] = {{"persistent","local_persistent",'b',0,1,NULL}};
    return shape(config, D, 1);
}
json_t *w16_terminalto_f0d0d6_resources(const json_t *cc) {
    static const KeyDef D[] = {
        {"cpu","container_cpu",'n',1,0,NULL}, {"memory","container_memory",'n',5120,0,NULL},
        {"disk","container_disk",'n',51200,0,NULL},
        {"persistent_filesystem","container_persistent",'b',0,1,NULL},
    };
    return shape(cc, D, sizeof(D)/sizeof(D[0]));
}
json_t *w16_terminalto_f0d0d6_container_config(const json_t *config) {
    static const KeyDef D[] = {
        {"container_cpu","container_cpu",'n',1,0,NULL},
        {"container_memory","container_memory",'n',5120,0,NULL},
        {"container_disk","container_disk",'n',51200,0,NULL},
        {"container_persistent","container_persistent",'b',0,1,NULL},
        {"modal_mode","modal_mode",'s',0,0,"auto"},
        {"vercel_runtime","vercel_runtime",'s',0,0,""},
        {"vercel_image","vercel_image",'s',0,0,""},
        {"docker_volumes","docker_volumes",'a',0,0,NULL},
        {"docker_mount_cwd_to_workspace","docker_mount_cwd_to_workspace",'b',0,0,NULL},
        {"docker_forward_env","docker_forward_env",'a',0,0,NULL},
        {"docker_env","docker_env",'o',0,0,NULL},
        {"docker_run_as_host_user","docker_run_as_host_user",'b',0,0,NULL},
        {"docker_extra_args","docker_extra_args",'a',0,0,NULL},
        {"docker_shm_size","docker_shm_size",'s',0,0,"1g"},
        {"docker_network","docker_network",'b',0,1,NULL},
        {"docker_persist_across_processes","docker_persist_across_processes",'b',0,1,NULL},
        {"docker_shared_container_key","docker_shared_container_key",'s',0,0,""},
        {"docker_orphan_reaper","docker_orphan_reaper",'b',0,1,NULL},
        {"docker_snap_compat","docker_snap_compat",'b',0,0,NULL},
        {"docker_image_pinned","docker_image_pinned",'b',0,0,NULL},
    };
    return shape(config, D, sizeof(D)/sizeof(D[0]));
}

/* --- requirement checkers --- */
static int supported_runtime(const char *runtime) {
    if (!runtime || !*runtime) return 1;
    return !strcmp(runtime,"node24") || !strcmp(runtime,"node22") ||
           !strcmp(runtime,"python3.13");
}
json_t *w16_terminalto_f0d0d6_is_supported_vercel_runtime(const char *runtime) {
    return json_bool(supported_runtime(runtime) ? true : false);
}

/* Python repr() of a simple string with single quotes. */
static void pyrepr(const char *s, char *out, size_t cap) {
    size_t p = 0;
    out[p++] = '\'';
    for (const char *c = s; *c && p + 3 < cap; ++c) {
        if (*c == '\\' || *c == '\'') out[p++] = '\\';
        out[p++] = *c;
    }
    out[p++] = '\''; out[p] = '\0';
}
static json_t *verdict(int ok, const char *reason) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    json_set(o, "reason", reason && *reason ? json_string(reason) : json_null());
    return o;
}
static const char *secret(const json_t *secrets, const char *k) {
    json_t *v = json_obj_get(secrets, k);
    if (is_nullish(v)) return NULL;
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL && !v->bool_val) return NULL;
    if (v->type == JSON_NUMBER && v->num_val == 0) return NULL;
    return "";
}

json_t *w16_terminalto_f0d0d6_check_vercel(const json_t *config, int vercel_installed,
                                          const json_t *secrets) {
    char reason[2048] = {0};
    char runtime[512], repr[1024];
    const char *rt = jstr(cfgget(config, "vercel_runtime", json_string("")));
    /* trim (strip) */
    size_t a = 0, b = strlen(rt);
    while (a < b && (rt[a]==' '||rt[a]=='\t'||rt[a]=='\n'||rt[a]=='\r')) a++;
    while (b > a && (rt[b-1]==' '||rt[b-1]=='\t'||rt[b-1]=='\n'||rt[b-1]=='\r')) b--;
    memcpy(runtime, rt + a, b - a); runtime[b-a] = '\0';
    if (!supported_runtime(runtime)) {
        pyrepr(runtime, repr, sizeof(repr));
        snprintf(reason, sizeof(reason),
                 "Vercel Sandbox runtime %s is not supported. Set TERMINAL_VERCEL_RUNTIME to "
                 "one of: node24, node22, python3.13.", repr);
        return verdict(0, reason);
    }
    json_t *dv = cfgget(config, "container_disk", json_number(51200));
    double disk = (dv && dv->type == JSON_NUMBER) ? dv->num_val : -1;
    if (disk != 0 && disk != 51200) {
        snprintf(reason, sizeof(reason),
                 "Vercel Sandbox does not support custom TERMINAL_CONTAINER_DISK=%g. Use the "
                 "default shared setting (51200 MB).", disk);
        return verdict(0, reason);
    }
    if (!vercel_installed) {
        snprintf(reason, sizeof(reason), "%s",
                 "vercel is required for the Vercel Sandbox terminal backend. Run hermes setup "
                 "terminal and select Vercel Sandbox.");
        return verdict(0, reason);
    }
    if (secret(secrets, "VERCEL_OIDC_TOKEN")) return verdict(1, NULL);
    int t = !!secret(secrets, "VERCEL_TOKEN");
    int p = !!secret(secrets, "VERCEL_PROJECT_ID");
    int m = !!secret(secrets, "VERCEL_TEAM_ID");
    if (t && p && m) return verdict(1, NULL);
    const char *head = (t || p || m)
        ? "selected with token auth, but VERCEL_TOKEN, VERCEL_PROJECT_ID, and VERCEL_TEAM_ID "
          "must all be set together."
        : "selected but no supported auth configuration was found. Set VERCEL_TOKEN, "
          "VERCEL_PROJECT_ID, and VERCEL_TEAM_ID for normal use.";
    snprintf(reason, sizeof(reason),
             "Vercel Sandbox backend %s VERCEL_OIDC_TOKEN is supported for one-off local "
             "development only.", head);
    return verdict(0, reason);
}
json_t *w16_terminalto_f0d0d6_ssh_pre(const json_t *config) {
    json_t *h = cfgget(config, "ssh_host", json_string(""));
    json_t *u = cfgget(config, "ssh_user", json_string(""));
    int ok = (h && h->type == JSON_STRING && h->str_val[0]) &&
             (u && u->type == JSON_STRING && u->str_val[0]);
    if (ok) return verdict(1, NULL);
    return verdict(0,
        "the SSH host and user are not configured (TERMINAL_SSH_HOST / TERMINAL_SSH_USER); "
        "run `hermes setup terminal` to enter them or pick the 'local' backend");
}
