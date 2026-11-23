/*
 * mcp_tool_transport.c — C home for upstream Hermes tools/mcp_tool_transport.py.
 * PoP: w11_mcptooltr_2238d8_content_type_base @ tools/mcp_tool_transport.py:_content_type_base
 * PoP: w11_mcptooltr_2238d8_is_2xx @ tools/mcp_tool_transport.py:_is_2xx
 * PoP: w11_mcptooltr_2238d8_present @ tools/mcp_tool_transport.py:_present
 *
 * Pure helpers only; the async transport bring-up (spawn / HTTP / negotiation)
 * stays in Python. These three are the deterministic seams the preflight and
 * client-construction paths share.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_mcptooltr_2238d8.h"

/* Python str.strip() on the ASCII whitespace domain. */
static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

char *w11_mcptooltr_2238d8_content_type_base(const char *content_type) {
    const char *s = content_type ? content_type : "";
    const char *semi = strchr(s, ';');
    size_t n = semi ? (size_t)(semi - s) : strlen(s);
    while (n > 0 && py_isspace((unsigned char)s[0])) { s++; n--; }
    while (n > 0 && py_isspace((unsigned char)s[n - 1])) { n--; }
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

int w11_mcptooltr_2238d8_is_2xx(int status_code) {
    return status_code >= 200 && status_code < 300;
}

json_t *w11_mcptooltr_2238d8_present(json_t *kwargs) {
    json_t *res = json_object();
    if (!kwargs || kwargs->type != JSON_OBJECT) return res;
    for (size_t i = 0; i < json_object_size(kwargs); i++) {
        const char *k = json_object_get_key_at(kwargs, i);
        json_t *v = json_object_get_at(kwargs, i);
        if (!k || !v || v->type == JSON_NULL) continue; /* kwargs minus None values */
        json_set(res, k, json_copy(v));
    }
    return res;
}
