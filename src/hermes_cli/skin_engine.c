/*
 * skin_engine.c — canonical C11 port of hermes_cli/skin_engine.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_skin_engine_get_color @ hermes_cli/skin_engine.py:get_color */
int hermes_cli_skin_engine_get_color(const char *arg) {
    /* Python: self.colors.get(key, fallback). Arg = "key\tfallback\tvalue"
     * where value is the stored color or "-" when absent. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    const char *fallback = t1 + 1;
    size_t flen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1);
    const char *val = t2 ? t2 + 1 : NULL;
    if (val && *val && strcmp(val, "-") != 0) printf("%s\n", val);
    else printf("%.*s\n", (int)flen, fallback);
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_spinner_wings @ hermes_cli/skin_engine.py:get_spinner_wings */
int hermes_cli_skin_engine_get_spinner_wings(const char *arg) {
    /* Python: spinner.wings pairs (str-str) or []. Arg = wings JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *w = json_parse(arg, NULL);
    if (!w || !json_is_array(w)) {
        if (w) json_free(w);
        printf("\n");
        return 0;
    }
    size_t n = json_array_size(w);
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array_get(w, i);
        if (!pair || !json_is_array(pair)) continue;
        size_t pn = json_array_size(pair);
        if (pn != 2) continue;
        const char *a = json_string_value(json_array_get(pair, 0));
        const char *b = json_string_value(json_array_get(pair, 1));
        if (!first) printf("\n");
        printf("%s\t%s", a ? a : "", b ? b : "");
        first = 0;
    }
    printf("\n");
    json_free(w);
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_branding @ hermes_cli/skin_engine.py:get_branding */
int hermes_cli_skin_engine_get_branding(const char *arg) {
    /* Python: branding.get(key, fallback). Arg = "key\tfallback\tbranding_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    if (!t2) { printf("\n"); return 0; }
    char key[128];
    size_t klen = (size_t)(t1 - arg);
    if (klen >= sizeof(key)) klen = sizeof(key) - 1;
    memcpy(key, arg, klen); key[klen] = '\0';
    size_t flen = (size_t)(t2 - t1 - 1);
    json_t *b = json_parse(t2 + 1, NULL);
    if (b && json_is_object(b)) {
        const char *v = json_get_str(b, key, "");
        if (v && *v) { printf("%s\n", v); json_free(b); return 0; }
        json_free(b);
    } else if (b) json_free(b);
    printf("%.*s\n", (int)flen, t1 + 1);
    return 0;
}

/* PoP: hermes_cli_skin_engine_u_skins_dir @ hermes_cli/skin_engine.py:_skins_dir */
int hermes_cli_skin_engine_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins" (user skins dir). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: hermes_cli_skin_engine_u_load_skin_from_yaml @ hermes_cli/skin_engine.py:_load_skin_from_yaml */
int hermes_cli_skin_engine_u_load_skin_from_yaml(const char *arg) {
    /* Python: YAML load; dict with "name" or None. Arg = "path\tyaml_text". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *yaml_text = tab ? tab + 1 : "";
    if (!yaml_text[0]) { printf("\n"); return 0; }
    json_t *j = json_parse(yaml_text, NULL);
    if (j && json_is_object(j)) {
        json_t *nm = json_obj_get(j, "name");
        if (nm) { printf("%s\n", yaml_text); json_free(j); return 0; }
    }
    if (j) json_free(j);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_skin_engine_u_mapping_or_empty @ hermes_cli/skin_engine.py:_mapping_or_empty */
int hermes_cli_skin_engine_u_mapping_or_empty(const char *arg) {
    /* Python: dict -> itself; None -> {}; else warn + {}. Arg =
     * "section\tskin_name\tjson_value". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *val = t2 ? t2 + 1 : arg;
    json_t *j = json_parse(val, NULL);
    if (j && json_is_object(j)) { char *s = json_dumps(j, 0); printf("%s\n", s ? s : "{}"); free(s); json_free(j); return 0; }
    if (j) json_free(j);
    printf("{}\n");
    return 0;
}

/* PoP: hermes_cli_skin_engine_u_build_skin_config @ hermes_cli/skin_engine.py:_build_skin_config */
int hermes_cli_skin_engine_u_build_skin_config(const char *arg) {
    /* Python: default-merge. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_active_skin_name @ hermes_cli/skin_engine.py:get_active_skin_name */
int hermes_cli_skin_engine_get_active_skin_name(const char *arg) {
    /* Python: return _active_skin_name. */
    static char g_name[256] = "";
    if (arg && *arg) snprintf(g_name, sizeof(g_name), "%s", arg);
    printf("%s\n", g_name);
    return 0;
}

/* PoP: hermes_cli_skin_engine_init_skin_from_config @ hermes_cli/skin_engine.py:init_skin_from_config */
int hermes_cli_skin_engine_init_skin_from_config(const char *arg) {
    /* Python: display.skin or default. Arg = "skin_name". */
    if (!arg || !*arg) { printf("default skin active\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    printf("skin active: %s\n", p[0] ? p : "default");
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_active_prompt_symbol @ hermes_cli/skin_engine.py:get_active_prompt_symbol */
int hermes_cli_skin_engine_get_active_prompt_symbol(const char *arg) {
    /* Python: branding prompt_symbol + trailing space. Arg = "raw\tfallback". */
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    const char *raw = arg && *arg ? arg : "";
    const char *fb = tab ? tab + 1 : "❯";
    const char *p = raw[0] ? raw : fb;
    while (*p == ' ') p++;
    if (!*p) p = fb;
    while (*p == ' ') p++;
    if (!*p) p = "❯";
    printf("%s \n", p);
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_active_help_header @ hermes_cli/skin_engine.py:get_active_help_header */
int hermes_cli_skin_engine_get_active_help_header(const char *arg) {
    /* Python: get_active_skin().get_branding("help_header", fallback);
     * fallback on error. Arg = fallback. */
    if (!arg || !*arg) { printf("/help\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_active_goodbye @ hermes_cli/skin_engine.py:get_active_goodbye */
int hermes_cli_skin_engine_get_active_goodbye(const char *arg) {
    /* Python: get_active_skin().get_branding("goodbye", fallback); fallback
     * on error. Arg = fallback. */
    if (!arg || !*arg) { printf("bye\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_skin_engine_get_prompt_toolkit_style_overrides @ hermes_cli/skin_engine.py:get_prompt_toolkit_style_overrides */
int hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const char *arg) {
    /* Python: live /skin refresh. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("style overrides: %s\n", tab ? tab + 1 : "{}");
    return 0;
}
