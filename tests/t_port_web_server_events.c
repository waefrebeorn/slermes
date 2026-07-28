/*
 * t_port_web_server_events.c — oracle harness for the events/PTY support
 * layer. Ops:
 *   {"op":"close_reason","text":"..."}
 *   {"op":"resolve_host","bound":"...","env":"..."}   (env optional)
 *   {"op":"gateway_url","bound":"...","port":N,"auth":b,"token":"...","internal":"..."}
 *   {"op":"sidecar_url","bound":"...","port":N,"auth":b,"token":"...","internal":"...","channel":"..."}
 *   {"op":"theme_esc","text":"..."}
 *   {"op":"registry_scenario"}   (behavioral invariants, self-checked)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_events.h"

static void pjs(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc(*p, stdout);
        }
    }
}

static bool send_ok(void *ctx, const char *payload) {
    (void)payload;
    int *n = ctx;
    (*n)++;
    return true;
}

static bool send_fail(void *ctx, const char *payload) {
    (void)ctx; (void)payload;
    return false;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");

    if (strcmp(op, "close_reason") == 0) {
        char *r = ws_events_close_reason(json_get_str(fx, "text", ""));
        printf("{\"reason\":\"");
        pjs(r);
        printf("\"}\n");
        free(r);
    } else if (strcmp(op, "resolve_host") == 0) {
        const char *env = json_get_str(fx, "env", NULL);
        if (env) setenv("HERMES_DASHBOARD_WS_HOST", env, 1);
        else unsetenv("HERMES_DASHBOARD_WS_HOST");
        char *r = ws_events_resolve_client_host(json_get_str(fx, "bound", ""));
        if (r) { printf("{\"host\":\""); pjs(r); printf("\"}\n"); free(r); }
        else printf("{\"host\":null}\n");
    } else if (strcmp(op, "gateway_url") == 0) {
        unsetenv("HERMES_DASHBOARD_WS_HOST");
        char *r = ws_events_build_gateway_ws_url(
            json_get_str(fx, "bound", ""), (int)json_get_num(fx, "port", 0),
            json_get_bool(fx, "auth", false), json_get_str(fx, "token", ""),
            json_get_str(fx, "internal", ""));
        if (r) { printf("{\"url\":\""); pjs(r); printf("\"}\n"); free(r); }
        else printf("{\"url\":null}\n");
    } else if (strcmp(op, "sidecar_url") == 0) {
        unsetenv("HERMES_DASHBOARD_WS_HOST");
        char *r = ws_events_build_sidecar_url(
            json_get_str(fx, "bound", ""), (int)json_get_num(fx, "port", 0),
            json_get_bool(fx, "auth", false), json_get_str(fx, "token", ""),
            json_get_str(fx, "internal", ""), json_get_str(fx, "channel", ""));
        if (r) { printf("{\"url\":\""); pjs(r); printf("\"}\n"); free(r); }
        else printf("{\"url\":null}\n");
    } else if (strcmp(op, "theme_esc") == 0) {
        char *r = ws_events_theme_css_esc(json_get_str(fx, "text", ""));
        printf("{\"esc\":\"");
        pjs(r);
        printf("\"}\n");
        free(r);
    } else if (strcmp(op, "registry_scenario") == 0) {
        /* Behavioral invariants of the Python channel registry. */
        ws_event_registry_t *reg = ws_event_registry_new();
        int got_a = 0, got_b = 0;
        int fails = 0;

        /* invalid channel rejected */
        if (ws_event_subscribe(reg, "bad channel!", send_ok, &got_a) != -1) fails++;
        /* subscribe two, broadcast reaches both */
        int s1 = ws_event_subscribe(reg, "chan-1", send_ok, &got_a);
        int s2 = ws_event_subscribe(reg, "chan-1", send_ok, &got_b);
        ws_event_publisher_connect(reg, "chan-1");
        if (ws_event_broadcast(reg, "chan-1", "hello") != 2) fails++;
        if (got_a != 1 || got_b != 1) fails++;
        /* failed sender is dropped */
        int s3 = ws_event_subscribe(reg, "chan-1", send_fail, NULL);
        (void)s3;
        if (ws_event_broadcast(reg, "chan-1", "x") != 2) fails++;
        if (ws_event_subscriber_count(reg, "chan-1") != 2) fails++;
        /* unsubscribe both; channel survives while publisher live */
        ws_event_unsubscribe(reg, "chan-1", s1);
        ws_event_unsubscribe(reg, "chan-1", s2);
        if (ws_event_channel_count(reg) != 1) fails++;
        /* publisher disconnect → auto-evict */
        ws_event_publisher_disconnect(reg, "chan-1");
        if (ws_event_channel_count(reg) != 0) fails++;

        /* session files: create → same path → forget unlinks */
        ws_session_files_t *sf = ws_session_files_new();
        char *p1 = ws_session_file_for_channel(sf, "c1");
        char *p2 = ws_session_file_for_channel(sf, "c1");
        if (!p1 || !p2 || strcmp(p1, p2) != 0) fails++;
        FILE *tf = p1 ? fopen(p1, "r") : NULL;
        if (!tf) fails++; else fclose(tf);
        ws_session_file_forget(sf, "c1");
        if (p1 && fopen(p1, "r") != NULL) fails++;
        free(p1); free(p2);
        ws_session_files_free(sf);
        ws_event_registry_free(reg);
        printf("{\"registry_fails\":%d}\n", fails);
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
