/*
 * plugins_content.c — C home for upstream Hermes hermes_cli/plugins_content.py.
 *
 * PoP: w13_pluginscon_bbba05_plugin_skill_owners @ hermes_cli/plugins_content.py:plugin_skill_owners
 * PoP: w13_pluginscon_bbba05_skill_name_valid @ hermes_cli/plugins_content.py:register_skill
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginscon_bbba05.h"

static char *dups_sub(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *dups(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count != 0;
}
static void add_owner(char **owners, size_t *n, const char *s) {
    size_t i;
    for (i = 0; i < *n; i++) if (!strcmp(owners[i], s)) return;
    owners[*n] = dups(s);
    (*n)++;
}
static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static bool valid_skill_name(const char *s) {
    size_t i;
    if (!s || !s[0]) return false;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

json_t *w13_pluginscon_bbba05_plugin_skill_owners(const struct json_t *in) {
    const json_t *skills = json_obj_get(in, "skills");
    const char *qualified = json_get_str(in, "qualified", "");
    const json_t *entry = (skills && skills->type == JSON_OBJECT) ? json_obj_get(skills, qualified) : NULL;
    json_t *out = json_array();
    char *owners[8];
    size_t n = 0, i;
    if (!truthy(entry)) entry = NULL;
    if (entry && entry->type == JSON_OBJECT) {
        const json_t *k = json_obj_get(entry, "plugin_key");
        const json_t *nm = json_obj_get(entry, "plugin_name");
        if (k && k->type == JSON_STRING && k->str_val && k->str_val[0]) add_owner(owners, &n, k->str_val);
        if (nm && nm->type == JSON_STRING && nm->str_val && nm->str_val[0]) add_owner(owners, &n, nm->str_val);
    }
    if (n == 0) {
        const char *colon = strchr(qualified, ':');
        if (colon) {
            char *head = dups_sub(qualified, (size_t)(colon - qualified));
            add_owner(owners, &n, head);
            free(head);
        } else add_owner(owners, &n, qualified);
    }
    qsort(owners, n, sizeof(char *), cmpstr);
    for (i = 0; i < n; i++) { json_append(out, json_string(owners[i])); free(owners[i]); }
    return out;
}

json_t *w13_pluginscon_bbba05_skill_name_valid(const struct json_t *in) {
    const char *name = json_get_str(in, "name", "");
    const char *pname = json_get_str(in, "plugin_name", "demo");
    json_t *out = json_object();
    if (strchr(name, ':')) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Skill name '%s' must not contain ':' (the namespace is derived from the plugin name '%s' automatically).",
                 name, pname);
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string(msg));
    } else if (!valid_skill_name(name)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Invalid skill name '%s'. Must match [a-zA-Z0-9_-]+.", name);
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string(msg));
    } else {
        json_set(out, "ok", json_bool(true));
        json_set(out, "value", json_string("registered"));
    }
    return out;
}
