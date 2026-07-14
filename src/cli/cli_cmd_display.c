/*
 * cli_cmd_display.c — Display slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_display.h"
#include "commands_shared.h"
#include "hermes_core_types.h"
#include "pet.h"

/* ─── /pet — Petdex pet management ─── */
/* PoP: cmd_pet @ agent/pet/commands.py:pet_cli */
void cmd_pet(const char *args, agent_state_t *state) {
    (void)state;

    /* Parse subcommand */
    const char *subcmd = args;
    while (subcmd && *subcmd == ' ') subcmd++;

    if (!subcmd || !subcmd[0] || strcmp(subcmd, "info") == 0) {
        char *json = pet_info_json();
        if (json) {
            printf("\n=== Pet Info ===\n");
            printf("  Status: %s\n", pet_is_enabled() ? "ENABLED" : "DISABLED");
            printf("  Active: %s\n", pet_active_slug()[0] ? pet_active_slug() : "(none)");
            printf("  State:  %s\n", pet_state_string(pet_get_state()));
            printf("  Scale:  %.2f\n", pet_get_scale());
            printf("  JSON:   %s\n", json);
            free(json);
        } else {
            printf("  No pet system initialized.\n");
        }
        return;
    }

    if (strcmp(subcmd, "gallery") == 0) {
        char *json = pet_gallery_json();
        if (json) {
            printf("\n=== Pet Gallery ===\n");
            printf("  %s\n", json);
            free(json);
        } else {
            printf("  No installed pets.\n");
        }
        return;
    }

    if (strncmp(subcmd, "select ", 7) == 0) {
        const char *slug = subcmd + 7;
        while (*slug == ' ') slug++;
        if (*slug) {
            bool ok = pet_select(slug);
            printf("\n=== Pet Select ===\n");
            if (ok) {
                printf("  Pet '%s' selected.\n", slug);
            } else {
                printf("  Could not select pet '%s'. Try installing it first.\n", slug);
            }
        } else {
            printf("  Usage: /pet select <slug>\n");
        }
        return;
    }

    if (strncmp(subcmd, "remove ", 7) == 0) {
        const char *slug = subcmd + 7;
        while (*slug == ' ') slug++;
        if (*slug) {
            bool removed = pet_remove_pet(slug);
            printf("\n=== Pet Remove ===\n");
            printf("  Pet '%s' %s.\n", slug, removed ? "removed" : "not found");
        } else {
            printf("  Usage: /pet remove <slug>\n");
        }
        return;
    }

    if (strcmp(subcmd, "disable") == 0) {
        pet_disable();
        printf("\n=== Pet Disable ===\n");
        printf("  Pet system disabled.\n");
        return;
    }

    if (strcmp(subcmd, "enable") == 0) {
        printf("\n=== Pet Enable ===\n");
        printf("  Pet system enabled (call pet_init() with config to reactivate).\n");
        return;
    }

    if (strncmp(subcmd, "scale ", 6) == 0) {
        const char *scale_str = subcmd + 6;
        while (*scale_str == ' ') scale_str++;
        if (*scale_str) {
            float scale = (float)atof(scale_str);
            pet_set_scale(scale);
            printf("\n=== Pet Scale ===\n");
            printf("  Set to %.2f\n", pet_get_scale());
        } else {
            printf("  Current scale: %.2f\n", pet_get_scale());
        }
        return;
    }

    /* Default: show usage */
    printf("\n=== Petdex — Pet Manager ===\n");
    printf("  Usage:\n");
    printf("    /pet info              Show active pet info\n");
    printf("    /pet gallery           List all installed pets\n");
    printf("    /pet select <slug>     Select/adopt a pet\n");
    printf("    /pet remove <slug>     Remove an installed pet\n");
    printf("    /pet scale <n>         Set pet scale (%.1f - %.1f)\n", PET_MIN_SCALE, PET_MAX_SCALE);
    printf("    /pet disable           Turn off the pet\n");
    printf("    /pet enable            Turn on the pet\n");
}

