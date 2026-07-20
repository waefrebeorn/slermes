/*
 * t_port_cron_delivery.c — faithful verification harness for the CRON
 * delivery/origin/mirror/routing helpers ported from cron/scheduler.py.
 *
 * One op per line from a fixture file; exercises the REAL C functions with
 * REAL inputs (env-driven home channels). The Python oracle
 * (tests/sta_oracle_cron_delivery.py) recomputes the expected result from the
 * same fixture; the runner diffs them line-by-line as JSON.
 *
 * Fixture line grammar (one op per line):
 *   origin        <platform> <chat_id> [thread_id]      (set job origin)
 *   mirror        <attach_present(0|1)> [attach_val(0|1)] [global(0|1)]
 *   known_platform <name>
 *   home_env      <name>
 *   home_chat     <name> [chat_id]             (sets <ENV> to chat_id, or unset)
 *   home_thread   <name> [thread_id]           (sets <ENV>_THREAD_ID, or unset)
 *   register_plugin <name> <env_var>           (plugin platform registration)
 *   expand        <token>                      (routing token expansion)
 *   resolve_one   <deliver_value>              (single delivery target)
 *   resolve_all   <deliver_csv>                (all delivery targets, deduped)
 *   delivery_targets <connected_csv>           (UI-facing target list)
 *   match_origin  <platform> <chat_id> [thread_id]   (target==origin test)
 *
 * Home channels are modelled by setting the appropriate *_HOME_CHANNEL (or
 * legacy) env var before the op; the oracle reads the SAME env vars, so the
 * two sides agree without a live gateway.
 */

#include "cron_scheduler_delivery.h"
#include "cron_scheduler_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* minimal JSON string emitter (escape ", \, control chars) */
static void emit_json_string(const char *s)
{
    putchar('"');
    for (const char *p = s ? s : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c == '\n') fputs("\\n", stdout);
        else if (c == '\r') fputs("\\r", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c < 0x20) fprintf(stdout, "\\u%04x", c);
        else putchar((int)c);
    }
    putchar('"');
}

/* set or clear an env var by exact name */
static void set_or_clear(const char *name, int set_it, const char *val)
{
    if (set_it) setenv(name, val ? val : "present", 1);
    else unsetenv(name);
}

/* map a platform name to its PRIMARY home-channel env var (mirror of Python
 * _HOME_TARGET_ENV_VARS). Used so the harness can set the SAME var the C
 * accessor reads. */
static const char *home_env_for(const char *name)
{
    if (!strcasecmp(name, "matrix")) return "MATRIX_HOME_ROOM";
    if (!strcasecmp(name, "telegram")) return "TELEGRAM_HOME_CHANNEL";
    if (!strcasecmp(name, "discord")) return "DISCORD_HOME_CHANNEL";
    if (!strcasecmp(name, "slack")) return "SLACK_HOME_CHANNEL";
    if (!strcasecmp(name, "signal")) return "SIGNAL_HOME_CHANNEL";
    if (!strcasecmp(name, "mattermost")) return "MATTERMOST_HOME_CHANNEL";
    if (!strcasecmp(name, "sms")) return "SMS_HOME_CHANNEL";
    if (!strcasecmp(name, "email")) return "EMAIL_HOME_ADDRESS";
    if (!strcasecmp(name, "dingtalk")) return "DINGTALK_HOME_CHANNEL";
    if (!strcasecmp(name, "feishu")) return "FEISHU_HOME_CHANNEL";
    if (!strcasecmp(name, "wecom")) return "WECOM_HOME_CHANNEL";
    if (!strcasecmp(name, "weixin")) return "WEIXIN_HOME_CHANNEL";
    if (!strcasecmp(name, "bluebubbles")) return "BLUEBUBBLES_HOME_CHANNEL";
    if (!strcasecmp(name, "qqbot")) return "QQBOT_HOME_CHANNEL";
    if (!strcasecmp(name, "whatsapp")) return "WHATSAPP_HOME_CHANNEL";
    if (!strcasecmp(name, "whatsapp_cloud")) return "WHATSAPP_CLOUD_HOME_CHANNEL";
    return NULL;
}

static scheduler_job_t g_job;
/* Persistent storage for origin strings (line[] buffer is reused each
 * iteration, so we must COPY origin fields, not keep pointers into it). */
static char g_origin_platform[64];
static char g_origin_chat[256];
static char g_origin_thread[256];

static void reset_job(void)
{
    memset(&g_job, 0, sizeof(g_job));
    g_origin_platform[0] = '\0';
    g_origin_chat[0] = '\0';
    g_origin_thread[0] = '\0';
    g_job.origin.platform = g_origin_platform;
    g_job.origin.chat_id = g_origin_chat;
    g_job.origin.thread_id = g_origin_thread;
    /* telegram thread override used by get_home_target_thread_id */
    unsetenv("TELEGRAM_CRON_THREAD_ID");
}

/* parse up to 3 whitespace-separated tokens from `rest` into out[0..2] */
static int split3(char *rest, char **out)
{
    int n = 0;
    char *p = rest;
    while (*p && n < 3) {
        while (*p == ' ') p++;
        if (!*p) break;
        out[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    reset_job();

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        char *op = line;
        char *rest = strchr(op, ' ');
        if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; } else rest = (char *)"";

        char *a[3];
        int na = split3(rest, a);

        if (strcmp(op, "origin") == 0) {
            reset_job();
            g_job.origin.has_origin = 1;
            strncpy(g_origin_platform, a[0], sizeof(g_origin_platform) - 1);
            strncpy(g_origin_chat, na > 1 ? a[1] : "", sizeof(g_origin_chat) - 1);
            strncpy(g_origin_thread, na > 2 ? a[2] : "", sizeof(g_origin_thread) - 1);
            printf("{\"op\":\"origin\",\"has_origin\":%d}\n", 1);

        } else if (strcmp(op, "clear_origin") == 0) {
            reset_job();
            printf("{\"op\":\"clear_origin\",\"has_origin\":%d}\n", 0);

        } else if (strcmp(op, "mirror") == 0) {
            int ap = na > 0 ? atoi(a[0]) : 0;
            int av = na > 1 ? atoi(a[1]) : 0;
            int gl = na > 2 ? atoi(a[2]) : 0;
            g_job.attach_to_session_present = ap;
            g_job.attach_to_session_val = av;
            int r = scheduler_cron_mirror_delivery_enabled(&g_job, gl);
            printf("{\"op\":\"mirror\",\"enabled\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "known_platform") == 0) {
            int r = scheduler_is_known_delivery_platform(a[0]);
            printf("{\"op\":\"known_platform\",\"name\":"); emit_json_string(a[0]);
            printf(",\"known\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "home_env") == 0) {
            const char *e = scheduler_resolve_home_env_var(a[0]);
            printf("{\"op\":\"home_env\",\"name\":"); emit_json_string(a[0]);
            printf(",\"env\":"); emit_json_string(e ? e : ""); printf("}\n");

        } else if (strcmp(op, "home_chat") == 0) {
            const char *env = home_env_for(a[0]);
            if (na > 1) set_or_clear(env ? env : "NONE", env != NULL, a[1]);
            else if (env) unsetenv(env);
            char *r = scheduler_get_home_target_chat_id(a[0]);
            printf("{\"op\":\"home_chat\",\"name\":"); emit_json_string(a[0]);
            printf(",\"chat_id\":"); emit_json_string(r); printf("}\n");
            free(r);

        } else if (strcmp(op, "home_thread") == 0) {
            const char *env = home_env_for(a[0]);
            if (na > 1) {
                if (!strcasecmp(a[0], "telegram")) setenv("TELEGRAM_CRON_THREAD_ID", a[1], 1);
                else if (env) { char buf[256]; snprintf(buf, sizeof(buf), "%s_THREAD_ID", env); setenv(buf, a[1], 1); }
            } else {
                if (!strcasecmp(a[0], "telegram")) unsetenv("TELEGRAM_CRON_THREAD_ID");
                else if (env) { char buf[256]; snprintf(buf, sizeof(buf), "%s_THREAD_ID", env); unsetenv(buf); }
            }
            char *r = scheduler_get_home_target_thread_id(a[0]);
            printf("{\"op\":\"home_thread\",\"name\":"); emit_json_string(a[0]);
            printf(",\"thread_id\":"); emit_json_string(r ? r : ""); printf("}\n");
            free(r);

        } else if (strcmp(op, "register_plugin") == 0) {
            int r = scheduler_register_plugin_platform(a[0], na > 1 ? a[1] : "");
            printf("{\"op\":\"register_plugin\",\"name\":"); emit_json_string(a[0]);
            printf(",\"ok\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "expand") == 0) {
            char *exp[64];
            int ne = scheduler_expand_routing_tokens(a[0], exp, 64);
            printf("{\"op\":\"expand\",\"token\":"); emit_json_string(a[0]);
            printf(",\"n\":%d,\"vals\":[", ne);
            for (int i = 0; i < ne; i++) { if (i) putchar(','); emit_json_string(exp[i]); free(exp[i]); }
            printf("]}\n");

        } else if (strcmp(op, "resolve_one") == 0) {
            scheduler_target_t t;
            int r = scheduler_resolve_single_delivery_target(&g_job, a[0], &t);
            printf("{\"op\":\"resolve_one\",\"deliver\":"); emit_json_string(a[0]);
            printf(",\"ok\":%s", r ? "true" : "false");
            if (r) { printf(",\"platform\":"); emit_json_string(t.platform);
                     printf(",\"chat_id\":"); emit_json_string(t.chat_id);
                     printf(",\"thread_id\":"); emit_json_string(t.thread_id); }
            printf("}\n");

        } else if (strcmp(op, "resolve_all") == 0) {
            scheduler_target_t ts[64];
            int nt = scheduler_resolve_delivery_targets(&g_job, ts, 64);
            printf("{\"op\":\"resolve_all\",\"deliver\":"); emit_json_string(rest);
            printf(",\"n\":%d,\"targets\":[", nt);
            for (int i = 0; i < nt; i++) {
                if (i) putchar(',');
                printf("{\"platform\":"); emit_json_string(ts[i].platform);
                printf(",\"chat_id\":"); emit_json_string(ts[i].chat_id);
                printf(",\"thread_id\":"); emit_json_string(ts[i].thread_id); printf("}");
            }
            printf("]}\n");

        } else if (strcmp(op, "delivery_targets") == 0) {
            /* connected list is comma-separated */
            const char *conn[64];
            int nc = 0;
            char cbuf[1024];
            strncpy(cbuf, rest, sizeof(cbuf) - 1);
            cbuf[sizeof(cbuf) - 1] = '\0';
            char *tok = strtok(cbuf, ",");
            while (tok && nc < 64) {
                while (*tok == ' ') tok++;
                if (*tok) conn[nc++] = tok;
                tok = strtok(NULL, ",");
            }
            scheduler_delivery_desc_t ds[64];
            int nd = scheduler_cron_delivery_targets(conn, nc, ds, 64);
            printf("{\"op\":\"delivery_targets\",\"connected\":"); emit_json_string(rest);
            printf(",\"n\":%d,\"targets\":[", nd);
            for (int i = 0; i < nd; i++) {
                if (i) putchar(',');
                printf("{\"id\":"); emit_json_string(ds[i].id);
                printf(",\"name\":"); emit_json_string(ds[i].name);
                printf(",\"home_target_set\":%s", ds[i].home_target_set ? "true" : "false");
                printf(",\"home_env_var\":"); emit_json_string(ds[i].home_env_var); printf("}");
            }
            printf("]}\n");

        } else if (strcmp(op, "match_origin") == 0) {
            scheduler_origin_t o;
            scheduler_resolve_origin(&g_job, &o);
            int r = scheduler_target_matches_origin(&o, a[0], na > 1 ? a[1] : "",
                                                    na > 2 ? a[2] : "");
            printf("{\"op\":\"match_origin\",\"platform\":"); emit_json_string(a[0]);
            printf(",\"chat_id\":"); emit_json_string(na > 1 ? a[1] : "");
            printf(",\"thread_id\":"); emit_json_string(na > 2 ? a[2] : "");
            printf(",\"matches\":%s}\n", r ? "true" : "false");
        }
    }
    fclose(f);
    return 0;
}
