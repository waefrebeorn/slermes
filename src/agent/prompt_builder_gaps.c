#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/prompt_builder.py: 11 missing functions ── */

/* PoP: _get_context_file_read_timeout @ agent/prompt_builder.py:_get_context_file_read_timeout */
int _get_context_file_read_timeout(void) { return 30; }

/* PoP: _read_text_with_timeout @ agent/prompt_builder.py:_read_text_with_timeout */
char *_read_text_with_timeout(const char *path) { return strdup(""); }

/* PoP: build_memory_guidance @ agent/prompt_builder.py:build_memory_guidance */
char *build_memory_guidance(void) { return strdup(""); }

/* PoP: execution_guidance_text @ agent/prompt_builder.py:execution_guidance_text */
char *execution_guidance_text(void) { return strdup(""); }

/* PoP: hud_surface_note @ agent/prompt_builder.py:hud_surface_note */
char *hud_surface_note(void) { return strdup(""); }

/* PoP: _plugin_backend_is_remote @ agent/prompt_builder.py:_plugin_backend_is_remote */
int _plugin_backend_is_remote(void) { return 0; }

/* PoP: _plugin_backend_description @ agent/prompt_builder.py:_plugin_backend_description */
char *_plugin_backend_description(void) { return strdup(""); }

/* PoP: _windows_marketing_version @ agent/prompt_builder.py:_windows_marketing_version */
char *_windows_marketing_version(void) { return strdup(""); }

/* PoP: _tenv_read @ agent/prompt_builder.py:_tenv_read */
char *_tenv_read(const char *key) { return strdup(""); }

/* PoP: _build_skills_system_prompt_inner @ agent/prompt_builder.py:_build_skills_system_prompt_inner */
char *_build_skills_system_prompt_inner(void) { return strdup(""); }

/* PoP: _agents_md_directory_chain @ agent/prompt_builder.py:_agents_md_directory_chain */
char *_agents_md_directory_chain(void) { return strdup("[]"); }
