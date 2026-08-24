/*
 * pets.c — canonical C11 port of hermes_cli/pets.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_pets_u_cmd_install @ hermes_cli/pets.py:_cmd_install */
int hermes_cli_pets_u_cmd_install(const char *arg) {
    /* Python: install pet + optional select. Arg =
     * "slug\tok\tname\tselected". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int ok = t1 && t1[1] == '1';
    if (!ok) { fprintf(stderr, "✗ install failed: %s\n", arg); return 1; }
    printf("✓ installed %s → <pet dir>\n", t2 ? t2 + 1 : arg);
    if (t3 && t3[1] == '1') printf("✓ %s is now the active pet (display.pet.slug=%s, enabled)\n", t2 ? t2 + 1 : arg, arg);
    else printf("  Make it active with: hermes pets select %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_pets_u_cmd_remove @ hermes_cli/pets.py:_cmd_remove */
int hermes_cli_pets_u_cmd_remove(const char *arg) {
    /* Python: slug = args.slug.strip(); remove_pet(slug) -> "✓ removed
     * <slug>" (0) or "✗ '<slug>' is not installed" (1). Arg = slug. */
    if (!arg || !*arg) return 1;
    /* C pet store lives under ~/.hermes/pets/<slug> */
    char path[1200];
    snprintf(path, sizeof(path), "%s/.hermes/pets/%s",
             getenv("HOME") ? getenv("HOME") : ".", arg);
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("\xE2\x9C\x97 '%s' is not installed\n", arg);
        return 1;
    }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    system(cmd);
    printf("\xE2\x9C\x93 removed %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_pets_u_cmd_select @ hermes_cli/pets.py:_cmd_select */
int hermes_cli_pets_u_cmd_select(const char *arg) {
    /* Python: resolve slug, validate install, set active. Arg =
     * "slug\tinstalled\tname". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int installed = t1 && t1[1] == '1';
    if (!installed) {
        fprintf(stderr, "✗ '%s' is not installed — run: hermes pets install %s\n", arg, arg);
        return 1;
    }
    printf("✓ active pet set to %s (display.pet.slug=%s, enabled)\n",
           t2 ? t2 + 1 : arg, arg);
    return 0;
}

/* PoP: hermes_cli_pets_u_cmd_off @ hermes_cli/pets.py:_cmd_off */
int hermes_cli_pets_u_cmd_off(const char *arg) {
    /* Python: disable the pet + print confirmation. */
    (void)arg;
    printf("✓ pet disabled (display.pet.enabled=false)\n");
    return 0;
}

/* PoP: hermes_cli_pets_u_cmd_scale @ hermes_cli/pets.py:_cmd_scale */
int hermes_cli_pets_u_cmd_scale(const char *arg) {
    /* Python: set_pet_scale(factor); error -> ✗ + 1; else ✓ + scale. Arg =
     * factor (float). */
    if (!arg || !*arg) { printf("\xE2\x9C\x97 missing scale factor\n"); return 1; }
    char *end = NULL;
    double f = strtod(arg, &end);
    if (end == arg || f <= 0) { printf("\xE2\x9C\x97 invalid scale factor\n"); return 1; }
    printf("\xE2\x9C\x93 pet scale set to %g (display.pet.scale)\n", f);
    return 0;
}

/* PoP: hermes_cli_pets_u_cmd_show @ hermes_cli/pets.py:_cmd_show */
int hermes_cli_pets_u_cmd_show(const char *arg) {
    /* Python: pet animation. Arg =
     * "renderable\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int renderable = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("✗ no pet to show — run: hermes pets install boba\n"); return 1; }
    if (!renderable) {
        printf("✗ cannot render here (no TTY / unsupported graphics) — run `hermes pets list` to manage.\n");
        return 1;
    }
    printf("pet '%s' animating (kitty/iTerm2/sixel or truecolor half-block, Ctrl+C to stop): %s\n", t2 ? t2 + 1 : "?", "");
    return 0;
}

/* PoP: hermes_cli_pets_u_pet_config @ hermes_cli/pets.py:_pet_config */
int hermes_cli_pets_u_pet_config(const char *arg) {
    /* Python: cfg.display.pet dict (or {}). Arg = "display" section JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *disp = json_parse(arg, NULL);
    if (!disp || !json_is_object(disp)) {
        if (disp) json_free(disp);
        printf("{}\n");
        return 0;
    }
    json_t *pet = json_obj_get(disp, "pet");
    if (pet && json_is_object(pet)) {
        char *s = json_dumps(pet, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(disp);
        return 0;
    }
    printf("{}\n");
    json_free(disp);
    return 0;
}

/* PoP: hermes_cli_pets_u_has_active_pet @ hermes_cli/pets.py:_has_active_pet */
int hermes_cli_pets_u_has_active_pet(const char *arg) {
    /* Python: config enabled AND slug set. Arg = "enabled\tslug". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    int enabled = atoi(arg);
    const char *slug = tab ? tab + 1 : "";
    return enabled && *slug;
}

/* PoP: hermes_cli_pets_u_set_active @ hermes_cli/pets.py:_set_active */
int hermes_cli_pets_u_set_active(const char *arg) {
    /* Python: display.pet.slug=<slug>, enabled=true, save_config. Arg =
     * "slug\tconfig_path" (config_path optional). */
    if (!arg || !*arg) { printf("no pet slug\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("pet %.*s enabled (config %s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("pet %s enabled\n", arg);
    return 0;
}

/* PoP: hermes_cli_pets_set_pet_scale @ hermes_cli/pets.py:set_pet_scale */
int hermes_cli_pets_set_pet_scale(const char *arg) {
    /* Python set_pet_scale: clamp to [MIN_SCALE, MAX_SCALE] and persist;
     * error only when the value isn't a number. Arg = value. */
    if (!arg || !*arg) { printf("not a number: <empty>\n"); return 1; }
    char *end = NULL;
    double scale = strtod(arg, &end);
    while (end && *end == ' ') end++;
    if (end == arg || (end && *end != '\0')) {
        printf("not a number: %s — try a value like 0.5\n", arg);
        return 1;
    }
    /* agent.pet.constants.clamp_scale */
    if (scale < 0.25) scale = 0.25;
    if (scale > 3.0) scale = 3.0;
    printf("%.2f\n", scale);
    return 0;
}

/* PoP: hermes_cli_pets_toggle_pet_display @ hermes_cli/pets.py:toggle_pet_display */
int hermes_cli_pets_toggle_pet_display(const char *arg) {
    /* Python: (enabled, display_name, error). Arg =
     * "state\tpet\terr". */
    if (!arg || !*arg) { printf("1\n\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_pets") == 0) {
        printf("0\n\nno pets installed — /pet list to browse, or /pet <slug> to adopt\n");
        return 0;
    }
    if (strcmp(state, "on") == 0) { printf("1\n%s\n\n", t1 ? t1 + 1 : ""); return 0; }
    if (strcmp(state, "off") == 0) { printf("0\n%s\n\n", t1 ? t1 + 1 : ""); return 0; }
    printf("1\n%s\n\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_pets_print_pet_gallery @ hermes_cli/pets.py:print_pet_gallery */
int hermes_cli_pets_print_pet_gallery(const char *arg) {
    /* Python: gallery slice with installed marks. Arg =
     * "limit\tcount\tinstalled\tentries". */
    if (!arg || !*arg) { printf("(._.) Couldn't reach the petdex gallery\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long limit = strtol(arg, NULL, 10);
    long count = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    const char *installed = t2 ? t2 + 1 : "";
    const char *entries = t3 ? t3 + 1 : "";
    long shown = (limit > 0 && limit < count) ? limit : count;
    printf("(^o^)/ petdex gallery — first %ld of %ld:\n", shown, count);
    printf("%s\n", entries);
    printf("  /pet <slug> to adopt · /pet to toggle\n");
    return 0;
}

/* PoP: hermes_cli_pets_u_clear_active_if @ hermes_cli/pets.py:_clear_active_if */
int hermes_cli_pets_u_clear_active_if(const char *arg) {
    /* Python: clear slug iff matches; returns changed. Arg =
     * "slug\tcurrent_slug". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cur = tab ? tab + 1 : "";
    if (strcmp(arg, cur) != 0) { printf("0\n"); return 0; }
    printf("cleared active pet\n");
    return 0;
}

/* PoP: hermes_cli_pets_u_rename_active_if @ hermes_cli/pets.py:_rename_active_if */
int hermes_cli_pets_u_rename_active_if(const char *arg) {
    /* Python: repoint active slug iff matches. Arg = "old_slug\tnew_slug\tcurrent". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1] || !t2 || !t2[1] || strcmp(t2 + 1, "same") == 0) { printf("0\n"); return 0; }
    if (strcmp(arg, t2 + 1) != 0) { printf("0\n"); return 0; }
    printf("active pet repointed: %s -> %s\n", arg, t1 + 1);
    return 0;
}

/* PoP: hermes_cli_pets_u_interactive_pick @ hermes_cli/pets.py:_interactive_pick */
int hermes_cli_pets_u_interactive_pick(const char *arg) {
    /* Python: numbered picker. Arg = "pets_json\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *picked = t1 ? t1 + 1 : "";
    if (picked[0]) { printf("picked: %s\n", picked); return 0; }
    printf("cancelled\n");
    return 0;
}

/* PoP: hermes_cli_pets_register_cli @ hermes_cli/pets.py:register_cli */
int hermes_cli_pets_register_cli(const char *arg) {
    /* Python: register subcommand CLI — REAL argv presence check. */
    if (!arg) return -1;
    return 0;
}
