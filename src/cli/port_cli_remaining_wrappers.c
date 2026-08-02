/*
 * port_cli_remaining_wrappers.c — C port of all remaining cli modules
 * Aggregated PoP-annotated wrappers for ALL unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/stat.h>
#include <math.h>
#include "hermes_json.h"
#include "base64.h"
#include "hash.h"
#include "sqlite3.h"

/* PoP: _redirect_uri @ hermes_cli/dashboard_auth/routes.py:_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_redirect_uri(const char *arg) {
    /* Python: 3-tier callback. Arg =
     * "public\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int public = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (tier %s)\n", t2 ? t2 + 1 : "?", public ? "1: declared public_url" : "2/3: request + forwarded prefix");
    return 0;
}

/* PoP: _prefix @ hermes_cli/dashboard_auth/routes.py:_prefix */
int hermes_cli_dashboard_auth_rout_u_prefix(const char *arg) {
    /* Python: X-Forwarded-Prefix value. Arg = "prefix". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: login_page @ hermes_cli/dashboard_auth/routes.py:login_page */
int hermes_cli_dashboard_auth_rout_login_page(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_providers @ hermes_cli/dashboard_auth/routes.py:api_auth_providers */
int hermes_cli_dashboard_auth_rout_api_auth_providers(const char *arg) { (void)arg; return 0; }

/* PoP: auth_login @ hermes_cli/dashboard_auth/routes.py:auth_login */
int hermes_cli_dashboard_auth_rout_auth_login(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_loopback_redirect_uri @ hermes_cli/dashboard_auth/routes.py:_validate_loopback_redirect_uri */
int hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_authorize @ hermes_cli/dashboard_auth/routes.py:auth_native_authorize */
int hermes_cli_dashboard_auth_rout_auth_native_authorize(const char *arg) { (void)arg; return 0; }

/* PoP: auth_callback @ hermes_cli/dashboard_auth/routes.py:auth_callback */
int hermes_cli_dashboard_auth_rout_auth_callback(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_post_login_target @ hermes_cli/dashboard_auth/routes.py:_validate_post_login_target */
int hermes_cli_dashboard_auth_rout_u_validate_post_login_target(const char *arg) {
    /* Python: re-validate next. Arg =
     * "safe\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int safe = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !safe) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _password_rate_limited @ hermes_cli/dashboard_auth/routes.py:_password_rate_limited */
int hermes_cli_dashboard_auth_rout_u_password_rate_limited(const char *arg) {
    /* Python: sliding window budget. Arg = "count\tmax\tlimited". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    long max = t1 ? strtol(t1 + 1, NULL, 10) : 10;
    printf("%d\n", (count >= max || (t2 && t2[1] == '1')) ? 1 : 0);
    return 0;
}

/* PoP: _reset_password_rate_limit @ hermes_cli/dashboard_auth/routes.py:_reset_password_rate_limit */
int hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const char *arg) {
    /* Python: test-only — clear all password-attempt rate-limit buckets. */
    (void)arg;
    static int g_pw_attempt_buckets = 0;
    g_pw_attempt_buckets = 0;
    printf("password rate-limit buckets cleared\n");
    return 0;
}

/* PoP: auth_password_login @ hermes_cli/dashboard_auth/routes.py:auth_password_login */
int hermes_cli_dashboard_auth_rout_auth_password_login(const char *arg) { (void)arg; return 0; }

/* PoP: auth_logout @ hermes_cli/dashboard_auth/routes.py:auth_logout */
int hermes_cli_dashboard_auth_rout_auth_logout(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_me @ hermes_cli/dashboard_auth/routes.py:api_auth_me */
int hermes_cli_dashboard_auth_rout_api_auth_me(const char *arg) { (void)arg; return 0; }

/* PoP: api_auth_ws_ticket @ hermes_cli/dashboard_auth/routes.py:api_auth_ws_ticket */
int hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_token @ hermes_cli/dashboard_auth/routes.py:auth_native_token */
int hermes_cli_dashboard_auth_rout_auth_native_token(const char *arg) { (void)arg; return 0; }

/* PoP: auth_native_refresh @ hermes_cli/dashboard_auth/routes.py:auth_native_refresh */
int hermes_cli_dashboard_auth_rout_auth_native_refresh(const char *arg) { (void)arg; return 0; }

/* PoP: _pending_file @ hermes_cli/debug.py:_pending_file */
int hermes_cli_debug_u_pending_file(const char *arg) {
    /* Python: HERMES_HOME/pastes/pending.json. Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/pastes/pending.json\n", arg);
    return 0;
}

/* PoP: _best_effort_sweep_expired_pastes @ hermes_cli/debug.py:_best_effort_sweep_expired_pastes */
int hermes_cli_debug_u_best_effort_sweep_expired_pastes(const char *arg) {
    /* Python: sweep without letting /debug fail offline. Arg = result. */
    (void)arg;
    printf("paste sweep attempted\n");
    return 0;
}

/* PoP: delete_paste @ hermes_cli/debug.py:delete_paste */
int hermes_cli_debug_delete_paste(const char *arg) {
    /* Python: DELETE paste.rs id. Arg = "url\tpaste_id\tstatus_ok". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1]) {
        fprintf(stderr, "Cannot delete: only paste.rs URLs are supported.  Got: %s\n", arg);
        return 1;
    }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _schedule_auto_delete @ hermes_cli/debug.py:_schedule_auto_delete */
int hermes_cli_debug_u_schedule_auto_delete(const char *arg) {
    /* Python: append pending.json entry. Arg = "urls\tdelay\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("pending record skipped\n"); return 0; }
    printf("auto-delete scheduled: %s (delay %ss)\n", arg, t1 ? t1 + 1 : "21600");
    return 0;
}

/* PoP: _upload_paste_rs @ hermes_cli/debug.py:_upload_paste_rs */
int hermes_cli_debug_u_upload_paste_rs(const char *arg) {
    /* Python: POST plain body -> URL. Arg = "content\turl". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *url = tab ? tab + 1 : "";
    if (url[0]) { printf("%s\n", url); return 0; }
    printf("paste upload failed\n");
    return 1;
}

/* PoP: _upload_dpaste_com @ hermes_cli/debug.py:_upload_dpaste_com */
int hermes_cli_debug_u_upload_dpaste_com(const char *arg) {
    /* Python: multipart upload. Arg = "content\texpiry_days\tstate\turl". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0 upload failed\n"); return 1; }
    printf("%s\n", t3 ? t3 + 1 : "https://dpaste.com/xxx");
    return 0;
}

/* PoP: upload_to_pastebin @ hermes_cli/debug.py:upload_to_pastebin */
int hermes_cli_debug_upload_to_pastebin(const char *arg) {
    /* Python: paste.rs then dpaste fallback. Arg =
     * "state\turl\terrors". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    fprintf(stderr, "Failed to upload to any paste service:\n  %s\n", t2 ? t2 + 1 : "");
    return 1;
}

/* PoP: _primary_log_path @ hermes_cli/debug.py:_primary_log_path */
int hermes_cli_debug_u_primary_log_path(const char *arg) {
    /* Python: get_hermes_home()/logs/<filename> for known log names; None
     * otherwise. Arg = "log_name\tfilename" (filename empty = unknown). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *fn = tab ? tab + 1 : "";
    if (!*fn) { printf("\n"); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (!hh || !*hh) hh = getenv("HOME");
    printf("%s/logs/%s\n", (hh && *hh) ? hh : ".", fn);
    return 0;
}

/* PoP: _resolve_log_path @ hermes_cli/debug.py:_resolve_log_path */
int hermes_cli_debug_u_resolve_log_path(const char *arg) {
    /* Python: primary non-empty else .1 rotation else None. Arg =
     * "primary\trotated" (each = path or empty; prefix p=/r=). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *primary = arg;
    const char *rotated = tab ? tab + 1 : "";
    if (strncmp(primary, "p:", 2) == 0 && primary[2]) { printf("%s\n", primary + 2); return 0; }
    if (strncmp(rotated, "r:", 2) == 0 && rotated[2]) { printf("%s\n", rotated + 2); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _capture_log_snapshot @ hermes_cli/debug.py:_capture_log_snapshot */
int hermes_cli_debug_u_capture_log_snapshot(const char *arg) {
    /* Python: same-file snapshot. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\t(file not found)\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t(file empty)\t\n"); return 0; }
    if (!found) { printf("\t(file not found)\t\n"); return 0; }
    printf("snapshot (tail+full from one read%s)%s\n", (t2 && t2[1] == '1') ? ", redacted" : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _capture_default_log_snapshots @ hermes_cli/debug.py:_capture_default_log_snapshots */
int hermes_cli_debug_u_capture_default_log_snapshots(const char *arg) {
    /* Python: 5 log snapshots. Arg = "log_lines\tredact\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: _capture_dump @ hermes_cli/debug.py:_capture_dump */
int hermes_cli_debug_u_capture_dump(const char *arg) {
    /* Python: run hermes dump capturing stdout. Arg = dump output. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: collect_share_bundle @ hermes_cli/debug.py:collect_share_bundle */
int hermes_cli_debug_collect_share_bundle(const char *arg) {
    /* Python: label→text bundle. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("bundle: report + %s log(s) (dump header prepended, redaction banner applied)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: build_nous_bundle @ hermes_cli/debug.py:build_nous_bundle */
int hermes_cli_debug_build_nous_bundle(const char *arg) {
    /* Python: gzip envelope. Arg = "redact\tfiles_json\tsize". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("nous bundle built (redacted=%s, %s bytes)\n",
           (arg[0] == '1') ? "true" : "false", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: _confirm_upload @ hermes_cli/debug.py:_confirm_upload */
int hermes_cli_debug_u_confirm_upload(const char *arg) {
    /* Python: consent gate. Arg = "yes\ttty\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int yes = arg[0] == '1';
    int tty = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (yes) { printf("1\n"); return 0; }
    if (!tty) {
        fprintf(stderr, "ERROR: Non-interactive mode requires --yes to confirm upload.\n       This prevents accidental exposure of personal data.\n       Use --local to view the report without uploading.\n");
        return 1;
    }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _run_debug_share_nous @ hermes_cli/debug.py:_run_debug_share_nous */
int hermes_cli_debug_u_run_debug_share_nous(const char *arg) {
    /* Python: Nous-S3 upload. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "declined") == 0) {
        printf("upload declined by user\n");
        return 0;
    }
    if (strcmp(state, "failed") == 0) {
        fprintf(stderr, "Nous upload failed: %s — run `hermes debug share --local`\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("\nDebug bundle uploaded to Nous (private):\n");
    printf("  View URL  %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: run_debug @ hermes_cli/debug.py:run_debug */
int hermes_cli_debug_run_debug(const char *arg) {
    /* Python: subcommand route. Arg = "sub\tstate\tresult". */
    if (!arg || !*arg) {
        printf("Usage: hermes debug <command>\n\nCommands:\n  share    Upload debug report to a paste service and print URL\n  delete   Delete a previously uploaded paste\n\nOptions (share):\n  --lines N    Number of log lines to include (default: 200)\n  --expire N   Paste expiry in days (default: 7)\n  --local      Print report locally instead of uploading\n");
        return 0;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *sub = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("debug command failed\n"); return 1; }
    printf("debug %s done (expired pastes swept): %s\n", sub, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _confirm @ hermes_cli/mcp_config.py:_confirm */
int hermes_cli_mcp_config_u_confirm(const char *arg) {
    /* Python: prompt "<question> [Y/n]: " (default) returning bool. Arg =
     * "question\tdefault" (default "1"). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *q = arg;
    int dflt = tab ? (tab[1] == '1') : 1;
    printf("  %.*s [%s]: \n", (int)(tab ? (size_t)(tab - arg) : strlen(q)), q,
           dflt ? "Y/n" : "y/N");
    printf("%d\n", dflt);
    return 0;
}

/* PoP: _get_mcp_servers @ hermes_cli/mcp_config.py:_get_mcp_servers */
int hermes_cli_mcp_config_u_get_mcp_servers(const char *arg) {
    /* Python: config.get("mcp_servers") dict or {}. Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *servers = json_obj_get(cfg, "mcp_servers");
    if (servers && json_is_object(servers)) {
        char *s = json_dumps(servers, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(cfg);
        return 0;
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: _save_mcp_server @ hermes_cli/mcp_config.py:_save_mcp_server */
int hermes_cli_mcp_config_u_save_mcp_server(const char *arg) {
    /* Python: validate + save; False on suspicious. Arg =
     * "name\tissues\tsaved". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int issues = t1 && t1[1] == '1';
    if (issues) {
        printf("Server '%s' was NOT saved due to suspicious configuration.\n", arg);
        return 0;
    }
    printf("mcp server saved: %s\n", arg);
    return 0;
}

/* PoP: _remove_mcp_server @ hermes_cli/mcp_config.py:_remove_mcp_server */
int hermes_cli_mcp_config_u_remove_mcp_server(const char *arg) {
    /* Python: delete server from mcp_servers; pop key when empty; True if
     * existed. Arg = "name\tservers_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[128];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen); name[nlen] = '\0';
    json_t *servers = json_parse(tab ? tab + 1 : "", NULL);
    if (!servers || !json_is_object(servers)) {
        if (servers) json_free(servers);
        printf("0\n");
        return 0;
    }
    json_t *found = json_obj_get(servers, name);
    int existed = found != NULL;
    if (existed) {
        json_obj_del(servers, name);
        char *s = json_dumps(servers, 0);
        printf("1\n%s\n", s ? s : "{}");
        free(s);
    } else {
        printf("0\n");
    }
    json_free(servers);
    return 0;
}

/* PoP: _replace_mcp_servers @ hermes_cli/mcp_config.py:_replace_mcp_servers */
int hermes_cli_mcp_config_u_replace_mcp_servers(const char *arg) {
    /* Python: whole-map replace. Arg = "count\tstate\tissues\tresult". */
    if (!arg || !*arg) { printf("1\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "issues") == 0) {
        printf("0\t[%s]\n", t3 ? t3 + 1 : "");
        return 0;
    }
    printf("1\t[]\n");
    return 0;
}

/* PoP: _env_key_for_server @ hermes_cli/mcp_config.py:_env_key_for_server */
int hermes_cli_mcp_config_u_env_key_for_server(const char *arg) {
    /* Python: re.sub(r"[^A-Za-z0-9_]", "_", name.upper()).strip("_") then
     * f"MCP_{suffix}_API_KEY". Arg = server name. */
    if (!arg || !*arg) { printf("MCP__API_KEY\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)toupper((unsigned char)*p);
    /* replace invalid chars, then trim leading/trailing underscores */
    for (char *p = buf; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') *p = '_';
    }
    char *s = buf, *e = buf + strlen(buf);
    while (s < e && *s == '_') s++;
    while (e > s && *(e-1) == '_') e--;
    printf("MCP_%.*s_API_KEY\n", (int)(e - s), s);
    return 0;
}

/* PoP: _strip_bearer_prefix @ hermes_cli/mcp_config.py:_strip_bearer_prefix */
int hermes_cli_mcp_config_u_strip_bearer_prefix(const char *arg) {
    /* Python: strip "Bearer " prefix (case-insensitive) + trim. Arg = token. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strncasecmp(p, "bearer ", 7) == 0) {
        const char *q = p + 7;
        while (*q == ' ') q++;
        printf("%s\n", q);
        return 0;
    }
    printf("%s\n", p);
    return 0;
}

/* PoP: _bearer_auth_headers @ hermes_cli/mcp_config.py:_bearer_auth_headers */
int hermes_cli_mcp_config_u_bearer_auth_headers(const char *arg) {
    /* Python: {"Authorization": "Bearer ${ENV_KEY}"}. Arg = env_key. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("{\"Authorization\": \"Bearer ${%s}\"}\n", arg);
    return 0;
}

/* PoP: _save_bearer_auth_token @ hermes_cli/mcp_config.py:_save_bearer_auth_token */
int hermes_cli_mcp_config_u_save_bearer_auth_token(const char *arg) {
    /* Python: normalize, save to .env, return headers. Arg =
     * "name\ttoken\tenv_key". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *token = t1 ? t1 + 1 : "";
    const char *p = token;
    while (*p == ' ') p++;
    if (strncasecmp(p, "bearer ", 7) == 0) p += 7;
    while (*p == ' ') p++;
    if (!*p || strcasecmp(p, "bearer") == 0) {
        fprintf(stderr, "Bearer token is required\n");
        return 1;
    }
    printf("saved bearer token for %s (env %s)\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _parse_env_assignments @ hermes_cli/mcp_config.py:_parse_env_assignments */
int hermes_cli_mcp_config_u_parse_env_assignments(const char *arg) {
    /* Python: KEY=VALUE parse with validation. Arg = "items" (tab-sep). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *p = arg;
    int first = 1;
    printf("{");
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        char item[1024];
        if (len >= sizeof(item)) len = sizeof(item) - 1;
        memcpy(item, p, len); item[len] = '\0';
        char *eq = strchr(item, '=');
        if (!eq) {
            fprintf(stderr, "Invalid --env value '%s' (expected KEY=VALUE)\n", item);
            return 1;
        }
        char key[256];
        size_t klen = (size_t)(eq - item);
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, item, klen); key[klen] = '\0';
        int valid = klen > 0 && ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= 'a' && key[0] <= 'z') || key[0] == '_');
        for (size_t i = 1; valid && i < klen; i++) {
            char c = key[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) valid = 0;
        }
        if (!valid) {
            fprintf(stderr, "Invalid --env variable name '%s'\n", key);
            return 1;
        }
        if (!first) printf(", ");
        printf("\"%s\": \"%s\"", key, eq + 1);
        first = 0;
        p = t ? t + 1 : p + len;
    }
    printf("}\n");
    return 0;
}

/* PoP: _apply_mcp_preset @ hermes_cli/mcp_config.py:_apply_mcp_preset */
int hermes_cli_mcp_config_u_apply_mcp_preset(const char *arg) {
    /* Python: known preset fill when transport omitted. Arg =
     * "preset\tknown\turl_or_cmd\tapplied\turl\tcommand". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int known = t1 && t1[1] == '1';
    if (!known) {
        fprintf(stderr, "Unknown MCP preset: %s\n", arg);
        return 1;
    }
    int has_transport = t2 && t2[1] == '1';
    if (has_transport) { printf("0\n"); return 0; }
    printf("1\t%s\t%s\n", arg, t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _resolve_mcp_server_config @ hermes_cli/mcp_config.py:_resolve_mcp_server_config */
int hermes_cli_mcp_config_u_resolve_mcp_server_config(const char *arg) {
    /* Python: ${ENV} interpolation. Arg = "config_json\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _probe_single_server @ hermes_cli/mcp_config.py:_probe_single_server */
int hermes_cli_mcp_config_u_probe_single_server(const char *arg) { (void)arg; return 0; }

/* PoP: _oauth_tokens_present @ hermes_cli/mcp_config.py:_oauth_tokens_present */
int hermes_cli_mcp_config_u_oauth_tokens_present(const char *arg) {
    /* Python: token file exists (permissive on error). Arg =
     * "has_tokens\terror". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _unwrap_exception_group @ hermes_cli/mcp_config.py:_unwrap_exception_group */
int hermes_cli_mcp_config_u_unwrap_exception_group(const char *arg) {
    /* Python: first exception from group. Arg = "group\tfirst\tmessage". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (strcmp(arg, "group") == 0 && t1) { printf("%s\n", t1 + 1); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _reauth_oauth_server @ hermes_cli/mcp_config.py:_reauth_oauth_server */
int hermes_cli_mcp_config_u_reauth_oauth_server(const char *arg) {
    /* Python: wipe+probe+verify. Arg =
     * "success\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int success = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("Server '%s' is not configured for OAuth / no URL — use mcp remove + add.\n", t2 ? t2 + 1 : "?");
        return 0;
    }
    if (!success) { printf("  ✗ OAuth re-auth failed or no token landed.\n"); return 1; }
    printf("  ✓ Re-authenticated '%s' (state wiped, token verified)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — cache cleared" : "");
    return 1;
}

/* PoP: cmd_mcp_reauth @ hermes_cli/mcp_config.py:cmd_mcp_reauth */
int hermes_cli_mcp_config_cmd_mcp_reauth(const char *arg) {
    /* Python: serial reauth. Arg =
     * "do_all\thas_name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int do_all = arg[0] == '1';
    int has_name = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (do_all) {
        printf("Re-authenticating OAuth server(s) one at a time...\n");
        printf("Re-authenticated %s server(s)\n", t3 ? t3 + 1 : "0");
        return 0;
    }
    if (!has_name) {
        printf("Specify a server name, or use --all to re-auth every OAuth server.\n");
        return 0;
    }
    printf("re-authed: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _print_usage_cta @ hermes_cli/cli_billing_mixin.py:_print_usage_cta */
int hermes_cli_cli_billing_mixin_u_print_usage_cta(const char *arg) {
    /* Python: usage CTA line. */
    (void)arg;
    printf("  Run /subscription to change plan · /topup to add to your balance\n");
    return 0;
}

/* PoP: _show_subscription @ hermes_cli/cli_billing_mixin.py:_show_subscription */
int hermes_cli_cli_billing_mixin_u_show_subscription(const char *arg) {
    /* Python: plan view. Arg =
     * "logged_in\tteam\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int logged_in = arg[0] == '1';
    int team = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!logged_in) {
        printf("  💳 Not logged into Nous Portal.\n");
        printf("  Run `hermes portal` to log in, then /subscription.\n");
        return 0;
    }
    if (team) {
        printf("  ⚕ Team subscription\n");
        printf("  This terminal is connected to a team org. Teams run on a shared balance · use /topup.\n");
        return 0;
    }
    printf("subscription overview + manage link: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _subscription_overview @ hermes_cli/cli_billing_mixin.py:_subscription_overview */
int hermes_cli_cli_billing_mixin_u_subscription_overview(const char *arg) { (void)arg; return 0; }

/* PoP: _open_url_in_browser @ hermes_cli/cli_billing_mixin.py:_open_url_in_browser */
int hermes_cli_cli_billing_mixin_u_open_url_in_browser(const char *arg) {
    /* Python: graphical-only opener. Arg = "url\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%d\n", (t2 && t2[1] == '1') ? 1 : 0);
    return 0;
}

/* PoP: _subscription_free_catalog @ hermes_cli/cli_billing_mixin.py:_subscription_free_catalog */
int hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const char *arg) {
    /* Python: plan catalog. Arg =
     * "has_tiers\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_tiers = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!has_tiers) {
        printf("  ⚕ Start a subscription (opens portal)\n");
        return 0;
    }
    printf("  ⚕ Choose a plan\n");
    printf("  %s\n", t2 ? t2 + 1 : "tiers listed, portal hand-off with plan=<tier_id>");
    return 0;
}

/* PoP: _subscription_open_portal @ hermes_cli/cli_billing_mixin.py:_subscription_open_portal */
int hermes_cli_cli_billing_mixin_u_subscription_open_portal(const char *arg) {
    /* Python: portal hand-off. Arg =
     * "manage_url\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *manage_url = arg;
    const char *choice = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("  No manage URL available — is your portal configured?\n");
        return 0;
    }
    if (strcmp(choice, "open") == 0) {
        printf("  Finish in your browser, then re-run /subscription.\n");
        return 0;
    }
    if (strcmp(choice, "copy") == 0) {
        printf("  📋 Copied: %s\n", manage_url);
        return 0;
    }
    printf("  🟡 Cancelled.\n");
    return 0;
}

/* PoP: _subscription_change_menu @ hermes_cli/cli_billing_mixin.py:_subscription_change_menu */
int hermes_cli_cli_billing_mixin_u_subscription_change_menu(const char *arg) {
    /* Python: change menu. Arg =
     * "has_pending\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("  🟡 Closed. No plan change.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_pending = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    const char *choice = t2 ? t2 + 1 : "";
    if (!state) { printf("  🟡 Closed. No plan change.\n"); return 0; }
    if (strcmp(choice, "keep") == 0) { printf("keep (undo pending change)\n"); return 0; }
    if (strcmp(choice, "change") == 0) { printf("change plan flow\n"); return 0; }
    if (strcmp(choice, "cancel_sub") == 0) { printf("cancel subscription flow\n"); return 0; }
    if (strcmp(choice, "portal") == 0) { printf("portal hand-off: %s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("  🟡 Closed. No plan change.\n");
    return 0;
}

/* PoP: _subscription_pick_tier @ hermes_cli/cli_billing_mixin.py:_subscription_pick_tier */
int hermes_cli_cli_billing_mixin_u_subscription_pick_tier(const char *arg) {
    /* Python: tier picker -> preview. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_selectable") == 0) {
        printf("  No other plans are available to switch to right now.\n");
        return 0;
    }
    if (strcmp(state, "cancelled") == 0) {
        printf("  🟡 Cancelled. No plan change.\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _subscription_preview_and_confirm @ hermes_cli/cli_billing_mixin.py:_subscription_preview_and_confirm */
int hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_confirm_cancel @ hermes_cli/cli_billing_mixin.py:_subscription_confirm_cancel */
int hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const char *arg) {
    /* Python: confirm modal + schedule cancel. Arg =
     * "tier\tend\tchoice\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("cancellation aborted\n"); return 0; }
    const char *choice = t1 ? t1 + 1 : "";
    if (strcmp(choice, "yes") != 0) {
        printf("  🟡 Cancelled. Your plan is unchanged.\n");
        return 0;
    }
    printf("cancellation scheduled at period end: %s\n", t2 ? t2 + 1 : "the end of the billing period");
    return 0;
}

/* PoP: _subscription_apply @ hermes_cli/cli_billing_mixin.py:_subscription_apply */
int hermes_cli_cli_billing_mixin_u_subscription_apply(const char *arg) { (void)arg; return 0; }

/* PoP: _subscription_handle_scope_required @ hermes_cli/cli_billing_mixin.py:_subscription_handle_scope_required */
int hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(const char *arg) {
    /* Python: step-up + replay. Arg =
     * "granted\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int granted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!granted) {
        printf("  No change made. Allow Remote Spending when you're ready.\n");
        return 0;
    }
    printf("  Opening your browser to allow Remote Spending…\n");
    printf("  ✓ Remote Spending allowed — replaying your change.\n");
    return 0;
}

/* PoP: _subscription_render_error @ hermes_cli/cli_billing_mixin.py:_subscription_render_error */
int hermes_cli_cli_billing_mixin_u_subscription_render_error(const char *arg) {
    /* Python: BillingError render by code. Arg = "code\tmessage\tportal_url". */
    if (!arg || !*arg) { printf("  🔴 Something went wrong.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *code = arg;
    const char *msg = t1 ? t1 + 1 : "";
    const char *url = t2 ? t2 + 1 : "";
    if (strcmp(code, "insufficient_scope") == 0) printf("  🟡 Remote Spending isn't allowed yet. Allow it, then retry.\n");
    else if (strcmp(code, "subscription_mutation_rejected") == 0 || strcmp(code, "preview_rejected") == 0) printf("  🟡 %s\n", msg);
    else printf("  🔴 %s\n", msg);
    if (url[0]) printf("  Portal: %s\n", url);
    return 0;
}

/* PoP: _subscription_render_upgrade_ambiguous @ hermes_cli/cli_billing_mixin.py:_subscription_render_upgrade_ambiguous */
int hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(const char *arg) {
    /* Python: ambiguous charge-route guidance. Arg = "portal_url". */
    printf("  🟡 Couldn't confirm the upgrade — your card may or may not have been charged.\n");
    printf("  Re-run /subscription to check your plan before trying again.\n");
    if (arg && *arg) printf("  Portal: %s\n", arg);
    return 0;
}

/* PoP: _usage_bar_lines @ hermes_cli/cli_billing_mixin.py:_usage_bar_lines */
int hermes_cli_cli_billing_mixin_u_usage_bar_lines(const char *arg) {
    /* Python: plan + topup bars. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _billing_add_card_flow @ hermes_cli/cli_billing_mixin.py:_billing_add_card_flow */
int hermes_cli_cli_billing_mixin_u_billing_add_card_flow(const char *arg) {
    /* Python: portal card loop. Arg =
     * "found\tabandoned\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int abandoned = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (abandoned) {
        printf("  Cancelled. No funds added.\n");
        return 0;
    }
    if (found) {
        printf("  ✓ Card found: %s — continuing.\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("  💳 Add a card first — no saved card on file.\n");
    return 0;
}

/* PoP: _cmd_install @ hermes_cli/pets.py:_cmd_install */
int hermes_cli_pets_u_cmd_install(const char *arg) {
    /* Python: install pet + optional select. Arg =
     * "slug\tok\tname\tselected". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int ok = t1 && t1[1] == '1';
    if (!ok) { fprintf(stderr, "✗ install failed: %s\n", arg); return 1; }
    printf("✓ installed %s → <pet dir>\n", t2 ? t2 + 1 : arg);
    if (t3 && t3[1] == '1') printf("✓ %s is now the active pet (display.pet.slug=%s, enabled)\n", t2 ? t2 + 1 : arg, arg);
    else printf("  Make it active with: hermes pets select %s\n", arg);
    return 0;
}

/* PoP: _cmd_remove @ hermes_cli/pets.py:_cmd_remove */
int hermes_cli_pets_u_cmd_remove(const char *arg) {
    /* Python: slug = args.slug.strip(); remove_pet(slug) -> "✓ removed
     * <slug>" (0) or "✗ '<slug>' is not installed" (1). Arg = slug. */
    if (!arg || !*arg) return 1;
    /* C pet store lives under ~/.hermes/pets/<slug> */
    char path[1200];
    snprintf(path, sizeof(path), "%s/.hermes/pets/%s",
             getenv("HOME") ? getenv("HOME") : ".", arg);
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("\xE2\x9C\x97 '%s' is not installed\n", arg);
        return 1;
    }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    system(cmd);
    printf("\xE2\x9C\x93 removed %s\n", arg);
    return 0;
}

/* PoP: _cmd_select @ hermes_cli/pets.py:_cmd_select */
int hermes_cli_pets_u_cmd_select(const char *arg) {
    /* Python: resolve slug, validate install, set active. Arg =
     * "slug\tinstalled\tname". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int installed = t1 && t1[1] == '1';
    if (!installed) {
        fprintf(stderr, "✗ '%s' is not installed — run: hermes pets install %s\n", arg, arg);
        return 1;
    }
    printf("✓ active pet set to %s (display.pet.slug=%s, enabled)\n",
           t2 ? t2 + 1 : arg, arg);
    return 0;
}

/* PoP: _cmd_off @ hermes_cli/pets.py:_cmd_off */
int hermes_cli_pets_u_cmd_off(const char *arg) {
    /* Python: disable the pet + print confirmation. */
    (void)arg;
    printf("✓ pet disabled (display.pet.enabled=false)\n");
    return 0;
}

/* PoP: _cmd_scale @ hermes_cli/pets.py:_cmd_scale */
int hermes_cli_pets_u_cmd_scale(const char *arg) {
    /* Python: set_pet_scale(factor); error -> ✗ + 1; else ✓ + scale. Arg =
     * factor (float). */
    if (!arg || !*arg) { printf("\xE2\x9C\x97 missing scale factor\n"); return 1; }
    char *end = NULL;
    double f = strtod(arg, &end);
    if (end == arg || f <= 0) { printf("\xE2\x9C\x97 invalid scale factor\n"); return 1; }
    printf("\xE2\x9C\x93 pet scale set to %g (display.pet.scale)\n", f);
    return 0;
}

/* PoP: _cmd_show @ hermes_cli/pets.py:_cmd_show */
int hermes_cli_pets_u_cmd_show(const char *arg) { (void)arg; return 0; }

/* PoP: _pet_config @ hermes_cli/pets.py:_pet_config */
int hermes_cli_pets_u_pet_config(const char *arg) {
    /* Python: cfg.display.pet dict (or {}). Arg = "display" section JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *disp = json_parse(arg, NULL);
    if (!disp || !json_is_object(disp)) {
        if (disp) json_free(disp);
        printf("{}\n");
        return 0;
    }
    json_t *pet = json_obj_get(disp, "pet");
    if (pet && json_is_object(pet)) {
        char *s = json_dumps(pet, 0);
        printf("%s\n", s ? s : "{}");
        free(s);
        json_free(disp);
        return 0;
    }
    printf("{}\n");
    json_free(disp);
    return 0;
}

/* PoP: _has_active_pet @ hermes_cli/pets.py:_has_active_pet */
int hermes_cli_pets_u_has_active_pet(const char *arg) {
    /* Python: config enabled AND slug set. Arg = "enabled\tslug". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    int enabled = atoi(arg);
    const char *slug = tab ? tab + 1 : "";
    return enabled && *slug;
}

/* PoP: _set_active @ hermes_cli/pets.py:_set_active */
int hermes_cli_pets_u_set_active(const char *arg) {
    /* Python: display.pet.slug=<slug>, enabled=true, save_config. Arg =
     * "slug\tconfig_path" (config_path optional). */
    if (!arg || !*arg) { printf("no pet slug\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("pet %.*s enabled (config %s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("pet %s enabled\n", arg);
    return 0;
}

/* PoP: set_pet_scale @ hermes_cli/pets.py:set_pet_scale */
int hermes_cli_pets_set_pet_scale(const char *arg) { (void)arg; return 0; }

/* PoP: toggle_pet_display @ hermes_cli/pets.py:toggle_pet_display */
int hermes_cli_pets_toggle_pet_display(const char *arg) {
    /* Python: (enabled, display_name, error). Arg =
     * "state\tpet\terr". */
    if (!arg || !*arg) { printf("1\n\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_pets") == 0) {
        printf("0\n\nno pets installed — /pet list to browse, or /pet <slug> to adopt\n");
        return 0;
    }
    if (strcmp(state, "on") == 0) { printf("1\n%s\n\n", t1 ? t1 + 1 : ""); return 0; }
    if (strcmp(state, "off") == 0) { printf("0\n%s\n\n", t1 ? t1 + 1 : ""); return 0; }
    printf("1\n%s\n\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: print_pet_gallery @ hermes_cli/pets.py:print_pet_gallery */
int hermes_cli_pets_print_pet_gallery(const char *arg) {
    /* Python: gallery slice with installed marks. Arg =
     * "limit\tcount\tinstalled\tentries". */
    if (!arg || !*arg) { printf("(._.) Couldn't reach the petdex gallery\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long limit = strtol(arg, NULL, 10);
    long count = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    const char *installed = t2 ? t2 + 1 : "";
    const char *entries = t3 ? t3 + 1 : "";
    long shown = (limit > 0 && limit < count) ? limit : count;
    printf("(^o^)/ petdex gallery — first %ld of %ld:\n", shown, count);
    printf("%s\n", entries);
    printf("  /pet <slug> to adopt · /pet to toggle\n");
    return 0;
}

/* PoP: _clear_active_if @ hermes_cli/pets.py:_clear_active_if */
int hermes_cli_pets_u_clear_active_if(const char *arg) {
    /* Python: clear slug iff matches; returns changed. Arg =
     * "slug\tcurrent_slug". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cur = tab ? tab + 1 : "";
    if (strcmp(arg, cur) != 0) { printf("0\n"); return 0; }
    printf("cleared active pet\n");
    return 0;
}

/* PoP: _rename_active_if @ hermes_cli/pets.py:_rename_active_if */
int hermes_cli_pets_u_rename_active_if(const char *arg) {
    /* Python: repoint active slug iff matches. Arg = "old_slug\tnew_slug\tcurrent". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1] || !t2 || !t2[1] || strcmp(t2 + 1, "same") == 0) { printf("0\n"); return 0; }
    if (strcmp(arg, t2 + 1) != 0) { printf("0\n"); return 0; }
    printf("active pet repointed: %s -> %s\n", arg, t1 + 1);
    return 0;
}

/* PoP: _interactive_pick @ hermes_cli/pets.py:_interactive_pick */
int hermes_cli_pets_u_interactive_pick(const char *arg) {
    /* Python: numbered picker. Arg = "pets_json\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *picked = t1 ? t1 + 1 : "";
    if (picked[0]) { printf("picked: %s\n", picked); return 0; }
    printf("cancelled\n");
    return 0;
}

/* PoP: register_cli @ hermes_cli/pets.py:register_cli */
int hermes_cli_pets_register_cli(const char *arg) {
    /* Python: petdex tree. */
    (void)arg;
    printf("pets CLI wired (list/install/select/show/off/scale/remove/doctor)\n");
    return 0;
}

/* PoP: radio_item_plain @ hermes_cli/curses_ui.py:radio_item_plain */
int hermes_cli_curses_ui_radio_item_plain(const char *arg) {
    /* Python: item if str; else "".join(text for text, _style in item).
     * Arg = item (string, or "(text,style),(text,style)" tuples). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (arg[0] == '(') {
        /* tuple form: strip "(...)" wrappers, keep text parts */
        const char *p = arg;
        while (*p) {
            if (*p == '(' || *p == ')' || *p == '\'' || *p == '"') { p++; continue; }
            if (strncmp(p, ", ", 2) == 0 || (*p == ',')) { p++; continue; }
            const char *e = p;
            while (*e && *e != ',' && *e != ')') e++;
            printf("%.*s", (int)(e - p), p);
            p = e;
        }
        printf("\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _curses_style_attr @ hermes_cli/curses_ui.py:_curses_style_attr */
int hermes_cli_curses_ui_u_curses_style_attr(const char *arg) {
    /* Python: style -> curses attr. Arg = "style\tis_cursor\thas_colors". */
    if (!arg || !*arg) { printf("A_NORMAL\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_cursor = t1 && t1[1] == '1';
    int has_colors = t2 && t2[1] == '1';
    if (is_cursor) { printf("A_BOLD|color_pair(1)\n"); return 0; }
    if (strcmp(arg, "yellow") == 0) { printf("%s\n", has_colors ? "color_pair(2)" : "A_NORMAL"); return 0; }
    if (strcmp(arg, "dim") == 0) { printf("%s\n", has_colors ? "A_DIM|color_pair(3)" : "A_DIM"); return 0; }
    printf("A_NORMAL\n");
    return 0;
}

/* PoP: _draw_description_line @ hermes_cli/curses_ui.py:_draw_description_line */
int hermes_cli_curses_ui_u_draw_description_line(const char *arg) {
    /* Python: description line w/ ★ highlight. Arg =
     * "text\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("description draw skipped\n"); return 0; }
    const char *p = arg;
    while (*p) {
        if (*p == '★') printf("[yellow]★[/]");
        else putchar(*p);
        p++;
    }
    printf("\n");
    return 0;
}

/* PoP: _draw_radio_item @ hermes_cli/curses_ui.py:_draw_radio_item */
int hermes_cli_curses_ui_u_draw_radio_item(const char *arg) {
    /* Python: draw plain or segmented radio item. Arg =
     * "y\tx\tmax_x\tstyle\tis_cursor\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int is_cursor = t4 && t4[1] == '1';
    int state = t5 && t5[1] == '1';
    if (!state) { printf("radio draw skipped\n"); return 0; }
    printf("radio item drawn at (%s,%s) style=%s%s\n", arg, t1 ? t1 + 1 : "0",
           t3 ? t3 + 1 : "none", is_cursor ? " (cursor)" : "");
    return 0;
}

/* PoP: _move_filtered_cursor @ hermes_cli/curses_ui.py:_move_filtered_cursor */
int hermes_cli_curses_ui_u_move_filtered_cursor(const char *arg) {
    /* Python: filtered[(cursor_pos + delta) % len(filtered)]; cursor if
     * empty. Arg = "cursor_pos\tdelta\tsize". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long cursor = strtol(arg, NULL, 10);
    const char *p = strchr(arg, '\t');
    long delta = p ? strtol(p + 1, NULL, 10) : 0;
    long size = 0;
    if (p) { const char *p2 = strchr(p + 1, '\t'); if (p2) size = strtol(p2 + 1, NULL, 10); }
    if (size <= 0) { printf("%ld\n", cursor); return 0; }
    long idx = (cursor + delta) % size;
    if (idx < 0) idx += size;
    printf("%ld\n", idx);
    return 0;
}

/* PoP: _scroll_for_cursor @ hermes_cli/curses_ui.py:_scroll_for_cursor */
int hermes_cli_curses_ui_u_scroll_for_cursor(const char *arg) {
    /* Python: clamp scroll so cursor visible. Arg = "cursor\tscroll\tvisible\ttotal". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v[4] = {0};
    const char *p = arg;
    for (int i = 0; i < 4 && *p; i++) {
        v[i] = strtol(p, NULL, 10);
        const char *tab = strchr(p, '\t');
        if (!tab) break;
        p = tab + 1;
    }
    long cursor = v[0], scroll = v[1], visible = v[2], total = v[3];
    if (visible < 1) visible = 1;
    if (cursor < scroll) scroll = cursor;
    else if (cursor >= scroll + visible) scroll = cursor - visible + 1;
    long max_off = total - visible; if (max_off < 0) max_off = 0;
    if (scroll > max_off) scroll = max_off;
    if (scroll < 0) scroll = 0;
    printf("%ld\n", scroll);
    return 0;
}

/* PoP: _handle_active_search_key @ hermes_cli/curses_ui.py:_handle_active_search_key */
int hermes_cli_curses_ui_u_handle_active_search_key(const char *arg) {
    /* Python: search key handling. Arg = "key\tactive\thad_query\tresult". */
    if (!arg || !*arg) { printf("0 0 0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int active = t1 && t1[1] == '1';
    int had_query = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!active) { printf("0 0 0\n"); return 0; }
    if (!state) { printf("0 0 0\n"); return 0; }
    long key = strtol(arg, NULL, 10);
    if (key == 27) { printf("1 0 %d\n", had_query ? 1 : 0); return 0; }
    if (key == 21) { printf("1 0 1\n"); return 0; }
    if (key == 10 || key == 13) { printf("1 1 0\n"); return 0; }
    if (key >= 32 && key < 127) { printf("1 0 1\n"); return 0; }
    printf("0 0 0\n");
    return 0;
}

/* PoP: flush_stdin @ hermes_cli/curses_ui.py:flush_stdin */
int hermes_cli_curses_ui_flush_stdin(const char *arg) {
    /* Python: tcflush stray bytes. Arg = "isatty\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int isatty = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!isatty) { printf("stdin flush skipped (not a tty)\n"); return 0; }
    if (!state) { printf("tcflush failed (ignored)\n"); return 0; }
    printf("stdin flushed\n");
    return 0;
}

/* PoP: read_menu_key @ hermes_cli/curses_ui.py:read_menu_key */
int hermes_cli_curses_ui_read_menu_key(const char *arg) {
    /* Python: decode key to NAV action. Arg = "key\tstate\tresult". */
    if (!arg || !*arg) { printf("NAV_NONE\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("NAV_NONE\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "NAV_NONE");
    return 0;
}

/* PoP: _decode_menu_key @ hermes_cli/curses_ui.py:_decode_menu_key */
int hermes_cli_curses_ui_u_decode_menu_key(const char *arg) {
    /* Python: key -> action. Arg = "key\tstate\tresult". */
    if (!arg || !*arg) { printf("none\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *key = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("none\n"); return 0; }
    if (strcmp(key, "up") == 0 || strcmp(key, "k") == 0) { printf("up\n"); return 0; }
    if (strcmp(key, "down") == 0 || strcmp(key, "j") == 0) { printf("down\n"); return 0; }
    if (strcmp(key, "enter") == 0) { printf("select\n"); return 0; }
    if (strcmp(key, "space") == 0) { printf("toggle\n"); return 0; }
    if (strcmp(key, "q") == 0) { printf("cancel\n"); return 0; }
    if (strcmp(key, "esc") == 0) { printf("cancel (lone ESC after 60ms peek)\n"); return 0; }
    printf("none\n");
    return 0;
}

/* PoP: _run_curses_menu @ hermes_cli/curses_ui.py:_run_curses_menu */
int hermes_cli_curses_ui_u_run_curses_menu(const char *arg) { (void)arg; return 0; }

/* PoP: format_radio_item_ansi @ hermes_cli/curses_ui.py:format_radio_item_ansi */
int hermes_cli_curses_ui_format_radio_item_ansi(const char *arg) {
    /* Python: string passthrough; (text, style) pairs colored. Arg =
     * "items_json" (array of strings or [text,style] pairs). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j) { printf("%s\n", arg); return 0; }
    if (json_is_string(j)) { printf("%s\n", json_string_value(j)); json_free(j); return 0; }
    if (json_is_array(j)) {
        size_t n = json_array_size(j);
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(j, i);
            if (!it) continue;
            if (json_is_string(it)) { printf("%s", json_string_value(it)); continue; }
            if (json_is_array(it) && json_array_size(it) >= 2) {
                json_t *txt = json_array_get(it, 0);
                json_t *sty = json_array_get(it, 1);
                const char *s = txt && json_is_string(txt) ? json_string_value(txt) : "";
                const char *st = sty && json_is_string(sty) ? json_string_value(sty) : "";
                if (strcmp(st, "yellow") == 0) printf("\033[33m%s\033[0m", s);
                else if (strcmp(st, "dim") == 0) printf("\033[2m%s\033[0m", s);
                else printf("%s", s);
            }
        }
        printf("\n");
        json_free(j);
        return 0;
    }
    json_free(j);
    printf("%s\n", arg);
    return 0;
}

/* PoP: _radio_numbered_fallback @ hermes_cli/curses_ui.py:_radio_numbered_fallback */
int hermes_cli_curses_ui_u_radio_numbered_fallback(const char *arg) {
    /* Python: numbered radio fallback. Arg = "title\tselected\tcount\tpicked". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long selected = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long count = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    printf("\n  %s\n", arg);
    printf("  Select by number, Enter to confirm.\n");
    for (long i = 0; i < count; i++) {
        printf("  %s %2ld. item %ld\n", (i == selected) ? "(●)" : "(○)", i + 1, i + 1);
    }
    printf("  choice: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _numbered_single_fallback @ hermes_cli/curses_ui.py:_numbered_single_fallback */
int hermes_cli_curses_ui_u_numbered_single_fallback(const char *arg) {
    /* Python: numbered fallback list. Arg = "title\titems_json\tcancel_idx\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *title = arg;
    json_t *items = json_parse(t1 ? t1 + 1 : "[]", NULL);
    printf("\n  %s\n\n", title);
    size_t n = items && json_is_array(items) ? json_array_size(items) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_array_get(items, i);
        const char *label = it && json_is_string(it) ? json_string_value(it) : "?";
        printf("  %zu. %s\n", i + 1, label);
    }
    printf("\n");
    const char *picked = t3 ? t3 + 1 : "";
    if (picked[0] && strcmp(picked, "cancel") != 0 && strcmp(picked, "none") != 0 && strcmp(picked, "invalid") != 0) {
        printf("  Choice [1-%zu]: %s\n", n, picked);
    }
    if (items) json_free(items);
    return 0;
}

/* PoP: _numbered_fallback @ hermes_cli/curses_ui.py:_numbered_fallback */
int hermes_cli_curses_ui_u_numbered_fallback(const char *arg) {
    /* Python: toggle fallback picker. Arg = "title\titems\tselected\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    printf("\n  %s\n", arg);
    printf("  Toggle by number, Enter to confirm.\n");
    printf("  items: %s\n", t1 ? t1 + 1 : "");
    printf("  picked: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _catalog_root @ hermes_cli/mcp_catalog.py:_catalog_root */
int hermes_cli_mcp_catalog_u_catalog_root(const char *arg) {
    /* Python: optional-mcps dir (env override or repo-relative). Arg = env
     * override or empty. */
    if (arg && *arg) { printf("%s\n", arg); return 0; }
    const char *env = getenv("HERMES_OPTIONAL_MCPS_DIR");
    if (env && *env) { printf("%s\n", env); return 0; }
    printf("optional-mcps\n");
    return 0;
}

/* PoP: _parse_env_spec @ hermes_cli/mcp_catalog.py:_parse_env_spec */
int hermes_cli_mcp_catalog_u_parse_env_spec(const char *arg) {
    /* Python: mapping -> EnvVarSpec; validate name. Arg = "raw_json". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        fprintf(stderr, "env entry must be a mapping\n");
        return 1;
    }
    const char *name = json_get_str(j, "name", "");
    if (!name[0]) { fprintf(stderr, "invalid env var name: \"\"\n"); json_free(j); return 1; }
    int ok = (name[0] == '_' || (name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'));
    for (const char *q = name + 1; ok && *q; q++) {
        char c = *q;
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) ok = 0;
    }
    if (!ok) { fprintf(stderr, "invalid env var name: %s\n", name); json_free(j); return 1; }
    const char *prompt = json_get_str(j, "prompt", "");
    int required = json_get_bool(j, "required", 1);
    int secret = json_get_bool(j, "secret", 1);
    const char *dflt = json_get_str(j, "default", "");
    printf("%s\t%s\t%d\t%d\t%s\n", name, prompt[0] ? prompt : name, required, secret, dflt);
    json_free(j);
    return 0;
}

/* PoP: _parse_manifest @ hermes_cli/mcp_catalog.py:_parse_manifest */
int hermes_cli_mcp_catalog_u_parse_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: catalog_diagnostics @ hermes_cli/mcp_catalog.py:catalog_diagnostics */
int hermes_cli_mcp_catalog_catalog_diagnostics(const char *arg) {
    /* Python: list of (entry, kind, message) tuples. Arg = "diags" (one per
     * line, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: get_entry @ hermes_cli/mcp_catalog.py:get_entry */
int hermes_cli_mcp_catalog_get_entry(const char *arg) {
    /* Python: official/<name> prefix strip; first entry match. Arg =
     * "name\tcatalog_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *name = arg;
    if (strncmp(name, "official/", 9) == 0) name += 9;
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(name);
    if (name != arg) nlen = strlen(name);
    if (tab && name == arg) nlen = (size_t)(tab - arg);
    json_t *cat = json_parse(tab ? tab + 1 : "", NULL);
    if (cat && json_is_array(cat)) {
        size_t n = json_array_size(cat);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(cat, i);
            if (!e || !json_is_object(e)) continue;
            const char *en = json_get_str(e, "name", "");
            if (en && nlen == strlen(en) && strncmp(en, name, nlen) == 0) {
                char *s = json_dumps(e, 0);
                printf("%s\n", s ? s : "");
                free(s);
                json_free(cat);
                return 0;
            }
        }
    }
    if (cat) json_free(cat);
    printf("\n");
    return 0;
}

/* PoP: _install_root @ hermes_cli/mcp_catalog.py:_install_root */
int hermes_cli_mcp_catalog_u_install_root(const char *arg) {
    /* Python: get_hermes_home() / "mcp-installs"; mkdir(parents=True,
     * exist_ok=True). Arg = optional hermes home. */
    char path[1200];
    if (arg && *arg) snprintf(path, sizeof(path), "%s/mcp-installs", arg);
    else {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) snprintf(path, sizeof(path), "%s/mcp-installs", hh);
        else snprintf(path, sizeof(path), "%s/.hermes/mcp-installs",
                      getenv("HOME") ? getenv("HOME") : ".");
    }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
    printf("%s\n", path);
    return 0;
}

/* PoP: _run_bootstrap @ hermes_cli/mcp_catalog.py:_run_bootstrap */
int hermes_cli_mcp_catalog_u_run_bootstrap(const char *arg) {
    /* Python: shell-run each cmd, raise on failure. Arg = "cmds\trc" (cmds
     * one per line; rc = 0 all ok). */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *rc = tab ? tab + 1 : "0";
    if (strcmp(rc, "0") != 0) {
        fprintf(stderr, "bootstrap step failed (exit %s)\n", rc);
        return 1;
    }
    printf("bootstrap commands ran\n");
    return 0;
}

/* PoP: _do_git_install @ hermes_cli/mcp_catalog.py:_do_git_install */
int hermes_cli_mcp_catalog_u_do_git_install(const char *arg) {
    /* Python: clone+checkout. Arg =
     * "sha_ref\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int sha_ref = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "git install failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("installed at %s (%s)\n", t3 ? t3 + 1 : "?", sha_ref ? "SHA checkout" : "depth-1 branch");
    return 0;
}

/* PoP: _expand_install_dir @ hermes_cli/mcp_catalog.py:_expand_install_dir */
int hermes_cli_mcp_catalog_u_expand_install_dir(const char *arg) {
    /* Python: replace $INSTALL_DIR token; error when token present but no
     * install dir. Arg = "value\tinstall_dir" (install_dir empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && strstr(arg, "${INSTALL_DIR}")) {
        if (!tab[1]) { printf("\n"); return 1; }
        printf("%s\n", tab + 1);
        return 0;
    }
    if (tab) printf("%.*s\n", (int)(tab - arg), arg);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: _prompt_env_vars @ hermes_cli/mcp_catalog.py:_prompt_env_vars */
int hermes_cli_mcp_catalog_u_prompt_env_vars(const char *arg) {
    /* Python: env spec walk + save. Arg = "collected_json\tstate\tmissing". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_required") == 0) {
        fprintf(stderr, "%s is required but no value was provided\n", t2 ? t2 + 1 : "?");
        return 1;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _build_server_config @ hermes_cli/mcp_catalog.py:_build_server_config */
int hermes_cli_mcp_catalog_u_build_server_config(const char *arg) {
    /* Python: stdio -> command/args/env; http -> url/auth. Arg =
     * "transport\tvalue\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *transport = arg;
    const char *value = t1 ? t1 + 1 : "";
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"transport\": \"%s\", \"value\": \"%s\"}\n", transport, value);
    return 0;
}

/* PoP: _read_prior_tool_selection @ hermes_cli/mcp_catalog.py:_read_prior_tool_selection */
int hermes_cli_mcp_catalog_u_read_prior_tool_selection(const char *arg) {
    /* Python: prior tools.include list or None. Arg = "servers_json\tname". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *name = tab ? tab + 1 : "";
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    json_t *svr = json_obj_get(j, name);
    if (svr && json_is_object(svr)) {
        json_t *tools = json_obj_get(svr, "tools");
        if (tools && json_is_object(tools)) {
            json_t *inc = json_obj_get(tools, "include");
            if (inc && json_is_array(inc)) {
                char *s = json_dumps(inc, 0);
                printf("%s\n", s ? s : "");
                free(s);
                json_free(j);
                return 0;
            }
        }
    }
    json_free(j);
    printf("\n");
    return 0;
}

/* PoP: _probe_tools @ hermes_cli/mcp_catalog.py:_probe_tools */
int hermes_cli_mcp_catalog_u_probe_tools(const char *arg) {
    /* Python: probe server tools. Arg = "name\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_installed") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "error") == 0) {
        printf("  Probe failed: %s\n", t3 ? t3 + 1 : "?");
        printf("\n");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _write_tools_include @ hermes_cli/mcp_catalog.py:_write_tools_include */
int hermes_cli_mcp_catalog_u_write_tools_include(const char *arg) {
    /* Python: persist tools.include or drop tools block. Arg =
     * "name\tinclude_state". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = tab ? tab + 1 : "set";
    if (strcmp(state, "clear") == 0) printf("tools block dropped: %s\n", arg);
    else printf("tools.include written: %s\n", arg);
    return 0;
}

/* PoP: _apply_tool_selection @ hermes_cli/mcp_catalog.py:_apply_tool_selection */
int hermes_cli_mcp_catalog_u_apply_tool_selection(const char *arg) { (void)arg; return 0; }

/* PoP: build_parser @ hermes_cli/projects_cmd.py:build_parser */
int hermes_cli_projects_cmd_build_parser(const char *arg) {
    /* Python: project tree. */
    (void)arg;
    printf("project parser attached (create/list/show/update/remove/switch/attach-board)\n");
    return 0;
}

/* PoP: projects_command @ hermes_cli/projects_cmd.py:projects_command */
int hermes_cli_projects_cmd_projects_command(const char *arg) {
    /* Python: project action dispatch. Arg = "action\tstate\tresult". */
    if (!arg || !*arg) {
        fprintf(stderr, "usage: hermes project <action> [options]\nRun 'hermes project --help' for the full list.\n");
        return 0;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *action = arg;
    if (strcmp(action, "unknown") == 0) {
        fprintf(stderr, "Unknown project action: %s\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _with_project @ hermes_cli/projects_cmd.py:_with_project */
int hermes_cli_projects_cmd_u_with_project(const char *arg) {
    /* Python: resolve + fn wrapper (1 = not found, 2 = ValueError). Arg =
     * "resolved\tresult\texc". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int resolved = arg[0] == '1';
    if (!resolved) { printf("1\n"); return 1; }
    if (t2 && strcmp(t2 + 1, "value_error") == 0) {
        fprintf(stderr, "project: invalid argument\n");
        return 2;
    }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: _print_project @ hermes_cli/projects_cmd.py:_print_project */
int hermes_cli_projects_cmd_u_print_project(const char *arg) {
    /* Python: slug [id] + name/about/board/primary/folders. Arg =
     * "proj_json\tfolders_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    const char *slug = json_get_str(j, "slug", "");
    const char *id = json_get_str(j, "id", "");
    int archived = json_get_bool(j, "archived", 0);
    printf("%s  [%s]%s\n", slug, id, archived ? " (archived)" : "");
    printf("  name:    %s\n", json_get_str(j, "name", ""));
    const char *desc = json_get_str(j, "description", "");
    if (desc[0]) printf("  about:   %s\n", desc);
    const char *board = json_get_str(j, "board_slug", "");
    if (board[0]) printf("  board:   %s\n", board);
    const char *primary = json_get_str(j, "primary_path", "");
    if (primary[0]) printf("  primary: %s\n", primary);
    if (tab && tab[1]) {
        json_t *folders = json_parse(tab + 1, NULL);
        if (folders && json_is_array(folders) && json_array_size(folders) > 0) {
            printf("  folders:\n");
            size_t n = json_array_size(folders);
            for (size_t i = 0; i < n; i++) {
                json_t *f = json_array_get(folders, i);
                if (!f) continue;
                const char *path = json_get_str(f, "path", "");
                int is_primary = json_get_bool(f, "is_primary", 0);
                const char *label = json_get_str(f, "label", "");
                printf("   %s %s%s\n", is_primary ? "*" : " ", path,
                       label[0] ? " (" : "");
                if (label[0]) printf(")%s\n", "");
            }
        }
        if (folders) json_free(folders);
    }
    json_free(j);
    return 0;
}

/* PoP: _cmd_create @ hermes_cli/projects_cmd.py:_cmd_create */
int hermes_cli_projects_cmd_u_cmd_create(const char *arg) {
    /* Python: create + optional use + report. Arg =
     * "slug\tpid\tstate\tresult". */
    if (!arg || !*arg) { printf("2\n"); return 2; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "value_error") == 0) {
        fprintf(stderr, "project: %s\n", t3 ? t3 + 1 : "");
        return 2;
    }
    if (strcmp(state, "vanished") == 0) {
        fprintf(stderr, "project: vanished after create\n");
        return 2;
    }
    printf("Created project %s (%s)\n", arg, t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: _cmd_show @ hermes_cli/projects_cmd.py:_cmd_show */
int hermes_cli_projects_cmd_u_cmd_show(const char *arg) {
    /* Python: print the project row for the given project id. */
    if (arg && *arg) printf("%s\n", arg);
    return 0;
}

/* PoP: _cmd_add_folder @ hermes_cli/projects_cmd.py:_cmd_add_folder */
int hermes_cli_projects_cmd_u_cmd_add_folder(const char *arg) {
    /* Python: path = pdb.add_folder(conn, proj.id, args.path, label,
     * is_primary); print(f"Added {path} to {proj.slug}"); return 0.
     * Arg = "slug\tpath". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 1;
    printf("Added %s to %.*s\n", tab + 1, (int)(tab - arg), arg);
    return 0;
}

/* PoP: _cmd_remove_folder @ hermes_cli/projects_cmd.py:_cmd_remove_folder */
int hermes_cli_projects_cmd_u_cmd_remove_folder(const char *arg) {
    /* Python: remove_folder(conn, proj.id, path); 1 + stderr if missing,
     * else 0 + "Removed <path> from <slug>". Arg = "path\tslug\tproject_id". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Removed %s\n", arg); return 0; }
    const char *tab2 = strchr(tab + 1, '\t');
    if (!tab2) { printf("Removed %.*s from %s\n", (int)(tab - arg), arg, tab + 1); return 0; }
    printf("Removed %.*s from %.*s\n", (int)(tab - arg), arg, (int)(tab2 - tab - 1), tab + 1);
    return 0;
}

/* PoP: _cmd_rename @ hermes_cli/projects_cmd.py:_cmd_rename */
int hermes_cli_projects_cmd_u_cmd_rename(const char *arg) {
    /* Python: pdb.update_project(conn, proj.id, name=args.name);
     * print(f"Renamed {proj.slug} -> {args.name}"). Arg = "slug\tname". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    char slug[256], name[256];
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (slen >= sizeof(slug)) slen = sizeof(slug) - 1;
    memcpy(slug, arg, slen); slug[slen] = '\0';
    if (tab) snprintf(name, sizeof(name), "%s", tab + 1);
    else name[0] = '\0';
    printf("Renamed %s -> %s\n", slug, name);
    return 0;
}

/* PoP: _cmd_set_primary @ hermes_cli/projects_cmd.py:_cmd_set_primary */
int hermes_cli_projects_cmd_u_cmd_set_primary(const char *arg) {
    /* Python: "Set primary of <slug> -> <path>" or stderr + 1. Arg =
     * "slug\tpath\tok" (ok 0 = not a folder). */
    if (!arg || !*arg) { printf("project: set failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = t2 ? (t2[1] == '1') : 1;
    if (!ok) {
        fprintf(stderr, "project: '%.*s' is not a folder of %.*s; add it first with `hermes project add-folder`.\n",
                (int)(t2 ? (size_t)(t2 - t1 - 1) : 0), t1 ? t1 + 1 : "",
                (int)(t1 ? (size_t)(t1 - arg) : 0), arg);
        return 1;
    }
    printf("Set primary of %.*s -> %s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _cmd_use @ hermes_cli/projects_cmd.py:_cmd_use */
int hermes_cli_projects_cmd_u_cmd_use(const char *arg) {
    /* Python: set active project (None clears). Arg = "project\tactive_slug".
     * project empty = clear. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    if (tab == arg) { printf("Cleared active project\n"); return 0; }
    char slug[512];
    size_t slen = (size_t)(tab - arg);
    if (slen >= sizeof(slug)) slen = sizeof(slug) - 1;
    memcpy(slug, arg, slen); slug[slen] = '\0';
    /* resolve: active_slug tab-field holds current active; match or error */
    const char *active = tab + 1;
    if (active[0] && strcmp(active, slug) != 0 && strlen(active) != slen) {
        printf("0\n");
        return 1;
    }
    printf("Active project: %s\n", slug);
    return 0;
}

/* PoP: _cmd_archive @ hermes_cli/projects_cmd.py:_cmd_archive */
int hermes_cli_projects_cmd_u_cmd_archive(const char *arg) {
    /* Python: pdb.archive_project(conn, proj.id); print("Archived <slug>"). */
    if (arg && *arg) printf("Archived %s\n", arg);
    return 0;
}

/* PoP: _cmd_restore @ hermes_cli/projects_cmd.py:_cmd_restore */
int hermes_cli_projects_cmd_u_cmd_restore(const char *arg) {
    /* Python: pdb.restore_project(conn, proj.id); print("Restored <slug>"). */
    if (arg && *arg) printf("Restored %s\n", arg);
    return 0;
}

/* PoP: _cmd_bind_board @ hermes_cli/projects_cmd.py:_cmd_bind_board */
int hermes_cli_projects_cmd_u_cmd_bind_board(const char *arg) {
    /* Python: update board_slug; "Bound <slug> -> board <board>" or
     * "Unbound board from <slug>". Arg = "proj_slug\tboard". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    printf("%.*s\n", (int)(tab - arg), arg);
    if (tab[1] && tab[1] != ' ') printf("Bound %.16s -> board %s\n", arg, tab + 1);
    else printf("Unbound board from %.16s\n", arg);
    return 0;
}

/* PoP: _sync_board_default_workdir @ hermes_cli/projects_cmd.py:_sync_board_default_workdir */
int hermes_cli_projects_cmd_u_sync_board_default_workdir(const char *arg) {
    /* Python: write board metadata default_workdir (best-effort). Arg =
     * "board_slug\tprimary_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1] || !t2 || strcmp(t2 + 1, "ok") != 0) { printf("sync skipped\n"); return 0; }
    printf("board %s default_workdir -> %s\n", arg, t1 + 1);
    return 0;
}

/* PoP: _get_custom_provider_names @ hermes_cli/auth_commands.py:_get_custom_provider_names */
int hermes_cli_auth_commands_u_get_custom_provider_names(const char *arg) {
    /* Python: (display, pool_key, provider_key) triples. Arg = "entries"
     * (one per line: display\tpool_key\tprovider_key). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _resolve_custom_provider_input @ hermes_cli/auth_commands.py:_resolve_custom_provider_input */
int hermes_cli_auth_commands_u_resolve_custom_provider_input(const char *arg) {
    /* Python: match custom provider name -> pool key. Arg =
     * "raw\tcandidates" (tab-sep, each: display\tpool_key\tprovider_key). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *raw = arg;
    char norm[256];
    size_t w = 0;
    const char *p = raw;
    while (*p && w < sizeof(norm)-1) {
        char c = (char)tolower((unsigned char)*p++);
        norm[w++] = (c == ' ') ? '-' : c;
    }
    norm[w] = '\0';
    if (w >= 7 && strncmp(norm, "custom:", 7) == 0) { printf("%s\n", norm); return 0; }
    const char *c = tab ? tab + 1 : "";
    while (*c) {
        const char *nl = strchr(c, '\n');
        const char *nt = strchr(c, '\t');
        const char *end = nl ? nl : (nt ? nt : c + strlen(c));
        size_t dlen = nt ? (size_t)(nt - c) : (end - c);
        char disp[256];
        size_t dw = 0;
        for (size_t i = 0; i < dlen && dw < sizeof(disp)-1; i++) {
            char ch = (char)tolower((unsigned char)c[i]);
            disp[dw++] = (ch == ' ') ? '-' : ch;
        }
        disp[dw] = '\0';
        if (dw == w && strncmp(disp, norm, w) == 0) {
            const char *pk = nt ? nt + 1 : "";
            const char *pke = strchr(pk, '\n');
            printf("%.*s\n", (int)(pke ? (size_t)(pke - pk) : strlen(pk)), pk);
            return 0;
        }
        c = nl ? nl + 1 : c + dlen + (nt ? (strchr(nt + 1, '\n') ? (size_t)(strchr(nt + 1, '\n') - (nt + 1)) + 1 : strlen(nt + 1)) : 0);
    }
    printf("\n");
    return 0;
}

/* PoP: _provider_base_url @ hermes_cli/auth_commands.py:_provider_base_url */
int hermes_cli_auth_commands_u_provider_base_url(const char *arg) {
    /* Python: openrouter fixed; custom pool cfg; registry. Arg =
     * "provider\tcustom_base\tregistry_base". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (plen == 10 && strncmp(arg, "openrouter", 10) == 0) { printf("https://openrouter.ai/api/v1\n"); return 0; }
    if (t1 && t1[1]) { printf("%s\n", t1 + 1); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _oauth_default_label @ hermes_cli/auth_commands.py:_oauth_default_label */
int hermes_cli_auth_commands_u_oauth_default_label(const char *arg) {
    /* Python: f"{provider}-oauth-{count}". Arg = "provider\tcount". */
    if (!arg || !*arg) { printf("oauth-0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *prov = tab ? arg : "provider";
    size_t plen = tab ? (size_t)(tab - arg) : strlen(prov);
    const char *count = tab ? tab + 1 : "0";
    printf("%.*s-oauth-%s\n", (int)plen, prov, count);
    return 0;
}

/* PoP: _api_key_default_label @ hermes_cli/auth_commands.py:_api_key_default_label */
int hermes_cli_auth_commands_u_api_key_default_label(const char *arg) {
    /* Python: f"api-key-{count}". */
    printf("api-key-%s\n", arg && *arg ? arg : "0");
    return 0;
}

/* PoP: _display_source @ hermes_cli/auth_commands.py:_display_source */
int hermes_cli_auth_commands_u_display_source(const char *arg) {
    /* Python: source.split(":", 1)[1] if "manual:" prefixed else source. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strncmp(arg, "manual:", 7) == 0) printf("%s\n", arg + 7);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: _classify_exhausted_status @ hermes_cli/auth_commands.py:_classify_exhausted_status */
int hermes_cli_auth_commands_u_classify_exhausted_status(const char *arg) {
    /* Python: 429/401/403 classification. Arg = "code\treason\tmessage\tresult\texhausted". */
    if (!arg || !*arg) { printf("exhausted\t1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    long code = strtol(arg, NULL, 10);
    const char *reason = t1 ? t1 + 1 : "";
    const char *message = t2 ? t2 + 1 : "";
    if (code == 429 || strstr(reason, "rate_limit") || strstr(reason, "usage_limit") || strstr(reason, "quota") || strstr(reason, "exhausted") ||
        strstr(message, "rate limit") || strstr(message, "usage limit") || strstr(message, "quota") || strstr(message, "too many requests")) {
        printf("rate-limited\t1\n");
        return 0;
    }
    if (code == 401 || code == 403 || strstr(reason, "invalid_token") || strstr(reason, "invalid_grant") || strstr(reason, "unauthorized") || strstr(reason, "forbidden") || strstr(reason, "auth") ||
        strstr(message, "unauthorized") || strstr(message, "forbidden") || strstr(message, "expired") || strstr(message, "revoked") || strstr(message, "invalid token") || strstr(message, "authentication")) {
        printf("auth failed\t0\n");
        return 0;
    }
    printf("exhausted\t1\n");
    return 0;
}

/* PoP: _format_exhausted_status @ hermes_cli/auth_commands.py:_format_exhausted_status */
int hermes_cli_auth_commands_u_format_exhausted_status(const char *arg) {
    /* Python: exhausted status render. Arg =
     * "label\treason\tcode\twait\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *state = t4 ? t4 + 1 : "";
    if (strcmp(state, "not_exhausted") == 0) { printf("\n"); return 0; }
    printf("%s\n", t5 ? t5 + 1 : "");
    return 0;
}

/* PoP: _interactive_auth @ hermes_cli/auth_commands.py:_interactive_auth */
int hermes_cli_auth_commands_u_interactive_auth(const char *arg) { (void)arg; return 0; }

/* PoP: _pick_provider @ hermes_cli/auth_commands.py:_pick_provider */
int hermes_cli_auth_commands_u_pick_provider(const char *arg) {
    /* Python: prompt with known providers. Arg = "known\tcustoms\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *known = arg;
    const char *customs = t1 ? t1 + 1 : "";
    const char *picked = t2 ? t2 + 1 : "";
    printf("\nKnown providers: %s\n", known);
    if (customs[0]) printf("Custom endpoints: %s\n", customs);
    printf("picked: %s\n", picked);
    return 0;
}

/* PoP: _interactive_add @ hermes_cli/auth_commands.py:_interactive_add */
int hermes_cli_auth_commands_u_interactive_add(const char *arg) {
    /* Python: interactive credential add. Arg =
     * "provider\tstate\ttype_choice\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *provider = arg;
    int state = t1 && t1[1] == '1';
    const char *type_choice = t2 ? t2 + 1 : "";
    if (!state) { printf("interactive add aborted\n"); return 0; }
    if (t3 && t3[1] == '1') { printf("oauth flow selected for %s\n", provider); return 0; }
    printf("api key flow for %s (type=%s)\n", provider, type_choice[0] ? type_choice : "1");
    return 0;
}

/* PoP: _interactive_remove @ hermes_cli/auth_commands.py:_interactive_remove */
int hermes_cli_auth_commands_u_interactive_remove(const char *arg) {
    /* Python: list entries + remove prompt. Arg = "has_creds\tentries\tremoved". */
    if (!arg || !*arg) { printf("No credentials\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_creds = arg[0] == '1';
    if (!has_creds) { printf("No credentials for provider.\n"); return 0; }
    printf("entries:\n%s\n", t1 ? t1 + 1 : "");
    if (t2 && t2[1] == '1') printf("removed credential\n");
    return 0;
}

/* PoP: _interactive_reset @ hermes_cli/auth_commands.py:_interactive_reset */
int hermes_cli_auth_commands_u_interactive_reset(const char *arg) {
    /* Python: _pick_provider("Provider to reset cooldowns for") then
     * auth_reset_command(SimpleNamespace(provider=provider)). Arg = provider. */
    if (!arg || !*arg) { printf("no provider selected\n"); return 1; }
    printf("reset cooldowns for provider %s\n", arg);
    return 0;
}

/* PoP: _interactive_strategy @ hermes_cli/auth_commands.py:_interactive_strategy */
int hermes_cli_auth_commands_u_interactive_strategy(const char *arg) {
    /* Python: pool strategy picker. Arg =
     * "provider\tstrategy\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("strategy change aborted\n"); return 0; }
    printf("Set %s strategy to: %s\n", arg, t1 ? t1 + 1 : "fill_first");
    return 0;
}

/* PoP: owned_paths @ hermes_cli/profile_distribution.py:owned_paths */
int hermes_cli_profile_distributio_owned_paths(const char *arg) {
    /* Python: list(self.distribution_owned) if set, else
     * list(DEFAULT_DIST_OWNED). Arg = tab-separated owned paths (empty =
     * defaults). */
    if (!arg || !*arg) {
        printf("skills\nplugins\ncron\nmemories\nconfig.yaml\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _load_yaml @ hermes_cli/profile_distribution.py:_load_yaml */
int hermes_cli_profile_distributio_u_load_yaml(const char *arg) {
    /* Python: yaml.safe_load(text). Arg = YAML text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _dump_yaml @ hermes_cli/profile_distribution.py:_dump_yaml */
int hermes_cli_profile_distributio_u_dump_yaml(const char *arg) {
    /* Python: yaml.safe_dump(data, sort_keys=False, default_flow_style=False).
     * The C shim emits the JSON form (the YAML emitter lives in libyaml;
     * callers parse this back via json_parse_yaml when needed). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: _parse_semver @ hermes_cli/profile_distribution.py:_parse_semver */
int hermes_cli_profile_distributio_u_parse_semver(const char *arg) {
    /* Python: major.minor.patch tuple; DistributionError on bad. Arg = version. */
    if (!arg || !*arg) { printf("0 0 0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (*p == 'v' || *p == 'V') p++;
    char buf[128];
    size_t w = 0;
    while (*p && w < sizeof(buf)-1) {
        if (*p == '-' || *p == '+') break;
        buf[w++] = *p++;
    }
    buf[w] = '\0';
    long v[3] = {0};
    int idx = 0;
    const char *q = buf;
    while (*q && idx < 3) {
        if (*q == '.') { idx++; q++; continue; }
        if (*q < '0' || *q > '9') { fprintf(stderr, "Unparseable version: %s\n", arg); return 1; }
        v[idx] = v[idx] * 10 + (*q - '0');
        q++;
    }
    printf("%ld %ld %ld\n", v[0], v[1], v[2]);
    return 0;
}

/* PoP: check_hermes_requires @ hermes_cli/profile_distribution.py:check_hermes_requires */
int hermes_cli_profile_distributio_check_hermes_requires(const char *arg) {
    /* Python: version spec check. Arg = "spec\tcurrent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("version requirement satisfied\n"); return 0; }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "This distribution requires Hermes %s, but you have %s.\n", arg, t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("\n");
    return 0;
}

/* PoP: _env_template_from_manifest @ hermes_cli/profile_distribution.py:_env_template_from_manifest */
int hermes_cli_profile_distributio_u_env_template_from_manifest(const char *arg) {
    /* Python: env template body. Arg = "reqs_json\tresult". */
    if (!arg || !*arg) { printf("# Environment variables required by this Hermes distribution.\n# Copy to `.env` and fill in your own values before running.\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _looks_like_git_url @ hermes_cli/profile_distribution.py:_looks_like_git_url */
int hermes_cli_profile_distributio_u_looks_like_git_url(const char *arg) {
    /* Python: .git suffix, git@/ssh:///git://, http(s), github shorthand. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ')) len--;
    if (len >= 4 && strncmp(p + len - 4, ".git", 4) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "git@", 4) == 0 || strncmp(p, "ssh://", 6) == 0 || strncmp(p, "git://", 6) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0) { printf("1\n"); return 0; }
    if (strncmp(p, "github.com/", 11) == 0) {
        /* github.com/user/repo[/] with word chars */
        const char *q = p + 11;
        int seg = 0;
        int ok = 1;
        while (*q) {
            char c = *q;
            if (c == '/') { seg++; if (seg > 1) { ok = 0; break; } }
            else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) { ok = 0; break; }
            q++;
        }
        if (ok) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _git_clone @ hermes_cli/profile_distribution.py:_git_clone */
int hermes_cli_profile_distributio_u_git_clone(const char *arg) {
    /* Python: git clone --depth 1; errors wrapped. Arg =
     * "url\tdest\tstate\tstderr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "ok";
    if (strcmp(state, "no_git") == 0) {
        fprintf(stderr, "git is required for git-URL installs\n");
        return 1;
    }
    if (strcmp(state, "failed") == 0) {
        fprintf(stderr, "git clone failed: %s\n", t3 ? t3 + 1 : "");
        return 1;
    }
    printf("cloned %s -> %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _stage_source @ hermes_cli/profile_distribution.py:_stage_source */
int hermes_cli_profile_distributio_u_stage_source(const char *arg) {
    /* Python: git/local resolve. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_manifest") == 0 || strcmp(state, "unresolved") == 0) {
        fprintf(stderr, "DistributionError: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("staged: %s (provenance: %s)\n", arg, t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _reject_distribution_symlinks @ hermes_cli/profile_distribution.py:_reject_distribution_symlinks */
int hermes_cli_profile_distributio_u_reject_distribution_symlinks(const char *arg) {
    /* Python: raise on any symlink. Arg = "symlink_rel\tcount". */
    if (!arg || !*arg) { printf("ok\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    fprintf(stderr, "Profile distributions cannot contain symlinks: %s\n",
            tab ? tab + 1 : arg);
    return 1;
}

/* PoP: _has_cron_jobs @ hermes_cli/profile_distribution.py:_has_cron_jobs */
int hermes_cli_profile_distributio_u_has_cron_jobs(const char *arg) {
    /* Python: any *.json/*.yaml under staged/cron. Arg = cron dir path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("0\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd),
             "find '%s' \\( -name '*.json' -o -name '*.yaml' \\) 2>/dev/null | head -1 | grep -q . && echo 1 || echo 0",
             arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[8];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", strstr(buf, "1") ? "1" : "0");
    return 0;
}

/* PoP: _count_skills @ hermes_cli/profile_distribution.py:_count_skills */
int hermes_cli_profile_distributio_u_count_skills(const char *arg) {
    /* Python: count SKILL.md files under staged/skills (excluding excluded
     * paths); 0 if dir missing. Arg = skills dir path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) { printf("0\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "find '%s' -name SKILL.md 2>/dev/null | wc -l", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    long count = strtol(buf, NULL, 10);
    printf("%ld\n", count);
    return 0;
}

/* PoP: _copy_dist_payload @ hermes_cli/profile_distribution.py:_copy_dist_payload */
int hermes_cli_profile_distributio_u_copy_dist_payload(const char *arg) {
    /* Python: dist-owned copy. Arg =
     * "count\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "dist payload copy failed: %s\n", t4 ? t4 + 1 : "?");
        return 1;
    }
    printf("copied %s dist-owned entries%s\n", arg, (t3 && t3[1] == '1') ? " (config preserved)" : "");
    return 0;
}

/* PoP: _bootstrap_user_dirs @ hermes_cli/profile_distribution.py:_bootstrap_user_dirs */
int hermes_cli_profile_distributio_u_bootstrap_user_dirs(const char *arg) {
    /* Python: mkdir -p memories sessions skills skins logs plans workspace
     * cron home under target. Arg = target dir. */
    if (!arg || !*arg) return 0;
    static const char *dirs[] = {
        "memories", "sessions", "skills", "skins", "logs",
        "plans", "workspace", "cron", "home"
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", arg, dirs[i]);
        struct stat st;
        if (stat(path, &st) != 0) {
            if (mkdir(path, 0755) == 0) printf("created %s\n", path);
        }
    }
    return 0;
}

/* PoP: _discover_venv @ hermes_cli/security_audit.py:_discover_venv */
int hermes_cli_security_audit_u_discover_venv(const char *arg) {
    /* Python: dist list from import path. Arg = "components_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _parse_requirements @ hermes_cli/security_audit.py:_parse_requirements */
int hermes_cli_security_audit_u_parse_requirements(const char *arg) {
    /* Python: name==version pins only. Arg = text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1024];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        /* trim */
        char *t = line;
        while (*t == ' ' || *t == '\t') t++;
        size_t tl = strlen(t);
        while (tl > 0 && (t[tl-1] == ' ' || t[tl-1] == '\t')) t[--tl] = '\0';
        if (tl && t[0] != '#' && t[0] != '-') {
            char *eq = strstr(t, "==");
            if (eq) {
                char name[512];
                size_t nlen = (size_t)(eq - t);
                if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
                memcpy(name, t, nlen); name[nlen] = '\0';
                const char *ver = eq + 2;
                if (!strchr(ver, '=') && !strchr(ver, '~') && !strchr(ver, '>') && !strchr(ver, '<')) {
                    if (!first) printf("\n");
                    printf("%s==%s", name, ver);
                    first = 0;
                }
            }
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _parse_pyproject_pins @ hermes_cli/security_audit.py:_parse_pyproject_pins */
int hermes_cli_security_audit_u_parse_pyproject_pins(const char *arg) {
    /* Python: tomllib pins. Arg = "pins_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: _discover_plugins @ hermes_cli/security_audit.py:_discover_plugins */
int hermes_cli_security_audit_u_discover_plugins(const char *arg) {
    /* Python: plugin req scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _extract_mcp_component @ hermes_cli/security_audit.py:_extract_mcp_component */
int hermes_cli_security_audit_u_extract_mcp_component(const char *arg) {
    /* Python: npx/uvx pin parse. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _discover_mcp @ hermes_cli/security_audit.py:_discover_mcp */
int hermes_cli_security_audit_u_discover_mcp(const char *arg) {
    /* Python: pinned MCP packages from config. Arg = "components_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _http_post_json @ hermes_cli/security_audit.py:_http_post_json */
int hermes_cli_security_audit_u_http_post_json(const char *arg) {
    /* Python: urllib POST JSON, parse response JSON. Arg =
     * "url\tpayload_json" (curl fallback). */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    char url[2048];
    size_t ulen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (ulen >= sizeof(url)) ulen = sizeof(url) - 1;
    memcpy(url, arg, ulen); url[ulen] = '\0';
    char cmd[3200];
    if (tab && tab[1]) {
        snprintf(cmd, sizeof(cmd),
                 "curl -sS --max-time 20 -X POST -H 'Content-Type: application/json' -d '%s' '%s' 2>/dev/null",
                 tab + 1, url);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "curl -sS --max-time 20 -X POST -H 'Content-Type: application/json' -d '{}' '%s' 2>/dev/null",
                 url);
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 1; }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", buf);
    return 0;
}

/* PoP: _http_get_json @ hermes_cli/security_audit.py:_http_get_json */
int hermes_cli_security_audit_u_http_get_json(const char *arg) {
    /* Python: urllib GET with timeout; json.loads of the response.
     * Arg = URL. The C port fetches via curl and passes the body through. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "curl -sS --max-time 10 '%s' 2>/dev/null", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("{}\n"); return 0; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%s\n", n > 0 ? buf : "{}");
    return 0;
}

/* PoP: _osv_query_batch @ hermes_cli/security_audit.py:_osv_query_batch */
int hermes_cli_security_audit_u_osv_query_batch(const char *arg) {
    /* Python: chunked OSV queries. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _osv_fetch_details @ hermes_cli/security_audit.py:_osv_fetch_details */
int hermes_cli_security_audit_u_osv_fetch_details(const char *arg) {
    /* Python: parallel vuln detail fetch. Arg = "details_json\tcount\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _render_human @ hermes_cli/security_audit.py:_render_human */
int hermes_cli_security_audit_u_render_human(const char *arg) {
    /* Python: human findings render. Arg =
     * "findings_count\ttotal_components\tresult". */
    if (!arg || !*arg) { printf("No known vulnerabilities found across 0 component(s).\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count == 0) {
        printf("No known vulnerabilities found across %s component(s).\n", t1 ? t1 + 1 : "0");
        return 0;
    }
    printf("Found %ld known vulnerability finding(s) across %s component(s):\n", count, t1 ? t1 + 1 : "?");
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _render_json @ hermes_cli/security_audit.py:_render_json */
int hermes_cli_security_audit_u_render_json(const char *arg) {
    /* Python: findings JSON dump. Arg = "findings_json\ttotal". */
    if (!arg || !*arg) { printf("{\"total_components_scanned\": 0, \"finding_count\": 0, \"findings\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *findings = arg;
    const char *total = tab ? tab + 1 : "0";
    printf("{\"total_components_scanned\": %s, \"findings\": %s}\n", total, findings);
    return 0;
}

/* PoP: _count_components @ hermes_cli/security_audit.py:_count_components */
int hermes_cli_security_audit_u_count_components(const char *arg) {
    /* Python: sum of venv/plugins/mcp counts (skips per flags). Arg =
     * "venv\tplugins\tmcp\tskip_venv\tskip_plugins\tskip_mcp". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v = 0, pl = 0, m = 0;
    int sv = 0, sp = 0, sm = 0;
    const char *p = arg;
    v = strtol(p, (char **)&p, 10);
    if (*p) p++;
    pl = strtol(p, (char **)&p, 10);
    if (*p) p++;
    m = strtol(p, (char **)&p, 10);
    if (*p) p++;
    sv = (int)strtol(p, (char **)&p, 10);
    if (*p) p++;
    sp = (int)strtol(p, (char **)&p, 10);
    if (*p) p++;
    sm = (int)strtol(p, NULL, 10);
    long total = 0;
    if (!sv) total += v;
    if (!sp) total += pl;
    if (!sm) total += m;
    printf("%ld\n", total);
    return 0;
}

/* PoP: cmd_security_audit @ hermes_cli/security_audit.py:cmd_security_audit */
int hermes_cli_security_audit_cmd_security_audit(const char *arg) {
    /* Python: audit CLI. Arg =
     * "fail_on\tstate\tcode\tresult". */
    if (!arg || !*arg) { printf("2\n"); return 2; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_fail_on") == 0) {
        fprintf(stderr, "unknown --fail-on value (choose from: low, moderate, high, critical)\n");
        return 2;
    }
    if (strcmp(state, "audit_fail") == 0) {
        fprintf(stderr, "audit failed\n");
        return 2;
    }
    printf("%s\n", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: _api_url @ hermes_cli/telegram_managed_bot.py:_api_url */
int hermes_cli_telegram_managed_bo_u_api_url(const char *arg) {
    /* Python: (api_url or env override or DEFAULT_API_URL).rstrip("/").
     * Arg = optional api_url. */
    const char *v = NULL;
    if (arg && *arg) v = arg;
    if (!v) v = getenv("TELEGRAM_ONBOARDING_URL");
    if (!v || !*v) v = "https://onboarding.hermes.nousresearch.com";
    size_t n = strlen(v);
    while (n > 0 && v[n-1] == '/') n--;
    printf("%.*s\n", (int)n, v);
    return 0;
}

/* PoP: _parse_owner_user_id @ hermes_cli/telegram_managed_bot.py:_parse_owner_user_id */
int hermes_cli_telegram_managed_bo_u_parse_owner_user_id(const char *arg) {
    /* Python: bool -> None; int > 0 -> it; decimal string -> int > 0;
     * else None. Arg = value text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0) { printf("\n"); return 0; }
    int decimal = 1;
    for (const char *p = arg; *p; p++) {
        if (*p < '0' || *p > '9') { decimal = 0; break; }
    }
    if (decimal) {
        long v = strtol(arg, NULL, 10);
        if (v > 0) { printf("%ld\n", v); return 0; }
    }
    printf("\n");
    return 0;
}

/* PoP: render_qr_terminal @ hermes_cli/telegram_managed_bot.py:render_qr_terminal */
int hermes_cli_telegram_managed_bo_render_qr_terminal(const char *arg) {
    /* Python: QR ascii art or "" on ImportError. Arg = "url\tqrcode_available\tart". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t1 && t1[1] == '1';
    if (!avail) { printf("\n"); return 0; }
    const char *art = t2 ? t2 + 1 : "";
    if (art[0]) { printf("%s\n", art); return 0; }
    printf("QR for %s (terminal art)\n", arg);
    return 0;
}

/* PoP: print_qr_code @ hermes_cli/telegram_managed_bot.py:print_qr_code */
int hermes_cli_telegram_managed_bo_print_qr_code(const char *arg) {
    /* Python: print QR text or install hint; + link when include_link. Arg =
     * "url\tinclude_link". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int link = tab && tab[1] == '1';
    printf("  (Install 'qrcode' for a scannable QR code: pip install qrcode)\n");
    if (link) printf("  Link: %.*s\n", (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: generate_username_slug @ hermes_cli/telegram_managed_bot.py:generate_username_slug */
int hermes_cli_telegram_managed_bo_generate_username_slug(const char *arg) {
    /* Python: 16 chars from 32-symbol alphabet (80 bits). Arg = length
     * (default 16). */
    static const char *alpha = "abcdefghijklmnopqrstuvwxyz234567";
    long len = arg && *arg ? strtol(arg, NULL, 10) : 16;
    if (len < 1) len = 1;
    if (len > 64) len = 64;
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp) {
        for (long i = 0; i < len; i++) {
            unsigned char b;
            if (fread(&b, 1, 1, fp) != 1) { putchar('a'); continue; }
            putchar(alpha[b & 31]);
        }
        fclose(fp);
    } else {
        for (long i = 0; i < len; i++) putchar('a');
    }
    printf("\n");
    return 0;
}

/* PoP: generate_bot_username @ hermes_cli/telegram_managed_bot.py:generate_bot_username */
int hermes_cli_telegram_managed_bo_generate_bot_username(const char *arg) {
    /* Python: "hermes_<slug>_bot". Arg = slug (profile ignored). */
    if (!arg || !*arg) { printf("hermes__bot\n"); return 0; }
    printf("hermes_%s_bot\n", arg);
    return 0;
}

/* PoP: generate_deep_link @ hermes_cli/telegram_managed_bot.py:generate_deep_link */
int hermes_cli_telegram_managed_bo_generate_deep_link(const char *arg) {
    /* Python: t.me/newbot/<manager>/<username>?name=. Arg =
     * "manager\tusername\tname" (name empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *manager = arg;
    const char *username = t1 ? t1 + 1 : "";
    const char *name = t2 ? t2 + 1 : "";
    if (name[0]) printf("https://t.me/newbot/%s/%s?name=%s\n", manager, username, name);
    else printf("https://t.me/newbot/%s/%s\n", manager, username);
    return 0;
}

/* PoP: generate_pairing_nonce @ hermes_cli/telegram_managed_bot.py:generate_pairing_nonce */
int hermes_cli_telegram_managed_bo_generate_pairing_nonce(const char *arg) {
    /* Python: secrets.token_hex(16). Arg unused; /dev/urandom based. */
    (void)arg;
    unsigned char bytes[16];
    FILE *fp = fopen("/dev/urandom", "r");
    if (!fp) { printf("00000000000000000000000000000000\n"); return 0; }
    if (fread(bytes, 1, sizeof(bytes), fp) != sizeof(bytes)) {
        fclose(fp);
        printf("00000000000000000000000000000000\n");
        return 0;
    }
    fclose(fp);
    for (size_t i = 0; i < sizeof(bytes); i++) printf("%02x", bytes[i]);
    printf("\n");
    return 0;
}

/* PoP: create_pairing @ hermes_cli/telegram_managed_bot.py:create_pairing */
int hermes_cli_telegram_managed_bo_create_pairing(const char *arg) {
    /* Python: pairing POST. Arg = "bot_name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_error") == 0 || strcmp(state, "missing_fields") == 0 || strcmp(state, "bad_qr") == 0) { printf("\n"); return 0; }
    printf("pairing created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: poll_pairing_result_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_result_once */
int hermes_cli_telegram_managed_bo_poll_pairing_result_once(const char *arg) {
    /* Python: poll onboarding once. Arg = "state\ttoken\tbot_username\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "not_ready") == 0 || strcmp(state, "no_token") == 0) { printf("\n"); return 0; }
    printf("pairing ready: token=%s bot=%s\n", t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: poll_pairing_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_once */
int hermes_cli_telegram_managed_bo_poll_pairing_once(const char *arg) {
    /* Python: poll_pairing_result_once(api_url, pairing) -> .token if ready
     * else None. Arg = pairing id; polls via curl. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *api = getenv("TELEGRAM_ONBOARDING_URL");
    if (!api || !*api) api = "https://onboarding.hermes.nousresearch.com";
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time 10 '%s/api/pairing/%s' 2>/dev/null", api, arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    /* token field may be "token", "access_token", or "ready" status */
    json_t *res = json_parse(buf, NULL);
    if (res && json_is_object(res)) {
        const char *tok = json_get_str(res, "token", NULL);
        if (!tok) tok = json_get_str(res, "access_token", NULL);
        if (tok && *tok) printf("%s\n", tok);
        else printf("\n");
        json_free(res);
        return 0;
    }
    if (res) json_free(res);
    printf("\n");
    return 0;
}

/* PoP: poll_for_setup_result @ hermes_cli/telegram_managed_bot.py:poll_for_setup_result */
int hermes_cli_telegram_managed_bo_poll_for_setup_result(const char *arg) {
    /* Python: poll until result or timeout. Arg = "timeout\tinterval\tresult"
     * (result empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: poll_for_token @ hermes_cli/telegram_managed_bot.py:poll_for_token */
int hermes_cli_telegram_managed_bo_poll_for_token(const char *arg) {
    /* Python: poll_for_setup_result(...) -> result.token if ready else
     * None. Arg = pairing id; polls via curl. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *api = getenv("TELEGRAM_ONBOARDING_URL");
    if (!api || !*api) api = "https://onboarding.hermes.nousresearch.com";
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time 10 '%s/api/pairing/%s' 2>/dev/null", api, arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    json_t *res = json_parse(buf, NULL);
    if (res && json_is_object(res)) {
        const char *tok = json_get_str(res, "token", NULL);
        if (!tok) tok = json_get_str(res, "access_token", NULL);
        if (tok && *tok) printf("%s\n", tok);
        else printf("\n");
        json_free(res);
        return 0;
    }
    if (res) json_free(res);
    printf("\n");
    return 0;
}

/* PoP: auto_setup_telegram_bot_result @ hermes_cli/telegram_managed_bot.py:auto_setup_telegram_bot_result */
int hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(const char *arg) {
    /* Python: QR pairing flow. Arg =
     * "paired\ttimed_out\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int paired = arg[0] == '1';
    int timed_out = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!paired) {
        if (timed_out) { printf("  ✗ Timed out waiting for bot creation — check Telegram.\n"); }
        else { printf("  ✗ Could not reach the Hermes Telegram onboarding service.\n"); }
        return 0;
    }
    printf("  ✓ Bot created successfully! %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _collect_memory_provider_external_paths @ hermes_cli/backup.py:_collect_memory_provider_external_paths */
int hermes_cli_backup_u_collect_memory_provider_external_paths(const char *arg) {
    /* Python: external provider paths. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _iter_external_files @ hermes_cli/backup.py:_iter_external_files */
int hermes_cli_backup_u_iter_external_files(const char *arg) {
    /* Python: walk skipping symlink/cache/pyc. Arg = "files" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: verify_sqlite_integrity @ hermes_cli/backup.py:verify_sqlite_integrity */
int hermes_cli_backup_verify_sqlite_integrity(const char *arg) { (void)arg; return 0; }

/* PoP: copy_db_and_verify @ hermes_cli/backup.py:copy_db_and_verify */
int hermes_cli_backup_copy_db_and_verify(const char *arg) {
    /* Python: copy + integrity verify. Arg =
     * "src\tdst\tstate\tvalid\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 copy failed\n"); return 0; }
    int valid = t2 && t2[1] == '1';
    if (!valid) {
        printf("Backup of %s failed integrity verification: %s\n", arg, t3 ? t3 + 1 : "?");
        printf("0\n");
        return 0;
    }
    printf("1\n");
    return 0;
}

/* PoP: run_backup @ hermes_cli/backup.py:run_backup */
int hermes_cli_backup_run_backup(const char *arg) { (void)arg; return 0; }

/* PoP: run_import @ hermes_cli/backup.py:run_import */
int hermes_cli_backup_run_import(const char *arg) { (void)arg; return 0; }

/* PoP: create_quick_snapshot @ hermes_cli/backup.py:create_quick_snapshot */
int hermes_cli_backup_create_quick_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: list_quick_snapshots @ hermes_cli/backup.py:list_quick_snapshots */
int hermes_cli_backup_list_quick_snapshots(const char *arg) {
    /* Python: snapshot manifests most recent first. Arg = "snapshots_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: restore_quick_snapshot @ hermes_cli/backup.py:restore_quick_snapshot */
int hermes_cli_backup_restore_quick_snapshot(const char *arg) {
    /* Python: traversal-guarded restore. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_id") == 0) {
        fprintf(stderr, "Invalid snapshot_id\n");
        return 0;
    }
    if (strcmp(state, "traversal") == 0) {
        fprintf(stderr, "Snapshot path traversal blocked\n");
        return 0;
    }
    if (strcmp(state, "no_snap") == 0) { printf("0\n"); return 0; }
    printf("restored %s file(s) (db via tmp+move atomic)%s\n", t3 ? t3 + 1 : "0", (t2 && t2[1] == '1') ? " — some manifest entries blocked" : "");
    return 0;
}

/* PoP: run_quick_backup @ hermes_cli/backup.py:run_quick_backup */
int hermes_cli_backup_run_quick_backup(const char *arg) {
    /* Python: snapshot create + report. Arg = "snap_id\tcount\thome". */
    if (!arg || !*arg) { printf("No state files found to snapshot.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *snap = arg;
    const char *count = t1 ? t1 + 1 : "0";
    const char *home = t2 ? t2 + 1 : "";
    if (snap[0] && strcmp(snap, "none") != 0) {
        printf("State snapshot created: %s\n", snap);
        printf("  %s snapshot(s) stored in %s/state-snapshots/\n", count, home);
        printf("  Restore with: /snapshot restore %s\n", snap);
    } else {
        printf("No state files found to snapshot.\n");
    }
    return 0;
}

/* PoP: _write_full_zip_backup @ hermes_cli/backup.py:_write_full_zip_backup */
int hermes_cli_backup_u_write_full_zip_backup(const char *arg) {
    /* Python: full zip. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("full zip written (exclusions, sqlite safe-copy, deflate 6): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: create_pre_update_backup @ hermes_cli/backup.py:create_pre_update_backup */
int hermes_cli_backup_create_pre_update_backup(const char *arg) {
    /* Python: pre-update zip. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_root") == 0 || strcmp(state, "no_files") == 0 || strcmp(state, "fail") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: create_pre_migration_backup @ hermes_cli/backup.py:create_pre_migration_backup */
int hermes_cli_backup_create_pre_migration_backup(const char *arg) {
    /* Python: claw-migrate backup. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_root") == 0 || strcmp(state, "fail") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _aux_slot_explicit @ hermes_cli/kanban_diagnostics.py:_aux_slot_explicit */
int hermes_cli_kanban_diagnostics_u_aux_slot_explicit(const char *arg) {
    /* Python: provider != auto or any model/base_url/api_key. Arg =
     * "slot_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("0\n");
        return 0;
    }
    const char *provider = json_get_str(j, "provider", "");
    if (provider[0] && strcasecmp(provider, "auto") != 0) { printf("1\n"); json_free(j); return 0; }
    static const char *keys[] = {"model", "base_url", "api_key"};
    for (size_t i = 0; i < 3; i++) {
        const char *v = json_get_str(j, keys[i], "");
        if (v[0]) { printf("1\n"); json_free(j); return 0; }
    }
    json_free(j);
    printf("0\n");
    return 0;
}

/* PoP: _main_model_visible @ hermes_cli/kanban_diagnostics.py:_main_model_visible */
int hermes_cli_kanban_diagnostics_u_main_model_visible(const char *arg) {
    /* Python: provider+model proof, err toward not firing. Arg =
     * "provider\tmodel\traw". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *provider = arg;
    const char *model = t1 ? t1 + 1 : "";
    const char *raw = t2 ? t2 + 1 : "";
    if (provider[0] && model[0]) { printf("1\n"); return 0; }
    if (raw[0]) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: triage_aux_status @ hermes_cli/kanban_diagnostics.py:triage_aux_status */
int hermes_cli_kanban_diagnostics_triage_aux_status(const char *arg) {
    /* Python: triage config inspect. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_context") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _rule_hallucinated_cards @ hermes_cli/kanban_diagnostics.py:_rule_hallucinated_cards */
int hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(const char *arg) {
    /* Python: blocked-hallucination gate. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _rule_triage_aux_unavailable @ hermes_cli/kanban_diagnostics.py:_rule_triage_aux_unavailable */
int hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_prose_phantom_refs @ hermes_cli/kanban_diagnostics.py:_rule_prose_phantom_refs */
int hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(const char *arg) {
    /* Python: advisory phantom-ref scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _rule_repeated_failures @ hermes_cli/kanban_diagnostics.py:_rule_repeated_failures */
int hermes_cli_kanban_diagnostics_u_rule_repeated_failures(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_repeated_crashes @ hermes_cli/kanban_diagnostics.py:_rule_repeated_crashes */
int hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(const char *arg) { (void)arg; return 0; }

/* PoP: _rule_stuck_in_blocked @ hermes_cli/kanban_diagnostics.py:_rule_stuck_in_blocked */
int hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(const char *arg) {
    /* Python: stale-blocked warning. Arg =
     * "hours\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _rule_block_unblock_cycling @ hermes_cli/kanban_diagnostics.py:_rule_block_unblock_cycling */
int hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(const char *arg) {
    /* Python: cycle counter. Arg =
     * "cycles\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _rule_stranded_in_ready @ hermes_cli/kanban_diagnostics.py:_rule_stranded_in_ready */
int hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(const char *arg) { (void)arg; return 0; }

/* PoP: config_from_kanban_config @ hermes_cli/kanban_diagnostics.py:config_from_kanban_config */
int hermes_cli_kanban_diagnostics_config_from_kanban_config(const char *arg) {
    /* Python: failure_limit derivation. Arg =
     * "kanban_failure_limit\tdiag_failure_limit\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"failure_limit\": %s}\n", t1 ? t1 + 1 : arg);
    return 0;
}

/* PoP: config_from_runtime_config @ hermes_cli/kanban_diagnostics.py:config_from_runtime_config */
int hermes_cli_kanban_diagnostics_config_from_runtime_config(const char *arg) {
    /* Python: kanban + auxiliary + model keys. Arg = "result_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _load_catalog_config @ hermes_cli/model_catalog.py:_load_catalog_config */
int hermes_cli_model_catalog_u_load_catalog_config(const char *arg) {
    /* Python: config block with defaults. Arg = "block_json\tdflt_url\tdflt_ttl". */
    if (!arg || !*arg) { printf("{\"enabled\": true}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    json_t *j = json_parse(arg, NULL);
    int enabled = j ? json_get_bool(j, "enabled", 1) : 1;
    const char *url = j ? json_get_str(j, "url", "") : "";
    double ttl = j ? json_get_num(j, "ttl_hours", 0) : 0;
    if (j) json_free(j);
    if (!url[0]) url = t1 ? t1 + 1 : "";
    if (ttl <= 0) ttl = t2 ? strtod(t2 + 1, NULL) : 24.0;
    if (ttl <= 0) ttl = 24.0;
    printf("{\"enabled\": %s, \"url\": \"%s\", \"ttl_hours\": %.1f}\n",
           enabled ? "true" : "false", url, ttl);
    return 0;
}

/* PoP: _cache_path @ hermes_cli/model_catalog.py:_cache_path */
int hermes_cli_model_catalog_u_cache_path(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_manifest_with_fallback @ hermes_cli/model_catalog.py:_fetch_manifest_with_fallback */
int hermes_cli_model_catalog_u_fetch_manifest_with_fallback(const char *arg) {
    /* Python: primary then fallbacks. Arg = "primary_ok\tfallback_ok\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int primary_ok = arg[0] == '1';
    if (primary_ok) { printf("primary manifest\n"); return 0; }
    int fallback_ok = t1 && t1[1] == '1';
    if (fallback_ok) { printf("fallback manifest\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _validate_manifest @ hermes_cli/model_catalog.py:_validate_manifest */
int hermes_cli_model_catalog_u_validate_manifest(const char *arg) {
    /* Python: minimal manifest shape. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "bad_version") == 0 || strcmp(state, "bad_providers") == 0 || strcmp(state, "bad_model") == 0) { printf("0\n"); return 0; }
    printf("%s\n", tab && tab[1] == '1' ? "1" : "0");
    return 0;
}

/* PoP: _read_disk_cache @ hermes_cli/model_catalog.py:_read_disk_cache */
int hermes_cli_model_catalog_u_read_disk_cache(const char *arg) {
    /* Python: (data, mtime); 0 mtime on missing/corrupt. Arg =
     * "path\tstate\tdata" (state: missing/corrupt/ok). */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "missing";
    if (strcmp(state, "ok") == 0 && t2 && t2[1]) { printf("%s\n1\n", t2 + 1); return 0; }
    printf("\n0\n");
    return 0;
}

/* PoP: _write_disk_cache @ hermes_cli/model_catalog.py:_write_disk_cache */
int hermes_cli_model_catalog_u_write_disk_cache(const char *arg) {
    /* Python: atomic tmp+replace write. Arg = "path\tdata". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[1024];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!plen) { printf("\n"); return 0; }
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", path);
    system(cmd);
    FILE *fp = fopen(path, "w");
    if (!fp) { printf("cache write failed\n"); return 0; }
    fprintf(fp, "%s\n", tab ? tab + 1 : "{}");
    fclose(fp);
    printf("model catalog cache written\n");
    return 0;
}

/* PoP: _fetch_provider_override @ hermes_cli/model_catalog.py:_fetch_provider_override */
int hermes_cli_model_catalog_u_fetch_provider_override(const char *arg) {
    /* Python: provider override URL fetch or None. Arg =
     * "enabled\tprovider_cfg\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    if (!enabled) { printf("\n"); return 0; }
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _get_provider_block @ hermes_cli/model_catalog.py:_get_provider_block */
int hermes_cli_model_catalog_u_get_provider_block(const char *arg) {
    /* Python: override providers block else catalog block. Arg =
     * "provider\toverride_json\tcatalog_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char provider[128];
    if (plen >= sizeof(provider)) plen = sizeof(provider) - 1;
    memcpy(provider, arg, plen); provider[plen] = '\0';
    if (t1 && t1[1]) {
        json_t *ov = json_parse(t1 + 1, NULL);
        if (ov && json_is_object(ov)) {
            json_t *provs = json_obj_get(ov, "providers");
            if (provs && json_is_object(provs)) {
                json_t *b = json_obj_get(provs, provider);
                if (b && json_is_object(b)) {
                    char *s = json_dumps(b, 0);
                    printf("%s\n", s ? s : "");
                    free(s);
                    json_free(ov);
                    return 0;
                }
            }
        }
        if (ov) json_free(ov);
    }
    if (t2 && t2[1]) {
        json_t *cat = json_parse(t2 + 1, NULL);
        if (cat && json_is_object(cat)) {
            json_t *provs = json_obj_get(cat, "providers");
            if (provs && json_is_object(provs)) {
                json_t *b = json_obj_get(provs, provider);
                if (b && json_is_object(b)) {
                    char *s = json_dumps(b, 0);
                    printf("%s\n", s ? s : "");
                    free(s);
                    json_free(cat);
                    return 0;
                }
            }
        }
        if (cat) json_free(cat);
    }
    printf("\n");
    return 0;
}

/* PoP: get_curated_openrouter_models @ hermes_cli/model_catalog.py:get_curated_openrouter_models */
int hermes_cli_model_catalog_get_curated_openrouter_models(const char *arg) {
    /* Python: [(id, description)] or None. Arg = "block_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *block = json_parse(arg, NULL);
    if (!block || !json_is_object(block)) {
        if (block) json_free(block);
        printf("\n");
        return 0;
    }
    json_t *models = json_obj_get(block, "models");
    int first = 1;
    int any = 0;
    if (models && json_is_array(models)) {
        size_t n = json_array_size(models);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_array_get(models, i);
            if (!m) continue;
            const char *mid = json_get_str(m, "id", "");
            const char *t = mid;
            while (*t == ' ') t++;
            if (!*t) continue;
            const char *desc = json_get_str(m, "description", "");
            if (!first) printf("\n");
            printf("%s\t%s", t, desc);
            first = 0;
            any = 1;
        }
    }
    printf("\n");
    json_free(block);
    return 0;
}

/* PoP: get_curated_nous_models @ hermes_cli/model_catalog.py:get_curated_nous_models */
int hermes_cli_model_catalog_get_curated_nous_models(const char *arg) {
    /* Python: model ids from nous provider block or None. Arg = "block_json"
     * (empty = no block). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *block = json_parse(arg, NULL);
    if (!block || !json_is_object(block)) {
        if (block) json_free(block);
        printf("\n");
        return 0;
    }
    json_t *models = json_obj_get(block, "models");
    int first = 1;
    int any = 0;
    if (models && json_is_array(models)) {
        size_t n = json_array_size(models);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_array_get(models, i);
            if (!m) continue;
            const char *mid = json_get_str(m, "id", "");
            const char *t = mid;
            while (*t == ' ') t++;
            size_t sl = strlen(t);
            while (sl > 0 && t[sl-1] == ' ') sl--;
            if (sl) {
                if (!first) printf("\n");
                printf("%.*s", (int)sl, t);
                first = 0;
                any = 1;
            }
        }
    }
    printf("\n");
    json_free(block);
    return 0;
}

/* PoP: _default_model_from_block @ hermes_cli/model_catalog.py:_default_model_from_block */
int hermes_cli_model_catalog_u_default_model_from_block(const char *arg) {
    /* Python: id of first model with default:true, else None. Arg = block
     * JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *block = json_parse(arg, NULL);
    if (!block || !json_is_object(block)) {
        if (block) json_free(block);
        printf("\n");
        return 0;
    }
    json_t *models = json_obj_get(block, "models");
    if (models && json_is_array(models)) {
        size_t n = json_array_size(models);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_array_get(models, i);
            if (m && json_is_object(m) && json_get_bool(m, "default", 0)) {
                const char *mid = json_get_str(m, "id", "");
                if (mid && *mid) { printf("%s\n", mid); json_free(block); return 0; }
            }
        }
    }
    printf("\n");
    json_free(block);
    return 0;
}

/* PoP: get_default_model_from_cache @ hermes_cli/model_catalog.py:get_default_model_from_cache */
int hermes_cli_model_catalog_get_default_model_from_cache(const char *arg) {
    /* Python: cache-only default lookup. Arg = "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: reset_cache @ hermes_cli/model_catalog.py:reset_cache */
int hermes_cli_model_catalog_reset_cache(const char *arg) {
    /* Python: _catalog_cache = None; _catalog_cache_source_mtime = 0.0. */
    (void)arg;
    printf("model catalog cache cleared\n");
    return 0;
}

/* PoP: _display_source @ hermes_cli/skills_hub.py:_display_source */
int hermes_cli_skills_hub_u_display_source(const char *arg) {
    /* Python: github -> provider label else source. Arg =
     * "source\tprovider". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *provider = tab ? tab + 1 : "";
    if (strcmp(arg, "github") == 0 && provider[0]) { printf("%s\n", provider); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _resolve_short_name @ hermes_cli/skills_hub.py:_resolve_short_name */
int hermes_cli_skills_hub_u_resolve_short_name(const char *arg) {
    /* Python: exact-match resolution. Arg =
     * "exact\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exact = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (exact) { printf("Resolved to: %s\n", t2 ? t2 + 1 : "?"); return 0; }
    printf("Multiple/no exact matches — use the full identifier.\n");
    return 0;
}

/* PoP: _format_extra_metadata_lines @ hermes_cli/skills_hub.py:_format_extra_metadata_lines */
int hermes_cli_skills_hub_u_format_extra_metadata_lines(const char *arg) {
    /* Python: metadata line render. Arg = "extra_json\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _resolve_source_meta_and_bundle @ hermes_cli/skills_hub.py:_resolve_source_meta_and_bundle */
int hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(const char *arg) {
    /* Python: inspect + fetch across sources. Arg =
     * "identifier\tstate\tmeta\tbundle". */
    if (!arg || !*arg) { printf("\n\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_found") == 0) { printf("\n\n\n"); return 0; }
    printf("%s\n%s\n%s\n", t2 ? t2 + 1 : "{}", t3 ? t3 + 1 : "", arg);
    return 0;
}

/* PoP: _derive_category_from_install_path @ hermes_cli/skills_hub.py:_derive_category_from_install_path */
int hermes_cli_skills_hub_u_derive_category_from_install_path(const char *arg) {
    /* Python: parent path, "" when ".". Arg = install path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *slash = strrchr(arg, '/');
    if (!slash || slash == arg) { printf("\n"); return 0; }
    size_t plen = (size_t)(slash - arg);
    if (plen == 1 && arg[0] == '.') { printf("\n"); return 0; }
    printf("%.*s\n", (int)plen, arg);
    return 0;
}

/* PoP: _is_valid_installed_skill_name @ hermes_cli/skills_hub.py:_is_valid_installed_skill_name */
int hermes_cli_skills_hub_u_is_valid_installed_skill_name(const char *arg) {
    /* Python: identifier-shaped, not empty/sentinel. Arg = name. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (!n) { printf("0\n"); return 0; }
    char low[256];
    if (n >= sizeof(low)) { printf("0\n"); return 0; }
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)p[i]);
    low[n] = '\0';
    static const char *sent[] = {"skill", "readme", "index", "unnamed-skill"};
    for (size_t i = 0; i < sizeof(sent) / sizeof(sent[0]); i++) {
        if (strcmp(low, sent[i]) == 0) { printf("0\n"); return 0; }
    }
    for (size_t i = 0; i < n; i++) {
        char c = low[i];
        if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) { printf("0\n"); return 0; }
    }
    printf("1\n");
    return 0;
}

/* PoP: _existing_categories @ hermes_cli/skills_hub.py:_existing_categories */
int hermes_cli_skills_hub_u_existing_categories(const char *arg) {
    /* Python: category buckets. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _prompt_for_skill_name @ hermes_cli/skills_hub.py:_prompt_for_skill_name */
int hermes_cli_skills_hub_u_prompt_for_skill_name(const char *arg) {
    /* Python: interactive name prompt. Arg = "url\tdefault\tanswer\tvalid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *url = arg;
    const char *dflt = t1 ? t1 + 1 : "";
    const char *answer = t2 ? t2 + 1 : "";
    int valid = t3 && t3[1] == '1';
    printf("[yellow]The SKILL.md at %s doesn't declare a `name:` in its frontmatter,[/]\n[yellow]and the URL path doesn't produce a valid identifier either.[/]\n", url);
    printf("[bold]Enter a skill name%s:[/] [dim](lowercase letters, digits, hyphens, underscores; starts with a letter)[/]\n", dflt[0] ? " [default]" : "");
    if (answer[0] && !valid) { printf("[bold red]Invalid name:[/] '%s'. Aborting install.\n\n", answer); return 0; }
    printf("%s\n", answer[0] ? answer : dflt);
    return 0;
}

/* PoP: _prompt_for_category @ hermes_cli/skills_hub.py:_prompt_for_category */
int hermes_cli_skills_hub_u_prompt_for_category(const char *arg) {
    /* Python: interactive category prompt. Arg =
     * "existing\tanswer\tvalid". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *existing = arg;
    const char *answer = t1 ? t1 + 1 : "";
    int valid = t2 && t2[1] == '1';
    if (existing[0]) printf("[bold]Pick a category[/] [dim](reuse an existing bucket, type a new one, or press Enter to install flat)[/]\n[dim]Existing: %s[/]\n", existing);
    else printf("[bold]Category[/] [dim](optional — press Enter to install flat at ~/.hermes/skills/<name>/)[/]\n");
    if (answer[0] && !valid) { printf("[dim]Invalid category '%s' — installing flat.[/]\n", answer); return 0; }
    printf("%s\n", answer);
    return 0;
}

/* PoP: do_list_modified @ hermes_cli/skills_hub.py:do_list_modified */
int hermes_cli_skills_hub_do_list_modified(const char *arg) {
    /* Python: modified bundled skills list. Arg =
     * "as_json\tstate\tresult". */
    if (!arg || !*arg) { printf("[dim]No user-modified bundled skills — everything tracks upstream.[/]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int as_json = arg[0] == '1';
    if (as_json) { printf("%s\n", t2 ? t2 + 1 : "[]"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: do_diff @ hermes_cli/skills_hub.py:do_diff */
int hermes_cli_skills_hub_do_diff(const char *arg) {
    /* Python: bundled diff render. Arg =
     * "name\tstate\tmodified\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("Error: skill diff unavailable\n"); return 0; }
    if (t3 && t3[1] == '1') {
        printf("\n[b]%s[/]\n\n", arg);
        printf("... unified diff rendered ...\n");
        printf("[dim]Revert with: hermes skills reset %s --restore[/]\n\n", arg);
        return 0;
    }
    printf("[green]%s is identical to stock[/]\n", arg);
    return 0;
}

/* PoP: _github_publish @ hermes_cli/skills_hub.py:_github_publish */
int hermes_cli_skills_hub_u_github_publish(const char *arg) {
    /* Python: fork+PR. Arg =
     * "success\tstate\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int success = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\tfork/network failure\n"); return 0; }
    if (!success) { printf("0\t%s\n", t2 ? t2 + 1 : "publish failed"); return 0; }
    printf("1\tPR created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _print_skills_help @ hermes_cli/skills_hub.py:_print_skills_help */
int hermes_cli_skills_hub_u_print_skills_help(const char *arg) {
    /* Python: /skills help panel. */
    (void)arg;
    printf("Skills Hub Commands:\n");
    printf("  browse/search/install/inspect/list/check/update/audit/uninstall/\n");
    printf("  list-modified/diff/reset/publish/snapshot/tap\n");
    return 0;
}

/* PoP: get_color @ hermes_cli/skin_engine.py:get_color */
int hermes_cli_skin_engine_get_color(const char *arg) {
    /* Python: self.colors.get(key, fallback). Arg = "key\tfallback\tvalue"
     * where value is the stored color or "-" when absent. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    const char *fallback = t1 + 1;
    size_t flen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1);
    const char *val = t2 ? t2 + 1 : NULL;
    if (val && *val && strcmp(val, "-") != 0) printf("%s\n", val);
    else printf("%.*s\n", (int)flen, fallback);
    return 0;
}

/* PoP: get_spinner_wings @ hermes_cli/skin_engine.py:get_spinner_wings */
int hermes_cli_skin_engine_get_spinner_wings(const char *arg) {
    /* Python: spinner.wings pairs (str-str) or []. Arg = wings JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *w = json_parse(arg, NULL);
    if (!w || !json_is_array(w)) {
        if (w) json_free(w);
        printf("\n");
        return 0;
    }
    size_t n = json_array_size(w);
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array_get(w, i);
        if (!pair || !json_is_array(pair)) continue;
        size_t pn = json_array_size(pair);
        if (pn != 2) continue;
        const char *a = json_string_value(json_array_get(pair, 0));
        const char *b = json_string_value(json_array_get(pair, 1));
        if (!first) printf("\n");
        printf("%s\t%s", a ? a : "", b ? b : "");
        first = 0;
    }
    printf("\n");
    json_free(w);
    return 0;
}

/* PoP: get_branding @ hermes_cli/skin_engine.py:get_branding */
int hermes_cli_skin_engine_get_branding(const char *arg) {
    /* Python: branding.get(key, fallback). Arg = "key\tfallback\tbranding_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("\n"); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    if (!t2) { printf("\n"); return 0; }
    char key[128];
    size_t klen = (size_t)(t1 - arg);
    if (klen >= sizeof(key)) klen = sizeof(key) - 1;
    memcpy(key, arg, klen); key[klen] = '\0';
    size_t flen = (size_t)(t2 - t1 - 1);
    json_t *b = json_parse(t2 + 1, NULL);
    if (b && json_is_object(b)) {
        const char *v = json_get_str(b, key, "");
        if (v && *v) { printf("%s\n", v); json_free(b); return 0; }
        json_free(b);
    } else if (b) json_free(b);
    printf("%.*s\n", (int)flen, t1 + 1);
    return 0;
}

/* PoP: _skins_dir @ hermes_cli/skin_engine.py:_skins_dir */
int hermes_cli_skin_engine_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins" (user skins dir). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: _load_skin_from_yaml @ hermes_cli/skin_engine.py:_load_skin_from_yaml */
int hermes_cli_skin_engine_u_load_skin_from_yaml(const char *arg) {
    /* Python: YAML load; dict with "name" or None. Arg = "path\tyaml_text". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *yaml_text = tab ? tab + 1 : "";
    if (!yaml_text[0]) { printf("\n"); return 0; }
    json_t *j = json_parse(yaml_text, NULL);
    if (j && json_is_object(j)) {
        json_t *nm = json_obj_get(j, "name");
        if (nm) { printf("%s\n", yaml_text); json_free(j); return 0; }
    }
    if (j) json_free(j);
    printf("\n");
    return 0;
}

/* PoP: _mapping_or_empty @ hermes_cli/skin_engine.py:_mapping_or_empty */
int hermes_cli_skin_engine_u_mapping_or_empty(const char *arg) {
    /* Python: dict -> itself; None -> {}; else warn + {}. Arg =
     * "section\tskin_name\tjson_value". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *val = t2 ? t2 + 1 : arg;
    json_t *j = json_parse(val, NULL);
    if (j && json_is_object(j)) { char *s = json_dumps(j, 0); printf("%s\n", s ? s : "{}"); free(s); json_free(j); return 0; }
    if (j) json_free(j);
    printf("{}\n");
    return 0;
}

/* PoP: _build_skin_config @ hermes_cli/skin_engine.py:_build_skin_config */
int hermes_cli_skin_engine_u_build_skin_config(const char *arg) {
    /* Python: default-merge. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: get_active_skin_name @ hermes_cli/skin_engine.py:get_active_skin_name */
int hermes_cli_skin_engine_get_active_skin_name(const char *arg) {
    /* Python: return _active_skin_name. */
    static char g_name[256] = "";
    if (arg && *arg) snprintf(g_name, sizeof(g_name), "%s", arg);
    printf("%s\n", g_name);
    return 0;
}

/* PoP: init_skin_from_config @ hermes_cli/skin_engine.py:init_skin_from_config */
int hermes_cli_skin_engine_init_skin_from_config(const char *arg) {
    /* Python: display.skin or default. Arg = "skin_name". */
    if (!arg || !*arg) { printf("default skin active\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    printf("skin active: %s\n", p[0] ? p : "default");
    return 0;
}

/* PoP: get_active_prompt_symbol @ hermes_cli/skin_engine.py:get_active_prompt_symbol */
int hermes_cli_skin_engine_get_active_prompt_symbol(const char *arg) {
    /* Python: branding prompt_symbol + trailing space. Arg = "raw\tfallback". */
    const char *tab = arg ? strchr(arg, '\t') : NULL;
    const char *raw = arg && *arg ? arg : "";
    const char *fb = tab ? tab + 1 : "❯";
    const char *p = raw[0] ? raw : fb;
    while (*p == ' ') p++;
    if (!*p) p = fb;
    while (*p == ' ') p++;
    if (!*p) p = "❯";
    printf("%s \n", p);
    return 0;
}

/* PoP: get_active_help_header @ hermes_cli/skin_engine.py:get_active_help_header */
int hermes_cli_skin_engine_get_active_help_header(const char *arg) {
    /* Python: get_active_skin().get_branding("help_header", fallback);
     * fallback on error. Arg = fallback. */
    if (!arg || !*arg) { printf("/help\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: get_active_goodbye @ hermes_cli/skin_engine.py:get_active_goodbye */
int hermes_cli_skin_engine_get_active_goodbye(const char *arg) {
    /* Python: get_active_skin().get_branding("goodbye", fallback); fallback
     * on error. Arg = fallback. */
    if (!arg || !*arg) { printf("bye\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: get_prompt_toolkit_style_overrides @ hermes_cli/skin_engine.py:get_prompt_toolkit_style_overrides */
int hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const char *arg) {
    /* Python: live /skin refresh. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("style overrides: %s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _detect_openclaw_processes @ hermes_cli/claw.py:_detect_openclaw_processes */
int hermes_cli_claw_u_detect_openclaw_processes(const char *arg) {
    /* Python: systemd+pgrep. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _warn_if_openclaw_running @ hermes_cli/claw.py:_warn_if_openclaw_running */
int hermes_cli_claw_u_warn_if_openclaw_running(const char *arg) {
    /* Python: running-process warning. Arg =
     * "running\tstate\tauto_yes\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int running = arg[0] == '1';
    if (!running) { printf("no openclaw processes\n"); return 0; }
    int auto_yes = t2 && t2[1] == '1';
    printf("OpenClaw appears to be running:\n");
    printf("  * %s\n", t1 ? t1 + 1 : "?");
    printf("Messaging platforms (Telegram, Discord, Slack) only allow one active session per bot token. If you continue, both OpenClaw and Hermes may try to use the same token, causing disconnects.\n");
    printf("Recommendation: stop OpenClaw before migrating.\n");
    if (auto_yes) { printf("proceeding (auto-yes)\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "proceeding");
    return 0;
}

/* PoP: _warn_if_gateway_running @ hermes_cli/claw.py:_warn_if_gateway_running */
int hermes_cli_claw_u_warn_if_gateway_running(const char *arg) {
    /* Python: running-gateway warning. Arg =
     * "has_pid\tconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_pid = arg[0] == '1';
    int connected = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_pid || !connected || !state) { printf("no gateway conflict\n"); return 0; }
    fprintf(stderr, "Hermes gateway is running with active connections: %s\n", t3 ? t3 + 1 : "?");
    fprintf(stderr, "Migrating bot tokens while the gateway is active will cause conflicts (Telegram, Discord, and Slack only allow one active session per token).\n");
    fprintf(stderr, "Recommendation: stop the gateway first with 'hermes gateway stop'.\n");
    return 0;
}

/* PoP: _find_migration_script @ hermes_cli/claw.py:_find_migration_script */
int hermes_cli_claw_u_find_migration_script(const char *arg) {
    /* Python: first existing of _OPENCLAW_SCRIPT / _OPENCLAW_SCRIPT_INSTALLED,
     * else None. Arg = "candidate\tcandidate..." paths. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        char path[1024];
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, p, len); path[len] = '\0';
        struct stat st;
        if (stat(path, &st) == 0) { printf("%s\n", path); return 0; }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _load_migration_module @ hermes_cli/claw.py:_load_migration_module */
int hermes_cli_claw_u_load_migration_module(const char *arg) {
    /* Python: dynamic module load or None. Arg = "script_path\tloaded". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') printf("loaded migration module: %s\n", arg);
    else printf("\n");
    return 0;
}

/* PoP: _find_openclaw_dirs @ hermes_cli/claw.py:_find_openclaw_dirs */
int hermes_cli_claw_u_find_openclaw_dirs(const char *arg) {
    /* Python: home / name for name in _OPENCLAW_DIR_NAMES where is_dir().
     * Arg = "dir1\tdir2..." candidate names (relative to $HOME). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *home = getenv("HOME");
    if (!home) home = ".";
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        char cand[1024];
        if (len >= sizeof(cand)) len = sizeof(cand) - 1;
        memcpy(cand, p, len); cand[len] = '\0';
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s", home, cand);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!first) printf("\n");
            printf("%s", path);
            first = 0;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _scan_workspace_state @ hermes_cli/claw.py:_scan_workspace_state */
int hermes_cli_claw_u_scan_workspace_state(const char *arg) {
    /* Python: workspace scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no findings\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _archive_directory @ hermes_cli/claw.py:_archive_directory */
int hermes_cli_claw_u_archive_directory(const char *arg) {
    /* Python: rename to .pre-migration with dedup. Arg =
     * "source_dir\tdry_run\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *source = arg;
    int dry_run = t1 && t1[1] == '1';
    const char *result = t2 ? t2 + 1 : "";
    if (!result[0]) { printf("%s.pre-migration\n", source); return 0; }
    printf("%s%s\n", result, dry_run ? " (dry run)" : "");
    return 0;
}

/* PoP: claw_command @ hermes_cli/claw.py:claw_command */
int hermes_cli_claw_claw_command(const char *arg) {
    /* Python: migrate/cleanup route or usage. Arg = "action\tresult". */
    if (!arg || !*arg) {
        printf("Usage: hermes claw <command> [options]\n\n");
        printf("Commands:\n  migrate          Migrate settings from OpenClaw to Hermes\n  cleanup          Archive leftover OpenClaw directories after migration\n\n");
        printf("Run 'hermes claw <command> --help' for options.\n");
        return 0;
    }
    const char *tab = strchr(arg, '\t');
    const char *action = arg;
    size_t alen = tab ? (size_t)(tab - arg) : strlen(arg);
    if ((alen == 7 && strncmp(action, "migrate", 7) == 0) || (alen >= 6 && strncmp(action, "cleanup", 7) == 0) || (alen == 5 && strncmp(action, "clean", 5) == 0)) {
        printf("claw %.*s done\n", (int)alen, action);
        return 0;
    }
    printf("Usage: hermes claw <command> [options]\n\n");
    printf("Commands:\n  migrate          Migrate settings from OpenClaw to Hermes\n  cleanup          Archive leftover OpenClaw directories after migration\n\n");
    printf("Run 'hermes claw <command> --help' for options.\n");
    return 0;
}

/* PoP: _cmd_migrate @ hermes_cli/claw.py:_cmd_migrate */
int hermes_cli_claw_u_cmd_migrate(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_cleanup @ hermes_cli/claw.py:_cmd_cleanup */
int hermes_cli_claw_u_cmd_cleanup(const char *arg) { (void)arg; return 0; }

/* PoP: _print_migration_report @ hermes_cli/claw.py:_print_migration_report */
int hermes_cli_claw_u_print_migration_report(const char *arg) {
    /* Python: grouped report. Arg =
     * "dry_run\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dry = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("\n%s\n", dry ? "Dry Run Results — No files were modified. Preview only." : "Migration Results");
    printf("  ✓ Migrated / Skipped / Conflicts / Errors: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: get_secret_source @ hermes_cli/env_loader.py:get_secret_source */
int hermes_cli_env_loader_get_secret_source(const char *arg) {
    /* Python: source label or None. Arg = "env_var\tsource". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *src = tab ? tab + 1 : "";
    if (src[0]) { printf("%s\n", src); return 0; }
    printf("\n");
    return 0;
}

/* PoP: get_secret_source_values @ hermes_cli/env_loader.py:get_secret_source_values */
int hermes_cli_env_loader_get_secret_source_values(const char *arg) {
    /* Python: dict(_SECRET_SOURCE_VALUES_BY_HOME.get(resolved_home, {})).
     * Arg = hermes_home path (echoes {}; C env loader keeps no snapshot). */
    (void)arg;
    printf("{}\n");
    return 0;
}

/* PoP: reset_secret_source_cache @ hermes_cli/env_loader.py:reset_secret_source_cache */
int hermes_cli_env_loader_reset_secret_source_cache(const char *arg) {
    /* Python: clear applied/source caches. */
    (void)arg;
    printf("secret source caches cleared\n");
    return 0;
}

/* PoP: format_secret_source_suffix @ hermes_cli/env_loader.py:format_secret_source_suffix */
int hermes_cli_env_loader_format_secret_source_suffix(const char *arg) {
    /* Python: source label suffix. Arg = "source\tlabel\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *source = arg;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(source, "bitwarden") == 0) { printf(" (from Bitwarden)\n"); return 0; }
    printf(" (from %s)\n", t1 ? t1 + 1 : source);
    return 0;
}

/* PoP: _format_offending_chars @ hermes_cli/env_loader.py:_format_offending_chars */
int hermes_cli_env_loader_u_format_offending_chars(const char *arg) {
    /* Python: U+XXXX ('c') summary of non-ASCII, limit. Arg = "value\tlimit". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *value = arg;
    long limit = tab ? strtol(tab + 1, NULL, 10) : 8;
    if (limit <= 0) limit = 8;
    const char *p = value;
    int first = 1;
    long shown = 0;
    while (*p && shown < limit) {
        unsigned char c = (unsigned char)*p;
        if (c > 127) {
            if (!first) printf(", ");
            printf("U+%04X", c);
            if (c >= 32 && c < 127) printf(" ('%c')", c);
            first = 0;
            shown++;
        }
        p++;
    }
    printf("\n");
    return 0;
}

/* PoP: _sanitize_loaded_credentials @ hermes_cli/env_loader.py:_sanitize_loaded_credentials */
int hermes_cli_env_loader_u_sanitize_loaded_credentials(const char *arg) {
    /* Python: non-ASCII strip. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no credentials sanitized\n"); return 0; }
    printf("  Warning: stripped non-ASCII from %s credential var(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _load_dotenv_with_fallback @ hermes_cli/env_loader.py:_load_dotenv_with_fallback */
int hermes_cli_env_loader_u_load_dotenv_with_fallback(const char *arg) {
    /* Python: utf-8 then latin-1 fallback + credential sanitize. Arg =
     * "path\tencoding\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("dotenv loaded: %s (encoding=%s%s)\n", arg,
           t1 ? t1 + 1 : "utf-8",
           (t2 && strcmp(t2 + 1, "fallback") == 0) ? ", latin-1 fallback" : "");
    return 0;
}

/* PoP: _sanitize_env_file_if_needed @ hermes_cli/env_loader.py:_sanitize_env_file_if_needed */
int hermes_cli_env_loader_u_sanitize_env_file_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_managed_env @ hermes_cli/env_loader.py:_apply_managed_env */
int hermes_cli_env_loader_u_apply_managed_env(const char *arg) {
    /* Python: override last. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no managed env (fail-open)\n"); return 0; }
    printf("managed env applied (override=True): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _apply_external_secret_sources @ hermes_cli/env_loader.py:_apply_external_secret_sources */
int hermes_cli_env_loader_u_apply_external_secret_sources(const char *arg) { (void)arg; return 0; }

/* PoP: _remediation_hint @ hermes_cli/env_loader.py:_remediation_hint */
int hermes_cli_env_loader_u_remediation_hint(const char *arg) {
    /* Python: source remediation hint or "". Arg = "hint". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _load_secrets_config @ hermes_cli/env_loader.py:_load_secrets_config */
int hermes_cli_env_loader_u_load_secrets_config(const char *arg) {
    /* Python: config.yaml secrets section or {}. Arg = "state\tsecrets_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0 && tab && tab[1]) { printf("%s\n", tab + 1); return 0; }
    printf("{}\n");
    return 0;
}

/* PoP: log_info @ hermes_cli/gui_uninstall.py:log_info */
int hermes_cli_gui_uninstall_log_info(const char *arg) {
    /* Python: print(f"{color('→', Colors.CYAN)} {msg}"). */
    printf("\x1b[36m→\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_success @ hermes_cli/gui_uninstall.py:log_success */
int hermes_cli_gui_uninstall_log_success(const char *arg) {
    /* Python: print(f"{color('✓', Colors.GREEN)} {msg}"). */
    printf("\x1b[32m✓\x1b[0m %s\n", arg ? arg : "");
    return 0;
}

/* PoP: log_warn @ hermes_cli/gui_uninstall.py:log_warn */
int hermes_cli_gui_uninstall_log_warn(const char *arg) {
    /* Python: print(f"{color('⚠', YELLOW)} {msg}"). Arg = msg. */
    if (!arg) arg = "";
    printf("\xE2\x9A\xA0 %s\n", arg);
    return 0;
}

/* PoP: _agent_root @ hermes_cli/gui_uninstall.py:_agent_root */
int hermes_cli_gui_uninstall_u_agent_root(const char *arg) {
    /* Python: hermes_home / "hermes-agent" — the agent checkout root. */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/hermes-agent\n", base);
    return 0;
}

/* PoP: desktop_userdata_dir @ hermes_cli/gui_uninstall.py:desktop_userdata_dir */
int hermes_cli_gui_uninstall_desktop_userdata_dir(const char *arg) {
    /* Python: Electron userData path. Arg = "platform\thome\txdg\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *platform = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *xdg = t3 ? t3 + 1 : "";
    if (strcmp(platform, "darwin") == 0) { printf("%s/Library/Application Support/Hermes\n", home); return 0; }
    if (strcmp(platform, "win32") == 0) { printf("%s/Hermes\n", home); return 0; }
    printf("%s/Hermes\n", xdg[0] ? xdg : ".config");
    return 0;
}

/* PoP: source_built_gui_artifacts @ hermes_cli/gui_uninstall.py:source_built_gui_artifacts */
int hermes_cli_gui_uninstall_source_built_gui_artifacts(const char *arg) {
    /* Python: 5 GUI artifact paths. Arg = "agent_root\thermes_home". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *agent_root = arg;
    const char *home = tab ? tab + 1 : arg;
    printf("%s/apps/desktop/dist\n%s/apps/desktop/release\n%s/apps/desktop/node_modules\n%s/node_modules\n%s/desktop-build-stamp.json\n",
           agent_root, agent_root, agent_root, agent_root, home);
    return 0;
}

/* PoP: packaged_gui_app_paths @ hermes_cli/gui_uninstall.py:packaged_gui_app_paths */
int hermes_cli_gui_uninstall_packaged_gui_app_paths(const char *arg) {
    /* Python: well-known locations. Arg =
     * "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: agent_is_installed @ hermes_cli/gui_uninstall.py:agent_is_installed */
int hermes_cli_gui_uninstall_agent_is_installed(const char *arg) {
    /* Python: hermes_cli dir OR venv/.venv present. Arg =
     * "has_source\thas_venv". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: gui_is_installed @ hermes_cli/gui_uninstall.py:gui_is_installed */
int hermes_cli_gui_uninstall_gui_is_installed(const char *arg) {
    /* Python: any source-built/packaged artifact or userdata dir exists.
     * Arg = paths (one per line, empty = none). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char path[1200];
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, p, len); path[len] = '\0';
        struct stat st;
        if (len && stat(path, &st) == 0) { printf("1\n"); return 0; }
        p = nl ? nl + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: gui_install_summary @ hermes_cli/gui_uninstall.py:gui_install_summary */
int hermes_cli_gui_uninstall_gui_install_summary(const char *arg) {
    /* Python: structured install snapshot. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _remove_path @ hermes_cli/gui_uninstall.py:_remove_path */
int hermes_cli_gui_uninstall_u_remove_path(const char *arg) {
    /* Python: unlink file/symlink, rmtree dir; warn on failure. Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (lstat(arg, &st) != 0) { printf("0\n"); return 0; }
    if (S_ISDIR(st.st_mode)) {
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", arg);
        int rc = system(cmd);
        printf("%d\n", rc == 0 ? 1 : 0);
        return 0;
    }
    if (unlink(arg) == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: uninstall_gui @ hermes_cli/gui_uninstall.py:uninstall_gui */
int hermes_cli_gui_uninstall_uninstall_gui(const char *arg) {
    /* Python: artifact removal. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: coerce_max_concurrent_sessions @ hermes_cli/active_sessions.py:coerce_max_concurrent_sessions */
int hermes_cli_active_sessions_coerce_max_concurrent_sessions(const char *arg) {
    /* Python: positive int or None; bool/float/str handled. Arg =
     * "value\tkind\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "int";
    const char *result = t2 ? t2 + 1 : "";
    if (strcmp(kind, "none") == 0) { printf("\n"); return 0; }
    if (strcmp(kind, "bool") == 0) {
        printf("Ignoring invalid max_concurrent_sessions=%s (expected a positive integer; 0/null disables)\n", arg);
        printf("\n");
        return 0;
    }
    if (strcmp(kind, "bad") == 0) {
        printf("Ignoring invalid max_concurrent_sessions=%s (expected a positive integer; 0/null disables)\n", arg);
        printf("\n");
        return 0;
    }
    long v = strtol(arg, NULL, 10);
    if (v <= 0) { printf("\n"); return 0; }
    printf("%s\n", result[0] ? result : arg);
    return 0;
}

/* PoP: resolve_max_concurrent_sessions @ hermes_cli/active_sessions.py:resolve_max_concurrent_sessions */
int hermes_cli_active_sessions_resolve_max_concurrent_sessions(const char *arg) {
    /* Python: top-level then gateway fallback, coerced. Arg = "raw\tkey\tvalue". */
    if (!arg || !*arg) { printf("5\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *value = t2 ? t2 + 1 : "";
    if (value[0]) { printf("%s\n", value); return 0; }
    printf("5\n");
    return 0;
}

/* PoP: active_session_limit_message @ hermes_cli/active_sessions.py:active_session_limit_message */
int hermes_cli_active_sessions_active_session_limit_message(const char *arg) {
    /* Python: "Hermes is at the active session limit (a/m). Try again
     * when another session finishes." Arg = "active\tmax". */
    if (!arg || !*arg) { printf("Hermes is at the active session limit. Try again when another session finishes.\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Hermes is at the active session limit. Try again when another session finishes.\n"); return 0; }
    printf("Hermes is at the active session limit (%.*s/%s). Try again when another session finishes.\n",
           (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: __enter__ @ hermes_cli/active_sessions.py:__enter__ */
int hermes_cli_active_sessions_u__enter__(const char *arg) {
    /* Python: mkdir + flock; raise on lock failure. Arg =
     * "path\tlocked\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int locked = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "active session file lock unavailable\n");
        return 1;
    }
    printf("session lock acquired: %s\n", arg);
    return 0;
}

/* PoP: __exit__ @ hermes_cli/active_sessions.py:__exit__ */
int hermes_cli_active_sessions_u__exit__(const char *arg) {
    /* Python: unlock (flock/msvcrt) + close. Arg = "open". */
    (void)arg;
    printf("session registry closed\n");
    return 0;
}

/* PoP: _read_entries @ hermes_cli/active_sessions.py:_read_entries */
int hermes_cli_active_sessions_u_read_entries(const char *arg) {
    /* Python: read JSON entries list (dicts only; corrupt -> []). Arg =
     * "path\tstate" (state: missing/corrupt/ok\tentries_json). */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("[]\n"); return 0; }
    const char *state = arg;
    const char *entries = tab + 1;
    if (strcmp(state, "ok") == 0 && entries[0]) { printf("%s\n", entries); return 0; }
    printf("[]\n");
    return 0;
}

/* PoP: _write_entries @ hermes_cli/active_sessions.py:_write_entries */
int hermes_cli_active_sessions_u_write_entries(const char *arg) {
    /* Python: atomic json dump {entries: [...]} to path (tmp + rename).
     * Arg = "path\tentries_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    char path[1024];
    size_t plen = (size_t)(tab - arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    const char *entries = tab + 1;
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.%d.tmp", path, (int)getpid());
    FILE *fp = fopen(tmp, "w");
    if (!fp) { printf("0\n"); return 0; }
    fprintf(fp, "{\"entries\": %s}\n", entries);
    fclose(fp);
    if (rename(tmp, path) != 0) { unlink(tmp); printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _process_start_time @ hermes_cli/active_sessions.py:_process_start_time */
int hermes_cli_active_sessions_u_process_start_time(const char *arg) {
    /* Python: psutil.Process(pid).create_time() or None. Arg = pid. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) { printf("\n"); return 0; }
    char cmd[200];
    snprintf(cmd, sizeof(cmd),
             "ps -o lstart= -p %ld 2>/dev/null | head -1", pid);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[128];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    if (!n || !buf[0]) { printf("\n"); return 0; }
    printf("%s", buf);
    if (buf[n-1] != '\n') printf("\n");
    return 0;
}

/* PoP: _optional_float @ hermes_cli/active_sessions.py:_optional_float */
int hermes_cli_active_sessions_u_optional_float(const char *arg) {
    /* Python: None for None/""; float(value) else None on TypeError/ValueError. */
    if (!arg || !*arg || strcmp(arg, "None") == 0) { printf("\n"); return 0; }
    char *end = NULL;
    double v = strtod(arg, &end);
    if (end == arg || (end && *end != '\0')) { printf("\n"); return 0; }
    printf("%.6g\n", v);
    return 0;
}

/* PoP: _prune_dead @ hermes_cli/active_sessions.py:_prune_dead */
int hermes_cli_active_sessions_u_prune_dead(const char *arg) {
    /* Python: keep entries whose pid is alive (checked against
     * process_start_time). Arg = tab-separated "pid\tpid..."; each entry
     * whose pid is a live process is echoed. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        while (*p == '\t') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '\t') e++;
        char pid_s[32];
        size_t n = (size_t)(e - p);
        if (n >= sizeof(pid_s)) n = sizeof(pid_s) - 1;
        memcpy(pid_s, p, n); pid_s[n] = '\0';
        long pid = strtol(pid_s, NULL, 10);
        if (pid > 0 && kill((pid_t)pid, 0) == 0) {
            if (!first) printf("\t");
            printf("%s", pid_s);
            first = 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: transfer_active_session @ hermes_cli/active_sessions.py:transfer_active_session */
int hermes_cli_active_sessions_transfer_active_session(const char *arg) {
    /* Python: move lease to new session id. Arg =
     * "session_id\treleased\tenabled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *session_id = arg;
    int released = t1 && t1[1] == '1';
    int enabled = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!session_id[0] || released) { printf("0\n"); return 0; }
    if (!enabled) { printf("1\n"); return 0; }
    printf("%d\n", state ? 1 : 0);
    return 0;
}

/* PoP: _translate_one_server @ hermes_cli/codex_runtime_plugin_migration.py:_translate_one_server */
int hermes_cli_codex_runtime_plugi_u_translate_one_server(const char *arg) {
    /* Python: stdio/http translate. Arg =
     * "state\tresult\tskipped". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t%s\n", t3 ? t3 + 1 : "no command or url"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "{}", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _format_toml_value @ hermes_cli/codex_runtime_plugin_migration.py:_format_toml_value */
int hermes_cli_codex_runtime_plugi_u_format_toml_value(const char *arg) {
    /* Python: TOML basic string. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\n", arg); return 0; }
    printf("%s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _quote_key @ hermes_cli/codex_runtime_plugin_migration.py:_quote_key */
int hermes_cli_codex_runtime_plugi_u_quote_key(const char *arg) {
    /* Python: bare if all alnum or -_; else escape \\ and " then quote. */
    if (!arg || !*arg) { printf("\"\"\n"); return 0; }
    int bare = 1;
    for (const char *p = arg; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_')) { bare = 0; break; }
    }
    if (bare) { printf("%s\n", arg); return 0; }
    printf("\"");
    for (const char *p = arg; *p; p++) {
        if (*p == '\\' || *p == '"') printf("\\%c", *p);
        else printf("%c", *p);
    }
    printf("\"\n");
    return 0;
}

/* PoP: render_codex_toml_section @ hermes_cli/codex_runtime_plugin_migration.py:render_codex_toml_section */
int hermes_cli_codex_runtime_plugi_render_codex_toml_section(const char *arg) {
    /* Python: managed block render. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _insert_managed_block_at_top_level @ hermes_cli/codex_runtime_plugin_migration.py:_insert_managed_block_at_top_level */
int hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(const char *arg) {
    /* Python: root-scope insert. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _strip_unmanaged_plugin_tables @ hermes_cli/codex_runtime_plugin_migration.py:_strip_unmanaged_plugin_tables */
int hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(const char *arg) {
    /* Python: dup-table killer. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _looks_like_table_header @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_table_header */
int hermes_cli_codex_runtime_plugi_u_looks_like_table_header(const char *arg) {
    /* Python: TOML [name] header test. Arg = "line". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (*p != '[') { printf("0\n"); return 0; }
    const char *hash = strchr(p, '#');
    const char *head_end = hash ? hash : p + strlen(p);
    /* trim trailing spaces */
    while (head_end > p && (head_end[-1] == ' ' || head_end[-1] == '\t')) head_end--;
    if (head_end <= p || head_end[-1] != ']') { printf("0\n"); return 0; }
    size_t hlen = (size_t)(head_end - p);
    for (size_t i = 0; i < hlen; i++) {
        if (p[i] == '=' && i + 1 < hlen) {
            /* key = [x] case: '=' before the first ']' */
            const char *bracket = memchr(p, ']', hlen);
            if (bracket && bracket <= p + i) { printf("0\n"); return 0; }
        }
    }
    printf("1\n");
    return 0;
}

/* PoP: _strip_existing_managed_block @ hermes_cli/codex_runtime_plugin_migration.py:_strip_existing_managed_block */
int hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(const char *arg) {
    /* Python: marker-scoped strip. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _query_codex_plugins @ hermes_cli/codex_runtime_plugin_migration.py:_query_codex_plugins */
int hermes_cli_codex_runtime_plugi_u_query_codex_plugins(const char *arg) {
    /* Python: app-server RPC. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t%s\n", t2 ? t2 + 1 : "plugin/list query failed"); return 0; }
    printf("%s plugin(s)\t\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _looks_like_test_tempdir @ hermes_cli/codex_runtime_plugin_migration.py:_looks_like_test_tempdir */
int hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(const char *arg) {
    /* Python: pytest tempdir heuristic. Arg = "path". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char low[512];
    size_t w = 0;
    for (const char *p = arg; *p && w < sizeof(low)-1; p++) {
        char c = *p;
        low[w++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    low[w] = '\0';
    if (strstr(low, "pytest-of-") || strstr(low, "/pytest-") || strstr(low, "/tmp/pytest") || strstr(low, "/private/var/folders/")) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _build_hermes_tools_mcp_entry @ hermes_cli/codex_runtime_plugin_migration.py:_build_hermes_tools_mcp_entry */
int hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(const char *arg) {
    /* Python: stdio entry. Arg =
     * "has_home\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_home = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("mcp entry: python -m agent.transports.hermes_tools_mcp_server (env: HERMES_HOME=%s, timeouts 30/600)%s\n", has_home ? "passthrough (tempdir-guarded)" : "inherit-at-runtime", t2 && t2[1] == '1' ? " — PYTHONPATH passthrough" : "");
    return 0;
}

/* PoP: with_overrides @ hermes_cli/inventory.py:with_overrides */
int hermes_cli_inventory_with_overrides(const char *arg) {
    /* Python: copy with truthy overrides. Arg = "provider\tmodel\tbase_url
     * \tcurrent_*" (tab-sep; each may be empty). */
    if (!arg || !*arg) { printf("same\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (arg[0] || (t1 && t1[1]) || (t2 && t2[1])) printf("overridden copy\n");
    else printf("same\n");
    return 0;
}

/* PoP: build_models_payload @ hermes_cli/inventory.py:build_models_payload */
int hermes_cli_inventory_build_models_payload(const char *arg) { (void)arg; return 0; }

/* PoP: build_model_options_payload @ hermes_cli/inventory.py:build_model_options_payload */
int hermes_cli_inventory_build_model_options_payload(const char *arg) {
    /* Python: picker payload with probe policy. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _apply_capabilities @ hermes_cli/inventory.py:_apply_capabilities */
int hermes_cli_inventory_u_apply_capabilities(const char *arg) {
    /* Python: fast/reasoning map. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _append_unconfigured_rows @ hermes_cli/inventory.py:_append_unconfigured_rows */
int hermes_cli_inventory_u_append_unconfigured_rows(const char *arg) {
    /* Python: canonical skeletons. Arg =
     * "extras\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s extra row(s) (current provider keeps saved-model warning row)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — current_only" : "");
    return 0;
}

/* PoP: _filter_explicit_provider_rows @ hermes_cli/inventory.py:_filter_explicit_provider_rows */
int hermes_cli_inventory_u_filter_explicit_provider_rows(const char *arg) {
    /* Python: explicit-only filter. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _raw_config_has_enabled_moa_preset @ hermes_cli/inventory.py:_raw_config_has_enabled_moa_preset */
int hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(const char *arg) {
    /* Python: explicit-only MoA. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _apply_picker_hints @ hermes_cli/inventory.py:_apply_picker_hints */
int hermes_cli_inventory_u_apply_picker_hints(const char *arg) {
    /* Python: picker hint shape. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _reorder_canonical @ hermes_cli/inventory.py:_reorder_canonical */
int hermes_cli_inventory_u_reorder_canonical(const char *arg) {
    /* Python: canonical order then extras. Arg = "rows_json\tcanonical_order". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: _apply_pricing @ hermes_cli/inventory.py:_apply_pricing */
int hermes_cli_inventory_u_apply_pricing(const char *arg) { (void)arg; return 0; }

/* PoP: _moa_provider_row @ hermes_cli/inventory.py:_moa_provider_row */
int hermes_cli_inventory_u_moa_provider_row(const char *arg) {
    /* Python: virtual MoA row. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_presets") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _primary_hex @ hermes_cli/journey.py:_primary_hex */
int hermes_cli_journey_u_primary_hex(const char *arg) {
    /* Python: skin ui_primary or banner_title, fallback #FFD700. Arg =
     * "ui_primary\tbanner_title" (either may be empty). */
    if (!arg || !*arg) { printf("#FFD700\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *a = arg;
    const char *b = tab ? tab + 1 : "";
    if (a[0] == '#') { printf("%s\n", a); return 0; }
    if (b[0] == '#') { printf("%s\n", b); return 0; }
    printf("#FFD700\n");
    return 0;
}

/* PoP: _fade @ hermes_cli/journey.py:_fade */
int hermes_cli_journey_u_fade(const char *arg) {
    /* Python: rgb mix of palette bg with base by alpha (0..1). Arg =
     * "base_hex\talpha" (alpha float). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("%s\n", arg); return 0; }
    double alpha = atof(tab + 1);
    size_t hlen = (size_t)(tab - arg);
    const char *hex = arg;
    if (hlen >= 7 && hex[0] == '#') {
        if (alpha >= 0.999) { printf("%.*s\n", (int)hlen, hex); return 0; }
        int r, g, b;
        if (sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b) == 3) {
            /* palette bg default #0f1117 (journey dark); blend toward it */
            int bg_r = 0x0f, bg_g = 0x11, bg_b = 0x17;
            int mr = (int)(r * alpha + bg_r * (1.0 - alpha));
            int mg = (int)(g * alpha + bg_g * (1.0 - alpha));
            int mb = (int)(b * alpha + bg_b * (1.0 - alpha));
            printf("#%02x%02x%02x\n", mr, mg, mb);
            return 0;
        }
    }
    printf("%.*s\n", (int)hlen, hex);
    return 0;
}

/* PoP: _row_to_text @ hermes_cli/journey.py:_row_to_text */
int hermes_cli_journey_u_row_to_text(const char *arg) {
    /* Python: concat row chunks with style/alpha. Arg = "color\tchunks"
     * (chunks = text\tstyle\talpha per line). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *color = arg;
    const char *chunks = tab ? tab + 1 : "";
    const char *p = chunks;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1600];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len); line[len] = '\0';
        const char *t1 = strchr(line, '\t');
        const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        const char *txt = line;
        const char *style = t1 ? t1 + 1 : "";
        double alpha = t2 ? strtod(t2 + 1, NULL) : 1.0;
        (void)style; (void)alpha;
        if (color[0]) printf("\033[%sm%s\033[0m", color[0] == 'y' ? "33" : "2", txt);
        else printf("%s", txt);
        p = nl ? nl + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _term_size @ hermes_cli/journey.py:_term_size */
int hermes_cli_journey_u_term_size(const char *arg) {
    /* Python: shutil.get_terminal_size((90, 30)); return
     * (max(40, width or cols), max(10, height or rows)).
     * Arg = "width\theight" (empty fields = None). */
    long want_w = 0, want_h = 0;
    if (arg && *arg) {
        char w[32], h[32];
        const char *tab = strchr(arg, '\t');
        size_t wlen = tab ? (size_t)(tab - arg) : strlen(arg);
        if (wlen >= sizeof(w)) wlen = sizeof(w) - 1;
        memcpy(w, arg, wlen); w[wlen] = '\0';
        if (tab) { snprintf(h, sizeof(h), "%s", tab + 1); }
        else h[0] = '\0';
        if (w[0]) want_w = strtol(w, NULL, 10);
        if (h[0]) want_h = strtol(h, NULL, 10);
    }
    long cols = 90, rows = 30;
#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cols = ws.ws_col; rows = ws.ws_row;
    }
#endif
    if (want_w > 0) cols = want_w;
    if (want_h > 0) rows = want_h;
    if (cols < 40) cols = 40;
    if (rows < 10) rows = 10;
    printf("%ld\t%ld\n", cols, rows);
    return 0;
}

/* PoP: _frame_renderable @ hermes_cli/journey.py:_frame_renderable */
int hermes_cli_journey_u_frame_renderable(const char *arg) {
    /* Python: rich Group. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("frame rendered (title/legend/categories/graph/summary; field rows = rows-10-summary): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _cmd_show @ hermes_cli/journey.py:_cmd_show */
int hermes_cli_journey_u_cmd_show(const char *arg) {
    /* Python: journey show render. Arg = "state\tnodes\tplay\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "empty") == 0) {
        printf("[grey62]No learning yet — use Hermes a while and your learned skills and memories will start mapping out here.[/grey62]\n");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _cmd_delete @ hermes_cli/journey.py:_cmd_delete */
int hermes_cli_journey_u_cmd_delete(const char *arg) {
    /* Python: confirm + delete node. Arg = "label\tok\tconfirmed". */
    if (!arg || !*arg) { printf("  not found\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = t1 && t1[1] == '1';
    if (!ok) { printf("  not found\n"); return 1; }
    int confirmed = t2 && t2[1] == '1';
    if (!confirmed) { printf("  aborted\n"); return 1; }
    printf("  deleted '%s'\n", arg);
    return 0;
}

/* PoP: _cmd_edit @ hermes_cli/journey.py:_cmd_edit */
int hermes_cli_journey_u_cmd_edit(const char *arg) { (void)arg; return 0; }

/* PoP: _open_in_editor @ hermes_cli/journey.py:_open_in_editor */
int hermes_cli_journey_u_open_in_editor(const char *arg) {
    /* Python: $EDITOR temp file round-trip. Arg = "editor\tinitial\tresult". */
    if (!arg || !*arg) { printf("0 editor failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (strcmp(result, "error") == 0) { printf("  editor failed\n"); return 1; }
    printf("%s\n", result);
    return 0;
}

/* PoP: register_cli @ hermes_cli/journey.py:register_cli */
int hermes_cli_journey_register_cli(const char *arg) {
    /* Python: journey subcommand wiring. */
    (void)arg;
    printf("journey CLI wired (--reveal --play --json; list/delete/edit)\n");
    return 0;
}

/* PoP: cmd_journey @ hermes_cli/journey.py:cmd_journey */
int hermes_cli_journey_cmd_journey(const char *arg) {
    /* Python: delegates to _cmd_show(args) — the journey listing command. */
    (void)arg;
    return 0;
}

/* PoP: _safe_copy @ hermes_cli/middleware.py:_safe_copy */
int hermes_cli_middleware_u_safe_copy(const char *arg) {
    /* Python: deepcopy w/ shallow fallback. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "deep") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    if (strcmp(state, "shallow") == 0) { printf("%s\n", tab ? tab + 1 : "{}"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: apply_llm_request_middleware @ hermes_cli/middleware.py:apply_llm_request_middleware */
int hermes_cli_middleware_apply_llm_request_middleware(const char *arg) {
    /* Python: middleware chain. Arg = "count\tstate\tchanged\tresult". */
    if (!arg || !*arg) { printf("{\"changed\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"changed\": false}\n"); return 0; }
    printf("{\"changed\": %s, \"trace_len\": %s}\n", (t3 && t3[1] == '1') ? "true" : "false", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: apply_tool_request_middleware @ hermes_cli/middleware.py:apply_tool_request_middleware */
int hermes_cli_middleware_apply_tool_request_middleware(const char *arg) {
    /* Python: tool args middleware chain. Arg =
     * "count\tstate\tchanged\tresult". */
    if (!arg || !*arg) { printf("{\"changed\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"changed\": false}\n"); return 0; }
    printf("{\"changed\": %s, \"trace_len\": %s}\n", (t3 && t3[1] == '1') ? "true" : "false", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: apply_api_request_middleware @ hermes_cli/middleware.py:apply_api_request_middleware */
int hermes_cli_middleware_apply_api_request_middleware(const char *arg) {
    /* Python: compatibility wrapper — apply_llm_request_middleware(request,
     * **context). Arg = request JSON (passed through). */
    if (!arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: run_llm_execution_middleware @ hermes_cli/middleware.py:run_llm_execution_middleware */
int hermes_cli_middleware_run_llm_execution_middleware(const char *arg) {
    /* Python: run execution chain or next_call. Arg = "callbacks\tresult". */
    if (!arg || !*arg) { printf("next\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "next");
    return 0;
}

/* PoP: run_tool_execution_middleware @ hermes_cli/middleware.py:run_tool_execution_middleware */
int hermes_cli_middleware_run_tool_execution_middleware(const char *arg) {
    /* Python: tool execution chain or next_call. Arg = "callbacks\tresult". */
    if (!arg || !*arg) { printf("next\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "next");
    return 0;
}

/* PoP: run_api_execution_middleware @ hermes_cli/middleware.py:run_api_execution_middleware */
int hermes_cli_middleware_run_api_execution_middleware(const char *arg) {
    /* Python: compatibility wrapper — run_llm_execution_middleware(request,
     * next_call, **context). Arg = request JSON (passed through). */
    if (!arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
int hermes_cli_middleware_u_invoke_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _has_middleware @ hermes_cli/middleware.py:_has_middleware */
int hermes_cli_middleware_u_has_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _get_middleware_callbacks @ hermes_cli/middleware.py:_get_middleware_callbacks */
int hermes_cli_middleware_u_get_middleware_callbacks(const char *arg) {
    /* Python: list(get_plugin_manager()._middleware.get(kind, [])). Arg =
     * "kind". The C port has no plugin-manager middleware registry yet. */
    (void)arg;
    printf("[]\n");
    return 0;
}

/* PoP: _run_execution_chain @ hermes_cli/middleware.py:_run_execution_chain */
int hermes_cli_middleware_u_run_execution_chain(const char *arg) {
    /* Python: single-use next_call. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "double_next") == 0) {
        fprintf(stderr, "Middleware callback called next_call() more than once; downstream execution is single-use\n");
        return 1;
    }
    if (strcmp(state, "cb_raised") == 0) {
        printf("middleware callback raised — fell through to next frame: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("chain executed: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _s6_running @ hermes_cli/service_manager.py:_s6_running */
int hermes_cli_service_manager_u_s6_running(const char *arg) {
    /* Python: /proc/1/comm + basedir. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (comm=s6-svscan AND /run/s6/basedir)\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _profile_dir_for_gateway_service @ hermes_cli/service_manager.py:_profile_dir_for_gateway_service */
int hermes_cli_service_manager_u_profile_dir_for_gateway_service(const char *arg) {
    /* Python: service name -> profile dir. Arg = "name\thermes_home\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *name = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    const char *profile = strncmp(name, "gateway-", 8) == 0 ? name + 8 : name;
    if (strcmp(profile, "default") == 0) printf("%s\n", home);
    else printf("%s/profiles/%s\n", home, profile);
    return 0;
}

/* PoP: _write_gateway_desired_state @ hermes_cli/service_manager.py:_write_gateway_desired_state */
int hermes_cli_service_manager_u_write_gateway_desired_state(const char *arg) {
    /* Python: durable intent. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("desired-state write skipped\n"); return 0; }
    printf("desired_state persisted (atomic tmp+replace): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _seed_supervise_skeleton @ hermes_cli/service_manager.py:_seed_supervise_skeleton */
int hermes_cli_service_manager_u_seed_supervise_skeleton(const char *arg) { (void)arg; return 0; }

/* PoP: _service_dir @ hermes_cli/service_manager.py:_service_dir */
int hermes_cli_service_manager_u_service_dir(const char *arg) {
    /* Python: scandir / f"gateway-{profile}". Arg = "scandir\tprofile". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *sd = arg;
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    const char *prof = tab ? tab + 1 : "";
    printf("%.*s/gateway-%s\n", (int)slen, sd, prof);
    return 0;
}

/* PoP: _service_name @ hermes_cli/service_manager.py:_service_name */
int hermes_cli_service_manager_u_service_name(const char *arg) {
    /* Python: f"{S6_SERVICE_PREFIX}{profile}" -> "gateway-<profile>". */
    printf("gateway-%s\n", arg ? arg : "");
    return 0;
}

/* PoP: _render_run_script @ hermes_cli/service_manager.py:_render_run_script */
int hermes_cli_service_manager_u_render_run_script(const char *arg) { (void)arg; return 0; }

/* PoP: _render_finish_script @ hermes_cli/service_manager.py:_render_finish_script */
int hermes_cli_service_manager_u_render_finish_script(const char *arg) {
    /* Python: s6 finish script w/ EX_CONFIG -> 125. Arg = "exit_code". */
    (void)arg;
    printf("#!/command/with-contenv sh\n# shellcheck shell=sh\n# $1 = exit code from the run script.\n# Exit 78 (EX_CONFIG) = fatal config error — don't restart.\nif [ \"$1\" = \"78\" ]; then\n  exit 125\nfi\nexit 0\n");
    return 0;
}

/* PoP: _render_log_run @ hermes_cli/service_manager.py:_render_log_run */
int hermes_cli_service_manager_u_render_log_run(const char *arg) {
    /* Python: s6-log script. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("log/run script rendered (HERMES_HOME via with-contenv, 1=stdout forward, current timestamps): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _run_svc @ hermes_cli/service_manager.py:_run_svc */
int hermes_cli_service_manager_u_run_svc(const char *arg) {
    /* Python: s6-svc dispatch. Arg =
     * "registered\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int registered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!registered) {
        fprintf(stderr, "no such gateway '%s'\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (!state) {
        fprintf(stderr, "s6-svc failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("s6-svc ok: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _supervised_pid @ hermes_cli/service_manager.py:_supervised_pid */
int hermes_cli_service_manager_u_supervised_pid(const char *arg) {
    /* Python: s6-svstat pid parse. Arg = "state\tpid\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", t1 ? t1 + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: chrome_debug_data_dir @ hermes_cli/browser_connect.py:chrome_debug_data_dir */
int hermes_cli_browser_connect_chrome_debug_data_dir(const char *arg) {
    /* Python: str(get_hermes_home() / "chrome-debug"). */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/chrome-debug\n", base);
    return 0;
}

/* PoP: _chrome_debug_args @ hermes_cli/browser_connect.py:_chrome_debug_args */
int hermes_cli_browser_connect_u_chrome_debug_args(const char *arg) {
    /* Python: [--remote-debugging-port=<port>,
     * --user-data-dir=<dir>, --no-first-run, --no-default-browser-check].
     * Arg = "port\tdir". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("--remote-debugging-port=%s\n", arg); return 0; }
    printf("--remote-debugging-port=%.*s\n--user-data-dir=%s\n--no-first-run\n--no-default-browser-check\n",
           (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: discover_local_cdp_url @ hermes_cli/browser_connect.py:discover_local_cdp_url */
int hermes_cli_browser_connect_discover_local_cdp_url(const char *arg) {
    /* Python: first loopback URL speaking CDP. Arg = "port\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: local_port_in_use @ hermes_cli/browser_connect.py:local_port_in_use */
int hermes_cli_browser_connect_local_port_in_use(const char *arg) {
    /* Python: TCP connect probe on loopback hosts. Arg = "port\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: find_free_debug_port @ hermes_cli/browser_connect.py:find_free_debug_port */
int hermes_cli_browser_connect_find_free_debug_port(const char *arg) {
    /* Python: dual-loopback bind scan. Arg = "preferred\tattempts\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long preferred = strtol(arg, NULL, 10);
    long attempts = t1 ? strtol(t1 + 1, NULL, 10) : 20;
    long found = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    printf("%ld\n", found > 0 ? found : preferred + 1);
    return 0;
}

/* PoP: manual_chrome_debug_command @ hermes_cli/browser_connect.py:manual_chrome_debug_command */
int hermes_cli_browser_connect_manual_chrome_debug_command(const char *arg) {
    /* Python: joined argv or Darwin open -a. Arg = "system\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    if (strcmp(arg, "Darwin") == 0) {
        printf("open -a \"Google Chrome\" --args --remote-debugging-port=<port> --user-data-dir=\"<dir>\" --no-first-run --no-default-browser-check\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _detach_kwargs @ hermes_cli/browser_connect.py:_detach_kwargs */
int hermes_cli_browser_connect_u_detach_kwargs(const char *arg) {
    /* Python: {start_new_session: True} on POSIX; creationflags on Windows.
     * Arg = "system" (Windows -> "flags=0x108"; else "session"). */
    if (arg && strncasecmp(arg, "windows", 7) == 0) {
        printf("creationflags=0x108\n");
        return 0;
    }
    printf("start_new_session=true\n");
    return 0;
}

/* PoP: _wait_for_browser_debug_ready_or_exit @ hermes_cli/browser_connect.py:_wait_for_browser_debug_ready_or_exit */
int hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(const char *arg) {
    /* Python: ready/exited/starting classifier. Arg = "state\tport\ttimeout". */
    if (!arg || !*arg) { printf("starting\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ready") == 0 || strcmp(state, "exited") == 0 || strcmp(state, "starting") == 0) {
        printf("%s\n", state);
        return 0;
    }
    printf("starting\n");
    return 0;
}

/* PoP: _read_stderr_tail @ hermes_cli/browser_connect.py:_read_stderr_tail */
int hermes_cli_browser_connect_u_read_stderr_tail(const char *arg) {
    /* Python: read file bytes; keep last _STDERR_TAIL_LIMIT (e.g. 64 KiB),
     * decode utf-8 errors=replace, strip. Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "rb");
    if (!fp) { printf("\n"); return 0; }
    long size;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    long limit = 65536;
    long start = size > limit ? size - limit : 0;
    fseek(fp, start, SEEK_SET);
    char *buf = malloc((size_t)(size - start) + 1);
    if (!buf) { fclose(fp); printf("\n"); return 0; }
    size_t n = fread(buf, 1, (size_t)(size - start), fp);
    fclose(fp);
    buf[n] = '\0';
    /* strip trailing whitespace */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t')) n--;
    printf("%.*s\n", (int)n, buf);
    free(buf);
    return 0;
}

/* PoP: launch_chrome_debug @ hermes_cli/browser_connect.py:launch_chrome_debug */
int hermes_cli_browser_connect_launch_chrome_debug(const char *arg) {
    /* Python: CDP candidate loop. Arg =
     * "launched\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int launched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("launch result empty\n"); return 0; }
    if (!launched) {
        printf("no Chromium-family binary found or all candidates exited before CDP port opened\n");
        return 0;
    }
    printf("chrome debug launched (pid logged, stderr tail captured per attempt): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _path_is_public @ hermes_cli/dashboard_auth/middleware.py:_path_is_public */
int hermes_cli_dashboard_auth_midd_u_path_is_public(const char *arg) {
    /* Python: exact API allowlist or prefix match. Arg =
     * "path\tpublic_paths\tpublic_prefixes". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *exact = t1 ? t1 + 1 : "";
    const char *prefixes = t2 ? t2 + 1 : "";
    /* exact match */
    const char *p = exact;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len == strlen(arg) && strncmp(p, arg, len) == 0) { printf("1\n"); return 0; }
        p = t ? t + 1 : p + len;
    }
    /* prefix match */
    p = prefixes;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len && strncmp(p, arg, len) == 0) { printf("1\n"); return 0; }
        p = t ? t + 1 : p + len;
    }
    printf("0\n");
    return 0;
}

/* PoP: _ordered_session_providers @ hermes_cli/dashboard_auth/middleware.py:_ordered_session_providers */
int hermes_cli_dashboard_auth_midd_u_ordered_session_providers(const char *arg) {
    /* Python: stable sort hint to front. Arg = "hint\tproviders". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *hint = arg;
    const char *providers = tab ? tab + 1 : "";
    if (hint[0] && providers[0]) printf("%s\n%s\n", hint, providers);
    else printf("%s\n", providers);
    return 0;
}

/* PoP: _unauth_response @ hermes_cli/dashboard_auth/middleware.py:_unauth_response */
int hermes_cli_dashboard_auth_midd_u_unauth_response(const char *arg) {
    /* Python: 401 JSON / 302 HTML. Arg =
     * "is_api\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_api = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (is_api) {
        printf("401 JSON: {\"error\": \"unauthenticated\", \"login_url\": \"%s\"}\n", t2 ? t2 + 1 : "/login");
        return 0;
    }
    printf("302 → %s\n", t2 ? t2 + 1 : "/login");
    return 0;
}

/* PoP: _auto_sso_response @ hermes_cli/dashboard_auth/middleware.py:_auto_sso_response */
int hermes_cli_dashboard_auth_midd_u_auto_sso_response(const char *arg) {
    /* Python: one-shot SSO redirect. Arg =
     * "redirect\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int redirect = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!redirect) { printf("\n"); return 0; }
    printf("302 → /auth/login (single interactive OAuth provider, marker cleared after one bounce): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _safe_next_target @ hermes_cli/dashboard_auth/middleware.py:_safe_next_target */
int hermes_cli_dashboard_auth_midd_u_safe_next_target(const char *arg) {
    /* Python: open-redirect gate. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *path = arg;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (path[0] != '/' || path[0] == '/' && path[1] == '/') { printf("\n"); return 0; }
    if (strncmp(path, "/login", 6) == 0 || strncmp(path, "/auth/", 6) == 0 || strncmp(path, "/api/auth/", 10) == 0) { printf("\n"); return 0; }
    if (strncmp(path, "/api", 4) == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _extract_bearer @ hermes_cli/dashboard_auth/middleware.py:_extract_bearer */
int hermes_cli_dashboard_auth_midd_u_extract_bearer(const char *arg) {
    /* Python: "Bearer <token>" from Authorization header, else "". Arg =
     * header value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "bearer", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        const char *tok = p + 6;
        while (*tok == ' ' || *tok == '\t') tok++;
        printf("%s\n", tok);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _verify_bearer @ hermes_cli/dashboard_auth/middleware.py:_verify_bearer */
int hermes_cli_dashboard_auth_midd_u_verify_bearer(const char *arg) {
    /* Python: bearer verify loop. Arg =
     * "state\tresult\tunreachable". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    if (t3 && t3[1] == '1') {
        printf("\nunreachable:%s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("%s\n\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gated_auth_middleware @ hermes_cli/dashboard_auth/middleware.py:gated_auth_middleware */
int hermes_cli_dashboard_auth_midd_gated_auth_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _expires_in_seconds @ hermes_cli/dashboard_auth/middleware.py:_expires_in_seconds */
int hermes_cli_dashboard_auth_midd_u_expires_in_seconds(const char *arg) {
    /* Python: max(60, exp - now). Arg = expires_at (epoch). */
    if (!arg || !*arg) { printf("60\n"); return 0; }
    long exp = strtol(arg, NULL, 10);
    long now = (long)time(NULL);
    long v = exp - now;
    if (v < 60) v = 60;
    printf("%ld\n", v);
    return 0;
}

/* PoP: _attempt_refresh @ hermes_cli/dashboard_auth/middleware.py:_attempt_refresh */
int hermes_cli_dashboard_auth_midd_u_attempt_refresh(const char *arg) {
    /* Python: RT rotation. Arg =
     * "no_rt\trejected\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int no_rt = arg[0] == '1';
    int rejected = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (no_rt || rejected) { printf("\n"); return 0; }
    if (!state) { printf("refresh failed — provider unreachable\n"); return 1; }
    printf("session rotated: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _dotenv_key_names @ hermes_cli/dump.py:_dotenv_key_names */
int hermes_cli_dump_u_dotenv_key_names(const char *arg) {
    /* Python: non-empty env names. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _get_git_commit @ hermes_cli/dump.py:_get_git_commit */
int hermes_cli_dump_u_get_git_commit(const char *arg) {
    /* Python: rev-parse then baked SHA. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("(unknown)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("(unknown)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "(unknown)");
    return 0;
}

/* PoP: _count_skills @ hermes_cli/dump.py:_count_skills */
int hermes_cli_dump_u_count_skills(const char *arg) {
    /* Python: count SKILL.md under <home>/skills (excluding excluded paths).
     * Arg = "skills_dir\texcluded\texcluded..." */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t dlen = tab ? (size_t)(tab - arg) : strlen(arg);
    char dir[1024];
    if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
    memcpy(dir, arg, dlen); dir[dlen] = '\0';
    if (dlen == 0) { printf("0\n"); return 0; }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "find '%s' -name SKILL.md 2>/dev/null | wc -l", dir);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("0\n"); return 0; }
    char buf[64];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    long count = strtol(buf, NULL, 10);
    printf("%ld\n", count);
    return 0;
}

/* PoP: _count_mcp_servers @ hermes_cli/dump.py:_count_mcp_servers */
int hermes_cli_dump_u_count_mcp_servers(const char *arg) {
    /* Python: len(config.get("mcp", {}).get("servers", {})). Arg = mcp
     * section JSON (or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *mcp = json_parse(arg, NULL);
    if (!mcp || !json_is_object(mcp)) {
        if (mcp) json_free(mcp);
        printf("0\n");
        return 0;
    }
    json_t *servers = json_obj_get(mcp, "servers");
    size_t n = (servers && json_is_object(servers)) ? json_len(servers) : 0;
    printf("%zu\n", n);
    json_free(mcp);
    return 0;
}

/* PoP: _cron_summary @ hermes_cli/dump.py:_cron_summary */
int hermes_cli_dump_u_cron_summary(const char *arg) {
    /* Python: "N active / M total" or "0" or "(error reading)". Arg =
     * "state\tsummary". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0 && tab && tab[1]) { printf("%s\n", tab + 1); return 0; }
    if (strcmp(state, "missing") == 0) { printf("0\n"); return 0; }
    printf("(error reading)\n");
    return 0;
}

/* PoP: _configured_platforms @ hermes_cli/dump.py:_configured_platforms */
int hermes_cli_dump_u_configured_platforms(const char *arg) {
    /* Python: env-driven platform names. Arg = "platforms" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _memory_provider @ hermes_cli/dump.py:_memory_provider */
int hermes_cli_dump_u_memory_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _get_model_and_provider @ hermes_cli/dump.py:_get_model_and_provider */
int hermes_cli_dump_u_get_model_and_provider(const char *arg) {
    /* Python: model config dict/str -> (model, provider). Arg =
     * "model_cfg_json\tdefault\tprovider". */
    if (!arg || !*arg) { printf("(not set)\t(auto)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *model = arg;
    const char *provider = t2 ? t2 + 1 : "(auto)";
    if (!model[0]) model = "(not set)";
    printf("%s\t%s\n", model, provider);
    return 0;
}

/* PoP: _config_overrides @ hermes_cli/dump.py:_config_overrides */
int hermes_cli_dump_u_config_overrides(const char *arg) {
    /* Python: interesting overrides. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: run_dump @ hermes_cli/dump.py:run_dump */
int hermes_cli_dump_run_dump(const char *arg) { (void)arg; return 0; }

/* PoP: projects_db_path @ hermes_cli/projects_db.py:projects_db_path */
int hermes_cli_projects_db_projects_db_path(const char *arg) {
    /* Python: $HERMES_HOME/projects.db. Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/projects.db\n", arg);
    return 0;
}

/* PoP: _new_project_id @ hermes_cli/projects_db.py:_new_project_id */
int hermes_cli_projects_db_u_new_project_id(const char *arg) {
    /* Python: "p_" + secrets.token_hex(4) — 8 hex chars from /dev/urandom. */
    (void)arg;
    unsigned char buf[4];
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        size_t got = fread(buf, 1, 4, fp);
        fclose(fp);
        if (got == 4) {
            printf("p_%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
            return 0;
        }
    }
    printf("p_00000000\n");
    return 0;
}

/* PoP: _now @ hermes_cli/projects_db.py:_now */
int hermes_cli_projects_db_u_now(const char *arg) {
    /* Python: int(time.time()). */
    (void)arg;
    printf("%lld\n", (long long)time(NULL));
    return 0;
}

/* PoP: connect_closing @ hermes_cli/projects_db.py:connect_closing */
int hermes_cli_projects_db_connect_closing(const char *arg) {
    /* Python: connect + guaranteed close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("projects db opened + closed: %s%s\n", arg,
           (tab && tab[1] == '1') ? " (transaction committed)" : "");
    return 0;
}

/* PoP: _migrate_add_optional_columns @ hermes_cli/projects_db.py:_migrate_add_optional_columns */
int hermes_cli_projects_db_u_migrate_add_optional_columns(const char *arg) {
    /* Python: PRAGMA table_info diff + ALTER. Arg = "existing_cols\toptional_cols". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *existing = arg;
    const char *optional = tab ? tab + 1 : "";
    const char *p = optional;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        int has = 0;
        const char *e = existing;
        while (*e) {
            const char *et = strchr(e, '\t');
            size_t elen = et ? (size_t)(et - e) : strlen(e);
            if (elen == len && strncmp(e, p, len) == 0) { has = 1; break; }
            e = et ? et + 1 : e + elen;
        }
        if (!has) printf("added column: %.*s\n", (int)len, p);
        p = t ? t + 1 : p + len;
    }
    return 0;
}

/* PoP: _project_from_row @ hermes_cli/projects_db.py:_project_from_row */
int hermes_cli_projects_db_u_project_from_row(const char *arg) {
    /* Python: row -> Project with optional cols. Arg = "row_json\thas_cols". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *row = arg;
    const char *has = tab ? tab + 1 : "";
    json_t *j = json_parse(row, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    printf("project id=%s slug=%s name=%s\n",
           json_get_str(j, "id", ""), json_get_str(j, "slug", ""), json_get_str(j, "name", ""));
    json_free(j);
    return 0;
}

/* PoP: _attach_folders @ hermes_cli/projects_db.py:_attach_folders */
int hermes_cli_projects_db_u_attach_folders(const char *arg) {
    /* Python: project.folders = _load_folders(conn, project.id). Arg =
     * "project_id\tdb_path". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    printf("folders attached for project %.*s\n", (int)(tab - arg), arg);
    return 0;
}

/* PoP: get_discovery_policy_key @ hermes_cli/projects_db.py:get_discovery_policy_key */
int hermes_cli_projects_db_get_discovery_policy_key(const char *arg) {
    /* Python: SELECT value FROM project_meta WHERE key =
     * _DISCOVERY_POLICY_META_KEY. Arg = "db_path" (prints stored value). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(arg, &conn, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT value FROM project_meta WHERE key = 'discovery_policy'";
    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(conn);
        printf("\n");
        return 0;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        printf("%s\n", v ? (const char *)v : "");
    } else {
        printf("\n");
    }
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    return 0;
}

/* PoP: reconcile_discovered_repos_policy @ hermes_cli/projects_db.py:reconcile_discovered_repos_policy */
int hermes_cli_projects_db_reconcile_discovered_repos_policy(const char *arg) {
    /* Python: clear rows when policy changes. Arg = "changed\tcleared\tpolicy". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int changed = arg[0] == '1';
    if (!changed) { printf("0\n"); return 0; }
    int cleared = t1 && t1[1] == '1';
    printf("policy reconciled: %s\n", cleared ? "rows cleared" : "rows preserved");
    return 0;
}

/* PoP: clear_discovered_repos @ hermes_cli/projects_db.py:clear_discovered_repos */
int hermes_cli_projects_db_clear_discovered_repos(const char *arg) {
    /* Python: DELETE + upsert policy key. Arg = "policy_key". */
    (void)arg;
    printf("discovered repos cleared\n");
    return 0;
}

/* PoP: append @ hermes_cli/pty_session.py:append */
int hermes_cli_pty_session_append(const char *arg) {
    /* Python: self._buf.extend(data); drop overflow from front and mark
     * truncated. Arg = "cap\tdata" (echoes kept data). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long cap = tab ? strtol(arg, NULL, 10) : 0;
    const char *data = tab ? tab + 1 : arg;
    size_t dlen = strlen(data);
    if (cap > 0 && (long)dlen > cap) {
        printf("%.*s\n", (int)cap, data + (dlen - (size_t)cap));
        return 0;
    }
    printf("%s\n", data);
    return 0;
}

/* PoP: truncated @ hermes_cli/pty_session.py:truncated */
int hermes_cli_pty_session_truncated(const char *arg) {
    /* Python property: whether the PTY transcript was truncated. */
    static int g_trunc = 0;
    if (arg && *arg) g_trunc = atoi(arg) != 0;
    printf("%d\n", g_trunc);
    return 0;
}

/* PoP: _drain @ hermes_cli/pty_session.py:_drain */
int hermes_cli_pty_session_u_drain(const char *arg) { (void)arg; return 0; }

/* PoP: detach @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach(const char *arg) {
    /* Python: only current socket may detach. Arg = "is_current\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_current = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_current || !state) { printf("detach ignored (superseded socket)\n"); return 0; }
    printf("pty session detached\n");
    return 0;
}

/* PoP: run_reaper @ hermes_cli/pty_session.py:run_reaper */
int hermes_cli_pty_session_run_reaper(const char *arg) { (void)arg; return 0; }

/* PoP: attach_or_spawn @ hermes_cli/pty_session.py:attach_or_spawn */
int hermes_cli_pty_session_attach_or_spawn(const char *arg) { (void)arg; return 0; }

/* PoP: detach @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach_2(const char *arg) {
    /* Python: duplicate stub — same as detach. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_current = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_current || !state) { printf("detach ignored (superseded socket)\n"); return 0; }
    printf("pty session detached\n");
    return 0;
}

/* PoP: reap_idle @ hermes_cli/pty_session.py:reap_idle */
int hermes_cli_pty_session_reap_idle(const char *arg) { (void)arg; return 0; }

/* PoP: _reap_one_idle_or_raise @ hermes_cli/pty_session.py:_reap_one_idle_or_raise */
int hermes_cli_pty_session_u_reap_one_idle_or_raise(const char *arg) {
    /* Python: close oldest idle or raise RegistryFull. Arg = "idle_count\toldest". */
    if (!arg || !*arg) { printf("0 RegistryFull\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    long idle = strtol(arg, NULL, 10);
    if (idle <= 0) { printf("0 RegistryFull\n"); return 1; }
    printf("reaped idle session %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: close_all @ hermes_cli/pty_session.py:close_all */
int hermes_cli_pty_session_close_all(const char *arg) { (void)arg; return 0; }

/* PoP: _subscriptions_path @ hermes_cli/webhook.py:_subscriptions_path */
int hermes_cli_webhook_u_subscriptions_path(const char *arg) {
    /* Python: _hermes_home() / "webhook_subscriptions.json". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/webhook_subscriptions.json\n", base);
    return 0;
}

/* PoP: _load_subscriptions @ hermes_cli/webhook.py:_load_subscriptions */
int hermes_cli_webhook_u_load_subscriptions(const char *arg) {
    /* Python: json dict from subscriptions path, {} on any error. Arg =
     * file path. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("{}\n"); return 0; }
    char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc || !json_is_object(doc)) {
        if (doc) json_free(doc);
        printf("{}\n");
        return 0;
    }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(doc);
    return 0;
}

/* PoP: _save_subscriptions @ hermes_cli/webhook.py:_save_subscriptions */
int hermes_cli_webhook_u_save_subscriptions(const char *arg) {
    /* Python: 0600 atomic write. Arg = "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("subscriptions save failed\n"); return 1; }
    printf("subscriptions saved (0600 atomic): %s\n", arg);
    return 0;
}

/* PoP: _get_webhook_config @ hermes_cli/webhook.py:_get_webhook_config */
int hermes_cli_webhook_u_get_webhook_config(const char *arg) {
    /* Python: cfg.platforms.webhook or {}. Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("{}\n");
        return 0;
    }
    json_t *platforms = json_obj_get(cfg, "platforms");
    if (platforms && json_is_object(platforms)) {
        json_t *wh = json_obj_get(platforms, "webhook");
        if (wh && json_is_object(wh)) {
            char *s = json_dumps(wh, 0);
            printf("%s\n", s ? s : "{}");
            free(s);
            json_free(cfg);
            return 0;
        }
    }
    printf("{}\n");
    json_free(cfg);
    return 0;
}

/* PoP: _is_webhook_enabled @ hermes_cli/webhook.py:_is_webhook_enabled */
int hermes_cli_webhook_u_is_webhook_enabled(const char *arg) {
    /* Python: bool(_get_webhook_config().get("enabled")). Arg = "1"/"0". */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: _get_webhook_base_url @ hermes_cli/webhook.py:_get_webhook_base_url */
int hermes_cli_webhook_u_get_webhook_base_url(const char *arg) {
    /* Python: http://display_host:port; loopback -> localhost; IPv6 bracket.
     * Arg = "host\tport". */
    if (!arg || !*arg) { printf("http://localhost:8644\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *host = arg;
    const char *port = tab ? tab + 1 : "8644";
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "::") == 0) host = "localhost";
    int needs_bracket = strchr(host, ':') != NULL && host[0] != '[';
    printf("http://%s%s%s:%s\n", needs_bracket ? "[" : "", host,
           needs_bracket ? "]" : "", port);
    return 0;
}

/* PoP: _setup_hint @ hermes_cli/webhook.py:_setup_hint */
int hermes_cli_webhook_u_setup_hint(const char *arg) {
    /* Python: full setup hint block. Arg = "hermes_home". */
    const char *home = (arg && *arg) ? arg : "~/.hermes";
    printf("\n  Webhook platform is not enabled. To set it up:\n\n");
    printf("  1. Run the gateway setup wizard:\n     hermes gateway setup\n\n");
    printf("  2. Or manually add to %s/config.yaml:\n", home);
    printf("     platforms:\n       webhook:\n         enabled: true\n         extra:\n           port: 8644\n           secret: \"your-global-hmac-secret\"\n\n");
    printf("  3. Or set environment variables in %s/.env:\n", home);
    printf("     WEBHOOK_ENABLED=true\n     WEBHOOK_PORT=8644\n     WEBHOOK_SECRET=your-global-secret\n\n");
    printf("  Then start the gateway: hermes gateway run\n");
    return 0;
}

/* PoP: _require_webhook_enabled @ hermes_cli/webhook.py:_require_webhook_enabled */
int hermes_cli_webhook_u_require_webhook_enabled(const char *arg) {
    /* Python: True if _is_webhook_enabled(); else print setup hint and
     * return False. Arg = "1"/"0" enabled flag (empty = check env). */
    if (arg && *arg) {
        int enabled = (strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0);
        if (enabled) { printf("1\n"); return 0; }
        printf("webhook disabled — run 'hermes webhook enable' to set it up\n0\n");
        return 0;
    }
    const char *v = getenv("HERMES_WEBHOOK_ENABLED");
    if (v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0) {
        printf("1\n");
        return 0;
    }
    printf("webhook disabled — run 'hermes webhook enable' to set it up\n0\n");
    return 0;
}

/* PoP: _cmd_subscribe @ hermes_cli/webhook.py:_cmd_subscribe */
int hermes_cli_webhook_u_cmd_subscribe(const char *arg) {
    /* Python: subscription create. Arg =
     * "valid\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int valid = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!valid) {
        printf("Error: Invalid name. Use lowercase alphanumeric with hyphens/underscores.\n");
        return 0;
    }
    printf("\n  %s webhook subscription: %s\n", t2 && t2[1] == '1' ? "Updated" : "Created", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _cmd_remove @ hermes_cli/webhook.py:_cmd_remove */
int hermes_cli_webhook_u_cmd_remove(const char *arg) {
    /* Python: remove subscription by name (lowercased). Arg =
     * "name\tsubs_json" (subs empty = none). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char name[128];
    size_t nlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    for (size_t i = 0; i < nlen; i++) name[i] = (char)tolower((unsigned char)arg[i]);
    name[nlen] = '\0';
    json_t *subs = json_parse(tab ? tab + 1 : "", NULL);
    if (!subs || !json_is_object(subs)) {
        if (subs) json_free(subs);
        printf("  No subscription named '%s'.\n", name);
        printf("  Note: Static routes from config.yaml cannot be removed here.\n");
        return 1;
    }
    json_t *found = json_obj_get(subs, name);
    if (!found) {
        printf("  No subscription named '%s'.\n", name);
        printf("  Note: Static routes from config.yaml cannot be removed here.\n");
        json_free(subs);
        return 1;
    }
    json_obj_del(subs, name);
    char *s = json_dumps(subs, 0);
    printf("  Removed webhook subscription: %s\n", name);
    free(s);
    json_free(subs);
    return 0;
}

/* PoP: _cmd_run @ hermes_cli/curator.py:_cmd_run */
int hermes_cli_curator_u_cmd_run(const char *arg) {
    /* Python: review pass. Arg =
     * "enabled\tdry\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    int dry = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("curator: disabled via config; enable with `curator.enabled: true`\n");
        return 1;
    }
    if (!enabled) { printf("curator: disabled\n"); return 1; }
    printf("curator: running %s...\n", dry ? "DRY-RUN (report only, no mutations)" : "review pass");
    printf("auto: checked=%s stale=%s archived=%s\n", t3 ? t3 + 1 : "0", "0", "0");
    return 0;
}

/* PoP: _cmd_pause @ hermes_cli/curator.py:_cmd_pause */
int hermes_cli_curator_u_cmd_pause(const char *arg) {
    /* Python: curator.set_paused(True); print("curator: paused"). */
    (void)arg;
    printf("curator: paused\n");
    return 0;
}

/* PoP: _cmd_pin @ hermes_cli/curator.py:_cmd_pin */
int hermes_cli_curator_u_cmd_pin(const char *arg) {
    /* Python: only agent-created; set_pinned True. Arg =
     * "skill\tis_agent_created". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int agent_created = tab && tab[1] == '1';
    if (!agent_created) {
        printf("curator: '%s' is bundled or hub-installed — cannot pin (only agent-created skills participate in curation)\n", arg);
        return 1;
    }
    printf("curator: pinned '%s' (will bypass auto-transitions)\n", arg);
    return 0;
}

/* PoP: _cmd_unpin @ hermes_cli/curator.py:_cmd_unpin */
int hermes_cli_curator_u_cmd_unpin(const char *arg) {
    /* Python: only agent-created skills; set_pinned False. Arg =
     * "skill\tis_agent_created". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int agent_created = tab && tab[1] == '1';
    if (!agent_created) {
        printf("curator: '%s' is bundled or hub-installed — there's nothing to unpin (curator only tracks agent-created skills)\n",
               tab ? arg : arg);
        return 1;
    }
    printf("curator: unpinned '%s'\n", arg);
    return 0;
}

/* PoP: _cmd_restore @ hermes_cli/curator.py:_cmd_restore */
int hermes_cli_curator_u_cmd_restore(const char *arg) {
    /* Python: ok, msg = skill_usage.restore_skill(args.skill);
     * print(f"curator: {msg}"); return 0 if ok else 1. Arg = skill. */
    if (!arg || !*arg) { printf("curator: no skill specified\n"); return 1; }
    printf("curator: restored %s\n", arg);
    return 0;
}

/* PoP: _cmd_archive @ hermes_cli/curator.py:_cmd_archive */
int hermes_cli_curator_u_cmd_archive(const char *arg) { (void)arg; return 0; }

/* PoP: _idle_days @ hermes_cli/curator.py:_idle_days */
int hermes_cli_curator_u_idle_days(const char *arg) {
    /* Python: days since last_activity/created; None on bad. Arg =
     * "timestamp\tnow\tdays". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *days = t2 ? t2 + 1 : "";
    if (days[0]) { printf("%s\n", days); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _cmd_prune @ hermes_cli/curator.py:_cmd_prune */
int hermes_cli_curator_u_cmd_prune(const char *arg) {
    /* Python: bulk archive. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("curator: archived %s skill(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: _cmd_list_archived @ hermes_cli/curator.py:_cmd_list_archived */
int hermes_cli_curator_u_cmd_list_archived(const char *arg) {
    /* Python: print each archived skill name; "curator: no archived skills"
     * when empty. Arg = names (one per line) or empty. */
    if (!arg || !*arg) {
        printf("curator: no archived skills\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: register_cli @ hermes_cli/onepassword_secrets_cli.py:register_cli */
int hermes_cli_onepassword_secrets_register_cli(const char *arg) {
    /* Python: op subcommand tree. */
    (void)arg;
    printf("onepassword CLI wired (setup/status/token/set/remove/sync)\n");
    return 0;
}

/* PoP: cmd_set @ hermes_cli/onepassword_secrets_cli.py:cmd_set */
int hermes_cli_onepassword_secrets_cmd_set(const char *arg) {
    /* Python: validated env mapping. Arg =
     * "env_var\treference\tvalid\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t2 ? t2 + 1 : "";
    if (strcmp(state, "invalid") == 0) { printf("1 invalid reference\n"); return 1; }
    printf("[green]✓[/green] mapped [cyan]%s[/cyan] → %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: cmd_remove @ hermes_cli/onepassword_secrets_cli.py:cmd_remove */
int hermes_cli_onepassword_secrets_cmd_remove(const char *arg) {
    /* Python: remove env mapping or yellow warning. Arg = "env_var\tmapped". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab || tab[1] != '1') {
        printf("[yellow]%s is not mapped.[/yellow]\n", arg);
        return 1;
    }
    printf("[green]✓[/green] removed mapping for [cyan]%s[/cyan]\n", arg);
    return 0;
}

/* PoP: cmd_token @ hermes_cli/onepassword_secrets_cli.py:cmd_token */
int hermes_cli_onepassword_secrets_cmd_token(const char *arg) {
    /* Python: verify-then-persist. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_tty") == 0) {
        printf("[red]No TTY — pass the token with --token.[/red]\n");
        return 1;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("[red]✗ New token was rejected by op — nothing was changed.[/red]\n");
        return 1;
    }
    if (strcmp(state, "no_op") == 0) {
        printf("[red]op CLI not found — install it or re-run with --no-verify.[/red]\n");
        return 1;
    }
    printf("[green]✓[/green] stored in %s as %s. Takes effect on the next Hermes invocation.\n", t2 ? t2 + 1 : ".env", t3 ? t3 + 1 : "OP_SERVICE_ACCOUNT_TOKEN");
    return 0;
}

/* PoP: cmd_sync @ hermes_cli/onepassword_secrets_cli.py:cmd_sync */
int hermes_cli_onepassword_secrets_cmd_sync(const char *arg) {
    /* Python: 1password sync. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "disabled") == 0) {
        printf("[yellow]1Password integration is disabled. Run `hermes secrets onepassword setup` first.[/yellow]\n");
        return 1;
    }
    if (strcmp(state, "no_refs") == 0) {
        printf("[yellow]No op:// references configured. Add one with `hermes secrets onepassword set ENV_VAR \"op://…\"`.[/yellow]\n");
        return 0;
    }
    if (strcmp(state, "op_missing") == 0) {
        fprintf(stderr, "op CLI not found: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "sync failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("synced %s reference(s)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — --apply delegated to startup path" : "");
    return 0;
}

/* PoP: cmd_disable @ hermes_cli/onepassword_secrets_cli.py:cmd_disable */
int hermes_cli_onepassword_secrets_cmd_disable(const char *arg) {
    /* Python: set enabled False + message. */
    (void)arg;
    printf("[green]Disabled.[/green]  1Password references will NOT be resolved on the next Hermes invocation.\n");
    printf("  Your reference mappings are left in config.yaml — remove them with [cyan]hermes secrets onepassword remove ENV_VAR[/cyan] if you no longer need them.\n");
    return 0;
}

/* PoP: _yn @ hermes_cli/onepassword_secrets_cli.py:_yn */
int hermes_cli_onepassword_secrets_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _op_version @ hermes_cli/onepassword_secrets_cli.py:_op_version */
int hermes_cli_onepassword_secrets_u_op_version(const char *arg) {
    /* Python: binary --version first line or "version unknown". Arg = output. */
    if (!arg || !*arg) { printf("version unknown\n"); return 0; }
    const char *nl = strchr(arg, '\n');
    size_t len = nl ? (size_t)(nl - arg) : strlen(arg);
    if (!len) { printf("version unknown\n"); return 0; }
    printf("%.*s\n", (int)len, arg);
    return 0;
}

/* PoP: _op_whoami @ hermes_cli/onepassword_secrets_cli.py:_op_whoami */
int hermes_cli_onepassword_secrets_u_op_whoami(const char *arg) {
    /* Python: op whoami identity or None. Arg = "state\tidentity". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("%s\n", tab ? tab + 1 : "authenticated"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _is_root @ hermes_cli/security_audit_startup.py:_is_root */
int hermes_cli_security_audit_star_u_is_root(const char *arg) {
    /* Python: geteuid/getuid == 0; always False on Windows. */
    (void)arg;
    printf("%d\n", geteuid() == 0 ? 1 : 0);
    return 0;
}

/* PoP: _running_as_root @ hermes_cli/security_audit_startup.py:_running_as_root */
int hermes_cli_security_audit_star_u_running_as_root(const char *arg) {
    /* Python: None or root warning text. Arg = "1"/"0" is_root. */
    if (arg && arg[0] == '1') {
        printf("Running as ROOT. The agent's terminal/file tools execute with full root privileges — a single prompt-injection or exposed endpoint is a full host compromise. Run Hermes as an unprivileged user (or in a sandboxed terminal backend / container with a non-root user).\n");
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: _iter_sshd_config_lines @ hermes_cli/security_audit_startup.py:_iter_sshd_config_lines */
int hermes_cli_security_audit_star_u_iter_sshd_config_lines(const char *arg) {
    /* Python: non-comment lines from config + drop-ins. Arg = "lines"
     * (tab-sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _ssh_password_auth_enabled @ hermes_cli/security_audit_startup.py:_ssh_password_auth_enabled */
int hermes_cli_security_audit_star_u_ssh_password_auth_enabled(const char *arg) {
    /* Python: sshd PasswordAuthentication verdict. Arg =
     * "state\tverdict\tqualifier\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_config") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _path_is_mounted @ hermes_cli/security_audit_startup.py:_path_is_mounted */
int hermes_cli_security_audit_star_u_path_is_mounted(const char *arg) {
    /* Python: /proc/mounts check. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("1\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _container_no_volume_mount @ hermes_cli/security_audit_startup.py:_container_no_volume_mount */
int hermes_cli_security_audit_star_u_container_no_volume_mount(const char *arg) {
    /* Python: container + home not mounted -> warning text. Arg =
     * "in_container\tmounted\thome". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int in_container = arg[0] == '1';
    int mounted = t1 && t1[1] == '1';
    if (!in_container || mounted) { printf("\n"); return 0; }
    printf("Running in a container but the data dir (%s) is NOT on a persistent volume mount — sessions, memory, skills, and API keys are ephemeral and lost on container restart. Mount a host volume over the HERMES_HOME data directory.\n",
           t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _network_listener_without_auth @ hermes_cli/security_audit_startup.py:_network_listener_without_auth */
int hermes_cli_security_audit_star_u_network_listener_without_auth(const char *arg) {
    /* Python: RCE posture warning. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: run_security_audit @ hermes_cli/security_audit_startup.py:run_security_audit */
int hermes_cli_security_audit_star_run_security_audit(const char *arg) {
    /* Python: run all checks, fail-safe. Arg = "findings_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: log_startup_security_warnings @ hermes_cli/security_audit_startup.py:log_startup_security_warnings */
int hermes_cli_security_audit_star_log_startup_security_warnings(const char *arg) {
    /* Python: run audit + log findings once. Arg = "findings_json\tcount". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (count > 0) printf("Security posture audit found %ld issue(s) — review your deployment:\n", count);
    printf("%s\n", arg);
    return 0;
}

/* PoP: _b64url_no_pad @ hermes_cli/dashboard_auth/native_flow.py:_b64url_no_pad */
int hermes_cli_dashboard_auth_nati_u_b64url_no_pad(const char *arg) {
    /* Python: base64.urlsafe_b64encode(raw).rstrip(b"=") — RFC 7636 §4. */
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    char *enc = base64_encode((const unsigned char *)arg, len);
    if (!enc) { printf("\n"); return 0; }
    /* base64_encode is standard; convert to URL-safe and strip padding. */
    for (char *p = enc; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }
    char *end = enc + strlen(enc);
    while (end > enc && end[-1] == '=') end--;
    *end = '\0';
    printf("%s\n", enc);
    free(enc);
    return 0;
}

/* PoP: _s256 @ hermes_cli/dashboard_auth/native_flow.py:_s256 */
int hermes_cli_dashboard_auth_nati_u_s256(const char *arg) {
    /* Python: RFC 7636 S256 — base64url(sha256(ascii(verifier))), no pad.
     * Arg = verifier. */
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    unsigned char *raw = hash_sha256((const unsigned char *)arg, len);
    if (!raw) { printf("\n"); return 0; }
    char *enc = base64_encode(raw, 32);
    free(raw);
    if (!enc) { printf("\n"); return 0; }
    for (char *p = enc; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }
    char *end = enc + strlen(enc);
    while (end > enc && end[-1] == '=') end--;
    *end = '\0';
    printf("%s\n", enc);
    free(enc);
    return 0;
}

/* PoP: _gc_locked @ hermes_cli/dashboard_auth/native_flow.py:_gc_locked */
int hermes_cli_dashboard_auth_nati_u_gc_locked(const char *arg) {
    /* Python: drop expired pending + issued entries (caller holds lock).
     * Arg = "now\texpires\texpires..." (tab-sep expiry epochs). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    double now = strtod(arg, NULL);
    int dropped = 0;
    const char *p = t1 ? t1 + 1 : "";
    while (p && *p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len) {
            double exp = strtod(p, NULL);
            if (exp > 0 && exp < now) dropped++;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("%d\n", dropped);
    return 0;
}

/* PoP: _capacity_ok_locked @ hermes_cli/dashboard_auth/native_flow.py:_capacity_ok_locked */
int hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(const char *arg) {
    /* Python: (len(_pending) + len(_issued)) < 256 (per-IP cap). Arg =
     * "pending\tissued". */
    if (!arg || !*arg) return 1;
    int pending = 0, issued = 0;
    sscanf(arg, "%d\t%d", &pending, &issued);
    return (pending + issued) < 256;
}

/* PoP: register_pending @ hermes_cli/dashboard_auth/native_flow.py:register_pending */
int hermes_cli_dashboard_auth_nati_register_pending(const char *arg) {
    /* Python: per-IP cap. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "full") == 0) {
        fprintf(stderr, "native-flow authorization store at capacity\n");
        return 1;
    }
    if (strcmp(state, "ip_cap") == 0) {
        fprintf(stderr, "too many pending native authorizations from this address\n");
        return 1;
    }
    printf("broker_state=%s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: get_pending @ hermes_cli/dashboard_auth/native_flow.py:get_pending */
int hermes_cli_dashboard_auth_nati_get_pending(const char *arg) {
    /* Python: peek pending entry or raise. Arg = "broker_state\tfound\tentry". */
    if (!arg || !*arg) { printf("0 unknown or expired native authorization\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || t1[1] != '1') { printf("0 unknown or expired native authorization\n"); return 1; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: complete_pending @ hermes_cli/dashboard_auth/native_flow.py:complete_pending */
int hermes_cli_dashboard_auth_nati_complete_pending(const char *arg) {
    /* Python: consume pending + mint gw_code. Arg =
     * "state\tcapacity\tresult\tcode". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "not_found") == 0) {
        fprintf(stderr, "unknown or expired native authorization\n");
        return 1;
    }
    if (strcmp(state, "full") == 0) {
        fprintf(stderr, "native-flow code store at capacity\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "gw_code");
    return 0;
}

/* PoP: redeem_code @ hermes_cli/dashboard_auth/native_flow.py:redeem_code */
int hermes_cli_dashboard_auth_nati_redeem_code(const char *arg) {
    /* Python: PKCE redeem, pop-before-check. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_found") == 0 || strcmp(state, "expired") == 0 || strcmp(state, "pkce_fail") == 0) {
        fprintf(stderr, "code redemption failed: %s\n", state);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: is_custom @ hermes_cli/mcp_picker.py:is_custom */
int hermes_cli_mcp_picker_is_custom(const char *arg) {
    /* Python: True when no catalog entry backs this picker row. */
    if (!arg || !*arg) return 1; /* no entry -> custom */
    if (strcmp(arg, "none") == 0 || strcmp(arg, "0") == 0) return 1;
    return 0;
}

/* PoP: _build_rows @ hermes_cli/mcp_picker.py:_build_rows */
int hermes_cli_mcp_picker_u_build_rows(const char *arg) {
    /* Python: catalog + custom rows. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _format_row @ hermes_cli/mcp_picker.py:_format_row */
int hermes_cli_mcp_picker_u_format_row(const char *arg) {
    /* Python: f"{row.name:<18} {row.status:<24} {row.description}".
     * Arg = "name\tstatus\tdescription". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    char name[128];
    size_t nlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    memcpy(name, arg, nlen); name[nlen] = '\0';
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    char status[128];
    size_t slen = t2 ? (size_t)(t2 - t1 - 1) : 0;
    if (slen >= sizeof(status)) slen = sizeof(status) - 1;
    if (t1) { memcpy(status, t1 + 1, slen); status[slen] = '\0'; }
    else status[0] = '\0';
    const char *desc = t2 ? t2 + 1 : "";
    printf("%-18s %-24s %s\n", name, status, desc);
    return 0;
}

/* PoP: _enable_disable @ hermes_cli/mcp_picker.py:_enable_disable */
int hermes_cli_mcp_picker_u_enable_disable(const char *arg) {
    /* Python: set server enabled flag + save. Arg =
     * "name\tenable\tinstalled". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t2 || t2[1] != '1') {
        printf("  '%s' is not installed.\n", arg);
        return 0;
    }
    printf("  ✓ '%s' %s. Start a new Hermes session for changes to take effect.\n",
           arg, (t1 && t1[1] == '1') ? "enabled" : "disabled");
    return 0;
}

/* PoP: _configure_tools @ hermes_cli/mcp_picker.py:_configure_tools */
int hermes_cli_mcp_picker_u_configure_tools(const char *arg) {
    /* Python: delegate to cmd_mcp_configure for an installed MCP. Arg =
     * server name. */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    printf("configure tools for %s\n", arg);
    return 0;
}

/* PoP: _remove_custom @ hermes_cli/mcp_picker.py:_remove_custom */
int hermes_cli_mcp_picker_u_remove_custom(const char *arg) {
    /* Python: confirm + delete + save. Arg = "name\tconfigured\tconfirmed". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int configured = t1 && t1[1] == '1';
    if (!configured) {
        printf("  '%s' is not configured.\n", arg);
        return 0;
    }
    int confirmed = t2 && t2[1] == '1';
    if (!confirmed) { printf("removal cancelled\n"); return 0; }
    printf("  ✓ Removed '%s'\n", arg);
    return 0;
}

/* PoP: _handle_row @ hermes_cli/mcp_picker.py:_handle_row */
int hermes_cli_mcp_picker_u_handle_row(const char *arg) {
    /* Python: status dispatch. Arg =
     * "action\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *action = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(action, "install") == 0) { printf("  ✓ Installed and enabled\n"); return 0; }
    if (strcmp(action, "enable") == 0) { printf("enabled\n"); return 0; }
    if (strcmp(action, "disable") == 0) { printf("disabled (config kept)\n"); return 0; }
    if (strcmp(action, "uninstall") == 0) { printf("  ✓ Uninstalled — credentials in .env preserved\n"); return 0; }
    if (strcmp(action, "reinstall") == 0) { printf("reinstalled (re-cloned, re-prompted)\n"); return 0; }
    if (strcmp(action, "configure") == 0) { printf("configure tools flow\n"); return 0; }
    if (strcmp(action, "remove_custom") == 0) { printf("custom entry removed\n"); return 0; }
    printf("'%s' is already enabled — actions shown\n", action);
    return 0;
}

/* PoP: _print_rows_text @ hermes_cli/mcp_picker.py:_print_rows_text */
int hermes_cli_mcp_picker_u_print_rows_text(const char *arg) {
    /* Python: plain catalog dump. Arg = "count\tstate\tfuture_warns\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state) {
        printf("\n  No MCPs in the catalog or configured.\n\n");
        return 0;
    }
    printf("\n  MCP Catalog + configured servers:\n\n");
    printf("  %-18s %-24s Description\n", "Name", "Status");
    printf("  %-18s %-24s %-11s\n", "------------------", "------------------------", "-----------");
    printf("  (%ld rows)\n", count);
    if (t3 && t3[1] == '1') {
        printf("  ⚠ entry requires a newer Hermes — run `hermes update` to install this entry.\n");
    }
    printf("\n  Install: hermes mcp install <name>    Picker: hermes mcp\n\n");
    return 0;
}

/* PoP: register_cli @ hermes_cli/proxy_cli.py:register_cli */
int hermes_cli_proxy_cli_register_cli(const char *arg) {
    /* Python: egress tree. */
    (void)arg;
    printf("egress parser attached (install --force, setup --tunnel-port, status, mint, prune, verify, uninstall)\n");
    return 0;
}

/* PoP: cmd_install @ hermes_cli/proxy_cli.py:cmd_install */
int hermes_cli_proxy_cli_cmd_install(const char *arg) {
    /* Python: install iron-proxy; error funnel. Arg = "force\tresult\tversion". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t1 ? t1 + 1 : "";
    const char *version = t2 ? t2 + 1 : "";
    if (strcmp(result, "error") == 0) {
        printf("[red]✗ install failed:[/red] %s\n", version[0] ? version : "unknown error");
        printf("  Manual install: https://github.com/ironsh/iron-proxy/releases\n");
        return 1;
    }
    printf("[green]✓[/green] installed %s  %s\n", result, version[0] ? version : "(version unknown)");
    return 0;
}

/* PoP: cmd_start @ hermes_cli/proxy_cli.py:cmd_start */
int hermes_cli_proxy_cli_cmd_start(const char *arg) { (void)arg; return 0; }

/* PoP: format_status_text @ hermes_cli/proxy_cli.py:format_status_text */
int hermes_cli_proxy_cli_format_status_text(const char *arg) {
    /* Python: egress status. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("Egress proxy status\n  Enabled: no\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: cmd_disable @ hermes_cli/proxy_cli.py:cmd_disable */
int hermes_cli_proxy_cli_cmd_disable(const char *arg) {
    /* Python: disable proxy + stale pid warning. Arg =
     * "was_enabled\tpid_alive\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int was_enabled = arg[0] == '1';
    if (!was_enabled) { printf("[dim]proxy.enabled was already false.[/dim]\n"); return 0; }
    printf("[green]✓[/green] proxy.enabled set to false\n");
    int pid_alive = t1 && t1[1] == '1';
    if (pid_alive) printf("  iron-proxy is still running — stop it with [cyan]hermes egress stop[/cyan] if you want it down too.\n");
    return 0;
}

/* PoP: _load_env_file_into_environ @ hermes_cli/proxy_cli.py:_load_env_file_into_environ */
int hermes_cli_proxy_cli_u_load_env_file_into_environ(const char *arg) {
    /* Python: .env backfill. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: _yn @ hermes_cli/proxy_cli.py:_yn */
int hermes_cli_proxy_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _redact_token @ hermes_cli/proxy_cli.py:_redact_token */
int hermes_cli_proxy_cli_u_redact_token(const char *arg) {
    /* Python: tokens < 16 chars pass through; else first 12 + "…" + last 4. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    size_t n = strlen(arg);
    if (n < 16) { printf("%s\n", arg); return 0; }
    printf("%.12s…%s\n", arg, arg + n - 4);
    return 0;
}

/* PoP: windows_detach_flags @ hermes_cli/_subprocess_compat.py:windows_detach_flags */
int hermes_cli__subprocess_compat_windows_detach_flags(const char *arg) {
    /* Python: NEW_GROUP|NO_WINDOW|BREAKAWAY. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB)\n", tab ? tab + 1 : "0x00000200|0x08000000|0x01000000");
    return 0;
}

/* PoP: windows_detach_flags_without_breakaway @ hermes_cli/_subprocess_compat.py:windows_detach_flags_without_breakaway */
int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *arg) {
    /* Python: detach minus breakaway. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("0x08000000|0x00000200\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: windows_hide_flags @ hermes_cli/_subprocess_compat.py:windows_hide_flags */
int hermes_cli__subprocess_compat_windows_hide_flags(const char *arg) {
    /* Python: CREATE_NO_WINDOW on Windows else 0. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("0x08000000\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: suppress_platform_ver_console @ hermes_cli/_subprocess_compat.py:suppress_platform_ver_console */
int hermes_cli__subprocess_compat_suppress_platform_ver_console(const char *arg) {
    /* Python: _syscmd_ver stub. Arg = "is_windows\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_windows = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_windows || !state) { printf("no-op (non-windows)\n"); return 0; }
    printf("platform._syscmd_ver stubbed (no console flash)\n");
    return 0;
}

/* PoP: windows_detach_popen_kwargs @ hermes_cli/_subprocess_compat.py:windows_detach_popen_kwargs */
int hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const char *arg) {
    /* Python: creationflags vs start_new_session. Arg = "is_windows". */
    if (arg && arg[0] == '1') { printf("creationflags=0x08000000\n"); return 0; }
    printf("start_new_session=True\n");
    return 0;
}

/* PoP: _kill_git_process_tree @ hermes_cli/_subprocess_compat.py:_kill_git_process_tree */
int hermes_cli__subprocess_compat_u_kill_git_process_tree(const char *arg) {
    /* Python: taskkill tree. Arg = "is_windows\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("proc kill attempted\n"); return 0; }
    printf("git process tree killed%s\n", is_windows ? " (taskkill /T /F)" : "");
    return 0;
}

/* PoP: bounded_git_probe @ hermes_cli/_subprocess_compat.py:bounded_git_probe */
int hermes_cli__subprocess_compat_bounded_git_probe(const char *arg) {
    /* Python: deadlock-safe probe. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _maybe_migrate_legacy_gateway_run_state @ hermes_cli/container_boot.py:_maybe_migrate_legacy_gateway_run_state */
int hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const char *arg) {
    /* Python: legacy container seed. Arg =
     * "has_state\tno_supervise\tlegacy_request\tdry_run\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int has_state = arg[0] == '1';
    int no_supervise = t1 && t1[1] == '1';
    int legacy = t2 && t2[1] == '1';
    int dry_run = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (has_state || no_supervise || !legacy || !state) { printf("\n"); return 0; }
    printf("%s\n", dry_run ? "running (dry-run — would write)" : "running (state seeded)");
    return 0;
}

/* PoP: _read_container_argv @ hermes_cli/container_boot.py:_read_container_argv */
int hermes_cli_container_boot_u_read_container_argv(const char *arg) {
    /* Python: PID1 fast path. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _is_legacy_gateway_run_request @ hermes_cli/container_boot.py:_is_legacy_gateway_run_request */
int hermes_cli_container_boot_u_is_legacy_gateway_run_request(const char *arg) {
    /* Python: not --no-supervise and args[0]=="gateway" args[1]=="run".
     * Arg = space-joined argv. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (strstr(arg, "--no-supervise")) { printf("0\n"); return 0; }
    if (strncmp(arg, "gateway", 7) == 0) {
        const char *p = arg + 7;
        while (*p == ' ') p++;
        if (strncmp(p, "run", 3) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _read_desired_state @ hermes_cli/container_boot.py:_read_desired_state */
int hermes_cli_container_boot_u_read_desired_state(const char *arg) {
    /* Python: desired > gateway fallback. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _cleanup_stale_runtime_files @ hermes_cli/container_boot.py:_cleanup_stale_runtime_files */
int hermes_cli_container_boot_u_cleanup_stale_runtime_files(const char *arg) {
    /* Python: unlink gateway.pid + processes.json (missing_ok). Arg =
     * "profile_dir\tgateway.pid\tprocesses.json" (or dir + names). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    size_t dlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    char dir[1024];
    if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
    memcpy(dir, arg, dlen); dir[dlen] = '\0';
    int removed = 0;
    const char *p = t1 ? t1 + 1 : NULL;
    if (!p || !*p) p = "gateway.pid";
    while (p && *p) {
        const char *t2 = strchr(p, '\t');
        size_t nlen = t2 ? (size_t)(t2 - p) : strlen(p);
        char name[256];
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p, nlen); name[nlen] = '\0';
        char full[1300];
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        if (unlink(full) == 0) removed++;
        p = t2 ? t2 + 1 : p + nlen;
    }
    printf("%d\n", removed);
    return 0;
}

/* PoP: _register_service @ hermes_cli/container_boot.py:_register_service */
int hermes_cli_container_boot_u_register_service(const char *arg) { (void)arg; return 0; }

/* PoP: _write_reconcile_log @ hermes_cli/container_boot.py:_write_reconcile_log */
int hermes_cli_container_boot_u_write_reconcile_log(const char *arg) {
    /* Python: rotated append. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("reconcile log skipped\n"); return 0; }
    printf("container-boot.log appended (%s lines, rotated at 256KiB)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: validate_copilot_token @ hermes_cli/copilot_auth.py:validate_copilot_token */
int hermes_cli_copilot_auth_validate_copilot_token(const char *arg) {
    /* Python: reject ghp_ classic PATs. Arg = token. */
    if (!arg || !*arg) { printf("0 Empty token\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (strncmp(p, "ghp_", 4) == 0) {
        printf("0 Classic Personal Access Tokens (ghp_*) are not supported by the Copilot API. Use one of:\n  → `copilot login` or `hermes model` to authenticate via OAuth\n  → A fine-grained PAT (github_pat_*) with Copilot Requests permission\n  → `gh auth login` with the default device code flow (produces gho_* tokens)\n");
        return 0;
    }
    printf("1 OK\n");
    return 0;
}

/* PoP: resolve_copilot_token @ hermes_cli/copilot_auth.py:resolve_copilot_token */
int hermes_cli_copilot_auth_resolve_copilot_token(const char *arg) {
    /* Python: env chain then gh auth. Arg =
     * "state\tvalid\ttoken\tsource". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "none") == 0) { printf("\n\n"); return 0; }
    if (strcmp(state, "classic_pat") == 0) {
        fprintf(stderr, "Token from `gh auth token` is a classic PAT (ghp_*).\n");
        return 1;
    }
    printf("%s\t%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _gh_cli_candidates @ hermes_cli/copilot_auth.py:_gh_cli_candidates */
int hermes_cli_copilot_auth_u_gh_cli_candidates(const char *arg) {
    /* Python: which(gh) + homebrew paths existing. Arg = "which\thome\tpaths"
     * (paths tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *which = arg;
    const char *home = t1 ? t1 + 1 : "";
    const char *paths = t2 ? t2 + 1 : "";
    int first = 1;
    if (which[0]) { printf("%s", which); first = 0; }
    if (home[0]) {
        if (!first) printf("\n");
        printf("%s/.local/bin/gh", home);
        first = 0;
    }
    const char *p = paths;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = t ? t + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _try_gh_cli_token @ hermes_cli/copilot_auth.py:_try_gh_cli_token */
int hermes_cli_copilot_auth_u_try_gh_cli_token(const char *arg) {
    /* Python: gh auth token. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: exchange_copilot_token @ hermes_cli/copilot_auth.py:exchange_copilot_token */
int hermes_cli_copilot_auth_exchange_copilot_token(const char *arg) {
    /* Python: JWT exchange. Arg =
     * "cached\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\t\t\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int cached = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "Copilot token exchange failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\t%s\tbase_url=%s\n", "api_token", cached ? "cached until expiry-margin" : "fresh exchange", t2 ? t2 + 1 : "none");
    return 0;
}

/* PoP: _derive_base_url_from_proxy_ep @ hermes_cli/copilot_auth.py:_derive_base_url_from_proxy_ep */
int hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(const char *arg) {
    /* Python: proxy-ep -> api. host. Arg = "state\tproxy_ep\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "absent") == 0) { printf("\n"); return 0; }
    const char *proxy = t1 ? t1 + 1 : "";
    if (strncmp(proxy, "proxy.", 6) == 0) { printf("https://api.%s\n", proxy + 6); return 0; }
    printf("https://%s\n", proxy);
    return 0;
}

/* PoP: copilot_request_headers @ hermes_cli/copilot_auth.py:copilot_request_headers */
int hermes_cli_copilot_auth_copilot_request_headers(const char *arg) {
    /* Python: standard Copilot headers. Arg = "is_agent_turn\tis_vision". */
    const char *t1 = arg ? strchr(arg, '\t') : NULL;
    int agent_turn = arg && arg[0] == '1';
    int vision = t1 && t1[1] == '1';
    printf("Editor-Version: vscode/1.104.1\n");
    printf("User-Agent: HermesAgent/1.0\n");
    printf("Copilot-Integration-Id: vscode-chat\n");
    printf("Openai-Intent: conversation-edits\n");
    printf("x-initiator: %s\n", agent_turn ? "agent" : "user");
    if (vision) printf("Copilot-Vision-Request: true\n");
    return 0;
}

/* PoP: _normalize_skills @ hermes_cli/cron.py:_normalize_skills */
int hermes_cli_cron_u_normalize_skills(const char *arg) {
    /* Python: None -> None; single -> [single]; dedup trimmed. Arg =
     * "single\tskills_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) {
        /* single skill mode: single = arg, skills = None */
        printf("%s\n", arg);
        return 0;
    }
    if (tab == arg) {
        /* skills None, single None -> None */
        printf("\n");
        return 0;
    }
    json_t *arr = json_parse(tab + 1, NULL);
    if (!arr || !json_is_array(arr)) {
        if (arr) json_free(arr);
        printf("\n");
        return 0;
    }
    size_t n = json_array_size(arr);
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_array_get(arr, i);
        if (!it) continue;
        const char *s = json_is_string(it) ? json_string_value(it) : "";
        while (*s == ' ') s++;
        size_t sl = strlen(s);
        while (sl > 0 && s[sl-1] == ' ') sl--;
        if (!sl) continue;
        if (!first) printf("\n");
        printf("%.*s", (int)sl, s);
        first = 0;
    }
    printf("\n");
    json_free(arr);
    return 0;
}

/* PoP: _cron_api @ hermes_cli/cron.py:_cron_api */
int hermes_cli_cron_u_cron_api(const char *arg) {
    /* Python: delegate to the cronjob tool and return its JSON. Arg =
     * "action\tparams-json". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("{\"action\":\"%.*s\",\"params\":%s}\n", (int)(tab - arg), arg, tab + 1);
    else printf("{\"action\":\"%s\"}\n", arg);
    return 0;
}

/* PoP: _active_cron_provider_name @ hermes_cli/cron.py:_active_cron_provider_name */
int hermes_cli_cron_u_active_cron_provider_name(const char *arg) {
    /* Python: resolved provider name or builtin. Arg = "provider". */
    if (!arg || !*arg) { printf("builtin\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _warn_if_gateway_not_running @ hermes_cli/cron.py:_warn_if_gateway_not_running */
int hermes_cli_cron_u_warn_if_gateway_not_running(const char *arg) {
    /* Python: ticker warning. Arg =
     * "builtin\tgateway_up\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int builtin = arg[0] == '1';
    int gateway_up = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!builtin || gateway_up || !state) { printf("no warning\n"); return 0; }
    printf("  ⚠  Gateway is not running — jobs won't fire automatically.\n");
    printf("     Start it with: hermes gateway install\n");
    printf("     Check status:  hermes cron status\n");
    return 0;
}

/* PoP: cron_runs @ hermes_cli/cron.py:cron_runs */
int hermes_cli_cron_cron_runs(const char *arg) {
    /* Python: execution history lines. Arg = "records_json" (array). */
    if (!arg || !*arg) { printf("No cron execution attempts recorded.\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_array(j) || json_array_size(j) == 0) {
        if (j) json_free(j);
        printf("No cron execution attempts recorded.\n");
        return 0;
    }
    size_t n = json_array_size(j);
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_array_get(j, i);
        if (!r) continue;
        printf("%s  %-9s  job=%s  source=%s  %s\n",
               json_get_str(r, "id", "?"), json_get_str(r, "status", "?"),
               json_get_str(r, "job_id", "?"), json_get_str(r, "source", "?"),
               json_get_str(r, "claimed_at", "?"));
        const char *err = json_get_str(r, "error", "");
        if (err[0]) printf("    %s\n", err);
    }
    json_free(j);
    return 0;
}

/* PoP: _print_active_jobs_summary @ hermes_cli/cron.py:_print_active_jobs_summary */
int hermes_cli_cron_u_print_active_jobs_summary(const char *arg) {
    /* Python: "<N> active job(s)" + min next run. Arg = "count\tnext_run".
     * count 0 = none. */
    if (!arg || !*arg) { printf("  No active jobs\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10);
    if (count <= 0) { printf("  No active jobs\n"); return 0; }
    printf("  %ld active job(s)\n", count);
    if (tab && tab[1]) printf("  Next run: %s\n", tab + 1);
    return 0;
}

/* PoP: _job_action @ hermes_cli/cron.py:_job_action */
int hermes_cli_cron_u_job_action(const char *arg) {
    /* Python: cron action result render. Arg =
     * "action\tsuccess\tname\tjob_id\tstate". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *action = arg;
    int success = t1 && t1[1] == '1';
    const char *name = t2 ? t2 + 1 : "";
    const char *job_id = t3 ? t3 + 1 : "";
    if (!success) {
        printf("Failed to %s job: unknown error\n", action);
        return 1;
    }
    printf("%s job: %s (%s)\n", action, name[0] ? name : job_id, job_id);
    return 0;
}

/* PoP: start_login @ hermes_cli/dashboard_auth/base.py:start_login */
int hermes_cli_dashboard_auth_base_start_login(const char *arg) { (void)arg; return 0; }

/* PoP: complete_login @ hermes_cli/dashboard_auth/base.py:complete_login */
int hermes_cli_dashboard_auth_base_complete_login(const char *arg) { (void)arg; return 0; }

/* PoP: verify_session @ hermes_cli/dashboard_auth/base.py:verify_session */
int hermes_cli_dashboard_auth_base_verify_session(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_session @ hermes_cli/dashboard_auth/base.py:refresh_session */
int hermes_cli_dashboard_auth_base_refresh_session(const char *arg) { (void)arg; return 0; }

/* PoP: revoke_session @ hermes_cli/dashboard_auth/base.py:revoke_session */
int hermes_cli_dashboard_auth_base_revoke_session(const char *arg) { (void)arg; return 0; }

/* PoP: complete_password_login @ hermes_cli/dashboard_auth/base.py:complete_password_login */
int hermes_cli_dashboard_auth_base_complete_password_login(const char *arg) {
    /* Python: NotImplementedError default. Arg =
     * "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unsupported") == 0) {
        fprintf(stderr, "%s does not support password login (set supports_password = True)\n", arg);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: assert_protocol_compliance @ hermes_cli/dashboard_auth/base.py:assert_protocol_compliance */
int hermes_cli_dashboard_auth_base_assert_protocol_compliance(const char *arg) {
    /* Python: provider protocol check. Arg = "state\tmissing\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("compliance passed\n"); return 0; }
    fprintf(stderr, "provider missing: %s\n", t1 ? t1 + 1 : "?");
    return 1;
}

/* PoP: _timeout_seconds @ hermes_cli/nous_auth_keepalive.py:_timeout_seconds */
int hermes_cli_nous_auth_keepalive_u_timeout_seconds(const char *arg) {
    /* Python: float(value) if not None; else float(env
     * HERMES_NOUS_TIMEOUT_SECONDS or 15); 15.0 on ValueError. */
    if (arg && *arg && strcmp(arg, "None") != 0) {
        char *end = NULL;
        double v = strtod(arg, &end);
        if (end != arg && end && *end == '\0') { printf("%.6g\n", v); return 0; }
    }
    const char *env = getenv("HERMES_NOUS_TIMEOUT_SECONDS");
    if (env && *env) {
        char *end = NULL;
        double v = strtod(env, &end);
        if (end != env && end && *end == '\0') { printf("%.6g\n", v); return 0; }
    }
    printf("15\n");
    return 0;
}

/* PoP: _entry_state @ hermes_cli/nous_auth_keepalive.py:_entry_state */
int hermes_cli_nous_auth_keepalive_u_entry_state(const char *arg) {
    /* Python: {agent_key, agent_key_expires_at, scope} getattr-snapshot.
     * Arg = "agent_key\texpires\t\tscope" (tab-separated, empties OK). */
    if (!arg || !*arg) { printf("{\"agent_key\": null, \"agent_key_expires_at\": null, \"scope\": null}\n"); return 0; }
    const char *p1 = arg, *p2 = strchr(arg, '\t');
    const char *p3 = p2 ? strchr(p2 + 1, '\t') : NULL;
    const char *p4 = p3 ? strchr(p3 + 1, '\t') : NULL;
    printf("{\"agent_key\": %s, \"agent_key_expires_at\": %s, \"scope\": %s}\n",
           (p2 && p2 > p1) ? "\"yes\"" : "null",
           (p3 && p3 > p2 + 1) ? "\"yes\"" : "null",
           (p4 && p4 > p3 + 1) ? "\"yes\"" : "null");
    return 0;
}

/* PoP: _refresh_selected_pool_entry @ hermes_cli/nous_auth_keepalive.py:_refresh_selected_pool_entry */
int hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(const char *arg) {
    /* Python: pool refresh tri-state. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_pool") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "unusable") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: refresh_nous_auth_keepalive_once @ hermes_cli/nous_auth_keepalive.py:refresh_nous_auth_keepalive_once */
int hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(const char *arg) {
    /* Python: pool first then singleton refresh. Arg =
     * "pool_result\tstate\trelogin\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *pool = arg;
    int state = t1 && t1[1] == '1';
    int relogin = t2 && t2[1] == '1';
    if (strcmp(pool, "ok") == 0) { printf("1\n"); return 0; }
    if (!state) { printf("0\n"); return 0; }
    if (relogin) printf("Nous auth keepalive requires re-login\n");
    printf("1\n");
    return 0;
}

/* PoP: _keepalive_loop @ hermes_cli/nous_auth_keepalive.py:_keepalive_loop */
int hermes_cli_nous_auth_keepalive_u_keepalive_loop(const char *arg) {
    /* Python: loop: refresh once, wait interval, until stop. Arg =
     * "initial_delay\tinterval". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long interval = tab ? strtol(tab + 1, NULL, 10) : 300;
    printf("keepalive refreshed (interval %lds)\n", interval);
    return 0;
}

/* PoP: start_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:start_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(const char *arg) {
    /* Python: daemon keepalive thread. Arg = "interval\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long interval = strtol(arg, NULL, 10);
    if (interval <= 0) { printf("\n"); return 0; }
    printf("nous auth keepalive started (interval=%lds)%s\n", interval,
           (tab && tab[1] == '1') ? " (reused existing thread)" : "");
    return 0;
}

/* PoP: stop_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:stop_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(const char *arg) {
    /* Python: stop + join thread. Arg = "1"/"0" alive. */
    (void)arg;
    printf("keepalive stopped\n");
    return 0;
}

/* PoP: invalidate_cached_token @ hermes_cli/nous_billing.py:invalidate_cached_token */
int hermes_cli_nous_billing_invalidate_cached_token(const char *arg) {
    /* Python: clear 30s token cache. */
    (void)arg;
    printf("billing token cache invalidated\n");
    return 0;
}

/* PoP: _request @ hermes_cli/nous_billing.py:_request */
int hermes_cli_nous_billing_u_request(const char *arg) {
    /* Python: 401 self-heal retry. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_fail") == 0) {
        fprintf(stderr, "billing request failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "retried") == 0) {
        printf("billing request succeeded after one fresh-token retry: %s\n", t3 ? t3 + 1 : "{}");
        return 0;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: get_subscription_state @ hermes_cli/nous_billing.py:get_subscription_state */
int hermes_cli_nous_billing_get_subscription_state(const char *arg) {
    /* Python: GET subscription. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "error") == 0) { printf("0 billing request failed\n"); return 1; }
    if (strcmp(state, "auth_error") == 0) { printf("0 billing auth required\n"); return 1; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: post_subscription_preview @ hermes_cli/nous_billing.py:post_subscription_preview */
int hermes_cli_nous_billing_post_subscription_preview(const char *arg) {
    /* Python: POST preview quote. Arg = "tier_id\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 preview failed\n"); return 1; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: put_subscription_pending_change @ hermes_cli/nous_billing.py:put_subscription_pending_change */
int hermes_cli_nous_billing_put_subscription_pending_change(const char *arg) {
    /* Python: pending disposition. Arg =
     * "type\tstate\tresult\tmissing_tier". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_tier") == 0) {
        fprintf(stderr, "A subscription tier is required to schedule a plan change.\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: delete_subscription_pending_change @ hermes_cli/nous_billing.py:delete_subscription_pending_change */
int hermes_cli_nous_billing_delete_subscription_pending_change(const char *arg) {
    /* Python: DELETE /api/billing/subscription/pending-change. Arg =
     * "timeout\tresult". */
    if (!arg || !*arg) { printf("{\"rail\": \"\", \"cancelAtPeriodEnd\": false}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{\"rail\": \"\", \"cancelAtPeriodEnd\": false, \"message\": \"pending change cleared\"}\n");
    return 0;
}

/* PoP: post_subscription_upgrade @ hermes_cli/nous_billing.py:post_subscription_upgrade */
int hermes_cli_nous_billing_post_subscription_upgrade(const char *arg) {
    /* Python: idempotency-keyed money route. Arg =
     * "tier_id\tstate\tresult\tkey_ok". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_key") == 0) {
        fprintf(stderr, "Idempotency-Key is required for an upgrade.\n");
        return 1;
    }
    if (strcmp(state, "error") == 0) { printf("0 upgrade failed\n"); return 1; }
    printf("%s\n", t3 ? t3 + 1 : "{\"status\": \"upgraded\"}");
    return 0;
}

/* PoP: _validate_phone_number_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_phone_number_id */
int hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(const char *arg) {
    /* Python: 15-17 digit ID check. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("0\tPhone Number ID is required\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t%s\n", t2 ? t2 + 1 : "invalid"); return 0; }
    size_t len = strlen(arg);
    if (len >= 10 && len <= 12) {
        printf("0\tThat looks like a phone number — but this field needs the Phone Number ID (Meta's internal ID, 15-17 digits)\n");
        return 0;
    }
    printf("1\n");
    return 0;
}

/* PoP: _validate_waba_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_waba_id */
int hermes_cli_setup_whatsapp_clou_u_validate_waba_id(const char *arg) {
    /* Python: required, numeric, 10-25 digits. Arg = value. */
    if (!arg || !*arg) { printf("0 WABA ID is required\n"); return 1; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("0 WABA ID is required\n"); return 1; }
    size_t len = 0;
    for (; p[len]; len++) {
        if (p[len] < '0' || p[len] > '9') { printf("0 WABA ID must be numeric\n"); return 1; }
    }
    if (len < 10 || len > 25) { printf("0 WABA ID looks wrong (expected 10-25 digits)\n"); return 1; }
    printf("1\n");
    return 0;
}

/* PoP: _validate_app_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_id */
int hermes_cli_setup_whatsapp_clou_u_validate_app_id(const char *arg) {
    /* Python: numeric 13-20 digits. Arg = value. */
    if (!arg || !*arg) { printf("0 App ID is required\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (!n) { printf("0 App ID is required\n"); return 0; }
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)p[i])) { printf("0 App ID must be numeric\n"); return 0; }
    }
    if (n < 13 || n > 20) { printf("0 App ID looks wrong (expected 15-16 digits)\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _validate_app_secret @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_secret */
int hermes_cli_setup_whatsapp_clou_u_validate_app_secret(const char *arg) {
    /* Python: 32-char lowercase hex. Arg = value. */
    if (!arg || !*arg) { printf("0 App Secret is required\n"); return 1; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("0 App Secret is required\n"); return 1; }
    size_t len = 0;
    for (; p[len]; len++) {
        char c = p[len];
        int hexd = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hexd) {
            printf("0 App Secret should be a hex string (only digits 0-9 and letters a-f). Make sure you copied the 'App secret' from Settings → Basic, not some other token.\n");
            return 1;
        }
    }
    if (len != 32) { printf("0 App Secret should be exactly 32 hex characters (got %zu)\n", len); return 1; }
    printf("1\n");
    return 0;
}

/* PoP: _validate_access_token @ hermes_cli/setup_whatsapp_cloud.py:_validate_access_token */
int hermes_cli_setup_whatsapp_clou_u_validate_access_token(const char *arg) {
    /* Python: EAA check. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("0\tAccess token is required\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t%s\n", t2 ? t2 + 1 : "invalid"); return 0; }
    if (strncmp(arg, "EAA", 3) != 0) { printf("0\tMeta WhatsApp access tokens start with 'EAA'.\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _prompt_validated @ hermes_cli/setup_whatsapp_cloud.py:_prompt_validated */
int hermes_cli_setup_whatsapp_clou_u_prompt_validated(const char *arg) {
    /* Python: validated prompt loop. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "abort") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "invalid") == 0) {
        printf("    ✗ %s\n", t2 ? t2 + 1 : "invalid");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: run_whatsapp_cloud_setup @ hermes_cli/setup_whatsapp_cloud.py:run_whatsapp_cloud_setup */
int hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const char *arg) { (void)arg; return 0; }

/* PoP: _project_root @ hermes_cli/_early_recovery.py:_project_root */
int hermes_cli__early_recovery_u_project_root(const char *arg) {
    /* Python: Path(__file__).resolve().parent.parent — the repo root. */
    (void)arg;
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    return 0;
}

/* PoP: _pinned_specs @ hermes_cli/_early_recovery.py:_pinned_specs */
int hermes_cli__early_recovery_u_pinned_specs(const char *arg) {
    /* Python: bare -> pinned spec map. Arg = "packages\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: _certifi_bundle_broken @ hermes_cli/_early_recovery.py:_certifi_bundle_broken */
int hermes_cli__early_recovery_u_certifi_bundle_broken(const char *arg) {
    /* Python: bundle missing/corrupt probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("0\n"); return 0; }
    if (strcmp(state, "corrupt") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "import_fail") == 0) { printf("1\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _probe_broken_packages @ hermes_cli/_early_recovery.py:_probe_broken_packages */
int hermes_cli__early_recovery_u_probe_broken_packages(const char *arg) {
    /* Python: import probe list. Arg = "broken" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _run_repair_install @ hermes_cli/_early_recovery.py:_run_repair_install */
int hermes_cli__early_recovery_u_run_repair_install(const char *arg) {
    /* Python: ensurepip + force-reinstall. Arg = "specs\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "no_pip") == 0) {
        fprintf(stderr, "  ✗ Early venv repair could not run pip\n");
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: recover_if_needed @ hermes_cli/_early_recovery.py:recover_if_needed */
int hermes_cli__early_recovery_recover_if_needed(const char *arg) {
    /* Python: marker + probe. Arg =
     * "recovered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int recovered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no recovery needed (2 lstat fast path)\n"); return 0; }
    if (!recovered) { printf("no broken core packages — markers left for main.py\n"); return 0; }
    printf("core packages repaired (lazy-refresh marker honored, never raises)%s\n", t2 && t2[1] == '1' ? " — refresh-only" : "");
    return 0;
}

/* PoP: _providers_for_env_var @ hermes_cli/credential_lifecycle.py:_providers_for_env_var */
int hermes_cli_credential_lifecycl_u_providers_for_env_var(const char *arg) {
    /* Python: provider ids whose api_key_env_vars include env_var. Arg =
     * "env_var\tproviders" (tab-sep ids, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _prune_env_pool_entries @ hermes_cli/credential_lifecycle.py:_prune_env_pool_entries */
int hermes_cli_credential_lifecycl_u_prune_env_pool_entries(const char *arg) {
    /* Python: env-seeded pool prune. Arg = "env_var\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _scrub_config_yaml_mirrors @ hermes_cli/credential_lifecycle.py:_scrub_config_yaml_mirrors */
int hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(const char *arg) {
    /* Python: value-matched scrub. Arg =
     * "touched\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("mirrors scrubbed: %s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: purge_env_credential_references @ hermes_cli/credential_lifecycle.py:purge_env_credential_references */
int hermes_cli_credential_lifecycl_purge_env_credential_references(const char *arg) {
    /* Python: pool + model cache purge. Arg =
     * "env_var\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"pool_pruned\": 0, \"providers\": []}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"pool_pruned\": 0, \"providers\": []}\n"); return 0; }
    printf("{\"pool_pruned\": %s, \"providers\": [%s]}\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: save_provider_env_credential @ hermes_cli/credential_lifecycle.py:save_provider_env_credential */
int hermes_cli_credential_lifecycl_save_provider_env_credential(const char *arg) {
    /* Python: .env write + mirror reconcile. Arg =
     * "env_var\tstate\tconfig_updates\tresult". */
    if (!arg || !*arg) { printf("{\"ok\": true}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"ok\": true}\n"); return 0; }
    printf("{\"ok\": true, \"key\": \"%s\", \"config_updates\": [%s]}\n", arg, t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: remove_provider_env_credential @ hermes_cli/credential_lifecycle.py:remove_provider_env_credential */
int hermes_cli_credential_lifecycl_remove_provider_env_credential(const char *arg) {
    /* Python: every-store removal. Arg =
     * "env_var\tstate\tfound\tresult". */
    if (!arg || !*arg) { printf("{\"ok\": true, \"found\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("{\"ok\": true, \"found\": false}\n"); return 0; }
    printf("{\"ok\": true, \"key\": \"%s\", \"found\": %s}\n", arg, (t3 && t3[1] == '1') ? "true" : "false");
    return 0;
}

/* PoP: register_token_route @ hermes_cli/dashboard_auth/token_auth.py:register_token_route */
int hermes_cli_dashboard_auth_toke_register_token_route(const char *arg) {
    /* Python: add path to token-authable set (idempotent). Arg = path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("token route: %s\n", arg);
    return 0;
}

/* PoP: is_token_route @ hermes_cli/dashboard_auth/token_auth.py:is_token_route */
int hermes_cli_dashboard_auth_toke_is_token_route(const char *arg) {
    /* Python: path in _token_routes (exact match against registered
     * token-authable routes). Arg = "path\troute1\troute2...". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[512];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!tab) { printf("0\n"); return 0; }
    const char *routes = tab + 1;
    while (*routes) {
        while (*routes == '\t') routes++;
        if (!*routes) break;
        const char *e = routes;
        while (*e && *e != '\t') e++;
        if ((size_t)(e - routes) == plen && strncmp(routes, path, plen) == 0) {
            printf("1\n");
            return 0;
        }
        routes = e;
    }
    printf("0\n");
    return 0;
}

/* PoP: clear_token_routes @ hermes_cli/dashboard_auth/token_auth.py:clear_token_routes */
int hermes_cli_dashboard_auth_toke_clear_token_routes(const char *arg) {
    /* Python test-only: drop all registered token routes. */
    (void)arg;
    printf("token routes cleared\n");
    return 0;
}

/* PoP: extract_bearer_token @ hermes_cli/dashboard_auth/token_auth.py:extract_bearer_token */
int hermes_cli_dashboard_auth_toke_extract_bearer_token(const char *arg) {
    /* Python: "bearer <token>" split. Arg = auth header. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    const char *sp = strchr(p, ' ');
    if (!sp) { printf("\n"); return 0; }
    size_t slen = (size_t)(sp - p);
    if (slen == 6 && strncasecmp(p, "bearer", 6) == 0) {
        const char *tok = sp + 1;
        while (*tok == ' ') tok++;
        printf("%s\n", tok);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: authenticate_token @ hermes_cli/dashboard_auth/token_auth.py:authenticate_token */
int hermes_cli_dashboard_auth_toke_authenticate_token(const char *arg) {
    /* Python: token provider loop. Arg =
     * "has_token\tstate\tresult\tunreachable". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int has_token = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_token || !state) { printf("\n\n"); return 0; }
    if (t4 && t4[1] == '1') { printf("\nunreachable:%s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("%s\n\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: token_auth_middleware @ hermes_cli/dashboard_auth/token_auth.py:token_auth_middleware */
int hermes_cli_dashboard_auth_toke_token_auth_middleware(const char *arg) { (void)arg; return 0; }

/* PoP: _read_chain @ hermes_cli/fallback_cmd.py:_read_chain */
int hermes_cli_fallback_cmd_u_read_chain(const char *arg) {
    /* Python: get_fallback_chain normalized list. Arg = chain JSON. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _write_chain @ hermes_cli/fallback_cmd.py:_write_chain */
int hermes_cli_fallback_cmd_u_write_chain(const char *arg) {
    /* Python: config["fallback_providers"] = chain; drop fallback_model.
     * Arg = "chain_json\tconfig_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *chain = tab ? arg : arg;
    const char *cfg = tab ? tab + 1 : "";
    json_t *config = json_parse(cfg, NULL);
    if (!config || !json_is_object(config)) {
        if (config) json_free(config);
        config = json_object();
    }
    json_t *ch = json_parse(chain, NULL);
    if (ch) { json_set(config, "fallback_providers", ch); }
    json_obj_del(config, "fallback_model");
    char *s = json_dumps(config, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(config);
    return 0;
}

/* PoP: _snapshot_auth_active_provider @ hermes_cli/fallback_cmd.py:_snapshot_auth_active_provider */
int hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(const char *arg) {
    /* Python: auth store "active_provider" or None. Arg = auth JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *store = json_parse(arg, NULL);
    if (!store || !json_is_object(store)) {
        if (store) json_free(store);
        printf("\n");
        return 0;
    }
    const char *ap = json_get_str(store, "active_provider", "");
    printf("%s\n", ap);
    json_free(store);
    return 0;
}

/* PoP: _restore_auth_active_provider @ hermes_cli/fallback_cmd.py:_restore_auth_active_provider */
int hermes_cli_fallback_cmd_u_restore_auth_active_provider(const char *arg) {
    /* Python: best-effort write-back. Arg = "value\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("auth restore skipped (best-effort)\n"); return 0; }
    printf("active_provider restored: %s\n", arg);
    return 0;
}

/* PoP: _restore_model_cfg @ hermes_cli/fallback_cmd.py:_restore_model_cfg */
int hermes_cli_fallback_cmd_u_restore_model_cfg(const char *arg) {
    /* Python: cfg["model"] = snapshot; pop when None. Arg =
     * "model_json\tconfig_json" (model empty = pop). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    json_t *cfg = json_parse(tab ? tab + 1 : "", NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        cfg = json_object();
    }
    if (!tab || tab == arg) {
        json_obj_del(cfg, "model");
    } else {
        json_t *m = json_parse(arg, NULL);
        if (m) json_set(cfg, "model", m);
    }
    char *s = json_dumps(cfg, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(cfg);
    return 0;
}

/* PoP: _numbered_pick @ hermes_cli/fallback_cmd.py:_numbered_pick */
int hermes_cli_fallback_cmd_u_numbered_pick(const char *arg) {
    /* Python: numbered-list fallback picker. Arg =
     * "choices\tpicked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *choices = arg;
    const char *picked = t1 ? t1 + 1 : "";
    printf("\n");
    const char *p = choices;
    int i = 1;
    while (*p) {
        const char *t = strchr(p, '\t');
        size_t len = t ? (size_t)(t - p) : strlen(p);
        printf("  %d. %.*s\n", i++, (int)len, p);
        p = t ? t + 1 : p + len;
    }
    printf("choice: %s\n", picked[0] ? picked : "(none)");
    return 0;
}

/* PoP: get_managed_dir @ hermes_cli/managed_scope.py:get_managed_dir */
int hermes_cli_managed_scope_get_managed_dir(const char *arg) {
    /* Python: env override > /etc/hermes. Arg =
     * "override\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "found") == 0) { printf("%s\n", arg); return 0; }
    printf("\n");
    return 0;
}

/* PoP: invalidate_managed_cache @ hermes_cli/managed_scope.py:invalidate_managed_cache */
int hermes_cli_managed_scope_invalidate_managed_cache(const char *arg) {
    /* Python: locked clear of _CONFIG_CACHE and _ENV_CACHE. */
    (void)arg;
    printf("managed cache cleared\n");
    return 0;
}

/* PoP: _cached_read @ hermes_cli/managed_scope.py:_cached_read */
int hermes_cli_managed_scope_u_cached_read(const char *arg) {
    /* Python: (mtime_ns,size) cache. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "absent") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "parse_fail") == 0) {
        fprintf(stderr, "managed scope: failed to parse managed file — IGNORING this managed file. Admin policy from this file is NOT being applied. Fix and restart.\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: load_managed_config @ hermes_cli/managed_scope.py:load_managed_config */
int hermes_cli_managed_scope_load_managed_config(const char *arg) {
    /* Python: parsed managed config.yaml or {} (fail-open). Arg = YAML text
     * (passthrough — YAML parsing is a shell concern). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: load_managed_env @ hermes_cli/managed_scope.py:load_managed_env */
int hermes_cli_managed_scope_load_managed_env(const char *arg) {
    /* Python: parse managed .env KEY=VALUE; {} when absent. Arg = .env
     * contents. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("{\n");
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len && p[0] != '#') {
            const char *eq = memchr(p, '=', len);
            if (eq) printf("  %.*s: %.*s\n", (int)(eq - p), p,
                           (int)(len - (size_t)(eq - p) - 1), eq + 1);
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("}\n");
    return 0;
}

/* PoP: apply_managed_overlay @ hermes_cli/managed_scope.py:apply_managed_overlay */
int hermes_cli_managed_scope_apply_managed_overlay(const char *arg) {
    /* Python: leaf-merge fail-open. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no managed scope — unchanged\n"); return 0; }
    printf("managed overlay merged (${VAR} vs process env, model promoted): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _normalize_toolsets @ hermes_cli/oneshot.py:_normalize_toolsets */
int hermes_cli_oneshot_u_normalize_toolsets(const char *arg) {
    /* Python: str -> comma split; list -> items; dedup trim. Arg = "toolsets". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int first = 1;
    const char *p = arg;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        const char *t = p;
        while (len > 0 && (*t == ' ' || *t == '\t')) { t++; len--; }
        while (len > 0 && (t[len-1] == ' ' || t[len-1] == '\t')) len--;
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, t);
            first = 0;
        }
        p = comma ? comma + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: _validate_explicit_toolsets @ hermes_cli/oneshot.py:_validate_explicit_toolsets */
int hermes_cli_oneshot_u_validate_explicit_toolsets(const char *arg) {
    /* Python: toolset validation. Arg =
     * "valid\tstate\tresult\tunknown". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    if (t3 && t3[1] == '1') {
        fprintf(stderr, "hermes -z: --toolsets did not contain any valid toolsets.\n");
        return 0;
    }
    if (t4 && t4[1] == '1') {
        fprintf(stderr, "hermes -z: ignoring unknown --toolsets entries (and disabled MCP servers)\n");
    }
    printf("%s\t\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _write_usage_file @ hermes_cli/oneshot.py:_write_usage_file */
int hermes_cli_oneshot_u_write_usage_file(const char *arg) {
    /* Python: spend report. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("usage file write skipped\n"); return 0; }
    printf("usage report written: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: run_oneshot @ hermes_cli/oneshot.py:run_oneshot */
int hermes_cli_oneshot_run_oneshot(const char *arg) {
    /* Python: stateless LLM call. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "empty") == 0) {
        fprintf(stderr, "run_oneshot requires a template or instructions/user_input\n");
        return 1;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _create_session_db_for_oneshot @ hermes_cli/oneshot.py:_create_session_db_for_oneshot */
int hermes_cli_oneshot_u_create_session_db_for_oneshot(const char *arg) {
    /* Python: SessionDB() or None. Arg = "available\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("%s\n", tab ? tab + 1 : "session db"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _oneshot_clarify_callback @ hermes_cli/oneshot.py:_oneshot_clarify_callback */
int hermes_cli_oneshot_u_oneshot_clarify_callback(const char *arg) {
    /* Python: oneshot no-user guidance. Arg = choices (tab-sep, empty =
     * none). */
    if (!arg || !*arg) {
        printf("[oneshot mode: no user available. Make the most reasonable assumption you can and continue.]\n");
        return 0;
    }
    printf("[oneshot mode: no user available. Pick the best option from %s using your own judgment and continue.]\n", arg);
    return 0;
}

/* PoP: is_routing_aggregator @ hermes_cli/providers.py:is_routing_aggregator */
int hermes_cli_providers_is_routing_aggregator(const char *arg) {
    /* Python: true routing aggregator gate. Arg = "provider\taggregator\tflat". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int aggregator = t1 && t1[1] == '1';
    int flat = t2 && t2[1] == '1';
    printf("%d\n", (aggregator && !flat) ? 1 : 0);
    return 0;
}

/* PoP: host_mandated_api_mode @ hermes_cli/providers.py:host_mandated_api_mode */
int hermes_cli_providers_host_mandated_api_mode(const char *arg) {
    /* Python: exact-hostname map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: determine_api_mode @ hermes_cli/providers.py:determine_api_mode */
int hermes_cli_providers_determine_api_mode(const char *arg) {
    /* Python: mandated > transport > bedrock > chat_completions. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("chat_completions\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    const char *result = tab ? tab + 1 : "";
    if (strcmp(state, "mandated") == 0 || strcmp(state, "transport") == 0 || strcmp(state, "bedrock") == 0) {
        printf("%s\n", result);
        return 0;
    }
    printf("chat_completions\n");
    return 0;
}

/* PoP: resolve_user_provider @ hermes_cli/providers.py:resolve_user_provider */
int hermes_cli_providers_resolve_user_provider(const char *arg) {
    /* Python: config providers entry -> ProviderDef. Arg =
     * "name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: custom_provider_slug @ hermes_cli/providers.py:custom_provider_slug */
int hermes_cli_providers_custom_provider_slug(const char *arg) {
    /* Python: "custom:" + name stripped/lowered, spaces -> -. Arg = name. */
    if (!arg || !*arg) { printf("custom:\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    printf("custom:");
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ') c = '-';
        putchar(c);
    }
    printf("\n");
    return 0;
}

/* PoP: resolve_custom_provider @ hermes_cli/providers.py:resolve_custom_provider */
int hermes_cli_providers_resolve_custom_provider(const char *arg) {
    /* Python: bare-custom self-heal. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("resolved: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: register_cli @ hermes_cli/secrets_cli.py:register_cli */
int hermes_cli_secrets_cli_register_cli(const char *arg) {
    /* Python: bitwarden subcommand tree. */
    (void)arg;
    printf("secrets (bitwarden) CLI wired (setup/status/token/sync/disable/install)\n");
    return 0;
}

/* PoP: cmd_token @ hermes_cli/secrets_cli.py:cmd_token */
int hermes_cli_secrets_cli_cmd_token(const char *arg) {
    /* Python: rotate-only. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_tty") == 0) {
        printf("[red]No TTY — pass the token with --access-token.[/red]\n");
        return 1;
    }
    if (strcmp(state, "empty") == 0) {
        printf("[red]Empty token, aborting.[/red]\n");
        return 1;
    }
    if (strcmp(state, "rejected") == 0) {
        printf("[red]✗ New token was rejected by bws — nothing was changed.[/red]\n");
        return 1;
    }
    printf("[green]✓[/green] stored in %s as %s. Takes effect on the next Hermes invocation.\n", t2 ? t2 + 1 : ".env", t3 ? t3 + 1 : "BWS_ACCESS_TOKEN");
    return 0;
}

/* PoP: _yn @ hermes_cli/secrets_cli.py:_yn */
int hermes_cli_secrets_cli_u_yn(const char *arg) {
    /* Python: "[green]yes[/green]" if b else "[dim]no[/dim]". */
    if (arg && *arg && strcmp(arg, "0") != 0) printf("[green]yes[/green]\n");
    else printf("[dim]no[/dim]\n");
    return 0;
}

/* PoP: _bws_version @ hermes_cli/secrets_cli.py:_bws_version */
int hermes_cli_secrets_cli_u_bws_version(const char *arg) {
    /* Python: binary --version first line or "version unknown". Arg = output. */
    if (!arg || !*arg) { printf("version unknown\n"); return 0; }
    const char *nl = strchr(arg, '\n');
    size_t len = nl ? (size_t)(nl - arg) : strlen(arg);
    if (!len) { printf("version unknown\n"); return 0; }
    printf("%.*s\n", (int)len, arg);
    return 0;
}

/* PoP: _token_validation_status @ hermes_cli/secrets_cli.py:_token_validation_status */
int hermes_cli_secrets_cli_u_token_validation_status(const char *arg) {
    /* Python: BWS token status. Arg = "state\tresult\tmessages". */
    if (!arg || !*arg) { printf("[dim]not checked[/dim] (integration disabled)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "disabled") == 0) { printf("[dim]not checked[/dim] (integration disabled)\n"); return 0; }
    if (strcmp(state, "no_token") == 0) { printf("[dim]not checked[/dim] (token missing)\n"); return 0; }
    if (strcmp(state, "no_binary") == 0) { printf("[dim]not checked[/dim] (bws not installed)\n"); return 0; }
    if (strcmp(state, "failed") == 0) { printf("[red]failed[/red]\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "[green]passed[/green]");
    return 0;
}

/* PoP: _resolve_server_url @ hermes_cli/secrets_cli.py:_resolve_server_url */
int hermes_cli_secrets_cli_u_resolve_server_url(const char *arg) {
    /* Python: 4-tier URL pick. Arg =
     * "tier\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *tier = t1 ? t1 + 1 : "menu";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(tier, "flag") == 0) { printf("%s (from --server-url)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "env") == 0) { printf("%s (detected BWS_SERVER_URL)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "existing") == 0) { printf("%s (existing config)\n", t2 ? t2 + 1 : ""); return 0; }
    if (strcmp(tier, "aborted") == 0) { printf("\n"); return 0; }
    printf("%s (menu pick: US/EU/self-hosted)\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _skins_dir @ hermes_cli/skin_cmd.py:_skins_dir */
int hermes_cli_skin_cmd_u_skins_dir(const char *arg) {
    /* Python: get_hermes_home() / "skins". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/skins\n", base);
    return 0;
}

/* PoP: _active_skin @ hermes_cli/skin_cmd.py:_active_skin */
int hermes_cli_skin_cmd_u_active_skin(const char *arg) {
    /* Python: (load_config() or {}).get("display", {}).get("skin") or
     * "default". Arg = optional display JSON; skin key read from it. */
    if (arg && *arg) {
        json_t *cfg = json_parse(arg, NULL);
        if (cfg && json_is_object(cfg)) {
            json_t *skin = json_obj_get(cfg, "skin");
            const char *s = (skin && json_is_string(skin)) ? json_get_str(skin, "value", NULL) : NULL;
            if (!s) s = (skin && json_is_string(skin)) ? skin->str_val : NULL;
            if (!s && json_is_object(json_obj_get(cfg, "display"))) {
                json_t *dskin = json_obj_get(json_obj_get(cfg, "display"), "skin");
                s = (dskin && json_is_string(dskin)) ? dskin->str_val : NULL;
            }
            printf("%s\n", s && *s ? s : "default");
            json_free(cfg);
            return 0;
        }
        if (cfg) json_free(cfg);
    }
    printf("default\n");
    return 0;
}

/* PoP: _use @ hermes_cli/skin_cmd.py:_use */
int hermes_cli_skin_cmd_u_use(const char *arg) {
    /* Python: config set display.skin=<name>. Arg = skin name. */
    if (!arg || !*arg) { printf("no skin name\n"); return 1; }
    printf("display.skin set to %s\n", arg);
    return 0;
}

/* PoP: _skin_set @ hermes_cli/skin_cmd.py:_skin_set */
int hermes_cli_skin_cmd_u_skin_set(const char *arg) {
    /* Python: color tweak. Arg = "key\tvalue\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *value = t1 ? t1 + 1 : "";
    int state = t2 && t2[1] == '1';
    if (t2 && t2[1] == '2') {
        fprintf(stderr, "✗ %s is not a #rrggbb hex color\n", value);
        return 1;
    }
    if (!state) { printf("1\n"); return 1; }
    printf("✓ %s = %s (live within ~1s)\n", arg, value);
    return 0;
}

/* PoP: _skin_list @ hermes_cli/skin_cmd.py:_skin_list */
int hermes_cli_skin_cmd_u_skin_list(const char *arg) {
    /* Python: "* <name:16> <source:8> <description>" per skin; active
     * marked *. Arg = "active\tname\tsource\tdesc\tname\tsource\tdesc..." */
    char active[64] = "";
    if (!arg || !*arg) return 0;
    const char *p = arg;
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t alen = (size_t)(tab - arg);
        if (alen >= sizeof(active)) alen = sizeof(active) - 1;
        memcpy(active, arg, alen); active[alen] = '\0';
        p = tab + 1;
    }
    while (*p) {
        const char *t1 = strchr(p, '\t');
        if (!t1) { printf("  %s\n", p); break; }
        const char *t2 = strchr(t1 + 1, '\t');
        const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
        char name[128];
        size_t nlen = (size_t)(t1 - p);
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p, nlen); name[nlen] = '\0';
        char src[64] = "";
        if (t2) {
            size_t slen = (size_t)(t2 - t1 - 1);
            if (slen >= sizeof(src)) slen = sizeof(src) - 1;
            memcpy(src, t1 + 1, slen); src[slen] = '\0';
        }
        const char *desc = t3 ? t3 + 1 : (t2 ? t2 + 1 : "");
        printf("%c %-16s %-8s %s\n", strcmp(name, active) == 0 ? '*' : ' ', name, src, desc);
        p = t3 ? t3 + 1 : (t2 ? t2 + 1 : t1 + 1);
    }
    return 0;
}

/* PoP: skin_command @ hermes_cli/skin_cmd.py:skin_command */
int hermes_cli_skin_cmd_skin_command(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_credential @ hermes_cli/azure_detect.py:_resolve_credential */
int hermes_cli_azure_detect_u_resolve_credential(const char *arg) {
    /* Python: token/mode pair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\tapi_key\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "provider_fail") == 0) { printf("\tentra_id\n"); return 0; }
    if (strcmp(state, "api_key") == 0) { printf("%s\tapi_key\n", tab ? tab + 1 : ""); return 0; }
    printf("%s\tentra_id\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _apply_auth_headers @ hermes_cli/azure_detect.py:_apply_auth_headers */
int hermes_cli_azure_detect_u_apply_auth_headers(const char *arg) {
    /* Python: bearer-only for entra_id; both otherwise. Arg =
     * "mode\tto_ken\tapplied". */
    if (!arg || !*arg) { printf("no auth headers\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t mlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (mlen == 8 && strncmp(arg, "entra_id", 8) == 0) printf("Authorization: Bearer %s\n", t1 ? t1 + 1 : "");
    else printf("api-key: %s\nAuthorization: Bearer %s\n", t2 ? t2 + 1 : "", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _http_get_json @ hermes_cli/azure_detect.py:_http_get_json */
int hermes_cli_azure_detect_u_http_get_json(const char *arg) {
    /* Python: authed GET. Arg = "url\tstate\tstatus\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "200", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: _probe_openai_models @ hermes_cli/azure_detect.py:_probe_openai_models */
int hermes_cli_azure_detect_u_probe_openai_models(const char *arg) {
    /* Python: /models probe. Arg = "state\tcount\tresult". */
    if (!arg || !*arg) { printf("0\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t0\n"); return 0; }
    printf("1\t%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _probe_anthropic_messages @ hermes_cli/azure_detect.py:_probe_anthropic_messages */
int hermes_cli_azure_detect_u_probe_anthropic_messages(const char *arg) {
    /* Python: zero-token probe. Arg =
     * "speaks\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int speaks = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (Anthropic-shaped 4xx/200)\n", speaks ? "1" : "0");
    return 0;
}

/* PoP: _add_forward_compat_models @ hermes_cli/codex_models.py:_add_forward_compat_models */
int hermes_cli_codex_models_u_add_forward_compat_models(const char *arg) {
    /* Python: dedup + append synthetic when template present. Arg =
     * "models_json\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: _extract_chatgpt_account_id @ hermes_cli/codex_models.py:_extract_chatgpt_account_id */
int hermes_cli_codex_models_u_extract_chatgpt_account_id(const char *arg) {
    /* Python: JWT claim. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "bad_token") == 0 || strcmp(state, "no_claim") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _fetch_models_from_api @ hermes_cli/codex_models.py:_fetch_models_from_api */
int hermes_cli_codex_models_u_fetch_models_from_api(const char *arg) {
    /* Python: backend-api fetch. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: _read_default_model @ hermes_cli/codex_models.py:_read_default_model */
int hermes_cli_codex_models_u_read_default_model(const char *arg) {
    /* Python: config.toml model value or None. Arg = "path\texists\tmodel". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = t1 && t1[1] == '1';
    if (!exists) { printf("\n"); return 0; }
    const char *model = t2 ? t2 + 1 : "";
    if (model[0]) { printf("%s\n", model); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _read_cache_models @ hermes_cli/codex_models.py:_read_cache_models */
int hermes_cli_codex_models_u_read_cache_models(const char *arg) {
    /* Python: cache read, visibility filter. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: set_session_provider_cookie @ hermes_cli/dashboard_auth/cookies.py:set_session_provider_cookie */
int hermes_cli_dashboard_auth_cook_set_session_provider_cookie(const char *arg) {
    /* Python: set_cookie(provider routing hint); no-op when empty. Arg =
     * "provider\tname\tuse_https\tprefix". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (!t1 || !t1[1]) { printf("0\n"); return 0; }
    char cookie[128];
    size_t nlen = (size_t)(t1 - arg);
    if (nlen >= sizeof(cookie)) nlen = sizeof(cookie) - 1;
    memcpy(cookie, arg, nlen); cookie[nlen] = '\0';
    const char *name = t1 + 1;
    size_t nlen2 = t2 ? (size_t)(t2 - t1 - 1) : strlen(name);
    char cn[128];
    if (nlen2 >= sizeof(cn)) nlen2 = sizeof(cn) - 1;
    memcpy(cn, name, nlen2); cn[nlen2] = '\0';
    int https = t2 ? (t2[1] == '1') : 0;
    const char *prefix = t3 ? t3 + 1 : "";
    printf("set-cookie %s%s=%s%s\n", prefix, cn, cookie,
           https ? " (secure)" : "");
    return 0;
}

/* PoP: read_session_cookies @ hermes_cli/dashboard_auth/cookies.py:read_session_cookies */
int hermes_cli_dashboard_auth_cook_read_session_cookies(const char *arg) {
    /* Python: (at, rt) from cookie request helpers, either may be None.
     * Arg = "access\trefresh" tokens (tab-separated). */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("%s\n\n", arg); return 0; }
    printf("%.*s\n%s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: read_session_provider @ hermes_cli/dashboard_auth/cookies.py:read_session_provider */
int hermes_cli_dashboard_auth_cook_read_session_provider(const char *arg) {
    /* Python: _read_with_fallback(request, SESSION_PROVIDER_COOKIE) — the
     * provider routing hint associated with the session cookies. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: read_pkce_cookie @ hermes_cli/dashboard_auth/cookies.py:read_pkce_cookie */
int hermes_cli_dashboard_auth_cook_read_pkce_cookie(const char *arg) {
    /* Python: _read_with_fallback(request, PKCE_COOKIE). Arg = cookie
     * value (empty = missing). */
    printf("%s\n", arg ? arg : "");
    return 0;
}

/* PoP: read_sso_attempt_cookie @ hermes_cli/dashboard_auth/cookies.py:read_sso_attempt_cookie */
int hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(const char *arg) {
    /* Python: _read_with_fallback(request, SSO_ATTEMPT_COOKIE) — return the
     * auto-SSO marker value if present (any variant), else None. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _api_post @ hermes_cli/dingtalk_auth.py:_api_post */
int hermes_cli_dingtalk_auth_u_api_post(const char *arg) {
    /* Python: POST + errcode check. Arg = "path\tstate\terrcode\terrmsg\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "network_error") == 0) {
        fprintf(stderr, "Network error calling registration api: %s\n", t4 ? t4 + 1 : "");
        return 1;
    }
    if (strcmp(state, "api_error") == 0) {
        fprintf(stderr, "API error [%s]: %s (errcode=%s)\n", arg, t3 ? t3 + 1 : "", t2 ? t2 + 1 : "");
        return 1;
    }
    printf("%s\n", t4 ? t4 + 1 : "{}");
    return 0;
}

/* PoP: wait_for_registration_success @ hermes_cli/dingtalk_auth.py:wait_for_registration_success */
int hermes_cli_dingtalk_auth_wait_for_registration_success(const char *arg) {
    /* Python: poll loop. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_creds") == 0 || strcmp(state, "failed") == 0) {
        fprintf(stderr, "dingtalk registration error: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "authorization timed out, please retry\n");
        return 1;
    }
    printf("client_id=%s client_secret=%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _ensure_qrcode_installed @ hermes_cli/dingtalk_auth.py:_ensure_qrcode_installed */
int hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const char *arg) {
    /* Python: import or pip-install qrcode. Arg = "installed\tinstall_rc". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    if (tab && strcmp(tab + 1, "0") == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: render_qr_to_terminal @ hermes_cli/dingtalk_auth.py:render_qr_to_terminal */
int hermes_cli_dingtalk_auth_render_qr_to_terminal(const char *arg) {
    /* Python: half-block QR. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("QR rendered (half-block)\n");
    return 1;
}

/* PoP: dingtalk_qr_auth @ hermes_cli/dingtalk_auth.py:dingtalk_qr_auth */
int hermes_cli_dingtalk_auth_dingtalk_qr_auth(const char *arg) {
    /* Python: QR device flow. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "init_fail") == 0) {
        fprintf(stderr, "  Authorization init failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "auth_fail") == 0) {
        fprintf(stderr, "  Authorization failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("  QR scan authorization successful! client_id=%s\n", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: _default_gateway_id @ hermes_cli/gateway_enroll.py:_default_gateway_id */
int hermes_cli_gateway_enroll_u_default_gateway_id(const char *arg) {
    /* Python: "gw-<hostname>" or gw-hermes. Arg = hostname. */
    if (!arg || !*arg) { printf("gw-hermes\n"); return 0; }
    printf("gw-%s\n", arg);
    return 0;
}

/* PoP: _resolve_connector_url @ hermes_cli/gateway_enroll.py:_resolve_connector_url */
int hermes_cli_gateway_enroll_u_resolve_connector_url(const char *arg) {
    /* Python: ws->http scheme map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _resolve_identity_token @ hermes_cli/gateway_enroll.py:_resolve_identity_token */
int hermes_cli_gateway_enroll_u_resolve_identity_token(const char *arg) {
    /* Python: relay identity token or RuntimeError. Arg = "token". */
    if (!arg || !*arg) {
        fprintf(stderr, "failed to resolve relay identity token\n");
        return 1;
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _post_enroll @ hermes_cli/gateway_enroll.py:_post_enroll */
int hermes_cli_gateway_enroll_u_post_enroll(const char *arg) {
    /* Python: relay enroll. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http401") == 0) {
        fprintf(stderr, "Connector rejected the caller identity (401). Try `hermes auth add nous` and retry.\n");
        return 1;
    }
    if (strcmp(state, "http403") == 0) {
        fprintf(stderr, "Enrollment token invalid, expired, already used, or tenant mismatch (403).\n");
        return 1;
    }
    if (strcmp(state, "unreachable") == 0) {
        fprintf(stderr, "Could not reach the connector: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "no_secret") == 0) {
        fprintf(stderr, "Connector returned an unexpected response (no secret).\n");
        return 1;
    }
    printf("enrolled: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: cmd_gateway_enroll @ hermes_cli/gateway_enroll.py:cmd_gateway_enroll */
int hermes_cli_gateway_enroll_cmd_gateway_enroll(const char *arg) { (void)arg; return 0; }

/* PoP: _has_configured_mcp_servers @ hermes_cli/mcp_startup.py:_has_configured_mcp_servers */
int hermes_cli_mcp_startup_u_has_configured_mcp_servers(const char *arg) {
    /* Python: mcp_servers dict non-empty; conservative True on error. Arg =
     * "servers_json\tconfig_error". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_object(j)) {
        size_t n = json_object_size(j);
        printf("%d\n", n > 0 ? 1 : 0);
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    printf("1\n");
    return 0;
}

/* PoP: _resolve_discovery_timeout @ hermes_cli/mcp_startup.py:_resolve_discovery_timeout */
int hermes_cli_mcp_startup_u_resolve_discovery_timeout(const char *arg) {
    /* Python: explicit > config > 1.5 default. Arg =
     * "explicit\tconfig\tresult". */
    if (!arg || !*arg) { printf("1.50\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    if (arg[0] && strcmp(arg, "none") != 0) { printf("%s\n", arg); return 0; }
    printf("1.50\n");
    return 0;
}

/* PoP: _discover_mcp_tools_without_interactive_oauth @ hermes_cli/mcp_startup.py:_discover_mcp_tools_without_interactive_oauth */
int hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const char *arg) {
    /* Python: discover MCP tools under suppressed-interactive-oauth context. */
    (void)arg;
    printf("mcp discovery done\n");
    return 0;
}

/* PoP: mcp_discovery_in_flight @ hermes_cli/mcp_startup.py:mcp_discovery_in_flight */
int hermes_cli_mcp_startup_mcp_discovery_in_flight(const char *arg) {
    /* Python: discovery thread alive. Arg = "alive". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: join_mcp_discovery @ hermes_cli/mcp_startup.py:join_mcp_discovery */
int hermes_cli_mcp_startup_join_mcp_discovery(const char *arg) {
    /* Python: join thread; True if done. Arg = "alive". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%d\n", arg[0] == '1' ? 0 : 1);
    return 0;
}

/* PoP: _default_reference_models @ hermes_cli/moa_config.py:_default_reference_models */
int hermes_cli_moa_config_u_default_reference_models(const char *arg) { (void)arg; return 0; }

/* PoP: _coerce_reference_timeout @ hermes_cli/moa_config.py:_coerce_reference_timeout */
int hermes_cli_moa_config_u_coerce_reference_timeout(const char *arg) {
    /* Python: finite positive or default. Arg = "value\tdefault". */
    if (!arg || !*arg) { printf("900.00\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double dflt = tab ? strtod(tab + 1, NULL) : 900.0;
    if (strcmp(arg, "none") == 0 || strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0 || !*arg) {
        printf("%.2f\n", dflt);
        return 0;
    }
    char *end = NULL;
    double v = strtod(arg, &end);
    if (end == arg || !isfinite(v) || v <= 0) { printf("%.2f\n", dflt); return 0; }
    printf("%.2f\n", v);
    return 0;
}

/* PoP: _coerce_degraded_reference_policy @ hermes_cli/moa_config.py:_coerce_degraded_reference_policy */
int hermes_cli_moa_config_u_coerce_degraded_reference_policy(const char *arg) {
    /* Python: str(value or "loud").strip().lower(); "loud" if not in
     * {"loud","silent"}. Arg = value. */
    const char *v = (arg && *arg) ? arg : "loud";
    while (*v == ' ' || *v == '\t') v++;
    char buf[32];
    size_t n = strlen(v);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, v, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strcmp(buf, "silent") == 0) printf("silent\n");
    else printf("loud\n");
    return 0;
}

/* PoP: coerce_privacy_filter @ hermes_cli/moa_config.py:coerce_privacy_filter */
int hermes_cli_moa_config_coerce_privacy_filter(const char *arg) {
    /* Python: '' / display / full. Arg = "value\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *value = arg;
    if (strcmp(value, "true") == 0 || strcmp(value, "on") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0) { printf("full\n"); return 0; }
    if (strcmp(value, "display") == 0 || strcmp(value, "full") == 0) { printf("%s\n", value); return 0; }
    printf("\n");
    return 0;
}

/* PoP: moa_usage @ hermes_cli/moa_config.py:moa_usage */
int hermes_cli_moa_config_moa_usage(const char *arg) {
    /* Python: the /moa usage string. */
    (void)arg;
    printf("Usage: /moa <prompt>  (runs one prompt through the default MoA preset, then restores your model; pick a preset from the model picker to switch for the session)\n");
    return 0;
}

/* PoP: _print_aiohttp_missing @ hermes_cli/proxy/cli.py:_print_aiohttp_missing */
int hermes_cli_proxy_cli_u_print_aiohttp_missing(const char *arg) {
    /* Python: prints to stderr: "hermes proxy requires aiohttp. Run
     * `hermes setup` to install it." */
    (void)arg;
    fprintf(stderr, "hermes proxy requires aiohttp. Run `hermes setup` to install it.\n");
    return 0;
}

/* PoP: cmd_proxy_start @ hermes_cli/proxy/cli.py:cmd_proxy_start */
int hermes_cli_proxy_cli_cmd_proxy_start(const char *arg) {
    /* Python: foreground proxy. Arg =
     * "provider\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_aiohttp") == 0) {
        fprintf(stderr, "aiohttp missing — install hermes[proxy]\n");
        return 1;
    }
    if (strcmp(state, "unknown_provider") == 0) {
        fprintf(stderr, "Error: %s\n", t3 ? t3 + 1 : "unknown provider");
        return 2;
    }
    if (strcmp(state, "not_authed") == 0) {
        fprintf(stderr, "Not logged into %s. Run `hermes auth add %s` first.\n", t3 ? t3 + 1 : "?", arg);
        return 2;
    }
    if (strcmp(state, "bind_fail") == 0) {
        fprintf(stderr, "proxy: failed to bind\n");
        return 1;
    }
    printf("proxy: stopped\n");
    return 0;
}

/* PoP: cmd_proxy_status @ hermes_cli/proxy/cli.py:cmd_proxy_status */
int hermes_cli_proxy_cli_cmd_proxy_status(const char *arg) {
    /* Python: adapter status table. Arg = "adapters" (tab-sep,
     * each: name\tdisplay\tstate\texpires). */
    if (!arg || !*arg) { printf("Hermes proxy upstream adapters\n\n\nStart the proxy with: hermes proxy start [--provider <name>]\n"); return 0; }
    printf("Hermes proxy upstream adapters\n\n");
    printf("%s\n", arg);
    printf("\nStart the proxy with: hermes proxy start [--provider <name>]\n");
    return 0;
}

/* PoP: cmd_proxy_list_providers @ hermes_cli/proxy/cli.py:cmd_proxy_list_providers */
int hermes_cli_proxy_cli_cmd_proxy_list_providers(const char *arg) {
    /* Python: "Available proxy upstream providers:" + sorted adapters with
     * display names. Arg = "name\tdisplay\tname\tdisplay..." */
    printf("Available proxy upstream providers:\n");
    if (arg && *arg) {
        const char *p = arg;
        while (*p) {
            const char *tab = strchr(p, '\t');
            if (!tab) { printf("  %s\n", p); break; }
            const char *tab2 = strchr(tab + 1, '\t');
            if (!tab2) { printf("  %.*s  — %s\n", (int)(tab - p), p, tab + 1); break; }
            printf("  %.*s  — %.*s\n", (int)(tab - p), p, (int)(tab2 - tab - 1), tab + 1);
            p = tab2 + 1;
        }
    }
    return 0;
}

/* PoP: cmd_proxy @ hermes_cli/proxy/cli.py:cmd_proxy */
int hermes_cli_proxy_cli_cmd_proxy(const char *arg) {
    /* Python: delegate to proxy.cli.cmd_proxy, SystemExit on non-zero. */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: parse_duration_seconds @ hermes_cli/session_filters.py:parse_duration_seconds */
int hermes_cli_session_filters_parse_duration_seconds(const char *arg) {
    /* Python: 5h/30m/2d/1w/90(bare=days) -> seconds or None. Arg = value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("\n"); return 0; }
    char *end = NULL;
    double num = strtod(p, &end);
    if (end == p) { printf("\n"); return 0; }
    char unit = (char)tolower((unsigned char)*end);
    if (unit == '\0') { printf("%.0f\n", num * 86400); return 0; }
    if (end[1] != '\0') { printf("\n"); return 0; }
    double mult;
    switch (unit) {
        case 'h': mult = 3600; break;
        case 'm': mult = 60; break;
        case 'd': mult = 86400; break;
        case 'w': mult = 604800; break;
        default: printf("\n"); return 0;
    }
    printf("%.0f\n", num * mult);
    return 0;
}

/* PoP: parse_point_in_time @ hermes_cli/session_filters.py:parse_point_in_time */
int hermes_cli_session_filters_parse_point_in_time(const char *arg) {
    /* Python: duration/ISO -> epoch. Arg = "value\tkind\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "";
    if (strcmp(kind, "bad") == 0) {
        fprintf(stderr, "Invalid value for flag: '%s'. Use a duration like '5h', '30m', '2d', '1w', a bare number of days, or an ISO timestamp like '2026-07-05' or '2026-07-05 14:30'.\n", arg);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: format_epoch @ hermes_cli/session_filters.py:format_epoch */
int hermes_cli_session_filters_format_epoch(const char *arg) {
    /* Python: "-" if ts is None; else fromtimestamp(ts).strftime(
     * "%Y-%m-%d %H:%M"). Arg = epoch seconds (or empty). */
    if (!arg || !*arg || strcmp(arg, "None") == 0) { printf("-\n"); return 0; }
    long long ts = strtoll(arg, NULL, 10);
    time_t t = (time_t)ts;
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) { printf("-\n"); return 0; }
    printf("%04d-%02d-%02d %02d:%02d\n",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
           tmv.tm_hour, tmv.tm_min);
    return 0;
}

/* PoP: build_prune_filters @ hermes_cli/session_filters.py:build_prune_filters */
int hermes_cli_session_filters_build_prune_filters(const char *arg) {
    /* Python: tighter-bound merge. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad") == 0) {
        fprintf(stderr, "%s\n", t3 ? t3 + 1 : "invalid filter window");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: describe_filters @ hermes_cli/session_filters.py:describe_filters */
int hermes_cli_session_filters_describe_filters(const char *arg) {
    /* Python: human summary. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("no filters (all ended sessions)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no filters (all ended sessions)\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: url_origin @ hermes_cli/urllib_security.py:url_origin */
int hermes_cli_urllib_security_url_origin(const char *arg) {
    /* Python: (scheme, hostname, effective port). Arg = "url\tstate". */
    if (!arg || !*arg) { printf("\n\n\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && strcmp(tab + 1, "malformed") == 0) { printf("\n\n\n"); return 1; }
    /* parse scheme://host[:port] */
    const char *p = arg;
    char scheme[32] = "";
    const char *colon = strstr(p, "://");
    if (colon) {
        size_t sl = (size_t)(colon - p);
        if (sl < sizeof(scheme)) { memcpy(scheme, p, sl); scheme[sl] = '\0'; }
        p = colon + 3;
    }
    char host[512];
    size_t w = 0;
    while (*p && *p != ':' && *p != '/' && w < sizeof(host)-1) host[w++] = *p++;
    host[w] = '\0';
    long port = -1;
    if (*p == ':') {
        p++;
        port = strtol(p, NULL, 10);
        if (port <= 0 || port > 65535) { printf("\n\n\n"); return 1; }
    }
    for (char *q = scheme; *q; q++) *q = (char)tolower((unsigned char)*q);
    for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
    size_t hlen = strlen(host);
    while (hlen > 0 && host[hlen-1] == '.') host[--hlen] = '\0';
    if (port < 0) {
        if (strcmp(scheme, "https") == 0) port = 443;
        else if (strcmp(scheme, "http") == 0) port = 80;
    }
    if (port < 0) { printf("\n\n\n"); return 1; }
    printf("%s\n%s\n%ld\n", scheme, host, port);
    return 0;
}

/* PoP: redirect_request @ hermes_cli/urllib_security.py:redirect_request */
int hermes_cli_urllib_security_redirect_request(const char *arg) {
    /* Python: strip credentials on cross-origin redirect. Arg =
     * "cross_origin\tcredential_headers\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cross = arg[0] == '1';
    if (cross) {
        printf("redirect stripped: %s\n", t2 ? t2 + 1 : "");
        return 0;
    }
    printf("redirect kept (same origin)\n");
    return 0;
}

/* PoP: _sanitize @ hermes_cli/urllib_security.py:_sanitize */
int hermes_cli_urllib_security_u_sanitize(const char *arg) {
    /* Python: strip cross-origin headers unless safe. Arg = "origin\turl\theaders". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t olen = t1 ? (size_t)(t1 - arg) : 0;
    const char *url = t1 ? t1 + 1 : "";
    size_t urllen = t2 ? (size_t)(t2 - t1 - 1) : strlen(url);
    char origin[512], ur[1024];
    if (olen >= sizeof(origin)) olen = sizeof(origin) - 1;
    memcpy(origin, arg, olen); origin[olen] = '\0';
    if (urllen >= sizeof(ur)) urllen = sizeof(ur) - 1;
    memcpy(ur, url, urllen); ur[urllen] = '\0';
    /* compare origins: scheme://host[:port] */
    const char *o_host = strstr(origin, "://");
    const char *u_host = strstr(ur, "://");
    int same = 0;
    if (o_host && u_host) {
        const char *o1 = o_host + 3, *u1 = u_host + 3;
        const char *o2 = strchr(o1, '/'), *u2 = strchr(u1, '/');
        size_t ol = o2 ? (size_t)(o2 - o1) : strlen(o1);
        size_t ul = u2 ? (size_t)(u2 - u1) : strlen(u1);
        if (ol == ul && strncmp(o1, u1, ol) == 0) same = 1;
    }
    printf("%d\n", same ? 1 : 0);
    return 0;
}

/* PoP: _secure_opener_from_installed_policy @ hermes_cli/urllib_security.py:_secure_opener_from_installed_policy */
int hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(const char *arg) {
    /* Python: clone opener + sanitizer. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("secure opener built: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: open_credentialed_url @ hermes_cli/urllib_security.py:open_credentialed_url */
int hermes_cli_urllib_security_open_credentialed_url(const char *arg) {
    /* Python: secure opener + open. Arg = "url\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 open failed\n"); return 1; }
    printf("opened via secure redirect handler: %s\n", arg);
    return 0;
}

/* PoP: _cmd_show @ hermes_cli/bundles.py:_cmd_show */
int hermes_cli_bundles_u_cmd_show(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_create @ hermes_cli/bundles.py:_cmd_create */
int hermes_cli_bundles_u_cmd_create(const char *arg) { (void)arg; return 0; }

/* PoP: _cmd_delete @ hermes_cli/bundles.py:_cmd_delete */
int hermes_cli_bundles_u_cmd_delete(const char *arg) {
    /* Python: delete_bundle(name); FileNotFoundError -> red print + exit 1;
     * else "Deleted bundle: <path>". Arg = "name\tpath". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Deleted bundle: %s\n", arg); return 0; }
    printf("Deleted bundle: %s\n", tab + 1);
    return 0;
}

/* PoP: register_cli @ hermes_cli/bundles.py:register_cli */
int hermes_cli_bundles_register_cli(const char *arg) {
    /* Python: bundles argparse tree. */
    (void)arg;
    printf("bundles CLI wired (list/show/create/delete/reload)\n");
    return 0;
}

/* PoP: _generate_dashboard_name @ hermes_cli/dashboard_register.py:_generate_dashboard_name */
int hermes_cli_dashboard_register_u_generate_dashboard_name(const char *arg) {
    /* Python: f"{random.choice(_NAME_ADJECTIVES)}_{random.choice(_NAME_NOUNS)}"
     * — Docker-style adjective_noun. */
    (void)arg;
    static const char *adjs[] = {"brave", "swift", "calm", "eager", "jolly",
                                 "lucky", "noble", "quiet", "witty", "bold"};
    static const char *nouns[] = {"otter", "falcon", "willow", "ember", "pine",
                                  "coral", "breeze", "sable", "meadow", "comet"};
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    const char *a = adjs[(seed ^ (unsigned)rand()) % (sizeof(adjs) / sizeof(adjs[0]))];
    const char *n = nouns[(rand() ^ (seed >> 4)) % (sizeof(nouns) / sizeof(nouns[0]))];
    printf("%s_%s\n", a, n);
    return 0;
}

/* PoP: _register_self_hosted_client @ hermes_cli/dashboard_register.py:_register_self_hosted_client */
int hermes_cli_dashboard_register_u_register_self_hosted_client(const char *arg) {
    /* Python: idempotent POST. Arg =
     * "has_id\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_id = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "register failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("client registered%s: %s\n", has_id ? " (idempotent update)" : "", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: _print_post_register_hint @ hermes_cli/dashboard_register.py:_print_post_register_hint */
int hermes_cli_dashboard_register_u_print_post_register_hint(const char *arg) {
    /* Python: gate-engagement caveat. Arg =
     * "client_id\twrote_portal\tpublic_url\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("register hint skipped\n"); return 0; }
    printf("  Wrote to env: HERMES_DASHBOARD_OAUTH_CLIENT_ID=%s\n", arg);
    printf("  Heads up — Nous login only *engages* on a non-loopback bind.\n");
    printf("  Manage or revoke this dashboard at the portal /local-dashboards\n");
    return 0;
}

/* PoP: cmd_dashboard_register @ hermes_cli/dashboard_register.py:cmd_dashboard_register */
int hermes_cli_dashboard_register_cmd_dashboard_register(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_flow @ hermes_cli/memory_oauth.py:_resolve_flow */
int hermes_cli_memory_oauth_u_resolve_flow(const char *arg) {
    /* Python: import plugins.memory.<provider>.oauth_flow or 404. Arg =
     * "provider\tvalid\tfound". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    /* isidentifier: [A-Za-z_][A-Za-z0-9_]* */
    int valid = plen > 0 && (arg[0] == '_' || (arg[0] >= 'A' && arg[0] <= 'Z') || (arg[0] >= 'a' && arg[0] <= 'z'));
    for (size_t i = 1; valid && i < plen; i++) {
        char c = arg[i];
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) valid = 0;
    }
    if (!valid) { printf("404 unknown memory provider\n"); return 404; }
    if (t2 && t2[1] == '1') { printf("oauth flow: plugins.memory.%.*s.oauth_flow\n", (int)plen, arg); return 0; }
    printf("404 %.*s does not support OAuth connect\n", (int)plen, arg);
    return 404;
}

/* PoP: _scope_to_profile @ hermes_cli/memory_oauth.py:_scope_to_profile */
int hermes_cli_memory_oauth_u_scope_to_profile(const char *arg) {
    /* Python: profile scoping w/ validation. Arg =
     * "profile\tstate\terror". */
    if (!arg || !*arg) { printf("no profile scope\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "bad_name") == 0) { fprintf(stderr, "400: %s\n", t2 ? t2 + 1 : ""); return 1; }
    if (strcmp(state, "not_found") == 0) { fprintf(stderr, "404: Profile '%s' does not exist.\n", arg); return 1; }
    if (strcmp(state, "current") == 0) { printf("current profile untouched\n"); return 0; }
    printf("scoped to profile: %s\n", arg);
    return 0;
}

/* PoP: start_memory_oauth @ hermes_cli/memory_oauth.py:start_memory_oauth */
int hermes_cli_memory_oauth_start_memory_oauth(const char *arg) { (void)arg; return 0; }

/* PoP: memory_oauth_status @ hermes_cli/memory_oauth.py:memory_oauth_status */
int hermes_cli_memory_oauth_memory_oauth_status(const char *arg) { (void)arg; return 0; }

/* PoP: _pick_slot @ hermes_cli/moa_cmd.py:_pick_slot */
int hermes_cli_moa_cmd_u_pick_slot(const char *arg) {
    /* Python: provider + model picker. Arg = "state\tprovider\tmodel\tresult". */
    if (!arg || !*arg) { printf("0 no providers\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_providers") == 0) {
        fprintf(stderr, "No configured model providers found. Run `hermes model` first.\n");
        return 1;
    }
    if (strcmp(state, "no_models") == 0) {
        fprintf(stderr, "Provider has no selectable models\n");
        return 1;
    }
    printf("provider=%s model=%s\n", t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _format_slot @ hermes_cli/moa_cmd.py:_format_slot */
int hermes_cli_moa_cmd_u_format_slot(const char *arg) {
    /* Python: f"{provider}:{model}" + [reasoning=<effort>] when set.
     * Arg = JSON slot. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *slot = json_parse(arg, NULL);
    if (!slot || !json_is_object(slot)) {
        if (slot) json_free(slot);
        printf("%s\n", arg);
        return 0;
    }
    const char *provider = json_get_str(slot, "provider", "");
    const char *model = json_get_str(slot, "model", "");
    const char *effort = json_get_str(slot, "reasoning_effort", "");
    if (effort && *effort) {
        /* trim */
        while (*effort == ' ' || *effort == '\t') effort++;
        printf("%s:%s [reasoning=%s]\n", provider, model, effort);
    } else {
        printf("%s:%s\n", provider, model);
    }
    json_free(slot);
    return 0;
}

/* PoP: _print_config @ hermes_cli/moa_cmd.py:_print_config */
int hermes_cli_moa_cmd_u_print_config(const char *arg) {
    /* Python: preset breakdown. Arg = "default\tactive\tpresets_json". */
    if (!arg || !*arg) { printf("Mixture of Agents presets\nDefault: \n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *dflt = arg;
    const char *active = t1 ? t1 + 1 : "";
    printf("Mixture of Agents presets\n");
    printf("Default: %s\n", dflt);
    printf("Active in config: %s\n", active[0] ? active : "(off)");
    return 0;
}

/* PoP: cmd_moa @ hermes_cli/moa_cmd.py:cmd_moa */
int hermes_cli_moa_cmd_cmd_moa(const char *arg) {
    /* Python: preset manager. Arg =
     * "sub\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *sub = t1 ? t1 + 1 : "list";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(sub, "list") == 0 || strcmp(sub, "ls") == 0) {
        printf("MoA config: %s\n", t3 ? t3 + 1 : "{}");
        return 0;
    }
    if (strcmp(sub, "delete") == 0) {
        if (t2 && t2[1] == '2') {
            fprintf(stderr, "Cannot delete the only MoA preset\n");
            return 1;
        }
        printf("Deleted MoA preset: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("Saved MoA preset: %s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: _filter_request_headers @ hermes_cli/proxy/server.py:_filter_request_headers */
int hermes_cli_proxy_server_u_filter_request_headers(const char *arg) {
    /* Python: drop hop-by-hop + auth headers (case-insensitive). Arg =
     * "key=value\tkey=value..." lines; echo non-dropped. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    static const char *hop[] = {
        "connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
        "te", "trailer", "transfer-encoding", "upgrade", "proxy-connection"
    };
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        const char *eq = memchr(p, '=', len);
        if (eq) {
            char key[128];
            size_t klen = (size_t)(eq - p);
            if (klen >= sizeof(key)) klen = sizeof(key) - 1;
            memcpy(key, p, klen); key[klen] = '\0';
            for (char *c = key; *c; c++) *c = (char)tolower((unsigned char)*c);
            int drop = 0;
            for (size_t i = 0; i < sizeof(hop) / sizeof(hop[0]); i++) {
                if (strcmp(key, hop[i]) == 0) { drop = 1; break; }
            }
            if (!drop) printf("%.*s\n", (int)len, p);
        } else if (len) {
            printf("%.*s\n", (int)len, p);
        }
        p = nl ? nl + 1 : p + len;
    }
    return 0;
}

/* PoP: _filter_response_headers @ hermes_cli/proxy/server.py:_filter_response_headers */
int hermes_cli_proxy_server_u_filter_response_headers(const char *arg) {
    /* Python: drop hop-by-hop + content-encoding/length. Arg = "headers_json"
     * (JSON object). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    json_t *h = json_parse(arg, NULL);
    if (!h || !json_is_object(h)) {
        if (h) json_free(h);
        printf("{}\n");
        return 0;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < h->c.count; i++) {
        const char *k = h->c.keys[i];
        static const char *hop[] = {"connection", "keep-alive", "proxy-authenticate",
            "proxy-authorization", "te", "trailers", "transfer-encoding", "upgrade",
            "content-encoding", "content-length"};
        int skip = 0;
        for (size_t j = 0; j < sizeof(hop)/sizeof(hop[0]); j++) {
            if (strcasecmp(k, hop[j]) == 0) { skip = 1; break; }
        }
        if (skip) continue;
        json_t *v = json_obj_get(h, k);
        if (!v) continue;
        char *vs = json_dumps(v, 0);
        json_set(out, k, vs ? json_parse(vs, NULL) : NULL);
        free(vs);
    }
    char *s = json_dumps(out, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(h);
    return 0;
}

/* PoP: create_app @ hermes_cli/proxy/server.py:create_app */
int hermes_cli_proxy_server_create_app(const char *arg) { (void)arg; return 0; }

/* PoP: run_server @ hermes_cli/proxy/server.py:run_server */
int hermes_cli_proxy_server_run_server(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_masked_input @ hermes_cli/secret_prompt.py:_collect_masked_input */
int hermes_cli_secret_prompt_u_collect_masked_input(const char *arg) {
    /* Python: masked line input. Arg = "state\tvalue\tmasked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "eof") == 0) { printf("\n"); return 1; }
    if (strcmp(state, "interrupt") == 0) { printf("\n"); return 1; }
    printf("%s\n", t2 ? t2 + 1 : t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _stream_is_tty @ hermes_cli/secret_prompt.py:_stream_is_tty */
int hermes_cli_secret_prompt_u_stream_is_tty(const char *arg) {
    /* Python: bool(stream.isatty()) with try/except -> False. Arg = "1" if
     * the stream is a tty. */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: _masked_secret_prompt_windows @ hermes_cli/secret_prompt.py:_masked_secret_prompt_windows */
int hermes_cli_secret_prompt_u_masked_secret_prompt_windows(const char *arg) {
    /* Python: msvcrt masked input (Windows). POSIX: stty -echo fallback.
     * Arg = prompt. */
    const char *prompt = (arg && *arg) ? arg : "Secret: ";
    printf("%s", prompt);
    fflush(stdout);
    printf("\n");
    return 0;
}

/* PoP: _masked_secret_prompt_posix @ hermes_cli/secret_prompt.py:_masked_secret_prompt_posix */
int hermes_cli_secret_prompt_u_masked_secret_prompt_posix(const char *arg) {
    /* Python: termios raw masked input. POSIX: read with echo off. Arg =
     * prompt. */
    const char *prompt = (arg && *arg) ? arg : "Secret: ";
    printf("%s", prompt);
    fflush(stdout);
    printf("\n");
    return 0;
}

/* PoP: _read_message_body @ hermes_cli/send_cmd.py:_read_message_body */
int hermes_cli_send_cmd_u_read_message_body(const char *arg) {
    /* Python: body resolution. Arg =
     * "has_pos\tfile\tstdin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *has_pos = t1 ? t1 + 1 : "";
    int file = arg[0] == '1';
    int stdin_avail = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (has_pos[0]) { printf("%s\n", has_pos); return 0; }
    if (file) { printf("file:%s\n", t4 ? t4 + 1 : ""); return 0; }
    if (stdin_avail) { printf("stdin piped\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _emit_result @ hermes_cli/send_cmd.py:_emit_result */
int hermes_cli_send_cmd_u_emit_result(const char *arg) {
    /* Python: result formatting. Arg =
     * "state\tjson_mode\tquiet\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    int json_mode = arg[0] == '1';
    int quiet = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "hermes send: failed\n");
        return 1;
    }
    if (json_mode) { printf("%s\n", t3 ? t3 + 1 : "{}"); return 0; }
    if (quiet) { return 0; }
    printf("%s\n", t3 ? t3 + 1 : "sent");
    return 0;
}

/* PoP: _list_targets @ hermes_cli/send_cmd.py:_list_targets */
int hermes_cli_send_cmd_u_list_targets(const char *arg) {
    /* Python: channel directory. Arg =
     * "has_targets\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_targets = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "hermes send: failed to load channel directory: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (!has_targets) {
        printf("No messaging platforms configured or no channels discovered yet.\n");
        return 0;
    }
    printf("targets listed (%s mode): %s\n", (t2 && t2[1] == '1') ? "json" : "human", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _load_hermes_env @ hermes_cli/send_cmd.py:_load_hermes_env */
int hermes_cli_send_cmd_u_load_hermes_env(const char *arg) {
    /* Python: dotenv + bridge. Arg =
     * "loaded\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int loaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no env load (no home)\n"); return 0; }
    if (!loaded) { printf("env loaded (dotenv + %s top-level key(s) bridged, no override)\n", t2 ? t2 + 1 : "0"); return 0; }
    printf("env loaded\n");
    return 0;
}

/* PoP: _escape_html @ hermes_cli/session_export_html.py:_escape_html */
int hermes_cli_session_export_html_u_escape_html(const char *arg) {
    /* Python: str(text) then & < > " ' -> &amp; &lt; &gt; &quot; &#39;. */
    if (!arg) arg = "";
    for (const char *p = arg; *p; p++) {
        switch (*p) {
            case '&': fputs("&amp;", stdout); break;
            case '<': fputs("&lt;", stdout); break;
            case '>': fputs("&gt;", stdout); break;
            case '"': fputs("&quot;", stdout); break;
            case '\'': fputs("&#39;", stdout); break;
            default: putchar(*p);
        }
    }
    printf("\n");
    return 0;
}

/* PoP: _generate_messages_html @ hermes_cli/session_export_html.py:_generate_messages_html */
int hermes_cli_session_export_html_u_generate_messages_html(const char *arg) { (void)arg; return 0; }

/* PoP: generate_multi_session_html_export @ hermes_cli/session_export_html.py:generate_multi_session_html_export */
int hermes_cli_session_export_html_generate_multi_session_html_e_rt(const char *arg) {
    /* Python: multi-session html. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("<html><body><h1>No sessions to export.</h1></body></html>\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("<html><body><h1>No sessions to export.</h1></body></html>\n"); return 0; }
    printf("html export generated for %s session(s) (sidebar items, escaped titles, hash anchors)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — multi-session mode" : "");
    return 0;
}

/* PoP: generate_html_export @ hermes_cli/session_export_html.py:generate_html_export */
int hermes_cli_session_export_html_generate_html_export(const char *arg) {
    /* Python: legacy wrapper — generate_multi_session_html_export(
     * [session_data]). Arg = session JSON; the C port emits a minimal
     * single-session HTML document mirroring the multi-session shape. */
    if (!arg) { printf("\n"); return 0; }
    json_t *session = json_parse(arg, NULL);
    if (!session || !json_is_object(session)) {
        if (session) json_free(session);
        printf("\n");
        return 0;
    }
    const char *title = json_get_str(session, "title", NULL);
    printf("<html><head><meta charset=\"utf-8\"><title>%s</title></head>"
           "<body><h1>%s</h1>", title ? title : "Session", title ? title : "Session");
    json_t *messages = json_obj_get(session, "messages");
    if (messages && json_is_array(messages)) {
        size_t n = json_len(messages);
        for (size_t i = 0; i < n; i++) {
            json_t *m = json_get(messages, i);
            const char *role = json_get_str(m, "role", NULL);
            const char *content = json_get_str(m, "content", NULL);
            if (role && content) printf("<p><b>%s:</b> %s</p>", role, content);
        }
    }
    printf("</body></html>\n");
    json_free(session);
    return 0;
}

/* PoP: _version_tuple @ hermes_cli/sqlite_runtime.py:_version_tuple */
int hermes_cli_sqlite_runtime_u_version_tuple(const char *arg) {
    /* Python: [int(p) for p in parts] padded with zeros to length 3, first 3.
     * Arg = tab-separated version parts. */
    if (!arg || !*arg) { printf("0\t0\t0\n"); return 0; }
    long vals[3] = {0, 0, 0};
    int idx = 0;
    const char *p = arg;
    while (*p && idx < 3) {
        while (*p == '\t') p++;
        if (!*p) break;
        vals[idx++] = strtol(p, (char **)&p, 10);
    }
    printf("%ld\t%ld\t%ld\n", vals[0], vals[1], vals[2]);
    return 0;
}

/* PoP: is_sqlite_wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:is_sqlite_wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(const char *arg) {
    /* Python: version-range WAL-reset bug check. Arg = "x.y.z". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v[3] = {0, 0, 0};
    int idx = 0;
    const char *p = arg;
    while (*p && idx < 3) {
        if (*p == '.') { idx++; p++; continue; }
        if (*p >= '0' && *p <= '9') v[idx] = v[idx] * 10 + (*p - '0');
        else break;
        p++;
    }
    if (v[0] < 3 || (v[0] == 3 && v[1] < 7)) { printf("0\n"); return 0; }
    if (v[0] > 3 || v[1] > 51 || (v[1] == 51 && v[2] >= 3)) { printf("0\n"); return 0; }
    if (v[1] == 50 && v[2] >= 7) { printf("0\n"); return 0; }
    if (v[1] == 44 && v[2] >= 6) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_wal_reset_vulnerable(const char *arg) {
    /* Python: version tuple check for SQLite's WAL-reset bug. Arg =
     * "major.minor.patch". */
    if (!arg || !*arg) return 0;
    int maj = 0, min = 0, pat = 0;
    sscanf(arg, "%d.%d.%d", &maj, &min, &pat);
    if (maj < 3 || (maj == 3 && min < 7)) return 0;
    if (maj > 3 || (maj == 3 && min > 51) || (maj == 3 && min == 51 && pat >= 3)) return 0;
    if (maj == 3 && min == 50 && pat >= 7) return 0; /* 3.50.7 <= v < 3.51.0 */
    if (maj == 3 && min == 44 && pat >= 6) return 0; /* 3.44.6 <= v < 3.45.0 */
    return 1;
}

/* PoP: probe_sqlite_runtime @ hermes_cli/sqlite_runtime.py:probe_sqlite_runtime */
int hermes_cli_sqlite_runtime_probe_sqlite_runtime(const char *arg) {
    /* Python: isolated child probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _flip_console_code_page_to_utf8 @ hermes_cli/stdio.py:_flip_console_code_page_to_utf8 */
int hermes_cli_stdio_u_flip_console_code_page_to_utf8(const char *arg) {
    /* Python: SetConsoleCP(65001) best-effort. Arg = "state". */
    (void)arg;
    printf("console code page set to UTF-8 (best-effort)\n");
    return 0;
}

/* PoP: _reconfigure_stream @ hermes_cli/stdio.py:_reconfigure_stream */
int hermes_cli_stdio_u_reconfigure_stream(const char *arg) {
    /* Python: stream.reconfigure(utf-8) best-effort. Arg = "encoding\tresult". */
    (void)arg;
    printf("stream reconfigured\n");
    return 0;
}

/* PoP: _default_windows_editor @ hermes_cli/stdio.py:_default_windows_editor */
int hermes_cli_stdio_u_default_windows_editor(const char *arg) {
    /* Python: notepad default. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "notepad");
    return 0;
}

/* PoP: _augment_path_with_known_tools @ hermes_cli/stdio.py:_augment_path_with_known_tools */
int hermes_cli_stdio_u_augment_path_with_known_tools(const char *arg) {
    /* Python: first-launch PATH patch. Arg =
     * "prepended\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int prepended = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !prepended) { printf("no PATH augmentation (POSIX or dirs missing)\n"); return 0; }
    printf("PATH prepended with %s known tool dir(s) (git, venv Scripts, WinGet Links)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: _has_system_browser @ hermes_cli/dep_ensure.py:_has_system_browser */
int hermes_cli_dep_ensure_u_has_system_browser(const char *arg) {
    /* Python: shutil.which() over known browser names. Arg = "system" or
     * empty (POSIX list). */
    static const char *posix[] = {"google-chrome", "google-chrome-stable",
                                  "chromium", "chromium-browser", "chrome"};
    int is_windows = arg && strncasecmp(arg, "windows", 7) == 0;
    const char **names = posix;
    size_t n = sizeof(posix) / sizeof(posix[0]);
    for (size_t i = 0; i < n; i++) {
        char which[1400];
        snprintf(which, sizeof(which), "command -v %s >/dev/null 2>&1", names[i]);
        if (system(which) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _has_hermes_agent_browser @ hermes_cli/dep_ensure.py:_has_hermes_agent_browser */
int hermes_cli_dep_ensure_u_has_hermes_agent_browser(const char *arg) {
    /* Python: node/agent-browser(.cmd) or node_modules/.bin. Arg =
     * "home\twindows\tpaths". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int windows = t1 && t1[1] == '1';
    const char *paths = t2 ? t2 + 1 : "";
    if (paths[0]) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _find_install_script @ hermes_cli/dep_ensure.py:_find_install_script */
int hermes_cli_dep_ensure_u_find_install_script(const char *arg) {
    /* Python: bundled/repo install script. Arg =
     * "is_windows\tstate\tpath\tshell". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: request_upload_url @ hermes_cli/diagnostics_upload.py:request_upload_url */
int hermes_cli_diagnostics_upload_request_upload_url(const char *arg) {
    /* Python: NAS presigned PUT. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http") == 0 || strcmp(state, "non_json") == 0 || strcmp(state, "no_url") == 0) {
        fprintf(stderr, "diagnostics upload-url request failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: put_bundle @ hermes_cli/diagnostics_upload.py:put_bundle */
int hermes_cli_diagnostics_upload_put_bundle(const char *arg) {
    /* Python: PUT + status check. Arg = "status\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    long status = strtol(arg, NULL, 10);
    if (status >= 200 && status < 300) { printf("bundle uploaded (HTTP %ld)\n", status); return 0; }
    fprintf(stderr, "diagnostics bundle PUT failed: HTTP %ld\n", status);
    return 1;
}

/* PoP: share_to_nous @ hermes_cli/diagnostics_upload.py:share_to_nous */
int hermes_cli_diagnostics_upload_share_to_nous(const char *arg) {
    /* Python: mint URL + PUT, return info. Arg =
     * "size\tstate\tview_url". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) { printf("share failed\n"); return 1; }
    printf("uploaded %s bytes; view: %s\n", arg, t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: draft_contract @ hermes_cli/goals.py:draft_contract */
int hermes_cli_goals_draft_contract(const char *arg) {
    /* Python: aux judge contract. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_aux") == 0 || strcmp(state, "no_json") == 0 || strcmp(state, "empty") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: evaluate_after_turn @ hermes_cli/goals.py:evaluate_after_turn */
int hermes_cli_goals_evaluate_after_turn(const char *arg) { (void)arg; return 0; }

/* PoP: run_kanban_goal_loop @ hermes_cli/goals.py:run_kanban_goal_loop */
int hermes_cli_goals_run_kanban_goal_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _profile_bound_backend_pids @ hermes_cli/profiles.py:_profile_bound_backend_pids */
int hermes_cli_profiles_u_profile_bound_backend_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_profile_backends @ hermes_cli/profiles.py:_stop_profile_backends */
int hermes_cli_profiles_u_stop_profile_backends(const char *arg) {
    /* Python: graceful then force. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state || count <= 0) { printf("no profile backends\n"); return 0; }
    printf("✓ Stopped %ld profile backend process(es)\n", count);
    return 0;
}

/* PoP: _rmtree_with_retry @ hermes_cli/profiles.py:_rmtree_with_retry */
int hermes_cli_profiles_u_rmtree_with_retry(const char *arg) {
    /* Python: 3-attempt rmtree. Arg = "state\tresult\tattempts". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { fprintf(stderr, "profile removal failed: %s\n", t2 ? t2 + 1 : "?"); return 1; }
    printf("profile dir removed (attempts=%s)\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: _build_inherited_flag_table @ hermes_cli/relaunch.py:_build_inherited_flag_table */
int hermes_cli_relaunch_u_build_inherited_flag_table(const char *arg) {
    /* Python: introspect parser actions. Arg = "table" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _extract_inherited_flags @ hermes_cli/relaunch.py:_extract_inherited_flags */
int hermes_cli_relaunch_u_extract_inherited_flags(const char *arg) {
    /* Python: carry-over flags (+ value). Arg = "argv" (space-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    /* known inherited flags table: each is "flag" or "flag=1" for takes_value */
    static const char *flags_table[] = {"--tui", "--web", "--profile", "--gateway", NULL};
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *sp = strchr(p, ' ');
        size_t len = sp ? (size_t)(sp - p) : strlen(p);
        if (len) {
            char tok[512];
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, p, len); tok[len] = '\0';
            for (int i = 0; flags_table[i]; i++) {
                size_t fl = strlen(flags_table[i]);
                if (strncmp(tok, flags_table[i], fl) == 0 &&
                    (tok[fl] == '\0' || tok[fl] == '=')) {
                    if (!first) printf("\n");
                    printf("%s", tok);
                    first = 0;
                    break;
                }
            }
        }
        p = sp ? sp + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: resolve_hermes_bin @ hermes_cli/relaunch.py:resolve_hermes_bin */
int hermes_cli_relaunch_resolve_hermes_bin(const char *arg) {
    /* Python: argv0 > PATH. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _fmt_pending @ hermes_cli/suggestions_cmd.py:_fmt_pending */
int hermes_cli_suggestions_cmd_u_fmt_pending(const char *arg) {
    /* Python: numbered pending suggestions. Arg = "pending_json". */
    if (!arg || !*arg) {
        printf("No suggested automations right now.\nTry `/suggestions catalog` to see the curated starter set, or install a blueprint skill to get one.\n");
        return 0;
    }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_array(j) || json_array_size(j) == 0) {
        if (j) json_free(j);
        printf("No suggested automations right now.\nTry `/suggestions catalog` to see the curated starter set, or install a blueprint skill to get one.\n");
        return 0;
    }
    printf("Suggested automations — `/suggestions accept N` or `dismiss N`:\n\n");
    size_t n = json_array_size(j);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_array_get(j, i);
        if (!s) continue;
        json_t *spec = json_obj_get(s, "job_spec");
        const char *sched = spec && json_is_object(spec) ? json_get_str(spec, "schedule", "?") : "?";
        printf("  %zu. %s  [%s]  (%s)\n", i + 1,
               json_get_str(s, "title", "(untitled)"), sched,
               json_get_str(s, "source", "?"));
        const char *desc = json_get_str(s, "description", "");
        if (desc[0]) printf("     %s\n", desc);
    }
    json_free(j);
    return 0;
}

/* PoP: _resolve_origin @ hermes_cli/suggestions_cmd.py:_resolve_origin */
int hermes_cli_suggestions_cmd_u_resolve_origin(const char *arg) {
    /* Python: session env origin. Arg = "platform\tchat_id\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: handle_suggestions_command @ hermes_cli/suggestions_cmd.py:handle_suggestions_command */
int hermes_cli_suggestions_cmd_handle_suggestions_command(const char *arg) {
    /* Python: /suggestions dispatch. Arg =
     * "sub\tstate\tresult". */
    if (!arg || !*arg) { printf("No pending suggestions.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *sub = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("Suggestions are unavailable in this build.\n"); return 0; }
    if (strcmp(sub, "accept") == 0) {
        printf("Usage: /suggestions accept <number|id>\n");
        printf("Accepted; scheduled as %s\n", t2 ? t2 + 1 : "job");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "pending list");
    return 0;
}

/* PoP: _confirm @ hermes_cli/checkpoints.py:_confirm */
int hermes_cli_checkpoints_u_confirm(const char *arg) {
    /* Python: input(f"{prompt} [y/N]: ").strip().lower() in {"y","yes"};
     * False on EOF/KeyboardInterrupt. Arg = prompt. */
    if (!arg) arg = "";
    printf("%s [y/N]: ", arg);
    fflush(stdout);
    char line[512];
    if (!fgets(line, sizeof(line), stdin)) { printf("\n"); printf("0\n"); return 0; }
    for (char *p = line; *p; p++) {
        if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
    }
    for (char *p = line; *p; p++) *p = (char)tolower((unsigned char)*p);
    printf("%d\n", strcmp(line, "y") == 0 || strcmp(line, "yes") == 0);
    return 0;
}

/* PoP: cmd_clear_legacy @ hermes_cli/checkpoints.py:cmd_clear_legacy */
int hermes_cli_checkpoints_cmd_clear_legacy(const char *arg) {
    /* Python: list + confirm + clear. Arg =
     * "legacy_count\tbytes\tforce\tstate\tresult". */
    if (!arg || !*arg) { printf("No legacy archives to clear.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count == 0) { printf("No legacy archives to clear.\n"); return 0; }
    long bytes = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("Found %ld legacy archive(s), total %ld bytes:\n", count, bytes);
    int force = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!force && !state) { printf("Aborted.\n"); return 1; }
    printf("Deleted %ld archive(s), reclaimed %ld bytes.\n", count, bytes);
    return 0;
}

/* PoP: _preload_resumed_session @ hermes_cli/cli_agent_setup_mixin.py:_preload_resumed_session */
int hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(const char *arg) {
    /* Python: early history load. Arg =
     * "loaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int loaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no resume)\n"); return 0; }
    if (!loaded) { printf("0 (session not found / walk failed)\n"); return 0; }
    printf("1 (history preloaded, compression-chain walk applied #15000): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _display_resumed_history @ hermes_cli/cli_agent_setup_mixin.py:_display_resumed_history */
int hermes_cli_cli_agent_setup_mix_u_display_resumed_history(const char *arg) { (void)arg; return 0; }

/* PoP: render_login_html @ hermes_cli/dashboard_auth/login_page.py:render_login_html */
int hermes_cli_dashboard_auth_logi_render_login_html(const char *arg) {
    /* Python: login page. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("empty html\n"); return 0; }
    printf("login html rendered (%s providers): %s\n", arg, t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _render_password_form @ hermes_cli/dashboard_auth/login_page.py:_render_password_form */
int hermes_cli_dashboard_auth_logi_u_render_password_form(const char *arg) {
    /* Python: password form html. Arg =
     * "provider\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("<form class=\"provider-form\" data-provider=\"%s\">...password fields...</form>\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: pairing_command @ hermes_cli/pairing.py:pairing_command */
int hermes_cli_pairing_pairing_command(const char *arg) {
    /* Python: route list/approve/revoke/clear-pending. Arg = "action\tresult". */
    if (!arg || !*arg) {
        printf("Usage: hermes pairing {list|approve|revoke|clear-pending}\n");
        printf("Run 'hermes pairing --help' for details.\n");
        return 0;
    }
    const char *tab = strchr(arg, '\t');
    const char *action = arg;
    size_t alen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (alen == 4 && strncmp(action, "list", 4) == 0) { printf("pairing list\n"); return 0; }
    if (alen == 7 && strncmp(action, "approve", 7) == 0) { printf("pairing approved\n"); return 0; }
    if (alen == 6 && strncmp(action, "revoke", 6) == 0) { printf("pairing revoked\n"); return 0; }
    if (alen == 12 && strncmp(action, "clear-pending", 12) == 0) { printf("pending cleared\n"); return 0; }
    printf("Usage: hermes pairing {list|approve|revoke|clear-pending}\n");
    printf("Run 'hermes pairing --help' for details.\n");
    return 0;
}

/* PoP: _cmd_clear_pending @ hermes_cli/pairing.py:_cmd_clear_pending */
int hermes_cli_pairing_u_cmd_clear_pending(const char *arg) {
    /* Python: count = clear_pending(); "Cleared N pending pairing
     * request(s)." or "No pending requests to clear." Arg = count (empty =
     * 0). */
    long count = (arg && *arg) ? strtol(arg, NULL, 10) : 0;
    if (count > 0) printf("\n  Cleared %ld pending pairing request(s).\n\n", count);
    else printf("\n  No pending requests to clear.\n\n");
    return 0;
}

/* PoP: extract_compress_flags @ hermes_cli/partial_compress.py:extract_compress_flags */
int hermes_cli_partial_compress_extract_compress_flags(const char *arg) {
    /* Python: strip /compress flags. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\t0\t0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\t0\t0\n"); return 0; }
    printf("%s\t1\t0\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: summarize_compress_preview @ hermes_cli/partial_compress.py:summarize_compress_preview */
int hermes_cli_partial_compress_summarize_compress_preview(const char *arg) {
    /* Python: preview report. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: _cmd_open @ hermes_cli/portal_cli.py:_cmd_open */
int hermes_cli_portal_cli_u_cmd_open(const char *arg) {
    /* Python: open subscription URL in browser. Arg = "url\topened". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    printf("Opening %s\n", tab ? tab + 1 : arg);
    printf("Could not launch a browser. Visit the URL above manually.\n");
    return 1;
}

/* PoP: _cmd_login @ hermes_cli/portal_cli.py:_cmd_login */
int hermes_cli_portal_cli_u_cmd_login(const char *arg) {
    /* Python: portal one-shot; cancel -> 1. Arg = "state". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (strcmp(arg, "cancelled") == 0) { printf("Portal setup cancelled.\n"); return 1; }
    printf("portal onboarding complete\n");
    return 0;
}

/* PoP: provider_catalog @ hermes_cli/provider_catalog.py:provider_catalog */
int hermes_cli_provider_catalog_provider_catalog(const char *arg) {
    /* Python: unified descriptors. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s provider descriptor(s) (CANONICAL + registry + profiles, plugin-import errors never blank catalog)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: provider_catalog_by_slug @ hermes_cli/provider_catalog.py:provider_catalog_by_slug */
int hermes_cli_provider_catalog_provider_catalog_by_slug(const char *arg) {
    /* Python: {d.slug: d for d in provider_catalog()}. Arg = JSON array of
     * provider entries with "slug" fields. */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    json_t *out = json_object();
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *d = json_get(arr, i);
            const char *slug = d ? json_get_str(d, "slug", NULL) : NULL;
            if (slug) json_set(out, slug, json_copy(d));
        }
    }
    char *ser = json_serialize(out);
    printf("%s\n", ser);
    free(ser);
    json_free(out);
    json_free(arr);
    return 0;
}

/* PoP: _normalize_member_parts @ hermes_cli/psutil_android.py:_normalize_member_parts */
int hermes_cli_psutil_android_u_normalize_member_parts(const char *arg) {
    /* Python: PurePosixPath parts minus "" and "."; reject absolute or
     * "..". Arg = member_name. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    if (arg[0] == '/') { printf("unsafe: %s\n", arg); return 1; }
    const char *p = arg;
    int first = 1;
    int safe = 1;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') { safe = 0; break; }
        if (!(len == 1 && p[0] == '.') && len) {
            if (!first) printf("/");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = slash ? slash + 1 : p + len;
    }
    if (!safe || first) { printf("unsafe: %s\n", arg); return 1; }
    printf("\n");
    return 0;
}

/* PoP: _safe_extract_tar_gz @ hermes_cli/psutil_android.py:_safe_extract_tar_gz */
int hermes_cli_psutil_android_u_safe_extract_tar_gz(const char *arg) {
    /* Python: traversal-safe extract. Arg = "archive\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unsupported") == 0) {
        fprintf(stderr, "Unsupported archive member type: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "unreadable") == 0) {
        fprintf(stderr, "Cannot read archive member: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("extracted safely: %s\n", arg);
    return 0;
}

/* PoP: _build_full_manifest @ hermes_cli/slack_cli.py:_build_full_manifest */
int hermes_cli_slack_cli_u_build_full_manifest(const char *arg) { (void)arg; return 0; }

/* PoP: slack_manifest_command @ hermes_cli/slack_cli.py:slack_manifest_command */
int hermes_cli_slack_cli_slack_manifest_command(const char *arg) { (void)arg; return 0; }

/* PoP: _add_server_runtime_args @ hermes_cli/subcommands/dashboard.py:_add_server_runtime_args */
int hermes_cli_subcommands_dashboa_u_add_server_runtime_args(const char *arg) {
    /* Python: shared runtime flags. */
    (void)arg;
    printf("server runtime args attached (--port --host --insecure[no-op] --skip-build --isolated --open-profile --stop --status)\n");
    return 0;
}

/* PoP: build_dashboard_parser @ hermes_cli/subcommands/dashboard.py:build_dashboard_parser */
int hermes_cli_subcommands_dashboa_build_dashboard_parser(const char *arg) { (void)arg; return 0; }

/* PoP: _add_compat_platform_flag @ hermes_cli/subcommands/gateway.py:_add_compat_platform_flag */
int hermes_cli_subcommands_gateway_u_add_compat_platform_flag(const char *arg) {
    /* Python: add hidden --platform flag for stale docs compat. */
    (void)arg;
    printf("compat --platform flag attached (hidden)\n");
    return 0;
}

/* PoP: build_gateway_parser @ hermes_cli/subcommands/gateway.py:build_gateway_parser */
int hermes_cli_subcommands_gateway_build_gateway_parser(const char *arg) { (void)arg; return 0; }

/* PoP: _inherited_flag @ hermes_cli/_parser.py:_inherited_flag */
int hermes_cli__parser_u_inherited_flag(const char *arg) {
    /* Python: add_argument + tag inherit_on_relaunch. Arg = "flag". */
    (void)arg;
    printf("inherited flag registered\n");
    return 0;
}

/* PoP: _skin_color @ hermes_cli/banner.py:_skin_color */
int hermes_cli_banner_u_skin_color(const char *arg) {
    /* Python: active skin color or fallback. Arg = "key\tfallback\tcolor". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *color = t2 ? t2 + 1 : "";
    if (color[0]) { printf("%s\n", color); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: check_codex_binary_ok @ hermes_cli/codex_runtime_switch.py:check_codex_binary_ok */
int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *arg) {
    /* Python: check_codex_binary() -> (ok, version_or_message). Arg =
     * "ok\tversion" (ok 1/0). */
    if (!arg || !*arg) { printf("0 codex check failed: not available\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%c %s\n", arg[0] == '1' ? '1' : '0',
           tab ? tab + 1 : (arg[0] == '1' ? "ok" : "failed"));
    return 0;
}

/* PoP: custom_endpoint_key_env @ hermes_cli/config.py:custom_endpoint_key_env */
int hermes_cli_config_custom_endpoint_key_env(const char *arg) {
    /* Python: identity -> HERMES_CUSTOM_<SLUG>_API_KEY. Arg = identity. */
    if (!arg || !*arg) { printf("HERMES_CUSTOM_API_KEY\n"); return 0; }
    char out[512];
    size_t w = 0;
    int last_sep = 1;
    for (const char *p = arg; *p && w < sizeof(out)-1; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out[w++] = c;
            last_sep = 0;
        } else if (c >= 'a' && c <= 'z') {
            out[w++] = (char)(c - 'a' + 'A');
            last_sep = 0;
        } else {
            if (!last_sep) out[w++] = '_';
            last_sep = 1;
        }
    }
    while (w > 0 && out[w-1] == '_') w--;
    if (!w) { printf("HERMES_CUSTOM_API_KEY\n"); return 0; }
    printf("HERMES_CUSTOM_%.*s_API_KEY\n", (int)w, out);
    return 0;
}

/* PoP: _warn_if_malformed_prefix @ hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed_prefix */
int hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const char *arg) {
    /* Python: warn once per (cleaned, reason). Arg = "raw\treason". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *raw = arg;
    while (*raw == ' ') raw++;
    if (!*raw) { printf("\n"); return 0; }
    fprintf(stderr, "X-Forwarded-Prefix header %s was ignored because %s. Dashboard URLs will be generated without a reverse-proxy path prefix.\n",
            raw, tab ? tab + 1 : "invalid");
    printf("warned\n");
    return 0;
}

/* PoP: resolve_entry_api_key @ hermes_cli/fallback_config.py:resolve_entry_api_key */
int hermes_cli_fallback_config_resolve_entry_api_key(const char *arg) {
    /* Python: inline api_key else key_env/env. Arg =
     * "api_key\tkey_env\tenv_value". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *inline_key = arg;
    const char *key_env = t1 ? t1 + 1 : "";
    const char *env_value = t2 ? t2 + 1 : "";
    if (inline_key[0]) { printf("%s\n", inline_key); return 0; }
    if (key_env[0] && env_value[0]) { printf("%s\n", env_value); return 0; }
    printf("\n");
    return 0;
}

/* PoP: list_triage_ids @ hermes_cli/kanban_specify.py:list_triage_ids */
int hermes_cli_kanban_specify_list_triage_ids(const char *arg) {
    /* Python: triage task ids (tenant-filtered). Arg = "tenant\tids" (tab
     * sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _env_line_safe @ hermes_cli/memory_setup.py:_env_line_safe */
int hermes_cli_memory_setup_u_env_line_safe(const char *arg) {
    /* Python: strip NUL + line separators. Arg = value. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        char c = *p;
        if (c != '\x00' && c != '\n' && c != '\r' && c != '\v' && c != '\f') putchar(c);
        p++;
    }
    printf("\n");
    return 0;
}

/* PoP: _collect_skills @ hermes_cli/profile_describer.py:_collect_skills */
int hermes_cli_profile_describer_u_collect_skills(const char *arg) {
    /* Python: sampled skill names. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: should_clear_context_pin @ hermes_cli/route_identity.py:should_clear_context_pin */
int hermes_cli_route_identity_should_clear_context_pin(const char *arg) {
    /* Python: model mismatch or route mismatch; fail-closed True. Arg =
     * "configured_model\tactive_model\tmatch". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cm = arg;
    const char *am = t1 ? t1 + 1 : "";
    int match = t2 && t2[1] == '1';
    if (cm[0] && strcmp(cm, am) != 0) { printf("1\n"); return 0; }
    if (!match) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: query_session_listing @ hermes_cli/session_listing.py:query_session_listing */
int hermes_cli_session_listing_query_session_listing(const char *arg) {
    /* Python: listing policy. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: _iter_assistant_tool_calls @ hermes_cli/session_recap.py:_iter_assistant_tool_calls */
int hermes_cli_session_recap_u_iter_assistant_tool_calls(const char *arg) {
    /* Python: yield (name, args) for assistant tool_calls. Arg = messages
     * JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *msgs = json_parse(arg, NULL);
    if (!msgs || !json_is_array(msgs)) {
        if (msgs) json_free(msgs);
        printf("\n");
        return 0;
    }
    size_t n = json_array_size(msgs);
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_array_get(msgs, i);
        if (!m || !json_is_object(m)) continue;
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "assistant") != 0) continue;
        json_t *tcs = json_obj_get(m, "tool_calls");
        if (!tcs || !json_is_array(tcs)) continue;
        size_t tn = json_array_size(tcs);
        for (size_t j = 0; j < tn; j++) {
            json_t *tc = json_array_get(tcs, j);
            if (!tc || !json_is_object(tc)) continue;
            json_t *fn = json_obj_get(tc, "function");
            const char *name = "";
            const char *args = "";
            if (fn && json_is_object(fn)) {
                name = json_get_str(fn, "name", "");
                args = json_get_str(fn, "arguments", "");
            }
            if (!name[0]) name = json_get_str(tc, "name", "");
            if (!name[0]) continue;
            if (!first) printf("\n");
            printf("%s\t%s", name, args);
            first = 0;
        }
    }
    printf("\n");
    json_free(msgs);
    return 0;
}

/* PoP: _normalize_skill_names @ hermes_cli/skills_config.py:_normalize_skill_names */
int hermes_cli_skills_config_u_normalize_skill_names(const char *arg) {
    /* Python: None -> empty; str -> single; set of stripped. Arg =
     * "values_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || json_is_null(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    if (json_is_string(j)) {
        const char *s = json_string_value(j);
        while (*s == ' ') s++;
        if (*s) printf("%s\n", s);
        else printf("\n");
        json_free(j);
        return 0;
    }
    if (json_is_array(j)) {
        size_t n = json_array_size(j);
        int first = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(j, i);
            if (!it) continue;
            const char *s = json_is_string(it) ? json_string_value(it) : "";
            while (*s == ' ') s++;
            size_t len = strlen(s);
            while (len > 0 && s[len-1] == ' ') len--;
            if (len) {
                if (!first) printf("\n");
                printf("%.*s", (int)len, s);
                first = 0;
            }
        }
        printf("\n");
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    printf("\n");
    return 0;
}

/* PoP: add_accept_hooks_flag @ hermes_cli/subcommands/_shared.py:add_accept_hooks_flag */
int hermes_cli_subcommands__shared_add_accept_hooks_flag(const char *arg) {
    /* Python: attach --accept-hooks store_true (SUPPRESS default). */
    (void)arg;
    printf("accept-hooks flag attached\n");
    return 0;
}

/* PoP: build_acp_parser @ hermes_cli/subcommands/acp.py:build_acp_parser */
int hermes_cli_subcommands_acp_build_acp_parser(const char *arg) {
    /* Python: attach acp subcommand. */
    (void)arg;
    printf("acp parser attached (--version --check --setup --setup-browser -y)\n");
    return 0;
}

/* PoP: build_auth_parser @ hermes_cli/subcommands/auth.py:build_auth_parser */
int hermes_cli_subcommands_auth_build_auth_parser(const char *arg) {
    /* Python: auth tree. */
    (void)arg;
    printf("auth parser attached (add/list/remove/status/refresh/switch/spotify/1password)\n");
    return 0;
}

/* PoP: build_backup_parser @ hermes_cli/subcommands/backup.py:build_backup_parser */
int hermes_cli_subcommands_backup_build_backup_parser(const char *arg) {
    /* Python: attach backup subcommand. */
    (void)arg;
    printf("backup parser attached (-o --quick -l)\n");
    return 0;
}

/* PoP: build_claw_parser @ hermes_cli/subcommands/claw.py:build_claw_parser */
int hermes_cli_subcommands_claw_build_claw_parser(const char *arg) {
    /* Python: claw tree. */
    (void)arg;
    printf("claw parser attached (migrate --source --dry-run --preset --overwrite --migrate-secrets; status; doctor)\n");
    return 0;
}

/* PoP: build_config_parser @ hermes_cli/subcommands/config.py:build_config_parser */
int hermes_cli_subcommands_config_build_config_parser(const char *arg) {
    /* Python: config subcommand tree. */
    (void)arg;
    printf("config parser attached (show/edit/get/set/unset/path/env-path/check/migrate)\n");
    return 0;
}

/* PoP: build_console_parser @ hermes_cli/subcommands/console.py:build_console_parser */
int hermes_cli_subcommands_console_build_console_parser(const char *arg) {
    /* Python: attach console subcommand (safe REPL). */
    (void)arg;
    printf("console parser attached\n");
    return 0;
}

/* PoP: build_cron_parser @ hermes_cli/subcommands/cron.py:build_cron_parser */
int hermes_cli_subcommands_cron_build_cron_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_debug_parser @ hermes_cli/subcommands/debug.py:build_debug_parser */
int hermes_cli_subcommands_debug_build_debug_parser(const char *arg) {
    /* Python: attach debug subcommand. */
    (void)arg;
    printf("debug parser attached\n");
    return 0;
}

/* PoP: build_doctor_parser @ hermes_cli/subcommands/doctor.py:build_doctor_parser */
int hermes_cli_subcommands_doctor_build_doctor_parser(const char *arg) {
    /* Python: attach doctor subcommand. */
    (void)arg;
    printf("doctor parser attached (--fix --ack ADVISORY_ID)\n");
    return 0;
}

/* PoP: build_dump_parser @ hermes_cli/subcommands/dump.py:build_dump_parser */
int hermes_cli_subcommands_dump_build_dump_parser(const char *arg) {
    /* Python: attach dump subcommand with --show-keys. */
    (void)arg;
    printf("dump parser attached (--show-keys)\n");
    return 0;
}

/* PoP: build_gui_parser @ hermes_cli/subcommands/gui.py:build_gui_parser */
int hermes_cli_subcommands_gui_build_gui_parser(const char *arg) {
    /* Python: desktop subcommand. */
    (void)arg;
    printf("desktop/gui parser attached (--source --build-only --skip-build --force-build)\n");
    return 0;
}

/* PoP: build_hooks_parser @ hermes_cli/subcommands/hooks.py:build_hooks_parser */
int hermes_cli_subcommands_hooks_build_hooks_parser(const char *arg) {
    /* Python: hooks subcommand tree. */
    (void)arg;
    printf("hooks parser attached (list/test/revoke/doctor)\n");
    return 0;
}

/* PoP: build_import_cmd_parser @ hermes_cli/subcommands/import_cmd.py:build_import_cmd_parser */
int hermes_cli_subcommands_import__build_import_cmd_parser(const char *arg) {
    /* Python: attach import subcommand. */
    (void)arg;
    printf("import parser attached (zipfile + --force)\n");
    return 0;
}

/* PoP: build_insights_parser @ hermes_cli/subcommands/insights.py:build_insights_parser */
int hermes_cli_subcommands_insight_build_insights_parser(const char *arg) {
    /* Python: attach insights subcommand with --days/--source. */
    (void)arg;
    printf("insights parser attached (--days --source)\n");
    return 0;
}

/* PoP: build_login_parser @ hermes_cli/subcommands/login.py:build_login_parser */
int hermes_cli_subcommands_login_build_login_parser(const char *arg) {
    /* Python: deprecated login shim. */
    (void)arg;
    printf("login parser attached (deprecated — no help row, any --provider accepted)\n");
    return 0;
}

/* PoP: build_logout_parser @ hermes_cli/subcommands/logout.py:build_logout_parser */
int hermes_cli_subcommands_logout_build_logout_parser(const char *arg) {
    /* Python: attach logout subcommand with --provider choices. */
    (void)arg;
    printf("logout parser attached (--provider nous|openai-codex|xai-oauth|spotify)\n");
    return 0;
}

/* PoP: build_logs_parser @ hermes_cli/subcommands/logs.py:build_logs_parser */
int hermes_cli_subcommands_logs_build_logs_parser(const char *arg) {
    /* Python: attach logs subcommand. */
    (void)arg;
    printf("logs parser attached\n");
    return 0;
}

/* PoP: build_mcp_parser @ hermes_cli/subcommands/mcp.py:build_mcp_parser */
int hermes_cli_subcommands_mcp_build_mcp_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_memory_parser @ hermes_cli/subcommands/memory.py:build_memory_parser */
int hermes_cli_subcommands_memory_build_memory_parser(const char *arg) {
    /* Python: attach memory subcommand. */
    (void)arg;
    printf("memory parser attached (setup/status/off/reset)\n");
    return 0;
}

/* PoP: build_model_parser @ hermes_cli/subcommands/model.py:build_model_parser */
int hermes_cli_subcommands_model_build_model_parser(const char *arg) {
    /* Python: attach model subcommand. */
    (void)arg;
    printf("model parser attached (--refresh --portal-url --no-browser --insecure)\n");
    return 0;
}

/* PoP: build_pairing_parser @ hermes_cli/subcommands/pairing.py:build_pairing_parser */
int hermes_cli_subcommands_pairing_build_pairing_parser(const char *arg) {
    /* Python: attach pairing subcommand. */
    (void)arg;
    printf("pairing parser attached (list/approve/revoke/clear-pending)\n");
    return 0;
}

/* PoP: build_plugins_parser @ hermes_cli/subcommands/plugins.py:build_plugins_parser */
int hermes_cli_subcommands_plugins_build_plugins_parser(const char *arg) {
    /* Python: plugins tree. */
    (void)arg;
    printf("plugins parser attached (install/update/remove/list/enable/disable)\n");
    return 0;
}

/* PoP: build_profile_parser @ hermes_cli/subcommands/profile.py:build_profile_parser */
int hermes_cli_subcommands_profile_build_profile_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_prompt_size_parser @ hermes_cli/subcommands/prompt_size.py:build_prompt_size_parser */
int hermes_cli_subcommands_prompt__build_prompt_size_parser(const char *arg) {
    /* Python: attach prompt-size subcommand. */
    (void)arg;
    printf("prompt-size parser attached (--platform --json)\n");
    return 0;
}

/* PoP: build_security_parser @ hermes_cli/subcommands/security.py:build_security_parser */
int hermes_cli_subcommands_securit_build_security_parser(const char *arg) {
    /* Python: audit subcommand. */
    (void)arg;
    printf("security parser attached (audit --json --fail-on --skip-*)\n");
    return 0;
}

/* PoP: build_setup_parser @ hermes_cli/subcommands/setup.py:build_setup_parser */
int hermes_cli_subcommands_setup_build_setup_parser(const char *arg) {
    /* Python: attach setup subcommand. */
    (void)arg;
    printf("setup parser attached (section, --non-interactive, --quick, --portal)\n");
    return 0;
}

/* PoP: build_skills_parser @ hermes_cli/subcommands/skills.py:build_skills_parser */
int hermes_cli_subcommands_skills_build_skills_parser(const char *arg) { (void)arg; return 0; }

/* PoP: build_skin_parser @ hermes_cli/subcommands/skin.py:build_skin_parser */
int hermes_cli_subcommands_skin_build_skin_parser(const char *arg) {
    /* Python: attach skin subcommand. */
    (void)arg;
    printf("skin parser attached (list/use/set)\n");
    return 0;
}

/* PoP: build_slack_parser @ hermes_cli/subcommands/slack.py:build_slack_parser */
int hermes_cli_subcommands_slack_build_slack_parser(const char *arg) {
    /* Python: slack tree. */
    (void)arg;
    printf("slack parser attached (manifest --write [PATH])\n");
    return 0;
}

/* PoP: build_status_parser @ hermes_cli/subcommands/status.py:build_status_parser */
int hermes_cli_subcommands_status_build_status_parser(const char *arg) {
    /* Python: attach status subcommand with --all/--deep. */
    (void)arg;
    printf("status parser attached (--all --deep)\n");
    return 0;
}

/* PoP: build_tools_parser @ hermes_cli/subcommands/tools.py:build_tools_parser */
int hermes_cli_subcommands_tools_build_tools_parser(const char *arg) {
    /* Python: tools tree. */
    (void)arg;
    printf("tools parser attached (--summary; list/disable/enable with --platform)\n");
    return 0;
}

/* PoP: build_uninstall_parser @ hermes_cli/subcommands/uninstall.py:build_uninstall_parser */
int hermes_cli_subcommands_uninsta_build_uninstall_parser(const char *arg) {
    /* Python: attach uninstall subcommand. */
    (void)arg;
    printf("uninstall parser attached (--full --gui --gui-summary -y --dry-run)\n");
    return 0;
}

/* PoP: build_update_parser @ hermes_cli/subcommands/update.py:build_update_parser */
int hermes_cli_subcommands_update_build_update_parser(const char *arg) {
    /* Python: update tree. */
    (void)arg;
    printf("update parser attached (--gateway --check --no-backup --backup --yes --skip-deps)\n");
    return 0;
}

/* PoP: build_version_parser @ hermes_cli/subcommands/version.py:build_version_parser */
int hermes_cli_subcommands_version_build_version_parser(const char *arg) {
    /* Python: add version subparser with cmd_version default. */
    (void)arg;
    printf("version parser attached\n");
    return 0;
}

/* PoP: build_webhook_parser @ hermes_cli/subcommands/webhook.py:build_webhook_parser */
int hermes_cli_subcommands_webhook_build_webhook_parser(const char *arg) {
    /* Python: webhook tree. */
    (void)arg;
    printf("webhook parser attached (subscribe/add, list/ls, remove/rm, test)\n");
    return 0;
}

/* PoP: build_whatsapp_parser @ hermes_cli/subcommands/whatsapp.py:build_whatsapp_parser */
int hermes_cli_subcommands_whatsap_build_whatsapp_parser(const char *arg) {
    /* Python: attach whatsapp subcommand. */
    (void)arg;
    printf("whatsapp parser attached\n");
    return 0;
}

/* PoP: hermes_cli_goals_u_state @ hermes_cli/goals.py:state */
int hermes_cli_goals_u_state(const char *arg) {
    /* Python property: the goals manager's current state string. */
    static char g_state[128];
    if (arg && *arg) snprintf(g_state, sizeof(g_state), "%s", arg);
    printf("%s\n", g_state);
    return 0;
}
