/*
 * title_generator.c — Port of Python agent/title_generator.py
 *
 * Python API → C implementation mapping:
 *   generate_title()              → agent_generate_title() in title.c (hermes_agent.h:214)
 *   auto_title_session()          → auto_title_session() in title.c (hermes_agent.h:216)
 *   maybe_auto_title()            → maybe_auto_title() in title.c (hermes_agent.h:219)
 *
 * Title generation uses call_llm() async in Python; C uses sync generation
 * via title.c. All 3 functions have C equivalents.
 */

#include "hermes_agent.h"   /* agent_generate_title(), auto_title_session(), maybe_auto_title() */
