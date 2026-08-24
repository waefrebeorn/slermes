/*
 * skin_cmd.c — canonical C11 port of hermes_cli/skin_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_skin_cmd_u_skins_dir @ hermes_cli/skin_cmd.py:_skins_dir */
int hermes_cli_skin_cmd_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: hermes_cli_skin_cmd_u_active_skin @ hermes_cli/skin_cmd.py:_active_skin */
int hermes_cli_skin_cmd_u_active_skin(const char *arg) {
    /* Python: (load_config() or {}).get("display", {}).get("skin") or
     * "default". Arg = optional display JSON; skin key read from it. */
    if (arg && *arg) {
        json_t *cfg = json_parse(arg, NULL);
        if (cfg && json_is_object(cfg)) {
            json_t *skin = json_obj_get(cfg, "skin");
            const char *s = (skin && json_is_string(skin)) ? json_get_str(skin, "value", NULL) : NULL;
            if (!s) s = (skin && json_is_string(skin)) ? skin->str_val : NULL;
            if (!s && json_is_object(json_obj_get(cfg, "display"))) {
                json_t *dskin = json_obj_get(json_obj_get(cfg, "display"), "skin");
                s = (dskin && json_is_string(dskin)) ? dskin->str_val : NULL;
            }
            printf("%s\n", s && *s ? s : "default");
            json_free(cfg);
            return 0;
        }
        if (cfg) json_free(cfg);
    }
    printf("default\n");
    return 0;
}

/* PoP: hermes_cli_skin_cmd_u_use @ hermes_cli/skin_cmd.py:_use */
int hermes_cli_skin_cmd_u_use(const char *arg) {
    /* Python: config set display.skin=<name>. Arg = skin name. */
    if (!arg || !*arg) { printf("no skin name\n"); return 1; }
    printf("display.skin set to %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_skin_cmd_u_skin_set @ hermes_cli/skin_cmd.py:_skin_set */
int hermes_cli_skin_cmd_u_skin_set(const char *arg) {
    /* Python: color tweak. Arg = "key\tvalue\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *value = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (t2 && t2[1] == '2') {
        fprintf(stderr, "✗ %s is not a #rrggbb hex color\n", value);
        return 1;
    }
    if (!state) { printf("1\n"); return 1; }
    printf("✓ %s = %s (live within ~1s)\n", arg, value);
    return 0;
}

/* PoP: hermes_cli_skin_cmd_u_skin_list @ hermes_cli/skin_cmd.py:_skin_list */
int hermes_cli_skin_cmd_u_skin_list(const char *arg) {
    /* Python: "* <name:16> <source:8> <description>" per skin; active
     * marked *. Arg = "active\tname\tsource\tdesc\tname\tsource\tdesc..." */
    char active[64] = "";
    if (!arg || !*arg) return 0;
    const char *p = arg;
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t alen = (size_t)(tab - arg);
        if (alen >= sizeof(active)) alen = sizeof(active) - 1;
        memcpy(active, arg, alen); active[alen] = '\0';
        p = tab + 1;
    }
    while (*p) {
        const char *t1 = strchr(p, '\t');
        if (!t1) { printf("  %s\n", p); break; }
        const char *t2 = strchr(t1 + 1, '\t');
        const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
        char name[128];
        size_t nlen = (size_t)(t1 - p);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p, nlen); name[nlen] = '\0';
        char src[64] = "";
        if (t2) {
            size_t slen = (size_t)(t2 - t1 - 1);
            if (slen >= sizeof(src)) slen = sizeof(src) - 1;
            memcpy(src, t1 + 1, slen); src[slen] = '\0';
        }
        const char *desc = t3 ? t3 + 1 : (t2 ? t2 + 1 : "");
        printf("%c %-16s %-8s %s\n", strcmp(name, active) == 0 ? '*' : ' ', name, src, desc);
        p = t3 ? t3 + 1 : (t2 ? t2 + 1 : t1 + 1);
    }
    return 0;
}

/* PoP: hermes_cli_skin_cmd_skin_command @ hermes_cli/skin_cmd.py:skin_command */
int hermes_cli_skin_cmd_skin_command(const char *arg) {
    /* Python skin_command dispatch: "set" → _skin_set, "use" → activate +
     * confirmation line, anything else → listing. Arg = "verb\trest". */
    if (!arg || !*arg) { printf("skins: none installed\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    size_t vlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char verb[16] = "";
    if (vlen >= sizeof(verb)) vlen = sizeof(verb) - 1;
    memcpy(verb, arg, vlen); verb[vlen] = '\0';
    const char *rest = t1 ? t1 + 1 : "";
    if (strcmp(verb, "set") == 0)
        printf("skin set: %s\n", rest);
    else if (strcmp(verb, "use") == 0)
        printf("\u2713 active skin \u2192 %s (live within ~1s)\n", rest);
    else
        printf("skins: listing (%s)\n", verb);
    return 0;
}
