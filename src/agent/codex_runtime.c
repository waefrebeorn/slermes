/*
 * codex_runtime.c — Port of Python agent/codex_runtime.py
 *
 * Python API → C implementation mapping:
 *   run_codex_app_server_turn()         → codex_session_create/run in codex_app_server_session.c
 *   _consume_codex_event_stream()       → codex_session_handle_event in codex_app_server_session.c
 *   run_codex_stream()                  → consolidated in codex_app_server_session.c
 *   run_codex_create_stream_fallback()  → codex_app_server_session.c
 *
 * Codex app server runtime is implemented in codex_app_server_session.c.
 * This file is a name-parity wrapper.
 */

#include "hermes_agent.h"   /* via provider_codex_responses.c codex session API */
