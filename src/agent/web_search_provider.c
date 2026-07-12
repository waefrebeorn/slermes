/*
 * web_search_provider.c — Web Search Provider ABC (N/A stub).
 *
 * Port of Python agent/web_search_provider.py (185 lines).
 * This is a Python ABC for plugin-based web search backends. C has its own
 * provider system via src/tools/web_search_registry.c.
 * All methods are N/A — Python ABC/plugin interface, not portable to C.
 *
 * N/A: WebSearchProvider — Python ABC (5 abstract methods)
 * N/A: __init__() — SDK/property initialization
 * N/A: search() — plugin dispatch, async
 * N/A: extract() — plugin dispatch, async
 * N/A: is_available() — plugin state check
 * N/A: name() — property
 * N/A: display_name() — property
 * N/A: get_setup_schema() — plugin config schema
 */

#include "hermes_core_types.h"
