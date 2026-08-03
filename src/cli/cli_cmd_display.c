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

    if (strcmp(subcmd, "hatch") == 0 || strncmp(subcmd, "hatch ", 6) == 0) {
        /* Real generation hatch: build the base prompt, call the image-gen
         * pipeline, install the result as a pet, and select it. Mirrors
         * agent/pet/generate/orchestrate.py:hatch_pet's non-interactive
         * single-draft path. */
        const char *concept = subcmd + 5;
        while (*concept == ' ') concept++;
        if (!*concept) concept = NULL;

        printf("\n=== Pet Hatch ===\n");
        printf("  Generating a new pet%s...\n",
               concept ? "" : " (no concept given — using a default mascot)");

        /* Build the prompt via the faithful prompts port. */
        extern char *pet_prompts_build_base(const char *concept, const char *style,
                                            const char *variation);
        char *prompt = pet_prompts_build_base(concept, NULL, NULL);
        if (!prompt) {
            printf("  Could not build a pet prompt.\n");
            return;
        }

        /* Call the real image-gen tool. */
        extern char *image_generate_handler(const char *args_json, const char *task_id);
        {
            size_t need = strlen(prompt) * 2 + 512;
            char *jargs = malloc(need);
            if (jargs) {
                snprintf(jargs, need,
                         "{\"prompt\":\"%s\",\"aspect_ratio\":\"1:1\","
                         "\"num_images\":1,\"save_local\":true,\"provider\":\"openai\"}",
                         prompt);
                char *result = image_generate_handler(jargs, NULL);
                free(jargs);
                free(prompt);
                if (!result) {
                    printf("  Image generation failed (no response).\n");
                    return;
                }
                /* Parse the result: {"success":true, "image": <path or b64>} */
                json_t *r = json_parse(result, NULL);
                free(result);
                if (!r) {
                    printf("  Image generation returned an unparseable response.\n");
                    return;
                }
                bool ok = false;
                const json_t *succ = json_obj_get(r, "success");
                if (succ && succ->type == JSON_BOOL) ok = succ->bool_val;
                if (!ok) {
                    const char *err = json_get_str(r, "error", "image generation failed");
                    printf("  Image generation failed: %s\n", err);
                    json_free(r);
                    return;
                }
                const char *img = json_get_str(r, "image", NULL);
                if (!img || !*img) {
                    printf("  Image generation returned no image path.\n");
                    json_free(r);
                    return;
                }
                /* Copy the image path before freeing the result JSON. */
                char img_copy[1024];
                snprintf(img_copy, sizeof(img_copy), "%s", img);
                json_free(r);

                /* Install the generated sprite as a pet and select it. */
                char slug[PET_MAX_SLUG];
                snprintf(slug, sizeof(slug), "hatched-%ld", (long)time(NULL));
                char *safe_slug = pet_safe_slug(slug);
                bool installed = pet_install(safe_slug,
                                             concept ? concept : "Hatched Pet",
                                             "AI-generated pet", img_copy);
                if (installed) {
                    pet_select(safe_slug);
                    printf("  Hatched and installed '%s'.\n", safe_slug);
                    printf("  Pet is now active. Run /pet gallery to see it.\n");
                } else {
                    printf("  Generated, but could not install the pet.\n");
                }
                return;
            }
            free(prompt);
            printf("  Out of memory building the generation request.\n");
            return;
        }
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

