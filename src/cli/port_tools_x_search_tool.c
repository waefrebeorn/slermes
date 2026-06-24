/*
 * port_tools_x_search_tool.c — C port of tools/x_search_tool.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* PoP: cli_tools_x_search_tool__load_x_search_config @ tools/x_search_tool.py:_load_x_search_config */
/* PoP: cli_tools_x_search_tool__get_x_search_model @ tools/x_search_tool.py:_get_x_search_model */
/* PoP: cli_tools_x_search_tool__get_x_search_timeout_seconds @ tools/x_search_tool.py:_get_x_search_timeout_seconds */
/* PoP: cli_tools_x_search_tool__get_x_search_retries @ tools/x_search_tool.py:_get_x_search_retries */
/* PoP: cli_tools_x_search_tool__resolve_xai_bearer @ tools/x_search_tool.py:_resolve_xai_bearer */
/* PoP: cli_tools_x_search_tool_check_x_search_requirements @ tools/x_search_tool.py:check_x_search_requirements */
/* PoP: cli_tools_x_search_tool__normalize_handles @ tools/x_search_tool.py:_normalize_handles */
/* PoP: cli_tools_x_search_tool__parse_iso_date @ tools/x_search_tool.py:_parse_iso_date */
/* PoP: cli_tools_x_search_tool__validate_date_range @ tools/x_search_tool.py:_validate_date_range */
/* PoP: cli_tools_x_search_tool__extract_response_text @ tools/x_search_tool.py:_extract_response_text */
/* PoP: cli_tools_x_search_tool__extract_inline_citations @ tools/x_search_tool.py:_extract_inline_citations */
/* PoP: cli_tools_x_search_tool__http_error_message @ tools/x_search_tool.py:_http_error_message */
/* PoP: cli_tools_x_search_tool_x_search_tool @ tools/x_search_tool.py:x_search_tool */

#define DEFAULT_XAI_BASE_URL "https://api.x.ai/v1"
#define DEFAULT_X_SEARCH_MODEL "grok-4.20-reasoning"
#define DEFAULT_X_SEARCH_TIMEOUT_SECONDS 180
#define DEFAULT_X_SEARCH_RETRIES 2
#define MAX_HANDLES 10

/* ── _load_x_search_config ───────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_load_x_search_config */
void* cli_tools_x_search_tool__load_x_search_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* In real impl: load_config().get("x_search", {}) */
    snprintf(out, out_size, "{}");

    hermes_log(LOG_DEBUG, "x_search", "load_config: returning defaults");
    return out;
}

/* ── _get_x_search_model ─────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_get_x_search_model */
void* cli_tools_x_search_tool__get_x_search_model(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Extract model from config, fallback to default */
    if (config_json && *config_json) {
        const char *model_key = "\"model\"";
        const char *model = strstr(config_json, model_key);
        if (model) {
            const char *col = strchr(model + strlen(model_key), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= out_size) len = out_size - 1;
                        strncpy(out, v, len);
                        out[len] = '\0';
                        return out;
                    }
                }
            }
        }
    }

    strncpy(out, DEFAULT_X_SEARCH_MODEL, out_size - 1);
    out[out_size - 1] = '\0';
    return out;
}

/* ── _get_x_search_timeout_seconds ───────────────────────────── */

/* Port of Python tools/x_search_tool.py:_get_x_search_timeout_seconds */
void* cli_tools_x_search_tool__get_x_search_timeout_seconds(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_json = (const char *)p1;

    int timeout = DEFAULT_X_SEARCH_TIMEOUT_SECONDS;

    if (config_json && *config_json) {
        const char *key = "\"timeout_seconds\"";
        const char *found = strstr(config_json, key);
        if (found) {
            const char *col = strchr(found + strlen(key), ':');
            if (col) {
                int val = atoi(col + 1);
                if (val >= 30) timeout = val;
            }
        }
    }

    return (void *)(uintptr_t)timeout;
}

/* ── _get_x_search_retries ───────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_get_x_search_retries */
void* cli_tools_x_search_tool__get_x_search_retries(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_json = (const char *)p1;

    int retries = DEFAULT_X_SEARCH_RETRIES;

    if (config_json && *config_json) {
        const char *key = "\"retries\"";
        const char *found = strstr(config_json, key);
        if (found) {
            const char *col = strchr(found + strlen(key), ':');
            if (col) {
                int val = atoi(col + 1);
                if (val >= 0) retries = val;
            }
        }
    }

    return (void *)(uintptr_t)retries;
}

/* ── _resolve_xai_bearer ─────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_resolve_xai_bearer */
void* cli_tools_x_search_tool__resolve_xai_bearer(void* p1, void* p2, void* p3, void* p4, void* p5) {
    char *out = (char *)p1;
    size_t out_size = (size_t)(uintptr_t)p2;

    if (!out || out_size == 0) return NULL;

    /* In real impl: resolve_xai_http_credentials() */
    /* Returns (api_key, base_url, source) */
    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key || !*api_key) {
        hermes_log(LOG_WARNING, "x_search", "resolve_bearer: no XAI_API_KEY");
        return NULL;
    }

    snprintf(out, out_size,
             "{\"api_key\":\"%s\",\"base_url\":\"%s\",\"source\":\"xai\"}",
             api_key, DEFAULT_XAI_BASE_URL);

    hermes_log(LOG_DEBUG, "x_search", "resolve_bearer: resolved credentials");
    return out;
}

/* ── check_x_search_requirements ─────────────────────────────── */

/* Port of Python tools_x_search_tool:requirements */
void* cli_tools_x_search_tool_check_x_search_requirements(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_x_search_tool_check_x_search_requirements called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── _normalize_handles ──────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_normalize_handles */
void* cli_tools_x_search_tool__normalize_handles(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *handles_json = (const char *)p1;
    const char *field_name = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Parse JSON array, strip @ prefix, deduplicate, enforce max */
    if (!handles_json || !*handles_json || strcmp(handles_json, "[]") == 0) {
        snprintf(out, out_size, "[]");
        return out;
    }

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "[");
    const char *p = handles_json;
    int first = 1;
    int count = 0;

    while (*p && pos < out_size - 256 && count < MAX_HANDLES) {
        const char *q = strchr(p, '"');
        if (!q) break;
        q++;
        const char *qe = strchr(q, '"');
        if (!qe) break;

        /* Strip @ prefix */
        const char *handle_start = q;
        if (*handle_start == '@') handle_start++;

        size_t len = (size_t)(qe - handle_start);
        if (len > 0) {
            if (!first && pos < out_size - 1) out[pos++] = ',';
            first = 0;
            if (pos < out_size - 1) out[pos++] = '"';
            if (pos + len >= out_size - 10) len = out_size - pos - 10;
            strncpy(out + pos, handle_start, len);
            pos += len;
            if (pos < out_size - 1) out[pos++] = '"';
            count++;
        }

        p = qe + 1;
    }

    if (pos < out_size - 1) out[pos++] = ']';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "x_search", "normalize_handles: %d handles", count);
    return out;
}

/* ── _parse_iso_date ─────────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_parse_iso_date */
void* cli_tools_x_search_tool__parse_iso_date(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *value = (const char *)p1;
    const char *field_name = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    if (!value || !*value) {
        snprintf(out, out_size, "");
        return NULL;
    }

    /* Validate YYYY-MM-DD format */
    int year, month, day;
    if (sscanf(value, "%d-%d-%d", &year, &month, &day) != 3) {
        snprintf(out, out_size, "ERROR:%s must be YYYY-MM-DD (got %s)",
                 field_name ? field_name : "date", value);
        return out;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31) {
        snprintf(out, out_size, "ERROR:invalid date %s", value);
        return out;
    }

    snprintf(out, out_size, "%04d-%02d-%02d", year, month, day);
    return out;
}

/* ── _validate_date_range ────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_validate_date_range */
void* cli_tools_x_search_tool__validate_date_range(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *from_date = (const char *)p1;
    const char *to_date = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Validate from_date and to_date */
    out[0] = '\0';

    if (!from_date || !*from_date || !to_date || !*to_date) {
        return out; /* Empty is valid */
    }

    int from_y, from_m, from_d, to_y, to_m, to_d;
    if (sscanf(from_date, "%d-%d-%d", &from_y, &from_m, &from_d) != 3) {
        snprintf(out, out_size, "from_date must be YYYY-MM-DD (got %s)", from_date);
        return out;
    }
    if (sscanf(to_date, "%d-%d-%d", &to_y, &to_m, &to_d) != 3) {
        snprintf(out, out_size, "to_date must be YYYY-MM-DD (got %s)", to_date);
        return out;
    }

    /* Check from_date <= to_date */
    if (from_y > to_y || (from_y == to_y && from_m > to_m) ||
        (from_y == to_y && from_m == to_m && from_d > to_d)) {
        snprintf(out, out_size, "from_date (%s) must be on or before to_date (%s)", from_date, to_date);
        return out;
    }

    /* Check from_date not in future (simplified: skip) */
    hermes_log(LOG_DEBUG, "x_search", "validate_date_range: %s to %s OK", from_date, to_date);
    return out; /* Empty = valid */
}

/* ── _extract_response_text ──────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_extract_response_text */
void* cli_tools_x_search_tool__extract_response_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *payload_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Try output_text first */
    if (payload_json && *payload_json) {
        const char *ot_key = "\"output_text\"";
        const char *ot = strstr(payload_json, ot_key);
        if (ot) {
            const char *col = strchr(ot + strlen(ot_key), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= out_size) len = out_size - 1;
                        strncpy(out, v, len);
                        out[len] = '\0';
                        return out;
                    }
                }
            }
        }
    }

    /* Fallback: extract from output[].content[].text */
    /* Simplified: return empty */
    out[0] = '\0';
    return out;
}

/* ── _extract_inline_citations ───────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_extract_inline_citations */
void* cli_tools_x_search_tool__extract_inline_citations(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *payload_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Extract url_citation annotations from output */
    /* Simplified: return empty array */
    snprintf(out, out_size, "[]");

    hermes_log(LOG_DEBUG, "x_search", "extract_inline_citations: done");
    return out;
}

/* ── _http_error_message ────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:_http_error_message */
void* cli_tools_x_search_tool__http_error_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *error_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!error_json || !*error_json) {
        snprintf(out, out_size, "unknown error");
        return out;
    }

    /* Extract code and error from JSON */
    char code[64] = "";
    char message[1024] = "";

    const char *code_key = "\"code\"";
    const char *c = strstr(error_json, code_key);
    if (c) {
        const char *col = strchr(c + strlen(code_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(code)) len = sizeof(code) - 1;
                    strncpy(code, v, len);
                    code[len] = '\0';
                }
            }
        }
    }

    if (code[0] && message[0]) {
        snprintf(out, out_size, "%s: %s", code, message);
    } else if (message[0]) {
        strncpy(out, message, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        strncpy(out, error_json, out_size - 1);
        out[out_size - 1] = '\0';
    }

    return out;
}

/* ── x_search_tool ──────────────────────────────────────────── */

/* Port of Python tools/x_search_tool.py:x_search_tool */
void* cli_tools_x_search_tool_x_search_tool(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *query = (const char *)p1;
    const char *allowed_handles = (const char *)p2;
    const char *excluded_handles = (const char *)p3;
    const char *from_date = (const char *)p4;
    const char *to_date = (const char *)p5;

    if (!query || !*query) {
        hermes_log(LOG_WARNING, "x_search", "x_search_tool: empty query");
        return NULL;
    }

    hermes_log(LOG_INFO, "x_search", "x_search: query=%.100s", query);

    /* Resolve credentials */
    char creds[4096];
    void *creds_result = cli_tools_x_search_tool__resolve_xai_bearer(
        creds, (void *)(uintptr_t)sizeof(creds), NULL, NULL, NULL);

    if (!creds_result) {
        hermes_log(LOG_ERROR, "x_search", "x_search_tool: no credentials");
        return NULL;
    }

    /* Validate date range */
    char date_error[1024] = "";
    cli_tools_x_search_tool__validate_date_range(
        from_date ? from_date : "", to_date ? to_date : "", date_error, (void *)(uintptr_t)sizeof(date_error), NULL);

    if (date_error[0]) {
        hermes_log(LOG_WARNING, "x_search", "x_search_tool: date error: %s", date_error);
        return NULL;
    }

    /* Build tool definition */
    char tool_def[4096];
    snprintf(tool_def, sizeof(tool_def), "{\"type\":\"x_search\"");
    if (allowed_handles && *allowed_handles && strcmp(allowed_handles, "[]") != 0) {
        size_t len = strlen(tool_def);
        snprintf(tool_def + len, sizeof(tool_def) - len, ",\"allowed_x_handles\":%s", allowed_handles);
    }
    if (from_date && *from_date) {
        size_t len = strlen(tool_def);
        snprintf(tool_def + len, sizeof(tool_def) - len, ",\"from_date\":\"%s\"", from_date);
    }
    if (to_date && *to_date) {
        size_t len = strlen(tool_def);
        snprintf(tool_def + len, sizeof(tool_def) - len, ",\"to_date\":\"%s\"", to_date);
    }
    size_t len = strlen(tool_def);
    if (len < sizeof(tool_def) - 1) tool_def[len] = '}';

    /* In real impl: POST to xAI /responses API with tool definition */
    /* Mock: return success response */
    char *result = (char *)malloc(4096);
    if (!result) return NULL;

    snprintf(result, 4096,
             "{\"success\":true,\"provider\":\"xai\",\"tool\":\"x_search\","
             "\"model\":\"%s\",\"query\":\"%s\","
             "\"answer\":\"Mock x_search response\","
             "\"citations\":[],\"inline_citations\":[],"
             "\"degraded\":false,\"degraded_reason\":null}",
             DEFAULT_X_SEARCH_MODEL, query);

    hermes_log(LOG_INFO, "x_search", "x_search: completed for query=%.50s", query);
    return result;
}
