/*
 * port_cron_monitor.c — Monitor-mode cron support (port of cron/monitor.py).
 *
 * A monitor job attaches a cheap monitor source (monitor_script or
 * monitor_url) to an ordinary LLM cron job. Each tick the source runs FIRST
 * and its exact output bytes are hashed against the last agent-triggering
 * tick:
 *
 *   unchanged → suppressed (silent no_change tick); no LLM, no delivery.
 *   changed or first run → MONITOR CHANGE DETECTED context block injected.
 *   source failure → ERROR (never a change); stored hash left untouched.
 *
 * Reuses scheduler_run_job_script() for the script source and libhttp for
 * the URL source; libhash for sha256. Mirrors cron/monitor.py verbatim.
 */

#include "cron_monitor.h"

#include "hermes_logger.h"
#include "hermes_json.h"
#include "cron_jobs.h"   /* cronjobs_job_output_dir, cronjobs_update_job, now_iso */
#include "hash.h"
#include "libhttp/http.h"
#include "difflib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* Re-declare the scheduler helper we link against (no public header). */
bool scheduler_run_job_script(const char *script_path, const char *workdir,
                              char **output_out);

/* Mirror constants (cron/monitor.py). */
#define CRON_MONITOR_MAX_DIFF_CHARS  4000
#define CRON_MONITOR_MAX_OUTPUT_CHARS 8000
#define CRON_MONITOR_URL_TIMEOUT_SEC 30
#define CRON_MONITOR_MAX_URL_BYTES   (256 * 1024)
#define CRON_MONITOR_SNAPSHOT_NAME  "monitor_last_output.txt"

/* ── Snapshot path ────────────────────────────────────────────────── */
static char *monitor_snapshot_path(const char *job_id) {
    char *err = NULL;
    char *dir = cronjobs_job_output_dir(job_id, &err);
    if (!dir) { free(err); return NULL; }
    char *p = malloc(strlen(dir) + 1 + strlen(CRON_MONITOR_SNAPSHOT_NAME) + 1);
    if (p) snprintf(p, strlen(dir) + 1 + strlen(CRON_MONITOR_SNAPSHOT_NAME) + 1,
                    "%s/%s", dir, CRON_MONITOR_SNAPSHOT_NAME);
    free(dir);
    return p;
}

/* ── Hash ─────────────────────────────────────────────────────────── */
char *cron_monitor_hash_output(const char *output) {
    if (!output) return hash_sha256_hex((const unsigned char *)"", 0);
    return hash_sha256_hex((const unsigned char *)output, strlen(output));
}

/* ── Source execution ─────────────────────────────────────────────── */

/* Bounded GET of a monitor URL. Returns true+body on 2xx. */
static bool fetch_monitor_url(const char *url, char **out) {
    if (!url) { if (out) *out = NULL; return false; }
    const char *l = strchr(url, ':');
    char scheme[16];
    if (l && (size_t)(l - url) < sizeof(scheme)) {
        memcpy(scheme, url, (size_t)(l - url));
        scheme[l - url] = '\0';
    } else { scheme[0] = '\0'; }
    char lc[16];
    for (size_t i = 0; scheme[i]; i++) lc[i] = (char)tolower((unsigned char)scheme[i]);
    lc[sizeof(scheme)-1] = '\0';
    if (strcmp(lc, "http") != 0 && strcmp(lc, "https") != 0) {
        if (out) *out = NULL;
        return false;
    }
    http_t *h = http_new(CRON_MONITOR_URL_TIMEOUT_SEC);
    if (!h) { if (out) *out = NULL; return false; }
    http_resp_t *r = http_get(h, url, "User-Agent: hermes-cron-monitor");
    bool ok = false;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        /* Cap at MAX_URL_BYTES (matches Python resp.read(MAX_URL_BYTES+1)). */
        size_t blen = r->body_len ? r->body_len : strlen(r->body);
        char *body = malloc(blen + 1);
        if (body) {
            memcpy(body, r->body, blen);
            body[blen < CRON_MONITOR_MAX_URL_BYTES ? blen : CRON_MONITOR_MAX_URL_BYTES] = '\0';
            if (out) *out = body;
            ok = true;
        }
    }
    if (r) { /* free resp */ }
    http_free(h);
    return ok;
}

/* Run the job's monitor source; (ok, output) where output is malloc'd. */
bool cron_monitor_run_source(const char *script, const char *url,
                             const char *workdir, char **output) {
    if (output) *output = NULL;
    if (script && *script)
        return scheduler_run_job_script(script, workdir, output);
    if (url && *url)
        return fetch_monitor_url(url, output);
    return false;
}

/* True iff a non-NULL, non-whitespace-only string has content after strip. */
static bool _cron_monitor_strip_has_nonempty(const char *s);

bool cron_monitor_has(const char *monitor_script, const char *monitor_url) {
    /* Python: (monitor_script or "").strip() — strip whitespace first. */
    const char *s = monitor_script ? monitor_script : "";
    const char *u = monitor_url    ? monitor_url    : "";
    return _cron_monitor_strip_has_nonempty(s) || _cron_monitor_strip_has_nonempty(u);
}

/* True iff a non-NULL, non-whitespace-only string has content after strip. */
static bool _cron_monitor_strip_has_nonempty(const char *s) {
    if (!s) return false;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return false;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    return end > s;
}

/* ── Diff ─────────────────────────────────────────────────────────── */

char *cron_monitor_build_diff(const char *old, const char *new_out) {
    /* Reuse libdifflib's faithful unified_diff (Myers/LCS). It emits
     * "--- original\n+++ modified\n" headers; cron/monitor.py pins
     * fromfile="previous"/tofile="current" via difflib.unified_diff(...,
     * lineterm=""). Rewrite the two header lines to match the Python
     * contract, then cap at MAX_DIFF_CHARS (mirrors the truncation slice). */
    char *diff = difflib_unified_diff(old, new_out, 3);
    if (!diff) return strdup("");

    const char *from = "--- previous\n";
    const char *to   = "+++ current\n";
    /* difflib_unified_diff always emits the "--- original / +++ modified"
     * header pair even for identical/no-change input. Python's
     * difflib.unified_diff yields NO lines when sequences are identical
     * (only hunk groups with changes; the headers are part of the first
     * yield which never happens for equal inputs). So: skip both header
     * lines and, if there is no hunk body, return "". */
    const char *nl1 = strchr(diff, '\n');
    const char *nl2 = nl1 ? strchr(nl1 + 1, '\n') : NULL;
    const char *body = nl2 ? nl2 + 1 : diff;

    /* Identical input -> no hunk body -> Python returns "". */
    if (body[0] == '\0') {
        free(diff);
        return strdup("");
    }

    size_t blen = strlen(body);
    size_t need = strlen(from) + strlen(to) + blen + 32;
    char *out = malloc(need);
    if (!out) { free(diff); return NULL; }
    size_t o = 0;
    memcpy(out + o, from, strlen(from)); o += strlen(from);
    memcpy(out + o, to,   strlen(to));   o += strlen(to);
    if (blen <= CRON_MONITOR_MAX_DIFF_CHARS) {
        memcpy(out + o, body, blen); o += blen;
    } else {
        size_t keep = CRON_MONITOR_MAX_DIFF_CHARS;
        memcpy(out + o, body, keep); o += keep;
        const char *trunc = "\n... [diff truncated]";
        memcpy(out + o, trunc, strlen(trunc)); o += strlen(trunc);
    }
    free(diff);
    /* Python: "\n".join(unified_diff(..., lineterm="")) — joins lines WITHOUT
     * a trailing newline. difflib_unified_diff emits a trailing "\n"; strip it. */
    while (o > 0 && out[o-1] == '\n') o--;
    out[o] = '\0';
    return out;
}

/* ── Context block + main entry ────────────────────────────────────── */

static char *monitor_read_last_output(const char *job_id) {
    char *path = monitor_snapshot_path(job_id);
    if (!path) return strdup("");
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return strdup("");
    char *buf = NULL; size_t cap = 0; size_t n = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (n + 1 >= cap) { cap = cap ? cap * 2 : 256; buf = realloc(buf, cap); }
        buf[n++] = (char)c;
    }
    fclose(f);
    if (buf) buf[n] = '\0';
    return buf ? buf : strdup("");
}

static void monitor_write_last_output(const char *job_id, const char *output) {
    char *path = monitor_snapshot_path(job_id);
    if (!path) return;
    /* cronjobs_save_job_output creates the output dir; reuse its mkdir -p. */
    char *err = NULL;
    char *dir = cronjobs_job_output_dir(job_id, &err);
    if (dir) {
#ifdef _WIN32
        _mkdir(dir);
#else
        mkdir(dir, 0700);
#endif
        free(dir);
    }
    free(err);
    FILE *f = fopen(path, "wb");
    free(path);
    if (!f) return;
    if (output) fwrite(output, 1, strlen(output), f);
    fclose(f);
}

static void monitor_persist(const char *job_id, const char *hash, const char *output) {
    monitor_write_last_output(job_id, output);
    char *now = now_iso();
    json_t *st = json_object();
    if (st) {
        json_set(st, "last_output_hash",  json_string(hash ? hash : ""));
        json_set(st, "last_changed_at",   json_string(now ? now : ""));
        char *err = NULL;
        json_t *rep = cronjobs_update_job(job_id, st, &err);
        free(err);
        json_free(rep);
    }
    json_free(st);
    free(now);
}

/* Truncate output to MAX_OUTPUT_CHARS, appending a truncation marker. */
static char *monitor_trim_output(const char *output) {
    if (!output) return strdup("");
    size_t n = strlen(output);
    if (n <= CRON_MONITOR_MAX_OUTPUT_CHARS) return strdup(output);
    char *out = malloc(CRON_MONITOR_MAX_OUTPUT_CHARS + 32);
    if (out) {
        memcpy(out, output, CRON_MONITOR_MAX_OUTPUT_CHARS);
        strcpy(out + CRON_MONITOR_MAX_OUTPUT_CHARS, "\n... [output truncated]");
    }
    return out;
}

monitor_outcome_t cron_monitor_check(const char *job_id,
                                     const char *monitor_script,
                                     const char *monitor_url,
                                     const char *workdir,
                                     const char *last_hash,
                                     const char *last_output) {
    monitor_outcome_t out = { .ok = false, .changed = false, .first_run = false,
                              .context_block = NULL, .error = NULL };

    char *source_out = NULL;
    bool source_ok = cron_monitor_run_source(monitor_script, monitor_url, workdir, &source_out);
    if (!source_ok) {
        out.ok = false;
        out.error = source_out ? source_out : strdup("monitor source failed");
        return out;
    }

    char *new_hash = cron_monitor_hash_output(source_out);

    bool first_run = (last_hash == NULL) || (*last_hash == '\0');
    bool changed = first_run || (new_hash && last_hash && strcmp(new_hash, last_hash) != 0);

    if (!changed) {
        free(new_hash);
        free(source_out);
        out.ok = true;
        return out;  /* suppressed: no agent run */
    }

    char *old_out = first_run ? strdup("") : strdup(last_output ? last_output : "");
    char *shown = monitor_trim_output(source_out);
    char *diff = cron_monitor_build_diff(old_out, source_out);

    /* Context block (exact bytes match the Python f-strings). */
    const char *header = first_run
        ? "## Monitor Baseline (first run)\n\n"
          "This is the first observation of the monitored source — there is "
          "no previous output to diff against.\n\n"
          "### Current output\n\n```\n"
        : "## MONITOR CHANGE DETECTED\n\n"
          "The monitored source's output changed since the last run.\n\n"
          "### Diff (previous → current)\n\n```diff\n";
    const char *closer = first_run ? "\n```\n" : "\n```\n\n### Current output\n\n```\n";

    size_t hlen = strlen(header), clen = strlen(closer);
    size_t dlen = diff ? strlen(diff) : 0;
    size_t slen = shown ? strlen(shown) : 0;
    char *block = malloc(hlen + dlen + clen + slen + 16);
    if (block) {
        size_t o = 0;
        o += snprintf(block + o, hlen + dlen + clen + slen + 16 - o, "%s", header);
        o += snprintf(block + o, hlen + dlen + clen + slen + 16 - o, "%s", diff ? diff : "");
        o += snprintf(block + o, hlen + dlen + clen + slen + 16 - o, "%s", closer);
        o += snprintf(block + o, hlen + dlen + clen + slen + 16 - o, "%s\n```\n", shown ? shown : "");
        out.context_block = block;
    }

    /* Persist hash + snapshot BEFORE returning (detection is the state boundary). */
    monitor_persist(job_id, new_hash, source_out);

    free(new_hash); free(source_out); free(old_out); free(shown); free(diff);
    out.ok = true;
    out.changed = true;
    out.first_run = first_run;
    return out;
}
