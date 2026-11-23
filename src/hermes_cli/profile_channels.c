/*
 * profile_channels.c - C home for upstream Hermes hermes_cli/profile_channels.py.
 * PoP: w15_profilecha_e2c58e_platform_env_prefixes @ hermes_cli/profile_channels.py:platform_env_prefixes
 * PoP: w15_profilecha_e2c58e_env_key_of_line @ hermes_cli/profile_channels.py:_env_key_of_line
 * PoP: w15_profilecha_e2c58e_explicit_enabled @ hermes_cli/profile_channels.py:_explicit_enabled
 * PoP: w15_profilecha_e2c58e_channel_config_paths @ hermes_cli/profile_channels.py:_channel_config_paths
 * PoP: w15_profilecha_e2c58e_shared_credential_warning @ hermes_cli/profile_channels.py:shared_credential_warning
 * PoP: w15_profilecha_e2c58e_format_stripped_notice @ hermes_cli/profile_channels.py:format_stripped_notice
 *
 * Pure channel-inventory helpers: registry/filesystem discovery is injected as JSON
 * inputs, so nothing here touches the process environment or the filesystem.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_profilecha_e2c58e.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return v->c.count > 0;
}

struct json_t *w15_profilecha_e2c58e_platform_env_prefixes(const char *platform_id) {
    const char *pid = platform_id ? platform_id : "";
    size_t n = strlen(pid);
    char *canon = (char *)malloc(n + 2);
    if (!canon) return json_array();
    for (size_t i = 0; i < n; i++) canon[i] = pid[i] == '-' ? '_' : (char)toupper((unsigned char)pid[i]);
    canon[n] = '_';
    canon[n + 1] = 0;
    json_t *r = json_array();
    json_append(r, json_string(canon));
    free(canon);
    if (!strcmp(pid, "email")) json_append(r, json_string("EMAIL_"));
    else if (!strcmp(pid, "qqbot")) json_append(r, json_string("QQ_"));
    else if (!strcmp(pid, "relay")) json_append(r, json_string("GATEWAY_RELAY_"));
    else if (!strcmp(pid, "sms")) json_append(r, json_string("TWILIO_"));
    return r;
}

/* ^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*= */
struct json_t *w15_profilecha_e2c58e_env_key_of_line(const char *line) {
    const char *p = line ? line : "";
    while (is_ws((unsigned char)*p)) p++;
    if (!strncmp(p, "export", 6) && is_ws((unsigned char)p[6])) {
        p += 6;
        while (is_ws((unsigned char)*p)) p++;
    }
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
        return json_null();
    const char *start = p;
    p++;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           (*p >= '0' && *p <= '9') || *p == '_') p++;
    size_t n = (size_t)(p - start);
    while (is_ws((unsigned char)*p)) p++;
    if (*p != '=') return json_null();
    char *key = (char *)malloc(n + 1);
    if (!key) return json_null();
    memcpy(key, start, n);
    key[n] = 0;
    json_t *r = json_string(key);
    free(key);
    return r;
}

struct json_t *w15_profilecha_e2c58e_explicit_enabled(const struct json_t *raw, const char *pid) {
    json_t *gateway = (raw && raw->type == JSON_OBJECT) ? json_obj_get(raw, "gateway") : NULL;
    json_t *gw = (gateway && gateway->type == JSON_OBJECT) ? gateway : NULL;
    json_t *sections[2];
    sections[0] = (raw && raw->type == JSON_OBJECT) ? json_obj_get(raw, "platforms") : NULL;
    sections[1] = gw ? json_obj_get(gw, "platforms") : NULL;
    for (int i = 0; i < 2; i++) {
        json_t *section = sections[i];
        json_t *block = (section && section->type == JSON_OBJECT) ? json_obj_get(section, pid) : NULL;
        if (block && block->type == JSON_OBJECT && json_obj_get(block, "enabled"))
            return json_bool(py_truthy(json_obj_get(block, "enabled")) ? true : false);
    }
    return json_null();
}

static void add_path(json_t *out, const char *a, const char *b) {
    json_t *p = json_array();
    json_append(p, json_string(a));
    if (b) json_append(p, json_string(b));
    json_append(out, p);
}

struct json_t *w15_profilecha_e2c58e_channel_config_paths(const struct json_t *raw,
                                                          const struct json_t *platforms) {
    json_t *out = json_array();
    json_t *gateway = (raw && raw->type == JSON_OBJECT) ? json_obj_get(raw, "gateway") : NULL;
    json_t *gw = (gateway && gateway->type == JSON_OBJECT) ? gateway : NULL;
    if (raw && raw->type == JSON_OBJECT && json_obj_get(raw, "platforms")) add_path(out, "platforms", NULL);
    if (gw && json_obj_get(gw, "platforms")) add_path(out, "gateway", "platforms");
    static const char *const owners[] = {"multiplex_profiles", "profile_routes", NULL};
    for (int i = 0; owners[i]; i++) {
        if (raw && raw->type == JSON_OBJECT && json_obj_get(raw, owners[i])) add_path(out, owners[i], NULL);
        if (gw && json_obj_get(gw, owners[i])) add_path(out, "gateway", owners[i]);
    }
    if (platforms && platforms->type == JSON_ARRAY) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            json_t *v = json_get(platforms, i);
            char *pid = (v && v->type == JSON_STRING) ? dupstr(v->str_val ? v->str_val : "") : dupstr("");
            if (raw && raw->type == JSON_OBJECT && json_obj_get(raw, pid)) add_path(out, pid, NULL);
            if (gw && json_obj_get(gw, pid)) add_path(out, "gateway", pid);
            free(pid);
        }
    }
    return out;
}

/* join a JSON string array with ", " exactly like ', '.join(list) */
static char *join_platforms(const struct json_t *platforms) {
    size_t total = 1;
    if (platforms && platforms->type == JSON_ARRAY) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            json_t *v = json_get(platforms, i);
            total += (v && v->type == JSON_STRING && v->str_val) ? strlen(v->str_val) : 0;
            total += 2;
        }
    }
    char *out = (char *)malloc(total + 8);
    if (!out) return dupstr("");
    size_t m = 0;
    out[m] = 0;
    if (platforms && platforms->type == JSON_ARRAY) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            if (i) { memcpy(out + m, ", ", 2); m += 2; }
            json_t *v = json_get(platforms, i);
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            size_t n = strlen(s);
            memcpy(out + m, s, n);
            m += n;
        }
    }
    out[m] = 0;
    return out;
}

struct json_t *w15_profilecha_e2c58e_shared_credential_warning(const char *profile,
                                                              const struct json_t *platforms,
                                                              const char *source) {
    char *joined = join_platforms(platforms);
    const char *src = (source && source[0]) ? source : "default";
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "\xe2\x9a\xa0 Profile '%s' shares its %s credential with %s: the bot can only belong to one profile. "
        "Give '%s' its own bot (hermes -p %s setup, or the dashboard Messaging page) or remove the token from "
        "'%s'; a multiplexed gateway parks the duplicate and `hermes gateway migrate --multiplex` refuses "
        "until it is gone.",
        profile ? profile : "", joined, src, profile ? profile : "", profile ? profile : "", profile ? profile : "");
    free(joined);
    return json_string(buf);
}

struct json_t *w15_profilecha_e2c58e_format_stripped_notice(const char *profile,
                                                            const struct json_t *platforms,
                                                            const char *clone_flag) {
    json_t *out = json_array();
    if (!platforms || platforms->type != JSON_ARRAY || platforms->c.count == 0) return out;
    char *joined = join_platforms(platforms);
    const char *flag = (clone_flag && clone_flag[0]) ? clone_flag : "--clone";
    char b1[2048], b2[2048], b3[2048];
    snprintf(b1, sizeof(b1),
        "Messaging channels were NOT cloned (%s): a copied bot token or allowlist would make two gateways "
        "fight over one bot.", joined);
    snprintf(b2, sizeof(b2),
        "  Configure this profile's own bots:  hermes -p %s setup   (or the dashboard Messaging page)",
        profile ? profile : "");
    snprintf(b3, sizeof(b3),
        "  To copy the source's channels anyway:  hermes profile create %s %s --clone-channels",
        profile ? profile : "", flag);
    free(joined);
    json_append(out, json_string(b1));
    json_append(out, json_string(b2));
    json_append(out, json_string(b3));
    return out;
}

