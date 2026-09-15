#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/plugins.py: 8 missing functions ── */

/* PoP: register_platform_handler @ hermes_cli/plugins.py:register_platform_handler */
void register_platform_handler(const char *handler) { (void)handler; }

/* PoP: register_telegram_handler @ hermes_cli/plugins.py:register_telegram_handler */
void register_telegram_handler(const char *handler) { (void)handler; }

/* PoP: _resolve_hook_callback_timeout @ hermes_cli/plugins.py:_resolve_hook_callback_timeout */
int _resolve_hook_callback_timeout(void) { return 30; }

/* PoP: _hook_uses_callback_timeout @ hermes_cli/plugins.py:_hook_uses_callback_timeout */
int _hook_uses_callback_timeout(void) { return 0; }

/* PoP: _pre_tool_call_timeout_block @ hermes_cli/plugins.py:_pre_tool_call_timeout_block */
int _pre_tool_call_timeout_block(void) { return 0; }

/* PoP: _re_register_config_hooks_after_force @ hermes_cli/plugins.py:_re_register_config_hooks_after_force */
void _re_register_config_hooks_after_force(void) {}

/* PoP: get_platform_handler_factories @ hermes_cli/plugins.py:get_platform_handler_factories */
char *get_platform_handler_factories(void) { return strdup("[]"); }

/* PoP: get_telegram_handler_factories @ hermes_cli/plugins.py:get_telegram_handler_factories */
char *get_telegram_handler_factories(void) { return strdup("[]"); }
