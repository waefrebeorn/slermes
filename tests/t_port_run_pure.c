/*
 * t_port_run_pure.c — oracle harness for the PURE gateway/run.py helpers
 * ported in src/gateway/run_pure.c. One op per line; each op exercises the
 * REAL C function with REAL inputs. The Python oracle
 * (tests/sta_oracle_run_pure.py) recomputes the expected result from the same
 * fixture; run_oracle.sh diffs them as JSON.
 *
 * Fixture line grammar (one op per line, '|' field separator so values may
 * contain spaces; a leading '#' or blank line is ignored):
 *   platform_value <raw>
 *   surface_raw <platform>
 *   nonconv <platform> [<json-metadata>]   (metadata optional; blank = null)
 *   provider_error <text>
 *   provider_reply <text>
 *   auto_noise <content>
 *   strip_auto <content>
 *   telegramize <platform>|<text>
 *   coerce <value>
 *   ts_enabled <json-user-config>
 *   transient <exc> [<cause>] [<context>]
 */

#include "gateway_run_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"

/* emit a top-level JSON string value: "text" (raw quotes, valid top-level). */
static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s ? s : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c < 0x20) fprintf(stdout, "\\u%04x", c);
        else putchar((int)c);
    }
    putchar('"');
}

/* split `rest` on '|' into up to n fields; empties preserved. */
static int split_pipe(char *rest, char **out, int n) {
    for (int i = 0; i < n; i++) out[i] = (char *)"";
    if (!rest) return 0;
    int cnt = 0;
    char *p = rest;
    while (*p && cnt < n) {
        out[cnt++] = p;
        char *colon = strchr(p, '|');
        if (!colon) break;
        *colon = '\0';
        p = colon + 1;
    }
    return cnt;
}

static char *slurp_to_end(char *rest) {
    /* For ops where the payload may contain '|' (json/config text), the
     * caller passes the remainder after the first '|' already split off. */
    return rest ? rest : (char *)"";
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0 || line[0] == '#') continue;
        char *op = line;
        char *rest = strchr(op, ' ');
        if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; } else rest = (char *)"";

        char *a[8];

        if (strcmp(op, "platform_value") == 0) {
            char *out = gateway_platform_value(rest);
            printf("{\"op\":\"platform_value\",\"in\":"); emit_json_string(rest);
            printf(",\"out\":"); emit_json_string(out ? out : ""); printf("}\n");
            free(out);

        } else if (strcmp(op, "surface_raw") == 0) {
            bool r = gateway_surface_passes_raw_text(rest[0] ? rest : "");
            printf("{\"op\":\"surface_raw\",\"platform\":"); emit_json_string(rest);
            printf(",\"raw\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "nonconv") == 0) {
            split_pipe(rest, a, 8);
            json_node_t *md = NULL;
            if (a[1][0]) {
                char *err = NULL;
                md = json_parse(a[1], &err);
                free(err);
            }
            json_node_t *res = gateway_non_conversational_metadata(md, a[0]);
            char *ser = res ? json_serialize(res) : strdup("null");
            printf("{\"op\":\"nonconv\",\"platform\":"); emit_json_string(a[0]);
            printf(",\"out\":"); emit_json_string(ser ? ser : "null"); printf("}\n");
            free(ser);
            if (res && res != md) json_free(res);
            if (md) json_free(md);

        } else if (strcmp(op, "provider_error") == 0) {
            bool r = gateway_looks_like_provider_error_regex(rest[0] ? rest : "");
            printf("{\"op\":\"provider_error\",\"text\":"); emit_json_string(rest);
            printf(",\"looks\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "provider_reply") == 0) {
            char *out = gateway_provider_error_reply_regex(rest[0] ? rest : "");
            printf("{\"op\":\"provider_reply\",\"text\":"); emit_json_string(rest);
            printf(",\"reply\":"); emit_json_string(out ? out : ""); printf("}\n");
            free(out);

        } else if (strcmp(op, "auto_noise") == 0) {
            bool r = gateway_is_auto_continue_noise(rest[0] ? rest : "");
            printf("{\"op\":\"auto_noise\",\"content\":"); emit_json_string(rest);
            printf(",\"noise\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "strip_auto") == 0) {
            char *out = gateway_strip_auto_continue_noise(rest[0] ? rest : "");
            printf("{\"op\":\"strip_auto\",\"content\":"); emit_json_string(rest);
            printf(",\"out\":"); emit_json_string(out ? out : ""); printf("}\n");
            free(out);

        } else if (strcmp(op, "telegramize") == 0) {
            split_pipe(rest, a, 8);
            char *out = gateway_telegramize_command_mentions(
                a[1][0] ? a[1] : "", a[0][0] ? a[0] : "");
            printf("{\"op\":\"telegramize\",\"platform\":"); emit_json_string(a[0]);
            printf(",\"text\":"); emit_json_string(a[1]);
            printf(",\"out\":"); emit_json_string(out ? out : ""); printf("}\n");
            free(out);

        } else if (strcmp(op, "coerce") == 0) {
            char *out = gateway_coerce_timestamp(rest[0] ? rest : "");
            printf("{\"op\":\"coerce\",\"value\":"); emit_json_string(rest);
            if (out) printf(",\"epoch\":\"%s\"", out);
            else printf(",\"epoch\":null");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "ts_enabled") == 0) {
            json_node_t *cfg = NULL;
            if (rest[0]) { char *err = NULL; cfg = json_parse(rest, &err); free(err); }
            bool r = gateway_message_timestamps_enabled(cfg);
            printf("{\"op\":\"ts_enabled\",\"enabled\":%s}\n", r ? "true" : "false");
            if (cfg) json_free(cfg);

        } else if (strcmp(op, "transient") == 0) {
            split_pipe(rest, a, 8);
            bool r = gateway_is_transient_network_error(
                a[0][0] ? a[0] : "",
                a[1][0] ? a[1] : NULL,
                a[2][0] ? a[2] : NULL);
            printf("{\"op\":\"transient\",\"exc\":"); emit_json_string(a[0]);
            printf(",\"transient\":%s}\n", r ? "true" : "false");
        }
    }
    fclose(f);
    return 0;
}
