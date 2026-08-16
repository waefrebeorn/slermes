/*
 * config_platforms.c -- extracted from cli/config.c monolith.
 * Real implementation of one config-lifecycle concern; public
 * hermes_config_* protos stay in include/hermes_core_types.h.
 */

#include "hermes_core_types.h"
#include "config_schema.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

bool hermes_config_set_platforms(hermes_config_t *cfg, const char *platforms) {
    /* Update in-memory struct first */
    if (platforms && platforms[0])
        snprintf(cfg->gateway_platforms, sizeof(cfg->gateway_platforms), "%s", platforms);
    else
        cfg->gateway_platforms[0] = '\0';

    /* Resolve config path */
    const char *home_env = getenv("SLERMES_HOME");
    if (!home_env) home_env = getenv("HERMES_HOME");
    if (!home_env) home_env = getenv("HOME");
    if (!home_env) return false;

    char config_path[4096];
    snprintf(config_path, sizeof(config_path), "%s/config.yaml", home_env);

    /* Read entire file */
    FILE *f = fopen(config_path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 1024 * 1024) { fclose(f); return false; }
    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return false; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    buf[nread] = '\0';
    fclose(f);

    /* Build the new line: "  platforms: \"<value>\"\n" or empty */
    char new_line[512];
    if (platforms && platforms[0])
        snprintf(new_line, sizeof(new_line), "  platforms: \"%s\"\n", platforms);
    else
        snprintf(new_line, sizeof(new_line), "  platforms: \"\"\n");

    /* Strategy: find or create the gateway: section, then find or insert platforms: */
    const char *gateway_marker = strstr(buf, "\ngateway:");
    if (!gateway_marker) {
        /* Also check for leading (first line) gateway: */
        if (strncmp(buf, "gateway:", 8) == 0)
            gateway_marker = buf;
    }

    /* Buffer for output */
    char *result = NULL;

    if (gateway_marker) {
        /* gateway: section exists. Look for platforms: within indented block after it. */
        char *search_start = (char *)gateway_marker + strlen(gateway_marker == buf ? "gateway:" : "\ngateway:");
        if (search_start > buf && *search_start == ':') search_start++; /* catch leading case */
        if (*search_start == '\n') search_start++;

        /* Find platforms: under the gateway block (must be indented and before next top-level key) */
        char *platforms_line = NULL;
        char *scan = search_start;
        while (*scan) {
            /* Stop at next top-level key (no leading whitespace) or end of file */
            if (*scan == '\n' && *(scan + 1) && *(scan + 1) != ' ' && *(scan + 1) != '\t' && *(scan + 1) != '\n') {
                if (strncmp(scan + 1, "gateway:", 8) == 0) {
                    /* Another gateway: found — skip */
                } else {
                    break; /* Reached next top-level section */
                }
            }
            if (strncmp(scan, "  platforms:", 12) == 0) {
                platforms_line = scan;
                break;
            }
            scan++;
        }

        if (platforms_line) {
            /* Find end of the platforms line */
            char *eol = strchr(platforms_line, '\n');
            size_t line_len = eol ? (size_t)(eol - platforms_line) : strlen(platforms_line);

            /* Build result: before + new_line + after */
            size_t before_len = (size_t)(platforms_line - buf);
            size_t after_len = eol ? (nread - before_len - line_len) : 0;
            result = (char *)malloc(before_len + strlen(new_line) + after_len + 1);
            if (result) {
                memcpy(result, buf, before_len);
                memcpy(result + before_len, new_line, strlen(new_line));
                if (after_len > 0)
                    memcpy(result + before_len + strlen(new_line), eol + 1, after_len);
                result[before_len + strlen(new_line) + after_len] = '\0';
            }
        } else {
            /* platforms: not found — insert after gateway: line or last child of gateway section */
            /* Find insertion point: after the last gateway child line or after gateway: itself */
            scan = search_start;
            char *last_gw_line_end = search_start;
            while (*scan) {
                /* Stop at next top-level key */
                if (*scan == '\n' && *(scan + 1) && *(scan + 1) != ' ' && *(scan + 1) != '\t' && *(scan + 1) != '\n') {
                    break;
                }
                if (*scan == '\n') last_gw_line_end = scan;
                scan++;
            }
            size_t insert_at = (size_t)(last_gw_line_end - buf) + 1;
            size_t after_len = nread - insert_at;
            result = (char *)malloc(insert_at + strlen(new_line) + after_len + 1);
            if (result) {
                memcpy(result, buf, insert_at);
                memcpy(result + insert_at, new_line, strlen(new_line));
                if (after_len > 0)
                    memcpy(result + insert_at + strlen(new_line), buf + insert_at, after_len);
                result[insert_at + strlen(new_line) + after_len] = '\0';
            }
        }
    } else {
        /* No gateway: section — append at end */
        size_t before_len = nread;
        size_t new_section_len = 0;
        /* Build gateway: section header */
        char section_hdr[128];
        if (nread > 0 && buf[nread - 1] != '\n')
            new_section_len += snprintf(section_hdr, sizeof(section_hdr), "\ngateway:\n");
        else
            new_section_len += snprintf(section_hdr, sizeof(section_hdr), "gateway:\n");
        result = (char *)malloc(before_len + strlen(section_hdr) + strlen(new_line) + 1);
        if (result) {
            memcpy(result, buf, before_len);
            memcpy(result + before_len, section_hdr, strlen(section_hdr));
            memcpy(result + before_len + strlen(section_hdr), new_line, strlen(new_line));
            result[before_len + strlen(section_hdr) + strlen(new_line)] = '\0';
        }
    }

    free(buf);
    if (!result) return false;

    /* Write back */
    f = fopen(config_path, "w");
    if (!f) { free(result); return false; }
    size_t written = fwrite(result, 1, strlen(result), f);
    fclose(f);
    free(result);
    return written > 0;
}
