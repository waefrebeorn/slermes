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

    /* Network/DNS resolution failure → offline hint (LIVE walks the exception
     * chain; this port sees the flattened message). */
    {
        static const char *markers[] = {
            "temporary failure in name resolution",
            "name or service not known",
            "nodename nor servname provided, or not known",
            "getaddrinfo failed",
            "no address associated with hostname",
            "network is unreachable",
            NULL
        };
        char low[8192];
        for (size_t i = 0; i <= raw_len && i < sizeof(low); i++)
            low[i] = (char)tolower((unsigned char)raw_error[i]);
        for (int i = 0; markers[i]; i++) {
            if (strstr(low, markers[i]))
                return strdup("Hermes can't reach the model provider. You may be offline. "
                              "Check your internet connection and try again.");
        }
    }

    /* Check for malformed streaming response (LIVE: ValueError with
     * "expected ident at line"). */
    if (strstr(raw_error, "expected ident at line")) {
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
            /* LIVE regex expects the Cloudflare <strong> wrapper — skip it. */
            if (strncmp(start, "<strong", 7) == 0) {
                const char *gt = strchr(start, '>');
                if (gt) start = gt + 1;
            }
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

    /* Check for JSON body errors (LIVE: body dict → error.message if the
     * "error" member is a dict, else top-level "message"; truncated to 300). */
    json_t *root = json_parse(raw_error, NULL);
    if (root && root->type == JSON_OBJECT) {
        const char *msg = NULL;
        json_t *err_obj = json_object_get(root, "error");
        if (err_obj && err_obj->type == JSON_OBJECT)
            msg = json_object_get_string(err_obj, "message", NULL);
        if (!msg)
            msg = json_object_get_string(root, "message", NULL);

        char buf[512];
        if (msg && msg[0]) {
            snprintf(buf, sizeof(buf), "%.300s", msg);
            json_free(root);
            return strdup(buf);
        }
        json_free(root);
    } else if (root) {
        json_free(root);
    }

    /* Fallback: truncate the raw string (LIVE: prefix + raw[:500]). */
    {
        int status_code = 0;
        const char *sc = strstr(raw_error, "HTTP ");
        if (sc) {
            sc += 5;
            if (isdigit((unsigned char)*sc)) status_code = atoi(sc);
        }
        char buf[600];
        if (status_code > 0)
            snprintf(buf, sizeof(buf), "HTTP %d: %.500s", status_code, raw_error);
        else
            snprintf(buf, sizeof(buf), "%.500s", raw_error);
        return strdup(buf);
    }
}
