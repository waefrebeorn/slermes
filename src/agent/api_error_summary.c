/*
 * api_error_summary.c — Port of Python run_agent.AIAgent._summarize_api_error
 * MIT License — WuBu Slermes Project
 *
 * Produces human-readable one-liners from API error strings. Handles Cloudflare
 * HTML pages, JSON body errors, and malformed streaming responses. (GAP 3 from
 * the original hermes_gap_fixes.c monolith — split into a self-contained module.)
 */

#include "api_error_summary.h"
#include "hermes_json.h"
#include "hermes_logger.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

char *summarize_api_error(const char *raw_error) {
    if (!raw_error || !*raw_error)
        return strdup("Unknown API error");

    size_t raw_len = strlen(raw_error);
    if (raw_len > 8192) raw_len = 8192;

    /* Check for malformed streaming response */
    if (strstr(raw_error, "expected ident at line") ||
        strstr(raw_error, "Expected value") ||
        strstr(raw_error, "Invalid JSON")) {
        char buf[400];
        snprintf(buf, sizeof(buf), "Malformed provider streaming response: %.300s", raw_error);
        return strdup(buf);
    }

    /* Check for Cloudflare / proxy HTML pages */
    if (strstr(raw_error, "<!DOCTYPE") || strstr(raw_error, "<html") ||
        strstr(raw_error, "<HTML")) {
        const char *title_start = strstr(raw_error, "<title");
        char title[256] = "HTML error page (title not found)";
        if (title_start) {
            const char *gt = strchr(title_start, '>');
            const char *lt = gt ? strchr(gt + 1, '<') : NULL;
            if (gt && lt && (size_t)(lt - gt - 1) < sizeof(title)) {
                memcpy(title, gt + 1, (size_t)(lt - gt - 1));
                title[lt - gt - 1] = '\0';
            }
        }

        const char *ray = strstr(raw_error, "Ray ID:");
        char ray_id[64] = "";
        if (ray) {
            const char *start = ray + 7;
            while (*start == ' ') start++;
            const char *end = strchr(start, '<');
            if (!end) end = strchr(start, '\n');
            if (!end) end = start + strlen(start);
            size_t rlen = (size_t)(end - start);
            if (rlen > 60) rlen = 60;
            memcpy(ray_id, start, rlen);
            ray_id[rlen] = '\0';
        }

        int status_code = 0;
        const char *sc = strstr(raw_error, "HTTP ");
        if (!sc) sc = strstr(raw_error, "http ");
        if (sc) {
            sc += 5;
            if (isdigit((unsigned char)*sc)) status_code = atoi(sc);
        }

        char buf[512];
        int pos = 0;
        if (status_code > 0)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "HTTP %d ", status_code);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", title);
        if (ray_id[0])
            snprintf(buf + pos, sizeof(buf) - (size_t)pos, " Ray %s", ray_id);

        return strdup(buf);
    }

    /* Check for JSON body errors */
    json_t *root = json_parse(raw_error, NULL);
    if (root && root->type == JSON_OBJECT) {
        json_t *error_obj = json_object_get(root, "error");
        if (error_obj && error_obj->type == JSON_OBJECT) {
            json_t *msg = json_object_get(error_obj, "message");
            json_t *code = json_object_get(error_obj, "code");
            json_t *type = json_object_get(error_obj, "type");

            char buf[512];
            if (code && code->type == JSON_STRING && code->str_val) {
                if (msg && msg->type == JSON_STRING && msg->str_val) {
                    snprintf(buf, sizeof(buf), "[%s] %s: %.400s",
                             code->str_val,
                             type && type->type == JSON_STRING ? type->str_val : "error",
                             msg->str_val);
                } else {
                    /* code present, message absent → Python returns the
                     * generic "API error (see logs)" string. */
                    snprintf(buf, sizeof(buf), "API error (see logs)");
                }
            } else if (msg && msg->type == JSON_STRING) {
                snprintf(buf, sizeof(buf), "%.400s", msg->str_val);
            } else {
                snprintf(buf, sizeof(buf), "API error (see logs)");
            }
            json_free(root);
            return strdup(buf);
        }

        json_t *err_str = json_object_get(root, "error");
        if (err_str && err_str->type == JSON_STRING && err_str->str_val) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%.400s", err_str->str_val);
            json_free(root);
            return strdup(buf);
        }
        json_free(root);
    } else if (root) {
        json_free(root);
    }

    /* Truncated raw error as fallback */
    {
        char buf[512];
        size_t len = strlen(raw_error);
        if (len > 400) {
            const char *nl = strchr(raw_error, '\n');
            if (nl && (size_t)(nl - raw_error) < 400) {
                memcpy(buf, raw_error, (size_t)(nl - raw_error));
                buf[nl - raw_error] = '\0';
            } else {
                memcpy(buf, raw_error, 400);
                buf[400] = '\0';
            }
        } else {
            snprintf(buf, sizeof(buf), "%s", raw_error);
        }
        return strdup(buf);
    }
}
