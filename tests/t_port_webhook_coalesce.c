/* t_port_webhook_coalesce.c — oracle harness for the pure core of
 * gateway/platforms/webhook_coalesce.py. */
#include "port_webhook_coalesce.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* prompt_with_note: count<=1 -> prompt unchanged; >1 -> note appended. */
    static const char *const prompts[] = { "seen", "started build", "" };
    static const long long counts[] = { 1, 3, 2 };
    for (size_t i = 0; i < sizeof(prompts) / sizeof(prompts[0]); i++) {
        char *out = wc_prompt_with_note(prompts[i], counts[i]);
        printf("{\"family\":\"pn\",\"case\":\"pn_%zu\",\"prompt\":", i);
        jstr(prompts[i]);
        printf(",\"count\":%lld,\"out\":", counts[i]);
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* group_key: rendered with an unresolved placeholder -> null; else prefixed. */
    static const char *const routes[] = { "gh", "pr" };
    static const char *const rendered[] = {
        "42", "main", "{pull_request.number}", "repo#12", "x{bad", "{a}"
    };
    for (size_t r = 0; r < sizeof(routes) / sizeof(routes[0]); r++) {
        for (size_t i = 0; i < sizeof(rendered) / sizeof(rendered[0]); i++) {
            char *out = wc_group_key(routes[r], rendered[i]);
            printf("{\"family\":\"gk\",\"case\":\"gk_%zu_%zu\",\"route\":", r, i);
            jstr(routes[r]);
            printf(",\"rendered\":");
            jstr(rendered[i]);
            printf(",\"out\":");
            jstr(out);
            printf("}\n");
            free(out);
        }
    }

    /* validate_coalesce_config: route dict (JSON) + route name. */
    static const char *const names[] = { "gh_pr", "slack" };
    static const char *const routes_json[] = {
        /* valid / no-coalesce / null */
        "{}",
        "{\"coalesce\":null}",
        "{\"coalesce\":{\"key\":\"pull\",\"window_seconds\":30}}",
        "{\"coalesce\":{\"key\":\"x\",\"window_seconds\":1.5,\"max_wait_seconds\":300}}",
        /* exclusive conflicts */
        "{\"coalesce\":{\"key\":\"k\"},\"deliver_only\":true}",
        "{\"coalesce\":{\"key\":\"k\"},\"cron_job\":true}",
        /* key problems */
        "{\"coalesce\":{}}",
        "{\"coalesce\":{\"key\":\"   \"}}",
        "{\"coalesce\":{\"key\":5}}",
        "{\"coalesce\":\"not-a-dict\"}",
        /* numeric problems */
        "{\"coalesce\":{\"key\":\"k\",\"window_seconds\":false}}",
        "{\"coalesce\":{\"key\":\"k\",\"window_seconds\":0}}",
        "{\"coalesce\":{\"key\":\"k\",\"max_wait_seconds\":-5}}",
        "{\"coalesce\":{\"key\":\"k\",\"window_seconds\":\"abc\"}}",
    };
    for (size_t i = 0; i < sizeof(routes_json) / sizeof(routes_json[0]); i++) {
        const char *name = names[i % 2];
        char *err = NULL;
        int code = wc_validate_coalesce_config(name, routes_json[i], &err);
        printf("{\"family\":\"vc\",\"case\":\"vc_%zu\",\"route_name\":", i);
        jstr(name);
        printf(",\"route\":");
        jstr(routes_json[i]);
        printf(",\"code\":%d,\"err\":", code);
        jstr(err);
        printf("}\n");
        free(err);
    }
    return 0;
}
