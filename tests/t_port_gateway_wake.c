/*
 * t_port_gateway_wake.c — Oracle harness for the pure shape helpers of
 * gateway/wake.py (src/cli/port_gateway_wake.c).
 */
#include "port_gateway_wake.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default:   fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    struct { int has, val, name; } pushes[] = {
        { 0, 0, 0 }, { 1, 0, 1 }, { 1, 1, 2 }, { 0, 1, 3 },
    };
    for (size_t i = 0; i < sizeof(pushes) / sizeof(pushes[0]); i++) {
        printf("{\"family\":\"push\",\"case\":\"p_%zu\",\"has\":%d,\"val\":%d,\"ret\":%d}\n",
               i, pushes[i].has, pushes[i].val,
               wake_adapter_supports_push(pushes[i].has, pushes[i].val));
    }

    struct { int runner, present, draining; } drains[] = {
        { 0, 1, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 1, 1, 0 },
    };
    for (size_t i = 0; i < sizeof(drains) / sizeof(drains[0]); i++) {
        printf("{\"family\":\"draining\",\"case\":\"d_%zu\",\"ret\":%d}\n",
               i, wake_runner_draining(drains[i].runner, drains[i].present,
                                       drains[i].draining));
    }

    static const char *const evts[] = {
        "{}",
        "{\"delegation_id\":\"dq1\",\"results\":[]}",
        "{\"delegation_id\":\"dq2\",\"results\":[{\"status\":\"completed\"},{\"status\":\"success\"}]}",
        "{\"delegation_id\":\"dq3\",\"results\":[{\"status\":\"failed\"},{\"status\":\"error\"},{\"status\":\"completed\"}]}",
        "{\"delegation_id\":\"dq4\",\"results\":\"nope\",\"duration_seconds\":2.5}",
        "{\"delegation_id\":\"dq5\",\"results\":[{\"status\":\"failed\",\"task_index\":3}],\"task_failure_notice\":true}",
        "{\"delegation_id\":\"dq6\",\"results\":[{\"status\":\"failed\",\"task_index\":0}],\"task_failure_notice\":\"yes\",\"total_duration_seconds\":9}",
        "{\"delegation_id\":null,\"results\":[7,\"x\",{\"status\":\"failed\"}],\"task_failure_notice\":1}",
        "{\"delegation_id\":\"dq8\",\"results\":[{\"status\":\"weird\"}],\"duration_seconds\":\"3\"}",
    };
    for (size_t i = 0; i < sizeof(evts) / sizeof(evts[0]); i++) {
        for (int w = 0; w < 2; w++) {
            json_t *meta = wake_delegation_display_metadata(evts[i], w);
            printf("{\"family\":\"metadata\",\"case\":\"m_%zu_%d\",\"out\":", i, w);
            if (!meta) {
                fputs("null", stdout);
            } else {
                char *s = json_serialize(meta);
                fputs(s ? s : "null", stdout);
                free(s);
                json_free(meta);
            }
            printf("}\n");
        }
    }
    return 0;
}
