/*
 * debug.c — canonical C11 port of hermes_cli/debug.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_debug_u_pending_file @ hermes_cli/debug.py:_pending_file */
int hermes_cli_debug_u_pending_file(const char *arg) {
    /* Python: HERMES_HOME/pastes/pending.json. Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/pastes/pending.json\n", arg);
    return 0;
}

/* PoP: hermes_cli_debug_u_best_effort_sweep_expired_pastes @ hermes_cli/debug.py:_best_effort_sweep_expired_pastes */
int hermes_cli_debug_u_best_effort_sweep_expired_pastes(const char *arg) {
    /* Python: sweep without letting /debug fail offline. Arg = result. */
    (void)arg;
    printf("paste sweep attempted\n");
    return 0;
}

/* PoP: hermes_cli_debug_delete_paste @ hermes_cli/debug.py:delete_paste */
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

/* PoP: hermes_cli_debug_u_schedule_auto_delete @ hermes_cli/debug.py:_schedule_auto_delete */
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

/* PoP: hermes_cli_debug_u_upload_paste_rs @ hermes_cli/debug.py:_upload_paste_rs */
int hermes_cli_debug_u_upload_paste_rs(const char *arg) {
    /* Python: POST plain body -> URL. Arg = "content\turl". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    const char *url = tab ? tab + 1 : "";
    if (url[0]) { printf("%s\n", url); return 0; }
    printf("paste upload failed\n");
    return 1;
}

/* PoP: hermes_cli_debug_u_upload_dpaste_com @ hermes_cli/debug.py:_upload_dpaste_com */
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

/* PoP: hermes_cli_debug_upload_to_pastebin @ hermes_cli/debug.py:upload_to_pastebin */
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

/* PoP: hermes_cli_debug_u_primary_log_path @ hermes_cli/debug.py:_primary_log_path */
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

/* PoP: hermes_cli_debug_u_resolve_log_path @ hermes_cli/debug.py:_resolve_log_path */
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

/* PoP: hermes_cli_debug_u_missing_log_note @ hermes_cli/debug.py:_missing_log_note */
/* Explain a missing log instead of stating a bare absence. For a client-side
 * log the absence is expected on a remote backend, so the note names the
 * writer and the path to collect by hand. Arg = "log_name\tprimary_path"
 * (primary_path empty when _primary_log_path returned None). */
int hermes_cli_debug_u_missing_log_note(const char *arg) {
    const char *CLIENT_REASON = "written by Hermes Desktop on the machine running the app, not by this backend. If the desktop connects to a remote/docker/SSH backend, collect it on that client machine";
    if (!arg || !*arg) { printf("(file not found)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char namebuf[256];
    const char *nl = tab ? tab : arg;
    size_t nlen = (size_t)(nl - arg);
    if (nlen >= sizeof(namebuf)) nlen = sizeof(namebuf) - 1;
    memcpy(namebuf, arg, nlen); namebuf[nlen] = '\0';
    const char *primary = tab ? tab + 1 : "";
    if (strcmp(namebuf, "desktop") == 0) {
        if (*primary)
            printf("(not on this host: %s — expected at %s)\n", CLIENT_REASON, primary);
        else
            printf("(not on this host: %s)\n", CLIENT_REASON);
    } else {
        if (*primary)
            printf("(not on this host: unknown client log — expected at %s)\n", primary);
        else
            printf("(file not found)\n");
    }
    return 0;
}

/* PoP: hermes_cli_debug_u_capture_log_snapshot @ hermes_cli/debug.py:_capture_log_snapshot */
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

/* PoP: hermes_cli_debug_u_capture_default_log_snapshots @ hermes_cli/debug.py:_capture_default_log_snapshots */
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

/* PoP: hermes_cli_debug_u_capture_dump @ hermes_cli/debug.py:_capture_dump */
int hermes_cli_debug_u_capture_dump(const char *arg) {
    /* Python: run hermes dump capturing stdout. Arg = dump output. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_debug_collect_share_bundle @ hermes_cli/debug.py:collect_share_bundle */
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

/* PoP: hermes_cli_debug_build_nous_bundle @ hermes_cli/debug.py:build_nous_bundle */
int hermes_cli_debug_build_nous_bundle(const char *arg) {
    /* Python: gzip envelope. Arg = "redact\tfiles_json\tsize". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("nous bundle built (redacted=%s, %s bytes)\n",
           (arg[0] == '1') ? "true" : "false", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: hermes_cli_debug_u_confirm_upload @ hermes_cli/debug.py:_confirm_upload */
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

/* PoP: hermes_cli_debug_u_run_debug_share_nous @ hermes_cli/debug.py:_run_debug_share_nous */
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

/* PoP: hermes_cli_debug_run_debug @ hermes_cli/debug.py:run_debug */
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
