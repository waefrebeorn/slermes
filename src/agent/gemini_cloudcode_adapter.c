/*
 * gemini_cloudcode_adapter.c — Port of Python agent/gemini_cloudcode_adapter.py
 *
 * Python API → C implementation mapping:
 *   Google Gemini Cloud Code adapter API → provider_google.c
 *     google_process_message()     → provider_google.c
 *     google_stream_chat()         → provider_google.c
 *     google_parse_response()      → provider_google.c
 *     google_build_request()       → provider_google.c
 *
 * The Gemini Cloud Code adapter is merged with google_native in provider_google.c.
 */

#include "provider.h"   /* provider_google interface via provider dispatch */
