/*
 * discord_http.c - Discord REST helpers.
 *
 * Split from discord.c (soul.md: self-contained modules). Token lookup, the
 * Authorization header, the REST call wrapper and the JSON error builders.
 * No file-scope state.
 */
#include "discord_http.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_tool_helpers.h"
#include "hermes_tool_config.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


/* Port of Python hermes_cli/proxy/server.py:_json_error(). */
/* ================================================================
 *  Error helpers
 * ================================================================ */

char *json_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    char *s = json_serialize(o);
    json_free(o);
    return s;
}


char *json_errorf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return json_error(buf);
}


/* Port of Python gateway/platforms/yuanbao.py:get_token(). */
/* ================================================================
 *  Auth
 * ================================================================ */

const char *get_token(void) {
    return tool_config_get_token("discord");
}


/* Build "Bot <token>" auth header */
static char *build_bot_header(void) {
    const char *token = get_token();
    if (!token) return NULL;
    size_t len = strlen(token) + 32;
    char *hdr = (char *)malloc(len);
    if (!hdr) return NULL;
    snprintf(hdr, len, "Authorization: Bot %s\nUser-Agent: Hermes-C (discord-tool)", token);
    return hdr;
}


/* Wrapper: API call with Discord Bot auth header */
tool_api_result_t *discord_api_call(const char *url, const char *method,
                                            const char *body, int timeout, int retries) {
    char *hdr = build_bot_header();
    tool_api_result_t *r = tool_api_call_with_headers(url, hdr, method, body, timeout, retries);
    free(hdr);
    return r;
}
