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

        /* LIVE regex: Cloudflare Ray ID:\s*<strong[^>]*>([^<]+)</strong> — the
         * "Cloudflare" prefix and <strong> wrapper are REQUIRED. */
        char ray_id[64] = "";
        {
            const char *ray = strstr(raw_error, "Cloudflare Ray ID:");
            if (ray) {
                const char *start = ray + strlen("Cloudflare Ray ID:");
                while (*start == ' ') start++;
                if (strncmp(start, "<strong", 7) == 0) {
                    const char *gt = strchr(start, '>');
                    if (gt) start = gt + 1;
                    const char *lt = strchr(start, '<');
                    if (lt && (size_t)(lt - start) < sizeof(ray_id)) {
                        memcpy(ray_id, start, (size_t)(lt - start));
                        ray_id[lt - start] = '\0';
                        size_t L = strlen(ray_id);
                        while (L > 0 && (ray_id[L-1] == ' ' || ray_id[L-1] == '\t')) ray_id[--L] = '\0';
                    }
                }
            }
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
        json_t *err_obj = json_object_get(root, "error");
        if (err_obj && err_obj->type == JSON_OBJECT) {
            /* LIVE: [code] type: message when a string code is present;
             * "API error (see logs)" for code-without-message; message
             * alone when no code; sentinel otherwise. */
            const char *msg = json_object_get_string(err_obj, "message", NULL);
            const char *code = json_object_get_string(err_obj, "code", NULL);
            const char *typ = json_object_get_string(err_obj, "type", NULL);
            char buf[512];
            if (code && code[0]) {
                if (msg && msg[0]) {
                    snprintf(buf, sizeof(buf), "[%.64s] %.64s: %.300s",
                             code, (typ && typ[0]) ? typ : "error", msg);
                    buf[sizeof(buf) - 1] = '\0';
                    json_free(root);
                    return strdup(buf);
                }
                json_free(root);
                return strdup("API error (see logs)");
            }
            if (msg && msg[0]) {
                snprintf(buf, sizeof(buf), "%.400s", msg);
                json_free(root);
                return strdup(buf);
            }
            json_free(root);
            return strdup("API error (see logs)");
        }
        if (err_obj && err_obj->type == JSON_STRING) {
            /* LIVE: {"error": "..."} -> the string itself. */
            const char *estr = err_obj->str_val ? err_obj->str_val : "";
            char buf[512];
            snprintf(buf, sizeof(buf), "%.400s", estr);
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
