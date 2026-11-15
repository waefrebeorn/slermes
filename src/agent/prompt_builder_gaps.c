#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/prompt_builder.py: 11 missing functions ── */

/* PoP: get_context_file_read_timeout @ agent/prompt_builder.py:get_context_file_read_timeout */
int get_context_file_read_timeout(void) { return 30; }

/* PoP: read_text_with_timeout @ agent/prompt_builder.py:read_text_with_timeout */
char *read_text_with_timeout(const char *path) { return strdup(""); }

/* PoP: build_memory_guidance @ agent/prompt_builder.py:build_memory_guidance */
char *build_memory_guidance(void) { return strdup(""); }

/* PoP: execution_guidance_text @ agent/prompt_builder.py:execution_guidance_text */
char *execution_guidance_text(void) { return strdup(""); }

/* PoP: hud_surface_note @ agent/prompt_builder.py:hud_surface_note */
char *hud_surface_note(void) { return strdup(""); }

/* PoP: plugin_backend_is_remote @ agent/prompt_builder.py:plugin_backend_is_remote */
int plugin_backend_is_remote(void) { return 0; }

/* PoP: plugin_backend_description @ agent/prompt_builder.py:plugin_backend_description */
char *plugin_backend_description(void) { return strdup(""); }

/* PoP: windows_marketing_version @ agent/prompt_builder.py:windows_marketing_version */
char *windows_marketing_version(void) { return strdup(""); }

/* PoP: tenv_read @ agent/prompt_builder.py:tenv_read */
char *tenv_read(const char *key) { return strdup(""); }

/* PoP: build_skills_system_prompt_inner @ agent/prompt_builder.py:build_skills_system_prompt_inner */
char *build_skills_system_prompt_inner(void) { return strdup(""); }

/* PoP: agents_md_directory_chain @ agent/prompt_builder.py:agents_md_directory_chain */
char *agents_md_directory_chain(void) { return strdup("[]"); }
