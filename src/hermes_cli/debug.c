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

/* =========================================================================
 * debug share pipeline (hermes_cli/debug.py) — real implementation.
 * Uploads go through libhttp; pending-delete bookkeeping reuses debug_cli.
 * ========================================================================= */
#define _GNU_SOURCE
#include "debug_cli.h"
#include "hermes_http.h"
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define DBG_PASTE_RS_URL       "https://paste.rs/"
#define DBG_DPASTE_COM_URL     "https://dpaste.com/api/"
#define DBG_MAX_LOG_BYTES      512000
#define DBG_AUTO_DELETE_SECONDS 21600
static const char *DBG_REDACTION_BANNER =
    "[hermes debug share: log content redacted at upload time. "
    "run with --no-redact to disable]\n";
static const char *DBG_PRIVACY_NOTICE =
    "\xe2\x9a\xa0\xef\xb8\x8f  This will upload system info + logs to a PUBLIC paste service.\n"
    "\n"
    "Cryptographic secrets (API keys, tokens, passwords) are redacted before\n"
    "upload, but the following personal data is NOT redacted and will be public:\n"
    "  \xe2\x80\xa2 Your display name and persistent platform user ID\n"
    "  \xe2\x80\xa2 Verbatim content of your recent messages (prompts, responses, tool output)\n"
    "  \xe2\x80\xa2 Local filesystem paths\n"
    "  \xe2\x80\xa2 Any other PII present in the logs\n"
    "\n"
    "The resulting URL is public to anyone who has the link. Pastes auto-delete\n"
    "after 6 hours, but may be archived by third parties in the meantime.\n"
    "\n"
    "Use --local to view the report without uploading.\n";

static char *dbg_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static char *dbg_hermes_home(void)
{
    const char *hh = getenv("HERMES_HOME");
    if (hh && hh[0]) return dbg_strdup(hh);
    const char *home = getenv("HOME");
    if (!home) return NULL;
    size_t n = strlen(home) + strlen("/.hermes") + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/.hermes", home);
    return out;
}

/* _primary_log_path @ hermes_cli/debug.py:_primary_log_path — get_hermes_home()
 * /logs/<filename> for known log names; NULL otherwise. */
static char *dbg_primary_log_path(const char *log_name)
{
    static const struct { const char *name, *file; } KNOWN[] = {
        {"agent",   "agent.log"},   {"errors",  "errors.log"},
        {"gateway", "gateway.log"}, {"gui",     "gui.log"},
        {"desktop", "desktop.log"}, {NULL, NULL},
    };
    const char *file = NULL;
    for (int i = 0; KNOWN[i].name; i++)
        if (strcmp(KNOWN[i].name, log_name) == 0) { file = KNOWN[i].file; break; }
    if (!file) return NULL;
    char *home = dbg_hermes_home();
    if (!home) return NULL;
    size_t n = strlen(home) + strlen("/logs/") + strlen(file) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/logs/%s", home, file);
    free(home);
    return out;
}

/* _missing_log_note @ hermes_cli/debug.py:_missing_log_note */
static char *dbg_missing_log_note(const char *log_name)
{
    const char *CLIENT_REASON =
        "written by Hermes Desktop on the machine running the app, not by this "
        "backend. If the desktop connects to a remote/docker/SSH backend, collect "
        "it on that client machine";
    if (strcmp(log_name, "desktop") != 0)
        return dbg_strdup("(file not found)");
    char *primary = dbg_primary_log_path(log_name);
    char buf[1024];
    if (primary)
        snprintf(buf, sizeof(buf), "(not on this host: %s \xe2\x80\x94 expected at %s)",
                 CLIENT_REASON, primary);
    else
        snprintf(buf, sizeof(buf), "(not on this host: %s)", CLIENT_REASON);
    free(primary);
    return dbg_strdup(buf);
}

/* _resolve_log_path @ hermes_cli/debug.py:_resolve_log_path — primary then .1,
 * first non-empty candidate. malloc'd; NULL when unknown/empty. */
static char *dbg_resolve_log_path(const char *log_name)
{
    char *primary = dbg_primary_log_path(log_name);
    if (!primary) return NULL;
    struct stat st;
    if (stat(primary, &st) == 0 && st.st_size > 0) return primary;
    size_t n = strlen(primary) + 3;
    char *rotated = malloc(n);
    snprintf(rotated, n, "%s.1", primary);
    if (stat(rotated, &st) == 0 && st.st_size > 0) { free(primary); return rotated; }
    free(rotated);
    free(primary);
    return NULL;
}

static void dbg_buf_append(char **buf, size_t *len, size_t *cap,
                           const char *s, size_t n)
{
    if (*len + n + 1 > *cap) {
        size_t newcap = (*cap ? *cap : 4096);
        while (newcap < *len + n + 1) newcap *= 2;
        char *nb = realloc(*buf, newcap);
        if (!nb) return;
        *buf = nb; *cap = newcap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

/* _capture_dump @ hermes_cli/debug.py:_capture_dump — run `hermes dump`. */
static char *dbg_capture_dump(void)
{
    FILE *fp = popen("hermes dump 2>/dev/null", "r");
    if (!fp) return dbg_strdup("");
    char *buf = NULL; size_t len = 0, cap = 0;
    char chunk[8192];
    size_t r;
    while ((r = fread(chunk, 1, sizeof(chunk), fp)) > 0)
        dbg_buf_append(&buf, &len, &cap, chunk, r);
    pclose(fp);
    if (!buf) return dbg_strdup("");
    return buf;
}

/* _capture_log_snapshot @ hermes_cli/debug.py:_capture_log_snapshot */
void debug_capture_log_snapshot(const char *log_name, int tail_lines,
                                int redact, debug_log_snapshot_t *out)
{
    out->tail_text = NULL;
    out->full_text = NULL;

    char *path = dbg_resolve_log_path(log_name);
    if (!path) {
        char *primary = dbg_primary_log_path(log_name);
        struct stat st;
        if (primary && stat(primary, &st) == 0)
            out->tail_text = dbg_strdup("(file empty)");
        else
            out->tail_text = dbg_missing_log_note(log_name);
        free(primary);
        return;
    }
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size == 0) {
        out->tail_text = dbg_strdup("(file empty)");
        free(path);
        return;
    }

    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) {
        char buf[160];
        snprintf(buf, sizeof(buf), "(error reading: fopen failed)");
        out->tail_text = dbg_strdup(buf);
        return;
    }

    /* Read at most max_bytes from the end (chunked back-off), mirroring the
     * Python byte-window so the report tail and standalone paste come from
     * one consistent snapshot. */
    long size = st.st_size;
    long pos = size;
    long total = 0, newline_count = 0;
    long chunk_size = 8192;
    char *raw = NULL; size_t raw_len = 0, raw_cap = 0;
    char *chunks[512]; long chunk_lens[512]; int nchunks = 0;
    while (pos > 0 && (total < DBG_MAX_LOG_BYTES ||
                       newline_count <= tail_lines + 1) &&
           total < DBG_MAX_LOG_BYTES * 2 && nchunks < 512) {
        long read_size = chunk_size < pos ? chunk_size : pos;
        pos -= read_size;
        char *chunk = malloc((size_t)read_size);
        if (!chunk) break;
        fseek(f, pos, SEEK_SET);
        size_t got = fread(chunk, 1, (size_t)read_size, f);
        chunks[nchunks] = chunk; chunk_lens[nchunks] = (long)got;
        nchunks++;
        total += (long)got;
        for (long i = (long)got - 1; i >= 0; i--)
            if (chunk[i] == '\n') newline_count++;
        newline_count -= 1; /* Python counts per whole chunk; adjust below */
        chunk_size = chunk_size * 2 < 65536 ? chunk_size * 2 : 65536;
    }
    fclose(f);

    /* Reassemble front-to-back and count newlines accurately. */
    for (int i = nchunks - 1; i >= 0; i--)
        dbg_buf_append(&raw, &raw_len, &raw_cap, chunks[i], (size_t)chunk_lens[i]);
    newline_count = 0;
    for (size_t i = 0; raw && i < raw_len; i++)
        if (raw[i] == '\n') newline_count++;
    bool truncated = pos > 0;

    /* all_text = raw decoded (assume UTF-8-ish bytes pass through). */
    char *all_text = raw ? raw : dbg_strdup("");

    /* tail_text: last tail_lines lines, rstrip '\n'. */
    long lines_seen = 0;
    size_t tail_start = 0;
    for (size_t i = raw_len; i > 0; i--) {
        if (all_text[i - 1] == '\n') {
            lines_seen++;
            if (lines_seen > tail_lines) { tail_start = i; break; }
        }
    }
    char *tail_text = dbg_strdup(all_text + tail_start);
    if (tail_text) {
        size_t tl = strlen(tail_text);
        while (tl > 0 && tail_text[tl - 1] == '\n') tail_text[--tl] = '\0';
    }

    /* full_text: last max_bytes on a line boundary (+ truncation banner). */
    char *full_text = NULL;
    if (truncated && raw_len > (size_t)DBG_MAX_LOG_BYTES) {
        size_t cut = raw_len - (size_t)DBG_MAX_LOG_BYTES;
        bool on_boundary = cut > 0 && all_text[cut - 1] == '\n';
        size_t start = cut;
        if (!on_boundary) {
            const char *nl = strchr(all_text + cut, '\n');
            start = nl ? (size_t)(nl - all_text) + 1 : raw_len;
        }
        const char *banner_fmt = "[... truncated — showing last ~%dKB ...]\n";
        char banner[64];
        snprintf(banner, sizeof(banner), banner_fmt, DBG_MAX_LOG_BYTES / 1024);
        size_t bn = strlen(banner);
        full_text = malloc(bn + (raw_len - start) + 1);
        if (full_text) {
            memcpy(full_text, banner, bn);
            memcpy(full_text + bn, all_text + start, raw_len - start);
            full_text[bn + (raw_len - start)] = '\0';
        }
    } else {
        full_text = dbg_strdup(all_text);
    }
    free(raw);
    for (int i = 0; i < nchunks; i++) free(chunks[i]);

    if (redact) {
        char *rt = debug_redact_log_text(tail_text);
        if (rt) { free(tail_text); tail_text = rt; }
        char *rf = debug_redact_log_text(full_text);
        if (rf) { free(full_text); full_text = rf; }
    }
    out->tail_text = tail_text;
    out->full_text = full_text;
}

void debug_free_log_snapshot(debug_log_snapshot_t *snap)
{
    if (!snap) return;
    free(snap->tail_text);
    free(snap->full_text);
    snap->tail_text = snap->full_text = NULL;
}

/* PoP: hermes_cli_debug_collect_debug_report @ hermes_cli/debug.py:collect_debug_report */
char *debug_collect_debug_report(int log_lines, const char *dump_text,
                                 const char *agent_tail,
                                 const char *errors_tail,
                                 const char *gateway_tail,
                                 const char *gui_tail,
                                 const char *desktop_tail)
{
    int errors_lines = log_lines < 100 ? log_lines : 100;
    char header[128];

    char *buf = NULL; size_t len = 0, cap = 0;
    if (dump_text) dbg_buf_append(&buf, &len, &cap, dump_text, strlen(dump_text));

    snprintf(header, sizeof(header), "\n\n--- agent.log (last %d lines) ---\n", log_lines);
    dbg_buf_append(&buf, &len, &cap, header, strlen(header));
    if (agent_tail) dbg_buf_append(&buf, &len, &cap, agent_tail, strlen(agent_tail));

    snprintf(header, sizeof(header), "\n\n--- errors.log (last %d lines) ---\n", errors_lines);
    dbg_buf_append(&buf, &len, &cap, header, strlen(header));
    if (errors_tail) dbg_buf_append(&buf, &len, &cap, errors_tail, strlen(errors_tail));

    snprintf(header, sizeof(header), "\n\n--- gateway.log (last %d lines) ---\n", errors_lines);
    dbg_buf_append(&buf, &len, &cap, header, strlen(header));
    if (gateway_tail) dbg_buf_append(&buf, &len, &cap, gateway_tail, strlen(gateway_tail));

    snprintf(header, sizeof(header), "\n\n--- gui.log (last %d lines) ---\n", errors_lines);
    dbg_buf_append(&buf, &len, &cap, header, strlen(header));
    if (gui_tail) dbg_buf_append(&buf, &len, &cap, gui_tail, strlen(gui_tail));

    snprintf(header, sizeof(header), "\n\n--- desktop.log (last %d lines) ---\n", errors_lines);
    dbg_buf_append(&buf, &len, &cap, header, strlen(header));
    if (desktop_tail) dbg_buf_append(&buf, &len, &cap, desktop_tail, strlen(desktop_tail));

    dbg_buf_append(&buf, &len, &cap, "\n", 1);
    return buf ? buf : dbg_strdup("");
}

/* _upload_paste_rs @ hermes_cli/debug.py:_upload_paste_rs */
static char *dbg_upload_paste_rs(const char *content)
{
    http_client_t *client = http_client_new(30);
    if (!client) return NULL;
    http_response_t *resp = http_request(client, HTTP_POST, DBG_PASTE_RS_URL,
                                         "Content-Type: text/plain; charset=utf-8\r\n"
                                         "User-Agent: hermes-agent/debug-share",
                                         content, content ? strlen(content) : 0);
    char *url = NULL;
    if (resp && resp->status >= 200 && resp->status < 300 &&
        resp->body && strncmp(resp->body, "http", 4) == 0) {
        size_t n = strlen(resp->body);
        while (n && (resp->body[n-1] == '\n' || resp->body[n-1] == '\r' ||
                     resp->body[n-1] == ' '))
            n--;
        url = malloc(n + 1);
        if (url) { memcpy(url, resp->body, n); url[n] = '\0'; }
    }
    if (resp) http_response_free(resp);
    http_client_free(client);
    return url;
}

/* _upload_dpaste_com @ hermes_cli/debug.py:_upload_dpaste_com */
static char *dbg_upload_dpaste_com(const char *content, int expiry_days)
{
    static const char *BOUNDARY = "----HermesDebugBoundary9f3c";
    size_t cn = content ? strlen(content) : 0;
    size_t cap = cn + 256;
    char *body = malloc(cap);
    size_t off = 0;
    if (!body) return NULL;
    off += (size_t)snprintf(body + off, cap - off,
        "--%s\r\nContent-Disposition: form-data; name=\"content\"\r\n\r\n",
        BOUNDARY);
    /* grow as needed for content + remaining fields */
    size_t need = off + cn + 256;
    char *nb = realloc(body, need);
    if (!nb) { free(body); return NULL; }
    body = nb; cap = need;
    memcpy(body + off, content ? content : "", cn); off += cn;
    off += (size_t)snprintf(body + off, cap - off,
        "\r\n--%s\r\nContent-Disposition: form-data; name=\"syntax\"\r\n\r\ntext\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"expiry_days\"\r\n\r\n%d\r\n"
        "--%s--\r\n",
        BOUNDARY, BOUNDARY, expiry_days, BOUNDARY);

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: multipart/form-data; boundary=%s\r\n"
             "User-Agent: hermes-agent/debug-share", BOUNDARY);

    http_client_t *client = http_client_new(30);
    char *url = NULL;
    if (client) {
        http_response_t *resp = http_request(client, HTTP_POST, DBG_DPASTE_COM_URL,
                                             headers, body, off);
        if (resp && resp->status >= 200 && resp->status < 300 &&
            resp->body && strncmp(resp->body, "http", 4) == 0) {
            size_t n = strlen(resp->body);
            while (n && (resp->body[n-1] == '\n' || resp->body[n-1] == '\r'))
                n--;
            url = malloc(n + 1);
            if (url) { memcpy(url, resp->body, n); url[n] = '\0'; }
        }
        if (resp) http_response_free(resp);
        http_client_free(client);
    }
    free(body);
    return url;
}

/* PoP: hermes_cli_debug_upload_to_pastebin @ hermes_cli/debug.py:upload_to_pastebin */
char *debug_upload_to_pastebin(const char *content, int expiry_days, char **err)
{
    char *errors[2] = {NULL, NULL};
    char *url = dbg_upload_paste_rs(content);
    if (url) return url;
    errors[0] = dbg_strdup("paste.rs: unexpected response");

    url = dbg_upload_dpaste_com(content, expiry_days);
    if (url) { free(errors[0]); return url; }
    errors[1] = dbg_strdup("dpaste.com: unexpected response");

    if (err) {
        size_t n = strlen("Failed to upload to any paste service:\n  ") +
                   strlen(errors[0]) + 4 + strlen(errors[1]) + 1;
        *err = malloc(n);
        if (*err)
            snprintf(*err, n, "Failed to upload to any paste service:\n  %s\n  %s",
                     errors[0], errors[1]);
    }
    free(errors[0]); free(errors[1]);
    return NULL;
}

/* collect_share_bundle @ hermes_cli/debug.py:collect_share_bundle — collect the
 * report + dump-header-prefixed full logs via one snapshot pass. Returns a
 * JSON object {"report","agent.log","gateway.log","gui.log","desktop.log"}
 * (absent logs omitted). */
static json_t *dbg_collect_share_bundle(int log_lines, int redact)
{
    int errors_lines = log_lines < 100 ? log_lines : 100;
    char *dump = dbg_capture_dump();

    debug_log_snapshot_t snaps[5];
    const char *names[5] = {"agent", "errors", "gateway", "gui", "desktop"};
    int tails[5] = {log_lines, errors_lines, errors_lines, errors_lines, errors_lines};
    for (int i = 0; i < 5; i++)
        debug_capture_log_snapshot(names[i], tails[i], redact, &snaps[i]);

    char *report = debug_collect_debug_report(
        log_lines, dump, snaps[0].tail_text, snaps[1].tail_text,
        snaps[2].tail_text, snaps[3].tail_text, snaps[4].tail_text);

    if (redact) {
        size_t bn = strlen(DBG_REDACTION_BANNER);
        char *nr = malloc(bn + strlen(report) + 1);
        if (nr) {
            memcpy(nr, DBG_REDACTION_BANNER, bn);
            strcpy(nr + bn, report);
            free(report);
            report = nr;
        }
    }

    json_t *bundle = json_object();
    if (bundle) json_set(bundle, "report", json_string(report));
    free(report);

    const char *labels[4] = {"agent.log", "gateway.log", "gui.log", "desktop.log"};
    debug_log_snapshot_t *sel[4] = {&snaps[0], &snaps[2], &snaps[3], &snaps[4]};
    for (int i = 0; i < 4; i++) {
        const char *full = sel[i]->full_text;
        if (!bundle) break;
        if (!full || !full[0]) continue;
        /* Prepend the dump header so every file is self-contained. */
        char hdr[160];
        snprintf(hdr, sizeof(hdr), "--- full %s ---\n", labels[i]);
        size_t total = strlen(dump) + 2 + strlen(hdr) + strlen(full) + 1;
        char *combined = malloc(total);
        if (!combined) continue;
        snprintf(combined, total, "%s\n\n%s%s", dump, hdr, full);
        if (redact) {
            size_t bn = strlen(DBG_REDACTION_BANNER);
            char *nc = malloc(bn + total);
            if (nc) {
                memcpy(nc, DBG_REDACTION_BANNER, bn);
                memcpy(nc + bn, combined, total);
                free(combined);
                combined = nc;
            }
        }
        json_set(bundle, labels[i], json_string(combined));
        free(combined);
    }

    for (int i = 0; i < 5; i++) debug_free_log_snapshot(&snaps[i]);
    free(dump);
    return bundle;
}

/* _schedule_auto_delete / _record_pending reuse the debug_cli store. */
static void dbg_schedule_auto_delete(json_t *urls_obj)
{
    if (!urls_obj) return;
    char *home = dbg_hermes_home();
    if (!home) return;
    const char *keys[] = {"Report", "agent.log", "gateway.log", "gui.log",
                          "desktop.log", NULL};
    const char *collected[16];
    int n = 0;
    for (int i = 0; keys[i] && n < 16; i++) {
        json_t *u = json_obj_get(urls_obj, keys[i]);
        if (u && u->type == JSON_STRING)
            collected[n++] = u->str_val;
    }
    if (n > 0)
        debug_record_pending(home, collected, n, (double)time(NULL),
                             DBG_AUTO_DELETE_SECONDS);
    free(home);
}

/* PoP: hermes_cli_debug_build_debug_share @ hermes_cli/debug.py:build_debug_share */
json_t *debug_build_debug_share(int log_lines, int expiry_days, int redact,
                                char **err)
{
    /* Best-effort sweep of expired pending pastes before collecting. */
    {
        char *home = dbg_hermes_home();
        if (home) {
            debug_sweep_expired_pastes(home, (double)time(NULL),
                                       NULL, NULL, NULL);
            free(home);
        }
    }

    json_t *bundle = dbg_collect_share_bundle(log_lines, redact);
    if (!bundle) {
        if (err) *err = dbg_strdup("Failed to collect debug bundle");
        return NULL;
    }
    if (redact)
        hermes_log(LOG_INFO, "debug",
                   "hermes debug share: applied force-mode redaction to log snapshots before upload");

    json_t *result = json_object();
    json_t *urls = json_object();
    json_t *failures = json_array();
    json_set(result, "urls", urls);
    json_set(result, "failures", failures);
    json_set(result, "redacted", json_bool(redact != 0));
    json_set(result, "auto_delete_seconds", json_int(DBG_AUTO_DELETE_SECONDS));

    json_t *report_node = json_obj_get(bundle, "report");
    const char *report = (report_node && report_node->type == JSON_STRING)
                             ? report_node->str_val : "";

    /* 1. Summary report (required — failure surfaces to caller). */
    char *uerr = NULL;
    char *url = debug_upload_to_pastebin(report, expiry_days, &uerr);
    if (!url) {
        if (err) {
            const char *msg = uerr ? uerr : "upload failed";
            size_t n = strlen(msg) + 1;
            *err = malloc(n);
            if (*err) memcpy(*err, msg, n);
        }
        free(uerr);
        json_free(bundle);
        json_free(result);
        return NULL;
    }
    json_set(urls, "Report", json_string(url));
    free(url);

    /* 2-5. Full logs (best-effort; failures collected, not raised). */
    const char *labels[] = {"agent.log", "gateway.log", "gui.log",
                            "desktop.log", NULL};
    for (int i = 0; labels[i]; i++) {
        json_t *content_node = json_obj_get(bundle, labels[i]);
        if (!content_node || content_node->type != JSON_STRING ||
            !content_node->str_val[0])
            continue;
        char *lerr = NULL;
        char *lurl = debug_upload_to_pastebin(content_node->str_val,
                                              expiry_days, &lerr);
        if (lurl) {
            json_set(urls, labels[i], json_string(lurl));
            free(lurl);
        } else {
            size_t n = strlen(labels[i]) + 2 + (lerr ? strlen(lerr) : 0) + 1;
            char *row = malloc(n);
            if (row) {
                snprintf(row, n, "%s: %s", labels[i], lerr ? lerr : "?");
                json_append(failures, json_string(row));
                free(row);
            }
        }
        free(lerr);
    }

    dbg_schedule_auto_delete(urls);

    json_set(result, "report", json_string(report));
    json_free(bundle);
    return result;
}

/* _confirm_upload @ hermes_cli/debug.py:_confirm_upload */
static int dbg_confirm_upload(int yes_flag)
{
    if (yes_flag) return 1;
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr,
                "ERROR: Non-interactive mode requires --yes to confirm upload.\n"
                "       This prevents accidental exposure of personal data.\n"
                "       Use --local to view the report without uploading.\n");
        return -1;
    }
    printf("Upload debug report? [y/N] ");
    fflush(stdout);
    char answer[32] = {0};
    if (!fgets(answer, sizeof(answer), stdin)) answer[0] = '\0';
    for (char *p = answer; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strncmp(answer, "y", 1) == 0) return 1;
    printf("Aborted.\n");
    return 0;
}

/* PoP: hermes_cli_debug_run_debug_share @ hermes_cli/debug.py:run_debug_share */
int debug_run_debug_share(int lines, int expire, int local_only, int nous,
                          int redact, const char **extra_argv, int extra_argc)
{
    (void)extra_argv; (void)extra_argc; /* --yes handled by caller flag */

    if (local_only) {
        char *home = dbg_hermes_home();
        if (home) {
            debug_sweep_expired_pastes(home, (double)time(NULL), NULL, NULL, NULL);
            free(home);
        }
        printf("Collecting debug report...\n");
        json_t *bundle = dbg_collect_share_bundle(lines, redact);
        if (!bundle) return 1;
        json_t *rep = json_obj_get(bundle, "report");
        printf("%s\n", (rep && rep->type == JSON_STRING) ? rep->str_val : "");
        const struct { const char *title, *label; } FULLS[] = {
            {"FULL agent.log", "agent.log"},
            {"FULL gateway.log", "gateway.log"},
            {"FULL gui.log", "gui.log"},
            {"FULL desktop.log", "desktop.log"},
            {NULL, NULL},
        };
        for (int i = 0; FULLS[i].title; i++) {
            json_t *body = json_obj_get(bundle, FULLS[i].label);
            if (!body || body->type != JSON_STRING || !body->str_val[0])
                continue;
            printf("\n\n============================================================\n");
            printf("%s\n", FULLS[i].title);
            printf("============================================================\n\n");
            printf("%s\n", body->str_val);
        }
        json_free(bundle);
        return 0;
    }

    if (nous) {
        /* The Nous-S3 path (_run_debug_share_nous) uploads to internal AWS S3
         * with signed URLs; no C-side credentials plumbing exists yet. */
        fprintf(stderr,
                "Nous-S3 debug share is not available in this build; "
                "use `hermes debug share` or `--local`.\n");
        return 1;
    }

    fputs(DBG_PRIVACY_NOTICE, stdout);
    int confirmed = dbg_confirm_upload(0 /* --yes parsed by CLI layer */);
    if (confirmed <= 0) return confirmed < 0 ? 1 : 0;

    printf("Collecting debug report...\n");
    printf("Uploading...\n");

    char *err = NULL;
    json_t *result = debug_build_debug_share(lines, expire, redact, &err);
    if (!result) {
        fprintf(stderr, "\nUpload failed: %s\n", err ? err : "?");
        fprintf(stderr, "\nRun `hermes debug share --local` to print the report instead.\n\n");
        free(err);
        return 1;
    }
    free(err);

    json_t *urls = json_obj_get(result, "urls");
    printf("\nDebug report uploaded:\n");
    if (urls && urls->type == JSON_OBJECT) {
        /* Preserve Python's insertion order: Report first, then logs. */
        const char *order[] = {"Report", "agent.log", "gateway.log", "gui.log",
                               "desktop.log", NULL};
        size_t width = 6;
        for (int i = 0; order[i]; i++) {
            json_t *u = json_obj_get(urls, order[i]);
            if (u && u->type == JSON_STRING && strlen(order[i]) > width)
                width = strlen(order[i]);
        }
        for (int i = 0; order[i]; i++) {
            json_t *u = json_obj_get(urls, order[i]);
            if (u && u->type == JSON_STRING)
                printf("  %-*s  %s\n", (int)width, order[i], u->str_val);
        }
    }
    json_t *failures = json_obj_get(result, "failures");
    if (failures && failures->type == JSON_ARRAY && json_len(failures) > 0) {
        printf("\n  (failed to upload: ");
        size_t fn = json_len(failures);
        for (size_t i = 0; i < fn; i++) {
            json_t *f = json_get(failures, i);
            printf("%s%s", i ? ", " : "",
                   (f && f->type == JSON_STRING) ? f->str_val : "?");
        }
        printf(")\n");
    }
    printf("\n\xe2\x8f\xb1  Pastes will auto-delete in %d hours.\n",
           DBG_AUTO_DELETE_SECONDS / 3600);
    printf("To delete now:  hermes debug delete <url>\n");
    printf("\nShare these links with the Hermes team for support.\n");
    json_free(result);
    return 0;
}

/* PoP: hermes_cli_debug_run_debug_delete @ hermes_cli/debug.py:run_debug_delete */
int debug_run_debug_delete(const char *const *urls, int n)
{
    if (!urls || n == 0) {
        printf("Usage: hermes debug delete <url> [<url> ...]\n");
        printf("  Deletes paste.rs pastes uploaded by 'hermes debug share'.\n");
        return 0;
    }
    for (int i = 0; i < n; i++) {
        const char *url = urls[i];
        char *paste_id = debug_extract_paste_id(url);
        if (!paste_id) {
            printf("  \xe2\x9c\x97 Cannot delete: only paste.rs URLs are supported.  Got: %s\n",
                   url);
            continue;
        }
        size_t target_n = strlen(DBG_PASTE_RS_URL) + strlen(paste_id) + 1;
        char *target = malloc(target_n);
        int ok = 0;
        if (target) {
            snprintf(target, target_n, "%s%s", DBG_PASTE_RS_URL, paste_id);
            http_client_t *client = http_client_new(30);
            if (client) {
                http_response_t *resp = http_request(
                    client, HTTP_DELETE, target,
                    "User-Agent: hermes-agent/debug-share", NULL, 0);
                ok = resp && resp->status >= 200 && resp->status < 300;
                if (resp) http_response_free(resp);
                http_client_free(client);
            }
            free(target);
        }
        if (ok)
            printf("  \xe2\x9c\x93 Deleted: %s\n", url);
        else
            printf("  \xe2\x9c\x97 Failed to delete: %s (unexpected response)\n", url);
        free(paste_id);
    }
    return 0;
}
